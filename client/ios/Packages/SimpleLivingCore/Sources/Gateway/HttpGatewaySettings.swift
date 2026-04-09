import Foundation

/// 可变的 HTTP 网关配置（访客会话在首次需要写接口时懒创建）。
public final class HttpGatewaySettings: @unchecked Sendable {
    public let baseURL: URL
    public var accessToken: String?
    private let lock = NSLock()
    private var _refreshToken: String?
    private var _guestSessionId: String?
    public let deviceId: String
    public let appVersion: String
    public let clientPlatform: String

    private static let udAccessKey = "simple_living_access_token"
    private static let udRefreshKey = "simple_living_refresh_token"

    public init(
        baseURL: URL,
        accessToken: String? = nil,
        refreshToken: String? = nil,
        guestSessionId: String? = nil,
        deviceId: String,
        appVersion: String,
        clientPlatform: String = "ios",
        userDefaults: UserDefaults = .standard
    ) {
        self.baseURL = baseURL
        let persisted = Self.loadPersistedTokens(userDefaults: userDefaults)
        self.accessToken = accessToken ?? persisted.access
        self._refreshToken = refreshToken ?? persisted.refresh
        self._guestSessionId = guestSessionId
        self.deviceId = deviceId
        self.appVersion = appVersion
        self.clientPlatform = clientPlatform
    }

    public var refreshToken: String? {
        get {
            lock.lock()
            defer { lock.unlock() }
            return _refreshToken
        }
        set {
            lock.lock()
            defer { lock.unlock() }
            _refreshToken = newValue
        }
    }

    public var guestSessionId: String? {
        get {
            lock.lock()
            defer { lock.unlock() }
            return _guestSessionId
        }
        set {
            lock.lock()
            defer { lock.unlock() }
            _guestSessionId = newValue
        }
    }

    /// 登录成功后写入令牌并清除访客会话头（Bearer 优先于访客）。
    public func applyTokenPair(_ pair: AuthTokenPair, userDefaults: UserDefaults = .standard) {
        lock.lock()
        accessToken = pair.accessToken
        _refreshToken = pair.refreshToken
        _guestSessionId = nil
        lock.unlock()
        Self.persistTokens(access: pair.accessToken, refresh: pair.refreshToken, userDefaults: userDefaults)
    }

    public func clearAuth(userDefaults: UserDefaults = .standard) {
        lock.lock()
        accessToken = nil
        _refreshToken = nil
        lock.unlock()
        userDefaults.removeObject(forKey: Self.udAccessKey)
        userDefaults.removeObject(forKey: Self.udRefreshKey)
    }

    private static func loadPersistedTokens(userDefaults: UserDefaults) -> (access: String?, refresh: String?) {
        (userDefaults.string(forKey: udAccessKey), userDefaults.string(forKey: udRefreshKey))
    }

    private static func persistTokens(access: String, refresh: String, userDefaults: UserDefaults) {
        userDefaults.set(access, forKey: udAccessKey)
        userDefaults.set(refresh, forKey: udRefreshKey)
    }

    /// 稳定设备 ID，便于访客会话与风控对齐。
    public static func resolvedDeviceId(userDefaults: UserDefaults = .standard) -> String {
        let key = "simple_living_device_id"
        if let existing = userDefaults.string(forKey: key) {
            return existing
        }
        let fresh = UUID().uuidString
        userDefaults.set(fresh, forKey: key)
        return fresh
    }
}
