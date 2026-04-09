import SwiftUI
import Combine
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

    private let nextCardThreshold: CGFloat = 90
    private let refreshHintThreshold: CGFloat = 28
    private let refreshTriggerThreshold: CGFloat = 92
    private let themeSwipeThreshold: CGFloat = 54

    init(api: GatewayAPI) {
        self.api = api
        _viewModel = StateObject(wrappedValue: HomeFlowViewModel(api: api))
    }

    var body: some View {
        VStack(spacing: 10) {
            themeTabs
            .padding(.horizontal, 16)
            .padding(.vertical, 6)
            .padding(.horizontal, 12)
            .gesture(
                DragGesture(minimumDistance: 18)
                    .onEnded(handleThemeSwipe)
            )

            content
                .padding(.horizontal, 16)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .navigationTitle("简单生活")
        .navigationDestination(item: $activeCard) { card in
            GuideDetailView(api: api, context: card.feedContext)
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
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.22) {
                themeFocusPulse = false
            }
        }
    }

    private var themeTabs: some View {
        HStack(spacing: 8) {
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
                        .padding(.vertical, 10)
                        .background {
                            if isSelected {
                                ZStack {
                                    Capsule(style: .continuous)
                                        .fill(.white.opacity(0.34))
                                    Capsule(style: .continuous)
                                        .strokeBorder(.white.opacity(0.55), lineWidth: 0.6)
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
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .fill(.ultraThinMaterial)
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .strokeBorder(.white.opacity(0.2), lineWidth: 1)
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .fill(.white.opacity(0.22))
                    .opacity(themeFocusPulse ? 1 : 0)
                    .blur(radius: themeFocusPulse ? 0 : 7)
            }
            .animation(.easeOut(duration: 0.28), value: themeFocusPulse)
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

        return VStack(spacing: 12) {
            if pullOffset >= refreshHintThreshold {
                Text(isReadyToRefresh ? "松手刷新" : "继续下滑刷新")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .opacity(pullHintOpacity)
            }

            GeometryReader { proxy in
                let pageDistance = max(proxy.size.height, 1)
                ZStack {
                    if cardDragOffset > 0, index > 0 {
                        cardContent(cards[index - 1], position: index, total: cards.count)
                            .offset(y: cardDragOffset - pageDistance)
                    }

                    if cardDragOffset < 0, index + 1 < cards.count {
                        cardContent(cards[index + 1], position: index + 2, total: cards.count)
                            .offset(y: cardDragOffset + pageDistance)
                    }

                    cardContent(card, position: index + 1, total: cards.count)
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

    private func cardContent(_ card: HomeCard, position: Int, total: Int) -> some View {
        Button {
            if !isCardDragging {
                activeCard = card
            }
        } label: {
            VStack(alignment: .leading, spacing: 16) {
                Text(card.title)
                    .font(.title2.weight(.semibold))
                    .foregroundStyle(.primary)

                Text(card.reason)
                    .font(.body)
                    .foregroundStyle(.secondary)

                Spacer(minLength: 0)

                HStack {
                    Text("\(position)/\(total)")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Text("上滑看下一个")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .padding(22)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 28, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: 28, style: .continuous)
                    .strokeBorder(.white.opacity(0.22), lineWidth: 1)
            )
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
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.12) {
                isCardDragging = false
            }
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
            } else {
                isRefreshing = true
                viewModel.refreshRecommendations()
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
