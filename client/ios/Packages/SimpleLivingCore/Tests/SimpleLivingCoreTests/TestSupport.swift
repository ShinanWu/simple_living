import Foundation
@testable import SimpleLivingCore

final class URLProtocolStub: URLProtocol {
    struct Stub {
        let statusCode: Int
        let headers: [String: String]
        let body: Data
    }

    static var stubs: [Stub] = []
    static var requests: [URLRequest] = []
    static let lock = NSLock()

    static func reset() {
        lock.lock()
        defer { lock.unlock() }
        stubs = []
        requests = []
    }

    static func enqueue(_ stub: Stub) {
        lock.lock()
        defer { lock.unlock() }
        stubs.append(stub)
    }

    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }

    override func startLoading() {
        URLProtocolStub.lock.lock()
        URLProtocolStub.requests.append(request)
        guard !URLProtocolStub.stubs.isEmpty else {
            URLProtocolStub.lock.unlock()
            client?.urlProtocol(self, didFailWithError: URLError(.badServerResponse))
            return
        }
        let stub = URLProtocolStub.stubs.removeFirst()
        URLProtocolStub.lock.unlock()

        let response = HTTPURLResponse(
            url: request.url!,
            statusCode: stub.statusCode,
            httpVersion: nil,
            headerFields: stub.headers
        )!
        client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
        client?.urlProtocol(self, didLoad: stub.body)
        client?.urlProtocolDidFinishLoading(self)
    }

    override func stopLoading() {}
}

final class StubGatewayAPI: GatewayAPI {
    var getHomeFeedImpl: ((Theme, Pagination?) async throws -> HomeFeedResponse)?
    var getGuideDetailImpl: ((String) async throws -> GuideDetailResponse)?
    var postRedirectPrepareImpl: ((FeedItemContext) async throws -> RedirectPrepareResponse)?
    var getMeSummaryImpl: (() async throws -> MeSummaryResponse)?
    var issueTokenWithPhoneImpl: ((String, String, String) async throws -> AuthTokenPair)?
    var issueTokenWithWeChatImpl: ((String, String) async throws -> AuthTokenPair)?
    var refreshAuthTokensImpl: (() async throws -> AuthTokenPair)?
    var logoutSessionImpl: ((Bool) async throws -> Void)?

    init() {}

    func getHomeFeed(theme: Theme, pagination: Pagination?) async throws -> HomeFeedResponse {
        if let impl = getHomeFeedImpl { return try await impl(theme, pagination) }
        return HomeFeedResponse(cards: [], nextCursor: nil)
    }

    func getGuideDetail(guideCardId: String) async throws -> GuideDetailResponse {
        if let impl = getGuideDetailImpl { return try await impl(guideCardId) }
        return GuideDetailResponse(guideCardId: guideCardId, title: "", summary: "")
    }

    func postRedirectPrepare(context: FeedItemContext) async throws -> RedirectPrepareResponse {
        if let impl = postRedirectPrepareImpl { return try await impl(context) }
        return RedirectPrepareResponse(landingUrl: "")
    }

    func getMeSummary() async throws -> MeSummaryResponse {
        if let impl = getMeSummaryImpl { return try await impl() }
        return MeSummaryResponse(isLoggedIn: false, favoritesCount: 0, historyCount: 0, consentGranted: false)
    }

    func issueTokenWithPhone(phoneE164: String, otpCode: String, verificationId: String) async throws -> AuthTokenPair {
        if let impl = issueTokenWithPhoneImpl { return try await impl(phoneE164, otpCode, verificationId) }
        return AuthTokenPair(accessToken: "", refreshToken: "", expiresInSeconds: nil, sessionId: nil)
    }

    func issueTokenWithWeChat(providerSubject: String, authorizationCode: String) async throws -> AuthTokenPair {
        if let impl = issueTokenWithWeChatImpl { return try await impl(providerSubject, authorizationCode) }
        return AuthTokenPair(accessToken: "", refreshToken: "", expiresInSeconds: nil, sessionId: nil)
    }

    func refreshAuthTokens() async throws -> AuthTokenPair {
        if let impl = refreshAuthTokensImpl { return try await impl() }
        return AuthTokenPair(accessToken: "", refreshToken: "", expiresInSeconds: nil, sessionId: nil)
    }

    func logoutSession(revokeAllDevices: Bool) async throws {
        if let impl = logoutSessionImpl { return try await impl(revokeAllDevices) }
    }
}

func makeTestSession() -> URLSession {
    let config = URLSessionConfiguration.ephemeral
    config.protocolClasses = [URLProtocolStub.self]
    return URLSession(configuration: config)
}

func waitUntil(
    timeoutNs: UInt64 = 1_000_000_000,
    intervalNs: UInt64 = 20_000_000,
    condition: @escaping () -> Bool
) async -> Bool {
    let deadline = DispatchTime.now().uptimeNanoseconds + timeoutNs
    while DispatchTime.now().uptimeNanoseconds < deadline {
        if condition() { return true }
        try? await Task.sleep(nanoseconds: intervalNs)
    }
    return condition()
}
