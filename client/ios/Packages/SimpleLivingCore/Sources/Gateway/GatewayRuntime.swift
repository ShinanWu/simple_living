import Foundation

/// App 入口使用的网关实现选择（Mock 或真实 HTTP），避免 App 模块直接依赖 `HttpGatewayAPI` 构造细节。
public enum GatewayRuntime {
    /// 未设置 `GATEWAY_BASE_URL` 时使用 Mock；否则使用 `HttpGatewayAPI`。
    public static func makeFromEnvironment() -> GatewayAPI {
        let env = ProcessInfo.processInfo.environment
        guard let raw = env["GATEWAY_BASE_URL"]?.trimmingCharacters(in: .whitespacesAndNewlines),
              !raw.isEmpty,
              let url = URL(string: raw) else {
            return MockGatewayAPI()
        }

        let settings = HttpGatewaySettings(
            baseURL: url,
            accessToken: env["GATEWAY_ACCESS_TOKEN"],
            refreshToken: env["GATEWAY_REFRESH_TOKEN"],
            guestSessionId: env["GATEWAY_GUEST_SESSION_ID"],
            deviceId: HttpGatewaySettings.resolvedDeviceId(),
            appVersion: env["GATEWAY_APP_VERSION"] ?? "0.1.0",
            clientPlatform: "ios"
        )
        return HttpGatewayAPI(settings: settings)
    }
}
