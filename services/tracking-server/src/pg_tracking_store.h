#pragma once

#include <postgresql/libpq-fe.h>
#include <mutex>
#include <string>

#include "tracking_server.pb.h"

namespace simple_living {
namespace tracking_server {

struct LinkRecord {
    std::string link_ref;
    std::string short_token;
    std::string landing_url;
    std::string guide_card_id;
    std::string scene;
    std::string placement;
};

class PgTrackingStore {
public:
    PgTrackingStore() = default;
    ~PgTrackingStore();
    PgTrackingStore(const PgTrackingStore&) = delete;
    PgTrackingStore& operator=(const PgTrackingStore&) = delete;

    bool ConnectAndInit(const std::string& conninfo);
    void Close();
    bool Ping();

    bool InsertLink(const LinkRecord& link);
    bool GetLinkByToken(const std::string& token, LinkRecord* out);
    bool GetLinkByRef(const std::string& ref, LinkRecord* out);
    bool InsertClick(const std::string& click_id,
                     const std::string& link_ref,
                     const std::string& short_token);
    bool InsertConversion(const IngestConversionRequest& req,
                          const std::string& conversion_id,
                          IngestConversionResponse* resp);
    bool FillCommissionSummary(GetCommissionSummaryResponse* resp);

private:
    std::mutex mu_;
    PGconn* conn_{nullptr};

    bool ExecSql(const char* sql);
    PGresult* ExecParams(const char* sql,
                         int n_params,
                         const char* const* param_values,
                         const int* param_lengths,
                         const int* param_formats);
    static bool LinkFromRow(PGresult* r, int row, LinkRecord* out);
    bool RecordOutboxEventLocked(const std::string& topic, const std::string& payload);
};

}  // namespace tracking_server
}  // namespace simple_living
