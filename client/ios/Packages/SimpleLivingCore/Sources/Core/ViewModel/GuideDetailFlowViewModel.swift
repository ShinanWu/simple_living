import Combine
import Foundation

@MainActor
public final class GuideDetailFlowViewModel: ObservableObject {
    @Published public var state: ViewLoadState<GuideDetailResponse> = .loading

    private let api: GatewayAPI
    private let guideCardId: String

    public init(api: GatewayAPI, guideCardId: String) {
        self.api = api
        self.guideCardId = guideCardId
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
            let response = try await api.getGuideDetail(guideCardId: guideCardId)
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
            state = .error(message: "详情加载失败，请稍后重试")
        }
    }
}
