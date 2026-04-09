import Combine
import Foundation

@MainActor
public final class LoginFlowViewModel: ObservableObject {
    @Published public var busy = false
    @Published public var errorMessage: String?

    private let api: GatewayAPI

    public init(api: GatewayAPI) {
        self.api = api
    }

    public func loginWithPhone(phoneE164: String, otpCode: String, verificationId: String) async -> Bool {
        errorMessage = nil
        busy = true
        defer { busy = false }
        do {
            _ = try await api.issueTokenWithPhone(phoneE164: phoneE164, otpCode: otpCode, verificationId: verificationId)
            return true
        } catch let urlError as URLError where urlError.code == .notConnectedToInternet {
            errorMessage = "网络不可用"
            return false
        } catch let apiError as GatewayAPIError {
            errorMessage = Self.message(for: apiError)
            return false
        } catch {
            errorMessage = "登录失败，请稍后重试"
            return false
        }
    }

    public func loginWithWeChat(providerSubject: String, authorizationCode: String) async -> Bool {
        errorMessage = nil
        busy = true
        defer { busy = false }
        do {
            _ = try await api.issueTokenWithWeChat(providerSubject: providerSubject, authorizationCode: authorizationCode)
            return true
        } catch let urlError as URLError where urlError.code == .notConnectedToInternet {
            errorMessage = "网络不可用"
            return false
        } catch let apiError as GatewayAPIError {
            errorMessage = Self.message(for: apiError)
            return false
        } catch {
            errorMessage = "登录失败，请稍后重试"
            return false
        }
    }

    private static func message(for error: GatewayAPIError) -> String {
        switch error {
        case .business(let code, let message):
            switch code {
            case 20006: return "验证码或授权信息无效，请重试"
            case 20005: return "当前环境暂不可登录"
            case 10005: return "操作过于频繁，请稍后再试"
            case 10001, 10002: return "请检查手机号与验证码"
            case 90002, 90003: return "服务繁忙，请稍后再试"
            default: return message
            }
        case .decoding: return "数据解析失败"
        case .transport(let underlying): return underlying
        }
    }
}
