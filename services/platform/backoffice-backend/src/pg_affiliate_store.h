#pragma once

#include <postgresql/libpq-fe.h>
#include <mutex>
#include <string>

#include "affiliate_server.pb.h"

namespace simple_living {
namespace affiliate_server {

class PgAffiliateStore {
public:
    PgAffiliateStore() = default;
    ~PgAffiliateStore();
    PgAffiliateStore(const PgAffiliateStore&) = delete;
    PgAffiliateStore& operator=(const PgAffiliateStore&) = delete;

    bool ConnectAndInit(const std::string& conninfo);
    void Close();
    bool Ping();
    bool EnsureSeedPartners();

    bool GetPartnerCapabilitySnapshot(const std::string& partner_id, PartnerCapabilitySnapshot* out);
    bool UpsertPartner(const PartnerCapabilitySnapshot& snapshot);
    bool CreateCommissionRuleSetVersion(const std::string& partner_id,
                                        std::string* rule_set_id,
                                        int32_t* version);
    bool GetCommissionRuleSet(const std::string& partner_id,
                              std::string* rule_set_id,
                              int32_t* version);

private:
    std::mutex mu_;
    PGconn* conn_{nullptr};
    bool ExecSql(const char* sql);
    PGresult* ExecParams(const char* sql,
                         int n_params,
                         const char* const* param_values);
    static std::string GenId(const std::string& prefix);
    static std::string HexEncode(const std::string& raw);
    static bool HexDecode(const std::string& hex, std::string* out);
};

}  // namespace affiliate_server
}  // namespace simple_living
