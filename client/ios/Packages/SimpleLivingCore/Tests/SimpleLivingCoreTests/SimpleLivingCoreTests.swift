import XCTest
@testable import SimpleLivingCore

final class SimpleLivingCoreTests: XCTestCase {
    func testThemeOrderAndRawValues() {
        XCTAssertEqual(Theme.allCases.map(\.rawValue), ["clothing", "food", "housing", "transport"])
        XCTAssertEqual(Theme.allCases.map(\.title), ["衣", "食", "住", "行"])
    }

    func testMockGatewayReturnsCards() async throws {
        let api = MockGatewayAPI()
        let result = try await api.getHomeFeed(theme: .clothing, pagination: Pagination(cursor: nil, limit: 20))
        XCTAssertFalse(result.cards.isEmpty)
        XCTAssertEqual(result.cards.first?.guideCardId, "guide_clothing_001")
        XCTAssertEqual(result.cards.first?.recommendationId, "rec_clothing_page1")
    }

    func testMockLoginThenMeSummaryShowsLoggedIn() async throws {
        let api = MockGatewayAPI()
        _ = try await api.issueTokenWithPhone(phoneE164: "+8613800138000", otpCode: "123456", verificationId: "v1")
        let me = try await api.getMeSummary()
        XCTAssertTrue(me.isLoggedIn)
    }

    @MainActor
    func testHomeFlowViewModelTransitionsToSuccess() async {
        let vm = HomeFlowViewModel(api: MockGatewayAPI())
        vm.onAppear()
        try? await Task.sleep(nanoseconds: 350_000_000)

        switch vm.state {
        case .success(let cards):
            XCTAssertFalse(cards.isEmpty)
        default:
            XCTFail("Expected success state")
        }
    }

    @MainActor
    func testRedirectPrepareFlowReturnsLandingUrl() async {
        let ctx = FeedItemContext(
            guideCardId: "guide-1",
            recommendationId: "rec-1",
            scene: "home_feed",
            itemRank: 1
        )
        let vm = RedirectPrepareFlowViewModel(api: MockGatewayAPI(), context: ctx)
        vm.onAppear()
        try? await Task.sleep(nanoseconds: 250_000_000)

        switch vm.state {
        case .success(let payload):
            XCTAssertTrue(payload.landingUrl.contains("guide-1"))
        default:
            XCTFail("Expected success state")
        }
    }
}
