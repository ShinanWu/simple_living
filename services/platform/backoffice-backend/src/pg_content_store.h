#pragma once

#include <postgresql/libpq-fe.h>
#include <mutex>
#include <string>
#include <vector>

#include "catalog.pb.h"

namespace simple_living {
namespace content_server {

using catalog::ContentLifecycleStatus;
using catalog::GuideCard;

struct GuideCardRevisionMeta {
    int64_t revision{0};
    std::string change_summary;
    std::string created_by;
};

// PostgreSQL-backed CMS store for guide cards. Thread-safe; owns one libpq connection.
class PgContentStore {
public:
    PgContentStore() = default;
    ~PgContentStore();
    PgContentStore(const PgContentStore&) = delete;
    PgContentStore& operator=(const PgContentStore&) = delete;

    bool ConnectAndInit(const std::string& conninfo);
    void Close();
    bool Ping();

    bool EnsureSeedGuideCards(const std::vector<GuideCard>& seeds);
    bool BatchGetGuideCards(const std::vector<std::string>& card_ids,
                            std::vector<GuideCard>* cards,
                            std::vector<std::string>* missing_ids);
    bool ListGuideCards(const std::string& theme_id,
                        int limit,
                        std::vector<GuideCard>* cards,
                        bool* has_more);
    bool UpsertGuideCard(GuideCard* card);
    bool LoadGuideCard(const std::string& card_id, GuideCard* card);
    bool ListGuideCardRevisions(const std::string& card_id,
                                std::vector<GuideCardRevisionMeta>* revisions);
    bool LoadGuideCardRevision(const std::string& card_id,
                               int64_t revision,
                               GuideCard* card);
    bool RollbackGuideCardRevision(const std::string& card_id,
                                   int64_t target_revision,
                                   GuideCard* updated);
    bool UpdateGuideCardStatus(const std::string& card_id,
                               ContentLifecycleStatus status,
                               int64_t published_revision,
                               GuideCard* updated);
    bool ListPublishedGuideCards(std::vector<GuideCard>* cards);

private:
    std::mutex mu_;
    PGconn* conn_{nullptr};

    bool ExecSql(const char* sql);
    PGresult* ExecParams(const char* sql,
                         int n_params,
                         const char* const* param_values,
                         const int* param_lengths,
                         const int* param_formats);
    static std::string HexEncode(const std::string& raw);
    static bool HexDecode(const std::string& hex, std::string* out);
    static std::string FirstSellingPoint(const GuideCard& card);
    static std::string FirstThemeId(const GuideCard& card);
    static std::string FirstAffiliateChannel(const GuideCard& card);
    static std::string FirstAffiliateExternalItemId(const GuideCard& card);
    static std::string FirstAffiliateLandingUrl(const GuideCard& card);
    static bool CardFromRow(PGresult* r, int row, GuideCard* card);
    bool UpsertGuideCardLocked(const GuideCard& card, bool record_revision_snapshot = true);
    bool RecordGuideCardRevisionLocked(const GuideCard& card, const std::string& change_summary);
    bool LoadGuideCardLocked(const std::string& card_id, GuideCard* card);
    bool LoadGuideCardRevisionLocked(const std::string& card_id, int64_t revision, GuideCard* card);
    int CountGuideCardsLocked();
};

}  // namespace content_server
}  // namespace simple_living
