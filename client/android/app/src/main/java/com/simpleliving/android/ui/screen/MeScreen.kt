package com.simpleliving.android.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import com.simpleliving.android.data.gateway.GatewayApi
import com.simpleliving.android.data.gateway.model.MeSummaryData
import com.simpleliving.android.ui.state.UiState

@Composable
fun MeScreen(
    gatewayApi: GatewayApi,
    onOpenMeSummary: () -> Unit,
) {
    var uiState by remember { mutableStateOf<UiState<MeSummaryData>>(UiState.Loading) }

    LaunchedEffect(Unit) {
        uiState = UiState.Loading
        uiState = runCatching { gatewayApi.getMeSummary().data }.fold(
            onSuccess = { UiState.Success(it) },
            onFailure = { UiState.Error(message = it.message ?: "加载失败") },
        )
    }

    when (val state = uiState) {
        UiState.Loading -> CenterText("加载中...")
        UiState.Empty -> CenterText("暂无个人数据")
        UiState.Offline -> CenterText("当前离线")
        is UiState.Error -> CenterText("请求失败：${state.message}")
        is UiState.Success -> MeContent(
            text = "我的：收藏 ${state.data.favorites_count}，历史 ${state.data.history_count}",
            onOpenMeSummary = onOpenMeSummary,
        )
    }
}

@Composable
private fun CenterText(text: String) {
    Column(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(text = text)
    }
}

@Composable
private fun MeContent(
    text: String,
    onOpenMeSummary: () -> Unit,
) {
    Column(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(text = text)
        Button(onClick = onOpenMeSummary) {
            Text(text = "查看摘要占位")
        }
    }
}
