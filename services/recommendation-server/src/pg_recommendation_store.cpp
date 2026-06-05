#include "pg_recommendation_store.h"

#include <butil/logging.h>

namespace simple_living {
namespace recommendation_server {

namespace {

void ClearRes(PGresult* r) {
    if (r) {
        PQclear(r);
    }
}

}  // namespace

PgRecommendationStore::~PgRecommendationStore() {
    Close();
}

void PgRecommendationStore::Close() {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool PgRecommendationStore::ExecSql(const char* sql) {
    PGresult* r = PQexec(conn_, sql);
    const auto st = r ? PQresultStatus(r) : PGRES_FATAL_ERROR;
    const bool ok = (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
    if (!ok && r) {
        LOG(ERROR) << "PG exec: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    return ok;
}

bool PgRecommendationStore::ConnectAndInit(const std::string& conninfo) {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
    conn_ = PQconnectdb(conninfo.c_str());
    if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
        LOG(ERROR) << "PQconnectdb failed: " << (conn_ ? PQerrorMessage(conn_) : "null conn");
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
        return false;
    }

    static const char* kDdl[] = {
        "CREATE TABLE IF NOT EXISTS recommendation_candidate ("
        "  guide_card_id TEXT PRIMARY KEY,"
        "  theme_id TEXT NOT NULL,"
        "  score DOUBLE PRECISION NOT NULL DEFAULT 1.0,"
        "  recall_sources TEXT NOT NULL DEFAULT 'content_published',"
        "  updated_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS recommendation_scene_config ("
        "  scene TEXT PRIMARY KEY,"
        "  pipeline TEXT NOT NULL,"
        "  strategy_id TEXT NOT NULL,"
        "  strategy_version TEXT NOT NULL,"
        "  updated_at TIMESTAMPTZ NOT NULL DEFAULT now())",
    };
    for (auto* ddl : kDdl) {
        if (!ExecSql(ddl)) {
            PQfinish(conn_);
            conn_ = nullptr;
            return false;
        }
    }
    ExecSql("INSERT INTO recommendation_scene_config (scene, pipeline, strategy_id, strategy_version) "
            "VALUES ('HOME_FEED', 'pg_candidate_pool', 'strat_pg', '1.0') "
            "ON CONFLICT (scene) DO NOTHING");
    LOG(INFO) << "recommendation-server PostgreSQL schema ready";
    return true;
}

bool PgRecommendationStore::Ping() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
        return false;
    }
    PGresult* r = PQexec(conn_, "SELECT 1");
    const bool ok = r && PQresultStatus(r) == PGRES_TUPLES_OK;
    ClearRes(r);
    return ok;
}

bool PgRecommendationStore::SyncCandidatesFromContentTable() {
    std::lock_guard<std::mutex> lock(mu_);
  // content_guide_card is owned by platform/backoffice-backend in the same database.
    const char* sql =
        "INSERT INTO recommendation_candidate (guide_card_id, theme_id, score, recall_sources, updated_at) "
        "SELECT card_id, COALESCE(theme_id, ''), 1.0, 'content_published', now() "
        "FROM content_guide_card WHERE status = 3 "
        "ON CONFLICT (guide_card_id) DO UPDATE SET theme_id = EXCLUDED.theme_id, "
        "score = EXCLUDED.score, recall_sources = EXCLUDED.recall_sources, updated_at = now()";
    PGresult* r = PQexec(conn_, sql);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    if (!ok && r) {
        LOG(WARNING) << "SyncCandidatesFromContentTable: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    return ok;
}

bool PgRecommendationStore::QueryCandidates(const std::string& theme_id,
                                            int limit,
                                            std::vector<RecommendationCandidateRow>* rows) {
    std::lock_guard<std::mutex> lock(mu_);
    rows->clear();
    if (limit <= 0) {
        limit = 10;
    }
    const std::string lim = std::to_string(limit);
    PGresult* r = nullptr;
    if (!theme_id.empty()) {
        const char* pv[] = {theme_id.c_str(), lim.c_str()};
        r = PQexecParams(conn_,
                         "SELECT guide_card_id, theme_id, score, recall_sources FROM recommendation_candidate "
                         "WHERE theme_id = $1 ORDER BY score DESC, updated_at DESC LIMIT $2::int",
                         2, nullptr, pv, nullptr, nullptr, 0);
    } else {
        const char* pv[] = {lim.c_str()};
        r = PQexecParams(conn_,
                         "SELECT guide_card_id, theme_id, score, recall_sources FROM recommendation_candidate "
                         "ORDER BY score DESC, updated_at DESC LIMIT $1::int",
                         1, nullptr, pv, nullptr, nullptr, 0);
    }
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return false;
    }
    for (int i = 0; i < PQntuples(r); ++i) {
        RecommendationCandidateRow row;
        row.guide_card_id = PQgetvalue(r, i, 0);
        row.theme_id = PQgetvalue(r, i, 1);
        row.score = std::atof(PQgetvalue(r, i, 2));
        row.recall_sources = PQgetvalue(r, i, 3);
        rows->push_back(row);
    }
    ClearRes(r);
    return true;
}

}  // namespace recommendation_server
}  // namespace simple_living
