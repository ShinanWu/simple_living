#include "pg_affiliate_store.h"

#include <atomic>
#include <chrono>
#include <cctype>

#include <butil/logging.h>

namespace simple_living {
namespace affiliate_server {

namespace {

std::atomic<int64_t> g_id_counter{1};

void ClearRes(PGresult* r) {
    if (r) {
        PQclear(r);
    }
}

}  // namespace

PgAffiliateStore::~PgAffiliateStore() {
    Close();
}

void PgAffiliateStore::Close() {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

std::string PgAffiliateStore::GenId(const std::string& prefix) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return prefix + "_" + std::to_string(ts) + "_" + std::to_string(g_id_counter.fetch_add(1));
}

bool PgAffiliateStore::ExecSql(const char* sql) {
    PGresult* r = PQexec(conn_, sql);
    const auto st = r ? PQresultStatus(r) : PGRES_FATAL_ERROR;
    const bool ok = (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
    if (!ok && r) {
        LOG(ERROR) << "PG exec: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    return ok;
}

PGresult* PgAffiliateStore::ExecParams(const char* sql, int n_params, const char* const* param_values) {
    return PQexecParams(conn_, sql, n_params, nullptr, param_values, nullptr, nullptr, 0);
}

std::string PgAffiliateStore::HexEncode(const std::string& raw) {
    static const char* d = "0123456789abcdef";
    std::string o;
    o.resize(raw.size() * 2);
    for (size_t i = 0; i < raw.size(); ++i) {
        const auto c = static_cast<unsigned char>(raw[i]);
        o[i * 2] = d[(c >> 4) & 15];
        o[i * 2 + 1] = d[c & 15];
    }
    return o;
}

bool PgAffiliateStore::HexDecode(const std::string& hex, std::string* out) {
    if (hex.size() % 2 != 0) {
        return false;
    }
    out->clear();
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto val = [](int c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + c - 'a';
            return -1;
        };
        const int hi = val(std::tolower(hex[i]));
        const int lo = val(std::tolower(hex[i + 1]));
        if (hi < 0 || lo < 0) {
            return false;
        }
        out->push_back(static_cast<char>((hi << 4) | lo));
    }
    return true;
}

bool PgAffiliateStore::ConnectAndInit(const std::string& conninfo) {
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
        "CREATE TABLE IF NOT EXISTS affiliate_partner ("
        "  partner_id TEXT PRIMARY KEY,"
        "  proto_hex TEXT NOT NULL,"
        "  updated_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS affiliate_commission_rule ("
        "  partner_id TEXT PRIMARY KEY,"
        "  rule_set_id TEXT NOT NULL,"
        "  version INT NOT NULL,"
        "  payload_json TEXT,"
        "  updated_at TIMESTAMPTZ NOT NULL DEFAULT now())",
    };
    for (auto* ddl : kDdl) {
        if (!ExecSql(ddl)) {
            PQfinish(conn_);
            conn_ = nullptr;
            return false;
        }
    }
    LOG(INFO) << "platform/backoffice-backend PostgreSQL schema ready";
    return true;
}

bool PgAffiliateStore::Ping() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
        return false;
    }
    PGresult* r = PQexec(conn_, "SELECT 1");
    const bool ok = r && PQresultStatus(r) == PGRES_TUPLES_OK;
    ClearRes(r);
    return ok;
}

bool PgAffiliateStore::UpsertPartner(const PartnerCapabilitySnapshot& snapshot) {
    std::string raw;
    snapshot.SerializeToString(&raw);
    const std::string hex = HexEncode(raw);
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {snapshot.partner_id().c_str(), hex.c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO affiliate_partner (partner_id, proto_hex) VALUES ($1, $2) "
        "ON CONFLICT (partner_id) DO UPDATE SET proto_hex = EXCLUDED.proto_hex, updated_at = now()",
        2, pv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    return ok;
}

bool PgAffiliateStore::ListPartners(std::vector<PartnerCapabilitySnapshot>* out) {
    out->clear();
    std::lock_guard<std::mutex> lock(mu_);
    PGresult* r = PQexec(conn_, "SELECT partner_id, proto_hex FROM affiliate_partner ORDER BY partner_id");
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return false;
    }
    for (int i = 0; i < PQntuples(r); ++i) {
        PartnerCapabilitySnapshot snap;
        std::string raw;
        if (HexDecode(PQgetvalue(r, i, 1), &raw) && snap.ParseFromString(raw)) {
            if (snap.partner_id().empty()) {
                snap.set_partner_id(PQgetvalue(r, i, 0));
            }
            out->push_back(snap);
        }
    }
    ClearRes(r);
    return true;
}

bool PgAffiliateStore::EnsureSeedPartners() {
    for (const char* pid : {"partner_taobao", "partner_jd", "partner_pinduoduo", "tmall"}) {
        PartnerCapabilitySnapshot s;
        s.set_partner_id(pid);
        s.set_capability_set_id(GenId("cs"));
        s.set_lifecycle_status(PARTNER_LIFECYCLE_STATUS_ACTIVE);
        s.add_flags(CAPABILITY_FLAG_DEEP_LINK);
        s.add_flags(CAPABILITY_FLAG_SUB_ID_SLOTS);
        s.mutable_link_constraints()->set_max_url_length(2048);
        UpsertPartner(s);
    }
    return true;
}

bool PgAffiliateStore::GetPartnerCapabilitySnapshot(const std::string& partner_id,
                                                    PartnerCapabilitySnapshot* out) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {partner_id.c_str()};
    PGresult* r = ExecParams("SELECT proto_hex FROM affiliate_partner WHERE partner_id = $1", 1, pv);
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        std::string raw;
        if (HexDecode(PQgetvalue(r, 0, 0), &raw) && out->ParseFromString(raw)) {
            ClearRes(r);
            return true;
        }
    }
    ClearRes(r);
    out->set_partner_id(partner_id);
    out->set_capability_set_id(GenId("cs"));
    out->set_lifecycle_status(PARTNER_LIFECYCLE_STATUS_ACTIVE);
    out->add_flags(CAPABILITY_FLAG_DEEP_LINK);
    UpsertPartner(*out);
    return true;
}

bool PgAffiliateStore::CreateCommissionRuleSetVersion(const std::string& partner_id,
                                                      std::string* rule_set_id,
                                                      int32_t* version) {
    std::lock_guard<std::mutex> lock(mu_);
    int32_t next = 1;
    const char* pv[] = {partner_id.c_str()};
    PGresult* r = ExecParams("SELECT version FROM affiliate_commission_rule WHERE partner_id = $1", 1, pv);
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        next = std::atoi(PQgetvalue(r, 0, 0)) + 1;
    }
    ClearRes(r);
    const std::string rsid = GenId("rs");
    const std::string ver = std::to_string(next);
    const char* iv[] = {partner_id.c_str(), rsid.c_str(), ver.c_str(), "{}"};
    r = ExecParams(
        "INSERT INTO affiliate_commission_rule (partner_id, rule_set_id, version, payload_json) "
        "VALUES ($1, $2, $3::int, $4) ON CONFLICT (partner_id) DO UPDATE SET "
        "rule_set_id = EXCLUDED.rule_set_id, version = EXCLUDED.version, payload_json = EXCLUDED.payload_json, "
        "updated_at = now()",
        4, iv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    if (ok) {
        *rule_set_id = rsid;
        *version = next;
    }
    return ok;
}

bool PgAffiliateStore::GetCommissionRuleSet(const std::string& partner_id,
                                            std::string* rule_set_id,
                                            int32_t* version) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        const char* pv[] = {partner_id.c_str()};
        PGresult* r = ExecParams(
            "SELECT rule_set_id, version FROM affiliate_commission_rule WHERE partner_id = $1", 1, pv);
        if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
            *rule_set_id = PQgetvalue(r, 0, 0);
            *version = std::atoi(PQgetvalue(r, 0, 1));
            ClearRes(r);
            return true;
        }
        ClearRes(r);
    }
    return CreateCommissionRuleSetVersion(partner_id, rule_set_id, version);
}

}  // namespace affiliate_server
}  // namespace simple_living
