// governance-domain brpc server — review/visibility/policy in MySQL/PostgreSQL + Redis per docs/architecture.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "governance_domain.pb.h"

DEFINE_int32(port, 9106, "TCP port for this brpc server");

namespace simple_living {
namespace governance_domain {

namespace {
static std::atomic<int64_t> g_counter{1};
std::string GenId(const std::string& p) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return p + "_" + std::to_string(ts) + "_" + std::to_string(g_counter.fetch_add(1));
}
}  // namespace

class GovernanceReviewServiceImpl : public GovernanceReviewService {
    std::mutex mu_;
    std::unordered_map<std::string, ReviewQueueItem> queue_items_;  // id -> item
    std::unordered_map<std::string, std::vector<ReviewDecision>> decisions_; // content_id -> decisions
public:
    void EnqueueReview(::google::protobuf::RpcController*,
                       const EnqueueReviewRequest* req,
                       EnqueueReviewResponse* resp,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto* item = resp->mutable_queue_item();
        item->set_id(GenId("qi"));
        item->set_content_id(req->content_id());
        item->set_content_version(req->content_version());
        item->set_status(REVIEW_QUEUE_ITEM_STATUS_PENDING);
        item->set_priority(req->priority());
        item->set_enqueue_reason(req->enqueue_reason());
        queue_items_[item->id()] = *item;
    }

    void ListReviewQueueItems(::google::protobuf::RpcController*,
                              const ListReviewQueueItemsRequest* req,
                              ListReviewQueueItemsResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [id, item] : queue_items_) {
            if (req->has_status() && item.status() != req->status()) continue;
            *resp->add_items() = item;
        }
    }

    void SubmitReviewDecision(::google::protobuf::RpcController*,
                              const SubmitReviewDecisionRequest* req,
                              SubmitReviewDecisionResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto* d = resp->mutable_decision();
        d->set_id(GenId("rd"));
        d->set_queue_item_id(req->queue_item_id());
        d->set_outcome(req->outcome());
        d->set_comment(req->comment());
        d->set_reviewer_id(req->reviewer_id());
        auto it = queue_items_.find(req->queue_item_id());
        if (it != queue_items_.end()) {
            d->set_content_id(it->second.content_id());
            it->second.set_status(REVIEW_QUEUE_ITEM_STATUS_COMPLETED);
        }
        decisions_[d->content_id()].push_back(*d);
    }

    void ListReviewDecisions(::google::protobuf::RpcController*,
                             const ListReviewDecisionsRequest* req,
                             ListReviewDecisionsResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = decisions_.find(req->content_id());
        if (it != decisions_.end()) {
            for (auto& d : it->second) *resp->add_decisions() = d;
        }
    }

    void GetReviewState(::google::protobuf::RpcController*,
                        const GetReviewStateRequest* req,
                        GetReviewStateResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        resp->mutable_state()->set_content_id(req->content_id());
        auto dit = decisions_.find(req->content_id());
        if (dit != decisions_.end() && !dit->second.empty()) {
            *resp->mutable_state()->mutable_latest_decision() = dit->second.back();
        }
    }
};

class GovernanceCooperationServiceImpl : public GovernanceCooperationService {
    std::mutex mu_;
    std::unordered_map<std::string, CooperationLabel> labels_; // id -> label
    std::vector<DisclosureTemplate> templates_;
public:
    GovernanceCooperationServiceImpl() {
        DisclosureTemplate t;
        t.set_id("dt_cn_1"); t.set_locale("zh-CN"); t.set_template_key("commercial_basic");
        t.set_body("本内容含有商业合作推广"); t.set_version(1);
        templates_.push_back(t);
    }

    void UpsertCooperationLabel(::google::protobuf::RpcController*,
                                const UpsertCooperationLabelRequest* req,
                                UpsertCooperationLabelResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        if (!req->has_label()) return;
        CooperationLabel lbl = req->label();
        if (lbl.id().empty()) lbl.set_id(GenId("cl"));
        labels_[lbl.id()] = lbl;
        *resp->mutable_label() = lbl;
    }

    void BatchGetCooperationLabels(::google::protobuf::RpcController*,
                                   const BatchGetCooperationLabelsRequest* req,
                                   BatchGetCooperationLabelsResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& key : req->keys()) {
            CooperationLabels cls;
            for (auto& [id, lbl] : labels_) {
                if (lbl.subject_id() == key.subject_id()) *cls.add_labels() = lbl;
            }
            (*resp->mutable_labels_by_subject())[key.subject_id()] = cls;
        }
    }

    void ListDisclosureTemplates(::google::protobuf::RpcController*,
                                 const ListDisclosureTemplatesRequest*,
                                 ListDisclosureTemplatesResponse* resp,
                                 ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        for (auto& t : templates_) *resp->add_templates() = t;
    }
};

class GovernanceVisibilityServiceImpl : public GovernanceVisibilityService {
    std::mutex mu_;
    std::unordered_map<std::string, VisibilityVerdict> verdicts_; // content_id -> verdict
public:
    void SetVisibilityVerdict(::google::protobuf::RpcController*,
                              const SetVisibilityVerdictRequest* req,
                              SetVisibilityVerdictResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto& v = verdicts_[req->content_id()];
        if (v.id().empty()) v.set_id(GenId("vv"));
        v.set_content_id(req->content_id());
        v.set_state(req->state());
        v.set_reason_code(req->reason_code());
        v.set_source(req->source());
        v.set_version(v.version() + 1);
        *resp->mutable_verdict() = v;
    }

    void BatchSetVisibilityVerdict(::google::protobuf::RpcController*,
                                   const BatchSetVisibilityVerdictRequest* req,
                                   BatchSetVisibilityVerdictResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& mut : req->items()) {
            auto& v = verdicts_[mut.content_id()];
            if (v.id().empty()) v.set_id(GenId("vv"));
            v.set_content_id(mut.content_id());
            v.set_state(mut.state());
            v.set_reason_code(mut.reason_code());
            v.set_version(v.version() + 1);
            *resp->add_verdicts() = v;
        }
    }

    void GetVisibilityVerdict(::google::protobuf::RpcController*,
                              const GetVisibilityVerdictRequest* req,
                              GetVisibilityVerdictResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = verdicts_.find(req->content_id());
        if (it != verdicts_.end()) *resp->mutable_verdict() = it->second;
        else {
            // Default: unpublished for unknown content
            resp->mutable_verdict()->set_content_id(req->content_id());
            resp->mutable_verdict()->set_state(VISIBILITY_STATE_UNPUBLISHED);
        }
    }

    void EvaluateVisibility(::google::protobuf::RpcController*,
                            const EvaluateVisibilityRequest* req,
                            EvaluateVisibilityResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = verdicts_.find(req->content_id());
        if (it != verdicts_.end() && it->second.state() == VISIBILITY_STATE_PUBLISHED) {
            resp->set_allowed(true);
            *resp->mutable_verdict() = it->second;
        } else {
            resp->set_allowed(false);
            resp->add_reason_codes("not_published");
        }
    }
};

class GovernanceOpsConfigServiceImpl : public GovernanceOpsConfigService {
    std::mutex mu_;
    struct ConfigEntry { std::string key; std::string payload_json; int32_t version = 0; std::string draft_id; };
    std::unordered_map<std::string, ConfigEntry> configs_; // key -> entry
public:
    void GetOpsConfigSnapshot(::google::protobuf::RpcController*,
                              const GetOpsConfigSnapshotRequest* req,
                              GetOpsConfigSnapshotResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = configs_.find(req->config_key());
        if (it != configs_.end()) {
            resp->mutable_snapshot()->set_config_key(it->second.key);
            resp->mutable_snapshot()->set_version(it->second.version);
            resp->mutable_snapshot()->set_payload_json(it->second.payload_json);
        }
    }

    void CreateOpsConfigDraft(::google::protobuf::RpcController*,
                              const CreateOpsConfigDraftRequest* req,
                              CreateOpsConfigDraftResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto* draft = resp->mutable_draft();
        draft->set_id(GenId("draft"));
        draft->set_config_key(req->config_key());
        draft->set_payload_json(req->payload_json());
        configs_[req->config_key()].draft_id = draft->id();
        configs_[req->config_key()].key = req->config_key();
    }

    void ReleaseOpsConfig(::google::protobuf::RpcController*,
                          const ReleaseOpsConfigRequest* req,
                          ReleaseOpsConfigResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [k, c] : configs_) {
            if (c.draft_id == req->draft_id()) {
                c.version++;
                resp->mutable_snapshot()->set_config_key(c.key);
                resp->mutable_snapshot()->set_version(c.version);
                break;
            }
        }
    }

    void RollbackOpsConfigRelease(::google::protobuf::RpcController*,
                                  const RollbackOpsConfigReleaseRequest* req,
                                  RollbackOpsConfigReleaseResponse* resp,
                                  ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = configs_.find(req->config_key());
        if (it != configs_.end()) {
            it->second.version = req->target_version();
            resp->mutable_snapshot()->set_config_key(req->config_key());
            resp->mutable_snapshot()->set_version(req->target_version());
        }
    }
};

class GovernanceRiskServiceImpl : public GovernanceRiskService {
    std::mutex mu_;
    std::unordered_map<std::string, RiskFlag> flags_; // id -> flag
public:
    void CreateRiskFlag(::google::protobuf::RpcController*,
                        const CreateRiskFlagRequest* req,
                        CreateRiskFlagResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto* f = resp->mutable_flag();
        f->set_id(GenId("rf"));
        f->set_subject_type(req->subject_type());
        f->set_subject_id(req->subject_id());
        f->set_flag_code(req->flag_code());
        f->set_severity(req->severity());
        f->set_source(req->source());
        f->set_active(true);
        flags_[f->id()] = *f;
    }

    void RevokeRiskFlag(::google::protobuf::RpcController*,
                        const RevokeRiskFlagRequest* req,
                        RevokeRiskFlagResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        auto it = flags_.find(req->flag_id());
        if (it != flags_.end()) { it->second.set_active(false); *resp->mutable_flag() = it->second; }
    }

    void ListRiskFlags(::google::protobuf::RpcController*,
                       const ListRiskFlagsRequest* req,
                       ListRiskFlagsResponse* resp,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [id, f] : flags_) {
            if (!req->subject_id().empty() && f.subject_id() != req->subject_id()) continue;
            if (req->active_only() && !f.active()) continue;
            *resp->add_flags() = f;
        }
    }

    void BatchGetRiskFlagsBySubject(::google::protobuf::RpcController*,
                                    const BatchGetRiskFlagsBySubjectRequest* req,
                                    BatchGetRiskFlagsBySubjectResponse* resp,
                                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& key : req->keys()) {
            RiskFlagList list;
            for (auto& [id, f] : flags_) {
                if (f.subject_id() == key.subject_id()) {
                    if (req->active_only() && !f.active()) continue;
                    *list.add_flags() = f;
                }
            }
            (*resp->mutable_flags_by_subject())[key.subject_id()] = list;
        }
    }
};

class GovernancePolicyServiceImpl : public GovernancePolicyService {
    std::mutex mu_;
    std::unordered_map<std::string, PolicyVersion> policy_versions_; // id -> version
public:
    void EvaluatePolicy(::google::protobuf::RpcController*,
                        const EvaluatePolicyRequest*,
                        EvaluatePolicyResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_evaluation_id(GenId("pe"));
    }

    void CreatePolicyVersion(::google::protobuf::RpcController*,
                             const CreatePolicyVersionRequest* req,
                             CreatePolicyVersionResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        std::lock_guard<std::mutex> lock(mu_);
        if (!req->has_version()) return;
        PolicyVersion v = req->version();
        if (v.id().empty()) v.set_id(GenId("pv"));
        policy_versions_[v.id()] = v;
        *resp->mutable_version() = v;
    }

    void SimulatePolicy(::google::protobuf::RpcController*,
                        const SimulatePolicyRequest*,
                        SimulatePolicyResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_evaluation_id(GenId("sim"));
    }
};

}  // namespace governance_domain
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;
    simple_living::governance_domain::GovernanceReviewServiceImpl g_review;
    if (server.AddService(&g_review, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) { LOG(ERROR) << "Fail review"; return 1; }
    simple_living::governance_domain::GovernanceCooperationServiceImpl g_coop;
    if (server.AddService(&g_coop, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) { LOG(ERROR) << "Fail coop"; return 1; }
    simple_living::governance_domain::GovernanceVisibilityServiceImpl g_vis;
    if (server.AddService(&g_vis, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) { LOG(ERROR) << "Fail vis"; return 1; }
    simple_living::governance_domain::GovernanceOpsConfigServiceImpl g_ops;
    if (server.AddService(&g_ops, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) { LOG(ERROR) << "Fail ops"; return 1; }
    simple_living::governance_domain::GovernanceRiskServiceImpl g_risk;
    if (server.AddService(&g_risk, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) { LOG(ERROR) << "Fail risk"; return 1; }
    simple_living::governance_domain::GovernancePolicyServiceImpl g_policy;
    if (server.AddService(&g_policy, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) { LOG(ERROR) << "Fail policy"; return 1; }
    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) { LOG(ERROR) << "Fail to start server"; return 1; }
    LOG(INFO) << "governance-domain server listening on port " << FLAGS_port;
    server.RunUntilAskedToQuit();
    return 0;
}
