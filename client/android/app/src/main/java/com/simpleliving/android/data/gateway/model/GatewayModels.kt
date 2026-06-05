package com.simpleliving.android.data.gateway.model

import com.simpleliving.android.model.Theme

data class HomeFeedRequest(
    val theme: Theme,
    val cursor: String? = null,
    val limit: Int = 20,
)

data class HomeCard(
    val id: String,
    val guide_card_id: String,
    val recommendation_id: String,
    val scene: String,
    val item_rank: Int,
    val title: String,
    val reason: String,
    val cover_url: String? = null,
)

data class Pagination(
    val next_cursor: String?,
    val has_more: Boolean? = null,
    val limit: Int? = null,
)

data class HomeFeedData(
    val items: List<HomeFeedItem>,
    val pagination: Pagination?,
)

data class HomeFeedItem(
    val recommendation_id: String,
    val scene: String,
    val rank: Int,
    val guide_card_id: String,
    val reason_tags: List<String>? = null,
    val guide_card: GuideCardSnippet? = null,
)

data class GuideCardSnippet(
    val title: String? = null,
    val summary: String? = null,
    val subtitle: String? = null,
    val cover_url: String? = null,
    val cover_media: CoverMedia? = null,
)

data class CoverMedia(
    val url: String? = null,
)

data class HomeFeedResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: HomeFeedData?,
    val meta: ResponseMeta? = null,
)

data class GuideDetailResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: GuideDetailData?,
    val meta: ResponseMeta? = null,
)

data class GuideDetailData(
    val guide_card: GuideCardDetail? = null,
    val guide: GuideCardDetail? = null,
)

data class GuideCardDetail(
    val guide_card_id: String,
    val title: String,
    val summary: String? = null,
    val subtitle: String? = null,
    val cover_url: String? = null,
    val cover_media: CoverMedia? = null,
    val is_commercial: Boolean? = null,
    val disclosure_text_key: String? = null,
)

data class RedirectPrepareRequest(
    val guide_card_id: String,
    val recommendation_id: String,
    val scene: String,
    val item_rank: Int,
)

data class RedirectPrepareResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: RedirectPrepareData?,
    val meta: ResponseMeta? = null,
)

data class RedirectPrepareData(
    val landing_url: String,
    val click_id: String? = null,
)

data class MeSummaryResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: MeSummaryData?,
    val meta: ResponseMeta? = null,
)

data class MeSummaryData(
    val profile: ProfileData,
    val counts: CountsData,
    val consent: ConsentData? = null,
)

data class ProfileData(
    val user_id: String? = null,
    val is_guest: Boolean? = null,
    val display_name: String? = null,
    val avatar_url: String? = null,
)

data class CountsData(
    val favorites_count: Int = 0,
    val history_count: Int = 0,
)

data class ConsentData(
    val personalization_allowed: Boolean? = null,
)

data class AuthTokenResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: TokenPairData?,
    val meta: ResponseMeta? = null,
)

data class TokenPairData(
    val access_token: String,
    val refresh_token: String,
    val expires_in: Int? = null,
    val session_id: String? = null,
)

data class GuestSessionResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: GuestSessionData?,
    val meta: ResponseMeta? = null,
)

data class GuestSessionData(
    val session_id: String,
    val created: Boolean? = null,
)

data class FavoriteItem(
    val favorite_id: String,
    val guide_card_id: String,
    val favorited_at: String,
)

data class FavoriteListData(
    val items: List<FavoriteItem>,
    val pagination: Pagination?,
)

data class FavoriteListResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: FavoriteListData?,
    val meta: ResponseMeta? = null,
)

data class AddFavoriteResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: AddFavoriteData?,
    val meta: ResponseMeta? = null,
)

data class AddFavoriteData(
    val favorite_id: String,
    val already_favorited: Boolean,
)

data class HistoryItem(
    val content_ref: ContentRef,
    val last_seen_at: String,
)

data class ContentRef(
    val type: String,
    val guide_card_id: String,
)

data class HistoryListData(
    val items: List<HistoryItem>,
    val pagination: Pagination?,
)

data class HistoryListResponse(
    val success: Boolean,
    val code: Int,
    val message: String,
    val data: HistoryListData?,
    val meta: ResponseMeta? = null,
)

data class ResponseMeta(
    val request_id: String? = null,
    val server_time_ms: Long? = null,
)
