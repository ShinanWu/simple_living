#pragma once

#include <postgresql/libpq-fe.h>
#include <mutex>
#include <string>
#include <vector>

#include "recommendation_server_service.pb.h"

namespace simple_living {
namespace recommendation_server {

struct RecommendationCandidateRow {
    std::string guide_card_id;
    std::string theme_id;
    double score{0.0};
    std::string recall_sources;
};

class PgRecommendationStore {
public:
    PgRecommendationStore() = default;
    ~PgRecommendationStore();
    PgRecommendationStore(const PgRecommendationStore&) = delete;
    PgRecommendationStore& operator=(const PgRecommendationStore&) = delete;

    bool ConnectAndInit(const std::string& conninfo);
    void Close();
    bool Ping();
    bool SyncCandidatesFromContentTable();
    bool QueryCandidates(const std::string& theme_id, int limit, std::vector<RecommendationCandidateRow>* rows);

private:
    std::mutex mu_;
    PGconn* conn_{nullptr};
    bool ExecSql(const char* sql);
};

}  // namespace recommendation_server
}  // namespace simple_living
