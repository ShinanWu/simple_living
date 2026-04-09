# iOS 交互实现说明

本文记录 iOS 端交互实现细节，必须遵循 `../cross-platform-interaction-consensus.md`。

## 首页（Home）当前实现

- 顶部 `衣/食/住/行`：支持点按 + 左右滑切换
- 底部 `Tab`：仅点按，不支持滑动切换
- 主区：单屏单卡片
- 上滑：下一条；下滑（非第一页）：上一条；第一页下滑松手判定刷新
- 仅点按卡片进入详情，拖拽期间不触发导航

## 登录（Login）对齐目标

- 登录入口：我的页「登录」toolbar；`LoginView` sheet（微信 + 手机号联调字段）
- 登录方式：`HttpGatewayAPI.issueTokenWithWeChat` / `issueTokenWithPhone`；`MockGatewayAPI` 内存模拟
- 退出：`logoutSession`；本地令牌持久化见 `HttpGatewaySettings.applyTokenPair` / `clearAuth`
- token：`MeSummaryFlowViewModel` 遇业务码 `20002` 时尝试一次 `refreshAuthTokens`

