package com.simpleliving.android.data.gateway

import com.simpleliving.android.data.gateway.model.GuideDetailData
import com.simpleliving.android.data.gateway.model.GuideDetailResponse
import com.simpleliving.android.data.gateway.model.HomeCard
import com.simpleliving.android.data.gateway.model.HomeFeedData
import com.simpleliving.android.data.gateway.model.HomeFeedRequest
import com.simpleliving.android.data.gateway.model.HomeFeedResponse
import com.simpleliving.android.data.gateway.model.MeSummaryData
import com.simpleliving.android.data.gateway.model.MeSummaryResponse
import com.simpleliving.android.data.gateway.model.Pagination
import com.simpleliving.android.data.gateway.model.RedirectPrepareData
import com.simpleliving.android.data.gateway.model.RedirectPrepareRequest
import com.simpleliving.android.data.gateway.model.RedirectPrepareResponse
import kotlinx.coroutines.delay

class FakeGatewayRepository : GatewayApi {
    override suspend fun getHomeFeed(request: HomeFeedRequest): HomeFeedResponse {
        delay(120)
        val cards = List(6) { index ->
            val position = index + 1
            HomeCard(
                recommendation_id = "${request.theme.apiValue}-$position",
                title = "${request.theme.label}主题推荐 $position",
                subtitle = "来自 /api/v2/pages/home_feed 的占位数据",
            )
        }
        return HomeFeedResponse(
            code = 0,
            message = "ok",
            data = HomeFeedData(
                cards = cards,
                pagination = Pagination(cursor = "next_cursor_token", has_more = true),
            ),
        )
    }

    override suspend fun getGuideDetail(guide_card_id: String): GuideDetailResponse {
        delay(80)
        return GuideDetailResponse(
            code = 0,
            message = "ok",
            data = GuideDetailData(
                guide_card_id = guide_card_id,
                title = "GuideDetail 占位内容",
                disclosure = "推广信息披露占位",
            ),
        )
    }

    override suspend fun postRedirectPrepare(request: RedirectPrepareRequest): RedirectPrepareResponse {
        delay(100)
        return RedirectPrepareResponse(
            code = 0,
            message = "ok",
            data = RedirectPrepareData(
                landing_url = "https://example.com/landing/${request.guide_card_id}",
                recommendation_id = "rec-${request.guide_card_id}",
            ),
        )
    }

    override suspend fun getMeSummary(): MeSummaryResponse {
        delay(90)
        return MeSummaryResponse(
            code = 0,
            message = "ok",
            data = MeSummaryData(
                user_id = "guest_user",
                favorites_count = 12,
                history_count = 34,
                consent_granted = true,
            ),
        )
    }
}
