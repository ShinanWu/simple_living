// gateway brpc edge server — HTTP+JSON front, calls downstream brpc services
#include <google/protobuf/empty.pb.h>
#include <google/protobuf/util/json_util.h>
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <brpc/channel.h>
#include <butil/logging.h>
#include <sys/time.h>

#include <cctype>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "gateway_user_edge.pb.h"
#include "gateway_pages_edge.pb.h"
#include "user_server_service.pb.h"
#include "recommendation_server_service.pb.h"
#include "content_service.pb.h"
#include "tracking_server.pb.h"
#include "governance_server.pb.h"
#include "affiliate_server.pb.h"
#include "gateway_backoffice_helpers.h"
#include "gateway_backoffice_media.h"

namespace {
constexpr const char* kDefaultUserServerAddr =
    "brpc://user-server.simple-living.svc.cluster.local:9101";
constexpr const char* kDefaultRecommendationServerAddr =
    "brpc://recommendation-server.simple-living.svc.cluster.local:9103";
constexpr const char* kDefaultTrackingServerAddr =
    "brpc://tracking-server.simple-living.svc.cluster.local:9105";
constexpr const char* kDefaultBackofficeBackendAddr =
    "brpc://backoffice-backend.simple-living.svc.cluster.local:9110";
}  // namespace

DEFINE_int32(port, 8080, "TCP port for this brpc server");
DEFINE_string(user_server_addr, kDefaultUserServerAddr, "user-server address");
DEFINE_string(recommendation_server_addr, kDefaultRecommendationServerAddr, "recommendation-server address");
DEFINE_string(tracking_server_addr, kDefaultTrackingServerAddr, "tracking-server address");
DEFINE_string(backoffice_backend_addr, kDefaultBackofficeBackendAddr, "platform/backoffice-backend address");

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
    outer->http_response().SetHeader("Access-Control-Allow-Origin", "*");
    outer->http_response().SetHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    outer->http_response().SetHeader("Access-Control-Allow-Headers",
                                     "Content-Type, Authorization, X-Guest-Session-Id, X-Request-Id, X-Backoffice-Role");
    outer->response_attachment().append(oss.str());
}

void FinishCorsPreflight(brpc::Controller* outer) {
    outer->http_response().set_status_code(brpc::HTTP_STATUS_NO_CONTENT);
    outer->http_response().SetHeader("Access-Control-Allow-Origin", "*");
    outer->http_response().SetHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    outer->http_response().SetHeader("Access-Control-Allow-Headers",
                                     "Content-Type, Authorization, X-Guest-Session-Id, X-Request-Id, X-Backoffice-Role");
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

simple_living::user_server::ClientPlatform ParseClientPlatform(const std::string& s) {
    if (s == "web") {
        return simple_living::user_server::CLIENT_PLATFORM_WEB;
    }
    if (s == "ios") {
        return simple_living::user_server::CLIENT_PLATFORM_IOS;
    }
    if (s == "android") {
        return simple_living::user_server::CLIENT_PLATFORM_ANDROID;
    }
    if (s == "wechat_miniprogram") {
        return simple_living::user_server::CLIENT_PLATFORM_WECHAT_MINIPROGRAM;
    }
    if (s == "douyin_miniprogram") {
        return simple_living::user_server::CLIENT_PLATFORM_DOUYIN_MINIPROGRAM;
    }
    return simple_living::user_server::CLIENT_PLATFORM_UNSPECIFIED;
}

simple_living::user_server::RevokeSessionScope ParseRevokeScope(const std::string& s) {
    if (s == "all_user_sessions") {
        return simple_living::user_server::REVOKE_SESSION_SCOPE_ALL_USER_SESSIONS;
    }
    return simple_living::user_server::REVOKE_SESSION_SCOPE_SINGLE_SESSION;
}

simple_living::user_server::ClearHistoryScope ParseClearHistoryScope(const std::string& s) {
    if (s == "before_time") {
        return simple_living::user_server::CLEAR_HISTORY_SCOPE_BEFORE_TIME;
    }
    return simple_living::user_server::CLEAR_HISTORY_SCOPE_ALL;
}

void MapHttpContentRefToUser(const user::HttpContentRef& href,
                             simple_living::user_server::ContentRef* out) {
    if (href.type() == "guide_card" || !href.guide_card_id().empty()) {
        out->set_type(simple_living::user_server::CONTENT_REF_TYPE_GUIDE_CARD);
        out->set_content_id(href.guide_card_id());
    }
}

simple_living::user_server::FeedbackTargetType ParseFeedbackTargetTypeStr(const std::string& n) {
    if (n == "guide_card") {
        return simple_living::user_server::FEEDBACK_TARGET_TYPE_GUIDE_CARD;
    }
    if (n == "recommendation_result") {
        return simple_living::user_server::FEEDBACK_TARGET_TYPE_RECOMMENDATION_RESULT;
    }
    if (n == "app") {
        return simple_living::user_server::FEEDBACK_TARGET_TYPE_APP;
    }
    if (n == "other") {
        return simple_living::user_server::FEEDBACK_TARGET_TYPE_OTHER;
    }
    return simple_living::user_server::FEEDBACK_TARGET_TYPE_UNSPECIFIED;
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
    if (outer->http_request().method() == brpc::HTTP_METHOD_OPTIONS) {
        FinishCorsPreflight(outer);
        return false;
    }
    if (outer->http_request().method() != brpc::HTTP_METHOD_POST) {
        FinishBizError(outer, 10001, "Method not allowed, use POST", brpc::HTTP_STATUS_METHOD_NOT_ALLOWED);
        return false;
    }
    return true;
}

bool EnsureReadHttp(brpc::Controller* outer) {
    const auto m = outer->http_request().method();
    if (m == brpc::HTTP_METHOD_OPTIONS) {
        FinishCorsPreflight(outer);
        return false;
    }
    if (m == brpc::HTTP_METHOD_GET || m == brpc::HTTP_METHOD_POST) {
        return true;
    }
    FinishBizError(outer, 10001, "Method not allowed, use GET or POST", brpc::HTTP_STATUS_METHOD_NOT_ALLOWED);
    return false;
}

std::string UrlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            auto hex = in.substr(i + 1, 2);
            char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            out.push_back(ch);
            i += 2;
        } else if (in[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

std::string QueryParam(const brpc::Controller* outer, const char* key) {
    const std::string& q = outer->http_request().uri().query();
    if (q.empty()) {
        return "";
    }
    const std::string prefix = std::string(key) + "=";
    size_t pos = q.find(prefix);
    if (pos == std::string::npos) {
        return "";
    }
    pos += prefix.size();
    size_t end = q.find('&', pos);
    std::string raw = q.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    return UrlDecode(raw);
}

enum class ActorKind { kNone = 0, kUser, kGuest };

struct ResolvedActor {
    ActorKind kind = ActorKind::kNone;
    std::string user_id;
    std::string session_id;
};

bool ResolveActor(simple_living::user_server::UserServerService_Stub* stub,
                  brpc::Controller* outer,
                  ResolvedActor* out) {
    if (const std::string* auth = outer->http_request().GetHeader("Authorization")) {
        static constexpr char kBearer[] = "Bearer ";
        if (auth->size() > sizeof(kBearer) - 1 &&
            auth->compare(0, sizeof(kBearer) - 1, kBearer) == 0) {
            std::string token = auth->substr(sizeof(kBearer) - 1);
            brpc::Controller c;
            simple_living::user_server::IntrospectAccessTokenRequest ireq;
            ireq.set_access_token(token);
            simple_living::user_server::IntrospectAccessTokenResponse iresp;
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
    simple_living::user_server::UserServerService_Stub user_stub_;

public:
    GatewayUserEdgeV2Impl() : user_stub_(&user_ch_) {
        InitChannel(&user_ch_, FLAGS_user_server_addr);
    }

    void PostGuestSession(::google::protobuf::RpcController* controller_base,
                          const GuestSessionHttpRequest* req,
                          ::simple_living::user_server::EnsureGuestSessionResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        simple_living::user_server::EnsureGuestSessionRequest dreq;
        if (req->has_device_id()) {
            dreq.set_device_id(req->device_id());
        }
        if (req->has_client_platform()) {
            dreq.set_client_platform(ParseClientPlatform(req->client_platform()));
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
                            ::simple_living::user_server::IssueTokenPairResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        if (!req->has_account_proof() || !req->account_proof().has_user_id() ||
            req->account_proof().user_id().empty()) {
            FinishBizError(outer, 10002, "Validation failed", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        simple_living::user_server::IssueTokenPairRequest ireq;
        ireq.set_user_id(req->account_proof().user_id());
        if (req->has_device_fingerprint()) {
            ireq.set_device_fingerprint(req->device_fingerprint());
        }
        if (req->has_client_platform()) {
            ireq.set_client_platform(ParseClientPlatform(req->client_platform()));
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
                              ::simple_living::user_server::IssueTokenPairResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        brpc::Controller cntl;
        simple_living::user_server::IntrospectRefreshTokenRequest rreq;
        rreq.set_refresh_token(req->refresh_token());
        if (req->has_request_context()) {
            *rreq.mutable_request_context() = req->request_context();
        }
        simple_living::user_server::IntrospectRefreshTokenResponse rresp;
        user_stub_.IntrospectRefreshToken(&cntl, &rreq, &rresp, nullptr);
        if (cntl.Failed() || !rresp.valid()) {
            FinishBizError(outer, 20003, "Refresh token invalid or expired", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        cntl.Reset();
        simple_living::user_server::IssueTokenPairRequest ireq;
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
                           ::simple_living::user_server::RevokeSessionResponse* resp,
                           ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        simple_living::user_server::RevokeSessionRequest dreq;
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
            simple_living::user_server::GetProfileRequest dreq;
            ResolvedActor actor;
            if (ResolveActor(&user_stub_, outer, &actor)) {
                if (actor.kind == ActorKind::kUser) {
                    dreq.set_user_id(actor.user_id);
                } else {
                    dreq.set_session_id(actor.session_id);
                }
            }
            simple_living::user_server::GetProfileResponse resp;
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
            simple_living::user_server::UpdateProfileRequest dreq;
            if (!JsonBodyToMessage(outer, &dreq)) {
                FinishBizError(outer, 10002, "Invalid JSON body", brpc::HTTP_STATUS_BAD_REQUEST);
                return;
            }
            dreq.set_user_id(actor.user_id);
            simple_living::user_server::UpdateProfileResponse resp;
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
            simple_living::user_server::GetPreferencesRequest dreq;
            dreq.set_user_id(actor.user_id);
            simple_living::user_server::GetPreferencesResponse resp;
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
            simple_living::user_server::UpdatePreferencesRequest dreq;
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
            simple_living::user_server::UpdatePreferencesResponse resp;
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
                        ::simple_living::user_server::ListFavoritesResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        simple_living::user_server::ListFavoritesRequest dreq;
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
                        ::simple_living::user_server::AddFavoriteResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        simple_living::user_server::AddFavoriteRequest dreq;
        dreq.set_user_id(actor.user_id);
        dreq.set_content_id(req->guide_card_id());
        dreq.set_content_type(simple_living::user_server::CONTENT_REF_TYPE_GUIDE_CARD);
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
                          ::simple_living::user_server::RemoveFavoriteResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        ResolvedActor actor;
        if (!ResolveActor(&user_stub_, outer, &actor) || actor.kind != ActorKind::kUser) {
            FinishBizError(outer, 20001, "Unauthorized", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        simple_living::user_server::RemoveFavoriteRequest dreq;
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
            simple_living::user_server::ListHistoryRequest dreq;
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
            simple_living::user_server::ListHistoryResponse resp;
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
            simple_living::user_server::ClearHistoryRequest dreq;
            ResolvedActor actor;
            if (ResolveActor(&user_stub_, outer, &actor)) {
                if (actor.kind == ActorKind::kUser) {
                    dreq.set_user_id(actor.user_id);
                } else {
                    dreq.set_session_id(actor.session_id);
                }
            }
            dreq.set_scope(ParseClearHistoryScope(hreq.scope()));
            simple_living::user_server::ClearHistoryResponse resp;
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
                            ::simple_living::user_server::RecordHistoryEventResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        simple_living::user_server::RecordHistoryEventRequest dreq;
        ResolvedActor actor;
        if (ResolveActor(&user_stub_, outer, &actor)) {
            if (actor.kind == ActorKind::kUser) {
                dreq.set_user_id(actor.user_id);
            } else {
                dreq.set_session_id(actor.session_id);
            }
        }
        if (req->has_content_ref()) {
            MapHttpContentRefToUser(req->content_ref(), dreq.mutable_content_ref());
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
                        ::simple_living::user_server::SubmitFeedbackResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        simple_living::user_server::SubmitFeedbackRequest dreq;
        ResolvedActor actor;
        if (ResolveActor(&user_stub_, outer, &actor)) {
            if (actor.kind == ActorKind::kUser) {
                dreq.set_user_id(actor.user_id);
            } else {
                dreq.set_session_id(actor.session_id);
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
            simple_living::user_server::GetConsentRequest dreq;
            dreq.set_user_id(actor.user_id);
            simple_living::user_server::GetConsentResponse resp;
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
            simple_living::user_server::UpdateConsentRequest dreq;
            if (!JsonBodyToMessage(outer, &dreq)) {
                FinishBizError(outer, 10002, "Invalid JSON body", brpc::HTTP_STATUS_BAD_REQUEST);
                return;
            }
            dreq.set_user_id(actor.user_id);
            simple_living::user_server::UpdateConsentResponse resp;
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
                      const ::simple_living::user_server::GetMeSummaryRequest* req,
                      ::simple_living::user_server::GetMeSummaryResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        simple_living::user_server::GetMeSummaryRequest dreq = *req;
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
                   const ::simple_living::user_server::HealthCheckRequest* req,
                   ::simple_living::user_server::HealthCheckResponse* resp,
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

namespace bo = backoffice_helpers;
namespace bo_media = backoffice_media;

bool ValidatePersistableCoverUrl(const std::string& url, brpc::Controller* outer) {
    if (url.empty() || bo_media::IsPersistableMediaUrl(url)) {
        return true;
    }
    FinishBizError(outer, 10002,
                   "cover_media.url must be an absolute http(s) media URL from POST /api/v2/backoffice/media/upload",
                   brpc::HTTP_STATUS_BAD_REQUEST);
    return false;
}

class GatewayPagesEdgeV2Impl : public GatewayPagesEdgeV2 {
    brpc::Channel rec_ch_;
    brpc::Channel backoffice_ch_;
    brpc::Channel tracking_ch_;
    simple_living::recommendation_server::RecommendationService_Stub rec_stub_;
    simple_living::content_server::ContentService_Stub catalog_stub_;
    simple_living::content_server::ContentService_Stub bo_content_stub_;
    simple_living::tracking_server::TrackingLinkService_Stub tracking_stub_;
    simple_living::governance_server::GovernanceVisibilityService_Stub bo_visibility_stub_;
    simple_living::governance_server::GovernanceReviewService_Stub bo_review_stub_;
    simple_living::affiliate_server::AffiliatePartnerService_Stub bo_affiliate_stub_;

    bool IsContentVisible(const std::string& content_id) {
        if (content_id.empty()) {
            return false;
        }
        brpc::Controller cntl;
        simple_living::content_server::BatchGetGuideCardsRequest req;
        req.add_card_ids(content_id);
        simple_living::content_server::BatchGetGuideCardsResponse resp;
        catalog_stub_.BatchGetGuideCards(&cntl, &req, &resp, nullptr);
        if (cntl.Failed()) {
            LOG(WARNING) << "catalog visibility check failed for " << content_id << ": "
                         << cntl.ErrorText();
            return false;
        }
        return resp.cards_size() > 0;
    }

    void SyncVisibilityForContent(const std::string& content_id, bool published) {
        if (content_id.empty()) {
            return;
        }
        brpc::Controller cntl;
        simple_living::governance_server::SetVisibilityVerdictRequest req;
        req.set_content_id(content_id);
        req.set_state(published ? simple_living::governance_server::VISIBILITY_STATE_PUBLISHED
                                : simple_living::governance_server::VISIBILITY_STATE_UNPUBLISHED);
        req.set_reason_code(published ? "content_published" : "content_unpublished");
        req.set_source(simple_living::governance_server::VISIBILITY_VERDICT_SOURCE_MANUAL_OPS);
        simple_living::governance_server::SetVisibilityVerdictResponse resp;
        bo_visibility_stub_.SetVisibilityVerdict(&cntl, &req, &resp, nullptr);
        if (cntl.Failed()) {
            LOG(WARNING) << "SetVisibilityVerdict failed for " << content_id << ": " << cntl.ErrorText();
        }
    }

public:
    GatewayPagesEdgeV2Impl()
        : rec_stub_(&rec_ch_),
          catalog_stub_(&rec_ch_),
          bo_content_stub_(&backoffice_ch_),
          tracking_stub_(&tracking_ch_),
          bo_visibility_stub_(&backoffice_ch_),
          bo_review_stub_(&backoffice_ch_),
          bo_affiliate_stub_(&backoffice_ch_) {
        InitChannel(&rec_ch_, FLAGS_recommendation_server_addr);
        InitChannel(&backoffice_ch_, FLAGS_backoffice_backend_addr);
        InitChannel(&tracking_ch_, FLAGS_tracking_server_addr);
    }

    void GetHomeFeed(::google::protobuf::RpcController* controller_base,
                     const HomeFeedRequest* req,
                     HomeFeedResponse* resp,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsureReadHttp(outer)) return;

        HomeFeedRequest parsed = *req;
        if (outer->http_request().method() == brpc::HTTP_METHOD_GET) {
            if (parsed.theme().empty()) {
                parsed.set_theme(QueryParam(outer, "theme"));
            }
            if (parsed.cursor().empty()) {
                parsed.set_cursor(QueryParam(outer, "cursor"));
            }
            if (parsed.limit() <= 0) {
                const std::string lim = QueryParam(outer, "limit");
                if (!lim.empty()) {
                    parsed.set_limit(std::stoi(lim));
                }
            }
        }

        int lim = (parsed.limit() > 0) ? parsed.limit() : 20;
        const std::string theme = parsed.theme().empty() ? "clothing" : parsed.theme();

        if (theme == "clothing") {
            brpc::Controller ccntl;
            simple_living::content_server::ListGuideCardsRequest lreq;
            lreq.set_theme_id(bo::ThemeIdFromTheme(theme));
            lreq.set_limit(lim);
            simple_living::content_server::ListGuideCardsResponse lresp;
            catalog_stub_.ListGuideCards(&ccntl, &lreq, &lresp, nullptr);
            if (ccntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, ccntl);
                return;
            }

            int rank = 1;
            const std::string recommendation_id = "rec_cms_" + std::to_string(NowUnixMs());
            for (const auto& card : lresp.cards()) {
                if (card.content_status() != simple_living::catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED) {
                    continue;
                }
                if (!IsContentVisible(card.card_id())) {
                    continue;
                }
                auto* fi = resp->add_items();
                fi->set_recommendation_id(recommendation_id);
                fi->set_scene("home_feed");
                fi->set_rank(rank++);
                fi->set_guide_card_id(card.card_id());
                auto* gs = fi->mutable_guide_card();
                gs->set_guide_card_id(card.card_id());
                gs->set_title(card.title());
                gs->set_subtitle(card.subtitle());
                gs->set_summary(card.subtitle());
                if (card.has_cover_media() && !card.cover_media().url().empty()) {
                    gs->set_cover_url(
                        bo_media::NormalizeMediaUrlForClient(card.cover_media().url()));
                }
                gs->set_theme(theme);
                fi->add_reason_tags("cms_published");
                fi->add_reason_tags("clothing_pick");
            }
            resp->mutable_pagination()->set_next_cursor(lresp.has_pagination() ? lresp.pagination().next_cursor() : "");
            resp->mutable_pagination()->set_has_more(lresp.has_pagination() && lresp.pagination().has_more());
            resp->mutable_pagination()->set_limit(lim);
            FinishOk(outer, *resp);
            return;
        }

        brpc::Controller cntl;
        simple_living::recommendation_server::QueryRecommendationsRequest rreq;
        rreq.set_scene(simple_living::recommendation_server::HOME_FEED);
        if (!parsed.theme().empty()) {
            rreq.mutable_context()->set_theme(parsed.theme());
        }
        rreq.mutable_cursor_limits()->set_limit(lim);
        if (!parsed.cursor().empty()) {
            rreq.mutable_cursor_limits()->set_cursor(parsed.cursor());
        }
        simple_living::recommendation_server::QueryRecommendationsResponse rresp;
        rec_stub_.QueryRecommendations(&cntl, &rreq, &rresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }

        std::vector<std::string> card_ids;
        card_ids.reserve(static_cast<size_t>(rresp.items_size()));
        for (const auto& item : rresp.items()) {
            card_ids.push_back(item.guide_card_id());
        }

        std::unordered_map<std::string, simple_living::catalog::GuideCard> card_by_id;
        if (!card_ids.empty()) {
            brpc::Controller ccntl;
            simple_living::content_server::BatchGetGuideCardsRequest creq;
            for (const auto& id : card_ids) {
                creq.add_card_ids(id);
            }
            simple_living::content_server::BatchGetGuideCardsResponse cresp;
            catalog_stub_.BatchGetGuideCards(&ccntl, &creq, &cresp, nullptr);
            if (!ccntl.Failed()) {
                for (const auto& card : cresp.cards()) {
                    card_by_id[card.card_id()] = card;
                }
            }
        }

        for (const auto& item : rresp.items()) {
            auto cit = card_by_id.find(item.guide_card_id());
            if (cit == card_by_id.end() ||
                cit->second.content_status() != simple_living::catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED) {
                continue;
            }
            if (!IsContentVisible(cit->second.card_id())) {
                continue;
            }
            auto* fi = resp->add_items();
            fi->set_recommendation_id(rresp.recommendation_id());
            fi->set_scene("home_feed");
            fi->set_rank(item.rank());
            fi->set_guide_card_id(item.guide_card_id());
            auto* gs = fi->mutable_guide_card();
            gs->set_guide_card_id(item.guide_card_id());

            const auto& card = cit->second;
            gs->set_title(card.title());
            gs->set_subtitle(card.subtitle());
            if (!card.selling_points().empty()) {
                gs->set_summary(card.selling_points(0));
            }
            if (card.has_cover_media() && !card.cover_media().url().empty()) {
                gs->set_cover_url(bo_media::NormalizeMediaUrlForClient(card.cover_media().url()));
            }
            gs->set_theme(parsed.theme());
            fi->add_reason_tags("clothing_pick");
            fi->add_reason_tags("tmall_deal");

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
        if (!EnsureReadHttp(outer)) return;

        std::string guide_card_id = req->guide_card_id();
        if (guide_card_id.empty()) {
            guide_card_id = QueryParam(outer, "guide_card_id");
        }
        if (guide_card_id.empty()) {
            FinishBizError(outer, 10002, "guide_card_id required", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        brpc::Controller cntl;
        simple_living::content_server::BatchGetGuideCardsRequest creq;
        creq.add_card_ids(guide_card_id);
        simple_living::content_server::BatchGetGuideCardsResponse cresp;
        catalog_stub_.BatchGetGuideCards(&cntl, &creq, &cresp, nullptr);
        if (cntl.Failed() || cresp.cards_size() == 0) {
            if (!cntl.Failed()) {
                FinishBizError(outer, 30001, "Not found", brpc::HTTP_STATUS_NOT_FOUND);
            } else {
                FinishDownstreamBrpcFailure(outer, cntl);
            }
            return;
        }
        const auto& card = cresp.cards(0);
        if (card.content_status() != simple_living::catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED) {
            FinishBizError(outer, 30001, "Not found", brpc::HTTP_STATUS_NOT_FOUND);
            return;
        }
        if (!IsContentVisible(card.card_id())) {
            FinishBizError(outer, 30001, "Not found", brpc::HTTP_STATUS_NOT_FOUND);
            return;
        }
        auto* detail = resp->mutable_guide_card();
        detail->set_guide_card_id(card.card_id());
        detail->set_title(card.title());
        detail->set_subtitle(card.subtitle());
        if (!card.selling_points().empty()) {
            detail->set_summary(card.selling_points(0));
        } else {
            detail->set_summary(card.subtitle());
        }
        detail->set_is_commercial(card.commercial_disclosure_required());
        if (card.has_cover_media()) {
            detail->set_cover_url(
                bo_media::NormalizeMediaUrlForClient(card.cover_media().url()));
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
        brpc::Controller content_cntl;
        simple_living::content_server::BatchGetGuideCardsRequest content_req;
        content_req.add_card_ids(req->guide_card_id());
        simple_living::content_server::BatchGetGuideCardsResponse content_resp;
        catalog_stub_.BatchGetGuideCards(&content_cntl, &content_req, &content_resp, nullptr);
        if (content_cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, content_cntl);
            return;
        }
        if (content_resp.cards_size() == 0 ||
            content_resp.cards(0).content_status() != simple_living::catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED) {
            FinishBizError(outer, 30001, "Not found", brpc::HTTP_STATUS_NOT_FOUND);
            return;
        }
        if (!IsContentVisible(req->guide_card_id())) {
            FinishBizError(outer, 30001, "Not found", brpc::HTTP_STATUS_NOT_FOUND);
            return;
        }
        std::string landing_url;
        if (!content_resp.cards(0).affiliate_refs().empty()) {
            const auto& aff = content_resp.cards(0).affiliate_refs(0);
            auto it = aff.payload().find("landing_url");
            if (it != aff.payload().end()) {
                landing_url = it->second;
            }
        }
        brpc::Controller cntl;
        simple_living::tracking_server::AssembleTrackingLinkRequest treq;
        treq.mutable_content_ref()->set_guide_card_id(req->guide_card_id());
        treq.mutable_content_ref()->set_recommendation_id(req->recommendation_id());
        treq.mutable_content_ref()->set_scene(req->scene());
        treq.mutable_content_ref()->set_item_rank(req->item_rank());
        treq.set_placement(req->preferred_channel_code().empty() ? "mini_program" : req->preferred_channel_code());
        treq.set_landing_url(landing_url);
        simple_living::tracking_server::AssembleTrackingLinkResponse tresp;
        tracking_stub_.AssembleTrackingLink(&cntl, &treq, &tresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        resp->set_landing_url(tresp.landing_url());
        if (!tresp.link_ref().empty()) {
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
        if (!EnsureReadHttp(outer)) return;
        resp->mutable_profile()->set_is_guest(true);
        resp->mutable_counts()->set_favorites_count(0);
        resp->mutable_counts()->set_history_count(0);
        FinishOk(outer, *resp);
    }

    void PostBackofficeLogin(::google::protobuf::RpcController* controller_base,
                             const BackofficeLoginRequest* req,
                             BackofficeLoginResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer)) return;
        std::string session_token;
        if (!bo::ValidateBackofficeLoginToken(req->access_token(), &session_token)) {
            FinishBizError(outer, 20004, "Invalid credentials", brpc::HTTP_STATUS_UNAUTHORIZED);
            return;
        }
        resp->set_success(true);
        resp->set_token(session_token);
        resp->set_role("backoffice_admin");
        FinishOk(outer, *resp);
    }

    void PostBackofficeMediaUpload(::google::protobuf::RpcController* controller_base,
                                   const BackofficeMediaUploadRequest* req,
                                   BackofficeMediaUploadResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (!bo_media::SaveBackofficeMediaUpload(*req, resp)) {
            FinishBizError(outer, 10002,
                           "invalid media upload or gateway_backoffice_media_public_base_url not configured",
                           brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        FinishOk(outer, *resp);
    }

    void GetBackofficeMediaAsset(::google::protobuf::RpcController* controller_base,
                                 const ::google::protobuf::Empty*,
                                 ::google::protobuf::Empty*,
                                 ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsureReadHttp(outer)) return;
        const std::string unresolved = outer->http_request().unresolved_path();
        const auto slash = unresolved.rfind('/');
        const std::string filename =
            slash == std::string::npos ? unresolved : unresolved.substr(slash + 1);
        if (!bo_media::ServeBackofficeMediaAsset(filename, outer)) {
            FinishBizError(outer, 10003, "media not found", brpc::HTTP_STATUS_NOT_FOUND);
        }
    }

    void GetBackofficeAffiliatePartners(::google::protobuf::RpcController* controller_base,
                                 const BackofficeListPartnersRequest*,
                                 BackofficePartnersResponse* resp,
                                 ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        brpc::Controller cntl;
        simple_living::affiliate_server::ListPartnersRequest req;
        simple_living::affiliate_server::ListPartnersResponse aresp;
        bo_affiliate_stub_.ListPartners(&cntl, &req, &aresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        for (const auto& snap : aresp.items()) {
            *resp->add_items() = bo::PartnerFromSnapshot(snap);
        }
        FinishOk(outer, *resp);
    }

    void PostBackofficeAffiliatePartner(::google::protobuf::RpcController* controller_base,
                                 const BackofficeCreatePartnerRequest* req,
                                 BackofficePartnersResponse* resp,
                                 ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->partner_id().empty() || req->display_name().empty()) {
            FinishBizError(outer, 10002, "Validation failed", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        brpc::Controller cntl;
        simple_living::affiliate_server::UpsertPartnerBackofficeRequest ureq;
        *ureq.mutable_partner() = bo::SnapshotFromPartnerRequest(*req);
        simple_living::affiliate_server::UpsertPartnerBackofficeResponse uresp;
        bo_affiliate_stub_.UpsertPartnerBackoffice(&cntl, &ureq, &uresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        brpc::Controller lcntl;
        simple_living::affiliate_server::ListPartnersRequest lreq;
        simple_living::affiliate_server::ListPartnersResponse lresp;
        bo_affiliate_stub_.ListPartners(&lcntl, &lreq, &lresp, nullptr);
        if (lcntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, lcntl);
            return;
        }
        for (const auto& snap : lresp.items()) {
            *resp->add_items() = bo::PartnerFromSnapshot(snap);
        }
        FinishOk(outer, *resp);
    }

    void GetBackofficeContentItems(::google::protobuf::RpcController* controller_base,
                            const BackofficeListContentItemsRequest*,
                            BackofficeContentItemsResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (!LoadBackofficeContentItems(resp, outer)) {
            return;
        }
        FinishOk(outer, *resp);
    }

    void PostBackofficeContentItem(::google::protobuf::RpcController* controller_base,
                            const BackofficeCreateContentItemRequest* req,
                            BackofficeContentItemsResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->title().empty() || req->landing_url().empty()) {
            FinishBizError(outer, 10002, "title and landing_url required", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        if (req->has_resource_kind() && !req->resource_kind().empty() &&
            req->resource_kind() != "guide_card") {
            FinishBizError(outer, 10002, "only guide_card is supported", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        if (req->has_cover_media() && !req->cover_media().url().empty() &&
            !ValidatePersistableCoverUrl(req->cover_media().url(), outer)) {
            return;
        }

        simple_living::catalog::GuideCard card = bo::BuildGuideCardFromCreate(*req);
        const std::string initial = !req->initial_status().empty() ? req->initial_status()
            : (!req->status().empty() ? req->status() : "draft");
        if (initial == "published") {
            FinishBizError(outer, 10002, "submit for review and publish after approval",
                           brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        brpc::Controller cntl;
        simple_living::content_server::UpsertGuideCardRequest creq;
        *creq.mutable_guide_card() = card;
        simple_living::content_server::UpsertGuideCardResponse cresp;
        bo_content_stub_.UpsertGuideCard(&cntl, &creq, &cresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        SyncVisibilityForContent(cresp.card_id(), false);

        if (!LoadBackofficeContentItems(resp, outer)) {
            return;
        }
        FinishOk(outer, *resp);
    }

    void PatchBackofficeContentItemStatus(::google::protobuf::RpcController* controller_base,
                                   const BackofficeUpdateContentStatusRequest* req,
                                   BackofficeContentItemsResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->content_id().empty() || req->status().empty()) {
            FinishBizError(outer, 10002, "Validation failed", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        if (!UpdateContentStatus(req->content_id(), req->status(), outer)) {
            return;
        }
        if (!LoadBackofficeContentItems(resp, outer)) {
            return;
        }
        FinishOk(outer, *resp);
    }

    void PostBackofficeContentItemUpdate(::google::protobuf::RpcController* controller_base,
                                const BackofficeUpdateContentItemRequest* req,
                                BackofficeContentItemsResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->content_id().empty()) {
            FinishBizError(outer, 10002, "content_id required", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        brpc::Controller gcntl;
        simple_living::content_server::BatchGetGuideCardsRequest greq;
        greq.add_card_ids(req->content_id());
        simple_living::content_server::BatchGetGuideCardsResponse gresp;
        bo_content_stub_.BatchGetGuideCards(&gcntl, &greq, &gresp, nullptr);
        if (gcntl.Failed() || gresp.cards_size() == 0) {
            FinishBizError(outer, 10003, "content not found", brpc::HTTP_STATUS_NOT_FOUND);
            return;
        }

        simple_living::catalog::GuideCard card = gresp.cards(0);
        if (req->has_revision() && card.revision() != req->revision()) {
            FinishBizError(outer, 10007, "revision mismatch", brpc::HTTP_STATUS_CONFLICT);
            return;
        }
        if (req->has_cover_media() && !req->cover_media().url().empty() &&
            !ValidatePersistableCoverUrl(req->cover_media().url(), outer)) {
            return;
        }
        if (req->has_cover_url() && !req->cover_url().empty() &&
            !ValidatePersistableCoverUrl(req->cover_url(), outer)) {
            return;
        }
        bo::ApplyContentUpdate(*req, &card);

        brpc::Controller cntl;
        simple_living::content_server::UpsertGuideCardRequest ureq;
        ureq.set_expected_revision(card.revision());
        *ureq.mutable_guide_card() = card;
        simple_living::content_server::UpsertGuideCardResponse uresp;
        bo_content_stub_.UpsertGuideCard(&cntl, &ureq, &uresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        if (!LoadBackofficeContentItems(resp, outer)) {
            return;
        }
        FinishOk(outer, *resp);
    }

    void PostBackofficeContentItemDetail(::google::protobuf::RpcController* controller_base,
                               const BackofficeContentDetailRequest* req,
                               BackofficeContentDetailResponse* resp,
                               ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->content_id().empty()) {
            FinishBizError(outer, 10002, "content_id required", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        if (!LoadBackofficeContentDetail(req->content_id(), resp, outer)) {
            return;
        }
        FinishOk(outer, *resp);
    }

    void PostBackofficeContentItemSubmitReview(::google::protobuf::RpcController* controller_base,
                                      const BackofficeSubmitReviewRequest* req,
                                      BackofficeSubmitReviewResponse* resp,
                                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->content_id().empty()) {
            FinishBizError(outer, 10002, "content_id required", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        brpc::Controller cntl;
        simple_living::content_server::SubmitForReviewRequest sreq;
        sreq.set_resource_kind(simple_living::catalog::CONTENT_RESOURCE_KIND_GUIDE_CARD);
        sreq.set_resource_id(req->content_id());
        sreq.set_revision(req->revision());
        simple_living::content_server::SubmitForReviewResponse sresp;
        bo_content_stub_.SubmitForReview(&cntl, &sreq, &sresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }

        brpc::Controller rcntl;
        simple_living::governance_server::EnqueueReviewRequest rreq;
        rreq.set_content_id(req->content_id());
        rreq.set_content_version(std::to_string(req->revision() > 0 ? req->revision() : 1));
        rreq.set_priority(0);
        rreq.set_enqueue_reason("submit_review");
        simple_living::governance_server::EnqueueReviewResponse rresp;
        bo_review_stub_.EnqueueReview(&rcntl, &rreq, &rresp, nullptr);
        if (rcntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, rcntl);
            return;
        }

        resp->set_success(true);
        resp->set_review_id(rresp.queue_item().id());
        resp->set_content_status("in_review");
        FinishOk(outer, *resp);
    }

    void PostBackofficeContentItemPublish(::google::protobuf::RpcController* controller_base,
                                const BackofficePublishRequest* req,
                                BackofficePublishResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->content_id().empty()) {
            FinishBizError(outer, 10002, "content_id required", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        brpc::Controller gcntl;
        simple_living::content_server::BatchGetGuideCardsRequest greq;
        greq.add_card_ids(req->content_id());
        simple_living::content_server::BatchGetGuideCardsResponse gresp;
        bo_content_stub_.BatchGetGuideCards(&gcntl, &greq, &gresp, nullptr);
        if (gcntl.Failed() || gresp.cards_size() == 0) {
            FinishBizError(outer, 10003, "content not found", brpc::HTTP_STATUS_NOT_FOUND);
            return;
        }
        if (!EnsurePublishAllowed(req->content_id(), gresp.cards(0), outer)) {
            return;
        }

        brpc::Controller pcntl;
        simple_living::content_server::PublishRevisionRequest preq;
        preq.set_resource_kind(simple_living::catalog::CONTENT_RESOURCE_KIND_GUIDE_CARD);
        preq.set_resource_id(req->content_id());
        preq.set_revision(req->revision());
        simple_living::content_server::PublishRevisionResponse presp;
        bo_content_stub_.PublishRevision(&pcntl, &preq, &presp, nullptr);
        if (pcntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, pcntl);
            return;
        }
        if (presp.content_status() != simple_living::catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED) {
            FinishBizError(outer, 10003, "publish failed", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        SyncVisibilityForContent(req->content_id(), true);

        resp->set_success(true);
        resp->set_content_id(req->content_id());
        resp->set_published_revision(req->revision());
        resp->set_visibility_state("published");
        FinishOk(outer, *resp);
    }

    void PostBackofficeContentItemRollback(::google::protobuf::RpcController* controller_base,
                                 const BackofficeRollbackRequest* req,
                                 BackofficeRollbackResponse* resp,
                                 ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->content_id().empty() || req->target_revision() <= 0) {
            FinishBizError(outer, 10002, "content_id and target_revision required", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        brpc::Controller cntl;
        simple_living::content_server::RollbackGuideCardRevisionRequest rreq;
        rreq.set_card_id(req->content_id());
        rreq.set_target_revision(req->target_revision());
        simple_living::content_server::RollbackGuideCardRevisionResponse rresp;
        bo_content_stub_.RollbackGuideCardRevision(&cntl, &rreq, &rresp, nullptr);
        if (cntl.Failed() || !rresp.has_new_revision()) {
            FinishBizError(outer, 10003, "rollback failed", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        resp->set_success(true);
        resp->set_new_revision(rresp.new_revision());
        resp->set_content_id(req->content_id());
        FinishOk(outer, *resp);
    }

    void PostBackofficeGovernanceVisibility(::google::protobuf::RpcController* controller_base,
                                   const BackofficeSetVisibilityRequest* req,
                                   BackofficeSetVisibilityResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->content_id().empty() || req->state().empty()) {
            FinishBizError(outer, 10002, "content_id and state required", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }

        simple_living::governance_server::SetVisibilityVerdictRequest vreq;
        vreq.set_content_id(req->content_id());
        if (req->state() == "published") {
            vreq.set_state(simple_living::governance_server::VISIBILITY_STATE_PUBLISHED);
        } else if (req->state() == "restricted") {
            vreq.set_state(simple_living::governance_server::VISIBILITY_STATE_RESTRICTED);
        } else {
            vreq.set_state(simple_living::governance_server::VISIBILITY_STATE_UNPUBLISHED);
        }
        if (req->has_reason_code()) {
            vreq.set_reason_code(req->reason_code());
        } else {
            vreq.set_reason_code("manual_ops");
        }
        vreq.set_source(simple_living::governance_server::VISIBILITY_VERDICT_SOURCE_MANUAL_OPS);

        brpc::Controller cntl;
        simple_living::governance_server::SetVisibilityVerdictResponse vresp;
        bo_visibility_stub_.SetVisibilityVerdict(&cntl, &vreq, &vresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }

        resp->set_success(true);
        resp->set_content_id(req->content_id());
        resp->set_visibility_state(req->state());
        FinishOk(outer, *resp);
    }

    void GetBackofficeGovernanceReviews(::google::protobuf::RpcController* controller_base,
                                 const BackofficeListReviewsRequest*,
                                 BackofficeReviewsResponse* resp,
                                 ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (!LoadBackofficeReviews(resp, outer)) {
            return;
        }
        FinishOk(outer, *resp);
    }

    void PatchBackofficeGovernanceReviewStatus(::google::protobuf::RpcController* controller_base,
                                        const BackofficeUpdateReviewStatusRequest* req,
                                        BackofficeReviewsResponse* resp,
                                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* outer = static_cast<brpc::Controller*>(controller_base);
        if (!EnsurePost(outer) || !bo::EnsureBackofficeAuth(outer)) return;
        if (req->review_id().empty() || req->status().empty()) {
            FinishBizError(outer, 10002, "Validation failed", brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        const auto outcome = bo::ReviewOutcomeFromString(req->status());
        if (outcome == simple_living::governance_server::REVIEW_OUTCOME_UNSPECIFIED) {
            FinishBizError(outer, 10002, "status must be approved, rejected, or needs_info",
                           brpc::HTTP_STATUS_BAD_REQUEST);
            return;
        }
        brpc::Controller cntl;
        simple_living::governance_server::SubmitReviewDecisionRequest sreq;
        sreq.set_queue_item_id(req->review_id());
        sreq.set_outcome(outcome);
        sreq.set_reviewer_id("backoffice_ops");
        simple_living::governance_server::SubmitReviewDecisionResponse sresp;
        bo_review_stub_.SubmitReviewDecision(&cntl, &sreq, &sresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return;
        }
        if (sresp.has_decision()) {
            if (outcome == simple_living::governance_server::REVIEW_OUTCOME_APPROVED) {
                if (!UpdateContentStatus(sresp.decision().content_id(), "approved", outer)) {
                    return;
                }
            } else if (outcome == simple_living::governance_server::REVIEW_OUTCOME_REJECTED ||
                       outcome == simple_living::governance_server::REVIEW_OUTCOME_NEEDS_INFO) {
                if (!UpdateContentStatus(sresp.decision().content_id(), "draft", outer)) {
                    return;
                }
            }
        }
        if (!LoadBackofficeReviews(resp, outer)) {
            return;
        }
        FinishOk(outer, *resp);
    }

private:
    bool LoadBackofficeReviews(BackofficeReviewsResponse* resp, brpc::Controller* outer) {
        brpc::Controller cntl;
        simple_living::governance_server::ListReviewQueueItemsRequest greq;
        simple_living::governance_server::ListReviewQueueItemsResponse gresp;
        bo_review_stub_.ListReviewQueueItems(&cntl, &greq, &gresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return false;
        }
        for (const auto& qi : gresp.items()) {
            auto* item = resp->add_items();
            item->set_review_id(qi.id());
            item->set_subject_id(qi.content_id());
            item->set_resource_kind("guide_card");
            std::string status = bo::ReviewQueueStatusToString(qi.status());
            if (qi.status() == simple_living::governance_server::REVIEW_QUEUE_ITEM_STATUS_COMPLETED) {
                brpc::Controller rcntl;
                simple_living::governance_server::GetReviewStateRequest rreq;
                rreq.set_content_id(qi.content_id());
                simple_living::governance_server::GetReviewStateResponse rresp;
                bo_review_stub_.GetReviewState(&rcntl, &rreq, &rresp, nullptr);
                if (!rcntl.Failed() && rresp.has_state() && rresp.state().has_latest_decision()) {
                    const auto& decision = rresp.state().latest_decision();
                    if (decision.queue_item_id() == qi.id()) {
                        status = bo::ReviewOutcomeToString(decision.outcome());
                    }
                }
            }
            item->set_status(status);
            item->set_enqueue_reason(qi.enqueue_reason());
        }
        return true;
    }

    bool LoadBackofficeContentDetail(const std::string& content_id,
                                     BackofficeContentDetailResponse* resp,
                                     brpc::Controller* outer) {
        brpc::Controller gcntl;
        simple_living::content_server::BatchGetGuideCardsRequest greq;
        greq.add_card_ids(content_id);
        simple_living::content_server::BatchGetGuideCardsResponse gresp;
        bo_content_stub_.BatchGetGuideCards(&gcntl, &greq, &gresp, nullptr);
        if (gcntl.Failed() || gresp.cards_size() == 0) {
            FinishBizError(outer, 10003, "content not found", brpc::HTTP_STATUS_NOT_FOUND);
            return false;
        }
        auto card = gresp.cards(0);
        MaybePromoteInReviewToApproved(&card);
        bo::FillBackofficeContentItem(card, resp->mutable_content());

        brpc::Controller revcntl;
        simple_living::content_server::ListGuideCardRevisionsRequest revreq;
        revreq.set_card_id(content_id);
        simple_living::content_server::ListGuideCardRevisionsResponse revresp;
        bo_content_stub_.ListGuideCardRevisions(&revcntl, &revreq, &revresp, nullptr);
        if (!revcntl.Failed()) {
            for (const auto& rev : revresp.revisions()) {
                auto* out = resp->add_revision_history();
                out->set_revision(rev.revision());
                out->set_change_summary(rev.change_summary());
                out->set_created_by(rev.created_by());
            }
        } else {
            auto* rev = resp->add_revision_history();
            rev->set_revision(gresp.cards(0).revision());
            rev->set_created_by("backoffice_ops");
            rev->set_change_summary("current");
        }

        brpc::Controller rcntl;
        simple_living::governance_server::GetReviewStateRequest rreq;
        rreq.set_content_id(content_id);
        simple_living::governance_server::GetReviewStateResponse rresp;
        bo_review_stub_.GetReviewState(&rcntl, &rreq, &rresp, nullptr);
        if (!rcntl.Failed() && rresp.has_state() && rresp.state().has_latest_decision()) {
            const auto& decision = rresp.state().latest_decision();
            auto* rs = resp->mutable_review_state();
            rs->set_review_id(decision.queue_item_id());
            if (decision.outcome() == simple_living::governance_server::REVIEW_OUTCOME_APPROVED) {
                rs->set_status("approved");
            } else if (decision.outcome() == simple_living::governance_server::REVIEW_OUTCOME_REJECTED) {
                rs->set_status("rejected");
            } else if (decision.outcome() == simple_living::governance_server::REVIEW_OUTCOME_NEEDS_INFO) {
                rs->set_status("needs_info");
            } else {
                rs->set_status("in_review");
            }
        } else {
            brpc::Controller qcntl;
            simple_living::governance_server::ListReviewQueueItemsRequest qreq;
            qreq.set_content_id_prefix(content_id);
            simple_living::governance_server::ListReviewQueueItemsResponse qresp;
            bo_review_stub_.ListReviewQueueItems(&qcntl, &qreq, &qresp, nullptr);
            if (!qcntl.Failed()) {
                for (const auto& qi : qresp.items()) {
                    if (qi.content_id() == content_id) {
                        auto* rs = resp->mutable_review_state();
                        rs->set_review_id(qi.id());
                        rs->set_status(bo::ReviewQueueStatusToString(qi.status()));
                        break;
                    }
                }
            }
        }

        brpc::Controller vcntl;
        simple_living::governance_server::GetVisibilityVerdictRequest vreq;
        vreq.set_content_id(content_id);
        simple_living::governance_server::GetVisibilityVerdictResponse vresp;
        bo_visibility_stub_.GetVisibilityVerdict(&vcntl, &vreq, &vresp, nullptr);
        if (!vcntl.Failed() && vresp.has_verdict()) {
            auto* vv = resp->mutable_visibility_verdict();
            vv->set_state(bo::VisibilityStateToString(vresp.verdict().state()));
            vv->set_reason_code(vresp.verdict().reason_code());
            vv->set_source(bo::VisibilitySourceToString(vresp.verdict().source()));
            vv->set_version(vresp.verdict().version());
        }
        return true;
    }

    bool MaybePromoteInReviewToApproved(simple_living::catalog::GuideCard* card) {
        using simple_living::catalog::CONTENT_LIFECYCLE_STATUS_APPROVED;
        using simple_living::catalog::CONTENT_LIFECYCLE_STATUS_IN_REVIEW;
        if (card->content_status() != CONTENT_LIFECYCLE_STATUS_IN_REVIEW) {
            return false;
        }
        brpc::Controller rcntl;
        simple_living::governance_server::GetReviewStateRequest rreq;
        rreq.set_content_id(card->card_id());
        simple_living::governance_server::GetReviewStateResponse rresp;
        bo_review_stub_.GetReviewState(&rcntl, &rreq, &rresp, nullptr);
        if (rcntl.Failed() || !rresp.has_state() || !rresp.state().has_latest_decision() ||
            rresp.state().latest_decision().outcome() !=
                simple_living::governance_server::REVIEW_OUTCOME_APPROVED) {
            return false;
        }
        card->set_content_status(CONTENT_LIFECYCLE_STATUS_APPROVED);
        brpc::Controller ucntl;
        simple_living::content_server::UpsertGuideCardRequest ureq;
        *ureq.mutable_guide_card() = *card;
        simple_living::content_server::UpsertGuideCardResponse uresp;
        bo_content_stub_.UpsertGuideCard(&ucntl, &ureq, &uresp, nullptr);
        return !ucntl.Failed();
    }

    bool LoadBackofficeContentItems(BackofficeContentItemsResponse* resp, brpc::Controller* outer, const std::string& filter_content_id = "") {
        brpc::Controller cntl;
        simple_living::content_server::ListGuideCardsRequest req;
        req.set_limit(50);
        simple_living::content_server::ListGuideCardsResponse cresp;
        bo_content_stub_.ListGuideCards(&cntl, &req, &cresp, nullptr);
        if (cntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, cntl);
            return false;
        }
        for (const auto& summary : cresp.cards()) {
            if (!filter_content_id.empty() && summary.card_id() != filter_content_id) {
                continue;
            }
            simple_living::content_server::BatchGetGuideCardsRequest breq;
            breq.add_card_ids(summary.card_id());
            simple_living::content_server::BatchGetGuideCardsResponse bresp;
            brpc::Controller bcntl;
            bo_content_stub_.BatchGetGuideCards(&bcntl, &breq, &bresp, nullptr);
            if (bcntl.Failed() || bresp.cards_size() == 0) {
                continue;
            }
            auto card = bresp.cards(0);
            MaybePromoteInReviewToApproved(&card);
            bo::FillBackofficeContentItem(card, resp->add_items());
        }
        return true;
    }

    bool EnsurePublishAllowed(const std::string& content_id,
                              const simple_living::catalog::GuideCard& card,
                              brpc::Controller* outer) {
        using simple_living::catalog::CONTENT_LIFECYCLE_STATUS_APPROVED;
        using simple_living::catalog::CONTENT_LIFECYCLE_STATUS_OFFLINE;
        using simple_living::catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED;
        if (card.content_status() == CONTENT_LIFECYCLE_STATUS_PUBLISHED ||
            card.content_status() == CONTENT_LIFECYCLE_STATUS_OFFLINE ||
            card.content_status() == CONTENT_LIFECYCLE_STATUS_APPROVED) {
            return true;
        }
        brpc::Controller rcntl;
        simple_living::governance_server::GetReviewStateRequest rreq;
        rreq.set_content_id(content_id);
        simple_living::governance_server::GetReviewStateResponse rresp;
        bo_review_stub_.GetReviewState(&rcntl, &rreq, &rresp, nullptr);
        if (rcntl.Failed() || !rresp.has_state() || !rresp.state().has_latest_decision() ||
            rresp.state().latest_decision().outcome() !=
                simple_living::governance_server::REVIEW_OUTCOME_APPROVED) {
            FinishBizError(outer, 10002, "review approval required before publish",
                           brpc::HTTP_STATUS_BAD_REQUEST);
            return false;
        }
        return true;
    }

    bool UpdateContentStatus(const std::string& content_id,
                             const std::string& status,
                             brpc::Controller* outer) {
        brpc::Controller bcntl;
        simple_living::content_server::BatchGetGuideCardsRequest breq;
        breq.add_card_ids(content_id);
        simple_living::content_server::BatchGetGuideCardsResponse bresp;
        bo_content_stub_.BatchGetGuideCards(&bcntl, &breq, &bresp, nullptr);
        if (bcntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, bcntl);
            return false;
        }
        if (bresp.cards_size() == 0) {
            FinishBizError(outer, 30001, "Not found", brpc::HTTP_STATUS_NOT_FOUND);
            return false;
        }

        auto card = bresp.cards(0);
        if (status == "published") {
            if (!EnsurePublishAllowed(content_id, card, outer)) {
                return false;
            }
            brpc::Controller pcntl;
            simple_living::content_server::PublishRevisionRequest preq;
            preq.set_resource_kind(simple_living::catalog::CONTENT_RESOURCE_KIND_GUIDE_CARD);
            preq.set_resource_id(content_id);
            preq.set_revision(card.revision());
            simple_living::content_server::PublishRevisionResponse presp;
            bo_content_stub_.PublishRevision(&pcntl, &preq, &presp, nullptr);
            if (pcntl.Failed()) {
                FinishDownstreamBrpcFailure(outer, pcntl);
                return false;
            }
            SyncVisibilityForContent(content_id, true);
            return true;
        }

        card.set_content_status(bo::ContentStatusFromString(status));
        brpc::Controller ucntl;
        simple_living::content_server::UpsertGuideCardRequest ureq;
        *ureq.mutable_guide_card() = card;
        simple_living::content_server::UpsertGuideCardResponse uresp;
        bo_content_stub_.UpsertGuideCard(&ucntl, &ureq, &uresp, nullptr);
        if (ucntl.Failed()) {
            FinishDownstreamBrpcFailure(outer, ucntl);
            return false;
        }
        if (status == "offline" || status == "archived" || status == "draft") {
            SyncVisibilityForContent(content_id, false);
        }
        return true;
    }
};

}  // namespace pages

}  // namespace gateway
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_user_server_addr == kDefaultUserServerAddr) {
        if (const char* v = std::getenv("GATEWAY_USER_SERVER_ADDR"); v != nullptr && v[0] != '\0') {
            FLAGS_user_server_addr = v;
        }
    }
    if (FLAGS_recommendation_server_addr == kDefaultRecommendationServerAddr) {
        if (const char* v = std::getenv("GATEWAY_RECOMMENDATION_SERVER_ADDR"); v != nullptr && v[0] != '\0') {
            FLAGS_recommendation_server_addr = v;
        }
    }
    if (FLAGS_tracking_server_addr == kDefaultTrackingServerAddr) {
        if (const char* v = std::getenv("GATEWAY_TRACKING_SERVER_ADDR"); v != nullptr && v[0] != '\0') {
            FLAGS_tracking_server_addr = v;
        }
    }
    if (FLAGS_backoffice_backend_addr == kDefaultBackofficeBackendAddr) {
        if (const char* v = std::getenv("GATEWAY_BACKOFFICE_BACKEND_ADDR"); v != nullptr && v[0] != '\0') {
            FLAGS_backoffice_backend_addr = v;
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
        "/api/v2/pages/me_summary      => GetMeSummary,"
        "/api/v2/backoffice/auth/login => PostBackofficeLogin,"
        "/api/v2/backoffice/media/upload => PostBackofficeMediaUpload,"
        "/media/backoffice/* => GetBackofficeMediaAsset,"
        "/api/v2/backoffice/affiliate/partners => GetBackofficeAffiliatePartners,"
        "/api/v2/backoffice/affiliate/partners/add => PostBackofficeAffiliatePartner,"
        "/api/v2/backoffice/content/items => GetBackofficeContentItems,"
        "/api/v2/backoffice/content/items/add => PostBackofficeContentItem,"
        "/api/v2/backoffice/content/items/update => PostBackofficeContentItemUpdate,"
        "/api/v2/backoffice/content/items/detail => PostBackofficeContentItemDetail,"
        "/api/v2/backoffice/content/items/submit-review => PostBackofficeContentItemSubmitReview,"
        "/api/v2/backoffice/content/items/publish => PostBackofficeContentItemPublish,"
        "/api/v2/backoffice/content/items/status => PatchBackofficeContentItemStatus,"
        "/api/v2/backoffice/content/items/rollback => PostBackofficeContentItemRollback,"
        "/api/v2/backoffice/governance/reviews => GetBackofficeGovernanceReviews,"
        "/api/v2/backoffice/governance/reviews/status => PatchBackofficeGovernanceReviewStatus,"
        "/api/v2/backoffice/governance/visibility => PostBackofficeGovernanceVisibility";
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
