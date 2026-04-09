package com.simpleliving.android.data.gateway.model

import com.simpleliving.android.model.Theme

data class HomeFeedRequest(
    val theme: Theme,
    val cursor: String? = null,
    val limit: Int = 20,
)

data class HomeCard(
    val recommendation_id: String,
    val title: String,
    val subtitle: String,
)

data class Pagination(
    val cursor: String?,
    val has_more: Boolean,
)

data class HomeFeedData(
    val cards: List<HomeCard>,
    val pagination: Pagination,
)

data class HomeFeedResponse(
    val code: Int,
    val message: String,
    val data: HomeFeedData,
)

data class GuideDetailResponse(
    val code: Int,
    val message: String,
    val data: GuideDetailData,
)

data class GuideDetailData(
    val guide_card_id: String,
    val title: String,
    val disclosure: String,
)

data class RedirectPrepareRequest(
    val guide_card_id: String,
    val scene: String,
)

data class RedirectPrepareResponse(
    val code: Int,
    val message: String,
    val data: RedirectPrepareData,
)

data class RedirectPrepareData(
    val landing_url: String,
    val recommendation_id: String,
)

data class MeSummaryResponse(
    val code: Int,
    val message: String,
    val data: MeSummaryData,
)

data class MeSummaryData(
    val user_id: String,
    val favorites_count: Int,
    val history_count: Int,
    val consent_granted: Boolean,
)
