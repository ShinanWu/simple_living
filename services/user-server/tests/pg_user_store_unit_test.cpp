#include <iostream>
#include <string>

#include "pg_user_store.h"

namespace {

int g_failures = 0;

void Expect(bool cond, const char* message) {
    if (!cond) {
        std::cerr << "[FAIL] " << message << std::endl;
        ++g_failures;
    }
}

void FillIssueContext(simple_living::user_server::IssueTokenPairRequest* req) {
    req->set_client_platform(simple_living::user_server::CLIENT_PLATFORM_WECHAT_MINIPROGRAM);
    req->set_device_id("device_lab_smoke");
}

bool RunPgIntegrationTests(simple_living::user_server::PgUserStore* store) {
    using simple_living::user_server::IssueTokenPairRequest;
    using simple_living::user_server::IssueTokenPairResponse;
    using simple_living::user_server::IntrospectAccessTokenRequest;
    using simple_living::user_server::IntrospectAccessTokenResponse;
    using simple_living::user_server::ListHistoryRequest;
    using simple_living::user_server::ListHistoryResponse;
    using simple_living::user_server::RecordHistoryEventRequest;
    using simple_living::user_server::RecordHistoryEventResponse;

    IssueTokenPairRequest phone_req;
    FillIssueContext(&phone_req);
    phone_req.mutable_phone_otp()->set_phone_e164("+8613800138000");
    phone_req.mutable_phone_otp()->set_verification_id("test_phone_verification");
    phone_req.mutable_phone_otp()->set_otp_code("123456");
    IssueTokenPairResponse phone_resp;
    store->IssueTokenPair(phone_req, &phone_resp);
    Expect(!phone_resp.access_token().empty(), "phone_otp lab should issue access_token");
    Expect(!phone_resp.refresh_token().empty(), "phone_otp lab should issue refresh_token");
    Expect(phone_resp.has_access_expires_at() && phone_resp.access_expires_at().seconds() > 0,
           "phone_otp lab should set access_expires_at");
    Expect(phone_resp.expires_in_seconds() == 3600, "phone_otp lab expires_in_seconds should be 3600");

    IntrospectAccessTokenRequest intro_req;
    intro_req.set_access_token(phone_resp.access_token());
    IntrospectAccessTokenResponse intro_resp;
    store->IntrospectAccessToken(intro_req, &intro_resp);
    Expect(intro_resp.valid(), "issued access_token should introspect as valid");
    Expect(intro_resp.user_id() == "usr_phone_+8613800138000",
           "phone_otp lab should derive stable usr_phone_<e164> user_id");

    IssueTokenPairRequest phone_repeat;
    FillIssueContext(&phone_repeat);
    phone_repeat.mutable_phone_otp()->set_phone_e164("+8613800138000");
    phone_repeat.mutable_phone_otp()->set_verification_id("test_phone_verification");
    phone_repeat.mutable_phone_otp()->set_otp_code("123456");
    IssueTokenPairResponse phone_repeat_resp;
    store->IssueTokenPair(phone_repeat, &phone_repeat_resp);
    IntrospectAccessTokenRequest intro_repeat_req;
    intro_repeat_req.set_access_token(phone_repeat_resp.access_token());
    IntrospectAccessTokenResponse intro_repeat_resp;
    store->IntrospectAccessToken(intro_repeat_req, &intro_repeat_resp);
    Expect(intro_repeat_resp.user_id() == "usr_phone_+8613800138000",
           "repeat phone_otp lab should reuse same user_id");

    IssueTokenPairRequest oauth_req;
    FillIssueContext(&oauth_req);
    oauth_req.mutable_oauth()->set_provider("wechat");
    oauth_req.mutable_oauth()->set_authorization_code("wx_lab_code_abc");
    IssueTokenPairResponse oauth_resp;
    store->IssueTokenPair(oauth_req, &oauth_resp);
    IntrospectAccessTokenRequest oauth_intro_req;
    oauth_intro_req.set_access_token(oauth_resp.access_token());
    IntrospectAccessTokenResponse oauth_intro_resp;
    store->IntrospectAccessToken(oauth_intro_req, &oauth_intro_resp);
    Expect(oauth_intro_resp.user_id() == "usr_wechat_wx_lab_code_abc",
           "wechat oauth lab should derive usr_wechat_<authorization_code>");

    IssueTokenPairRequest oauth_subject_req;
    FillIssueContext(&oauth_subject_req);
    oauth_subject_req.mutable_oauth()->set_provider("wechat");
    oauth_subject_req.mutable_oauth()->set_authorization_code("ignored_code");
    oauth_subject_req.mutable_oauth()->set_provider_subject("openid_lab_001");
    IssueTokenPairResponse oauth_subject_resp;
    store->IssueTokenPair(oauth_subject_req, &oauth_subject_resp);
    IntrospectAccessTokenRequest oauth_subject_intro_req;
    oauth_subject_intro_req.set_access_token(oauth_subject_resp.access_token());
    IntrospectAccessTokenResponse oauth_subject_intro_resp;
    store->IntrospectAccessToken(oauth_subject_intro_req, &oauth_subject_intro_resp);
    Expect(oauth_subject_intro_resp.user_id() == "usr_wechat_openid_lab_001",
           "wechat oauth lab should prefer provider_subject");

    IssueTokenPairRequest bad_otp_req;
    FillIssueContext(&bad_otp_req);
    bad_otp_req.mutable_phone_otp()->set_phone_e164("+8613800138001");
    bad_otp_req.mutable_phone_otp()->set_verification_id("test_phone_verification");
    bad_otp_req.mutable_phone_otp()->set_otp_code("000000");
    IssueTokenPairResponse bad_otp_resp;
    store->IssueTokenPair(bad_otp_req, &bad_otp_resp);
    Expect(bad_otp_resp.access_token().empty(), "invalid lab otp should not issue tokens");

    const int64_t kOccurredAt = 1718366400;  // 2024-06-14T12:00:00Z
    RecordHistoryEventRequest hist_req;
    hist_req.set_user_id("usr_phone_+8613800138000");
    hist_req.mutable_content_ref()->set_content_id("guide_card_ts_test");
    hist_req.mutable_occurred_at()->set_seconds(kOccurredAt);
    RecordHistoryEventResponse hist_resp;
    store->RecordHistoryEvent(hist_req, &hist_resp);
    Expect(hist_resp.recorded(), "RecordHistoryEvent should succeed for integration user");

    ListHistoryRequest list_req;
    list_req.set_user_id("usr_phone_+8613800138000");
    ListHistoryResponse list_resp;
    store->ListHistory(list_req, &list_resp);
    Expect(list_resp.items_size() > 0, "ListHistory should return recorded item");
    bool found_ts = false;
    for (int i = 0; i < list_resp.items_size(); ++i) {
        if (list_resp.items(i).content_ref().content_id() != "guide_card_ts_test") {
            continue;
        }
        found_ts = true;
        Expect(list_resp.items(i).last_seen_at().seconds() == kOccurredAt,
               "ListHistory last_seen_at should match stored timestamptz epoch");
        Expect(list_resp.items(i).first_seen_at().seconds() == kOccurredAt,
               "ListHistory first_seen_at should match stored timestamptz epoch");
    }
    Expect(found_ts, "ListHistory should include guide_card_ts_test item");

    return g_failures == 0;
}

}  // namespace

int main() {
    simple_living::user_server::PgUserStore store;

    Expect(!store.Ping(), "Ping should be false before connect");
    store.Close();
    store.Close();
    Expect(!store.Ping(), "Ping should remain false after Close without connect");

    const bool bad_connect = store.ConnectAndInit(
        "host=127.0.0.1 port=1 dbname=missing user=missing password=missing connect_timeout=1");
    Expect(!bad_connect, "ConnectAndInit should fail for invalid local PostgreSQL endpoint");
    Expect(!store.Ping(), "Ping should be false after failed connect");
    store.Close();

    const std::string pg_conninfo =
        "host=127.0.0.1 port=5432 dbname=simple_living user=simple password=simple connect_timeout=2";
    if (store.ConnectAndInit(pg_conninfo) && store.Ping()) {
        std::cout << "PostgreSQL available; running integration tests" << std::endl;
        RunPgIntegrationTests(&store);
    } else {
        std::cout << "PostgreSQL unavailable; skipping integration tests" << std::endl;
        store.Close();
    }

    if (g_failures == 0) {
        std::cout << "pg_user_store_unit_test passed" << std::endl;
        return 0;
    }
    return 1;
}
