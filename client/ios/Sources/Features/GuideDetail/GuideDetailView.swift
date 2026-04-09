import SwiftUI
import SimpleLivingCore

struct GuideDetailView: View {
    let api: GatewayAPI
    let context: FeedItemContext
    @StateObject private var viewModel: GuideDetailFlowViewModel

    init(api: GatewayAPI, context: FeedItemContext) {
        self.api = api
        self.context = context
        _viewModel = StateObject(wrappedValue: GuideDetailFlowViewModel(api: api, guideCardId: context.guideCardId))
    }

    var body: some View {
        content
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .navigationTitle("导购详情")
            .task { viewModel.onAppear() }
    }

    @ViewBuilder
    private var content: some View {
        switch viewModel.state {
        case .loading:
            ProgressView("加载详情中...")
        case .success(let detail):
            VStack(spacing: 12) {
                Text(detail.title).font(.headline)
                Text("guide_card_id: \(detail.guideCardId)")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                Text(detail.summary)
                    .font(.body)
                NavigationLink("去购买") {
                    RedirectPrepareView(api: api, context: context)
                }
                .buttonStyle(.borderedProminent)
            }
            .padding(.horizontal, 16)
        case .empty:
            Text("暂无详情")
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
