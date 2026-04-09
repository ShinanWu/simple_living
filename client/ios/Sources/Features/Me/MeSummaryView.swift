import SwiftUI
import SimpleLivingCore

struct MeSummaryView: View {
    private let api: GatewayAPI
    @StateObject private var viewModel: MeSummaryFlowViewModel
    @State private var showLogin = false

    init(api: GatewayAPI) {
        self.api = api
        _viewModel = StateObject(wrappedValue: MeSummaryFlowViewModel(api: api))
    }

    var body: some View {
        NavigationStack {
            content
                .navigationTitle("我的")
                .toolbar {
                    ToolbarItem(placement: .primaryAction) {
                        if case .success(let s) = viewModel.state, s.isLoggedIn {
                            Button("退出") {
                                Task {
                                    try? await api.logoutSession(revokeAllDevices: false)
                                    viewModel.refresh()
                                }
                            }
                        } else if case .success = viewModel.state {
                            Button("登录") { showLogin = true }
                        }
                    }
                }
                .sheet(isPresented: $showLogin) {
                    LoginView(api: api) {
                        viewModel.refresh()
                    }
                }
                .task { viewModel.onAppear() }
        }
    }

    @ViewBuilder
    private var content: some View {
        switch viewModel.state {
        case .loading:
            ProgressView("加载中...")
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        case .success(let summary):
            List {
                Text(summary.isLoggedIn ? "已登录" : "访客模式")
                Text("收藏: \(summary.favoritesCount)")
                Text("历史: \(summary.historyCount)")
                Text(summary.consentGranted ? "隐私授权: 已同意" : "隐私授权: 未同意")
            }
        case .empty:
            Text("暂无数据").frame(maxWidth: .infinity, maxHeight: .infinity)
        case .error(let message):
            VStack(spacing: 8) {
                Text(message)
                Button("重试") { viewModel.retry() }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        case .offline:
            VStack(spacing: 8) {
                Text("网络不可用")
                Button("重试") { viewModel.retry() }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }
}
