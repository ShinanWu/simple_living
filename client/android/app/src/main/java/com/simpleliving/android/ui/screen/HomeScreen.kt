package com.simpleliving.android.ui.screen

import android.widget.Toast
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ScrollableTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRowDefaults
import androidx.compose.material3.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Search
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.blur
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.simpleliving.android.data.gateway.GatewayApi
import com.simpleliving.android.data.gateway.model.HomeCard
import com.simpleliving.android.data.gateway.model.HomeFeedRequest
import com.simpleliving.android.model.Theme
import com.simpleliving.android.ui.state.UiState

private val GlassWhite = Color(0xB3FFFFFF)
private val GlassBorder = Color(0x73FFFFFF)
private val GlassBorderSubtle = Color(0x26FFFFFF)
private val GlassShadow = Color(0x0F18181B)

private val ThemeAccentClothing = Color(0xFFE91E8C)
private val ThemeAccentFood = Color(0xFFF97316)
private val ThemeAccentHousing = Color(0xFF14B8A6)
private val ThemeAccentTransport = Color(0xFF3B82F6)

@Composable
fun HomeScreen(
    gatewayApi: GatewayApi,
    onOpenGuideDetail: () -> Unit,
    onOpenRedirectPrepare: () -> Unit,
    onOpenMe: () -> Unit,
) {
    var selectedTheme by remember { mutableStateOf(Theme.Clothing) }
    var uiState by remember { mutableStateOf<UiState<List<HomeCard>>>(UiState.Loading) }

    val accentColor = themeAccentColor(selectedTheme)

    LaunchedEffect(selectedTheme) {
        uiState = UiState.Loading
        uiState = runCatching {
            gatewayApi.getHomeFeed(HomeFeedRequest(theme = selectedTheme)).data?.items?.map { item ->
                val title = item.guide_card?.title ?: item.guide_card_id
                val reason = item.reason_text?.trim().takeUnless { it.isNullOrEmpty() } ?: "-"
                HomeCard(
                    id = "${item.recommendation_id}-${item.rank}",
                    guide_card_id = item.guide_card_id,
                    recommendation_id = item.recommendation_id,
                    scene = item.scene,
                    item_rank = item.rank,
                    title = title,
                    reason = reason,
                    cover_url = item.guide_card?.cover_url ?: item.guide_card?.cover_media?.url,
                )
            } ?: emptyList()
        }.fold(
            onSuccess = { cards ->
                if (cards.isEmpty()) UiState.Empty else UiState.Success(cards)
            },
            onFailure = { UiState.Error(message = it.message ?: "加载失败") },
        )
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        Color(0xFFFAFAFA),
                        accentColor.copy(alpha = 0.08f),
                        Color(0xFFECECEE),
                    )
                )
            )
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            TopBar(onOpenMe = onOpenMe)

            when (val state = uiState) {
                UiState.Loading -> StateText("加载中...")
                UiState.Empty -> StateText("暂无推荐内容")
                UiState.Offline -> StateText("当前离线，请检查网络")
                is UiState.Error -> StateText("请求失败：${state.message}")
                is UiState.Success -> HomeFeedList(
                    cards = state.data,
                    accentColor = accentColor,
                    onOpenGuideDetail = onOpenGuideDetail,
                    onOpenRedirectPrepare = onOpenRedirectPrepare,
                )
            }

            GlassThemeTabs(
                selectedTheme = selectedTheme,
                onThemeSelected = { selectedTheme = it },
            )
        }
    }
}

@Composable
private fun TopBar(onOpenMe: () -> Unit) {
    val context = LocalContext.current

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Row(
            modifier = Modifier
                .weight(1f)
                .clip(RoundedCornerShape(24.dp))
                .background(GlassWhite)
                .clickable {
                    Toast
                        .makeText(context, "搜索功能即将上线", Toast.LENGTH_SHORT)
                        .show()
                }
                .padding(horizontal = 14.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                imageVector = Icons.Default.Search,
                contentDescription = null,
                modifier = Modifier.size(18.dp),
                tint = Color.Gray,
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(text = "搜索少糖推荐", color = Color.Gray, style = MaterialTheme.typography.bodySmall)
        }

        Spacer(modifier = Modifier.width(12.dp))

        Box(
            modifier = Modifier
                .size(36.dp)
                .clip(CircleShape)
                .background(GlassWhite)
                .clickable(onClick = onOpenMe),
            contentAlignment = Alignment.Center,
        ) {
            Icon(
                imageVector = Icons.Default.Person,
                contentDescription = "我的",
                modifier = Modifier.size(20.dp),
                tint = Color.DarkGray,
            )
        }
    }
}

@Composable
private fun GlassThemeTabs(
    selectedTheme: Theme,
    onThemeSelected: (Theme) -> Unit,
) {
    val themes = Theme.entries
    val selectedIndex = themes.indexOf(selectedTheme).coerceAtLeast(0)

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .clip(RoundedCornerShape(24.dp))
            .background(GlassWhite.copy(alpha = 0.7f))
            .padding(4.dp),
        horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        themes.forEach { theme ->
            val isSelected = selectedTheme == theme
            Box(
                modifier = Modifier
                    .weight(1f)
                    .clip(RoundedCornerShape(20.dp))
                    .then(
                        if (isSelected) Modifier.background(Color.White.copy(alpha = 0.72f))
                        else Modifier
                    )
                    .clickable { onThemeSelected(theme) }
                    .padding(vertical = 10.dp),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = theme.label,
                    style = MaterialTheme.typography.labelLarge,
                    color = if (isSelected) Color.DarkGray else Color.Gray,
                )
            }
        }
    }
}

@Composable
private fun HomeFeedList(
    cards: List<HomeCard>,
    accentColor: Color,
    onOpenGuideDetail: () -> Unit,
    onOpenRedirectPrepare: () -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        items(cards, key = { it.id }) { card ->
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable(onClick = onOpenGuideDetail),
                shape = RoundedCornerShape(24.dp),
                colors = CardDefaults.cardColors(
                    containerColor = GlassWhite,
                ),
                elevation = CardDefaults.cardElevation(defaultElevation = 4.dp),
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(200.dp)
                            .clip(RoundedCornerShape(18.dp))
                            .background(
                                Brush.linearGradient(
                                    colors = listOf(
                                        accentColor.copy(alpha = 0.24f),
                                        accentColor.copy(alpha = 0.08f),
                                    )
                                )
                            ),
                        contentAlignment = Alignment.Center,
                    ) {
                        Text(text = card.title, style = MaterialTheme.typography.titleMedium)
                    }

                    Spacer(modifier = Modifier.height(12.dp))

                    Text(
                        text = "推荐理由",
                        style = MaterialTheme.typography.labelSmall,
                        color = Color.Gray,
                    )
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        text = card.reason,
                        style = MaterialTheme.typography.bodyMedium,
                        maxLines = 2,
                    )

                    Spacer(modifier = Modifier.height(12.dp))

                    Button(
                        onClick = onOpenRedirectPrepare,
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(24.dp),
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xFF18181B),
                        ),
                    ) {
                        Text(text = "去购买")
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
        Text(text = text, color = Color.Gray)
    }
}

private fun themeAccentColor(theme: Theme): Color = when (theme) {
    Theme.Clothing -> ThemeAccentClothing
    Theme.Food -> ThemeAccentFood
    Theme.Housing -> ThemeAccentHousing
    Theme.Transport -> ThemeAccentTransport
}
