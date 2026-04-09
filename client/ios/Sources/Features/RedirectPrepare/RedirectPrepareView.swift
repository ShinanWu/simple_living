import SwiftUI
import SimpleLivingCore

struct RedirectPrepareView: View {
    let api: GatewayAPI
    let context: FeedItemContext
    @StateObject private var viewModel: RedirectPrepareFlowViewModel

    init(api: GatewayAPI, context: FeedItemContext) {
        self.api = api
        self.context = context
        _viewModel = StateObject(wrappedValue: RedirectPrepareFlowViewModel(api: api, context: context))
    }

    var body: some View {
        content
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .navigationTitle("跳转准备")
            .task { viewModel.onAppear() }
    }

    @ViewBuilder
    private var content: some View {
        switch viewModel.state {
        case .loading:
            ProgressView("生成跳转链接中...")
        case .success(let payload):
            VStack(spacing: 12) {
                Text("即将跳转")
                    .font(.headline)
                Text("guide_card_id: \(context.guideCardId)")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                Text("landing_url")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text(payload.landingUrl)
                    .multilineTextAlignment(.center)
                    .font(.footnote)
            }
            .padding(.horizontal, 16)
        case .empty:
            Text("暂无可跳转链接")
        case .offline:
            VStack(spacing: 8) {
                Text("网络不可用")
                Button("重试") { viewModel.retry() }
            }
        case .error(let message):
            VStack(spacing: 8) {
                Text(message)
                Button("重试") { viewModel.retry() }
            }
        }
    }
}
