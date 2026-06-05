#include "pg_tracking_store.h"

#include <cstdlib>

#include <butil/logging.h>

namespace simple_living {
namespace tracking_server {

namespace {

void ClearRes(PGresult* r) {
    if (r) {
        PQclear(r);
    }
}

std::string MoneyValue(const MoneyMinor& money) {
    return std::to_string(money.value_minor());
}

std::string CommissionValue(const CommissionMinor& commission) {
    return std::to_string(commission.value_minor());
}

}  // namespace

PgTrackingStore::~PgTrackingStore() {
    Close();
}

void PgTrackingStore::Close() {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool PgTrackingStore::ExecSql(const char* sql) {
    PGresult* r = PQexec(conn_, sql);
    const auto st = r ? PQresultStatus(r) : PGRES_FATAL_ERROR;
    const bool ok = (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
    if (!ok && r) {
        LOG(ERROR) << "PG exec: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    return ok;
}

PGresult* PgTrackingStore::ExecParams(const char* sql,
                                      int n_params,
                                      const char* const* param_values,
                                      const int* param_lengths,
                                      const int* param_formats) {
    return PQexecParams(conn_, sql, n_params, nullptr, param_values, param_lengths, param_formats, 0);
}

bool PgTrackingStore::ConnectAndInit(const std::string& conninfo) {
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
        "CREATE TABLE IF NOT EXISTS tracking_link ("
        "link_ref TEXT PRIMARY KEY,"
        "short_token TEXT NOT NULL UNIQUE,"
        "landing_url TEXT NOT NULL,"
        "guide_card_id TEXT,"
        "scene TEXT,"
        "placement TEXT,"
        "status INT NOT NULL DEFAULT 1,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS tracking_click ("
        "click_id TEXT PRIMARY KEY,"
        "link_ref TEXT,"
        "short_token TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE INDEX IF NOT EXISTS idx_tracking_click_link_ref ON tracking_click (link_ref)",
        "CREATE TABLE IF NOT EXISTS tracking_conversion ("
        "conversion_id TEXT PRIMARY KEY,"
        "external_event_id TEXT UNIQUE,"
        "click_id TEXT,"
        "conversion_type INT,"
        "amount_minor BIGINT NOT NULL DEFAULT 0,"
        "commission_minor BIGINT NOT NULL DEFAULT 0,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS tracking_event_outbox ("
        "event_id BIGSERIAL PRIMARY KEY,"
        "topic TEXT NOT NULL,"
        "payload TEXT NOT NULL,"
        "published BOOLEAN NOT NULL DEFAULT FALSE,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
    };
    for (auto* ddl : kDdl) {
        if (!ExecSql(ddl)) {
            PQfinish(conn_);
            conn_ = nullptr;
            return false;
        }
    }
    LOG(INFO) << "tracking-server PostgreSQL schema ready";
    return true;
}

bool PgTrackingStore::Ping() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
        return false;
    }
    PGresult* r = PQexec(conn_, "SELECT 1");
    const bool ok = r && PQresultStatus(r) == PGRES_TUPLES_OK;
    ClearRes(r);
    return ok;
}

bool PgTrackingStore::RecordOutboxEventLocked(const std::string& topic, const std::string& payload) {
    const char* pv[] = {topic.c_str(), payload.c_str()};
    PGresult* r = ExecParams("INSERT INTO tracking_event_outbox (topic, payload) VALUES ($1, $2)",
                             2, pv, nullptr, nullptr);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    return ok;
}

bool PgTrackingStore::InsertLink(const LinkRecord& link) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {
        link.link_ref.c_str(),
        link.short_token.c_str(),
        link.landing_url.c_str(),
        link.guide_card_id.c_str(),
        link.scene.c_str(),
        link.placement.c_str(),
    };
    PGresult* r = ExecParams(
        "INSERT INTO tracking_link (link_ref, short_token, landing_url, guide_card_id, scene, placement) "
        "VALUES ($1, $2, $3, $4, $5, $6)",
        6, pv, nullptr, nullptr);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    if (!ok && r) {
        LOG(ERROR) << "InsertLink: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    if (ok) {
        RecordOutboxEventLocked("tracking.link.created", link.link_ref);
    }
    return ok;
}

bool PgTrackingStore::LinkFromRow(PGresult* r, int row, LinkRecord* out) {
    out->link_ref = PQgetvalue(r, row, 0);
    out->short_token = PQgetvalue(r, row, 1);
    out->landing_url = PQgetvalue(r, row, 2);
    out->guide_card_id = PQgetvalue(r, row, 3);
    out->scene = PQgetvalue(r, row, 4);
    out->placement = PQgetvalue(r, row, 5);
    return true;
}

bool PgTrackingStore::GetLinkByToken(const std::string& token, LinkRecord* out) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {token.c_str()};
    PGresult* r = ExecParams("SELECT link_ref, short_token, landing_url, guide_card_id, scene, placement "
                             "FROM tracking_link WHERE short_token = $1 AND status = 1",
                             1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        ClearRes(r);
        return false;
    }
    const bool ok = LinkFromRow(r, 0, out);
    ClearRes(r);
    return ok;
}

bool PgTrackingStore::GetLinkByRef(const std::string& ref, LinkRecord* out) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {ref.c_str()};
    PGresult* r = ExecParams("SELECT link_ref, short_token, landing_url, guide_card_id, scene, placement "
                             "FROM tracking_link WHERE link_ref = $1 AND status = 1",
                             1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        ClearRes(r);
        return false;
    }
    const bool ok = LinkFromRow(r, 0, out);
    ClearRes(r);
    return ok;
}

bool PgTrackingStore::InsertClick(const std::string& click_id,
                                  const std::string& link_ref,
                                  const std::string& short_token) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {click_id.c_str(), link_ref.c_str(), short_token.c_str()};
    PGresult* r = ExecParams("INSERT INTO tracking_click (click_id, link_ref, short_token) VALUES ($1, $2, $3)",
                             3, pv, nullptr, nullptr);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    if (!ok && r) {
        LOG(ERROR) << "InsertClick: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    if (ok) {
        RecordOutboxEventLocked("tracking.click.acked", click_id);
    }
    return ok;
}

bool PgTrackingStore::InsertConversion(const IngestConversionRequest& req,
                                       const std::string& conversion_id,
                                       IngestConversionResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!req.external_event_id().empty()) {
        const char* check_pv[] = {req.external_event_id().c_str()};
        PGresult* check = ExecParams("SELECT conversion_id FROM tracking_conversion WHERE external_event_id = $1",
                                     1, check_pv, nullptr, nullptr);
        if (check && PQresultStatus(check) == PGRES_TUPLES_OK && PQntuples(check) > 0) {
            resp->set_status(CONVERSION_INGEST_STATUS_DUPLICATE);
            resp->set_match_confidence(MATCH_CONFIDENCE_UNMATCHED);
            ClearRes(check);
            return true;
        }
        ClearRes(check);
    }
    const std::string conversion_type = std::to_string(req.conversion_type());
    const std::string amount = req.has_amount() ? MoneyValue(req.amount()) : "0";
    const std::string commission = req.has_commission() ? CommissionValue(req.commission()) : "0";
    const char* pv[] = {
        conversion_id.c_str(),
        req.external_event_id().c_str(),
        req.click_id().c_str(),
        conversion_type.c_str(),
        amount.c_str(),
        commission.c_str(),
    };
    PGresult* r = ExecParams(
        "INSERT INTO tracking_conversion "
        "(conversion_id, external_event_id, click_id, conversion_type, amount_minor, commission_minor) "
        "VALUES ($1, NULLIF($2, ''), $3, $4::int, $5::bigint, $6::bigint)",
        6, pv, nullptr, nullptr);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    if (!ok && r) {
        LOG(ERROR) << "InsertConversion: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    if (!ok) {
        return false;
    }
    RecordOutboxEventLocked("tracking.conversion.ingested", conversion_id);
    resp->set_conversion_id(conversion_id);
    resp->set_matched_click_id(req.click_id());
    resp->set_status(CONVERSION_INGEST_STATUS_ACCEPTED);
    resp->set_match_confidence(req.click_id().empty() ? MATCH_CONFIDENCE_UNMATCHED : MATCH_CONFIDENCE_EXACT);
    return true;
}

bool PgTrackingStore::FillCommissionSummary(GetCommissionSummaryResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    PGresult* r = PQexec(conn_, "SELECT COALESCE(sum(commission_minor), 0), count(*) FROM tracking_conversion");
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        ClearRes(r);
        return false;
    }
    auto* b = resp->add_buckets();
    b->set_estimated_minor(std::atoll(PQgetvalue(r, 0, 0)));
    b->set_reported_minor(0);
    b->set_currency("CNY");
    b->set_order_count(std::atoi(PQgetvalue(r, 0, 1)));
    ClearRes(r);
    return true;
}

}  // namespace tracking_server
}  // namespace simple_living
