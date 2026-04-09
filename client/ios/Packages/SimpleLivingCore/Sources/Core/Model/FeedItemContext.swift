import Foundation

/// 从首页推荐流带入详情与跳转准备的归因上下文（对齐 gateway `redirect_prepare` 可选字段）。
public struct FeedItemContext: Equatable, Hashable, Sendable {
    public let guideCardId: String
    public let recommendationId: String
    public let scene: String
    public let itemRank: Int

    public init(guideCardId: String, recommendationId: String, scene: String, itemRank: Int) {
        self.guideCardId = guideCardId
        self.recommendationId = recommendationId
        self.scene = scene
        self.itemRank = itemRank
    }
}
