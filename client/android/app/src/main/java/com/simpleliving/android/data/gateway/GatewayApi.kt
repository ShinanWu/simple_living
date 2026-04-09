package com.simpleliving.android.data.gateway

import com.simpleliving.android.data.gateway.model.GuideDetailResponse
import com.simpleliving.android.data.gateway.model.HomeFeedRequest
import com.simpleliving.android.data.gateway.model.HomeFeedResponse
import com.simpleliving.android.data.gateway.model.MeSummaryResponse
import com.simpleliving.android.data.gateway.model.RedirectPrepareRequest
import com.simpleliving.android.data.gateway.model.RedirectPrepareResponse

interface GatewayApi {
    suspend fun getHomeFeed(request: HomeFeedRequest): HomeFeedResponse
    suspend fun getGuideDetail(guide_card_id: String): GuideDetailResponse
    suspend fun postRedirectPrepare(request: RedirectPrepareRequest): RedirectPrepareResponse
    suspend fun getMeSummary(): MeSummaryResponse
}
