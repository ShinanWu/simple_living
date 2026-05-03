import SwiftUI
import SimpleLivingCore

struct GuideDetailView: View {
    let api: GatewayAPI
    let context: FeedItemContext
    let previewTitle: String
    let previewReason: String
    let theme: Theme
    @StateObject private var viewModel: GuideDetailFlowViewModel
    @State private var currentImageIndex: Int = 0

    init(api: GatewayAPI, context: FeedItemContext, previewTitle: String, previewReason: String, theme: Theme) {
        self.api = api
        self.context = context
        self.previewTitle = previewTitle
        self.previewReason = previewReason
        self.theme = theme
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
            let gallery = themeGallery(theme)
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    TabView(selection: $currentImageIndex) {
                        ForEach(Array(gallery.enumerated()), id: \.offset) { idx, item in
                            RoundedRectangle(cornerRadius: 18, style: .continuous)
                                .fill(
                                    LinearGradient(
                                        colors: [
                                            item.tint.opacity(0.26),
                                            item.tint.opacity(0.1)
                                        ],
                                        startPoint: .topLeading,
                                        endPoint: .bottomTrailing
                                    )
                                )
                                .overlay {
                                    VStack(spacing: 10) {
                                        Image(systemName: item.symbol)
                                            .font(.system(size: 62, weight: .semibold))
                                            .foregroundStyle(item.tint.opacity(0.88))
                                        Text(item.caption)
                                            .font(.subheadline.weight(.medium))
                                            .foregroundStyle(.secondary)
                                    }
                                }
                                .tag(idx)
                        }
                    }
                    .frame(height: 300)
                    .tabViewStyle(.page(indexDisplayMode: .automatic))

                    HStack {
                        Spacer()
                        Text("\(currentImageIndex + 1)/\(gallery.count)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }

                    Text(detail.title.isEmpty ? previewTitle : detail.title)
                        .font(.title3.weight(.semibold))

                    VStack(alignment: .leading, spacing: 8) {
                        Text("推荐理由")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(.secondary)
                        Text(previewReason == "-" ? "为你精选的轻量推荐" : previewReason)
                            .font(.body)
                            .foregroundStyle(.primary)
                    }

                    VStack(alignment: .leading, spacing: 8) {
                        Text("完整内容")
                            .font(.caption.weight(.semibold))
                            .foregroundStyle(.secondary)
                        Text(detail.summary)
                            .font(.body)
                            .foregroundStyle(.primary)
                    }

                    NavigationLink("去购买") {
                        RedirectPrepareView(api: api, context: context)
                    }
                    .buttonStyle(.borderedProminent)
                }
                .padding(.horizontal, 16)
                .padding(.vertical, 12)
            }
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

    private func themeAccent(_ theme: Theme) -> Color {
        switch theme {
        case .clothing:
            return .pink
        case .food:
            return .orange
        case .housing:
            return .mint
        case .transport:
            return .blue
        }
    }

    private func themeGallery(_ theme: Theme) -> [(symbol: String, caption: String, tint: Color)] {
        switch theme {
        case .clothing:
            return [
                ("tshirt", "日常通勤搭配", .pink),
                ("shoe.2", "舒适鞋履选择", .purple),
                ("bag", "轻便随身单品", .indigo),
            ]
        case .food:
            return [
                ("fork.knife", "轻负担三餐", .orange),
                ("takeoutbag.and.cup.and.straw", "一周菜单灵感", .yellow),
                ("cup.and.saucer", "下午茶时刻", .brown),
            ]
        case .housing:
            return [
                ("house", "小空间整理", .mint),
                ("lamp.table", "氛围照明建议", .teal),
                ("bed.double", "舒眠卧室布置", .cyan),
            ]
        case .transport:
            return [
                ("car", "通勤路线优化", .blue),
                ("tram", "公共交通衔接", .indigo),
                ("bicycle", "短途低碳出行", .green),
            ]
        }
    }
}
