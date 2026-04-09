#pragma once

#include <postgresql/libpq-fe.h>
#include <mutex>
#include <string>

#include "user_domain_service.pb.h"

namespace simple_living {
namespace user_domain {

// PostgreSQL-backed persistence for user-domain (libpq). All public methods are thread-safe.
class PgUserStore {
public:
    PgUserStore() = default;
    ~PgUserStore();
    PgUserStore(const PgUserStore&) = delete;
    PgUserStore& operator=(const PgUserStore&) = delete;

    bool ConnectAndInit(const std::string& conninfo);
    void Close();
    bool Ping();

    void IssueTokenPair(const IssueTokenPairRequest& req, IssueTokenPairResponse* resp);
    void IntrospectAccessToken(const IntrospectAccessTokenRequest& req, IntrospectAccessTokenResponse* resp);
    void IntrospectRefreshToken(const IntrospectRefreshTokenRequest& req, IntrospectRefreshTokenResponse* resp);
    void RevokeSession(const RevokeSessionRequest& req, RevokeSessionResponse* resp);
    void EnsureGuestSession(const EnsureGuestSessionRequest& req, EnsureGuestSessionResponse* resp);

    void GetProfile(const GetProfileRequest& req, GetProfileResponse* resp);
    void UpdateProfile(const UpdateProfileRequest& req, UpdateProfileResponse* resp);
    void GetPreferences(const GetPreferencesRequest& req, GetPreferencesResponse* resp);
    void UpdatePreferences(const UpdatePreferencesRequest& req, UpdatePreferencesResponse* resp);

    void ListFavorites(const ListFavoritesRequest& req, ListFavoritesResponse* resp);
    void AddFavorite(const AddFavoriteRequest& req, AddFavoriteResponse* resp);
    void RemoveFavorite(const RemoveFavoriteRequest& req, RemoveFavoriteResponse* resp);

    void ListHistory(const ListHistoryRequest& req, ListHistoryResponse* resp);
    void RecordHistoryEvent(const RecordHistoryEventRequest& req, RecordHistoryEventResponse* resp);
    void ClearHistory(const ClearHistoryRequest& req, ClearHistoryResponse* resp);

    void SubmitFeedback(const SubmitFeedbackRequest& req, SubmitFeedbackResponse* resp);
    void GetConsent(const GetConsentRequest& req, GetConsentResponse* resp);
    void UpdateConsent(const UpdateConsentRequest& req, UpdateConsentResponse* resp);

    void GetSignalBundleRef(const GetSignalBundleRefRequest& req, GetSignalBundleRefResponse* resp);

    void GetMeSummary(const GetMeSummaryRequest& req, GetMeSummaryResponse* resp);

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
    static std::string OwnerKeyFromHistoryReq(const RecordHistoryEventRequest& req);
    static std::string OwnerKeyFromListHistory(const ListHistoryRequest& req);
    static std::string OwnerKeyFromClearHistory(const ClearHistoryRequest& req);
    static std::string UserKeyForSignal(const GetSignalBundleRefRequest& req);

    void GetProfileUnlocked(const GetProfileRequest& req, GetProfileResponse* resp);
    void GetPreferencesUnlocked(const GetPreferencesRequest& req, GetPreferencesResponse* resp);
    void GetConsentUnlocked(const GetConsentRequest& req, GetConsentResponse* resp);
};

}  // namespace user_domain
}  // namespace simple_living
