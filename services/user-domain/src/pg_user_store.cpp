#include "pg_user_store.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>

#include <postgresql/libpq-fe.h>

#include <butil/logging.h>

namespace simple_living {
namespace user_domain {

namespace {

std::atomic<int64_t> g_id_counter{1};

std::string GenId(const std::string& prefix) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return prefix + "_" + std::to_string(ts) + "_" + std::to_string(g_id_counter.fetch_add(1));
}

void ClearRes(PGresult* r) {
    if (r) {
        PQclear(r);
    }
}

}  // namespace

PgUserStore::~PgUserStore() {
    Close();
}

void PgUserStore::Close() {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool PgUserStore::ExecSql(const char* sql) {
    PGresult* r = PQexec(conn_, sql);
    const auto st = r ? PQresultStatus(r) : PGRES_FATAL_ERROR;
    const bool ok = (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
    if (!ok && r) {
        LOG(ERROR) << "PG exec: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    return ok;
}

PGresult* PgUserStore::ExecParams(const char* sql,
                                  int n_params,
                                  const char* const* param_values,
                                  const int* param_lengths,
                                  const int* param_formats) {
    return PQexecParams(conn_, sql, n_params, nullptr, param_values, param_lengths, param_formats, 0);
}

std::string PgUserStore::HexEncode(const std::string& raw) {
    static const char* d = "0123456789abcdef";
    std::string o;
    o.resize(raw.size() * 2);
    for (size_t i = 0; i < raw.size(); ++i) {
        o[i * 2] = d[(static_cast<unsigned char>(raw[i]) >> 4) & 15];
        o[i * 2 + 1] = d[static_cast<unsigned char>(raw[i]) & 15];
    }
    return o;
}

bool PgUserStore::HexDecode(const std::string& hex, std::string* out) {
    if (hex.size() % 2 != 0) {
        return false;
    }
    out->clear();
    out->reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = std::tolower(hex[i]);
        int lo = std::tolower(hex[i + 1]);
        auto val = [](int c) -> int {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f') {
                return 10 + c - 'a';
            }
            return -1;
        };
        int vh = val(hi);
        int vl = val(lo);
        if (vh < 0 || vl < 0) {
            return false;
        }
        out->push_back(static_cast<char>((vh << 4) | vl));
    }
    return true;
}

bool PgUserStore::ConnectAndInit(const std::string& conninfo) {
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
        "CREATE TABLE IF NOT EXISTS user_account ("
        "user_id TEXT PRIMARY KEY,"
        "is_guest BOOLEAN NOT NULL DEFAULT FALSE,"
        "display_name TEXT,"
        "avatar_url TEXT,"
        "locale TEXT,"
        "bio TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS user_session ("
        "session_id TEXT PRIMARY KEY,"
        "user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,"
        "access_token TEXT NOT NULL UNIQUE,"
        "refresh_token TEXT NOT NULL UNIQUE,"
        "is_guest BOOLEAN NOT NULL DEFAULT FALSE,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS guest_device ("
        "device_key TEXT PRIMARY KEY,"
        "session_id TEXT NOT NULL REFERENCES user_session (session_id) ON DELETE CASCADE,"
        "user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS user_preferences_blob ("
        "user_id TEXT PRIMARY KEY REFERENCES user_account (user_id) ON DELETE CASCADE,"
        "prefs_hex TEXT)",
        "CREATE TABLE IF NOT EXISTS user_favorite ("
        "favorite_id TEXT PRIMARY KEY,"
        "user_id TEXT NOT NULL REFERENCES user_account (user_id) ON DELETE CASCADE,"
        "guide_card_id TEXT NOT NULL,"
        "UNIQUE (user_id, guide_card_id))",
        "CREATE TABLE IF NOT EXISTS user_history ("
        "owner_key TEXT NOT NULL,"
        "guide_card_id TEXT NOT NULL,"
        "PRIMARY KEY (owner_key, guide_card_id))",
        "CREATE TABLE IF NOT EXISTS user_consent_blob ("
        "user_id TEXT PRIMARY KEY REFERENCES user_account (user_id) ON DELETE CASCADE,"
        "consent_hex TEXT)",
        "CREATE TABLE IF NOT EXISTS user_signal ("
        "user_key TEXT PRIMARY KEY,"
        "signal_ref TEXT NOT NULL,"
        "bundle_version TEXT NOT NULL DEFAULT 'v1')",
        "CREATE TABLE IF NOT EXISTS user_feedback ("
        "feedback_id TEXT PRIMARY KEY,"
        "actor_user_id TEXT,"
        "actor_session_id TEXT,"
        "target_type INT,"
        "target_id TEXT,"
        "client_request_id TEXT,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
    };
    for (auto* ddl : kDdl) {
        if (!ExecSql(ddl)) {
            LOG(ERROR) << "Schema init failed";
            PQfinish(conn_);
            conn_ = nullptr;
            return false;
        }
    }
    LOG(INFO) << "PostgreSQL connected and schema ready";
    return true;
}

bool PgUserStore::Ping() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
        return false;
    }
    PGresult* r = PQexec(conn_, "SELECT 1");
    const bool ok = r && PQresultStatus(r) == PGRES_TUPLES_OK;
    ClearRes(r);
    return ok;
}

void PgUserStore::IssueTokenPair(const IssueTokenPairRequest& req, IssueTokenPairResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string user_id;
    if (req.has_user_id()) {
        user_id = req.user_id();
    } else {
        user_id = GenId("usr");
    }
    const char* pv[] = {user_id.c_str()};
    PGresult* r = ExecParams("INSERT INTO user_account (user_id, is_guest) VALUES ($1, false) "
                           "ON CONFLICT (user_id) DO NOTHING",
                           1, pv, nullptr, nullptr);
    ClearRes(r);

    std::string sid = GenId("sid");
    std::string at = GenId("at");
    std::string rt = GenId("rt");
    const char* pv2[] = {sid.c_str(), user_id.c_str(), at.c_str(), rt.c_str()};
    r = ExecParams(
        "INSERT INTO user_session (session_id, user_id, access_token, refresh_token, is_guest) "
        "VALUES ($1, $2, $3, $4, false)",
        4, pv2, nullptr, nullptr);
    const bool ins_ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    if (!ins_ok && r) {
        LOG(ERROR) << "IssueTokenPair: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    if (ins_ok) {
        resp->set_access_token(at);
        resp->set_refresh_token(rt);
        resp->set_expires_in_seconds(3600);
        resp->set_session_id(sid);
    }
}

void PgUserStore::IntrospectAccessToken(const IntrospectAccessTokenRequest& req,
                                        IntrospectAccessTokenResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {req.access_token().c_str()};
    PGresult* r = ExecParams(
        "SELECT user_id, session_id FROM user_session WHERE access_token = $1", 1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        resp->set_valid(false);
        ClearRes(r);
        return;
    }
    resp->set_valid(true);
    resp->set_user_id(PQgetvalue(r, 0, 0));
    resp->set_session_id(PQgetvalue(r, 0, 1));
    ClearRes(r);
}

void PgUserStore::IntrospectRefreshToken(const IntrospectRefreshTokenRequest& req,
                                         IntrospectRefreshTokenResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {req.refresh_token().c_str()};
    PGresult* r = ExecParams(
        "SELECT user_id, session_id FROM user_session WHERE refresh_token = $1", 1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        resp->set_valid(false);
        ClearRes(r);
        return;
    }
    resp->set_valid(true);
    resp->set_user_id(PQgetvalue(r, 0, 0));
    resp->set_session_id(PQgetvalue(r, 0, 1));
    ClearRes(r);
}

void PgUserStore::RevokeSession(const RevokeSessionRequest& req, RevokeSessionResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    PGresult* r = nullptr;
    if (req.scope() == REVOKE_SESSION_SCOPE_ALL_USER_SESSIONS && req.has_user_id()) {
        const char* pv[] = {req.user_id().c_str()};
        r = ExecParams("DELETE FROM user_session WHERE user_id = $1", 1, pv, nullptr, nullptr);
    } else if (req.has_session_id()) {
        const char* pv[] = {req.session_id().c_str()};
        r = ExecParams("DELETE FROM user_session WHERE session_id = $1", 1, pv, nullptr, nullptr);
    } else {
        resp->set_revoked(false);
        return;
    }
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    char* n = r ? PQcmdTuples(r) : nullptr;
    int nrows = n ? std::atoi(n) : 0;
    ClearRes(r);
    resp->set_revoked(ok && nrows > 0);
}

void PgUserStore::EnsureGuestSession(const EnsureGuestSessionRequest& req, EnsureGuestSessionResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string device_key = std::string("guest_") + (req.has_device_id() ? req.device_id() : GenId("dev"));
    const char* pv[] = {device_key.c_str()};
    PGresult* r = ExecParams(
        "SELECT session_id FROM guest_device WHERE device_key = $1", 1, pv, nullptr, nullptr);
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        resp->set_session_id(PQgetvalue(r, 0, 0));
        resp->set_created(false);
        ClearRes(r);
        return;
    }
    ClearRes(r);

    std::string user_id = GenId("guest");
    const char* p1[] = {user_id.c_str()};
    r = ExecParams("INSERT INTO user_account (user_id, is_guest, display_name) VALUES ($1, true, 'Guest')", 1,
                   p1, nullptr, nullptr);
    ClearRes(r);

    std::string sid = GenId("gsid");
    std::string at = GenId("gat");
    std::string rt = GenId("grt");
    const char* p2[] = {sid.c_str(), user_id.c_str(), at.c_str(), rt.c_str()};
    r = ExecParams(
        "INSERT INTO user_session (session_id, user_id, access_token, refresh_token, is_guest) "
        "VALUES ($1, $2, $3, $4, true)",
        4, p2, nullptr, nullptr);
    ClearRes(r);

    const char* p3[] = {device_key.c_str(), sid.c_str(), user_id.c_str()};
    r = ExecParams("INSERT INTO guest_device (device_key, session_id, user_id) VALUES ($1, $2, $3)", 3, p3,
                   nullptr, nullptr);
    ClearRes(r);

    resp->set_session_id(sid);
    resp->set_created(true);
}

void PgUserStore::GetProfile(const GetProfileRequest& req, GetProfileResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    GetProfileUnlocked(req, resp);
}

void PgUserStore::GetProfileUnlocked(const GetProfileRequest& req, GetProfileResponse* resp) {
    PGresult* r = nullptr;
    if (req.has_user_id()) {
        const char* pv[] = {req.user_id().c_str()};
        r = ExecParams(
            "SELECT user_id, is_guest, display_name, avatar_url, locale, bio FROM user_account WHERE user_id = "
            "$1",
            1, pv, nullptr, nullptr);
    } else if (req.has_session_id()) {
        const char* pv[] = {req.session_id().c_str()};
        r = ExecParams(
            "SELECT a.user_id, a.is_guest, a.display_name, a.avatar_url, a.locale, a.bio "
            "FROM user_account a JOIN user_session s ON s.user_id = a.user_id WHERE s.session_id = $1",
            1, pv, nullptr, nullptr);
    } else {
        return;
    }
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        ClearRes(r);
        return;
    }
    auto* p = resp->mutable_profile();
    p->set_user_id(PQgetvalue(r, 0, 0));
    p->set_is_guest(PQgetvalue(r, 0, 1)[0] == 't');
    if (!PQgetisnull(r, 0, 2)) {
        p->set_display_name(PQgetvalue(r, 0, 2));
    }
    if (!PQgetisnull(r, 0, 3)) {
        p->set_avatar_url(PQgetvalue(r, 0, 3));
    }
    if (!PQgetisnull(r, 0, 4)) {
        p->set_locale(PQgetvalue(r, 0, 4));
    }
    if (!PQgetisnull(r, 0, 5)) {
        p->set_bio(PQgetvalue(r, 0, 5));
    }
    ClearRes(r);
}

void PgUserStore::UpdateProfile(const UpdateProfileRequest& req, UpdateProfileResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* dn = req.has_display_name() ? req.display_name().c_str() : nullptr;
    const char* av = req.has_avatar_url() ? req.avatar_url().c_str() : nullptr;
    const char* loc = req.has_locale() ? req.locale().c_str() : nullptr;
    const char* bio = req.has_bio() ? req.bio().c_str() : nullptr;
    const char* pv[] = {dn, av, loc, bio, req.user_id().c_str()};
    PGresult* r = ExecParams(
        "UPDATE user_account SET "
        "display_name = COALESCE($1, display_name), "
        "avatar_url = COALESCE($2, avatar_url), "
        "locale = COALESCE($3, locale), "
        "bio = COALESCE($4, bio) "
        "WHERE user_id = $5",
        5, pv, nullptr, nullptr);
    ClearRes(r);
    GetProfileRequest gr;
    gr.set_user_id(req.user_id());
    GetProfileResponse gpr;
    GetProfileUnlocked(gr, &gpr);
    if (gpr.has_profile()) {
        resp->mutable_profile()->CopyFrom(gpr.profile());
    }
}

void PgUserStore::GetPreferences(const GetPreferencesRequest& req, GetPreferencesResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    GetPreferencesUnlocked(req, resp);
}

void PgUserStore::GetPreferencesUnlocked(const GetPreferencesRequest& req, GetPreferencesResponse* resp) {
    const char* pv[] = {req.user_id().c_str()};
    PGresult* r =
        ExecParams("SELECT prefs_hex FROM user_preferences_blob WHERE user_id = $1", 1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1 || PQgetisnull(r, 0, 0)) {
        ClearRes(r);
        return;
    }
    std::string raw;
    if (!HexDecode(PQgetvalue(r, 0, 0), &raw)) {
        ClearRes(r);
        return;
    }
    ClearRes(r);
    resp->mutable_preferences()->ParseFromString(raw);
}

void PgUserStore::UpdatePreferences(const UpdatePreferencesRequest& req, UpdatePreferencesResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string blob;
    if (req.has_preferences()) {
        req.preferences().SerializeToString(&blob);
    }
    std::string hex = HexEncode(blob);
    const char* pv[] = {req.user_id().c_str(), hex.c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO user_preferences_blob (user_id, prefs_hex) VALUES ($1, $2) "
        "ON CONFLICT (user_id) DO UPDATE SET prefs_hex = EXCLUDED.prefs_hex",
        2, pv, nullptr, nullptr);
    ClearRes(r);
    GetPreferencesRequest gr;
    gr.set_user_id(req.user_id());
    GetPreferencesResponse gpr;
    GetPreferencesUnlocked(gr, &gpr);
    if (gpr.has_preferences()) {
        resp->mutable_preferences()->CopyFrom(gpr.preferences());
    }
}

void PgUserStore::ListFavorites(const ListFavoritesRequest& req, ListFavoritesResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {req.user_id().c_str()};
    PGresult* r = ExecParams(
        "SELECT favorite_id, guide_card_id FROM user_favorite WHERE user_id = $1 ORDER BY favorite_id", 1, pv,
        nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return;
    }
    int n = PQntuples(r);
    for (int i = 0; i < n; ++i) {
        auto* it = resp->add_items();
        it->set_favorite_id(PQgetvalue(r, i, 0));
        it->set_guide_card_id(PQgetvalue(r, i, 1));
    }
    ClearRes(r);
}

void PgUserStore::AddFavorite(const AddFavoriteRequest& req, AddFavoriteResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {req.user_id().c_str(), req.guide_card_id().c_str()};
    PGresult* r = ExecParams(
        "SELECT favorite_id FROM user_favorite WHERE user_id = $1 AND guide_card_id = $2", 2, pv, nullptr,
        nullptr);
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        resp->set_favorite_id(PQgetvalue(r, 0, 0));
        resp->set_already_favorited(true);
        ClearRes(r);
        return;
    }
    ClearRes(r);
    std::string fid = GenId("fav");
    const char* pv2[] = {fid.c_str(), req.user_id().c_str(), req.guide_card_id().c_str()};
    r = ExecParams(
        "INSERT INTO user_favorite (favorite_id, user_id, guide_card_id) VALUES ($1, $2, $3)", 3, pv2, nullptr,
        nullptr);
    ClearRes(r);
    resp->set_favorite_id(fid);
    resp->set_already_favorited(false);
}

void PgUserStore::RemoveFavorite(const RemoveFavoriteRequest& req, RemoveFavoriteResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    PGresult* r = nullptr;
    if (!req.favorite_id().empty()) {
        const char* pv[] = {req.user_id().c_str(), req.favorite_id().c_str()};
        r = ExecParams("DELETE FROM user_favorite WHERE user_id = $1 AND favorite_id = $2", 2, pv, nullptr,
                       nullptr);
    } else if (!req.guide_card_id().empty()) {
        const char* pv[] = {req.user_id().c_str(), req.guide_card_id().c_str()};
        r = ExecParams("DELETE FROM user_favorite WHERE user_id = $1 AND guide_card_id = $2", 2, pv, nullptr,
                       nullptr);
    } else {
        resp->set_removed(false);
        return;
    }
    char* nt = r ? PQcmdTuples(r) : nullptr;
    int nr = nt ? std::atoi(nt) : 0;
    ClearRes(r);
    resp->set_removed(nr > 0);
}

std::string PgUserStore::OwnerKeyFromListHistory(const ListHistoryRequest& req) {
    if (req.has_user_id()) {
        return std::string("u:") + req.user_id();
    }
    return std::string("s:") + req.session_id();
}

std::string PgUserStore::OwnerKeyFromHistoryReq(const RecordHistoryEventRequest& req) {
    if (req.has_user_id()) {
        return std::string("u:") + req.user_id();
    }
    return std::string("s:") + req.session_id();
}

std::string PgUserStore::OwnerKeyFromClearHistory(const ClearHistoryRequest& req) {
    if (req.has_user_id()) {
        return std::string("u:") + req.user_id();
    }
    return std::string("s:") + req.session_id();
}

void PgUserStore::ListHistory(const ListHistoryRequest& req, ListHistoryResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string ok = OwnerKeyFromListHistory(req);
    const char* pv[] = {ok.c_str()};
    PGresult* r =
        ExecParams("SELECT guide_card_id FROM user_history WHERE owner_key = $1", 1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return;
    }
    int n = PQntuples(r);
    for (int i = 0; i < n; ++i) {
        auto* it = resp->add_items();
        it->mutable_content_ref()->set_guide_card_id(PQgetvalue(r, i, 0));
    }
    ClearRes(r);
}

void PgUserStore::RecordHistoryEvent(const RecordHistoryEventRequest& req, RecordHistoryEventResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string ok = OwnerKeyFromHistoryReq(req);
    std::string gid = req.has_content_ref() ? req.content_ref().guide_card_id() : "";
    const char* pv[] = {ok.c_str(), gid.c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO user_history (owner_key, guide_card_id) VALUES ($1, $2) ON CONFLICT DO NOTHING", 2, pv,
        nullptr, nullptr);
    char* nt = r ? PQcmdTuples(r) : nullptr;
    int nr = nt ? std::atoi(nt) : 0;
    ClearRes(r);
    if (nr > 0) {
        resp->set_recorded(true);
        resp->set_deduplicated(false);
    } else {
        resp->set_recorded(false);
        resp->set_deduplicated(true);
    }
}

void PgUserStore::ClearHistory(const ClearHistoryRequest& req, ClearHistoryResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string ok = OwnerKeyFromClearHistory(req);
    const char* pv[] = {ok.c_str()};
    PGresult* r = ExecParams("DELETE FROM user_history WHERE owner_key = $1", 1, pv, nullptr, nullptr);
    char* nt = r ? PQcmdTuples(r) : nullptr;
    int nr = nt ? std::atoi(nt) : 0;
    ClearRes(r);
    resp->set_removed_count(nr);
}

void PgUserStore::SubmitFeedback(const SubmitFeedbackRequest& req, SubmitFeedbackResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string fid = GenId("fb");
    const char* au = req.has_user_id() ? req.user_id().c_str() : nullptr;
    const char* as = req.has_session_id() ? req.session_id().c_str() : nullptr;
    std::string tt = req.has_target_type() ? std::to_string(static_cast<int>(req.target_type())) : std::string();
    const char* ttp = req.has_target_type() ? tt.c_str() : nullptr;
    const char* tid = req.has_target_id() ? req.target_id().c_str() : nullptr;
    const char* cr = req.has_client_request_id() ? req.client_request_id().c_str() : nullptr;
    const char* pv[] = {fid.c_str(), au, as, ttp, tid, cr};
    PGresult* r = ExecParams(
        "INSERT INTO user_feedback (feedback_id, actor_user_id, actor_session_id, target_type, target_id, "
        "client_request_id) VALUES ($1, $2, $3, $4::int, $5, $6)",
        6, pv, nullptr, nullptr);
    ClearRes(r);
    resp->set_feedback_id(fid);
}

void PgUserStore::GetConsent(const GetConsentRequest& req, GetConsentResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    GetConsentUnlocked(req, resp);
}

void PgUserStore::GetConsentUnlocked(const GetConsentRequest& req, GetConsentResponse* resp) {
    const char* pv[] = {req.user_id().c_str()};
    PGresult* r =
        ExecParams("SELECT consent_hex FROM user_consent_blob WHERE user_id = $1", 1, pv, nullptr, nullptr);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1 || PQgetisnull(r, 0, 0)) {
        ClearRes(r);
        return;
    }
    std::string raw;
    if (!HexDecode(PQgetvalue(r, 0, 0), &raw)) {
        ClearRes(r);
        return;
    }
    ClearRes(r);
    resp->mutable_consent()->ParseFromString(raw);
}

void PgUserStore::UpdateConsent(const UpdateConsentRequest& req, UpdateConsentResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string blob;
    if (req.has_consent()) {
        req.consent().SerializeToString(&blob);
    }
    std::string hex = HexEncode(blob);
    const char* pv[] = {req.user_id().c_str(), hex.c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO user_consent_blob (user_id, consent_hex) VALUES ($1, $2) "
        "ON CONFLICT (user_id) DO UPDATE SET consent_hex = EXCLUDED.consent_hex",
        2, pv, nullptr, nullptr);
    ClearRes(r);
    GetConsentRequest gr;
    gr.set_user_id(req.user_id());
    GetConsentResponse gcr;
    GetConsentUnlocked(gr, &gcr);
    if (gcr.has_consent()) {
        resp->mutable_consent()->CopyFrom(gcr.consent());
    }
}

std::string PgUserStore::UserKeyForSignal(const GetSignalBundleRefRequest& req) {
    if (req.has_user_id()) {
        return std::string("u:") + req.user_id();
    }
    return std::string("s:") + req.session_id();
}

void PgUserStore::GetSignalBundleRef(const GetSignalBundleRefRequest& req, GetSignalBundleRefResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string uk = UserKeyForSignal(req);
    const char* pv[] = {uk.c_str()};
    PGresult* r = ExecParams("SELECT signal_ref, bundle_version FROM user_signal WHERE user_key = $1", 1, pv,
                             nullptr, nullptr);
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        resp->mutable_ref()->set_signal_bundle_ref(PQgetvalue(r, 0, 0));
        if (!PQgetisnull(r, 0, 1)) {
            resp->mutable_ref()->set_bundle_version(PQgetvalue(r, 0, 1));
        } else {
            resp->mutable_ref()->set_bundle_version("v1");
        }
        ClearRes(r);
        return;
    }
    ClearRes(r);

    std::string ref = GenId("sbr");
    const char* pv2[] = {uk.c_str(), ref.c_str(), "v1"};
    PGresult* r2 = ExecParams(
        "INSERT INTO user_signal (user_key, signal_ref, bundle_version) VALUES ($1, $2, $3) "
        "ON CONFLICT (user_key) DO NOTHING",
        3, pv2, nullptr, nullptr);
    ClearRes(r2);

    r = ExecParams("SELECT signal_ref, bundle_version FROM user_signal WHERE user_key = $1", 1, pv, nullptr,
                   nullptr);
    if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
        resp->mutable_ref()->set_signal_bundle_ref(PQgetvalue(r, 0, 0));
        if (!PQgetisnull(r, 0, 1)) {
            resp->mutable_ref()->set_bundle_version(PQgetvalue(r, 0, 1));
        } else {
            resp->mutable_ref()->set_bundle_version("v1");
        }
    } else {
        resp->mutable_ref()->set_signal_bundle_ref(ref);
        resp->mutable_ref()->set_bundle_version("v1");
    }
    ClearRes(r);
}

void PgUserStore::GetMeSummary(const GetMeSummaryRequest& req, GetMeSummaryResponse* resp) {
    std::string uid;
    if (req.has_user_id()) {
        uid = req.user_id();
    } else {
        std::lock_guard<std::mutex> lock(mu_);
        const char* pv[] = {req.session_id().c_str()};
        PGresult* r =
            ExecParams("SELECT user_id FROM user_session WHERE session_id = $1", 1, pv, nullptr, nullptr);
        if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
            uid = PQgetvalue(r, 0, 0);
        }
        ClearRes(r);
    }
    if (uid.empty()) {
        return;
    }
    GetProfileRequest gpr;
    gpr.set_user_id(uid);
    GetProfileResponse gpr_out;
    GetProfile(gpr, &gpr_out);
    if (gpr_out.has_profile()) {
        resp->mutable_profile()->CopyFrom(gpr_out.profile());
    }
    {
        std::lock_guard<std::mutex> lock(mu_);
        std::string uk = std::string("u:") + uid;
        const char* pv[] = {uid.c_str()};
        PGresult* r =
            ExecParams("SELECT COUNT(*) FROM user_favorite WHERE user_id = $1", 1, pv, nullptr, nullptr);
        int64_t fc = 0;
        if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
            fc = std::strtoll(PQgetvalue(r, 0, 0), nullptr, 10);
        }
        ClearRes(r);
        const char* pv2[] = {uk.c_str()};
        r = ExecParams("SELECT COUNT(*) FROM user_history WHERE owner_key = $1", 1, pv2, nullptr, nullptr);
        int64_t hc = 0;
        if (r && PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
            hc = std::strtoll(PQgetvalue(r, 0, 0), nullptr, 10);
        }
        ClearRes(r);
        resp->mutable_counts()->set_favorites_count(fc);
        resp->mutable_counts()->set_history_count(hc);
    }
    GetConsentRequest gcr;
    gcr.set_user_id(uid);
    GetConsentResponse gcr_out;
    GetConsent(gcr, &gcr_out);
    if (gcr_out.has_consent()) {
        resp->mutable_consent()->CopyFrom(gcr_out.consent());
    }
}

}  // namespace user_domain
}  // namespace simple_living
