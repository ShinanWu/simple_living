package com.simpleliving.android.data.gateway

import android.content.Context
import android.content.SharedPreferences
import java.util.UUID

class GatewaySettings(context: Context) {

    private val prefs: SharedPreferences = context.getSharedPreferences(
        "simple_living_gateway",
        Context.MODE_PRIVATE
    )

    var accessToken: String?
        get() = prefs.getString(KEY_ACCESS_TOKEN, null)
        set(value) {
            prefs.edit().putString(KEY_ACCESS_TOKEN, value).apply()
        }

    var refreshToken: String?
        get() = prefs.getString(KEY_REFRESH_TOKEN, null)
        set(value) {
            prefs.edit().putString(KEY_REFRESH_TOKEN, value).apply()
        }

    var guestSessionId: String?
        get() = prefs.getString(KEY_GUEST_SESSION_ID, null)
        set(value) {
            prefs.edit().putString(KEY_GUEST_SESSION_ID, value).apply()
        }

    val deviceId: String
        get() {
            var id = prefs.getString(KEY_DEVICE_ID, null)
            if (id == null) {
                id = UUID.randomUUID().toString()
                prefs.edit().putString(KEY_DEVICE_ID, id).apply()
            }
            return id
        }

    fun applyTokenPair(pair: TokenPairData) {
        prefs.edit().apply {
            putString(KEY_ACCESS_TOKEN, pair.access_token)
            putString(KEY_REFRESH_TOKEN, pair.refresh_token)
            putString(KEY_GUEST_SESSION_ID, null)
            apply()
        }
    }

    fun clearAuth() {
        prefs.edit().apply {
            putString(KEY_ACCESS_TOKEN, null)
            putString(KEY_REFRESH_TOKEN, null)
            apply()
        }
    }

    companion object {
        private const val KEY_ACCESS_TOKEN = "simple_living_access_token"
        private const val KEY_REFRESH_TOKEN = "simple_living_refresh_token"
        private const val KEY_GUEST_SESSION_ID = "simple_living_guest_session_id"
        private const val KEY_DEVICE_ID = "simple_living_device_id"
    }
}
