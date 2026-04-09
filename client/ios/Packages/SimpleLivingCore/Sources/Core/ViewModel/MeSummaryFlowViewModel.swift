import Combine
import Foundation

@MainActor
public final class MeSummaryFlowViewModel: ObservableObject {
    @Published public var state: ViewLoadState<MeSummaryResponse> = .loading

    private let api: GatewayAPI

    public init(api: GatewayAPI) {
        self.api = api
    }

    public func onAppear() {
        Task { await load() }
    }

    public func retry() {
        Task { await load() }
    }

    public func refresh() {
        Task { await load() }
    }

    private func load() async {
        state = .loading
        do {
            let summary = try await api.getMeSummary()
            state = .success(summary)
        } catch let urlError as URLError where urlError.code == .notConnectedToInternet {
            state = .offline
        } catch let apiError as GatewayAPIError {
            if case .business(let code, _) = apiError, code == 20002 {
                do {
                    _ = try await api.refreshAuthTokens()
                    let summary = try await api.getMeSummary()
                    state = .success(summary)
                } catch {
                    state = .error(message: "登录已过期，请重新登录")
                }
                return
            }
            switch apiError {
            case .business(_, let message):
                state = .error(message: message)
            case .decoding:
                state = .error(message: "数据解析失败")
            case .transport(let underlying):
                state = .error(message: underlying)
            }
        } catch {
            state = .error(message: "加载失败，请稍后重试")
        }
    }
}
