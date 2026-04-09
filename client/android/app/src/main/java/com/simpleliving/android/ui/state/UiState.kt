package com.simpleliving.android.ui.state

sealed interface UiState<out T> {
    data object Loading : UiState<Nothing>
    data class Success<T>(val data: T) : UiState<T>
    data object Empty : UiState<Nothing>
    data class Error(val code: Int? = null, val message: String) : UiState<Nothing>
    data object Offline : UiState<Nothing>
}
