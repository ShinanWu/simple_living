# iOS 交互实现说明

本文记录 iOS 端交互实现细节，必须遵循 `../cross-platform-interaction-consensus.md`。

## 首页（Home）当前实现

- 顶栏：搜索框（占位，placeholder「搜索少糖推荐」）+ 右侧「我的」入口图标
- 底部固定 `衣/食/住/行` 主题栏：支持点按 + 左右滑切换
- 主区：单屏单卡片（图片区占主视觉，推荐理由区固定两行省略）
- 上滑：下一条；下滑（非第一页）：上一条；第一页下滑松手判定刷新
- 最后一条继续上滑时显示"少糖就先到这里吧"收尾提示，并保持停留在最后一条
- 仅点按卡片进入详情，拖拽期间不触发导航
- 详情页使用可滚动布局，顶部为可左右滑的多图画廊，并展示完整文案

## 文档先行补录说明（2026-04-14）

- 本次首页改版属于交互流程调整（标签位置从顶部改为底部），按规范应先更新页面文档再实现。
- 实际执行中先完成了 `HomeView` 验证，以快速确认底部标签与卡片手势是否可共存，随后补录页面规格与信息架构文档。
- 该变更不涉及 JSON/`proto` 契约，不影响网关字段语义；影响范围限定在 iOS 表现层与客户端页面规格。

## 登录（Login）对齐目标

- 登录入口：我的页「登录」toolbar；`LoginView` sheet（微信 + 手机号联调字段）
- 登录方式：运行时使用 `HttpGatewayAPI.issueTokenWithWeChat` / `issueTokenWithPhone`；`MockGatewayAPI` 仅用于测试夹具
- 退出：`logoutSession`；本地令牌持久化见 `HttpGatewaySettings.applyTokenPair` / `clearAuth`
- token 刷新：`HttpGatewayAPI` 内置 single-flight refresh 机制，防止并发重复刷新
- 20002 自动重试：`HttpGatewayAPI.dataTask` 层自动拦截 `code=20002` 并触发 refresh，刷新失败回退访客态

## Gateway 响应模型（2026-05-28 补全）

- `HomeCard` 新增 `coverUrl` 字段，映射自 `guide_card.cover_url` 或 `guide_card.cover_media.url`
- `GuideDetailResponse` 新增 `subtitle`、`coverUrl`、`galleryUrls`、`isCommercial`、`disclosureText`
- `MeSummaryResponse` 新增 `displayName`、`avatarUrl`，映射自 `profile.display_name` / `profile.avatar_url`
- 所有字段语义与微信小程序 `types.ts` 保持一致

