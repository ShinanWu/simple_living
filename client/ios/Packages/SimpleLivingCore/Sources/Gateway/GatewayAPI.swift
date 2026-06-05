import Foundation

public struct Pagination: Sendable {
    public let cursor: String?
    public let limit: Int

    public init(cursor: String?, limit: Int) {
        self.cursor = cursor
        self.limit = limit
    }
}

public struct HomeCard: Identifiable, Equatable, Hashable, Sendable {
    public let id: String
    public let guideCardId: String
    public let recommendationId: String
    public let scene: String
    public let itemRank: Int
    public let title: String
    public let reason: String
    public let coverUrl: String?

    public var feedContext: FeedItemContext {
        FeedItemContext(
            guideCardId: guideCardId,
            recommendationId: recommendationId,
            scene: scene,
            itemRank: itemRank
        )
    }

    public init(
        id: String,
        guideCardId: String,
        recommendationId: String,
        scene: String,
        itemRank: Int,
        title: String,
        reason: String,
        coverUrl: String? = nil
    ) {
        self.id = id
        self.guideCardId = guideCardId
        self.recommendationId = recommendationId
        self.scene = scene
        self.itemRank = itemRank
        self.title = title
        self.reason = reason
        self.coverUrl = coverUrl
    }
}

public struct HomeFeedResponse: Equatable, Sendable {
    public let cards: [HomeCard]
    public let nextCursor: String?

    public init(cards: [HomeCard], nextCursor: String?) {
        self.cards = cards
        self.nextCursor = nextCursor
    }
}

public struct GuideDetailResponse: Equatable, Sendable {
    public let guideCardId: String
    public let title: String
    public let summary: String
    public let subtitle: String?
    public let coverUrl: String?
    public let galleryUrls: [String]
    public let isCommercial: Bool
    public let disclosureText: String?

    public init(guideCardId: String, title: String, summary: String, subtitle: String? = nil, coverUrl: String? = nil, galleryUrls: [String] = [], isCommercial: Bool = false, disclosureText: String? = nil) {
        self.guideCardId = guideCardId
        self.title = title
        self.summary = summary
        self.subtitle = subtitle
        self.coverUrl = coverUrl
        self.galleryUrls = galleryUrls
        self.isCommercial = isCommercial
        self.disclosureText = disclosureText
    }
}

public struct RedirectPrepareResponse: Equatable, Sendable {
    public let landingUrl: String

    public init(landingUrl: String) {
        self.landingUrl = landingUrl
    }
}

public struct MeSummaryResponse: Equatable, Sendable {
    public let isLoggedIn: Bool
    public let displayName: String?
    public let avatarUrl: String?
    public let favoritesCount: Int
    public let historyCount: Int
    public let consentGranted: Bool

    public init(isLoggedIn: Bool, favoritesCount: Int, historyCount: Int, consentGranted: Bool, displayName: String? = nil, avatarUrl: String? = nil) {
        self.isLoggedIn = isLoggedIn
        self.displayName = displayName
        self.avatarUrl = avatarUrl
        self.favoritesCount = favoritesCount
        self.historyCount = historyCount
        self.consentGranted = consentGranted
    }
}

/// 网关 `POST /api/v2/auth/token/issue` 与 `token/refresh` 返回的令牌对。
public struct AuthTokenPair: Equatable, Sendable {
    public let accessToken: String
    public let refreshToken: String
    public let expiresInSeconds: Int?
    public let sessionId: String?

    public init(accessToken: String, refreshToken: String, expiresInSeconds: Int?, sessionId: String?) {
        self.accessToken = accessToken
        self.refreshToken = refreshToken
        self.expiresInSeconds = expiresInSeconds
        self.sessionId = sessionId
    }
}

public protocol GatewayAPI {
    func getHomeFeed(theme: Theme, pagination: Pagination?) async throws -> HomeFeedResponse
    func getGuideDetail(guideCardId: String) async throws -> GuideDetailResponse
    func postRedirectPrepare(context: FeedItemContext) async throws -> RedirectPrepareResponse
    func getMeSummary() async throws -> MeSummaryResponse

    /// `account_proof.phone_otp`，由网关在 `IssueTokenPair` 前校验验证码。
    func issueTokenWithPhone(phoneE164: String, otpCode: String, verificationId: String) async throws -> AuthTokenPair

    /// `account_proof.oauth`，`provider` 固定为 `wechat`；生产环境 `authorizationCode` 来自微信 SDK。
    func issueTokenWithWeChat(providerSubject: String, authorizationCode: String) async throws -> AuthTokenPair

    func refreshAuthTokens() async throws -> AuthTokenPair

    /// `DELETE /api/v2/auth/session`，成功后应 `clearAuth`（由实现方写入 settings）。
    func logoutSession(revokeAllDevices: Bool) async throws
}
