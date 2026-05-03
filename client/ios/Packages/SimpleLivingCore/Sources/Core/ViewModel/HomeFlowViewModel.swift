import Combine
import Foundation

@MainActor
public final class HomeFlowViewModel: ObservableObject {
    @Published public var selectedTheme: Theme = .clothing
    @Published public var state: ViewLoadState<[HomeCard]> = .loading

    private let api: GatewayAPI
    private var hasLoaded = false
    private var loadTask: Task<Void, Never>?
    private var latestRequestID: UInt64 = 0

    public init(api: GatewayAPI) {
        self.api = api
    }

    public func onAppear() {
        guard !hasLoaded else { return }
        hasLoaded = true
        scheduleLoad(theme: selectedTheme)
    }

    public func onThemeChanged(_ theme: Theme) {
        guard selectedTheme != theme else { return }
        selectedTheme = theme
        scheduleLoad(theme: theme)
    }

    public func retry() {
        scheduleLoad(theme: selectedTheme)
    }

    public func refreshRecommendations() {
        scheduleLoad(theme: selectedTheme)
    }

    deinit {
        loadTask?.cancel()
    }

    private func scheduleLoad(theme: Theme) {
        latestRequestID &+= 1
        let requestID = latestRequestID
        loadTask?.cancel()
        loadTask = Task { [weak self] in
            await self?.load(theme: theme, requestID: requestID)
        }
    }

    private func load(theme: Theme, requestID: UInt64) async {
        state = .loading
        do {
            let response = try await api.getHomeFeed(
                theme: theme,
                pagination: Pagination(cursor: nil, limit: 20)
            )
            guard !Task.isCancelled, requestID == latestRequestID else { return }
            state = response.cards.isEmpty ? .empty : .success(response.cards)
        } catch is CancellationError {
            return
        } catch let urlError as URLError where urlError.code == .notConnectedToInternet {
            guard !Task.isCancelled, requestID == latestRequestID else { return }
            state = .offline
        } catch let apiError as GatewayAPIError {
            guard !Task.isCancelled, requestID == latestRequestID else { return }
            switch apiError {
            case .business(_, let message):
                state = .error(message: message)
            case .decoding:
                state = .error(message: "数据解析失败")
            case .transport(let underlying):
                state = .error(message: underlying)
            }
        } catch {
            guard !Task.isCancelled, requestID == latestRequestID else { return }
            state = .error(message: "加载失败，请稍后重试")
        }
    }
}
