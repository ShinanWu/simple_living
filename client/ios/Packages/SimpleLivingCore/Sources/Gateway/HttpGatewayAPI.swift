import Foundation

/// 刷新令牌的单飞任务，防止并发重复刷新
private var refreshInFlight: Task<AuthTokenPair, Error>? = nil

/// 真实 HTTPS + JSON 网关客户端；字段语义对齐 `services/gateway/api.md`。
public final class HttpGatewayAPI: GatewayAPI {
    private let settings: HttpGatewaySettings
    private let session: URLSession

    public init(settings: HttpGatewaySettings, session: URLSession = .shared) {
        self.settings = settings
        self.session = session
    }

    public func getHomeFeed(theme: Theme, pagination: Pagination?) async throws -> HomeFeedResponse {
        var components = URLComponents(
            url: settings.baseURL.appendingPathComponent("api/v2/pages/home_feed"),
            resolvingAgainstBaseURL: false
        )!
        var items: [URLQueryItem] = [
            URLQueryItem(name: "limit", value: String(pagination?.limit ?? 20)),
        ]
        if let cursor = pagination?.cursor, !cursor.isEmpty {
            items.append(URLQueryItem(name: "cursor", value: cursor))
        }
        items.append(URLQueryItem(name: "theme", value: theme.rawValue))
        components.queryItems = items

        let (data, _) = try await dataTask(components.url!, method: "GET", body: nil, jsonBody: false)
        let envelope = try decode(ApiEnvelopeDTO<HomeFeedDataDTO>.self, from: data)
        try throwIfNeeded(envelope)
        guard let payload = envelope.data else {
            throw GatewayAPIError.business(code: envelope.code, message: envelope.message)
        }

        let cards = payload.items.map { item in
            let title = item.guideCard?.title ?? item.guideCardId
            let reason = (item.reasonTags ?? []).joined(separator: " / ")
            let id = "\(item.recommendationId)-\(item.rank)"
            let coverUrl = item.guideCard?.coverUrl ?? item.guideCard?.coverMedia?.url
            return HomeCard(
                id: id,
                guideCardId: item.guideCardId,
                recommendationId: item.recommendationId,
                scene: item.scene,
                itemRank: item.rank,
                title: title,
                reason: reason.isEmpty ? "-" : reason,
                coverUrl: coverUrl
            )
        }

        let next = payload.pagination?.nextCursor
        return HomeFeedResponse(cards: cards, nextCursor: next)
    }

    public func getGuideDetail(guideCardId: String) async throws -> GuideDetailResponse {
        var components = URLComponents(
            url: settings.baseURL.appendingPathComponent("api/v2/pages/guide_detail"),
            resolvingAgainstBaseURL: false
        )!
        components.queryItems = [
            URLQueryItem(name: "guide_card_id", value: guideCardId),
            URLQueryItem(name: "include_related", value: "true"),
        ]

        let (data, _) = try await dataTask(components.url!, method: "GET", body: nil, jsonBody: false)
        let envelope = try decode(ApiEnvelopeDTO<GuideDetailDataDTO>.self, from: data)
        try throwIfNeeded(envelope)
        guard let guide = envelope.data?.guide else {
            throw GatewayAPIError.business(code: envelope.code, message: envelope.message)
        }

        let summary = guide.summary ?? guide.subtitle ?? ""
        let coverUrl = guide.coverUrl ?? guide.coverMedia?.url
        let galleryUrls: [String] = (coverUrl != nil) ? [coverUrl!] : []
        
        return GuideDetailResponse(
            guideCardId: guide.guideCardId,
            title: guide.title,
            summary: summary.isEmpty ? "暂无摘要" : summary,
            subtitle: guide.subtitle,
            coverUrl: coverUrl,
            galleryUrls: galleryUrls,
            isCommercial: guide.isCommercial ?? false,
            disclosureText: guide.disclosureTextKey
        )
    }

    public func postRedirectPrepare(context: FeedItemContext) async throws -> RedirectPrepareResponse {
        try await ensureGuestSessionIfNeeded()

        let url = settings.baseURL.appendingPathComponent("api/v2/pages/redirect_prepare")
        let body = RedirectPrepareBodyDTO(
            guideCardId: context.guideCardId,
            recommendationId: context.recommendationId,
            scene: context.scene,
            itemRank: context.itemRank
        )
        let encoded = try encoder.encode(body)

        let (data, _) = try await dataTask(url, method: "POST", body: encoded, jsonBody: true)
        let envelope = try decode(ApiEnvelopeDTO<RedirectPrepareDataDTO>.self, from: data)
        try throwIfNeeded(envelope)
        guard let landing = envelope.data?.landingUrl else {
            throw GatewayAPIError.business(code: envelope.code, message: envelope.message)
        }
        return RedirectPrepareResponse(landingUrl: landing)
    }

    public func getMeSummary() async throws -> MeSummaryResponse {
        try await ensureGuestSessionIfNeeded()

        let url = settings.baseURL.appendingPathComponent("api/v2/pages/me_summary")
        let (data, _) = try await dataTask(url, method: "GET", body: nil, jsonBody: false, authMode: .bearerOrGuest)
        let envelope = try decode(ApiEnvelopeDTO<MeSummaryDataDTO>.self, from: data)
        try throwIfNeeded(envelope)
        guard let d = envelope.data else {
            throw GatewayAPIError.business(code: envelope.code, message: envelope.message)
        }

        let isGuest = d.profile.isGuest ?? true
        let consent = d.consent?.personalizationAllowed ?? false

        return MeSummaryResponse(
            isLoggedIn: !isGuest,
            favoritesCount: d.counts.favoritesCount,
            historyCount: d.counts.historyCount,
            consentGranted: consent,
            displayName: d.profile.displayName,
            avatarUrl: d.profile.avatarUrl
        )
    }

    public func issueTokenWithPhone(phoneE164: String, otpCode: String, verificationId: String) async throws
        -> AuthTokenPair
    {
        let body = IssueTokenPhoneBodyDTO(
            accountProof: IssueTokenPhoneBodyDTO.AccountProof(phoneOtp: .init(
                phoneE164: phoneE164,
                otpCode: otpCode,
                verificationId: verificationId
            )),
            clientPlatform: settings.clientPlatform,
            deviceId: settings.deviceId,
            appVersion: settings.appVersion
        )
        return try await postIssueToken(body: body)
    }

    public func issueTokenWithWeChat(providerSubject: String, authorizationCode: String) async throws -> AuthTokenPair {
        let body = IssueTokenWeChatBodyDTO(
            accountProof: IssueTokenWeChatBodyDTO.AccountProof(oauth: .init(
                provider: "wechat",
                providerSubject: providerSubject,
                authorizationCode: authorizationCode
            )),
            clientPlatform: settings.clientPlatform,
            deviceId: settings.deviceId,
            appVersion: settings.appVersion
        )
        return try await postIssueToken(body: body)
    }

    public func refreshAuthTokens() async throws -> AuthTokenPair {
        guard let refresh = settings.refreshToken, !refresh.isEmpty else {
            throw GatewayAPIError.business(code: 20003, message: "no refresh token")
        }
        
        if let existing = refreshInFlight {
            return try await existing.value
        }
        
        let task = Task<AuthTokenPair, Error> {
            let url = settings.baseURL.appendingPathComponent("api/v2/auth/token/refresh")
            let payload = RefreshTokenBodyDTO(
                refreshToken: refresh,
                requestContext: RpcRequestContextDTO(
                    clientPlatform: settings.clientPlatform,
                    appVersion: settings.appVersion,
                    deviceId: settings.deviceId
                )
            )
            let encoded = try encoder.encode(payload)
            let (data, _) = try await dataTask(url, method: "POST", body: encoded, jsonBody: true, authMode: .none)
            let envelope = try decode(ApiEnvelopeDTO<TokenPairDataDTO>.self, from: data)
            if envelope.success, envelope.code == 0, let d = envelope.data {
                let pair = authPair(from: d)
                settings.applyTokenPair(pair)
                return pair
            }
            throw GatewayAPIError.business(code: envelope.code, message: envelope.message)
        }
        
        refreshInFlight = task
        
        do {
            let pair = try await task.value
            refreshInFlight = nil
            return pair
        } catch {
            refreshInFlight = nil
            throw error
        }
    }

    public func logoutSession(revokeAllDevices: Bool) async throws {
        defer {
            settings.clearAuth()
            settings.guestSessionId = nil
        }
        guard settings.accessToken != nil else { return }
        let url = settings.baseURL.appendingPathComponent("api/v2/auth/session")
        let payload = LogoutBodyDTO(revokeScope: revokeAllDevices ? "all_user_sessions" : "single_session")
        let encoded = try encoder.encode(payload)
        let (data, _) = try await dataTask(url, method: "DELETE", body: encoded, jsonBody: true, authMode: .bearerOrGuest)
        let envelope = try decode(ApiEnvelopeDTO<LogoutDataDTO>.self, from: data)
        try throwIfNeeded(envelope)
    }

    // MARK: - Private

    private var decoder: JSONDecoder {
        let d = JSONDecoder()
        d.keyDecodingStrategy = .convertFromSnakeCase
        return d
    }

    private var encoder: JSONEncoder {
        let e = JSONEncoder()
        e.keyEncodingStrategy = .convertToSnakeCase
        return e
    }

    private func decode<T: Decodable>(_ type: T.Type, from data: Data) throws -> T {
        do {
            return try decoder.decode(T.self, from: data)
        } catch {
            throw GatewayAPIError.decoding(underlying: String(describing: error))
        }
    }

    private func throwIfNeeded<T>(_ envelope: ApiEnvelopeDTO<T>) throws {
        guard envelope.success, envelope.code == 0 else {
            throw GatewayAPIError.business(code: envelope.code, message: envelope.message)
        }
    }

    private enum AuthHeaderMode {
        case none
        case bearerOrGuest
    }

    private func postIssueToken<Body: Encodable>(body: Body) async throws -> AuthTokenPair {
        let url = settings.baseURL.appendingPathComponent("api/v2/auth/token/issue")
        let encoded = try encoder.encode(body)
        let (data, _) = try await dataTask(url, method: "POST", body: encoded, jsonBody: true, authMode: .none)
        let envelope = try decode(ApiEnvelopeDTO<TokenPairDataDTO>.self, from: data)
        if envelope.success, envelope.code == 0, let d = envelope.data {
            let pair = authPair(from: d)
            settings.applyTokenPair(pair)
            return pair
        }
        throw GatewayAPIError.business(code: envelope.code, message: envelope.message)
    }

    private func authPair(from d: TokenPairDataDTO) -> AuthTokenPair {
        AuthTokenPair(
            accessToken: d.accessToken,
            refreshToken: d.refreshToken,
            expiresInSeconds: d.expiresIn,
            sessionId: d.sessionId
        )
    }

    private func dataTask(
        _ url: URL,
        method: String,
        body: Data?,
        jsonBody: Bool,
        authMode: AuthHeaderMode = .bearerOrGuest,
        retried: Bool = false
    ) async throws -> (Data, URLResponse) {
        var request = URLRequest(url: url)
        request.httpMethod = method
        if jsonBody {
            request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        }
        switch authMode {
        case .none:
            break
        case .bearerOrGuest:
            if let token = settings.accessToken {
                request.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
            } else if let guest = settings.guestSessionId {
                request.setValue(guest, forHTTPHeaderField: "X-Guest-Session-Id")
            }
        }

        request.httpBody = body

        do {
            let (data, response) = try await session.data(for: request)
            
            if !retried, authMode == .bearerOrGuest, settings.refreshToken != nil {
                let envelope = try? decoder.decode(ApiEnvelopeDTO<EmptyDataDTO>.self, from: data)
                if envelope?.code == 20002 {
                    do {
                        _ = try await refreshAuthTokens()
                        return try await dataTask(url, method: method, body: body, jsonBody: jsonBody, authMode: authMode, retried: true)
                    } catch {
                        settings.clearAuth()
                        throw GatewayAPIError.business(code: 20003, "refresh token invalid")
                    }
                }
            }
            
            return (data, response)
        } catch let urlError as URLError where urlError.code == .notConnectedToInternet {
            throw urlError
        } catch let apiError as GatewayAPIError where apiError.localizedDescription.contains("20003") {
            throw apiError
        } catch {
            throw GatewayAPIError.transport(underlying: error.localizedDescription)
        }
    }

    private func ensureGuestSessionIfNeeded() async throws {
        if settings.accessToken != nil { return }
        if settings.guestSessionId != nil { return }

        let url = settings.baseURL.appendingPathComponent("api/v2/guest/session")
        let payload = GuestSessionBodyDTO(
            deviceId: settings.deviceId,
            clientPlatform: settings.clientPlatform,
            appVersion: settings.appVersion
        )
        let data = try encoder.encode(payload)
        let (respData, _) = try await dataTask(url, method: "POST", body: data, jsonBody: true, authMode: .none)
        let envelope = try decode(ApiEnvelopeDTO<GuestSessionDataDTO>.self, from: respData)
        try throwIfNeeded(envelope)
        guard let sessionId = envelope.data?.sessionId else {
            throw GatewayAPIError.business(code: envelope.code, message: envelope.message)
        }
        settings.guestSessionId = sessionId
    }
}

// MARK: - DTOs

private struct EmptyDataDTO: Decodable {}

private struct ApiEnvelopeDTO<T: Decodable>: Decodable {
    let success: Bool
    let code: Int
    let message: String
    let data: T?
}

private struct HomeFeedDataDTO: Decodable {
    let items: [HomeFeedItemDTO]
    let pagination: PaginationDTO?
}

private struct HomeFeedItemDTO: Decodable {
    let recommendationId: String
    let scene: String
    let rank: Int
    let guideCardId: String
    let reasonTags: [String]?
    let guideCard: GuideCardSnippetDTO?
}

private struct GuideCardSnippetDTO: Decodable {
    let title: String?
    let summary: String?
    let subtitle: String?
    let coverUrl: String?
    let coverMedia: CoverMediaDTO?
    
    struct CoverMediaDTO: Decodable {
        let url: String?
    }
}

private struct PaginationDTO: Decodable {
    let nextCursor: String?
    let hasMore: Bool?
    let limit: Int?
}

private struct GuideDetailDataDTO: Decodable {
    let guide: GuideCardDetailDTO
}

private struct GuideCardDetailDTO: Decodable {
    let guideCardId: String
    let title: String
    let summary: String?
    let subtitle: String?
    let coverUrl: String?
    let coverMedia: CoverMediaDTO?
    let isCommercial: Bool?
    let disclosureTextKey: String?
    
    struct CoverMediaDTO: Decodable {
        let url: String?
    }
}

private struct RedirectPrepareBodyDTO: Encodable {
    let guideCardId: String
    let recommendationId: String
    let scene: String
    let itemRank: Int
}

private struct RedirectPrepareDataDTO: Decodable {
    let landingUrl: String
}

private struct MeSummaryDataDTO: Decodable {
    let profile: ProfileDTO
    let counts: CountsDTO
    let consent: ConsentDTO?
}

private struct ProfileDTO: Decodable {
    let userId: String?
    let isGuest: Bool?
    let displayName: String?
    let avatarUrl: String?
}

private struct CountsDTO: Decodable {
    let favoritesCount: Int
    let historyCount: Int
}

private struct ConsentDTO: Decodable {
    let personalizationAllowed: Bool?
}

private struct GuestSessionBodyDTO: Encodable {
    let deviceId: String
    let clientPlatform: String
    let appVersion: String?
}

private struct GuestSessionDataDTO: Decodable {
    let sessionId: String
}

private struct RpcRequestContextDTO: Encodable {
    let clientPlatform: String
    let appVersion: String?
    let deviceId: String
}

private struct IssueTokenPhoneBodyDTO: Encodable {
    let accountProof: AccountProof
    let clientPlatform: String
    let deviceId: String
    let appVersion: String?

    struct AccountProof: Encodable {
        let phoneOtp: PhoneOtp

        struct PhoneOtp: Encodable {
            let phoneE164: String
            let otpCode: String
            let verificationId: String
        }
    }
}

private struct IssueTokenWeChatBodyDTO: Encodable {
    let accountProof: AccountProof
    let clientPlatform: String
    let deviceId: String
    let appVersion: String?

    struct AccountProof: Encodable {
        let oauth: OAuth

        struct OAuth: Encodable {
            let provider: String
            let providerSubject: String
            let authorizationCode: String
        }
    }
}

private struct RefreshTokenBodyDTO: Encodable {
    let refreshToken: String
    let requestContext: RpcRequestContextDTO
}

private struct TokenPairDataDTO: Decodable {
    let accessToken: String
    let refreshToken: String
    let expiresIn: Int?
    let sessionId: String?
}

private struct LogoutBodyDTO: Encodable {
    let revokeScope: String
}

private struct LogoutDataDTO: Decodable {
    let revoked: Bool?
}
