import Foundation

public final class MockGatewayAPI: GatewayAPI {
    public var shouldSimulateOffline: Bool = false
    private var loggedIn: Bool = false
    private var mockRefreshToken: String = ""

    public init(shouldSimulateOffline: Bool = false) {
        self.shouldSimulateOffline = shouldSimulateOffline
    }

    public func getHomeFeed(theme: Theme, pagination: Pagination?) async throws -> HomeFeedResponse {
        try await Task.sleep(nanoseconds: 200_000_000)
        if shouldSimulateOffline {
            throw URLError(.notConnectedToInternet)
        }

        let recId = "rec_\(theme.rawValue)_page1"
        let cards = [
            HomeCard(
                id: "\(recId)-1",
                guideCardId: "guide_\(theme.rawValue)_001",
                recommendationId: recId,
                scene: "home_feed",
                itemRank: 1,
                title: "\(theme.title)主题精选 1",
                reason: "基于主题与基础偏好生成"
            ),
            HomeCard(
                id: "\(recId)-2",
                guideCardId: "guide_\(theme.rawValue)_002",
                recommendationId: recId,
                scene: "home_feed",
                itemRank: 2,
                title: "\(theme.title)主题精选 2",
                reason: "结合历史行为做轻量排序"
            ),
        ]

        return HomeFeedResponse(cards: cards, nextCursor: nil)
    }

    public func getGuideDetail(guideCardId: String) async throws -> GuideDetailResponse {
        try await Task.sleep(nanoseconds: 150_000_000)
        return GuideDetailResponse(
            guideCardId: guideCardId,
            title: "导购详情（Mock）",
            summary: "用于承接详情页布局与字段映射。"
        )
    }

    public func postRedirectPrepare(context: FeedItemContext) async throws -> RedirectPrepareResponse {
        try await Task.sleep(nanoseconds: 120_000_000)
        return RedirectPrepareResponse(
            landingUrl: "https://example.com/landing/\(context.guideCardId)?rec=\(context.recommendationId)"
        )
    }

    public func getMeSummary() async throws -> MeSummaryResponse {
        try await Task.sleep(nanoseconds: 150_000_000)
        return MeSummaryResponse(
            isLoggedIn: loggedIn,
            favoritesCount: loggedIn ? 3 : 0,
            historyCount: loggedIn ? 5 : 0,
            consentGranted: true
        )
    }

    public func issueTokenWithPhone(phoneE164: String, otpCode: String, verificationId: String) async throws -> AuthTokenPair {
        try await Task.sleep(nanoseconds: 150_000_000)
        if shouldSimulateOffline {
            throw URLError(.notConnectedToInternet)
        }
        loggedIn = true
        mockRefreshToken = "mock_rt_phone"
        return AuthTokenPair(
            accessToken: "mock_at_phone",
            refreshToken: mockRefreshToken,
            expiresInSeconds: 3600,
            sessionId: "sess_mock"
        )
    }

    public func issueTokenWithWeChat(providerSubject: String, authorizationCode: String) async throws -> AuthTokenPair {
        try await Task.sleep(nanoseconds: 150_000_000)
        if shouldSimulateOffline {
            throw URLError(.notConnectedToInternet)
        }
        loggedIn = true
        mockRefreshToken = "mock_rt_wx"
        return AuthTokenPair(
            accessToken: "mock_at_wx",
            refreshToken: mockRefreshToken,
            expiresInSeconds: 3600,
            sessionId: "sess_mock"
        )
    }

    public func refreshAuthTokens() async throws -> AuthTokenPair {
        try await Task.sleep(nanoseconds: 80_000_000)
        if mockRefreshToken.isEmpty {
            throw GatewayAPIError.business(code: 20003, message: "invalid refresh token")
        }
        mockRefreshToken = "mock_rt_rotated"
        return AuthTokenPair(
            accessToken: "mock_at_refreshed",
            refreshToken: mockRefreshToken,
            expiresInSeconds: 3600,
            sessionId: "sess_mock"
        )
    }

    public func logoutSession(revokeAllDevices: Bool) async throws {
        try await Task.sleep(nanoseconds: 80_000_000)
        loggedIn = false
        mockRefreshToken = ""
    }
}
