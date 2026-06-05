# 微信小程序交互实现说明

## 1. 与全局共识的关系

- 页面状态、手势语义、登录状态机遵循 [cross-platform-interaction-consensus.md](../cross-platform-interaction-consensus.md)。
- 本文仅记录**微信小程序宿主**下的实现差异与约束。

## 2. 导航

| 能力 | 实现 |
|------|------|
| 首页 → 我的 | 首页顶栏右侧图标 → `navigateTo` `/pages/me/me` |
| 详情 / 跳转 | `navigateTo` 子页面；返回使用 `navigateBack` |
| 登录 | 独立页 `/pages/login/login`；支持 `redirect` 回跳参数 |

## 3. 首页手势（对齐 iOS 阈值）

| 手势 | 行为 |
|------|------|
| 左右滑（主题栏或卡片区） | 切换 `clothing` → `food` → `housing` → `transport` |
| 上滑 | 下一条推荐 |
| 下滑（非首卡） | 上一条 |
| 下滑（首卡） | 二段提示；松手超过阈值触发刷新 |
| 拖拽中 | `isCardDragging` 为 true 时不触发进入详情 |

阈值（px，与 iOS 逻辑同量级）：下一条 90、刷新提示 28、刷新触发 92、主题横滑 54。

## 4. 登录

| 方式 | 实现 |
|------|------|
| 微信 | 主按钮 `wx.login` → `issue_token(oauth)`；`client_platform=wechat_miniprogram` |
| 手机号 | 输入框 + 验证码 + `verification_id`（联调/测试环境） |
| 协议 | 固定展示用户协议与隐私说明文案区 |

错误文案使用 [frontend-login-error-copy.md](../frontend-login-error-copy.md) 映射表。

## 5. 外链跳转

| 步骤 | 实现 |
|------|------|
| 准备 | `POST /api/v2/pages/redirect_prepare` |
| 复制链接 | `wx.setClipboardData` |
| 打开 | 优先 `navigateTo` `web-outbound`（`web-view`）；失败则提示配置业务域名 |

## 6. 收藏与历史

- 详情页「收藏」：未登录 `navigateTo` 登录页并带 `action=favorite` 回跳参数。
- 我的页入口：收藏 / 历史列表页，分别调用 `GET /api/v2/me/favorites`、`GET /api/v2/me/history`。

## 7. 埋点（v1 最小）

使用 `console` 结构化日志 + 预留 `reportAnalytics` 封装（未接第三方时 no-op），事件名风格：`page_action_result`，字段 `snake_case`。

## 8. 验收

与 [frontend-mock-and-acceptance.md](../frontend-mock-and-acceptance.md) 矩阵一致；另见 `npm test` 与开发者工具真机预览。
