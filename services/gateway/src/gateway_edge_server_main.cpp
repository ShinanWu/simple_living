// gateway brpc edge server — HTTP+JSON front, calls downstream brpc services
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <brpc/channel.h>
#include <butil/logging.h>

#include "gateway_user_edge.pb.h"
#include "gateway_pages_edge.pb.h"
#include "user_domain_service.pb.h"
#include "recommendation_domain_service.pb.h"
#include "content_domain.pb.h"
#include "tracking_domain.pb.h"

DEFINE_int32(port, 8080, "TCP port for this brpc server");
DEFINE_string(user_domain_addr,   "127.0.0.1:9101", "user-domain address");
DEFINE_string(content_domain_addr, "127.0.0.1:9102", "content-domain address");
DEFINE_string(recommendation_domain_addr, "127.0.0.1:9103", "recommendation-domain address");
DEFINE_string(tracking_domain_addr, "127.0.0.1:9105", "tracking-domain address");

namespace simple_living {
namespace gateway {

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
    GatewayUserEdgeV2Impl() : user_stub_(&user_ch_) {
        InitChannel(&user_ch_, FLAGS_user_domain_addr);
    }

    void PostGuestSession(::google::protobuf::RpcController*,
                          const ::simple_living::user_domain::EnsureGuestSessionRequest* req,
                          ::simple_living::user_domain::EnsureGuestSessionResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.EnsureGuestSession(&cntl, req, resp, nullptr);
        if (cntl.Failed()) LOG(WARNING) << "PostGuestSession failed: " << cntl.ErrorText();
    }

    void PostAuthTokenIssue(::google::protobuf::RpcController*,
                            const ::simple_living::user_domain::IssueTokenPairRequest* req,
                            ::simple_living::user_domain::IssueTokenPairResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.IssueTokenPair(&cntl, req, resp, nullptr);
        if (cntl.Failed()) LOG(WARNING) << "PostAuthTokenIssue failed: " << cntl.ErrorText();
    }

    void PostAuthTokenRefresh(::google::protobuf::RpcController*,
                              const RefreshTokenHttpRequest* req,
                              ::simple_living::user_domain::IssueTokenPairResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::user_domain::IntrospectRefreshTokenRequest rreq;
        rreq.set_refresh_token(req->refresh_token());
        simple_living::user_domain::IntrospectRefreshTokenResponse rresp;
        user_stub_.IntrospectRefreshToken(&cntl, &rreq, &rresp, nullptr);
        if (cntl.Failed() || !rresp.valid()) return;
        cntl.Reset();
        simple_living::user_domain::IssueTokenPairRequest ireq;
        ireq.set_user_id(rresp.user_id());
        user_stub_.IssueTokenPair(&cntl, &ireq, resp, nullptr);
    }

    void DeleteAuthSession(::google::protobuf::RpcController*,
                           const ::simple_living::user_domain::RevokeSessionRequest* req,
                           ::simple_living::user_domain::RevokeSessionResponse* resp,
                           ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.RevokeSession(&cntl, req, resp, nullptr);
    }

    void GetMeProfile(::google::protobuf::RpcController*,
                      const ::simple_living::user_domain::GetProfileRequest* req,
                      ::simple_living::user_domain::GetProfileResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.GetProfile(&cntl, req, resp, nullptr);
    }

    void PatchMeProfile(::google::protobuf::RpcController*,
                        const ::simple_living::user_domain::UpdateProfileRequest* req,
                        ::simple_living::user_domain::UpdateProfileResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.UpdateProfile(&cntl, req, resp, nullptr);
    }

    void GetMePreferences(::google::protobuf::RpcController*,
                          const ::simple_living::user_domain::GetPreferencesRequest* req,
                          ::simple_living::user_domain::GetPreferencesResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.GetPreferences(&cntl, req, resp, nullptr);
    }

    void PutMePreferences(::google::protobuf::RpcController*,
                          const ::simple_living::user_domain::UpdatePreferencesRequest* req,
                          ::simple_living::user_domain::UpdatePreferencesResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.UpdatePreferences(&cntl, req, resp, nullptr);
    }

    void GetMeFavorites(::google::protobuf::RpcController*,
                        const ListFavoritesHttpRequest* req,
                        ::simple_living::user_domain::ListFavoritesResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::user_domain::ListFavoritesRequest dreq;
        dreq.set_user_id(req->acting_user_id());
        if (req->limit() > 0) dreq.mutable_page()->set_limit(req->limit());
        user_stub_.ListFavorites(&cntl, &dreq, resp, nullptr);
    }

    void PostMeFavorite(::google::protobuf::RpcController*,
                        const AddFavoriteHttpRequest* req,
                        ::simple_living::user_domain::AddFavoriteResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::user_domain::AddFavoriteRequest dreq;
        dreq.set_user_id(req->acting_user_id());
        dreq.set_guide_card_id(req->guide_card_id());
        user_stub_.AddFavorite(&cntl, &dreq, resp, nullptr);
    }

    void DeleteMeFavorite(::google::protobuf::RpcController*,
                          const RemoveFavoriteHttpRequest* req,
                          ::simple_living::user_domain::RemoveFavoriteResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::user_domain::RemoveFavoriteRequest dreq;
        dreq.set_user_id(req->acting_user_id());
        dreq.set_favorite_id(req->favorite_id());
        user_stub_.RemoveFavorite(&cntl, &dreq, resp, nullptr);
    }

    void GetMeHistory(::google::protobuf::RpcController*,
                      const ListHistoryHttpRequest* req,
                      ::simple_living::user_domain::ListHistoryResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::user_domain::ListHistoryRequest dreq;
        if (req->has_acting_user_id()) dreq.set_user_id(req->acting_user_id());
        else dreq.set_session_id(req->session_id());
        user_stub_.ListHistory(&cntl, &dreq, resp, nullptr);
    }

    void PostMeHistoryEvent(::google::protobuf::RpcController*,
                            const RecordHistoryEventHttpRequest* req,
                            ::simple_living::user_domain::RecordHistoryEventResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::user_domain::RecordHistoryEventRequest dreq;
        if (req->has_acting_user_id()) dreq.set_user_id(req->acting_user_id());
        else dreq.set_session_id(req->session_id());
        if (req->has_content_ref()) *dreq.mutable_content_ref() = req->content_ref();
        user_stub_.RecordHistoryEvent(&cntl, &dreq, resp, nullptr);
    }

    void DeleteMeHistory(::google::protobuf::RpcController*,
                         const ClearHistoryHttpRequest* req,
                         ::simple_living::user_domain::ClearHistoryResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::user_domain::ClearHistoryRequest dreq;
        if (req->has_acting_user_id()) dreq.set_user_id(req->acting_user_id());
        else dreq.set_session_id(req->session_id());
        dreq.set_scope(simple_living::user_domain::CLEAR_HISTORY_SCOPE_ALL);
        user_stub_.ClearHistory(&cntl, &dreq, resp, nullptr);
    }

    void PostMeFeedback(::google::protobuf::RpcController*,
                        const SubmitFeedbackHttpRequest* req,
                        ::simple_living::user_domain::SubmitFeedbackResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::user_domain::SubmitFeedbackRequest dreq;
        if (req->has_acting_user_id()) dreq.set_user_id(req->acting_user_id());
        else dreq.set_session_id(req->session_id());
        dreq.set_target_type(req->target_type());
        dreq.set_target_id(req->target_id());
        user_stub_.SubmitFeedback(&cntl, &dreq, resp, nullptr);
    }

    void GetMeConsent(::google::protobuf::RpcController*,
                      const ::simple_living::user_domain::GetConsentRequest* req,
                      ::simple_living::user_domain::GetConsentResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.GetConsent(&cntl, req, resp, nullptr);
    }

    void PutMeConsent(::google::protobuf::RpcController*,
                      const ::simple_living::user_domain::UpdateConsentRequest* req,
                      ::simple_living::user_domain::UpdateConsentResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.UpdateConsent(&cntl, req, resp, nullptr);
    }

    void GetMeSummary(::google::protobuf::RpcController*,
                      const ::simple_living::user_domain::GetMeSummaryRequest* req,
                      ::simple_living::user_domain::GetMeSummaryResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        user_stub_.GetMeSummary(&cntl, req, resp, nullptr);
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
        InitChannel(&rec_ch_,      FLAGS_recommendation_domain_addr);
        InitChannel(&content_ch_,  FLAGS_content_domain_addr);
        InitChannel(&tracking_ch_, FLAGS_tracking_domain_addr);
    }

    void GetHomeFeed(::google::protobuf::RpcController*,
                     const HomeFeedRequest* req,
                     HomeFeedResponse* resp,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::recommendation_domain::QueryRecommendationsRequest rreq;
        rreq.set_scene(simple_living::recommendation_domain::HOME_FEED);
        if (!req->theme().empty()) rreq.mutable_context()->set_theme(req->theme());
        int lim = (req->limit() > 0) ? req->limit() : 20;
        rreq.mutable_cursor_limits()->set_limit(lim);
        if (!req->cursor().empty()) rreq.mutable_cursor_limits()->set_cursor(req->cursor());
        simple_living::recommendation_domain::QueryRecommendationsResponse rresp;
        rec_stub_.QueryRecommendations(&cntl, &rreq, &rresp, nullptr);
        if (cntl.Failed()) {
            LOG(WARNING) << "GetHomeFeed rec failed: " << cntl.ErrorText();
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
            for (const auto& rt : item.reason_tags()) fi->add_reason_tags(rt);
        }
        if (rresp.has_cursor_pagination()) {
            resp->mutable_pagination()->set_next_cursor(rresp.cursor_pagination().next_cursor());
            resp->mutable_pagination()->set_has_more(rresp.cursor_pagination().has_more());
            resp->mutable_pagination()->set_limit(lim);
        }
    }

    void GetGuideDetail(::google::protobuf::RpcController*,
                        const GuideDetailRequest* req,
                        GuideDetailResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::content_domain::BatchGetGuideCardsRequest creq;
        creq.add_card_ids(req->guide_card_id());
        simple_living::content_domain::BatchGetGuideCardsResponse cresp;
        content_stub_.BatchGetGuideCards(&cntl, &creq, &cresp, nullptr);
        if (cntl.Failed() || cresp.cards_size() == 0) return;
        const auto& card = cresp.cards(0);
        auto* detail = resp->mutable_guide_card();
        detail->set_guide_card_id(card.card_id());
        detail->set_title(card.title());
        detail->set_subtitle(card.subtitle());
        detail->set_is_commercial(card.commercial_disclosure_required());
        if (card.has_cover_media()) detail->set_cover_url(card.cover_media().url());
    }

    void PrepareRedirect(::google::protobuf::RpcController*,
                         const RedirectPrepareRequest* req,
                         RedirectPrepareResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        brpc::Controller cntl;
        simple_living::tracking_domain::AssembleTrackingLinkRequest treq;
        treq.mutable_content_ref()->set_guide_card_id(req->guide_card_id());
        treq.mutable_content_ref()->set_recommendation_id(req->recommendation_id());
        treq.mutable_content_ref()->set_scene(req->scene());
        treq.mutable_content_ref()->set_item_rank(req->item_rank());
        simple_living::tracking_domain::AssembleTrackingLinkResponse tresp;
        tracking_stub_.AssembleTrackingLink(&cntl, &treq, &tresp, nullptr);
        if (cntl.Failed()) {
            LOG(WARNING) << "PrepareRedirect failed: " << cntl.ErrorText();
            return;
        }
        resp->set_landing_url(tresp.landing_url());
        // Prefer attribution click_id, fall back to link_ref then short_token
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
    }

    void GetMeSummary(::google::protobuf::RpcController*,
                      const MeSummaryRequest*,
                      MeSummaryResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        // Minimal pages-edge response; full user aggregate remains on user-edge GetMeSummary.
        resp->mutable_profile()->set_is_guest(true);
        resp->mutable_counts()->set_favorites_count(0);
        resp->mutable_counts()->set_history_count(0);
    }
};

}  // namespace pages

}  // namespace gateway
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;

    simple_living::gateway::user::GatewayUserEdgeV2Impl g_user_edge;
    if (server.AddService(&g_user_edge, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add GatewayUserEdgeV2";
        return 1;
    }

    simple_living::gateway::pages::GatewayPagesEdgeV2Impl g_pages_edge;
    if (server.AddService(&g_pages_edge, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
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
