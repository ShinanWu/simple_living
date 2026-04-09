// gateway brpc edge server — HTTP+JSON front, calls downstream brpc services
#include <google/protobuf/empty.pb.h>
#include <google/protobuf/util/json_util.h>
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <brpc/channel.h>
#include <butil/logging.h>
#include <sys/time.h>

#include <cstdlib>
#include <sstream>
#include <string>

#include "gateway_user_edge.pb.h"
#include "gateway_pages_edge.pb.h"
#include "user_domain_service.pb.h"
#include "recommendation_domain_service.pb.h"
#include "content_domain.pb.h"
#include "tracking_domain.pb.h"

namespace {
constexpr const char* kDefaultUserDomainAddr =
    "brpc://user-domain.simple-living.svc.cluster.local:9101";
constexpr const char* kDefaultContentDomainAddr =
    "brpc://content-domain.simple-living.svc.cluster.local:9102";
constexpr const char* kDefaultRecommendationDomainAddr =
    "brpc://recommendation-domain.simple-living.svc.cluster.local:9103";
constexpr const char* kDefaultTrackingDomainAddr =
    "brpc://tracking-domain.simple-living.svc.cluster.local:9105";
}  // namespace

DEFINE_int32(port, 8080, "TCP port for this brpc server");
DEFINE_string(user_domain_addr, kDefaultUserDomainAddr, "user-domain address");
DEFINE_string(content_domain_addr, kDefaultContentDomainAddr, "content-domain address");
DEFINE_string(recommendation_domain_addr, kDefaultRecommendationDomainAddr, "recommendation-domain address");
DEFINE_string(tracking_domain_addr, kDefaultTrackingDomainAddr, "tracking-domain address");

namespace simple_living {
namespace gateway {

namespace {

int64_t NowUnixMs() {
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

std::string JsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':
                o += "\\\"";
                break;
            case '\\':
                o += "\\\\";
                break;
            case '\n':
                o += "\\n";
                break;
            case '\r':
                o += "\\r";
                break;
            case '\t':
                o += "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

bool PbToJsonData(const google::protobuf::Message& msg, std::string* out) {
    google::protobuf::util::JsonPrintOptions opts;
    opts.preserve_proto_field_names = true;
    return google::protobuf::util::MessageToJsonString(msg, out, opts).ok();
}

void WriteJsonEnvelope(brpc::Controller* outer,
                       bool success,
                       int code,
                       const std::string& message,
                       const std::string& data_json,
                       int http_status) {
    outer->manage_http_body_on_error(true);
    const std::string* req_id_hdr = outer->http_request().GetHeader("X-Request-Id");
    std::string req_id = (req_id_hdr && !req_id_hdr->empty()) ? *req_id_hdr : outer->request_id();
    if (req_id.empty()) {
        req_id = "gw_req";
    }

    std::ostringstream oss;
    oss << "{\"success\":" << (success ? "true" : "false") << ",\"code\":" << code << ",\"message\":\""
        << JsonEscape(message) << "\",\"data\":" << data_json << ",\"meta\":{\"request_id\":\""
        << JsonEscape(req_id) << "\",\"server_time_ms\":" << NowUnixMs() << "}}";

    outer->http_response().set_status_code(http_status);
    outer->http_response().set_content_type("application/json");
    outer->response_attachment().append(oss.str());
}

void FinishOk(brpc::Controller* outer, const google::protobuf::Message& data_msg) {
    std::string inner;
    if (!PbToJsonData(data_msg, &inner)) {
        WriteJsonEnvelope(outer, false, 50000, "Failed to serialize response", "null",
                          brpc::HTTP_STATUS_INTERNAL_SERVER_ERROR);
        return;
    }
    WriteJsonEnvelope(outer, true, 0, "OK", inner, brpc::HTTP_STATUS_OK);
}

void FinishBizError(brpc::Controller* outer,
                    int code,
                    const std::string& msg,
                    int http) {
    WriteJsonEnvelope(outer, false, code, msg, "null", http);
}

void FinishDownstreamBrpcFailure(brpc::Controller* outer, const brpc::Controller& inner) {
    // Prefer stable client-facing code; keep detail out of envelope message in production if needed.
    std::string detail = inner.ErrorText();
    if (detail.size() > 200) {
        detail.resize(200);
    }
    FinishBizError(outer, 50001, detail.empty() ? "Upstream error" : detail,
                   brpc::HTTP_STATUS_BAD_GATEWAY);
}

simple_living::user_domain::ClientPlatform ParseClientPlatform(const std::string& s) {
    if (s == "web") {
        return simple_living::user_domain::CLIENT_PLATFORM_WEB;
    }
    if (s == "ios") {
        return simple_living::user_domain::CLIENT_PLATFORM_IOS;
    }
    if (s == "android") {
        return simple_living::user_domain::CLIENT_PLATFORM_ANDROID;
    }
    if (s == "wechat_miniprogram") {
        return simple_living::user_domain::CLIENT_PLATFORM_WECHAT_MINIPROGRAM;
    }
    if (s == "douyin_miniprogram") {
        return simple_living::user_domain::CLIENT_PLATFORM_DOUYIN_MINIPROGRAM;
    }
    return simple_living::user_domain::CLIENT_PLATFORM_UNSPECIFIED;
}

simple_living::user_domain::RevokeSessionScope ParseRevokeScope(const std::string& s) {
    if (s == "all_user_sessions") {
        return simple_living::user_domain::REVOKE_SESSION_SCOPE_ALL_USER_SESSIONS;
    }
    return simple_living::user_domain::REVOKE_SESSION_SCOPE_SINGLE_SESSION;
}

simple_living::user_domain::ClearHistoryScope ParseClearHistoryScope(const std::string& s) {
    if (s == "before_time") {
        return simple_living::user_domain::CLEAR_HISTORY_SCOPE_BEFORE_TIME;
    }
    return simple_living::user_domain::CLEAR_HISTORY_SCOPE_ALL;
}

simple_living::user_domain::FeedbackTargetType ParseFeedbackTargetTypeStr(const std::string& n) {
    if (n == "guide_card") {
        return simple_living::user_domain::FEEDBACK_TARGET_TYPE_GUIDE_CARD;
    }
    if (n == "recommendation_result") {
        return simple_living::user_domain::FEEDBACK_TARGET_TYPE_RECOMMENDATION_RESULT;
    }
    if (n == "app") {
        return simple_living::user_domain::FEEDBACK_TARGET_TYPE_APP;
    }
    if (n == "other") {
        return simple_living::user_domain::FEEDBACK_TARGET_TYPE_OTHER;
    }
    return simple_living::user_domain::FEEDBACK_TARGET_TYPE_UNSPECIFIED;
}

bool JsonBodyToMessage(brpc::Controller* outer, google::protobuf::Message* msg) {
    std::string body = outer->request_attachment().to_string();
    if (body.empty()) {
        return true;
    }
    google::protobuf::util::JsonParseOptions opts;
    opts.ignore_unknown_fields = true;
    return google::protobuf::util::JsonStringToMessage(body, msg, opts).ok();
}

bool EnsurePost(brpc::Controller* outer) {
    if (outer->http_request().method() != brpc::HTTP_METHOD_POST) {
        FinishBizError(outer, 10001, "Method not allowed, use POST", brpc::HTTP_STATUS_METHOD_NOT_ALLOWED);
        return false;
    }
    return true;
}

void FillRpcContextFromHeaders(brpc::Controller* outer,
                               simple_living::user_domain::RpcRequestContext* ctx) {
    if (const std::string* p = outer->http_request().GetHeader("X-Client-Platform")) {
        ctx->set_client_platform(ParseClientPlatform(*p));
    }
    if (const std::string* v = outer->http_request().GetHeader("X-Client-Version")) {
        ctx->set_app_version(*v);
    }
    if (const std::string* d = outer->http_request().GetHeader("X-Device-Id")) {
        ctx->set_device_id(*d);
    }
}

enum class ActorKind { kNone = 0, kUser, kGuest };

struct ResolvedActor {
    ActorKind kind = ActorKind::kNone;
    std::string user_id;
    std::string session_id;
};

bool ResolveActor(simple_living::user_domain::UserDomainService_Stub* stub,
                  brpc::Controller* outer,
                  ResolvedActor* out) {
    if (const std::string* auth = outer->http_request().GetHeader("Authorization")) {
        static constexpr char kBearer[] = "Bearer ";
        if (auth->size() > sizeof(kBearer) - 1 &&
            auth->compare(0, sizeof(kBearer) - 1, kBearer) == 0) {
            std::string token = auth->substr(sizeof(kBearer) - 1);
            brpc::Controller c;
            simple_living::user_domain::IntrospectAccessTokenRequest ireq;
            ireq.set_access_token(token);
            FillRpcContextFromHeaders(outer, ireq.mutable_request_context());
            simple_living::user_domain::IntrospectAccessTokenResponse iresp;
            stub->IntrospectAccessToken(&c, &ireq, &iresp, nullptr);
            if (!c.Failed() && iresp.valid()) {
                out->kind = ActorKind::kUser;
                out->user_id = iresp.user_id();
                out->session_id = iresp.session_id();
                return true;
            }
            return false;
        }
    }
    if (const std::string* guest = outer->http_request().GetHeader("X-Guest-Session-Id")) {
        if (!guest->empty()) {
            out->kind = ActorKind::kGuest;
            out->session_id = *guest;
            return true;
        }
    }
    return false;
}

}  // namespace

static void InitChannel(brpc::Channel* ch, const std::string& addr) {
    brpc::ChannelOptions opts;
    opts.timeout_ms = 5000;
    opts.max_retry = 2;
    if (ch->Init(addr.c_str(), &opts) != 0) {
        LOG(FATAL) << "Fail to init channel to " << addr;
    }
}

namespace user {

class GatewayUserEdgeV2Impl : public GatewayUserEdgeV2 {
    brpc::Channel user_ch_;
    simple_living::user_domain::UserDomainService_Stub user_stub_;

public:
    GatewayUserEdgeV2Impl() : user_stub_(&user_ch_) { InitChannel(&user_ch_, FLAGS_user_domain_addr); }

    void PostGuestSession(::google::protobuf::RpcController* controller_base,
                          const GuestSessionHttpRequest* req,
                          ::simple_living::user_domain::EnsureGuestSessionResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        simple_living::user_domain::EnsureGuestSessionRequest dreq;
        if (req->has_device_id()) {
            dreq.set_device_id(req->device_id());
        } else if (const std::string* d = outer->http_request().GetHeader("X-Device-Id")) {
            dreq.set_device_id(*d);
        }
        if (req->has_client_platform()) {
            dreq.set_client_platform(ParseClientPlatform(req->client_platform()));
        } else if (const std::string* p = outer->http_request().GetHeader("X-Client-Platform")) {
            dreq.set_client_platform(ParseClientPlatform(*p));
        }
        if (req->has_app_version()) {
            dreq.set_app_version(req->app_version());
        }
        brpc::Controller cntl;
        user_stub_.EnsureGuestSession(&cntl, &dreq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void PostAuthTokenIssue(::google::protobuf::RpcController* controller_base,
                            const TokenIssueHttpRequest* req,
                            ::simple_living::user_domain::IssueTokenPairResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        if (!req->has_account_proof() || !req->account_proof().has_user_id() ||
            req->account_proof().user_id().empty()) {
            FinishBizError(outer, 10002, "Validation failed", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        simple_living::user_domain::IssueTokenPairRequest ireq;
        ireq.set_user_id(req->account_proof().user_id());
        if (req->has_device_fingerprint()) {
            ireq.set_device_fingerprint(req->device_fingerprint());
        }
        if (req->has_client_platform()) {
            ireq.set_client_platform(ParseClientPlatform(req->client_platform()));
        } else {
            FillRpcContextFromHeaders(outer, ireq.mutable_request_context());
        }
        if (req->has_app_version()) {
            ireq.set_app_version(req->app_version());
        }
        if (req->has_device_id()) {
            ireq.set_device_id(req->device_id());
        }
        brpc::Controller cntl;
        user_stub_.IssueTokenPair(&cntl, &ireq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void PostAuthTokenRefresh(::google::protobuf::RpcController* controller_base,
                              const RefreshTokenHttpRequest* req,
                              ::simple_living::user_domain::IssueTokenPairResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        brpc::Controller cntl;
        simple_living::user_domain::IntrospectRefreshTokenRequest rreq;
        rreq.set_refresh_token(req->refresh_token());
        if (req->has_request_context()) {
            *rreq.mutable_request_context() = req->request_context();
        } else {
            FillRpcContextFromHeaders(outer, rreq.mutable_request_context());
        }
        simple_living::user_domain::IntrospectRefreshTokenResponse rresp;
        user_stub_.IntrospectRefreshToken(&cntl, &rreq, &rresp, nullptr);
        if (cntl.Failed() || !rresp.valid()) {
            FinishBizError(outer, 20003, "Refresh token invalid or expired", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        cntl.Reset();
        simple_living::user_domain::IssueTokenPairRequest ireq;
        ireq.set_user_id(rresp.user_id());
        user_stub_.IssueTokenPair(&cntl, &ireq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void DeleteAuthSession(::google::protobuf::RpcController* controller_base,
                           const RevokeSessionHttpRequest* req,
                           ::simple_living::user_domain::RevokeSessionResponse* resp,
                           ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        simple_living::user_domain::RevokeSessionRequest dreq;
        dreq.set_user_id(actor.user_id);
        if (!actor.session_id.empty()) {
            dreq.set_session_id(actor.session_id);
        }
        dreq.set_scope(ParseRevokeScope(req->revoke_scope()));
        brpc::Controller cntl;
        user_stub_.RevokeSession(&cntl, &dreq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void MeProfileHttp(::google::protobuf::RpcController* controller_base,
                       const ::google::protobuf::Empty*,
                       ::google::protobuf::Empty*,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        const std::string action = outer->http_request().unresolved_path();
        if (action == "get") {
            simple_living::user_domain::GetProfileRequest dreq;
            ResolvedActor actor;
            if (ResolveActor(&user_stub_, outer, &actor)) {
                if (actor.kind == ActorKind::kUser) {
                    dreq.set_user_id(actor.user_id);
                } else {
                    dreq.set_session_id(actor.session_id);
                }
            }
            simple_living::user_domain::GetProfileResponse resp;
            brpc::Controller cntl;
            user_stub_.GetProfile(&cntl, &dreq, &resp, nullptr);
            if (cntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, cntl);
                return;
            }
            FinishOk(outer, resp);
            return;
        }
        if (action == "update") {
            ResolvedActor actor;
            if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
                FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
                return;
            }
            simple_living::user_domain::UpdateProfileRequest dreq;
            if (!JsonBodyToMessage(outer, &dreq)) {
                FinishBizError(outer, 10002, "Invalid JSON body", brpc::HTTP_STATUS_BAD_REQUEST);
                return;
            }
            dreq.set_user_id(actor.user_id);
            simple_living::user_domain::UpdateProfileResponse resp;
            brpc::Controller cntl;
            user_stub_.UpdateProfile(&cntl, &dreq, &resp, nullptr);
            if (cntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, cntl);
                return;
            }
            FinishOk(outer, resp);
            return;
        }
        FinishBizError(outer, 10002, "Unknown profile action", brpc::HTTP_STATUS_BAD_REQUEST);
    }

    void MePreferencesHttp(::google::protobuf::RpcController* controller_base,
                           const ::google::protobuf::Empty*,
                           ::google::protobuf::Empty*,
                           ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        const std::string action = outer->http_request().unresolved_path();
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        if (action == "get") {
            simple_living::user_domain::GetPreferencesRequest dreq;
            dreq.set_user_id(actor.user_id);
            simple_living::user_domain::GetPreferencesResponse resp;
            brpc::Controller cntl;
            user_stub_.GetPreferences(&cntl, &dreq, &resp, nullptr);
            if (cntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, cntl);
                return;
            }
            FinishOk(outer, resp);
            return;
        }
        if (action == "update") {
            PutMePreferencesHttpRequest hreq;
            if (!JsonBodyToMessage(outer, &hreq)) {
                FinishBizError(outer, 10002, "Invalid JSON body", brpc::HTTP_STATUS_BAD_REQUEST);
                return;
            }
            simple_living::user_domain::UpdatePreferencesRequest dreq;
            dreq.set_user_id(actor.user_id);
            if (hreq.has_preferences()) {
                const PreferencesBodyHttp& p = hreq.preferences();
                if (p.has_preferences_version()) {
                    dreq.mutable_preferences()->set_preferences_version(p.preferences_version());
                }
                for (int i = 0; i < p.themes_size(); ++i) {
                    dreq.mutable_preferences()->mutable_structured()->add_theme_interests(p.themes(i));
                }
            }
            simple_living::user_domain::UpdatePreferencesResponse resp;
            brpc::Controller cntl;
            user_stub_.UpdatePreferences(&cntl, &dreq, &resp, nullptr);
            if (cntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, cntl);
                return;
            }
            FinishOk(outer, resp);
            return;
        }
        FinishBizError(outer, 10002, "Unknown preferences action", brpc::HTTP_STATUS_BAD_REQUEST);
    }

    void GetMeFavorites(::google::protobuf::RpcController* controller_base,
                        const ListFavoritesHttpRequest* req,
                        ::simple_living::user_domain::ListFavoritesResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        simple_living::user_domain::ListFavoritesRequest dreq;
        dreq.set_user_id(actor.user_id);
        if (req->limit() > 0) {
            dreq.mutable_page()->set_limit(req->limit());
        }
        if (req->has_cursor()) {
            dreq.mutable_page()->set_cursor(req->cursor());
        }
        brpc::Controller cntl;
        user_stub_.ListFavorites(&cntl, &dreq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void PostMeFavorite(::google::protobuf::RpcController* controller_base,
                        const AddFavoriteHttpRequest* req,
                        ::simple_living::user_domain::AddFavoriteResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        simple_living::user_domain::AddFavoriteRequest dreq;
        dreq.set_user_id(actor.user_id);
        dreq.set_guide_card_id(req->guide_card_id());
        brpc::Controller cntl;
        user_stub_.AddFavorite(&cntl, &dreq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void DeleteMeFavorite(::google::protobuf::RpcController* controller_base,
                          const RemoveFavoriteHttpRequest* req,
                          ::simple_living::user_domain::RemoveFavoriteResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        simple_living::user_domain::RemoveFavoriteRequest dreq;
        dreq.set_user_id(actor.user_id);
        std::string fid = req->favorite_id();
        dreq.set_favorite_id(fid);
        brpc::Controller cntl;
        user_stub_.RemoveFavorite(&cntl, &dreq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void MeHistoryHttp(::google::protobuf::RpcController* controller_base,
                       const ::google::protobuf::Empty*,
                       ::google::protobuf::Empty*,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        const std::string action = outer->http_request().unresolved_path();
        if (action == "list") {
            simple_living::user_domain::ListHistoryRequest dreq;
            ListHistoryHttpRequest hreq;
            if (JsonBodyToMessage(outer, &hreq)) {
                if (hreq.limit() > 0) dreq.mutable_page()->set_limit(hreq.limit());
                if (hreq.has_cursor()) dreq.mutable_page()->set_cursor(hreq.cursor());
            }
            ResolvedActor actor;
            if (ResolveActor(&user_stub_, outer, &actor)) {
                if (actor.kind == ActorKind::kUser) {
                    dreq.set_user_id(actor.user_id);
                } else {
                    dreq.set_session_id(actor.session_id);
                }
            }
            simple_living::user_domain::ListHistoryResponse resp;
            brpc::Controller cntl;
            user_stub_.ListHistory(&cntl, &dreq, &resp, nullptr);
            if (cntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, cntl);
                return;
            }
            FinishOk(outer, resp);
            return;
        }
        if (action == "clear") {
            ClearHistoryHttpRequest hreq;
            if (!JsonBodyToMessage(outer, &hreq)) {
                FinishBizError(outer, 10002, "Invalid JSON body", brpc::HTTP_STATUS_BAD_REQUEST);
                return;
            }
            simple_living::user_domain::ClearHistoryRequest dreq;
            if (hreq.has_acting_user_id()) {
                dreq.set_user_id(hreq.acting_user_id());
            } else if (hreq.has_session_id()) {
                dreq.set_session_id(hreq.session_id());
            } else {
                ResolvedActor actor;
                if (ResolveActor(&user_stub_, outer, &actor)) {
                    if (actor.kind == ActorKind::kUser) {
                        dreq.set_user_id(actor.user_id);
                    } else {
                        dreq.set_session_id(actor.session_id);
                    }
                }
            }
            dreq.set_scope(ParseClearHistoryScope(hreq.scope()));
            simple_living::user_domain::ClearHistoryResponse resp;
            brpc::Controller cntl;
            user_stub_.ClearHistory(&cntl, &dreq, &resp, nullptr);
            if (cntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, cntl);
                return;
            }
            FinishOk(outer, resp);
            return;
        }
        FinishBizError(outer, 10002, "Unknown history action", brpc::HTTP_STATUS_BAD_REQUEST);
    }

    void PostMeHistoryEvent(::google::protobuf::RpcController* controller_base,
                            const RecordHistoryEventHttpRequest* req,
                            ::simple_living::user_domain::RecordHistoryEventResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        simple_living::user_domain::RecordHistoryEventRequest dreq;
        if (req->has_acting_user_id()) {
            dreq.set_user_id(req->acting_user_id());
        } else if (req->has_session_id()) {
            dreq.set_session_id(req->session_id());
        } else {
            ResolvedActor actor;
            if (ResolveActor(&user_stub_, outer, &actor)) {
                if (actor.kind == ActorKind::kUser) {
                    dreq.set_user_id(actor.user_id);
                } else {
                    dreq.set_session_id(actor.session_id);
                }
            }
        }
        if (req->has_content_ref()) {
            *dreq.mutable_content_ref() = req->content_ref();
            if (!dreq.content_ref().has_type() && dreq.content_ref().has_guide_card_id()) {
                dreq.mutable_content_ref()->set_type(simple_living::user_domain::CONTENT_REF_TYPE_GUIDE_CARD);
            }
        }
        brpc::Controller cntl;
        user_stub_.RecordHistoryEvent(&cntl, &dreq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void PostMeFeedback(::google::protobuf::RpcController* controller_base,
                        const SubmitFeedbackHttpRequest* req,
                        ::simple_living::user_domain::SubmitFeedbackResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        simple_living::user_domain::SubmitFeedbackRequest dreq;
        if (req->has_acting_user_id()) {
            dreq.set_user_id(req->acting_user_id());
        } else if (req->has_session_id()) {
            dreq.set_session_id(req->session_id());
        } else {
            ResolvedActor actor;
            if (ResolveActor(&user_stub_, outer, &actor)) {
                if (actor.kind == ActorKind::kUser) {
                    dreq.set_user_id(actor.user_id);
                } else {
                    dreq.set_session_id(actor.session_id);
                }
            }
        }
        dreq.set_target_type(ParseFeedbackTargetTypeStr(req->target_type()));
        dreq.set_target_id(req->target_id());
        brpc::Controller cntl;
        user_stub_.SubmitFeedback(&cntl, &dreq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void MeConsentHttp(::google::protobuf::RpcController* controller_base,
                       const ::google::protobuf::Empty*,
                       ::google::protobuf::Empty*,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        const std::string action = outer->http_request().unresolved_path();
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        if (action == "get") {
            simple_living::user_domain::GetConsentRequest dreq;
            dreq.set_user_id(actor.user_id);
            simple_living::user_domain::GetConsentResponse resp;
            brpc::Controller cntl;
            user_stub_.GetConsent(&cntl, &dreq, &resp, nullptr);
            if (cntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, cntl);
                return;
            }
            FinishOk(outer, resp);
            return;
        }
        if (action == "update") {
            simple_living::user_domain::UpdateConsentRequest dreq;
            if (!JsonBodyToMessage(outer, &dreq)) {
                FinishBizError(outer, 10002, "Invalid JSON body", brpc::HTTP_STATUS_BAD_REQUEST);
                return;
            }
            dreq.set_user_id(actor.user_id);
            simple_living::user_domain::UpdateConsentResponse resp;
            brpc::Controller cntl;
            user_stub_.UpdateConsent(&cntl, &dreq, &resp, nullptr);
            if (cntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, cntl);
                return;
            }
            FinishOk(outer, resp);
            return;
        }
        FinishBizError(outer, 10002, "Unknown consent action", brpc::HTTP_STATUS_BAD_REQUEST);
    }

    void GetMeSummary(::google::protobuf::RpcController* controller_base,
                      const ::simple_living::user_domain::GetMeSummaryRequest* req,
                      ::simple_living::user_domain::GetMeSummaryResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        simple_living::user_domain::GetMeSummaryRequest dreq = *req;
        if (!dreq.has_user_id() && !dreq.has_session_id()) {
            ResolvedActor actor;
            if (ResolveActor(&user_stub_, outer, &actor)) {
                if (actor.kind == ActorKind::kUser) {
                    dreq.set_user_id(actor.user_id);
                } else {
                    dreq.set_session_id(actor.session_id);
                }
            }
        }
        brpc::Controller cntl;
        user_stub_.GetMeSummary(&cntl, &dreq, resp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        FinishOk(outer, *resp);
    }

    void GetHealth(::google::protobuf::RpcController* controller_base,
                   const ::simple_living::user_domain::HealthCheckRequest* req,
                   ::simple_living::user_domain::HealthCheckResponse* resp,
                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        brpc::Controller cntl;
        user_stub_.HealthCheck(&cntl, req, resp, nullptr);
        if (cntl.Failed()) {
            resp->Clear();
            resp->set_status("degraded");
            FinishOk(outer, *resp);
            return;
        }
        FinishOk(outer, *resp);
    }
};

}  // namespace user

namespace pages {

class GatewayPagesEdgeV2Impl : public GatewayPagesEdgeV2 {
    brpc::Channel rec_ch_;
    brpc::Channel content_ch_;
    brpc::Channel tracking_ch_;
    simple_living::recommendation_domain::RecommendationService_Stub rec_stub_;
    simple_living::content_domain::ContentService_Stub content_stub_;
    simple_living::tracking_domain::TrackingLinkService_Stub tracking_stub_;

public:
    GatewayPagesEdgeV2Impl()
        : rec_stub_(&rec_ch_),
          content_stub_(&content_ch_),
          tracking_stub_(&tracking_ch_) {
        InitChannel(&rec_ch_, FLAGS_recommendation_domain_addr);
        InitChannel(&content_ch_, FLAGS_content_domain_addr);
        InitChannel(&tracking_ch_, FLAGS_tracking_domain_addr);
    }

    void GetHomeFeed(::google::protobuf::RpcController* controller_base,
                     const HomeFeedRequest* req,
                     HomeFeedResponse* resp,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        brpc::Controller cntl;
        simple_living::recommendation_domain::QueryRecommendationsRequest rreq;
        rreq.set_scene(simple_living::recommendation_domain::HOME_FEED);
        if (!req->theme().empty()) {
            rreq.mutable_context()->set_theme(req->theme());
        }
        int lim = (req->limit() > 0) ? req->limit() : 20;
        rreq.mutable_cursor_limits()->set_limit(lim);
        if (!req->cursor().empty()) {
            rreq.mutable_cursor_limits()->set_cursor(req->cursor());
        }
        simple_living::recommendation_domain::QueryRecommendationsResponse rresp;
        rec_stub_.QueryRecommendations(&cntl, &rreq, &rresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        for (const auto& item : rresp.items()) {
            auto* fi = resp->add_items();
            fi->set_recommendation_id(rresp.recommendation_id());
            fi->set_scene("home_feed");
            fi->set_rank(item.rank());
            fi->set_guide_card_id(item.guide_card_id());
            auto* gs = fi->mutable_guide_card();
            gs->set_guide_card_id(item.guide_card_id());
            gs->set_title("Guide " + item.guide_card_id());
            for (const auto& rt : item.reason_tags()) {
                fi->add_reason_tags(rt);
            }
        }
        if (rresp.has_cursor_pagination()) {
            resp->mutable_pagination()->set_next_cursor(rresp.cursor_pagination().next_cursor());
            resp->mutable_pagination()->set_has_more(rresp.cursor_pagination().has_more());
            resp->mutable_pagination()->set_limit(lim);
        }
        FinishOk(outer, *resp);
    }

    void GetGuideDetail(::google::protobuf::RpcController* controller_base,
                        const GuideDetailRequest* req,
                        GuideDetailResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        brpc::Controller cntl;
        simple_living::content_domain::BatchGetGuideCardsRequest creq;
        creq.add_card_ids(req->guide_card_id());
        simple_living::content_domain::BatchGetGuideCardsResponse cresp;
        content_stub_.BatchGetGuideCards(&cntl, &creq, &cresp, nullptr);
        if (cntl.Failed() || cresp.cards_size() == 0) {
            if (!cntl.Failed()) {
                FinishBizError(outer, 30001, "Not found", brpc::HTTP_STATUS_NOT_FOUND);
            } else {
                FinishDownstreamBrpcFailure(outer, cntl);
            }
            return;
        }
        const auto& card = cresp.cards(0);
        auto* detail = resp->mutable_guide_card();
        detail->set_guide_card_id(card.card_id());
        detail->set_title(card.title());
        detail->set_subtitle(card.subtitle());
        detail->set_is_commercial(card.commercial_disclosure_required());
        if (card.has_cover_media()) {
            detail->set_cover_url(card.cover_media().url());
        }
        FinishOk(outer, *resp);
    }

    void PrepareRedirect(::google::protobuf::RpcController* controller_base,
                         const RedirectPrepareRequest* req,
                         RedirectPrepareResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        brpc::Controller cntl;
        simple_living::tracking_domain::AssembleTrackingLinkRequest treq;
        treq.mutable_content_ref()->set_guide_card_id(req->guide_card_id());
        treq.mutable_content_ref()->set_recommendation_id(req->recommendation_id());
        treq.mutable_content_ref()->set_scene(req->scene());
        treq.mutable_content_ref()->set_item_rank(req->item_rank());
        simple_living::tracking_domain::AssembleTrackingLinkResponse tresp;
        tracking_stub_.AssembleTrackingLink(&cntl, &treq, &tresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        resp->set_landing_url(tresp.landing_url());
        if (tresp.has_attribution_echo() && !tresp.attribution_echo().guide_card_id().empty()) {
            resp->set_click_id(tresp.attribution_echo().guide_card_id());
        } else if (!tresp.link_ref().empty()) {
            resp->set_click_id(tresp.link_ref());
        } else {
            resp->set_click_id(tresp.short_token());
        }
        if (tresp.has_attribution_echo()) {
            resp->mutable_attribution()->set_channel_code(tresp.attribution_echo().placement());
            resp->mutable_attribution()->set_scene(req->scene());
            resp->mutable_attribution()->set_item_rank(req->item_rank());
        }
        FinishOk(outer, *resp);
    }

    void GetMeSummary(::google::protobuf::RpcController* controller_base,
                      const MeSummaryRequest*,
                      MeSummaryResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        resp->mutable_profile()->set_is_guest(true);
        resp->mutable_counts()->set_favorites_count(0);
        resp->mutable_counts()->set_history_count(0);
        FinishOk(outer, *resp);
    }
};

}  // namespace pages

}  // namespace gateway
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_user_domain_addr == kDefaultUserDomainAddr) {
        if (const char* v = std::getenv("GATEWAY_USER_DOMAIN_ADDR"); v != nullptr && v[0] != '\0') {
            FLAGS_user_domain_addr = v;
        }
    }
    if (FLAGS_content_domain_addr == kDefaultContentDomainAddr) {
        if (const char* v = std::getenv("GATEWAY_CONTENT_DOMAIN_ADDR"); v != nullptr && v[0] != '\0') {
            FLAGS_content_domain_addr = v;
        }
    }
    if (FLAGS_recommendation_domain_addr == kDefaultRecommendationDomainAddr) {
        if (const char* v = std::getenv("GATEWAY_RECOMMENDATION_DOMAIN_ADDR"); v != nullptr && v[0] != '\0') {
            FLAGS_recommendation_domain_addr = v;
        }
    }
    if (FLAGS_tracking_domain_addr == kDefaultTrackingDomainAddr) {
        if (const char* v = std::getenv("GATEWAY_TRACKING_DOMAIN_ADDR"); v != nullptr && v[0] != '\0') {
            FLAGS_tracking_domain_addr = v;
        }
    }

    brpc::Server server;

    simple_living::gateway::user::GatewayUserEdgeV2Impl g_user_edge;
    const char kUserRestful[] =
        "/api/v2/guest/session              => PostGuestSession,"
        "/api/v2/auth/token/issue           => PostAuthTokenIssue,"
        "/api/v2/auth/token/refresh         => PostAuthTokenRefresh,"
        "/api/v2/auth/session/revoke        => DeleteAuthSession,"
        "/api/v2/me/profile/*               => MeProfileHttp,"
        "/api/v2/me/preferences/*           => MePreferencesHttp,"
        "/api/v2/me/favorites/list          => GetMeFavorites,"
        "/api/v2/me/favorites/add           => PostMeFavorite,"
        "/api/v2/me/favorites/remove        => DeleteMeFavorite,"
        "/api/v2/me/history/*               => MeHistoryHttp,"
        "/api/v2/me/history/events          => PostMeHistoryEvent,"
        "/api/v2/me/feedback                => PostMeFeedback,"
        "/api/v2/me/consent/*               => MeConsentHttp,"
        "/api/v2/me/summary/get             => GetMeSummary,"
        "/api/v2/health/check               => GetHealth";
    if (server.AddService(&g_user_edge, brpc::SERVER_DOESNT_OWN_SERVICE, kUserRestful) != 0) {
        LOG(ERROR) << "Fail to add GatewayUserEdgeV2";
        return 1;
    }

    simple_living::gateway::pages::GatewayPagesEdgeV2Impl g_pages_edge;
    const char kPagesRestful[] =
        "/api/v2/pages/home_feed       => GetHomeFeed,"
        "/api/v2/pages/guide_detail    => GetGuideDetail,"
        "/api/v2/pages/redirect_prepare => PrepareRedirect,"
        "/api/v2/pages/me_summary      => GetMeSummary";
    if (server.AddService(&g_pages_edge, brpc::SERVER_DOESNT_OWN_SERVICE, kPagesRestful) != 0) {
        LOG(ERROR) << "Fail to add GatewayPagesEdgeV2";
        return 1;
    }

    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start gateway server";
        return 1;
    }
    LOG(INFO) << "gateway server listening on port " << FLAGS_port;
    server.RunUntilAskedToQuit();
    return 0;
}
