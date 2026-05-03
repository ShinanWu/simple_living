import XCTest
@testable import SimpleLivingCore

final class ViewModelFlowTests: XCTestCase {
    @MainActor
    func testHomeFlowViewModelSameThemeDoesNotTriggerExtraLoad() async {
        let api = StubGatewayAPI()
        var callCount = 0
        api.getHomeFeedImpl = { _, _ in
            callCount += 1
            return HomeFeedResponse(cards: [], nextCursor: nil)
        }
        let vm = HomeFlowViewModel(api: api)
        vm.onAppear()
        _ = await waitUntil { callCount == 1 }
        vm.onThemeChanged(.clothing)
        try? await Task.sleep(nanoseconds: 60_000_000)
        XCTAssertEqual(callCount, 1)
    }

    @MainActor
    func testHomeFlowViewModelLastThemeRequestWins() async {
        let api = StubGatewayAPI()
        api.getHomeFeedImpl = { theme, _ in
            if theme == .clothing {
                try? await Task.sleep(nanoseconds: 150_000_000)
                return HomeFeedResponse(cards: [
                    HomeCard(
                        id: "old",
                        guideCardId: "g-old",
                        recommendationId: "r-old",
                        scene: "home_feed",
                        itemRank: 1,
                        title: "old",
                        reason: "old"
                    ),
                ], nextCursor: nil)
            }
            return HomeFeedResponse(cards: [
                HomeCard(
                    id: "new",
                    guideCardId: "g-new",
                    recommendationId: "r-new",
                    scene: "home_feed",
                    itemRank: 1,
                    title: "new",
                    reason: "new"
                ),
            ], nextCursor: nil)
        }

        let vm = HomeFlowViewModel(api: api)
        vm.onAppear()
        vm.onThemeChanged(.food)
        let ok = await waitUntil {
            if case .success(let cards) = vm.state {
                return cards.first?.id == "new"
            }
            return false
        }
        XCTAssertTrue(ok)
    }

    @MainActor
    func testHomeFlowViewModelMapsOfflineState() async {
        let api = StubGatewayAPI()
        api.getHomeFeedImpl = { _, _ in throw URLError(.notConnectedToInternet) }
        let vm = HomeFlowViewModel(api: api)
        vm.onAppear()
        let ok = await waitUntil {
            if case .offline = vm.state { return true }
            return false
        }
        XCTAssertTrue(ok)
    }

    @MainActor
    func testGuideDetailFlowMapsBusinessErrorToMessage() async {
        let api = StubGatewayAPI()
        api.getGuideDetailImpl = { _ in
            throw GatewayAPIError.business(code: 30001, message: "not found")
        }
        let vm = GuideDetailFlowViewModel(api: api, guideCardId: "g-1")
        vm.onAppear()

        let ok = await waitUntil {
            if case .error(let message) = vm.state {
                return message == "not found"
            }
            return false
        }
        XCTAssertTrue(ok)
    }

    @MainActor
    func testGuideDetailFlowMapsDecodingErrorMessage() async {
        let api = StubGatewayAPI()
        api.getGuideDetailImpl = { _ in throw GatewayAPIError.decoding(underlying: "bad json") }
        let vm = GuideDetailFlowViewModel(api: api, guideCardId: "g-1")
        vm.onAppear()
        let ok = await waitUntil {
            if case .error(let message) = vm.state {
                return message == "数据解析失败"
            }
            return false
        }
        XCTAssertTrue(ok)
    }

    @MainActor
    func testRedirectPrepareFlowOfflineState() async {
        let api = StubGatewayAPI()
        api.postRedirectPrepareImpl = { _ in throw URLError(.notConnectedToInternet) }
        let vm = RedirectPrepareFlowViewModel(
            api: api,
            context: FeedItemContext(guideCardId: "g", recommendationId: "r", scene: "home_feed", itemRank: 1)
        )
        vm.onAppear()

        let ok = await waitUntil {
            if case .offline = vm.state { return true }
            return false
        }
        XCTAssertTrue(ok)
    }

    @MainActor
    func testRedirectPrepareFlowMapsTransportErrorMessage() async {
        let api = StubGatewayAPI()
        api.postRedirectPrepareImpl = { _ in throw GatewayAPIError.transport(underlying: "upstream down") }
        let vm = RedirectPrepareFlowViewModel(
            api: api,
            context: FeedItemContext(guideCardId: "g", recommendationId: "r", scene: "home_feed", itemRank: 1)
        )
        vm.onAppear()
        let ok = await waitUntil {
            if case .error(let message) = vm.state { return message == "upstream down" }
            return false
        }
        XCTAssertTrue(ok)
    }

    @MainActor
    func testMeSummaryFlowRefreshesOn20002() async {
        let api = StubGatewayAPI()
        var callCount = 0
        api.getMeSummaryImpl = {
            callCount += 1
            if callCount == 1 {
                throw GatewayAPIError.business(code: 20002, message: "expired")
            }
            return MeSummaryResponse(isLoggedIn: true, favoritesCount: 3, historyCount: 4, consentGranted: true)
        }
        api.refreshAuthTokensImpl = {
            AuthTokenPair(accessToken: "a2", refreshToken: "r2", expiresInSeconds: 3600, sessionId: "s2")
        }

        let vm = MeSummaryFlowViewModel(api: api)
        vm.onAppear()
        let ok = await waitUntil {
            if case .success(let summary) = vm.state {
                return summary.isLoggedIn && summary.favoritesCount == 3
            }
            return false
        }
        XCTAssertTrue(ok)
        XCTAssertEqual(callCount, 2)
    }

    @MainActor
    func testMeSummaryFlowShowsReLoginWhenRefreshFails() async {
        let api = StubGatewayAPI()
        api.getMeSummaryImpl = {
            throw GatewayAPIError.business(code: 20002, message: "expired")
        }
        api.refreshAuthTokensImpl = {
            throw GatewayAPIError.business(code: 20003, message: "refresh invalid")
        }
        let vm = MeSummaryFlowViewModel(api: api)
        vm.onAppear()

        let ok = await waitUntil {
            if case .error(let message) = vm.state {
                return message == "登录已过期，请重新登录"
            }
            return false
        }
        XCTAssertTrue(ok)
    }

    @MainActor
    func testLoginFlowPhoneErrorMappingAndBusyReset() async {
        let api = StubGatewayAPI()
        api.issueTokenWithPhoneImpl = { _, _, _ in
            throw GatewayAPIError.business(code: 20006, message: "invalid")
        }
        let vm = LoginFlowViewModel(api: api)

        let ok = await vm.loginWithPhone(phoneE164: "+8613800138000", otpCode: "111111", verificationId: "v1")
        XCTAssertFalse(ok)
        XCTAssertEqual(vm.errorMessage, "验证码或授权信息无效，请重试")
        XCTAssertFalse(vm.busy)
    }

    @MainActor
    func testLoginFlowWeChatSuccess() async {
        let api = StubGatewayAPI()
        api.issueTokenWithWeChatImpl = { _, _ in
            AuthTokenPair(accessToken: "a", refreshToken: "r", expiresInSeconds: 3600, sessionId: "s")
        }
        let vm = LoginFlowViewModel(api: api)
        let ok = await vm.loginWithWeChat(providerSubject: "openid", authorizationCode: "code")
        XCTAssertTrue(ok)
        XCTAssertNil(vm.errorMessage)
        XCTAssertFalse(vm.busy)
    }

    @MainActor
    func testLoginFlowMapsRateLimitMessage() async {
        let api = StubGatewayAPI()
        api.issueTokenWithWeChatImpl = { _, _ in
            throw GatewayAPIError.business(code: 10005, message: "rate limited")
        }
        let vm = LoginFlowViewModel(api: api)
        let ok = await vm.loginWithWeChat(providerSubject: "openid", authorizationCode: "code")
        XCTAssertFalse(ok)
        XCTAssertEqual(vm.errorMessage, "操作过于频繁，请稍后再试")
        XCTAssertFalse(vm.busy)
    }
}
