package com.simpleliving.android.ui.screen

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ScrollableTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.simpleliving.android.data.gateway.GatewayApi
import com.simpleliving.android.data.gateway.model.HomeCard
import com.simpleliving.android.data.gateway.model.HomeFeedRequest
import com.simpleliving.android.model.Theme
import com.simpleliving.android.ui.state.UiState

@Composable
fun HomeScreen(
    gatewayApi: GatewayApi,
    onOpenGuideDetail: () -> Unit,
    onOpenRedirectPrepare: () -> Unit,
) {
    var selectedTheme by remember { mutableStateOf(Theme.Clothing) }
    var uiState by remember { mutableStateOf<UiState<List<HomeCard>>>(UiState.Loading) }

    LaunchedEffect(selectedTheme) {
        uiState = UiState.Loading
        uiState = runCatching {
            gatewayApi.getHomeFeed(HomeFeedRequest(theme = selectedTheme)).data.cards
        }.fold(
            onSuccess = { cards ->
                if (cards.isEmpty()) UiState.Empty else UiState.Success(cards)
            },
            onFailure = { UiState.Error(message = it.message ?: "加载失败") },
        )
    }

    Column(modifier = Modifier.fillMaxSize()) {
        ThemeTabs(
            selectedTheme = selectedTheme,
            onThemeSelected = { selectedTheme = it },
        )

        when (val state = uiState) {
            UiState.Loading -> StateText("加载中...")
            UiState.Empty -> StateText("暂无推荐内容")
            UiState.Offline -> StateText("当前离线，请检查网络")
            is UiState.Error -> StateText("请求失败：${state.message}")
            is UiState.Success -> HomeFeedList(
                cards = state.data,
                onOpenGuideDetail = onOpenGuideDetail,
                onOpenRedirectPrepare = onOpenRedirectPrepare,
            )
        }
    }
}

@Composable
private fun ThemeTabs(
    selectedTheme: Theme,
    onThemeSelected: (Theme) -> Unit,
) {
    val themes = Theme.entries
    val selectedIndex = themes.indexOf(selectedTheme).coerceAtLeast(0)
    ScrollableTabRow(selectedTabIndex = selectedIndex) {
        themes.forEach { theme ->
            Tab(
                selected = selectedTheme == theme,
                onClick = { onThemeSelected(theme) },
                text = { Text(text = theme.label) },
            )
        }
    }
}

@Composable
private fun HomeFeedList(
    cards: List<HomeCard>,
    onOpenGuideDetail: () -> Unit,
    onOpenRedirectPrepare: () -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        items(cards, key = { it.recommendation_id }) { card ->
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable(onClick = onOpenGuideDetail),
            ) {
                Column(modifier = Modifier.padding(12.dp)) {
                    Text(text = card.title)
                    Text(text = card.subtitle)
                    Text(text = "recommendation_id: ${card.recommendation_id}")
                    Button(onClick = onOpenRedirectPrepare) {
                        Text(text = "去购买（占位）")
                    }
                }
            }
        }
    }
}

@Composable
private fun StateText(text: String) {
    Column(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(text = text)
    }
}
