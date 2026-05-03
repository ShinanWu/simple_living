# user-domain 与客户端页面映射

> 客户端**不直连** `user-domain`；一律经 `gateway`。下列为逻辑页面与**用户域负责的数据块**；路径示例仅供对齐，真实路由以 `services/gateway/docs/` 为准。

## 1. 原则

- **鉴权页、登录态**依赖 `gateway` 调用本域 `IssueTokenPair`、`IntrospectAccessToken` 等；契约层见 [auth.md](../../../docs/contracts/auth.md)。
- **个人中心、设置、收藏、历史**等以本域为 SoT；导购卡富文本仍来自 `content-domain`。
- **推荐列表**由 `recommendation-domain` 排序；本域只提供 **consent** 与 **signal_bundle_ref**（经 gateway 注入上下文）。

---

## 2. 启动与账号

| 页面 / 模块 | user-domain 数据 | 备注 |
|-------------|------------------|------|
| 冷启动 / 访客态 | `EnsureGuestSession` | 与请求体中的 `device_id`、`client_platform` 配合 |
| 登录 / 注册成功 | `IssueTokenPair` | 客户端仅存契约约定令牌 |
| 自动登录 / 刷新 | `IntrospectRefreshToken` + `IssueTokenPair` | 错误码 `20002`/`20003` 走重新登录 |
| Token 校验（每请求） | `IntrospectAccessToken` | 在 gateway，不在端上直连 |

---

## 3. 「我的」与个人资料

| 页面 / 模块 | user-domain 数据 | 备注 |
|-------------|------------------|------|
| 我的首页摘要 | `GetMeSummary` 或聚合 `GetProfile` + counts | 与 [gateway pages.md](../../gateway/docs/pages.md) `me_summary` 对齐 |
| 编辑资料 | `GetProfile` / `UpdateProfile` | 头像 URL 可能来自对象存储，本域存 ref |
| 偏好设置 | `GetPreferences` / `UpdatePreferences` | 非推荐引擎配置名，避免与 `recommendation-domain` 策略混淆 |

---

## 4. 隐私与个性化

| 页面 / 模块 | user-domain 数据 | 备注 |
|-------------|------------------|------|
| 个性化与隐私开关 | `GetConsent` / `UpdateConsent` | 关闭后推荐应降级（由推荐域执行） |
| 合规文案版本 | `consent_version` | 可展示「已同意条款版本」 |

---

## 5. 收藏

| 页面 / 模块 | user-domain 数据 | 备注 |
|-------------|------------------|------|
| 收藏列表 | `ListFavorites` | 卡片展示需再请求 `content-domain` `BatchGetGuideCards` |
| 收藏 / 取消 | `AddFavorite` / `RemoveFavorite` | 卡片详情页操作 |

---

## 6. 历史

| 页面 / 模块 | user-domain 数据 | 备注 |
|-------------|------------------|------|
| 最近浏览 | `ListHistory` | 下架内容可由治理或内容域过滤；见集成策略 |
| 清空历史 | `ClearHistory` | |

---

## 7. 反馈

| 页面 / 模块 | user-domain 数据 | 备注 |
|-------------|------------------|------|
| 内容差评 / 举报入口 | `SubmitFeedback` | 重度违规流转可能涉及 `governance-domain`，不在本页展开 |

---

## 8. 隐式消费（无独立页面）

| 场景 | user-domain 角色 |
|------|------------------|
| 首页/频道推荐 | `gateway` 组装上下文：`GetConsent`、`GetSignalBundleRef`（及 `user_id`/`session_id`）传给 `recommendation-domain` |
| 多端一致用户态 | `user_id`、`is_guest` 与 [auth.md](../../../docs/contracts/auth.md) 摘要一致 |

---

## 9. 非目标

- 不在此文档定义 **HTTP 路径**；由 `gateway` 的 `api.md` / `pages.md` 定义。
- 不描述 **推荐卡片排序规则**。
