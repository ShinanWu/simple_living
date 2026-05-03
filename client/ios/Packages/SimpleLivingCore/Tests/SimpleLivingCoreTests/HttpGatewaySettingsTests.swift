import XCTest
@testable import SimpleLivingCore

final class HttpGatewaySettingsTests: XCTestCase {
    func testApplyTokenPairClearsGuestAndPersistsTokens() {
        let defaults = UserDefaults(suiteName: "HttpGatewaySettingsTests.apply")!
        defaults.removePersistentDomain(forName: "HttpGatewaySettingsTests.apply")
        let settings = HttpGatewaySettings(
            baseURL: URL(string: "https://example.com")!,
            guestSessionId: "guest_1",
            deviceId: "dev-1",
            appVersion: "1.0.0",
            userDefaults: defaults
        )

        settings.applyTokenPair(
            AuthTokenPair(accessToken: "a1", refreshToken: "r1", expiresInSeconds: 3600, sessionId: "s1"),
            userDefaults: defaults
        )

        XCTAssertEqual(settings.accessToken, "a1")
        XCTAssertEqual(settings.refreshToken, "r1")
        XCTAssertNil(settings.guestSessionId)
        XCTAssertEqual(defaults.string(forKey: "simple_living_access_token"), "a1")
        XCTAssertEqual(defaults.string(forKey: "simple_living_refresh_token"), "r1")
    }

    func testClearAuthRemovesPersistedTokens() {
        let defaults = UserDefaults(suiteName: "HttpGatewaySettingsTests.clear")!
        defaults.set("a1", forKey: "simple_living_access_token")
        defaults.set("r1", forKey: "simple_living_refresh_token")

        let settings = HttpGatewaySettings(
            baseURL: URL(string: "https://example.com")!,
            accessToken: "a1",
            refreshToken: "r1",
            deviceId: "dev-1",
            appVersion: "1.0.0",
            userDefaults: defaults
        )
        settings.clearAuth(userDefaults: defaults)

        XCTAssertNil(settings.accessToken)
        XCTAssertNil(settings.refreshToken)
        XCTAssertNil(defaults.string(forKey: "simple_living_access_token"))
        XCTAssertNil(defaults.string(forKey: "simple_living_refresh_token"))
    }

    func testInitLoadsPersistedTokensWhenArgumentsMissing() {
        let defaults = UserDefaults(suiteName: "HttpGatewaySettingsTests.init")!
        defaults.set("persisted_a", forKey: "simple_living_access_token")
        defaults.set("persisted_r", forKey: "simple_living_refresh_token")

        let settings = HttpGatewaySettings(
            baseURL: URL(string: "https://example.com")!,
            deviceId: "dev-1",
            appVersion: "1.0.0",
            userDefaults: defaults
        )

        XCTAssertEqual(settings.accessToken, "persisted_a")
        XCTAssertEqual(settings.refreshToken, "persisted_r")
    }

    func testResolvedDeviceIdIsStablePerUserDefaults() {
        let defaults = UserDefaults(suiteName: "HttpGatewaySettingsTests.device")!
        defaults.removePersistentDomain(forName: "HttpGatewaySettingsTests.device")

        let first = HttpGatewaySettings.resolvedDeviceId(userDefaults: defaults)
        let second = HttpGatewaySettings.resolvedDeviceId(userDefaults: defaults)

        XCTAssertEqual(first, second)
        XCTAssertFalse(first.isEmpty)
    }
}
