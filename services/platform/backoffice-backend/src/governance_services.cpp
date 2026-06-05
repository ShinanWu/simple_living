// platform/backoffice-backend brpc server — PostgreSQL + Redis/Kafka per services/README.md.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include "governance_server.pb.h"
#include "pg_governance_store.h"
#include "snapshot_export.h"


namespace simple_living {
namespace governance_server {

class GovernanceReviewServiceImpl : public GovernanceReviewService {
    PgGovernanceStore* store_;

public:
    explicit GovernanceReviewServiceImpl(PgGovernanceStore* s) : store_(s) {}

    void EnqueueReview(::google::protobuf::RpcController*,
                       const EnqueueReviewRequest* req,
                       EnqueueReviewResponse* resp,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->EnqueueReview(*req, resp);
    }

    void ListReviewQueueItems(::google::protobuf::RpcController*,
                              const ListReviewQueueItemsRequest* req,
                              ListReviewQueueItemsResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ListReviewQueueItems(*req, resp);
    }

    void SubmitReviewDecision(::google::protobuf::RpcController*,
                              const SubmitReviewDecisionRequest* req,
                              SubmitReviewDecisionResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->SubmitReviewDecision(*req, resp);
    }

    void ListReviewDecisions(::google::protobuf::RpcController*,
                             const ListReviewDecisionsRequest* req,
                             ListReviewDecisionsResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ListReviewDecisions(*req, resp);
    }

    void GetReviewState(::google::protobuf::RpcController*,
                        const GetReviewStateRequest* req,
                        GetReviewStateResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->GetReviewState(*req, resp);
    }
};

class GovernanceCooperationServiceImpl : public GovernanceCooperationService {
    PgGovernanceStore* store_;

public:
    explicit GovernanceCooperationServiceImpl(PgGovernanceStore* s) : store_(s) {}

    void UpsertCooperationLabel(::google::protobuf::RpcController*,
                                const UpsertCooperationLabelRequest* req,
                                UpsertCooperationLabelResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->UpsertCooperationLabel(*req, resp);
    }

    void BatchGetCooperationLabels(::google::protobuf::RpcController*,
                                   const BatchGetCooperationLabelsRequest* req,
                                   BatchGetCooperationLabelsResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->BatchGetCooperationLabels(*req, resp);
    }

    void ListDisclosureTemplates(::google::protobuf::RpcController*,
                                 const ListDisclosureTemplatesRequest*,
                                 ListDisclosureTemplatesResponse* resp,
                                 ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ListDisclosureTemplates(resp);
    }
};

class GovernanceVisibilityServiceImpl : public GovernanceVisibilityService {
    PgGovernanceStore* store_;
    backoffice_backend::SnapshotExportCoordinator* export_coord_{nullptr};

public:
    GovernanceVisibilityServiceImpl(PgGovernanceStore* s,
                                    backoffice_backend::SnapshotExportCoordinator* export_coord)
        : store_(s), export_coord_(export_coord) {}

    void SetVisibilityVerdict(::google::protobuf::RpcController*,
                              const SetVisibilityVerdictRequest* req,
                              SetVisibilityVerdictResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->SetVisibilityVerdict(*req, resp);
        if (export_coord_) {
            export_coord_->RefreshNow();
        }
    }

    void BatchSetVisibilityVerdict(::google::protobuf::RpcController*,
                                   const BatchSetVisibilityVerdictRequest* req,
                                   BatchSetVisibilityVerdictResponse* resp,
                                   ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->BatchSetVisibilityVerdict(*req, resp);
        if (export_coord_) {
            export_coord_->RefreshNow();
        }
    }

    void GetVisibilityVerdict(::google::protobuf::RpcController*,
                              const GetVisibilityVerdictRequest* req,
                              GetVisibilityVerdictResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->GetVisibilityVerdict(*req, resp);
    }

    void EvaluateVisibility(::google::protobuf::RpcController*,
                            const EvaluateVisibilityRequest* req,
                            EvaluateVisibilityResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->EvaluateVisibility(*req, resp);
    }
};

class GovernanceRiskServiceImpl : public GovernanceRiskService {
    PgGovernanceStore* store_;

public:
    explicit GovernanceRiskServiceImpl(PgGovernanceStore* s) : store_(s) {}

    void CreateRiskFlag(::google::protobuf::RpcController*,
                        const CreateRiskFlagRequest* req,
                        CreateRiskFlagResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->CreateRiskFlag(*req, resp);
    }

    void RevokeRiskFlag(::google::protobuf::RpcController*,
                        const RevokeRiskFlagRequest* req,
                        RevokeRiskFlagResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->RevokeRiskFlag(*req, resp);
    }

    void ListRiskFlags(::google::protobuf::RpcController*,
                       const ListRiskFlagsRequest* req,
                       ListRiskFlagsResponse* resp,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ListRiskFlags(*req, resp);
    }

    void BatchGetRiskFlagsBySubject(::google::protobuf::RpcController*,
                                    const BatchGetRiskFlagsBySubjectRequest* req,
                                    BatchGetRiskFlagsBySubjectResponse* resp,
                                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        for (const auto& key : req->keys()) {
            ListRiskFlagsRequest lreq;
            lreq.set_subject_id(key.subject_id());
            lreq.set_active_only(req->active_only());
            ListRiskFlagsResponse lresp;
            store_->ListRiskFlags(lreq, &lresp);
            (*resp->mutable_flags_by_subject())[key.subject_id()] = lresp;
        }
    }
};

class GovernanceOpsConfigServiceImpl : public GovernanceOpsConfigService {
public:
    void GetOpsConfigSnapshot(::google::protobuf::RpcController*,
                              const GetOpsConfigSnapshotRequest* req,
                              GetOpsConfigSnapshotResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->mutable_snapshot()->set_config_key(req->config_key());
        resp->mutable_snapshot()->set_version(1);
        resp->mutable_snapshot()->set_payload_json("{}");
    }

    void CreateOpsConfigDraft(::google::protobuf::RpcController*,
                              const CreateOpsConfigDraftRequest* req,
                              CreateOpsConfigDraftResponse* resp,
                              ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->mutable_draft()->set_id("draft_ops");
        resp->mutable_draft()->set_config_key(req->config_key());
        resp->mutable_draft()->set_payload_json(req->payload_json());
    }

    void ReleaseOpsConfig(::google::protobuf::RpcController*,
                          const ReleaseOpsConfigRequest*,
                          ReleaseOpsConfigResponse* resp,
                          ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->mutable_snapshot()->set_version(2);
    }

    void RollbackOpsConfigRelease(::google::protobuf::RpcController*,
                                  const RollbackOpsConfigReleaseRequest* req,
                                  RollbackOpsConfigReleaseResponse* resp,
                                  ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->mutable_snapshot()->set_config_key(req->config_key());
        resp->mutable_snapshot()->set_version(req->target_version());
    }
};

class GovernancePolicyServiceImpl : public GovernancePolicyService {
public:
    void EvaluatePolicy(::google::protobuf::RpcController*,
                        const EvaluatePolicyRequest*,
                        EvaluatePolicyResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_evaluation_id("pe_default");
    }

    void CreatePolicyVersion(::google::protobuf::RpcController*,
                             const CreatePolicyVersionRequest* req,
                             CreatePolicyVersionResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        if (req->has_version()) {
            *resp->mutable_version() = req->version();
        }
    }

    void SimulatePolicy(::google::protobuf::RpcController*,
                        const SimulatePolicyRequest*,
                        SimulatePolicyResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_evaluation_id("sim_default");
    }
};

}  // namespace governance_server

namespace backoffice_backend {

struct GovernanceModule {
    governance_server::PgGovernanceStore store;
    governance_server::GovernanceReviewServiceImpl review{&store};
    governance_server::GovernanceCooperationServiceImpl coop{&store};
    governance_server::GovernanceVisibilityServiceImpl vis{&store, nullptr};
    governance_server::GovernanceOpsConfigServiceImpl ops;
    governance_server::GovernanceRiskServiceImpl risk{&store};
    governance_server::GovernancePolicyServiceImpl policy;
};

bool RegisterGovernanceModule(brpc::Server* server,
                              const std::string& pg_conninfo,
                              GovernanceModule** out_mod,
                              SnapshotExportCoordinator* export_coord) {
    auto* mod = new GovernanceModule();
    if (!mod->store.ConnectAndInit(pg_conninfo)) {
        delete mod;
        return false;
    }
    mod->vis = governance_server::GovernanceVisibilityServiceImpl(&mod->store, export_coord);
    mod->store.EnsureSeedDisclosureTemplates();
    mod->store.EnsurePublishedVisibilityFromContent();
    if (server->AddService(&mod->review, brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ||
        server->AddService(&mod->coop, brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ||
        server->AddService(&mod->vis, brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ||
        server->AddService(&mod->ops, brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ||
        server->AddService(&mod->risk, brpc::SERVER_DOESNT_OWN_SERVICE) != 0 ||
        server->AddService(&mod->policy, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        mod->store.Close();
        delete mod;
        return false;
    }
    *out_mod = mod;
    return true;
}

void ShutdownGovernanceModule(GovernanceModule* mod) {
    if (mod) {
        mod->store.Close();
        delete mod;
    }
}

}  // namespace backoffice_backend
}  // namespace simple_living


