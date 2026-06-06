#include "pg_content_store.h"

#include <cctype>
#include <cstdlib>

#include <butil/logging.h>

namespace simple_living {
namespace content_server {

using catalog::CONTENT_LIFECYCLE_STATUS_PUBLISHED;

namespace {

void ClearRes(PGresult* r) {
    if (r) {
        PQclear(r);
    }
}

}  // namespace

PgContentStore::~PgContentStore() {
    Close();
}

void PgContentStore::Close() {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool PgContentStore::ExecSql(const char* sql) {
    PGresult* r = PQexec(conn_, sql);
    const auto st = r ? PQresultStatus(r) : PGRES_FATAL_ERROR;
    const bool ok = (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
    if (!ok && r) {
        LOG(ERROR) << "PG exec: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    return ok;
}

PGresult* PgContentStore::ExecParams(const char* sql,
                                     int n_params,
                                     const char* const* param_values,
                                     const int* param_lengths,
                                     const int* param_formats) {
    return PQexecParams(conn_, sql, n_params, nullptr, param_values, param_lengths, param_formats, 0);
}

std::string PgContentStore::HexEncode(const std::string& raw) {
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

bool PgContentStore::HexDecode(const std::string& hex, std::string* out) {
    if (hex.size() % 2 != 0) {
        return false;
    }
    out->clear();
    out->reserve(hex.size() / 2);
    auto val = [](int c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int hi = val(std::tolower(hex[i]));
        const int lo = val(std::tolower(hex[i + 1]));
        if (hi < 0 || lo < 0) {
            return false;
        }
        out->push_back(static_cast<char>((hi << 4) | lo));
    }
    return true;
}

std::string PgContentStore::FirstSellingPoint(const GuideCard& card) {
    return card.selling_points_size() > 0 ? card.selling_points(0) : "";
}

std::string PgContentStore::FirstThemeId(const GuideCard& card) {
    return card.theme_ids_size() > 0 ? card.theme_ids(0) : "";
}

std::string PgContentStore::FirstAffiliateChannel(const GuideCard& card) {
    return card.affiliate_refs_size() > 0 ? card.affiliate_refs(0).channel() : "";
}

std::string PgContentStore::FirstAffiliateExternalItemId(const GuideCard& card) {
    return card.affiliate_refs_size() > 0 ? card.affiliate_refs(0).external_item_id() : "";
}

std::string PgContentStore::FirstAffiliateLandingUrl(const GuideCard& card) {
    if (card.affiliate_refs_size() == 0) {
        return "";
    }
    const auto& payload = card.affiliate_refs(0).payload();
    const auto it = payload.find("landing_url");
    return it == payload.end() ? "" : it->second;
}

bool PgContentStore::CardFromRow(PGresult* r, int row, GuideCard* card) {
    std::string raw;
    if (!HexDecode(PQgetvalue(r, row, 0), &raw)) {
        return false;
    }
    return card->ParseFromString(raw);
}

bool PgContentStore::ConnectAndInit(const std::string& conninfo) {
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
        "CREATE TABLE IF NOT EXISTS content_guide_card ("
        "card_id TEXT PRIMARY KEY,"
        "title TEXT NOT NULL,"
        "subtitle TEXT,"
        "summary TEXT,"
        "theme_id TEXT,"
        "status INT NOT NULL,"
        "revision BIGINT NOT NULL DEFAULT 1,"
        "published_revision BIGINT NOT NULL DEFAULT 0,"
        "price_hint TEXT,"
        "affiliate_channel TEXT,"
        "external_item_id TEXT,"
        "landing_url TEXT,"
        "commercial_disclosure_required BOOLEAN NOT NULL DEFAULT FALSE,"
        "proto_hex TEXT NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE INDEX IF NOT EXISTS idx_content_guide_card_theme_status "
        "ON content_guide_card (theme_id, status, updated_at DESC)",
        "CREATE TABLE IF NOT EXISTS content_publish_audit ("
        "audit_id BIGSERIAL PRIMARY KEY,"
        "resource_id TEXT NOT NULL,"
        "resource_kind TEXT NOT NULL,"
        "from_status INT,"
        "to_status INT NOT NULL,"
        "revision BIGINT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
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

bool PgContentStore::Ping() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
        return false;
    }
    PGresult* r = PQexec(conn_, "SELECT 1");
    const bool ok = r && PQresultStatus(r) == PGRES_TUPLES_OK;
    ClearRes(r);
    return ok;
}

int PgContentStore::CountGuideCardsLocked() {
    PGresult* r = PQexec(conn_, "SELECT count(*) FROM content_guide_card");
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        ClearRes(r);
        return -1;
    }
    const int count = std::atoi(PQgetvalue(r, 0, 0));
    ClearRes(r);
    return count;
}

bool PgContentStore::EnsureSeedGuideCards(const std::vector<GuideCard>& seeds) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!conn_) {
        return false;
    }
    const int count = CountGuideCardsLocked();
    if (count < 0) {
        return false;
    }
    if (count > 0) {
        return true;
    }
    for (const auto& seed : seeds) {
        if (!UpsertGuideCardLocked(seed)) {
            return false;
        }
    }
    return true;
}

bool PgContentStore::UpsertGuideCardLocked(const GuideCard& card) {
    std::string raw;
    if (!card.SerializeToString(&raw)) {
        return false;
    }
    const std::string proto_hex = HexEncode(raw);
    const std::string summary = FirstSellingPoint(card);
    const std::string theme_id = FirstThemeId(card);
    const std::string affiliate_channel = FirstAffiliateChannel(card);
    const std::string external_item_id = FirstAffiliateExternalItemId(card);
    const std::string landing_url = FirstAffiliateLandingUrl(card);
    const std::string status = std::to_string(card.content_status());
    const std::string revision = std::to_string(card.revision());
    const std::string published_revision = std::to_string(card.published_revision());
    const std::string commercial = card.commercial_disclosure_required() ? "true" : "false";
    const char* pv[] = {
        card.card_id().c_str(),
        card.title().c_str(),
        card.subtitle().c_str(),
        summary.c_str(),
        theme_id.c_str(),
        status.c_str(),
        revision.c_str(),
        published_revision.c_str(),
        card.price_hint().c_str(),
        affiliate_channel.c_str(),
        external_item_id.c_str(),
        landing_url.c_str(),
        commercial.c_str(),
        proto_hex.c_str(),
    };
    PGresult* r = ExecParams(
        "INSERT INTO content_guide_card "
        "(card_id,title,subtitle,summary,theme_id,status,revision,published_revision,price_hint,"
        "affiliate_channel,external_item_id,landing_url,commercial_disclosure_required,proto_hex) "
        "VALUES ($1,$2,$3,$4,$5,$6::int,$7::bigint,$8::bigint,$9,$10,$11,$12,$13::boolean,$14) "
        "ON CONFLICT (card_id) DO UPDATE SET "
        "title=EXCLUDED.title, subtitle=EXCLUDED.subtitle, summary=EXCLUDED.summary, "
        "theme_id=EXCLUDED.theme_id, status=EXCLUDED.status, revision=EXCLUDED.revision, "
        "published_revision=EXCLUDED.published_revision, price_hint=EXCLUDED.price_hint, "
        "affiliate_channel=EXCLUDED.affiliate_channel, external_item_id=EXCLUDED.external_item_id, "
        "landing_url=EXCLUDED.landing_url, commercial_disclosure_required=EXCLUDED.commercial_disclosure_required, "
        "proto_hex=EXCLUDED.proto_hex, updated_at=now()",
        14, pv, nullptr, nullptr);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    if (!ok && r) {
        LOG(ERROR) << "UpsertGuideCard: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    return ok;
}

bool PgContentStore::UpsertGuideCard(GuideCard* card) {
    std::lock_guard<std::mutex> lock(mu_);
    return conn_ && UpsertGuideCardLocked(*card);
}

bool PgContentStore::LoadGuideCardLocked(const std::string& card_id, GuideCard* card) {
    const char* pv[] = {card_id.c_str()};
    PGresult* r = ExecParams("SELECT proto_hex FROM content_guide_card WHERE card_id = $1", 1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        ClearRes(r);
        return false;
    }
    const bool ok = CardFromRow(r, 0, card);
    ClearRes(r);
    return ok;
}

bool PgContentStore::BatchGetGuideCards(const std::vector<std::string>& card_ids,
                                        std::vector<GuideCard>* cards,
                                        std::vector<std::string>* missing_ids) {
    std::lock_guard<std::mutex> lock(mu_);
    cards->clear();
    missing_ids->clear();
    for (const auto& id : card_ids) {
        GuideCard card;
        if (LoadGuideCardLocked(id, &card)) {
            cards->push_back(card);
        } else {
            missing_ids->push_back(id);
        }
    }
    return true;
}

bool PgContentStore::ListGuideCards(const std::string& theme_id,
                                    int limit,
                                    std::vector<GuideCard>* cards,
                                    bool* has_more) {
    std::lock_guard<std::mutex> lock(mu_);
    cards->clear();
    *has_more = false;
    const std::string limit_str = std::to_string(limit + 1);
    PGresult* r = nullptr;
    if (theme_id.empty()) {
        const char* pv[] = {limit_str.c_str()};
        r = ExecParams("SELECT proto_hex FROM content_guide_card ORDER BY updated_at DESC LIMIT $1::int",
                       1, pv, nullptr, nullptr);
    } else {
        const char* pv[] = {theme_id.c_str(), limit_str.c_str()};
        r = ExecParams("SELECT proto_hex FROM content_guide_card WHERE theme_id = $1 "
                       "ORDER BY updated_at DESC LIMIT $2::int",
                       2, pv, nullptr, nullptr);
    }
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        if (r) {
            LOG(ERROR) << "ListGuideCards: " << PQresultErrorMessage(r);
        }
        ClearRes(r);
        return false;
    }
    for (int i = 0; i < PQntuples(r); ++i) {
        if (static_cast<int>(cards->size()) >= limit) {
            *has_more = true;
            break;
        }
        GuideCard card;
        if (CardFromRow(r, i, &card)) {
            cards->push_back(card);
        }
    }
    ClearRes(r);
    return true;
}

bool PgContentStore::ListPublishedGuideCards(std::vector<GuideCard>* cards) {
    std::lock_guard<std::mutex> lock(mu_);
    cards->clear();
    const std::string published_status =
        std::to_string(static_cast<int>(CONTENT_LIFECYCLE_STATUS_PUBLISHED));
    const char* pv[] = {published_status.c_str()};
    PGresult* r = ExecParams(
        "SELECT proto_hex FROM content_guide_card WHERE status = $1::int ORDER BY updated_at DESC",
        1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        if (r) {
            LOG(ERROR) << "ListPublishedGuideCards: " << PQresultErrorMessage(r);
        }
        ClearRes(r);
        return false;
    }
    for (int i = 0; i < PQntuples(r); ++i) {
        GuideCard card;
        if (CardFromRow(r, i, &card)) {
            cards->push_back(card);
        }
    }
    ClearRes(r);
    return true;
}

bool PgContentStore::UpdateGuideCardStatus(const std::string& card_id,
                                           ContentLifecycleStatus status,
                                           int64_t published_revision,
                                           GuideCard* updated) {
    std::lock_guard<std::mutex> lock(mu_);
    GuideCard card;
    if (!LoadGuideCardLocked(card_id, &card)) {
        return false;
    }
    const auto from_status = card.content_status();
    card.set_content_status(status);
    if (published_revision > 0) {
        card.set_published_revision(published_revision);
    }
    if (!UpsertGuideCardLocked(card)) {
        return false;
    }
    const std::string from_status_str = std::to_string(from_status);
    const std::string to_status_str = std::to_string(status);
    const std::string revision_str = std::to_string(card.revision());
    const char* pv[] = {card_id.c_str(), from_status_str.c_str(), to_status_str.c_str(), revision_str.c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO content_publish_audit (resource_id, resource_kind, from_status, to_status, revision) "
        "VALUES ($1, 'guide_card', $2::int, $3::int, $4::bigint)",
        4, pv, nullptr, nullptr);
    ClearRes(r);
    if (updated) {
        *updated = card;
    }
    return true;
}

}  // namespace content_server
}  // namespace simple_living
