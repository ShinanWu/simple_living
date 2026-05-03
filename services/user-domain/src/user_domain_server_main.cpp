// user-domain brpc server — PostgreSQL (libpq) + Redis/Kafka per docs/architecture.
#include <gflags/gflags.h>
#include <brpc/server.h>
#include <butil/logging.h>

#include "pg_user_store.h"
#include "user_domain_service.pb.h"

DEFINE_int32(port, 9101, "TCP port for user-domain brpc server");
DEFINE_string(pg_conninfo,
              "host=127.0.0.1 port=5432 dbname=simple_living user=simple password=simple",
              "libpq connection string; must point to PostgreSQL (see services/foundation/postgres/deploy/docker-compose.yml)");

namespace simple_living {
namespace user_domain {

class UserDomainServiceImpl : public UserDomainService {
    PgUserStore* store_;

public:
    explicit UserDomainServiceImpl(PgUserStore* s) : store_(s) {}

    void IssueTokenPair(::google::protobuf::RpcController*,
                        const IssueTokenPairRequest* req,
                        IssueTokenPairResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->IssueTokenPair(*req, resp);
    }

    void IntrospectAccessToken(::google::protobuf::RpcController*,
                               const IntrospectAccessTokenRequest* req,
                               IntrospectAccessTokenResponse* resp,
                               ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->IntrospectAccessToken(*req, resp);
    }

    void IntrospectRefreshToken(::google::protobuf::RpcController*,
                                const IntrospectRefreshTokenRequest* req,
                                IntrospectRefreshTokenResponse* resp,
                                ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->IntrospectRefreshToken(*req, resp);
    }

    void RevokeSession(::google::protobuf::RpcController*,
                       const RevokeSessionRequest* req,
                       RevokeSessionResponse* resp,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->RevokeSession(*req, resp);
    }

    void EnsureGuestSession(::google::protobuf::RpcController*,
                            const EnsureGuestSessionRequest* req,
                            EnsureGuestSessionResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->EnsureGuestSession(*req, resp);
    }

    void GetProfile(::google::protobuf::RpcController*,
                    const GetProfileRequest* req,
                    GetProfileResponse* resp,
                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->GetProfile(*req, resp);
    }

    void UpdateProfile(::google::protobuf::RpcController*,
                       const UpdateProfileRequest* req,
                       UpdateProfileResponse* resp,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->UpdateProfile(*req, resp);
    }

    void GetPreferences(::google::protobuf::RpcController*,
                        const GetPreferencesRequest* req,
                        GetPreferencesResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->GetPreferences(*req, resp);
    }

    void UpdatePreferences(::google::protobuf::RpcController*,
                           const UpdatePreferencesRequest* req,
                           UpdatePreferencesResponse* resp,
                           ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->UpdatePreferences(*req, resp);
    }

    void ListFavorites(::google::protobuf::RpcController*,
                       const ListFavoritesRequest* req,
                       ListFavoritesResponse* resp,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ListFavorites(*req, resp);
    }

    void AddFavorite(::google::protobuf::RpcController*,
                     const AddFavoriteRequest* req,
                     AddFavoriteResponse* resp,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->AddFavorite(*req, resp);
    }

    void RemoveFavorite(::google::protobuf::RpcController*,
                        const RemoveFavoriteRequest* req,
                        RemoveFavoriteResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->RemoveFavorite(*req, resp);
    }

    void ListHistory(::google::protobuf::RpcController*,
                     const ListHistoryRequest* req,
                     ListHistoryResponse* resp,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ListHistory(*req, resp);
    }

    void RecordHistoryEvent(::google::protobuf::RpcController*,
                            const RecordHistoryEventRequest* req,
                            RecordHistoryEventResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->RecordHistoryEvent(*req, resp);
    }

    void ClearHistory(::google::protobuf::RpcController*,
                      const ClearHistoryRequest* req,
                      ClearHistoryResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->ClearHistory(*req, resp);
    }

    void SubmitFeedback(::google::protobuf::RpcController*,
                        const SubmitFeedbackRequest* req,
                        SubmitFeedbackResponse* resp,
                        ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->SubmitFeedback(*req, resp);
    }

    void GetConsent(::google::protobuf::RpcController*,
                    const GetConsentRequest* req,
                    GetConsentResponse* resp,
                    ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->GetConsent(*req, resp);
    }

    void UpdateConsent(::google::protobuf::RpcController*,
                       const UpdateConsentRequest* req,
                       UpdateConsentResponse* resp,
                       ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->UpdateConsent(*req, resp);
    }

    void GetSignalBundleRef(::google::protobuf::RpcController*,
                            const GetSignalBundleRefRequest* req,
                            GetSignalBundleRefResponse* resp,
                            ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->GetSignalBundleRef(*req, resp);
    }

    void ResolveSignalBundle(::google::protobuf::RpcController*,
                             const ResolveSignalBundleRequest*,
                             ResolveSignalBundleResponse* resp,
                             ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        resp->set_resolved(true);
        resp->mutable_payload()->set_opaque_payload_json("{}");
    }

    void GetMeSummary(::google::protobuf::RpcController*,
                      const GetMeSummaryRequest* req,
                      GetMeSummaryResponse* resp,
                      ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        store_->GetMeSummary(*req, resp);
    }

    void HealthCheck(::google::protobuf::RpcController*,
                     const HealthCheckRequest*,
                     HealthCheckResponse* resp,
                     ::google::protobuf::Closure* done) override {
        brpc::ClosureGuard g(done);
        const bool pg_ok = store_->Ping();
        resp->set_status(pg_ok ? "ok" : "degraded");
        auto* c = resp->add_components();
        c->set_name("postgresql");
        c->set_ok(pg_ok);
        c->set_detail(pg_ok ? "connected" : "unreachable");
    }
};

}  // namespace user_domain
}  // namespace simple_living

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);

    simple_living::user_domain::PgUserStore store;
    if (!store.ConnectAndInit(FLAGS_pg_conninfo)) {
        LOG(ERROR) << "PostgreSQL ConnectAndInit failed; check -pg_conninfo and that the server is running "
                        "(see services/foundation/postgres/docs/README.md)";
        return 1;
    }

    simple_living::user_domain::UserDomainServiceImpl svc(&store);
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

    LOG(INFO) << "user-domain server listening on port " << FLAGS_port;
    server.RunUntilAskedToQuit();
    return 0;
}
