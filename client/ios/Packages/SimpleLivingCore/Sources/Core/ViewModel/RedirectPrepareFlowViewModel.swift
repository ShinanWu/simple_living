import Combine
import Foundation

@MainActor
public final class RedirectPrepareFlowViewModel: ObservableObject {
    @Published public var state: ViewLoadState<RedirectPrepareResponse> = .loading

    private let api: GatewayAPI
    private let context: FeedItemContext

    public init(api: GatewayAPI, context: FeedItemContext) {
        self.api = api
        self.context = context
    }

    public func onAppear() {
        Task { await load() }
    }

    public func retry() {
        Task { await load() }
    }

    private func load() async {
        state = .loading
        do {
            let response = try await api.postRedirectPrepare(context: context)
            state = .success(response)
        } catch let urlError as URLError where urlError.code == .notConnectedToInternet {
            state = .offline
        } catch let apiError as GatewayAPIError {
            switch apiError {
            case .business(_, let message):
                state = .error(message: message)
            case .decoding:
                state = .error(message: "数据解析失败")
            case .transport(let underlying):
                state = .error(message: underlying)
            }
        } catch {
            state = .error(message: "跳转准备失败，请稍后重试")
        }
    }
}
