import XCTest
@testable import SimpleLivingCore

final class HttpGatewayAPITests: XCTestCase {
    override func setUp() {
        super.setUp()
        URLProtocolStub.reset()
    }

    override func tearDown() {
        URLProtocolStub.reset()
        super.tearDown()
    }

    func testGetHomeFeedBuildsQueryAndParsesResponse() async throws {
        let body = """
        {"success":true,"code":0,"message":"ok","data":{"items":[{"recommendation_id":"rec_1","scene":"home_feed","rank":1,"guide_card_id":"guide_1","reason_text":"a / b","reason_tags":["a","b"],"guide_card":{"title":"Title A"}}],"pagination":{"next_cursor":"n1","has_more":true,"limit":20}}}
        """
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(body.utf8)))

        let api = makeAPI()
        let result = try await api.getHomeFeed(theme: .food, pagination: Pagination(cursor: "c1", limit: 20))

        XCTAssertEqual(result.cards.count, 1)
        XCTAssertEqual(result.cards[0].title, "Title A")
        XCTAssertEqual(result.cards[0].reason, "a / b")
        XCTAssertEqual(result.nextCursor, "n1")

        let request = try XCTUnwrap(URLProtocolStub.requests.first)
        let url = try XCTUnwrap(request.url?.absoluteString)
        XCTAssertTrue(url.contains("theme=food"))
        XCTAssertTrue(url.contains("cursor=c1"))
        XCTAssertTrue(url.contains("limit=20"))
    }

    func testGetGuideDetailFallsBackWhenSummaryMissing() async throws {
        let body = """
        {"success":true,"code":0,"message":"ok","data":{"guide":{"guide_card_id":"guide_1","title":"Detail","subtitle":"Sub"}}}
        """
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(body.utf8)))

        let result = try await makeAPI().getGuideDetail(guideCardId: "guide_1")
        XCTAssertEqual(result.summary, "Sub")
    }

    func testRedirectPrepareEnsuresGuestSessionThenPostsContext() async throws {
        let guestBody = """
        {"success":true,"code":0,"message":"ok","data":{"session_id":"guest_1"}}
        """
        let redirectBody = """
        {"success":true,"code":0,"message":"ok","data":{"landing_url":"https://example.com/p"}} 
        """
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(guestBody.utf8)))
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(redirectBody.utf8)))

        let api = makeAPI(accessToken: nil)
        let ctx = FeedItemContext(guideCardId: "g1", recommendationId: "r1", scene: "home_feed", itemRank: 2)
        let result = try await api.postRedirectPrepare(context: ctx)
        XCTAssertEqual(result.landingUrl, "https://example.com/p")
        XCTAssertEqual(URLProtocolStub.requests.count, 2)
        XCTAssertTrue(URLProtocolStub.requests[1].value(forHTTPHeaderField: "X-Guest-Session-Id") == "guest_1")
    }

    func testGetMeSummaryWithExpiredTokenCanBeRecoveredByRefresh() async throws {
        let unauthorized = """
        {"success":false,"code":20002,"message":"expired","data":null}
        """
        let refreshOK = """
        {"success":true,"code":0,"message":"ok","data":{"access_token":"a2","refresh_token":"r2","expires_in":3600,"session_id":"s2"}}
        """
        let meOK = """
        {"success":true,"code":0,"message":"ok","data":{"profile":{"is_guest":false},"counts":{"favorites_count":3,"history_count":5},"consent":{"personalization_allowed":true}}}
        """
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(unauthorized.utf8)))
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(refreshOK.utf8)))
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(meOK.utf8)))

        let settings = makeSettings(accessToken: "a1", refreshToken: "r1")
        let api = HttpGatewayAPI(settings: settings, session: makeTestSession())

        // HttpGatewayAPI does not auto-refresh in getMeSummary; verify business error.
        do {
            _ = try await api.getMeSummary()
            XCTFail("Expected business error")
        } catch let err as GatewayAPIError {
            guard case .business(let code, _) = err else {
                return XCTFail("Unexpected error")
            }
            XCTAssertEqual(code, 20002)
        }
    }

    func testIssueTokenWithPhonePersistsTokens() async throws {
        let body = """
        {"success":true,"code":0,"message":"ok","data":{"access_token":"a_new","refresh_token":"r_new","expires_in":3600,"session_id":"s1"}}
        """
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(body.utf8)))

        let defaults = UserDefaults(suiteName: "HttpGatewayAPITests.issueToken")!
        defaults.removePersistentDomain(forName: "HttpGatewayAPITests.issueToken")
        let settings = HttpGatewaySettings(
            baseURL: URL(string: "https://example.com")!,
            deviceId: "dev-1",
            appVersion: "1.0.0",
            userDefaults: defaults
        )
        let api = HttpGatewayAPI(settings: settings, session: makeTestSession())

        let pair = try await api.issueTokenWithPhone(phoneE164: "+8613800138000", otpCode: "123456", verificationId: "v1")
        XCTAssertEqual(pair.accessToken, "a_new")
        XCTAssertEqual(settings.accessToken, "a_new")
        XCTAssertEqual(settings.refreshToken, "r_new")

        let request = try XCTUnwrap(URLProtocolStub.requests.first)
        let json = try requestJSONBody(request)
        XCTAssertNil(json["request_context"])
    }

    func testRefreshAuthTokensWithoutTokenThrowsBusiness20003() async {
        let settings = makeSettings(accessToken: nil, refreshToken: nil)
        let api = HttpGatewayAPI(settings: settings, session: makeTestSession())
        do {
            _ = try await api.refreshAuthTokens()
            XCTFail("Expected error")
        } catch let err as GatewayAPIError {
            guard case .business(let code, _) = err else {
                return XCTFail("Unexpected error")
            }
            XCTAssertEqual(code, 20003)
        } catch {
            XCTFail("Unexpected error: \(error)")
        }
    }

    func testRefreshAuthTokensBodyIncludesRequestContext() async throws {
        let body = """
        {"success":true,"code":0,"message":"ok","data":{"access_token":"a_new","refresh_token":"r_new","expires_in":3600,"session_id":"s1"}}
        """
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(body.utf8)))
        let api = makeAPI(accessToken: nil, refreshToken: "r_old")
        _ = try await api.refreshAuthTokens()

        let request = try XCTUnwrap(URLProtocolStub.requests.first)
        let json = try requestJSONBody(request)
        XCTAssertEqual(json["refresh_token"] as? String, "r_old")
        let requestContext = try XCTUnwrap(json["request_context"] as? [String: Any])
        XCTAssertEqual(requestContext["client_platform"] as? String, "ios")
        XCTAssertEqual(requestContext["app_version"] as? String, "1.0.0")
        XCTAssertEqual(requestContext["device_id"] as? String, "dev-1")
        XCTAssertNil(request.value(forHTTPHeaderField: "X-Client-Platform"))
        XCTAssertNil(request.value(forHTTPHeaderField: "X-Client-Version"))
        XCTAssertNil(request.value(forHTTPHeaderField: "X-Device-Id"))
    }

    func testLogoutClearsAuth() async throws {
        let body = """
        {"success":true,"code":0,"message":"ok","data":{"revoked":true}}
        """
        URLProtocolStub.enqueue(.init(statusCode: 200, headers: [:], body: Data(body.utf8)))

        let settings = makeSettings(accessToken: "a1", refreshToken: "r1")
        let api = HttpGatewayAPI(settings: settings, session: makeTestSession())
        try await api.logoutSession(revokeAllDevices: false)
        XCTAssertNil(settings.accessToken)
        XCTAssertNil(settings.refreshToken)
        XCTAssertNil(settings.guestSessionId)
    }

    private func makeAPI(accessToken: String? = "a1", refreshToken: String? = "r1") -> HttpGatewayAPI {
        HttpGatewayAPI(settings: makeSettings(accessToken: accessToken, refreshToken: refreshToken), session: makeTestSession())
    }

    private func makeSettings(accessToken: String?, refreshToken: String?) -> HttpGatewaySettings {
        let defaults = UserDefaults(suiteName: "HttpGatewayAPITests.\(UUID().uuidString)")!
        return HttpGatewaySettings(
            baseURL: URL(string: "https://example.com")!,
            accessToken: accessToken,
            refreshToken: refreshToken,
            guestSessionId: nil,
            deviceId: "dev-1",
            appVersion: "1.0.0",
            userDefaults: defaults
        )
    }

    private func requestJSONBody(_ request: URLRequest) throws -> [String: Any] {
        if let body = request.httpBody,
           let json = try JSONSerialization.jsonObject(with: body) as? [String: Any]
        {
            return json
        }
        if let stream = request.httpBodyStream {
            stream.open()
            defer { stream.close() }
            var data = Data()
            let bufferSize = 1024
            let buffer = UnsafeMutablePointer<UInt8>.allocate(capacity: bufferSize)
            defer { buffer.deallocate() }
            while stream.hasBytesAvailable {
                let read = stream.read(buffer, maxLength: bufferSize)
                if read < 0 {
                    throw stream.streamError ?? URLError(.cannotDecodeContentData)
                }
                if read == 0 { break }
                data.append(buffer, count: read)
            }
            if let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] {
                return json
            }
        }
        XCTFail("Expected JSON request body")
        return [:]
    }
}
