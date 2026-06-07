#pragma once

#include <postgresql/libpq-fe.h>
#include <mutex>
#include <string>
#include <vector>

#include "governance_server.pb.h"

namespace simple_living {
namespace governance_server {

class PgGovernanceStore {
public:
    PgGovernanceStore() = default;
    ~PgGovernanceStore();
    PgGovernanceStore(const PgGovernanceStore&) = delete;
    PgGovernanceStore& operator=(const PgGovernanceStore&) = delete;

    bool ConnectAndInit(const std::string& conninfo);
    void Close();
    bool Ping();
    bool EnsureSeedDisclosureTemplates();
    bool EnsurePublishedVisibilityFromContent();

    bool EnqueueReview(const EnqueueReviewRequest& req, EnqueueReviewResponse* resp);
    bool ListReviewQueueItems(const ListReviewQueueItemsRequest& req, ListReviewQueueItemsResponse* resp);
    bool SubmitReviewDecision(const SubmitReviewDecisionRequest& req, SubmitReviewDecisionResponse* resp);
    bool ListReviewDecisions(const ListReviewDecisionsRequest& req, ListReviewDecisionsResponse* resp);
    bool GetReviewState(const GetReviewStateRequest& req, GetReviewStateResponse* resp);

    bool SetVisibilityVerdict(const SetVisibilityVerdictRequest& req, SetVisibilityVerdictResponse* resp);
    bool BatchSetVisibilityVerdict(const BatchSetVisibilityVerdictRequest& req,
                                   BatchSetVisibilityVerdictResponse* resp);
    bool GetVisibilityVerdict(const GetVisibilityVerdictRequest& req, GetVisibilityVerdictResponse* resp);
    bool EvaluateVisibility(const EvaluateVisibilityRequest& req, EvaluateVisibilityResponse* resp);
    bool IsCsideVisible(const std::string& content_id);

    bool UpsertCooperationLabel(const UpsertCooperationLabelRequest& req, UpsertCooperationLabelResponse* resp);
    bool BatchGetCooperationLabels(const BatchGetCooperationLabelsRequest& req,
                                   BatchGetCooperationLabelsResponse* resp);
    bool ListDisclosureTemplates(ListDisclosureTemplatesResponse* resp);

    bool CreateRiskFlag(const CreateRiskFlagRequest& req, CreateRiskFlagResponse* resp);
    bool RevokeRiskFlag(const RevokeRiskFlagRequest& req, RevokeRiskFlagResponse* resp);
    bool ListRiskFlags(const ListRiskFlagsRequest& req, ListRiskFlagsResponse* resp);

private:
    std::mutex mu_;
    PGconn* conn_{nullptr};

    bool ExecSql(const char* sql);
    PGresult* ExecParams(const char* sql,
                         int n_params,
                         const char* const* param_values,
                         const int* param_lengths = nullptr,
                         const int* param_formats = nullptr);
    static std::string GenId(const std::string& prefix);
    static std::string HexEncode(const std::string& raw);
    static bool HexDecode(const std::string& hex, std::string* out);
    bool RecordOutboxEventLocked(const std::string& topic, const std::string& payload);
};

}  // namespace governance_server
}  // namespace simple_living
