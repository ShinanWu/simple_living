// recommendation-server brpc server — PostgreSQL candidate pool + Redis/Kafka per services/README.md.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <atomic>
#include <chrono>
#include <string>

#include "catalog_read_service.h"
#include "recommendation_server_service.pb.h"
#include "pg_recommendation_store.h"
#include "snapshot_store.h"

DEFINE_int32(port, 9103, "TCP port for recommendation-server brpc");
DEFINE_string(pg_conninfo,
              "host=127.0.0.1 port=5432 dbname=simple_living user=simple password=simple",
              "libpq connection string (recommendation private tables)");
DEFINE_string(snapshot_dir, "/var/lib/simple-living/exports", "Shared snapshot dir with backoffice-backend");

namespace simple_living {
namespace recommendation_server {

namespace {
static std::atomic<int64_t> g_counter{1};
std::string GenId(const std::string& p) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return p + "_" + std::to_string(ts) + "_" + std::to_string(g_counter.fetch_add(1));
}

std::string ThemeIdFromTheme(const std::string& theme) {
    if (theme == "clothing" || theme.empty()) {
        return "theme_1";
    }
    if (theme == "food") {
        return "theme_2";
    }
    if (theme == "housing") {
        return "theme_3";
    }
    if (theme == "transport") {
        return "theme_4";
    }
    return theme;
}
}  // namespace

class RecommendationServiceImpl : public RecommendationService {
    PgRecommendationStore* store_;

public:
    explicit RecommendationServiceImpl(PgRecommendationStore* s) : store_(s) {}

    void QueryRecommendations(::google::protobuf::RpcController*,
                              const QueryRecommendationsRequest* req,
                              QueryRecommendationsResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->SyncCandidatesFromContentTable();

        resp->set_recommendation_id(GenId("rec"));
        resp->set_scene(req->scene());
        resp->set_generated_at("2026-01-01T00:00:00Z");
        resp->mutable_strategy()->set_id("strat_pg");
        resp->mutable_strategy()->set_version("1.0");
        resp->mutable_strategy()->set_pipeline("pg_candidate_pool");

        int limit = (req->has_cursor_limits() && req->cursor_limits().limit() > 0)
                        ? req->cursor_limits().limit()
                        : 10;
        const std::string theme = req->has_context() ? req->context().theme() : "";
        const std::string theme_id = ThemeIdFromTheme(theme);
        const std::string scene_hint = RecommendationScene_Name(req->scene());

        std::vector<RecommendationCandidateRow> rows;
        if (store_->QueryCandidates(theme_id, limit, &rows)) {
            int rank = 1;
            for (const auto& row : rows) {
                auto* item = resp->add_items();
                item->set_guide_card_id(row.guide_card_id);
                item->set_score(row.score);
                item->set_rank(rank++);
                item->add_recall_sources(row.recall_sources);
                item->set_placement(scene_hint.empty() ? "feed" : scene_hint);
            }
        }

        resp->mutable_cursor_pagination()->set_has_more(false);
        resp->mutable_cursor_pagination()->set_limit(limit);
    }

    void GetPopularRecommendations(::google::protobuf::RpcController*,
                                   const GetPopularRecommendationsRequest* req,
                                   GetPopularRecommendationsResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        QueryRecommendationsRequest qreq;
        qreq.set_scene(req->scene());
        if (req->has_cursor_limits()) {
            *qreq.mutable_cursor_limits() = req->cursor_limits();
        }
        QueryRecommendationsResponse qresp;
        QueryRecommendations(nullptr, &qreq, &qresp, nullptr);
        resp->set_recommendation_id(qresp.recommendation_id());
        resp->set_scene(qresp.scene());
        resp->set_generated_at(qresp.generated_at());
        *resp->mutable_strategy() = qresp.strategy();
        *resp->mutable_items() = qresp.items();
        *resp->mutable_cursor_pagination() = qresp.cursor_pagination();
    }

    void ExplainRecommendations(::google::protobuf::RpcController*,
                                const ExplainRecommendationsRequest* req,
                                ExplainRecommendationsResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        for (const auto& item_ref : req->items()) {
            auto* exp = resp->add_explanations();
            exp->set_summary("Recommended from PostgreSQL candidate pool");
            exp->add_reason_codes("pg_candidate_pool");
            exp->add_reason_codes("theme_match");
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
        const bool ok = store_->Ping();
        resp->set_status(ok ? HEALTH_OK : HEALTH_DEGRADED);
        resp->set_index_status(ok ? INDEX_UP : INDEX_DOWN);
    }
};

}  // namespace recommendation_server
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);

    simple_living::recommendation_server::PgRecommendationStore store;
    if (!store.ConnectAndInit(FLAGS_pg_conninfo)) {
        LOG(ERROR) << "PostgreSQL ConnectAndInit failed";
        return 1;
    }
    store.SyncCandidatesFromContentTable();

    simple_living::recommendation_server::SnapshotStore snapshot(FLAGS_snapshot_dir);
    auto* catalog_svc =
        simple_living::recommendation_server::NewCatalogReadService(&snapshot);

    simple_living::recommendation_server::RecommendationServiceImpl g_svc(&store);
    brpc::Server server;
    if (server.AddService(&g_svc, brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ||
        !simple_living::recommendation_server::RegisterCatalogReadService(server, catalog_svc)) {
        LOG(ERROR) << "Fail to add recommendation-server services";
        return 1;
    }
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start server";
        return 1;
    }
    LOG(INFO) << "recommendation-server listening on port " << FLAGS_port
              << " snapshot_dir=" << FLAGS_snapshot_dir;
    server.RunUntilAskedToQuit();
    store.Close();
    delete catalog_svc;
    return 0;
}
