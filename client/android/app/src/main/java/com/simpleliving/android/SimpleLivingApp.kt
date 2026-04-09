package com.simpleliving.android

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.BottomAppBar
import androidx.compose.material3.BottomAppBarDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import com.simpleliving.android.data.gateway.FakeGatewayRepository
import com.simpleliving.android.ui.screen.GuideDetailPlaceholder
import com.simpleliving.android.ui.screen.HomeScreen
import com.simpleliving.android.ui.screen.MeScreen
import com.simpleliving.android.ui.screen.MeSummaryPlaceholder
import com.simpleliving.android.ui.screen.RedirectPreparePlaceholder

private enum class BottomRoute(val label: String) {
    Home("首页"),
    Me("我的"),
}

@Composable
fun SimpleLivingApp() {
    val gatewayApi = remember { FakeGatewayRepository() }
    var bottomRoute by remember { mutableStateOf(BottomRoute.Home) }
    var currentPage by remember { mutableStateOf("home") }

    MaterialTheme {
        Scaffold(
            bottomBar = {
                BottomAppBar(containerColor = BottomAppBarDefaults.containerColor) {
                    BottomRoute.entries.forEach { route ->
                        NavigationBarItem(
                            selected = bottomRoute == route,
                            onClick = {
                                bottomRoute = route
                                currentPage = if (route == BottomRoute.Home) "home" else "me"
                            },
                            icon = {},
                            label = { Text(text = route.label) },
                        )
                    }
                }
            },
        ) { innerPadding ->
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(innerPadding),
            ) {
                when (currentPage) {
                    "guide_detail" -> GuideDetailPlaceholder()
                    "redirect_prepare" -> RedirectPreparePlaceholder()
                    "me_summary" -> MeSummaryPlaceholder()
                    "home" -> HomeScreen(
                        gatewayApi = gatewayApi,
                        onOpenGuideDetail = { currentPage = "guide_detail" },
                        onOpenRedirectPrepare = { currentPage = "redirect_prepare" },
                    )
                    else -> MeScreen(
                        gatewayApi = gatewayApi,
                        onOpenMeSummary = { currentPage = "me_summary" },
                    )
                }
            }
        }
    }
}
