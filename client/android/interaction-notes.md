# Android 交互实现说明

本文记录 Android 端交互实现细节，必须遵循 `../cross-platform-interaction-consensus.md`。

## 首页（Home）对齐目标

- 顶栏：搜索框（占位，placeholder「搜索少糖推荐」）+ 右侧「我的」入口图标
- 底部 `衣/食/住/行` 主题栏：支持点按 + 左右滑切换
- 主区：单屏单卡片
- 上滑：下一条；下滑（非第一页）：上一条；第一页下滑松手判定刷新
- 仅点按卡片进入详情，拖拽期间禁止误触发

## 登录（Login）对齐目标

- 登录入口：我的页"去登录"与受保护动作拦截统一使用登录页/弹层
- 登录方式：支持 `微信登录` 与 `手机号验证码登录`
- 手机号流程：输入手机号 -> 发送验证码 -> 输入验证码 -> 登录成功回跳
- token 过期处理：`20002` 自动 refresh 一次，`20003` 回退访客态并引导登录

## Gateway 实现

- `HttpGatewayApi`：真实 HTTPS + JSON 网关客户端，对齐微信小程序 `HttpGatewayAPI`
- `FakeGatewayRepository`：仅用于测试夹具，响应模型已补全
- `GatewaySettings`：令牌持久化与设备 ID 管理（SharedPreferences）
- Single-flight refresh：使用 `Mutex` 防止并发重复刷新
- 20002 自动重试：`requestWithAuth` 层自动拦截并触发 refresh

### 响应模型

所有模型字段与微信小程序 `types.ts` 和 iOS `GatewayAPI.swift` 保持一致：

- `HomeCard`：`id`、`guide_card_id`、`recommendation_id`、`scene`、`item_rank`、`title`、`reason`、`cover_url`
- `GuideDetailResponse`：`success`、`code`、`message`、`data`（含 `guide_card`/`guide` 嵌套对象）
- `MeSummaryResponse`：`profile`（含 `display_name`、`avatar_url`）、`counts`、`consent`
- 所有响应均包含 `success`、`code`、`message`、`data`、`meta` 信封结构
