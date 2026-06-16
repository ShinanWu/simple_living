# 微信小程序交互实现说明

## 1. 与全局共识的关系

- 页面状态、手势语义、登录状态机遵循 [cross-platform-interaction-consensus.md](../cross-platform-interaction-consensus.md)。
- 本文仅记录**微信小程序宿主**下的实现差异与约束。

## 2. 导航

| 能力 | 实现 |
|------|------|
| 首页 → 我的 | 首页顶栏右侧图标 → `navigateTo` `/pages/me/me` |
| 详情 / 跳转 | `navigateTo` 子页面；返回使用 `navigateBack` |
| 登录 | 独立页 `/pages/login/login`；支持 `return_url` 回跳 |

## 3. 首页卡片

| 能力 | 实现 |
|------|------|
| 上下滑切卡 | 原生 **`swiper` 纵向**；每张卡一个 `swiper-item`，数据直接 `wx:for="{{cards}}"` |
| 主题切换 | 底部 Tab 点击；主题栏横滑（阈值约 54px） |
| 首张刷新 | 第一张卡内 `scroll-view` 原生下拉刷新（`refresher`）；空态/错误态「重试」 |
| 末张提示 | 滑到最后一张时顶部展示 `brandEndHint` |
| 卡片按钮 | **收藏** + **详情**；点卡片或「详情」进详情页 |
| 数据刷新 | 冷启动预拉四主题缓存；切主题不重复请求；刷新仅当前主题 |
| 翻页加载 | `has_more` + `next_cursor`；滑到倒数第二张附近追加 |
| 回到顶部 | `swiperCurrent > 0` 时右下悬浮圆钮，`swiperCurrent` 置 0 |

`swiper`：`duration=320`、`easing-function=easeOutCubic`、`skip-hidden-item-layout`。

主题 Tab 选中态使用对应 `--tab-accent` 微光晕胶囊。

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
| 准备 | `POST /api/v2/pages/redirect_prepare`；`landing_url` 为空时展示可重试错误态，不展示技术字段 |
| 展示 | 商品标题 +「打开购买页」主按钮 +「复制链接」次按钮 |
| 打开 | 优先 `navigateTo` `web-outbound`（`web-view`）；失败则提示配置业务域名 |

## 6. 收藏与历史

- 首页 / 详情页「收藏」：未登录 `navigateTo` 登录页；登录后 `navigateBack` 回首页时由 `pendingHomeFavoriteGuide` 自动完成收藏，详情页回跳 `action=favorite`。
- 我的页入口：收藏 / 历史列表页，分别调用 `GET /api/v2/me/favorites`、`GET /api/v2/me/history`。

## 7. 埋点（最小）

使用 `console` 结构化日志 + 预留 `reportAnalytics` 封装（未接第三方时 no-op），事件名风格：`page_action_result`，字段 `snake_case`。

## 8. 验收

与 [frontend-mock-and-acceptance.md](../frontend-mock-and-acceptance.md) 矩阵一致；另见 `npm test` 与开发者工具真机预览。
