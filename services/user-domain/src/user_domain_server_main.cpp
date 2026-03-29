// Placeholder brpc server: registers UserDomainService; RPC bodies are stubs (see docs/api.md).
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include "user_domain_service.pb.h"

DEFINE_int32(port, 9101, "TCP port for user-domain brpc server");

namespace simple_living {
namespace user_domain {

class UserDomainServiceImpl : public UserDomainService {
public:
    void IntrospectAccessToken(::google::protobuf::RpcController* controller,
                               const IntrospectAccessTokenRequest* request,
                               IntrospectAccessTokenResponse* response,
                               ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void IntrospectRefreshToken(::google::protobuf::RpcController* controller,
                                const IntrospectRefreshTokenRequest* request,
                                IntrospectRefreshTokenResponse* response,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void IssueTokenPair(::google::protobuf::RpcController* controller,
                        const IssueTokenPairRequest* request,
                        IssueTokenPairResponse* response,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void RevokeSession(::google::protobuf::RpcController* controller,
                       const RevokeSessionRequest* request,
                       RevokeSessionResponse* response,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void EnsureGuestSession(::google::protobuf::RpcController* controller,
                            const EnsureGuestSessionRequest* request,
                            EnsureGuestSessionResponse* response,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void GetProfile(::google::protobuf::RpcController* controller,
                    const GetProfileRequest* request,
                    GetProfileResponse* response,
                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void UpdateProfile(::google::protobuf::RpcController* controller,
                       const UpdateProfileRequest* request,
                       UpdateProfileResponse* response,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void GetPreferences(::google::protobuf::RpcController* controller,
                        const GetPreferencesRequest* request,
                        GetPreferencesResponse* response,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void UpdatePreferences(::google::protobuf::RpcController* controller,
                           const UpdatePreferencesRequest* request,
                           UpdatePreferencesResponse* response,
                           ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void ListFavorites(::google::protobuf::RpcController* controller,
                       const ListFavoritesRequest* request,
                       ListFavoritesResponse* response,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void AddFavorite(::google::protobuf::RpcController* controller,
                     const AddFavoriteRequest* request,
                     AddFavoriteResponse* response,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void RemoveFavorite(::google::protobuf::RpcController* controller,
                        const RemoveFavoriteRequest* request,
                        RemoveFavoriteResponse* response,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void ListHistory(::google::protobuf::RpcController* controller,
                     const ListHistoryRequest* request,
                     ListHistoryResponse* response,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void RecordHistoryEvent(::google::protobuf::RpcController* controller,
                            const RecordHistoryEventRequest* request,
                            RecordHistoryEventResponse* response,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void ClearHistory(::google::protobuf::RpcController* controller,
                      const ClearHistoryRequest* request,
                      ClearHistoryResponse* response,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void SubmitFeedback(::google::protobuf::RpcController* controller,
                        const SubmitFeedbackRequest* request,
                        SubmitFeedbackResponse* response,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void GetConsent(::google::protobuf::RpcController* controller,
                    const GetConsentRequest* request,
                    GetConsentResponse* response,
                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void UpdateConsent(::google::protobuf::RpcController* controller,
                       const UpdateConsentRequest* request,
                       UpdateConsentResponse* response,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void GetSignalBundleRef(::google::protobuf::RpcController* controller,
                            const GetSignalBundleRefRequest* request,
                            GetSignalBundleRefResponse* response,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void ResolveSignalBundle(::google::protobuf::RpcController* controller,
                             const ResolveSignalBundleRequest* request,
                             ResolveSignalBundleResponse* response,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void GetMeSummary(::google::protobuf::RpcController* controller,
                      const GetMeSummaryRequest* request,
                      GetMeSummaryResponse* response,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        (void)response;
    }

    void HealthCheck(::google::protobuf::RpcController* controller,
                     const HealthCheckRequest* request,
                     HealthCheckResponse* response,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard done_guard(done);
        (void)controller;
        (void)request;
        response->set_status("ok");
    }
};

}  // namespace user_domain
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);

    simple_living::user_domain::UserDomainServiceImpl svc;
    brpc::Server server;
    if (server.AddService(&svc, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add UserDomainService";
        return 1;
    }

    brpc::ServerOptions options;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start user-domain server";
        return 1;
    }

    server.RunUntilAskedToQuit();
    return 0;
}
