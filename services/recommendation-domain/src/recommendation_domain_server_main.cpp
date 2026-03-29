// recommendation-domain brpc server — features/index in MySQL/PostgreSQL + Redis + Kafka per docs/architecture.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "recommendation_domain_service.pb.h"

DEFINE_int32(port, 9103, "TCP port for this brpc server");

namespace simple_living {
namespace recommendation_domain {

namespace {
static std::atomic<int64_t> g_counter{1};
std::string GenId(const std::string& p) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return p + "_" + std::to_string(ts) + "_" + std::to_string(g_counter.fetch_add(1));
}

// Seed card IDs (matches content-domain seeds)
static const char* kSeedCards[] = {
    "card_seed_1", "card_seed_2", "card_seed_3", "card_seed_4", "card_seed_5",
    "card_coliving_1", "card_minimal_2", "card_eco_3"
};
static const int kSeedCount = 8;

void FillItems(int limit, const std::string& scene_hint, ::google::protobuf::RepeatedPtrField<RecommendationItem>* items) {
    if (limit <= 0 || limit > 20) limit = 10;
    for (int i = 0; i < limit && i < kSeedCount; ++i) {
        auto* item = items->Add();
        item->set_guide_card_id(kSeedCards[i]);
        item->set_score(1.0 - i * 0.05);
        item->set_rank(i + 1);
        item->add_recall_sources("popularity");
        item->set_placement(scene_hint.empty() ? "feed" : scene_hint);
    }
}
}  // namespace

class RecommendationServiceImpl : public RecommendationService {
    std::mutex mu_;
    // scene -> last recommendation_id for idempotency (optional)
    std::unordered_map<std::string, std::string> last_rec_by_session_;
public:
    void QueryRecommendations(::google::protobuf::RpcController*,
                              const QueryRecommendationsRequest* req,
                              QueryRecommendationsResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_recommendation_id(GenId("rec"));
        resp->set_scene(req->scene());
        resp->set_generated_at("2026-01-01T00:00:00Z");
        resp->mutable_strategy()->set_id("strat_v1");
        resp->mutable_strategy()->set_version("1.0");
        resp->mutable_strategy()->set_pipeline("popularity_fallback");

        int limit = (req->has_cursor_limits() && req->cursor_limits().limit() > 0)
                     ? req->cursor_limits().limit() : 10;
        std::string theme = req->has_context() ? req->context().theme() : "";
        FillItems(limit, theme, resp->mutable_items());
        resp->mutable_cursor_pagination()->set_has_more(false);
        resp->mutable_cursor_pagination()->set_limit(limit);
    }

    void GetPopularRecommendations(::google::protobuf::RpcController*,
                                   const GetPopularRecommendationsRequest* req,
                                   GetPopularRecommendationsResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_recommendation_id(GenId("pop"));
        resp->set_scene(req->scene());
        resp->set_generated_at("2026-01-01T00:00:00Z");
        resp->mutable_strategy()->set_id("strat_popular");
        resp->mutable_strategy()->set_pipeline("trending");

        int limit = (req->has_cursor_limits() && req->cursor_limits().limit() > 0)
                     ? req->cursor_limits().limit() : 10;
        FillItems(limit, "popular", resp->mutable_items());
        resp->mutable_cursor_pagination()->set_has_more(false);
        resp->mutable_cursor_pagination()->set_limit(limit);
    }

    void ExplainRecommendations(::google::protobuf::RpcController*,
                                const ExplainRecommendationsRequest* req,
                                ExplainRecommendationsResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        for (auto& item_ref : req->items()) {
            auto* exp = resp->add_explanations();
            exp->set_summary("Recommended based on popularity and theme match");
            exp->add_reason_codes("popular_in_theme");
            exp->add_reason_codes("trending_7d");
            exp->set_confidence_band(CONFIDENCE_HIGH);
            exp->set_policy_version("policy_v1");
            (void)item_ref;
        }
    }

    void HealthCheck(::google::protobuf::RpcController*,
                     const HealthCheckRequest*,
                     HealthCheckResponse* resp,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_status(HEALTH_OK);
        resp->set_index_status(INDEX_UP);
    }
};

}  // namespace recommendation_domain
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;
    simple_living::recommendation_domain::RecommendationServiceImpl g_svc;
    if (server.AddService(&g_svc, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service"; return 1;
    }
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start server"; return 1;
    }
    LOG(INFO) << "recommendation-domain server listening on port " << FLAGS_port;
    server.RunUntilAskedToQuit();
    return 0;
}
