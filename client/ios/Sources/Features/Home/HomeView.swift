import SwiftUI
import SimpleLivingCore

struct HomeView: View {
    @StateObject private var viewModel: HomeFlowViewModel
    private let api: GatewayAPI
    @Namespace private var themeTabNamespace
    @State private var currentIndex: Int = 0
    @State private var pullOffset: CGFloat = 0
    @State private var isRefreshing = false
    @State private var themeFocusPulse = false
    @State private var activeCard: HomeCard?
    @State private var isCardDragging = false
    @State private var cardDragOffset: CGFloat = 0
    @State private var themePulseTask: Task<Void, Never>?
    @State private var dragResetTask: Task<Void, Never>?
    @State private var showSearchPlaceholder = false

    private let nextCardThreshold: CGFloat = 90
    private let refreshHintThreshold: CGFloat = 28
    private let refreshTriggerThreshold: CGFloat = 92
    private let themeSwipeThreshold: CGFloat = 54

    init(api: GatewayAPI) {
        self.api = api
        _viewModel = StateObject(wrappedValue: HomeFlowViewModel(api: api))
    }

    var body: some View {
        VStack(spacing: 0) {
            headerView
                .padding(.horizontal, 16)
                .padding(.top, 12)

            content
                .padding(.horizontal, 16)
                .padding(.top, 12)
                .frame(maxWidth: .infinity, maxHeight: .infinity)

            bottomThemeTabs
                .padding(.horizontal, 16)
                .padding(.top, 10)
                .padding(.bottom, 8)
        }
        .background(
            LinearGradient(
                colors: [
                    Color(.systemBackground),
                    themeAccent(viewModel.selectedTheme).opacity(0.07),
                    Color(.secondarySystemBackground).opacity(0.7)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
        )
        .navigationDestination(item: $activeCard) { card in
            GuideDetailView(
                api: api,
                context: card.feedContext,
                previewTitle: card.title,
                previewReason: card.reason,
                theme: viewModel.selectedTheme
            )
        }
        .onAppear { viewModel.onAppear() }
        .onReceive(viewModel.$state.dropFirst()) { newState in
            if case .success = newState {
                currentIndex = 0
                isRefreshing = false
                withAnimation(.spring(response: 0.25, dampingFraction: 0.8)) {
                    pullOffset = 0
                    cardDragOffset = 0
                }
            }
        }
        .onReceive(viewModel.$selectedTheme.dropFirst()) { _ in
            themeFocusPulse = true
            themePulseTask?.cancel()
            themePulseTask = Task {
                try? await Task.sleep(nanoseconds: 220_000_000)
                guard !Task.isCancelled else { return }
                themeFocusPulse = false
            }
        }
        .onDisappear {
            themePulseTask?.cancel()
            dragResetTask?.cancel()
        }
    }

    private var headerView: some View {
        HStack(spacing: 12) {
            Button {
                showSearchPlaceholder = true
            } label: {
                HStack(spacing: 8) {
                    Image(systemName: "magnifyingglass")
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(.secondary)
                    Text("搜索少糖推荐")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                    Spacer()
                }
                .padding(.horizontal, 14)
                .padding(.vertical, 10)
                .background(
                    ZStack {
                        Capsule(style: .continuous)
                            .fill(.ultraThinMaterial)
                        Capsule(style: .continuous)
                            .fill(
                                LinearGradient(
                                    colors: [.white.opacity(0.15), .white.opacity(0.03)],
                                    startPoint: .topLeading,
                                    endPoint: .bottomTrailing
                                )
                            )
                    }
                )
                .overlay(
                    Capsule(style: .continuous)
                        .strokeBorder(
                            LinearGradient(
                                colors: [.white.opacity(0.45), .white.opacity(0.1)],
                                startPoint: .topLeading,
                                endPoint: .bottomTrailing
                            ),
                            lineWidth: 0.8
                        )
                )
                .shadow(color: .black.opacity(0.03), radius: 6, y: 2)
            }
            .buttonStyle(.plain)

            NavigationLink {
                MeSummaryView(api: api)
            } label: {
                Image(systemName: "person.crop.circle.fill")
                    .font(.title3)
                    .foregroundStyle(Color.primary.opacity(0.82))
                    .frame(width: 40, height: 40)
                    .background(
                        ZStack {
                            Circle()
                                .fill(.ultraThinMaterial)
                            Circle()
                                .fill(
                                    LinearGradient(
                                        colors: [.white.opacity(0.15), .white.opacity(0.03)],
                                        startPoint: .topLeading,
                                        endPoint: .bottomTrailing
                                    )
                                )
                        }
                    )
                    .overlay(
                        Circle()
                            .strokeBorder(
                                LinearGradient(
                                    colors: [.white.opacity(0.45), .white.opacity(0.1)],
                                    startPoint: .topLeading,
                                    endPoint: .bottomTrailing
                                ),
                                lineWidth: 1
                            )
                    )
                    .shadow(color: .black.opacity(0.03), radius: 6, y: 2)
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .alert("搜索功能即将上线", isPresented: $showSearchPlaceholder) {
            Button("知道了", role: .cancel) {}
        }
    }

    private var bottomThemeTabs: some View {
        HStack(spacing: 10) {
            ForEach(Theme.allCases) { theme in
                let isSelected = viewModel.selectedTheme == theme
                Button {
                    withAnimation(.interactiveSpring(response: 0.48, dampingFraction: 0.8, blendDuration: 0.2)) {
                        viewModel.onThemeChanged(theme)
                    }
                } label: {
                    Text(theme.title)
                        .font(.headline.weight(.semibold))
                        .foregroundStyle(isSelected ? .primary : .secondary)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 8)
                        .background {
                            if isSelected {
                                ZStack {
                                    Capsule(style: .continuous)
                                        .fill(.white.opacity(0.34))
                                    Capsule(style: .continuous)
                                        .strokeBorder(.white.opacity(0.55), lineWidth: 0.8)
                                }
                                .matchedGeometryEffect(id: "theme-liquid-pill", in: themeTabNamespace)
                                .shadow(color: .white.opacity(0.3), radius: 8, y: 2)
                            }
                        }
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .padding(6)
        .background(
            ZStack {
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .fill(.ultraThinMaterial)
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .fill(
                        LinearGradient(
                            colors: [.white.opacity(0.12), .white.opacity(0.03)],
                            startPoint: .topLeading,
                            endPoint: .bottomTrailing
                        )
                    )
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .strokeBorder(
                        LinearGradient(
                            colors: [.white.opacity(0.45), .white.opacity(0.12)],
                            startPoint: .topLeading,
                            endPoint: .bottomTrailing
                        ),
                        lineWidth: 0.9
                    )
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .fill(.white.opacity(0.22))
                    .opacity(themeFocusPulse ? 1 : 0)
                    .blur(radius: themeFocusPulse ? 0 : 7)
            }
            .shadow(color: .black.opacity(0.04), radius: 10, y: 4)
            .animation(.easeOut(duration: 0.28), value: themeFocusPulse)
        )
        .gesture(
            DragGesture(minimumDistance: 18)
                .onEnded(handleThemeSwipe)
        )
    }

    @ViewBuilder
    private var content: some View {
        switch viewModel.state {
        case .loading:
            ProgressView("加载中...")
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        case .success(let cards):
            if cards.isEmpty {
                StatePlaceholderView(title: "暂无内容", actionTitle: "重试", action: viewModel.retry)
            } else {
                singleCardView(cards: cards)
            }
        case .empty:
            StatePlaceholderView(title: "暂无内容", actionTitle: "重试", action: viewModel.retry)
        case .error(let message):
            StatePlaceholderView(title: message, actionTitle: "重试", action: viewModel.retry)
        case .offline:
            StatePlaceholderView(title: "网络不可用", actionTitle: "重试", action: viewModel.retry)
        }
    }

    private func singleCardView(cards: [HomeCard]) -> some View {
        let index = min(max(currentIndex, 0), cards.count - 1)
        let card = cards[index]
        let pullHintOpacity = min(max((pullOffset - refreshHintThreshold) / 18, 0), 1)
        let isReadyToRefresh = pullOffset >= refreshTriggerThreshold
        let endHintOpacity = min(max((-cardDragOffset - 18) / 30, 0), 1)
        let shouldShowEndHint = index == cards.count - 1 && cardDragOffset < -18

        return VStack(spacing: 12) {
            if pullOffset >= refreshHintThreshold {
                Text(isReadyToRefresh ? "松手刷新" : "继续下滑刷新")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .opacity(pullHintOpacity)
            }

            if shouldShowEndHint {
                Text("少糖就先到这里吧")
                    .font(.footnote.weight(.medium))
                    .foregroundStyle(.secondary)
                    .opacity(endHintOpacity)
                    .transition(.opacity)
            }

            GeometryReader { proxy in
                let pageDistance = max(proxy.size.height, 1)
                ZStack {
                    if cardDragOffset > 0, index > 0 {
                        cardContent(cards[index - 1], position: index, total: cards.count, cardHeight: pageDistance)
                            .offset(y: cardDragOffset - pageDistance)
                    }

                    if cardDragOffset < 0, index + 1 < cards.count {
                        cardContent(cards[index + 1], position: index + 2, total: cards.count, cardHeight: pageDistance)
                            .offset(y: cardDragOffset + pageDistance)
                    }

                    cardContent(card, position: index + 1, total: cards.count, cardHeight: pageDistance)
                        .offset(y: cardDragOffset)
                }
                .animation(.interactiveSpring(response: 0.32, dampingFraction: 0.84), value: cardDragOffset)
                .offset(y: pullOffset)
                .animation(.spring(response: 0.24, dampingFraction: 0.9), value: pullOffset)
                .highPriorityGesture(
                    DragGesture(minimumDistance: 18)
                        .onChanged { value in
                            handleCardDragChanged(value, cards: cards, pageDistance: pageDistance)
                        }
                        .onEnded { value in
                            handleCardDragEnded(value, cards: cards, pageDistance: pageDistance)
                        }
                )
            }
        }
    }

    private func cardContent(_ card: HomeCard, position: Int, total: Int, cardHeight: CGFloat) -> some View {
        let imageHeight = min(max(cardHeight - 220, 300), 520)
        return Button {
            if !isCardDragging {
                activeCard = card
            }
        } label: {
            VStack(alignment: .leading, spacing: 14) {
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .fill(
                        LinearGradient(
                            colors: [
                                themeAccent(viewModel.selectedTheme).opacity(0.24),
                                themeAccent(viewModel.selectedTheme).opacity(0.08)
                            ],
                            startPoint: .topLeading,
                            endPoint: .bottomTrailing
                        )
                    )
                    .frame(height: imageHeight)
                    .overlay {
                        Image(systemName: themeImage(viewModel.selectedTheme))
                            .font(.system(size: 48, weight: .semibold))
                            .foregroundStyle(themeAccent(viewModel.selectedTheme).opacity(0.88))
                    }

                VStack(alignment: .leading, spacing: 8) {
                    Text("推荐理由")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.secondary)
                    Text(card.reason == "-" ? "为你精选的轻量推荐" : card.reason)
                        .font(.body)
                        .foregroundStyle(.primary)
                        .lineLimit(2)
                        .truncationMode(.tail)
                        .frame(height: 44, alignment: .topLeading)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                Spacer(minLength: 0)

                HStack {
                    Text("\(position)/\(total)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(22)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background(
                ZStack {
                    RoundedRectangle(cornerRadius: 30, style: .continuous)
                        .fill(.ultraThinMaterial)
                    RoundedRectangle(cornerRadius: 30, style: .continuous)
                        .fill(
                            LinearGradient(
                                colors: [
                                    .white.opacity(0.18),
                                    .white.opacity(0.04)
                                ],
                                startPoint: .topLeading,
                                endPoint: .bottomTrailing
                            )
                        )
                }
            )
            .overlay(
                RoundedRectangle(cornerRadius: 30, style: .continuous)
                    .strokeBorder(
                        LinearGradient(
                            colors: [.white.opacity(0.5), .white.opacity(0.15)],
                            startPoint: .topLeading,
                            endPoint: .bottomTrailing
                        ),
                        lineWidth: 1
                    )
            )
            .shadow(color: .black.opacity(0.05), radius: 16, y: 8)
            .shadow(color: themeAccent(viewModel.selectedTheme).opacity(0.06), radius: 24, y: 12)
        }
        .buttonStyle(.plain)
        .id(card.id)
    }

    private func handleCardDragChanged(_ value: DragGesture.Value, cards: [HomeCard], pageDistance: CGFloat) {
        let vertical = value.translation.height
        let horizontal = value.translation.width
        if abs(vertical) > 10 || abs(horizontal) > 10 {
            isCardDragging = true
        }
        guard abs(vertical) > abs(horizontal) else { return }

        if vertical > 0, currentIndex == 0 {
            pullOffset = min(vertical, refreshTriggerThreshold + 24)
        } else {
            pullOffset = 0
        }

        let isAtTop = currentIndex == 0
        let isAtBottom = currentIndex >= cards.count - 1
        let cardDragLimit = pageDistance * 0.95
        if vertical > 0, isAtTop {
            cardDragOffset = min(vertical, cardDragLimit)
        } else if vertical < 0, isAtBottom {
            cardDragOffset = max(vertical * 0.35, -cardDragLimit * 0.45)
        } else if vertical > 0, !isAtTop {
            cardDragOffset = min(vertical, cardDragLimit)
        } else if vertical < 0, !isAtBottom {
            cardDragOffset = max(vertical, -cardDragLimit)
        } else {
            cardDragOffset = 0
        }
    }

    private func handleCardDragEnded(_ value: DragGesture.Value, cards: [HomeCard], pageDistance: CGFloat) {
        defer {
            withAnimation(.spring(response: 0.26, dampingFraction: 0.86)) {
                pullOffset = 0
                cardDragOffset = 0
            }
            scheduleDragReset()
        }

        let vertical = value.translation.height
        let predictedVertical = value.predictedEndTranslation.height
        let horizontal = value.translation.width
        if abs(horizontal) > abs(vertical), abs(horizontal) > themeSwipeThreshold {
            handleThemeSwipe(value)
            return
        }
        guard abs(vertical) > abs(horizontal) else { return }

        let velocityFlipThreshold = pageDistance * 0.22
        let shouldFlipNext = vertical < -nextCardThreshold || predictedVertical < -velocityFlipThreshold
        let shouldFlipPrev = vertical > nextCardThreshold || predictedVertical > velocityFlipThreshold
        let shouldRefreshTop = (vertical > refreshTriggerThreshold || predictedVertical > refreshTriggerThreshold) && currentIndex == 0

        if shouldFlipNext {
            let nextIndex = currentIndex + 1
            if nextIndex < cards.count {
                withAnimation(.interactiveSpring(response: 0.38, dampingFraction: 0.8, blendDuration: 0.2)) {
                    currentIndex = nextIndex
                }
            }
        } else if shouldFlipPrev, currentIndex > 0 {
            withAnimation(.interactiveSpring(response: 0.38, dampingFraction: 0.8, blendDuration: 0.2)) {
                currentIndex -= 1
            }
        } else if shouldRefreshTop, !isRefreshing {
            isRefreshing = true
            viewModel.refreshRecommendations()
        }
    }

    private func handleThemeSwipe(_ value: DragGesture.Value) {
        let horizontal = value.translation.width
        let vertical = value.translation.height
        guard abs(horizontal) > themeSwipeThreshold, abs(horizontal) > abs(vertical) else { return }
        guard let current = Theme.allCases.firstIndex(of: viewModel.selectedTheme) else { return }

        let nextIndex: Int
        if horizontal < 0 {
            nextIndex = min(current + 1, Theme.allCases.count - 1)
        } else {
            nextIndex = max(current - 1, 0)
        }

        guard nextIndex != current else { return }
        withAnimation(.spring(response: 0.3, dampingFraction: 0.85)) {
            viewModel.onThemeChanged(Theme.allCases[nextIndex])
        }
    }

    private func scheduleDragReset() {
        dragResetTask?.cancel()
        dragResetTask = Task {
            try? await Task.sleep(nanoseconds: 120_000_000)
            guard !Task.isCancelled else { return }
            isCardDragging = false
        }
    }

    private func themeSlogan(_ theme: Theme) -> String {
        switch theme {
        case .clothing:
            return "简单搭配，你也很美"
        case .food:
            return "好好吃饭，就是治愈"
        case .housing:
            return "住得舒服，心才安定"
        case .transport:
            return "轻松出发，步步从容"
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

    private func themeImage(_ theme: Theme) -> String {
        switch theme {
        case .clothing:
            return "tshirt"
        case .food:
            return "fork.knife"
        case .housing:
            return "house"
        case .transport:
            return "car"
        }
    }
}

private struct StatePlaceholderView: View {
    let title: String
    let actionTitle: String
    let action: () -> Void

    var body: some View {
        VStack(spacing: 10) {
            Text(title)
            Button(actionTitle, action: action)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
