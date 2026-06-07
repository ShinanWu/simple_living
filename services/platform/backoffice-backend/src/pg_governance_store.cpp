#include "pg_governance_store.h"

#include <atomic>
#include <chrono>
#include <cctype>

#include <butil/logging.h>

namespace simple_living {
namespace governance_server {

namespace {

std::atomic<int64_t> g_id_counter{1};

void ClearRes(PGresult* r) {
    if (r) {
        PQclear(r);
    }
}

}  // namespace

PgGovernanceStore::~PgGovernanceStore() {
    Close();
}

void PgGovernanceStore::Close() {
    std::lock_guard<std::mutex> lock(mu_);
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

std::string PgGovernanceStore::GenId(const std::string& prefix) {
    auto ts = std::chrono::system_clock::now().time_since_epoch().count();
    return prefix + "_" + std::to_string(ts) + "_" + std::to_string(g_id_counter.fetch_add(1));
}

bool PgGovernanceStore::ExecSql(const char* sql) {
    PGresult* r = PQexec(conn_, sql);
    const auto st = r ? PQresultStatus(r) : PGRES_FATAL_ERROR;
    const bool ok = (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
    if (!ok && r) {
        LOG(ERROR) << "PG exec: " << PQresultErrorMessage(r);
    }
    ClearRes(r);
    return ok;
}

PGresult* PgGovernanceStore::ExecParams(const char* sql,
                                        int n_params,
                                        const char* const* param_values,
                                        const int* param_lengths,
                                        const int* param_formats) {
    return PQexecParams(conn_, sql, n_params, nullptr, param_values, param_lengths, param_formats, 0);
}

bool PgGovernanceStore::RecordOutboxEventLocked(const std::string& topic, const std::string& payload) {
    const char* pv[] = {topic.c_str(), payload.c_str()};
    PGresult* r = ExecParams("INSERT INTO governance_event_outbox (topic, payload) VALUES ($1, $2)",
                             2, pv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    return ok;
}

bool PgGovernanceStore::ConnectAndInit(const std::string& conninfo) {
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
        "CREATE TABLE IF NOT EXISTS governance_review_queue ("
        "  id TEXT PRIMARY KEY,"
        "  content_id TEXT NOT NULL,"
        "  content_version TEXT,"
        "  status INT NOT NULL DEFAULT 1,"
        "  priority INT NOT NULL DEFAULT 0,"
        "  enqueue_reason TEXT,"
        "  created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS governance_review_decision ("
        "  id TEXT PRIMARY KEY,"
        "  queue_item_id TEXT NOT NULL,"
        "  content_id TEXT NOT NULL,"
        "  outcome INT NOT NULL,"
        "  comment TEXT,"
        "  reviewer_id TEXT,"
        "  created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS governance_visibility_verdict ("
        "  content_id TEXT PRIMARY KEY,"
        "  verdict_id TEXT NOT NULL,"
        "  state INT NOT NULL,"
        "  reason_code TEXT,"
        "  source INT NOT NULL DEFAULT 4,"
        "  version INT NOT NULL DEFAULT 1,"
        "  updated_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS governance_cooperation_label ("
        "  id TEXT PRIMARY KEY,"
        "  subject_type INT NOT NULL,"
        "  subject_id TEXT NOT NULL,"
        "  label_key TEXT,"
        "  proto_hex TEXT NOT NULL)",
        "CREATE TABLE IF NOT EXISTS governance_disclosure_template ("
        "  id TEXT PRIMARY KEY,"
        "  locale TEXT,"
        "  template_key TEXT,"
        "  body TEXT,"
        "  version INT NOT NULL DEFAULT 1)",
        "CREATE TABLE IF NOT EXISTS governance_risk_flag ("
        "  id TEXT PRIMARY KEY,"
        "  subject_type INT NOT NULL,"
        "  subject_id TEXT NOT NULL,"
        "  flag_code TEXT,"
        "  severity INT NOT NULL,"
        "  source TEXT,"
        "  active BOOLEAN NOT NULL DEFAULT TRUE,"
        "  created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
        "CREATE TABLE IF NOT EXISTS governance_event_outbox ("
        "  event_id BIGSERIAL PRIMARY KEY,"
        "  topic TEXT NOT NULL,"
        "  payload TEXT NOT NULL,"
        "  published BOOLEAN NOT NULL DEFAULT FALSE,"
        "  created_at TIMESTAMPTZ NOT NULL DEFAULT now())",
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

bool PgGovernanceStore::Ping() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
        return false;
    }
    PGresult* r = PQexec(conn_, "SELECT 1");
    const bool ok = r && PQresultStatus(r) == PGRES_TUPLES_OK;
    ClearRes(r);
    return ok;
}

bool PgGovernanceStore::EnsurePublishedVisibilityFromContent() {
    std::lock_guard<std::mutex> lock(mu_);
    const char* sql =
        "INSERT INTO governance_visibility_verdict (content_id, verdict_id, state, reason_code, source, version) "
        "SELECT card_id, 'vv_seed_' || card_id, 1, 'seed_sync', 4, 1 FROM content_guide_card WHERE status = 3 "
        "ON CONFLICT (content_id) DO NOTHING";
    PGresult* r = PQexec(conn_, sql);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    return ok;
}

bool PgGovernanceStore::EnsureSeedDisclosureTemplates() {
    std::lock_guard<std::mutex> lock(mu_);
    PGresult* r = PQexec(conn_, "SELECT COUNT(*) FROM governance_disclosure_template");
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        ClearRes(r);
        return false;
    }
    const int count = std::atoi(PQgetvalue(r, 0, 0));
    ClearRes(r);
    if (count > 0) {
        return true;
    }
    const char* pv[] = {"dt_cn_1", "zh-CN", "commercial_basic", "本内容含有商业合作推广", "1"};
    r = ExecParams("INSERT INTO governance_disclosure_template (id, locale, template_key, body, version) "
                   "VALUES ($1, $2, $3, $4, $5)",
                   5, pv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    return ok;
}

bool PgGovernanceStore::EnqueueReview(const EnqueueReviewRequest& req, EnqueueReviewResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const std::string id = GenId("qi");
    const std::string status = std::to_string(REVIEW_QUEUE_ITEM_STATUS_PENDING);
    const std::string priority = std::to_string(req.priority());
    const char* pv[] = {id.c_str(), req.content_id().c_str(), req.content_version().c_str(), status.c_str(),
                        priority.c_str(), req.enqueue_reason().c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO governance_review_queue (id, content_id, content_version, status, priority, enqueue_reason) "
        "VALUES ($1, $2, $3, $4::int, $5::int, $6)",
        6, pv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    if (!ok) {
        return false;
    }
    auto* item = resp->mutable_queue_item();
    item->set_id(id);
    item->set_content_id(req.content_id());
    item->set_content_version(req.content_version());
    item->set_status(REVIEW_QUEUE_ITEM_STATUS_PENDING);
    item->set_priority(req.priority());
    item->set_enqueue_reason(req.enqueue_reason());
    return true;
}

bool PgGovernanceStore::ListReviewQueueItems(const ListReviewQueueItemsRequest& req,
                                             ListReviewQueueItemsResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    PGresult* r = nullptr;
    if (req.has_status()) {
        const std::string st = std::to_string(req.status());
        const char* pv[] = {st.c_str()};
        r = ExecParams("SELECT id, content_id, content_version, status, priority, enqueue_reason "
                       "FROM governance_review_queue WHERE status = $1::int ORDER BY created_at DESC LIMIT 100",
                       1, pv);
    } else {
        r = PQexec(conn_, "SELECT id, content_id, content_version, status, priority, enqueue_reason "
                           "FROM governance_review_queue ORDER BY created_at DESC LIMIT 100");
    }
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return false;
    }
    for (int i = 0; i < PQntuples(r); ++i) {
        auto* item = resp->add_items();
        item->set_id(PQgetvalue(r, i, 0));
        item->set_content_id(PQgetvalue(r, i, 1));
        item->set_content_version(PQgetvalue(r, i, 2));
        item->set_status(static_cast<ReviewQueueItemStatus>(std::atoi(PQgetvalue(r, i, 3))));
        item->set_priority(std::atoi(PQgetvalue(r, i, 4)));
        item->set_enqueue_reason(PQgetvalue(r, i, 5));
    }
    ClearRes(r);
    return true;
}

bool PgGovernanceStore::SubmitReviewDecision(const SubmitReviewDecisionRequest& req,
                                             SubmitReviewDecisionResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    std::string content_id;
    {
        const char* pv[] = {req.queue_item_id().c_str()};
        PGresult* qr = ExecParams("SELECT content_id FROM governance_review_queue WHERE id = $1", 1, pv);
        if (qr && PQresultStatus(qr) == PGRES_TUPLES_OK && PQntuples(qr) > 0) {
            content_id = PQgetvalue(qr, 0, 0);
        }
        ClearRes(qr);
    }
    const std::string did = GenId("rd");
    const std::string outcome = std::to_string(req.outcome());
    const char* pv[] = {did.c_str(), req.queue_item_id().c_str(), content_id.c_str(), outcome.c_str(),
                        req.comment().c_str(), req.reviewer_id().c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO governance_review_decision (id, queue_item_id, content_id, outcome, comment, reviewer_id) "
        "VALUES ($1, $2, $3, $4::int, $5, $6)",
        6, pv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    if (!ok) {
        return false;
    }
    const std::string completed = std::to_string(REVIEW_QUEUE_ITEM_STATUS_COMPLETED);
    const char* uv[] = {completed.c_str(), req.queue_item_id().c_str()};
    r = ExecParams("UPDATE governance_review_queue SET status = $1::int WHERE id = $2", 2, uv);
    ClearRes(r);

    auto* d = resp->mutable_decision();
    d->set_id(did);
    d->set_queue_item_id(req.queue_item_id());
    d->set_content_id(content_id);
    d->set_outcome(req.outcome());
    d->set_comment(req.comment());
    d->set_reviewer_id(req.reviewer_id());
    return true;
}

bool PgGovernanceStore::ListReviewDecisions(const ListReviewDecisionsRequest& req,
                                          ListReviewDecisionsResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {req.content_id().c_str()};
    PGresult* r = ExecParams(
        "SELECT id, queue_item_id, content_id, outcome, comment, reviewer_id FROM governance_review_decision "
        "WHERE content_id = $1 ORDER BY created_at DESC",
        1, pv);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return false;
    }
    for (int i = 0; i < PQntuples(r); ++i) {
        auto* d = resp->add_decisions();
        d->set_id(PQgetvalue(r, i, 0));
        d->set_queue_item_id(PQgetvalue(r, i, 1));
        d->set_content_id(PQgetvalue(r, i, 2));
        d->set_outcome(static_cast<ReviewOutcome>(std::atoi(PQgetvalue(r, i, 3))));
        d->set_comment(PQgetvalue(r, i, 4));
        d->set_reviewer_id(PQgetvalue(r, i, 5));
    }
    ClearRes(r);
    return true;
}

bool PgGovernanceStore::GetReviewState(const GetReviewStateRequest& req, GetReviewStateResponse* resp) {
    ListReviewDecisionsRequest lreq;
    lreq.set_content_id(req.content_id());
    ListReviewDecisionsResponse lresp;
    if (!ListReviewDecisions(lreq, &lresp)) {
        return false;
    }
    resp->mutable_state()->set_content_id(req.content_id());
    if (lresp.decisions_size() > 0) {
        *resp->mutable_state()->mutable_latest_decision() = lresp.decisions(0);
    }
    return true;
}

bool PgGovernanceStore::SetVisibilityVerdict(const SetVisibilityVerdictRequest& req,
                                           SetVisibilityVerdictResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const std::string vid = GenId("vv");
    const std::string state = std::to_string(req.state());
    const std::string source = std::to_string(req.has_source() ? req.source() : VISIBILITY_VERDICT_SOURCE_SYSTEM);
    const char* pv[] = {req.content_id().c_str(), vid.c_str(), state.c_str(), req.reason_code().c_str(),
                        source.c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO governance_visibility_verdict (content_id, verdict_id, state, reason_code, source, version) "
        "VALUES ($1, $2, $3::int, $4, $5::int, 1) "
        "ON CONFLICT (content_id) DO UPDATE SET verdict_id = EXCLUDED.verdict_id, state = EXCLUDED.state, "
        "reason_code = EXCLUDED.reason_code, source = EXCLUDED.source, "
        "version = governance_visibility_verdict.version + 1, updated_at = now()",
        5, pv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    if (!ok) {
        return false;
    }
    auto* v = resp->mutable_verdict();
    v->set_id(vid);
    v->set_content_id(req.content_id());
    v->set_state(req.state());
    v->set_reason_code(req.reason_code());
    v->set_source(req.has_source() ? req.source() : VISIBILITY_VERDICT_SOURCE_SYSTEM);
    RecordOutboxEventLocked("governance.visibility.changed", req.content_id());
    return true;
}

bool PgGovernanceStore::BatchSetVisibilityVerdict(const BatchSetVisibilityVerdictRequest& req,
                                                BatchSetVisibilityVerdictResponse* resp) {
    for (const auto& mut : req.items()) {
        SetVisibilityVerdictRequest sreq;
        sreq.set_content_id(mut.content_id());
        sreq.set_state(mut.state());
        sreq.set_reason_code(mut.reason_code());
        SetVisibilityVerdictResponse sresp;
        if (!SetVisibilityVerdict(sreq, &sresp)) {
            return false;
        }
        *resp->add_verdicts() = sresp.verdict();
    }
    return true;
}

bool PgGovernanceStore::GetVisibilityVerdict(const GetVisibilityVerdictRequest& req,
                                           GetVisibilityVerdictResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {req.content_id().c_str()};
    PGresult* r = ExecParams(
        "SELECT verdict_id, content_id, state, reason_code, source, version FROM governance_visibility_verdict "
        "WHERE content_id = $1",
        1, pv);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return false;
    }
    if (PQntuples(r) < 1) {
        ClearRes(r);
        resp->mutable_verdict()->set_content_id(req.content_id());
        resp->mutable_verdict()->set_state(VISIBILITY_STATE_UNPUBLISHED);
        return true;
    }
    auto* v = resp->mutable_verdict();
    v->set_id(PQgetvalue(r, 0, 0));
    v->set_content_id(PQgetvalue(r, 0, 1));
    v->set_state(static_cast<VisibilityState>(std::atoi(PQgetvalue(r, 0, 2))));
    v->set_reason_code(PQgetvalue(r, 0, 3));
    v->set_source(static_cast<VisibilityVerdictSource>(std::atoi(PQgetvalue(r, 0, 4))));
    v->set_version(std::atoi(PQgetvalue(r, 0, 5)));
    ClearRes(r);
    return true;
}

bool PgGovernanceStore::IsCsideVisible(const std::string& content_id) {
    GetVisibilityVerdictRequest req;
    req.set_content_id(content_id);
    GetVisibilityVerdictResponse resp;
    if (!GetVisibilityVerdict(req, &resp)) {
        return false;
    }
    return resp.verdict().state() == VISIBILITY_STATE_PUBLISHED;
}

bool PgGovernanceStore::EvaluateVisibility(const EvaluateVisibilityRequest& req,
                                         EvaluateVisibilityResponse* resp) {
    GetVisibilityVerdictRequest greq;
    greq.set_content_id(req.content_id());
    GetVisibilityVerdictResponse gresp;
    if (!GetVisibilityVerdict(greq, &gresp)) {
        return false;
    }
    const bool allowed = gresp.verdict().state() == VISIBILITY_STATE_PUBLISHED;
    resp->set_allowed(allowed);
    *resp->mutable_verdict() = gresp.verdict();
    if (!allowed) {
        resp->add_reason_codes("not_published");
    }
    return true;
}

std::string PgGovernanceStore::HexEncode(const std::string& raw) {
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

bool PgGovernanceStore::HexDecode(const std::string& hex, std::string* out) {
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

bool PgGovernanceStore::UpsertCooperationLabel(const UpsertCooperationLabelRequest& req,
                                             UpsertCooperationLabelResponse* resp) {
    if (!req.has_label()) {
        return false;
    }
    CooperationLabel lbl = req.label();
    if (lbl.id().empty()) {
        lbl.set_id(GenId("cl"));
    }
    std::string raw;
    lbl.SerializeToString(&raw);
    const std::string hex = HexEncode(raw);
    std::lock_guard<std::mutex> lock(mu_);
    const std::string st = std::to_string(lbl.subject_type());
    const char* pv[] = {lbl.id().c_str(), st.c_str(), lbl.subject_id().c_str(),
                        lbl.cooperation_type().c_str(),
                        hex.c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO governance_cooperation_label (id, subject_type, subject_id, label_key, proto_hex) "
        "VALUES ($1, $2::int, $3, $4, $5) ON CONFLICT (id) DO UPDATE SET subject_type = EXCLUDED.subject_type, "
        "subject_id = EXCLUDED.subject_id, label_key = EXCLUDED.label_key, proto_hex = EXCLUDED.proto_hex",
        5, pv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    if (!ok) {
        return false;
    }
    *resp->mutable_label() = lbl;
    return true;
}

bool PgGovernanceStore::BatchGetCooperationLabels(const BatchGetCooperationLabelsRequest& req,
                                                BatchGetCooperationLabelsResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& key : req.keys()) {
        const char* pv[] = {key.subject_id().c_str()};
        PGresult* r = ExecParams("SELECT proto_hex FROM governance_cooperation_label WHERE subject_id = $1",
                                 1, pv);
        CooperationLabels cls;
        if (r && PQresultStatus(r) == PGRES_TUPLES_OK) {
            for (int i = 0; i < PQntuples(r); ++i) {
                std::string raw;
                if (HexDecode(PQgetvalue(r, i, 0), &raw)) {
                    CooperationLabel lbl;
                    if (lbl.ParseFromString(raw)) {
                        *cls.add_labels() = lbl;
                    }
                }
            }
        }
        ClearRes(r);
        (*resp->mutable_labels_by_subject())[key.subject_id()] = cls;
    }
    return true;
}

bool PgGovernanceStore::ListDisclosureTemplates(ListDisclosureTemplatesResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    PGresult* r = PQexec(conn_, "SELECT id, locale, template_key, body, version FROM governance_disclosure_template");
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return false;
    }
    for (int i = 0; i < PQntuples(r); ++i) {
        auto* t = resp->add_templates();
        t->set_id(PQgetvalue(r, i, 0));
        t->set_locale(PQgetvalue(r, i, 1));
        t->set_template_key(PQgetvalue(r, i, 2));
        t->set_body(PQgetvalue(r, i, 3));
        t->set_version(std::atoi(PQgetvalue(r, i, 4)));
    }
    ClearRes(r);
    return true;
}

bool PgGovernanceStore::CreateRiskFlag(const CreateRiskFlagRequest& req, CreateRiskFlagResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const std::string id = GenId("rf");
    const std::string st = std::to_string(req.subject_type());
    const std::string sev = std::to_string(req.severity());
    const char* pv[] = {id.c_str(), st.c_str(), req.subject_id().c_str(), req.flag_code().c_str(), sev.c_str(),
                        req.source().c_str()};
    PGresult* r = ExecParams(
        "INSERT INTO governance_risk_flag (id, subject_type, subject_id, flag_code, severity, source, active) "
        "VALUES ($1, $2::int, $3, $4, $5::int, $6, TRUE)",
        6, pv);
    const bool ok = r && PQresultStatus(r) == PGRES_COMMAND_OK;
    ClearRes(r);
    if (!ok) {
        return false;
    }
    auto* f = resp->mutable_flag();
    f->set_id(id);
    f->set_subject_type(req.subject_type());
    f->set_subject_id(req.subject_id());
    f->set_flag_code(req.flag_code());
    f->set_severity(req.severity());
    f->set_source(req.source());
    f->set_active(true);
    return true;
}

bool PgGovernanceStore::RevokeRiskFlag(const RevokeRiskFlagRequest& req, RevokeRiskFlagResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    const char* pv[] = {req.flag_id().c_str()};
    PGresult* r = ExecParams("UPDATE governance_risk_flag SET active = FALSE WHERE id = $1 RETURNING id, "
                             "subject_type, subject_id, flag_code, severity, source, active",
                             1, pv);
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) < 1) {
        ClearRes(r);
        return false;
    }
    auto* f = resp->mutable_flag();
    f->set_id(PQgetvalue(r, 0, 0));
    f->set_subject_type(static_cast<RiskSubjectType>(std::atoi(PQgetvalue(r, 0, 1))));
    f->set_subject_id(PQgetvalue(r, 0, 2));
    f->set_flag_code(PQgetvalue(r, 0, 3));
    f->set_severity(static_cast<RiskSeverity>(std::atoi(PQgetvalue(r, 0, 4))));
    f->set_source(PQgetvalue(r, 0, 5));
    f->set_active(PQgetvalue(r, 0, 6)[0] == 't');
    ClearRes(r);
    return true;
}

bool PgGovernanceStore::ListRiskFlags(const ListRiskFlagsRequest& req, ListRiskFlagsResponse* resp) {
    std::lock_guard<std::mutex> lock(mu_);
    PGresult* r = PQexec(conn_, "SELECT id, subject_type, subject_id, flag_code, severity, source, active "
                                "FROM governance_risk_flag ORDER BY created_at DESC LIMIT 200");
    if (!r || PQresultStatus(r) != PGRES_TUPLES_OK) {
        ClearRes(r);
        return false;
    }
    for (int i = 0; i < PQntuples(r); ++i) {
        if (!req.subject_id().empty() && req.subject_id() != PQgetvalue(r, i, 2)) {
            continue;
        }
        const bool active = PQgetvalue(r, i, 6)[0] == 't';
        if (req.active_only() && !active) {
            continue;
        }
        auto* f = resp->add_flags();
        f->set_id(PQgetvalue(r, i, 0));
        f->set_subject_type(static_cast<RiskSubjectType>(std::atoi(PQgetvalue(r, i, 1))));
        f->set_subject_id(PQgetvalue(r, i, 2));
        f->set_flag_code(PQgetvalue(r, i, 3));
        f->set_severity(static_cast<RiskSeverity>(std::atoi(PQgetvalue(r, i, 4))));
        f->set_source(PQgetvalue(r, i, 5));
        f->set_active(active);
    }
    ClearRes(r);
    return true;
}

}  // namespace governance_server
}  // namespace simple_living
