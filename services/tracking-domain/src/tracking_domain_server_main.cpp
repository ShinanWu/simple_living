// tracking-domain brpc server — clicks/attribution in MySQL/PostgreSQL + Redis; async via Kafka per docs/architecture.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "tracking_domain.pb.h"

DEFINE_int32(port, 9105, "TCP port for this brpc server");

namespace simple_living {
namespace tracking_domain {

namespace {
static std::atomic<int64_t> g_counter{1};
std::string GenId(const std::string& p) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return p + "_" + std::to_string(ts) + "_" + std::to_string(g_counter.fetch_add(1));
}

struct LinkRecord {
    std::string link_ref;
    std::string short_token;
    std::string landing_url;
    std::string guide_card_id;
    std::string scene;
    std::string placement;
};
}  // namespace

class TrackingLinkServiceImpl : public TrackingLinkService {
    std::mutex mu_;
    std::unordered_map<std::string, LinkRecord> links_by_ref_;  // link_ref -> record
    std::unordered_map<std::string, std::string> token_to_ref_; // short_token -> link_ref
public:
    void AssembleTrackingLink(::google::protobuf::RpcController*,
                              const AssembleTrackingLinkRequest* req,
                              AssembleTrackingLinkResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        LinkRecord lr;
        lr.link_ref = GenId("lr");
        lr.short_token = GenId("tk");
        lr.guide_card_id = req->has_content_ref() ? req->content_ref().guide_card_id() : "";
        lr.scene = req->has_content_ref() ? req->content_ref().scene() : "";
        lr.placement = req->placement();
        lr.landing_url = "https://go.simpleliving.com/r/" + lr.short_token;
        links_by_ref_[lr.link_ref] = lr;
        token_to_ref_[lr.short_token] = lr.link_ref;
        resp->set_landing_url(lr.landing_url);
        resp->set_link_ref(lr.link_ref);
        resp->set_short_token(lr.short_token);
        resp->mutable_attribution_echo()->set_guide_card_id(lr.guide_card_id);
        resp->mutable_attribution_echo()->set_placement(lr.placement);
        resp->mutable_attribution_echo()->set_scene(lr.scene);
    }

    bool GetLinkByToken(const std::string& token, LinkRecord* out) {
        auto it = token_to_ref_.find(token);
        if (it == token_to_ref_.end()) return false;
        auto it2 = links_by_ref_.find(it->second);
        if (it2 == links_by_ref_.end()) return false;
        *out = it2->second;
        return true;
    }

    bool GetLinkByRef(const std::string& ref, LinkRecord* out) {
        auto it = links_by_ref_.find(ref);
        if (it == links_by_ref_.end()) return false;
        *out = it->second;
        return true;
    }

    std::mutex& GetMutex() { return mu_; }
};

class TrackingRedirectServiceImpl : public TrackingRedirectService {
    TrackingLinkServiceImpl* link_svc_;
public:
    explicit TrackingRedirectServiceImpl(TrackingLinkServiceImpl* ls) : link_svc_(ls) {}

    void ResolveRedirect(::google::protobuf::RpcController*,
                         const ResolveRedirectRequest* req,
                         ResolveRedirectResponse* resp,
                         ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(link_svc_->GetMutex());
        LinkRecord lr;
        bool found = link_svc_->GetLinkByToken(req->short_token(), &lr);
        if (!found) {
            resp->set_disposition(REDIRECT_DISPOSITION_GONE);
            return;
        }
        resp->set_disposition(REDIRECT_DISPOSITION_HTTP_302);
        resp->set_http_location(lr.landing_url);
        resp->set_click_id(GenId("ck"));
    }
};

class TrackingClickServiceImpl : public TrackingClickService {
    std::mutex mu_;
    std::unordered_map<std::string, std::string> clicks_; // click_id -> link_ref
public:
    void AckClick(::google::protobuf::RpcController*,
                  const AckClickRequest* req,
                  AckClickResponse* resp,
                  ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        std::string click_id = GenId("ck");
        std::string ref = req->has_link_ref() ? req->link_ref() : req->short_token();
        clicks_[click_id] = ref;
        resp->set_click_id(click_id);
    }
};

class TrackingConversionServiceImpl : public TrackingConversionService {
    std::mutex mu_;
    std::unordered_set<std::string> seen_events_;
    std::vector<std::string> conversions_; // list of conversion_ids
public:
    void IngestConversion(::google::protobuf::RpcController*,
                          const IngestConversionRequest* req,
                          IngestConversionResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        if (!req->external_event_id().empty() && seen_events_.count(req->external_event_id())) {
            resp->set_status(CONVERSION_INGEST_STATUS_DUPLICATE);
            resp->set_match_confidence(MATCH_CONFIDENCE_UNMATCHED);
            return;
        }
        if (!req->external_event_id().empty()) seen_events_.insert(req->external_event_id());
        std::string conv_id = GenId("cv");
        conversions_.push_back(conv_id);
        resp->set_conversion_id(conv_id);
        resp->set_matched_click_id(req->click_id());
        resp->set_status(CONVERSION_INGEST_STATUS_ACCEPTED);
        resp->set_match_confidence(req->click_id().empty() ? MATCH_CONFIDENCE_UNMATCHED : MATCH_CONFIDENCE_EXACT);
    }
};

class TrackingCommissionViewServiceImpl : public TrackingCommissionViewService {
public:
    void GetCommissionSummary(::google::protobuf::RpcController*,
                              const GetCommissionSummaryRequest*,
                              GetCommissionSummaryResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* b = resp->add_buckets();
        b->set_estimated_minor(0);
        b->set_reported_minor(0);
        b->set_currency("CNY");
        b->set_order_count(0);
    }

    void ListCommissionItems(::google::protobuf::RpcController*,
                             const ListCommissionItemsRequest*,
                             ListCommissionItemsResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->mutable_pagination()->set_has_more(false);
    }
};

}  // namespace tracking_domain
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;
    static simple_living::tracking_domain::TrackingLinkServiceImpl g_link;
    if (server.AddService(&g_link, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingLinkService"; return 1;
    }
    static simple_living::tracking_domain::TrackingRedirectServiceImpl g_redirect(&g_link);
    if (server.AddService(&g_redirect, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingRedirectService"; return 1;
    }
    simple_living::tracking_domain::TrackingClickServiceImpl g_click;
    if (server.AddService(&g_click, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingClickService"; return 1;
    }
    simple_living::tracking_domain::TrackingConversionServiceImpl g_conv;
    if (server.AddService(&g_conv, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingConversionService"; return 1;
    }
    simple_living::tracking_domain::TrackingCommissionViewServiceImpl g_commission;
    if (server.AddService(&g_commission, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingCommissionViewService"; return 1;
    }
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start server"; return 1;
    }
    LOG(INFO) << "tracking-domain server listening on port " << FLAGS_port;
    server.RunUntilAskedToQuit();
    return 0;
}
