package com.simpleliving.android.ui.screen

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier

@Composable
fun GuideDetailPlaceholder() {
    CenteredPlaceholder(title = "GuideDetail", subtitle = "导购详情占位页")
}

@Composable
fun RedirectPreparePlaceholder() {
    CenteredPlaceholder(title = "RedirectPrepare", subtitle = "跳转准备占位页")
}

@Composable
fun MeSummaryPlaceholder() {
    CenteredPlaceholder(title = "MeSummary", subtitle = "我的摘要占位页")
}

@Composable
private fun CenteredPlaceholder(title: String, subtitle: String) {
    Column(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(text = title)
        Text(text = subtitle)
    }
}
