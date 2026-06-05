# user-server — 典型集成模式

> 客户端**不直连** `user-server`；一律经 API 网关。下列为典型集成场景与**用户域负责的数据块**；具体路由与字段映射以 API 网关文档为准。

## 1. 原则

- **鉴权**依赖网关调用本域 `IssueTokenPair`、`IntrospectAccessToken` 等
- **用户数据**（资料、偏好、收藏、历史、反馈、同意）以本域为 SoT；内容详情来自内容服务
- **个性化信号**由下游服务消费；本域只提供 **consent** 与 **signal_bundle_ref**
- **多应用**：各应用通过 `app_id` 隔离数据，共享同一套 API

---

## 2. 启动与账号

| 场景 | user-server 数据 | 备注 |
|------|------------------|------|
| 冷启动 / 访客态 | `EnsureGuestSession` | 与 `device_id`、`client_platform` 配合 |
| 登录 / 注册 | `IssueTokenPair` | 客户端仅存契约约定令牌 |
| 自动登录 / 刷新 | `IntrospectRefreshToken` + `IssueTokenPair` | 错误码 `20002`/`20003` 走重新登录 |
| Token 校验（每请求） | `IntrospectAccessToken` | 在网关执行，不在端上直连 |

---

## 3. 个人资料

| 场景 | user-server 数据 | 备注 |
|------|------------------|------|
| 用户摘要 | `GetMeSummary` 或聚合 `GetProfile` + counts | 一次请求获取资料+计数+同意 |
| 编辑资料 | `GetProfile` / `UpdateProfile` | 头像 URL 可能来自对象存储，本域存 ref |
| 偏好设置 | `GetPreferences` / `UpdatePreferences` | 版本化，支持乐观并发控制 |

---

## 4. 隐私与个性化

| 场景 | user-server 数据 | 备注 |
|------|------------------|------|
| 个性化与隐私开关 | `GetConsent` / `UpdateConsent` | 关闭后下游应降级 |
| 合规文案版本 | `consent_version` | 可展示「已同意条款版本」 |

---

## 5. 收藏

| 场景 | user-server 数据 | 备注 |
|------|------------------|------|
| 收藏列表 | `ListFavorites` | 内容详情需再请求内容服务 |
| 收藏 / 取消 | `AddFavorite` / `RemoveFavorite` | 支持任意内容类型 |

---

## 6. 历史

| 场景 | user-server 数据 | 备注 |
|------|------------------|------|
| 最近浏览 | `ListHistory` | 下架内容可由治理或内容服务过滤 |
| 清空历史 | `ClearHistory` | 支持全部清空或按时间清空 |

---

## 7. 反馈

| 场景 | user-server 数据 | 备注 |
|------|------------------|------|
| 内容评价 / 举报 | `SubmitFeedback` | 重度违规可能流转至治理服务 |

---

## 8. 隐式消费（无独立页面）

| 场景 | user-server 角色 |
|------|------------------|
| 个性化推荐/搜索 | 网关组装上下文：`GetConsent`、`GetSignalBundleRef`（及 `user_id`/`session_id`）传给下游服务 |
| 多端一致用户态 | `user_id`、`is_guest` 与鉴权契约摘要一致 |
| 跨应用用户识别 | 同一用户在不同应用中可有相同 `user_id`（共享账号）或不同 `user_id`（独立账号），由配置决定 |

---

## 9. 非目标

- 不在此文档定义 **HTTP 路径**；由 API 网关文档定义
- 不描述 **推荐/搜索排序规则**
- 不描述特定应用的 UI 交互细节
