// affiliate-domain brpc server — partner/rules data in MySQL/PostgreSQL + Redis per docs/architecture.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "affiliate_domain.pb.h"

DEFINE_int32(port, 9104, "TCP port for this brpc server");

namespace simple_living {
namespace affiliate_domain {

namespace {
static std::atomic<int64_t> g_counter{1};
std::string GenId(const std::string& prefix) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return prefix + "_" + std::to_string(ts) + "_" + std::to_string(g_counter.fetch_add(1));
}
void SeedPartners(std::unordered_map<std::string, PartnerCapabilitySnapshot>& partners) {
    for (const char* pid : {"partner_taobao", "partner_jd", "partner_pinduoduo"}) {
        PartnerCapabilitySnapshot s;
        s.set_partner_id(pid);
        s.set_capability_set_id(GenId("cs"));
        s.set_lifecycle_status(PARTNER_LIFECYCLE_STATUS_ACTIVE);
        s.add_flags(CAPABILITY_FLAG_DEEP_LINK);
        s.add_flags(CAPABILITY_FLAG_SUB_ID_SLOTS);
        s.mutable_link_constraints()->set_max_url_length(2048);
        partners[pid] = s;
    }
}
}  // namespace

class AffiliatePartnerServiceImpl : public AffiliatePartnerService {
    std::mutex mu_;
    std::unordered_map<std::string, PartnerCapabilitySnapshot> partners_;
public:
    AffiliatePartnerServiceImpl() { SeedPartners(partners_); }

    void GetPartnerCapabilitySnapshot(::google::protobuf::RpcController*,
                                      const GetPartnerCapabilitySnapshotRequest* req,
                                      GetPartnerCapabilitySnapshotResponse* resp,
                                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = partners_.find(req->partner_id());
        if (it != partners_.end()) {
            *resp->mutable_snapshot() = it->second;
        } else {
            PartnerCapabilitySnapshot s;
            s.set_partner_id(req->partner_id());
            s.set_capability_set_id(GenId("cs"));
            s.set_lifecycle_status(PARTNER_LIFECYCLE_STATUS_ACTIVE);
            s.add_flags(CAPABILITY_FLAG_DEEP_LINK);
            partners_[req->partner_id()] = s;
            *resp->mutable_snapshot() = s;
        }
    }

    void ValidateLinkGenerationInput(::google::protobuf::RpcController*,
                                     const ValidateLinkGenerationInputRequest* req,
                                     ValidateLinkGenerationInputResponse* resp,
                                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::string partner_id = req->has_input() ? req->input().partner_id() : "unknown";
        auto* spec = resp->mutable_spec();
        spec->set_spec_id(GenId("spec"));
        spec->set_partner_id(partner_id);
        spec->set_target_base("https://go.simpleliving.com/r");
    }
};

class AffiliateCommissionRuleServiceImpl : public AffiliateCommissionRuleService {
    std::mutex mu_;
    struct RuleSet { std::string id; int32_t version; std::string partner_id; std::string payload; };
    std::unordered_map<std::string, RuleSet> rules_;  // partner_id -> latest
public:
    void CreateCommissionRuleSetVersion(::google::protobuf::RpcController*,
                                        const CreateCommissionRuleSetVersionRequest* req,
                                        CreateCommissionRuleSetVersionResponse* resp,
                                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto& r = rules_[req->partner_id()];
        r.id = GenId("rs");
        r.version++;
        r.partner_id = req->partner_id();
        resp->set_rule_set_id(r.id);
        resp->set_version(r.version);
    }

    void GetCommissionRuleSet(::google::protobuf::RpcController*,
                              const GetCommissionRuleSetRequest* req,
                              GetCommissionRuleSetResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = rules_.find(req->partner_id());
        if (it != rules_.end()) {
            resp->set_rule_set_id(it->second.id);
            resp->set_version(it->second.version);
            resp->set_rule_payload(it->second.payload);
        }
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
        bool dup = seen_event_ids_.count(req->partner_event_id()) > 0;
        if (!dup) seen_event_ids_.insert(req->partner_event_id());
        resp->set_accepted(!dup);
    }

    void RegisterReportBatch(::google::protobuf::RpcController*,
                             const RegisterReportBatchRequest* req,
                             RegisterReportBatchResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        bool dup = seen_batch_checksums_.count(req->batch_checksum()) > 0;
        if (!dup) seen_batch_checksums_.insert(req->batch_checksum());
        resp->set_batch_id(GenId("batch"));
        resp->set_duplicate(dup);
    }
};

}  // namespace affiliate_domain
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;
    simple_living::affiliate_domain::AffiliatePartnerServiceImpl g_partner;
    if (server.AddService(&g_partner, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add AffiliatePartnerService"; return 1;
    }
    simple_living::affiliate_domain::AffiliateCommissionRuleServiceImpl g_rule;
    if (server.AddService(&g_rule, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add AffiliateCommissionRuleService"; return 1;
    }
    simple_living::affiliate_domain::AffiliateIntakeServiceImpl g_intake;
    if (server.AddService(&g_intake, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add AffiliateIntakeService"; return 1;
    }
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start server"; return 1;
    }
    LOG(INFO) << "affiliate-domain server listening on port " << FLAGS_port;
    server.RunUntilAskedToQuit();
    return 0;
}
