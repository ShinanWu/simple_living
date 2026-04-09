import Combine
import Foundation

@MainActor
public final class HomeFlowViewModel: ObservableObject {
    @Published public var selectedTheme: Theme = .clothing
    @Published public var state: ViewLoadState<[HomeCard]> = .loading

    private let api: GatewayAPI
    private var hasLoaded = false

    public init(api: GatewayAPI) {
        self.api = api
    }

    public func onAppear() {
        guard !hasLoaded else { return }
        hasLoaded = true
        Task { await load(theme: selectedTheme) }
    }

    public func onThemeChanged(_ theme: Theme) {
        selectedTheme = theme
        Task { await load(theme: theme) }
    }

    public func retry() {
        Task { await load(theme: selectedTheme) }
    }

    public func refreshRecommendations() {
        Task { await load(theme: selectedTheme) }
    }

    private func load(theme: Theme) async {
        state = .loading
        do {
            let response = try await api.getHomeFeed(
                theme: theme,
                pagination: Pagination(cursor: nil, limit: 20)
            )
            state = response.cards.isEmpty ? .empty : .success(response.cards)
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
            state = .error(message: "加载失败，请稍后重试")
        }
    }
}
