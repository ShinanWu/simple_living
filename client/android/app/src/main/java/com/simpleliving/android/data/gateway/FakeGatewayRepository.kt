package com.simpleliving.android.data.gateway

import com.simpleliving.android.data.gateway.model.*
import com.simpleliving.android.model.Theme
import kotlinx.coroutines.delay

class FakeGatewayRepository : GatewayApi {
    override suspend fun getHomeFeed(request: HomeFeedRequest): HomeFeedResponse {
        delay(120)
        val items = List(6) { index ->
            val position = index + 1
            val theme = request.theme
            HomeFeedItem(
                recommendation_id = "${theme.apiValue}-$position",
                scene = "home_feed",
                rank = position,
                guide_card_id = "gc_${theme.apiValue}_$position",
                reason_tags = listOf("精选", "热门"),
                guide_card = GuideCardSnippet(
                    title = "${theme.label}主题推荐 $position",
                    summary = "来自 /api/v2/pages/home_feed 的占位数据",
                    cover_url = null,
                )
            )
        }
        return HomeFeedResponse(
            success = true,
            code = 0,
            message = "ok",
            data = HomeFeedData(
                items = items,
                pagination = Pagination(next_cursor = "next_cursor_token", has_more = true),
            ),
        )
    }

    override suspend fun getGuideDetail(guide_card_id: String): GuideDetailResponse {
        delay(80)
        return GuideDetailResponse(
            success = true,
            code = 0,
            message = "ok",
            data = GuideDetailData(
                guide_card = GuideCardDetail(
                    guide_card_id = guide_card_id,
                    title = "GuideDetail 占位内容",
                    summary = "这是占位摘要内容",
                    disclosure_text_key = "推广信息披露占位",
                ),
            ),
        )
    }

    override suspend fun postRedirectPrepare(request: RedirectPrepareRequest): RedirectPrepareResponse {
        delay(100)
        return RedirectPrepareResponse(
            success = true,
            code = 0,
            message = "ok",
            data = RedirectPrepareData(
                landing_url = "https://example.com/landing/${request.guide_card_id}",
            ),
        )
    }

    override suspend fun getMeSummary(): MeSummaryResponse {
        delay(90)
        return MeSummaryResponse(
            success = true,
            code = 0,
            message = "ok",
            data = MeSummaryData(
                profile = ProfileData(
                    user_id = "guest_user",
                    is_guest = true,
                ),
                counts = CountsData(
                    favorites_count = 12,
                    history_count = 34,
                ),
                consent = ConsentData(personalization_allowed = true),
            ),
        )
    }
}
