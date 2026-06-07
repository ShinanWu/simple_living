// platform/backoffice-backend brpc server — PostgreSQL partner/rules store per services/README.md.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <mutex>
#include <string>
#include <unordered_set>

#include "affiliate_server.pb.h"
#include "pg_affiliate_store.h"
#include "snapshot_export.h"


namespace simple_living {
namespace affiliate_server {

namespace {
std::string GenId(const std::string& prefix) {
    return prefix + "_local";
}
}  // namespace

class AffiliatePartnerServiceImpl : public AffiliatePartnerService {
    PgAffiliateStore* store_;
    backoffice_backend::SnapshotExportCoordinator* export_coord_{nullptr};

public:
    explicit AffiliatePartnerServiceImpl(PgAffiliateStore* s) : store_(s) {}

    void SetExportCoordinator(backoffice_backend::SnapshotExportCoordinator* export_coord) {
        export_coord_ = export_coord;
    }

    void GetPartnerCapabilitySnapshot(::google::protobuf::RpcController*,
                                      const GetPartnerCapabilitySnapshotRequest* req,
                                      GetPartnerCapabilitySnapshotResponse* resp,
                                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->GetPartnerCapabilitySnapshot(req->partner_id(), resp->mutable_snapshot());
    }

    void ValidateLinkGenerationInput(::google::protobuf::RpcController*,
                                     const ValidateLinkGenerationInputRequest* req,
                                     ValidateLinkGenerationInputResponse* resp,
                                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::string partner_id = req->has_input() ? req->input().partner_id() : "tmall";
        PartnerCapabilitySnapshot snap;
        store_->GetPartnerCapabilitySnapshot(partner_id, &snap);
        auto* spec = resp->mutable_spec();
        spec->set_spec_id(GenId("spec"));
        spec->set_partner_id(partner_id);
        spec->set_target_base("https://go.simpleliving.com/r");
    }

    void ListPartners(::google::protobuf::RpcController*,
                      const ListPartnersRequest*,
                      ListPartnersResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::vector<PartnerCapabilitySnapshot> items;
        store_->ListPartners(&items);
        for (const auto& item : items) {
            *resp->add_items() = item;
        }
    }

    void UpsertPartnerBackoffice(::google::protobuf::RpcController*,
                                 const UpsertPartnerBackofficeRequest* req,
                                 UpsertPartnerBackofficeResponse* resp,
                                 ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (!req->has_partner() || req->partner().partner_id().empty()) {
            return;
        }
        PartnerCapabilitySnapshot snap = req->partner();
        if (snap.capability_set_id().empty()) {
            snap.set_capability_set_id(GenId("cs"));
        }
        if (snap.lifecycle_status() == PARTNER_LIFECYCLE_STATUS_UNSPECIFIED) {
            snap.set_lifecycle_status(PARTNER_LIFECYCLE_STATUS_ACTIVE);
        }
        if (snap.flags_size() == 0) {
            snap.add_flags(CAPABILITY_FLAG_DEEP_LINK);
        }
        store_->UpsertPartner(snap);
        *resp->mutable_partner() = snap;
        if (export_coord_) {
            export_coord_->RefreshNow();
        }
    }
};

class AffiliateCommissionRuleServiceImpl : public AffiliateCommissionRuleService {
    PgAffiliateStore* store_;

public:
    explicit AffiliateCommissionRuleServiceImpl(PgAffiliateStore* s) : store_(s) {}

    void CreateCommissionRuleSetVersion(::google::protobuf::RpcController*,
                                        const CreateCommissionRuleSetVersionRequest* req,
                                        CreateCommissionRuleSetVersionResponse* resp,
                                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::string rsid;
        int32_t ver = 0;
        store_->CreateCommissionRuleSetVersion(req->partner_id(), &rsid, &ver);
        resp->set_rule_set_id(rsid);
        resp->set_version(ver);
    }

    void GetCommissionRuleSet(::google::protobuf::RpcController*,
                              const GetCommissionRuleSetRequest* req,
                              GetCommissionRuleSetResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::string rsid;
        int32_t ver = 0;
        store_->GetCommissionRuleSet(req->partner_id(), &rsid, &ver);
        resp->set_rule_set_id(rsid);
        resp->set_version(ver);
        resp->set_rule_payload("{}");
    }
};

class AffiliateIntakeServiceImpl : public AffiliateIntakeService {
    std::mutex mu_;
    std::unordered_set<std::string> seen_event_ids_;
    std::unordered_set<std::string> seen_batch_checksums_;

public:
    void IngestPartnerWebhook(::google::protobuf::RpcController*,
                              const IngestPartnerWebhookRequest* req,
                              IngestPartnerWebhookResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        const bool dup = seen_event_ids_.count(req->partner_event_id()) > 0;
        if (!dup) {
            seen_event_ids_.insert(req->partner_event_id());
        }
        resp->set_accepted(!dup);
    }

    void RegisterReportBatch(::google::protobuf::RpcController*,
                             const RegisterReportBatchRequest* req,
                             RegisterReportBatchResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        const bool dup = seen_batch_checksums_.count(req->batch_checksum()) > 0;
        if (!dup) {
            seen_batch_checksums_.insert(req->batch_checksum());
        }
        resp->set_batch_id(GenId("batch"));
        resp->set_duplicate(dup);
    }
};

}  // namespace affiliate_server

namespace backoffice_backend {

struct AffiliateModule {
    affiliate_server::PgAffiliateStore store;
    affiliate_server::AffiliatePartnerServiceImpl partner{&store};
    affiliate_server::AffiliateCommissionRuleServiceImpl rule{&store};
    affiliate_server::AffiliateIntakeServiceImpl intake;
};

bool RegisterAffiliateModule(brpc::Server* server,
                             const std::string& pg_conninfo,
                             AffiliateModule** out_mod,
                             SnapshotExportCoordinator* export_coord) {
    auto* mod = new AffiliateModule();
    if (!mod->store.ConnectAndInit(pg_conninfo)) {
        delete mod;
        return false;
    }
    mod->store.EnsureSeedPartners();
    if (export_coord) {
        mod->partner.SetExportCoordinator(export_coord);
    }
    if (server->AddService(&mod->partner, brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ||
        server->AddService(&mod->rule, brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ||
        server->AddService(&mod->intake, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        mod->store.Close();
        delete mod;
        return false;
    }
    *out_mod = mod;
    return true;
}

void ShutdownAffiliateModule(AffiliateModule* mod) {
    if (mod) {
        mod->store.Close();
        delete mod;
    }
}

affiliate_server::PgAffiliateStore* AffiliateModuleStore(AffiliateModule* mod) {
    return mod ? &mod->store : nullptr;
}

}  // namespace backoffice_backend
}  // namespace simple_living


