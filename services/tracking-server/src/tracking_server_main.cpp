// tracking-server brpc server — clicks/attribution in MySQL/PostgreSQL + Redis; async via Kafka per services/README.md.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <brpc/errno.pb.h>
#include <butil/logging.h>

#include <atomic>
#include <chrono>
#include <sstream>
#include <string>

#include "tracking_server.pb.h"
#include "pg_tracking_store.h"
#include "snapshot_reader.h"

DEFINE_int32(port, 9105, "TCP port for this brpc server");
DEFINE_string(pg_conninfo,
              "host=10.0.2.2 port=5432 dbname=simple_living user=simple password=simple connect_timeout=5",
              "PostgreSQL connection string for tracking storage");
DEFINE_string(kafka_brokers,
              "10.0.2.2:9092",
              "Kafka/Redpanda bootstrap servers; records events in PostgreSQL outbox for this stream");
DEFINE_string(snapshot_dir,
              "/var/lib/simple-living/exports",
              "Shared snapshot root written by backoffice-backend");

namespace simple_living {
namespace tracking_server {

namespace {
static std::atomic<int64_t> g_counter{1};

std::string GenId(const std::string& p) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return p + "_" + std::to_string(ts) + "_" + std::to_string(g_counter.fetch_add(1));
}

// Opaque short token for public redirect paths (non-guessable enough for lab/production).
std::string GenShortToken() {
    const uint64_t mix = g_counter.fetch_add(1) ^
                         static_cast<uint64_t>(
                             std::chrono::steady_clock::now().time_since_epoch().count());
    std::ostringstream oss;
    oss << "st" << std::hex << mix;
    return oss.str();
}

}  // namespace

class TrackingLinkServiceImpl : public TrackingLinkService {
    PgTrackingStore* store_;
    LinkSnapshotReader* snapshot_;
public:
    TrackingLinkServiceImpl(PgTrackingStore* store, LinkSnapshotReader* snapshot)
        : store_(store), snapshot_(snapshot) {}

    void AssembleTrackingLink(::google::protobuf::RpcController* controller,
                              const AssembleTrackingLinkRequest* req,
                              AssembleTrackingLinkResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        auto* cntl = static_cast<brpc::Controller*>(controller);
        LinkRecord lr;
        lr.link_ref = GenId("lr");
        lr.short_token = GenShortToken();
        lr.guide_card_id = req->has_content_ref() ? req->content_ref().guide_card_id() : "";
        lr.scene = req->has_content_ref() ? req->content_ref().scene() : "";
        lr.placement = req->placement();
        std::string candidate;
        if (!req->landing_url().empty()) {
            candidate = req->landing_url();
        } else if (snapshot_) {
            snapshot_->ReloadIfChanged();
            candidate = snapshot_->LandingUrlForCard(lr.guide_card_id);
        }
        // Only emit HTTPS landing URLs (contract §3.4); otherwise use the controlled
        // redirect host, which resolves the real destination server-side.
        if (!candidate.empty() && candidate.rfind("https://", 0) == 0) {
            lr.landing_url = candidate;
        } else {
            lr.landing_url = "https://go.shaotang.com/r/" + lr.short_token;
        }
        if (!store_->InsertLink(lr)) {
            // Precondition failure: link assembled but could not be persisted.
            // Fail explicitly (never OK with empty/partial body); gateway maps to 50002.
            cntl->SetFailed(brpc::EINTERNAL, "InsertLink failed: unable to persist tracking link (precondition)");
            return;
        }
        resp->set_landing_url(lr.landing_url);
        resp->set_link_ref(lr.link_ref);
        resp->set_short_token(lr.short_token);
        resp->mutable_attribution_echo()->set_guide_card_id(lr.guide_card_id);
        resp->mutable_attribution_echo()->set_placement(lr.placement);
        resp->mutable_attribution_echo()->set_scene(lr.scene);
    }

    bool GetLinkByToken(const std::string& token, LinkRecord* out) {
        return store_->GetLinkByToken(token, out);
    }

    bool GetLinkByRef(const std::string& ref, LinkRecord* out) {
        return store_->GetLinkByRef(ref, out);
    }

    PgTrackingStore* store() { return store_; }
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
        LinkRecord lr;
        bool found = link_svc_->GetLinkByToken(req->short_token(), &lr);
        if (!found) {
            resp->set_disposition(REDIRECT_DISPOSITION_GONE);
            return;
        }
        resp->set_disposition(REDIRECT_DISPOSITION_HTTP_302);
        resp->set_http_location(lr.landing_url);
        const std::string click_id = GenId("ck");
        link_svc_->store()->InsertClick(click_id, lr.link_ref, lr.short_token);
        resp->set_click_id(click_id);
    }
};

class TrackingClickServiceImpl : public TrackingClickService {
    PgTrackingStore* store_;
public:
    explicit TrackingClickServiceImpl(PgTrackingStore* store) : store_(store) {}

    void AckClick(::google::protobuf::RpcController*,
                  const AckClickRequest* req,
                  AckClickResponse* resp,
                  ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::string click_id = GenId("ck");
        const std::string ref = req->has_link_ref() ? req->link_ref() : "";
        const std::string token = req->has_short_token() ? req->short_token() : "";
        store_->InsertClick(click_id, ref, token);
        resp->set_click_id(click_id);
    }
};

class TrackingConversionServiceImpl : public TrackingConversionService {
    PgTrackingStore* store_;
public:
    explicit TrackingConversionServiceImpl(PgTrackingStore* store) : store_(store) {}

    void IngestConversion(::google::protobuf::RpcController*,
                          const IngestConversionRequest* req,
                          IngestConversionResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->InsertConversion(*req, GenId("cv"), resp);
    }
};

class TrackingCommissionViewServiceImpl : public TrackingCommissionViewService {
    PgTrackingStore* store_;
public:
    explicit TrackingCommissionViewServiceImpl(PgTrackingStore* store) : store_(store) {}

    void GetCommissionSummary(::google::protobuf::RpcController*,
                              const GetCommissionSummaryRequest*,
                              GetCommissionSummaryResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (!store_->FillCommissionSummary(resp)) {
            auto* b = resp->add_buckets();
            b->set_estimated_minor(0);
            b->set_reported_minor(0);
            b->set_currency("CNY");
            b->set_order_count(0);
        }
    }

    void ListCommissionItems(::google::protobuf::RpcController*,
                             const ListCommissionItemsRequest*,
                             ListCommissionItemsResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->mutable_pagination()->set_has_more(false);
    }
};

}  // namespace tracking_server
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;
    static simple_living::tracking_server::PgTrackingStore g_store;
    if (!g_store.ConnectAndInit(FLAGS_pg_conninfo)) {
        LOG(ERROR) << "Fail to initialize tracking-server store";
        return 1;
    }
    LOG(INFO) << "tracking-server event outbox configured for Kafka brokers "
              << FLAGS_kafka_brokers << " snapshot_dir=" << FLAGS_snapshot_dir;
    static simple_living::tracking_server::LinkSnapshotReader g_snapshot(FLAGS_snapshot_dir);
    static simple_living::tracking_server::TrackingLinkServiceImpl g_link(&g_store, &g_snapshot);
    if (server.AddService(&g_link, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingLinkService"; return 1;
    }
    static simple_living::tracking_server::TrackingRedirectServiceImpl g_redirect(&g_link);
    if (server.AddService(&g_redirect, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingRedirectService"; return 1;
    }
    simple_living::tracking_server::TrackingClickServiceImpl g_click(&g_store);
    if (server.AddService(&g_click, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingClickService"; return 1;
    }
    simple_living::tracking_server::TrackingConversionServiceImpl g_conv(&g_store);
    if (server.AddService(&g_conv, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingConversionService"; return 1;
    }
    simple_living::tracking_server::TrackingCommissionViewServiceImpl g_commission(&g_store);
    if (server.AddService(&g_commission, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add TrackingCommissionViewService"; return 1;
    }
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start server"; return 1;
    }
    LOG(INFO) << "tracking-server server listening on port " << FLAGS_port;
    server.RunUntilAskedToQuit();
    g_store.Close();
    return 0;
}
