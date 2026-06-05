package com.simpleliving.android.data.gateway

import com.google.gson.Gson
import com.google.gson.JsonObject
import com.simpleliving.android.data.gateway.model.*
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets

class GatewayBusinessError(val code: Int, message: String) : Exception(message)

class HttpGatewayApi(
    private val baseUrl: String,
    private val clientPlatform: String = "android",
    private val appVersion: String = "0.1.0",
) : GatewayApi {

    private val gson = Gson()
    private val settings = GatewaySettings()
    private val refreshMutex = Mutex()
    @Volatile
    private var refreshInFlight: TokenPairData? = null

    override suspend fun getHomeFeed(request: HomeFeedRequest): HomeFeedResponse {
        val query = buildString {
            append("limit=${request.limit}&theme=${request.theme.apiValue}")
            if (request.cursor != null) {
                append("&cursor=${request.cursor}")
            }
        }
        val response = requestWithAuth<HomeFeedResponse>("GET", "/api/v2/pages/home_feed?$query")
        return response
    }

    override suspend fun getGuideDetail(guide_card_id: String): GuideDetailResponse {
        val q = "guide_card_id=$guide_card_id&include_related=true"
        val response = requestWithAuth<GuideDetailResponse>("GET", "/api/v2/pages/guide_detail?$q")
        return response
    }

    override suspend fun postRedirectPrepare(request: RedirectPrepareRequest): RedirectPrepareResponse {
        ensureGuestSessionIfNeeded()
        val body = gson.toJson(request)
        val response = requestWithAuth<RedirectPrepareResponse>("POST", "/api/v2/pages/redirect_prepare", body)
        return response
    }

    override suspend fun getMeSummary(): MeSummaryResponse {
        ensureGuestSessionIfNeeded()
        val response = requestWithAuth<MeSummaryResponse>("GET", "/api/v2/pages/me_summary")
        return response
    }

    suspend fun issueTokenWithPhone(
        phoneE164: String,
        otpCode: String,
        verificationId: String
    ): TokenPairData {
        val body = JsonObject().apply {
            add("account_proof", JsonObject().apply {
                add("phone_otp", JsonObject().apply {
                    addProperty("phone_e164", phoneE164)
                    addProperty("otp_code", otpCode)
                    addProperty("verification_id", verificationId)
                })
            })
            addProperty("client_platform", clientPlatform)
            addProperty("device_id", settings.deviceId)
            addProperty("app_version", appVersion)
        }
        return postIssueToken(gson.toJson(body))
    }

    suspend fun issueTokenWithWeChat(providerSubject: String, authorizationCode: String): TokenPairData {
        val body = JsonObject().apply {
            add("account_proof", JsonObject().apply {
                add("oauth", JsonObject().apply {
                    addProperty("provider", "wechat")
                    addProperty("provider_subject", providerSubject)
                    addProperty("authorization_code", authorizationCode)
                })
            })
            addProperty("client_platform", clientPlatform)
            addProperty("device_id", settings.deviceId)
            addProperty("app_version", appVersion)
        }
        return postIssueToken(gson.toJson(body))
    }

    suspend fun refreshAuthTokens(): TokenPairData {
        val refreshToken = settings.refreshToken
        if (refreshToken == null) {
            throw GatewayBusinessError(20003, "no refresh token")
        }

        refreshInFlight?.let { return it }

        return refreshMutex.withLock {
            refreshInFlight?.let { return it }

            try {
                val body = JsonObject().apply {
                    addProperty("refresh_token", refreshToken)
                    add("request_context", JsonObject().apply {
                        addProperty("client_platform", clientPlatform)
                        addProperty("app_version", appVersion)
                        addProperty("device_id", settings.deviceId)
                    })
                }
                val response = makeRequest("POST", "/api/v2/auth/token/refresh", gson.toJson(body), authMode = AuthMode.NONE)
                val envelope = gson.fromJson(response, AuthTokenResponse::class.java)
                if (envelope.success && envelope.code == 0 && envelope.data != null) {
                    settings.applyTokenPair(envelope.data)
                    refreshInFlight = envelope.data
                    return envelope.data
                }
                throw GatewayBusinessError(envelope.code, envelope.message)
            } finally {
                refreshInFlight = null
            }
        }
    }

    suspend fun logoutSession(revokeAllDevices: Boolean) {
        try {
            if (settings.accessToken != null) {
                val body = JsonObject().apply {
                    addProperty("revoke_scope", if (revokeAllDevices) "all_user_sessions" else "single_session")
                }
                requestWithAuth<JsonObject>("DELETE", "/api/v2/auth/session", gson.toJson(body))
            }
        } finally {
            settings.clearAuth()
        }
    }

    suspend fun listFavorites(cursor: String? = null, limit: Int = 20): FavoriteListResponse {
        val q = buildString {
            append("limit=$limit")
            if (cursor != null) append("&cursor=$cursor")
        }
        return requestWithAuth<FavoriteListResponse>("GET", "/api/v2/me/favorites?$q")
    }

    suspend fun addFavorite(guideCardId: String): AddFavoriteResponse {
        val body = JsonObject().apply {
            addProperty("guide_card_id", guideCardId)
        }
        return requestWithAuth<AddFavoriteResponse>("POST", "/api/v2/me/favorites", gson.toJson(body))
    }

    suspend fun listHistory(cursor: String? = null, limit: Int = 20): HistoryListResponse {
        val q = buildString {
            append("limit=$limit")
            if (cursor != null) append("&cursor=$cursor")
        }
        return requestWithAuth<HistoryListResponse>("GET", "/api/v2/me/history?$q")
    }

    suspend fun recordHistoryEvent(guideCardId: String, sourceSurface: String) {
        ensureGuestSessionIfNeeded()
        val body = JsonObject().apply {
            add("content_ref", JsonObject().apply {
                addProperty("type", "guide_card")
                addProperty("guide_card_id", guideCardId)
            })
            addProperty("source_surface", sourceSurface)
        }
        requestWithAuth<JsonObject>("POST", "/api/v2/me/history/events", gson.toJson(body))
    }

    private suspend fun <T> requestWithAuth(method: String, path: String, body: String? = null): T {
        val response = makeRequest(method, path, body, authMode = AuthMode.BEARER_OR_GUEST)
        val envelope = gson.fromJson(response, ApiEnvelope::class.java)
        
        if (envelope.success && envelope.code == 0) {
            @Suppress("UNCHECKED_CAST")
            return response as T
        }

        if (envelope.code == 20002 && settings.refreshToken != null) {
            try {
                refreshAuthTokens()
                return requestWithAuth(method, path, body)
            } catch (e: Exception) {
                settings.clearAuth()
                throw GatewayBusinessError(20003, "refresh token invalid")
            }
        }

        throw GatewayBusinessError(envelope.code, envelope.message)
    }

    private suspend fun postIssueToken(bodyJson: String): TokenPairData {
        val response = makeRequest("POST", "/api/v2/auth/token/issue", bodyJson, authMode = AuthMode.NONE)
        val envelope = gson.fromJson(response, AuthTokenResponse::class.java)
        if (envelope.success && envelope.code == 0 && envelope.data != null) {
            settings.applyTokenPair(envelope.data)
            return envelope.data
        }
        throw GatewayBusinessError(envelope.code, envelope.message)
    }

    private suspend fun ensureGuestSessionIfNeeded() {
        if (settings.accessToken != null) return
        if (settings.guestSessionId != null) return

        val body = JsonObject().apply {
            addProperty("device_id", settings.deviceId)
            addProperty("client_platform", clientPlatform)
            addProperty("app_version", appVersion)
        }
        val response = makeRequest("POST", "/api/v2/guest/session", gson.toJson(body), authMode = AuthMode.NONE)
        val envelope = gson.fromJson(response, GuestSessionResponse::class.java)
        if (envelope.success && envelope.code == 0 && envelope.data != null) {
            settings.guestSessionId = envelope.data.session_id
        } else {
            throw GatewayBusinessError(envelope.code, envelope.message)
        }
    }

    private enum class AuthMode {
        NONE,
        BEARER_OR_GUEST
    }

    private data class ApiEnvelope(
        val success: Boolean,
        val code: Int,
        val message: String
    )

    private fun makeRequest(
        method: String,
        path: String,
        body: String? = null,
        authMode: AuthMode = AuthMode.BEARER_OR_GUEST
    ): String {
        val url = URL("$baseUrl$path")
        val connection = url.openConnection() as HttpURLConnection
        try {
            connection.requestMethod = method
            connection.setRequestProperty("Accept", "application/json")
            
            if (body != null) {
                connection.setRequestProperty("Content-Type", "application/json")
            }

            when (authMode) {
                AuthMode.BEARER_OR_GUEST -> {
                    if (settings.accessToken != null) {
                        connection.setRequestProperty("Authorization", "Bearer ${settings.accessToken}")
                    } else if (settings.guestSessionId != null) {
                        connection.setRequestProperty("X-Guest-Session-Id", settings.guestSessionId)
                    }
                }
                AuthMode.NONE -> {}
            }

            if (body != null) {
                connection.doOutput = true
                OutputStreamWriter(connection.outputStream, StandardCharsets.UTF_8).use {
                    it.write(body)
                }
            }

            val statusCode = connection.responseCode
            if (statusCode >= 500) {
                throw GatewayBusinessError(90001, "http $statusCode")
            }

            val responseStream = if (statusCode in 200..299) {
                connection.inputStream
            } else {
                connection.errorStream
            }

            val response = BufferedReader(InputStreamReader(responseStream, StandardCharsets.UTF_8)).use {
                it.readText()
            }
            return response
        } finally {
            connection.disconnect()
        }
    }
}
