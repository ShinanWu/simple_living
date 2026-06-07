# Gateway 对外 HTTP API（v2 交付面）

## 1. 文档范围与版本

- **范围**：终端经前置 `Nginx` 到 `gateway` 的 **HTTPS + JSON** 约定；本文件包含 **可直接实现** 的细粒度用户路由与页面聚合路由、请求/响应 `data` 形状、嵌套对象、枚举、错误码与到各域 RPC 的映射说明。
- **路径版本**：`/api/v2/...` 与内部 `simple_living.user_server` / `simple_living.gateway.user` proto 对齐；破坏性变更通过新路径或迁移期在 changelog 说明。
- **字段命名**：JSON 一律 **`snake_case`**，与全局契约一致。
- **时间**：业务时间字段在 JSON 中为 **ISO 8601 UTC 字符串**（例 `2026-03-28T12:00:00Z`）；机器时间戳仍可由网关写入信封 `meta.server_time_ms`（Unix 毫秒）。
- **Proto 骨架**：`services/gateway/proto/gateway_user_http_messages.proto`、`gateway_user_edge.proto`、`gateway_pages_edge.proto`；下游类型见各域 `services/<service>/proto/`。

## 2. 传输与通用头

| 项 | 约定 |
|----|------|
| 协议 | HTTPS |
| 请求体 | `Content-Type: application/json`（本文件所列 POST/PATCH/PUT/DELETE 带体路由） |
| 编码 | UTF-8 |

### 2.0 前置 Nginx 约束

- 生产入口建议采用 `Nginx -> gateway` 拓扑，`Nginx` 负责 TLS 终止、通用反向代理、基础限流与连接治理。
- `Nginx` 不定义或改写业务 JSON 字段语义，不承担页面聚合与 JSON ↔ proto 映射职责。
- 业务契约、错误码语义与字段稳定性仍以本文件和 `.cursor/rules/shared-contracts.mdc` 为准。

| 请求头 | 必填 | 说明 |
|--------|------|------|
| `Authorization` | 按路由 | 已登录路由使用 `Bearer <access_token>` |
| `X-Request-Id` | 否 | 可参与生成；响应 `meta.request_id` **以网关最终值为准** |

### 2.1 主体载体（登录 / 访客）

- **已登录主体**：使用 `Authorization: Bearer <access_token>`；网关调用 `IntrospectAccessToken` 后注入 `user_id`、`session_id`。
- **访客主体**：不使用独立访客 Bearer；通过 `POST /api/v2/guest/session` 申请 `session_id`，后续访问“Bearer 或访客会话”路由时，使用请求头 `X-Guest-Session-Id: <session_id>` 传递。
- **优先级**：若同时携带登录 Bearer 与 `X-Guest-Session-Id`，以 Bearer 解析出的登录主体为准，忽略访客会话头。
- **未携带主体**：对标记为“Bearer 或访客会话”的路由返回 `20001`。

### 2.2 单一来源规则（身份来自头，业务上下文来自 body）

- 身份主体仅来自请求头：`Authorization` 或 `X-Guest-Session-Id`。
- 客户端业务上下文统一来自请求体字段（含顶层字段或体内 `request_context`）；网关不依赖 `X-Client-*` 头构建业务上下文。
- 网关不接受体内 `acting_user_id` / `session_id` 作为主体来源。
- 若未来为 Nginx 增加上下文相关请求头，这些头仅供网关前置层使用，不参与业务语义融合。

## 3. 响应信封（顶层）

所有 JSON 响应体必须符合以下 **顶层** 结构（与全局 common-response 语义一致）：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `success` | boolean | 是 | `true` 当且仅当 `code === 0` |
| `code` | integer | 是 | `0` 成功；非零见 §8 |
| `message` | string | 是 | 人类可读；**禁止**作为唯一分支依据 |
| `data` | object \| null | 是 | 成功时为业务负载；失败时多为 `null` |
| `meta` | object | 否 | 含 `request_id`、`trace_id`、`server_time_ms` 等 |
| `errors` | array | 否 | 校验失败时的字段级明细 |
| `warnings` | array | 否 | 仅允许在 `success === true` 时出现 |

**不变量**：`code === 0` ⇔ `success === true`；`code !== 0` ⇔ `success === false`。

## 4. 分页（游标模式）

列表类接口在 `data` 内统一使用：

| 位置 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 请求 query | `cursor` | string | 上一页 `data.pagination.next_cursor`；首页省略或空 |
| 请求 query | `limit` | integer | 默认与上限由路由表给出；**上限 100** |
| `data` | `items` | array | 列表元素 |
| `data.pagination` | `next_cursor` | string \| null | 下一页游标；无更多为 `null` |
| `data.pagination` | `has_more` | boolean | 是否还有下一页 |
| `data.pagination` | `limit` | integer | 本页请求的最大条数 |

## 5. 嵌套对象定义（`data` 内复用）

### 5.1 `notification_prefs`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `email_enabled` | boolean | 否 | 默认由产品约定 |
| `push_enabled` | boolean | 否 | |
| `sms_enabled` | boolean | 否 | |
| `in_app_enabled` | boolean | 否 | |

### 5.2 `theme_preferences`（`preferences.structured`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `theme_interests` | string[] | 否 | 产品 taxonomy key，如 `clothing` |
| `content_filters` | object | 否 | 键值对；过滤类配置 |
| `default_sort` | string | 否 | 前端默认排序 key |

### 5.3 `preferences`（用户产品偏好）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `preferences_version` | integer | 否 | 单调递增；缓存失效 |
| `structured` | object | 否 | 见 `theme_preferences` |

### 5.4 `profile`（用户可见资料子集）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `user_id` | string \| null | 是 | 访客可为 `null` |
| `is_guest` | boolean | 是 | |
| `display_name` | string | 否 | |
| `avatar_url` | string | 否 | |
| `locale` | string | 否 | BCP 47 或产品约定 |
| `bio` | string | 否 | |
| `notification_prefs` | object | 否 | 见 §5.1 |

### 5.5 `content_ref`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `type` | string | 是 | 枚举 `content_ref_type` |
| `guide_card_id` | string | 条件 | `type === guide_card` 时必填 |

### 5.6 `favorite_item`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `favorite_id` | string | 是 | |
| `guide_card_id` | string | 是 | |
| `favorited_at` | string | 是 | ISO 8601 UTC |

### 5.7 `history_item`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_ref` | object | 是 | 见 §5.5 |
| `last_seen_at` | string | 是 | ISO 8601 UTC |
| `first_seen_at` | string | 否 | ISO 8601 UTC |
| `impression_count` | integer | 否 | 默认 0 或省略 |
| `source_surface` | string | 否 | 枚举 `history_source_surface` |

### 5.8 `consent`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `personalization_allowed` | boolean | 是 | |
| `analytics_allowed` | boolean | 否 | |
| `marketing_allowed` | boolean | 否 | |
| `consent_version` | string | 是 | 策略文档版本 |
| `updated_at` | string | 是 | ISO 8601 UTC |
| `jurisdiction` | string | 否 | 地域/法域标记 |

### 5.9 `me_counts`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `favorites_count` | integer | 是 | |
| `history_count` | integer | 是 | |

### 5.10 请求上下文来源

`request_context` 可作为对外 JSON 入参，且仅作为业务上下文来源之一。客户端上下文优先来自请求体，不依赖 `X-Client-*` 头。

### 5.11 `account_proof`（登录发令牌，oneof 形态）

请求体 **三选一**：

| 判别字段 | 类型 | 说明 |
|----------|------|------|
| `user_id` | string | 已存在用户直接发令牌 |
| `phone_otp` | object | `{ "phone_e164", "otp_code", "verification_id" }` |
| `oauth` | object | `{ "provider", "provider_subject", "authorization_code" }` |

## 6. 枚举（JSON 字符串值）

| 枚举名 | 允许值 | 备注 |
|--------|--------|------|
| `client_platform` | `web`, `ios`, `android`, `wechat_miniprogram`, `douyin_miniprogram` | 与鉴权契约头约定一致 |
| `content_ref_type` | `guide_card` | 扩展需协同 platform/backoffice-backend |
| `favorite_content_type` | `guide_card` | 列表筛选；默认 `guide_card` |
| `feedback_target_type` | `guide_card`, `recommendation_result`, `app`, `other` | |
| `history_source_surface` | `feed`, `search`, `detail` | |
| `clear_history_scope` | `all`, `before_time` | `before_time` 须配合 `before_time` 时间字段 |
| `revoke_scope`（DELETE `/auth/session` 体字段） | `single_session`, `all_user_sessions` | 映射 `RevokeSessionScope` |

**映射到 proto**：网关将字符串枚举转为 `user_server_models.proto` 中对应 `enum` 数值（实现侧维护表；新增值仅追加）。

## 7. 路由总表（用户相关 v2）

| 方法 | 路径 | 鉴权 | 默认 limit | 主下游 RPC（`UserServerService`） |
|------|------|------|------------|-----------------------------------|
| POST | `/api/v2/guest/session` | 无 | — | `EnsureGuestSession` |
| POST | `/api/v2/auth/token/issue` | 无（由上游校验链保证） | — | `IssueTokenPair` |
| POST | `/api/v2/auth/token/refresh` | 无 | — | `IntrospectRefreshToken` → `IssueTokenPair` |
| DELETE | `/api/v2/auth/session` | Bearer | — | `RevokeSession` |
| GET | `/api/v2/me/profile` | Bearer 或访客会话 | — | `GetProfile` |
| PATCH | `/api/v2/me/profile` | Bearer | — | `UpdateProfile` |
| GET | `/api/v2/me/preferences` | Bearer | — | `GetPreferences` |
| PUT | `/api/v2/me/preferences` | Bearer | — | `UpdatePreferences` |
| GET | `/api/v2/me/favorites` | Bearer | 20 | `ListFavorites` |
| POST | `/api/v2/me/favorites` | Bearer | — | `AddFavorite` |
| DELETE | `/api/v2/me/favorites/{favorite_id}` | Bearer | — | `RemoveFavorite` |
| GET | `/api/v2/me/history` | Bearer 或访客 | 20 | `ListHistory` |
| POST | `/api/v2/me/history/events` | Bearer 或访客 | — | `RecordHistoryEvent` |
| DELETE | `/api/v2/me/history` | Bearer 或访客 | — | `ClearHistory` |
| POST | `/api/v2/me/feedback` | Bearer 或访客 | — | `SubmitFeedback` |
| GET | `/api/v2/me/consent` | Bearer | — | `GetConsent` |
| PUT | `/api/v2/me/consent` | Bearer | — | `UpdateConsent` |
| GET | `/api/v2/me/summary` | Bearer 或访客 | — | `GetMeSummary` |
| GET | `/api/v2/health` | 无 | — | `HealthCheck`（可与其他域探活组合） |

**内部（非表列 HTTP）**：`IntrospectAccessToken`、`GetSignalBundleRef`、`ResolveSignalBundle` 由网关在入口或 BFF 流程中直接调用 `UserServerService`，**不**对终端暴露独立路径。

## 8. 错误码与 HTTP 状态

客户端以 JSON **`code`** 为准。完整码值区间与语义以 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) §错误码为准，本节只列 gateway 实际使用/映射的码及触发条件，不重述全文。

### 8.1 网关入口层错误码（`10050`–`10099` 预留区）

以下码由 gateway **入口/路由/聚合层**产生（非透传自业务域），取值固定，仅允许在区间内追加：

| `code` | 语义 | 典型 HTTP | 触发条件 |
|--------|------|-----------|----------|
| `10050` | 路由不存在 | 404 | 请求 path/method 未匹配任何已登记路由 |
| `10051` | 方法不被允许 | 405 | path 命中但 HTTP 方法不支持 |
| `10052` | 请求体解析失败 | 400 | JSON 语法错误或非 `application/json` 体 |
| `10053` | 网关聚合部分失败（已降级） | 200 | 页面聚合中可降级依赖失败，返回部分数据 + `warnings`（仅 `success===true` 时） |
| `10054` | 网关聚合关键依赖失败 | 502/504 | 页面聚合中**不可降级**的关键下游失败，整页失败（HTTP 依失败类型取 502/504） |

### 8.2 通用 / 鉴权 / 资源类（入口校验或透传细分）

| `code` | 语义 | 典型 HTTP |
|--------|------|-----------|
| `0` | 成功 | 200 |
| `10001` | 缺少必填参数 | 400 |
| `10002` | 参数校验失败 | 400 |
| `10003` | 不支持的内容类型 | 415/400 |
| `10004` | 接口版本不支持 | 400/426 |
| `10005` | 请求频率超限 | 429 |
| `10006` | 请求体过大 | 413 |
| `10007` | 版本冲突（乐观锁） | 409 |
| `20001` | 未登录或会话缺失 | 401 |
| `20002` | 访问令牌无效或过期 | 401 |
| `20003` | 刷新令牌无效或过期 | 401 |
| `20004` | 权限不足 | 403 |
| `20005` | 设备或环境被拒绝 | 403 |
| `30001` | 资源不存在 | 404 |
| `90001` | 内部错误 | 500 |
| `90002` | 依赖超时 | 504 |
| `90003` | 依赖错误 | 502 |

### 8.3 业务域错误码透传

gateway 对客户端**透传**各业务域返回的领域码，不重映射其语义（仅在缺乏明确业务码时按 §8.4 的 gRPC 兜底映射）：

| 区间 | 来源域 | 典型码（语义见各域 `api.md` + 共享契约） |
|------|--------|------------------------------------------|
| `30000`–`30999` | platform/backoffice-backend（见 `../../platform/docs/backend-api.md`） | `30001` 不存在、`30002` 已下架/不可见、`30003` 未发布 |
| `40000`–`40999` | recommendation-server（见 `../../recommendation-server/docs/api.md`） | `40001` 场景不支持、`40002` 推荐不可用、`40003` 已降级 |
| `50000`–`50999` | affiliate / tracking（见 `../../tracking-server/docs/api.md`、`../../platform/docs/backend-api.md`） | `50001` 渠道不可用、`50002` 转链失败、`50003` 链接失效 |
| `60000`–`60999` | platform/backoffice-backend（见 `../../platform/docs/backend-api.md`） | `60001` 合规拦截、`60002` 地域/策略限制 |

透传规则：顶层 `code` 用主因码；字段级细分放 `errors[].code`（仍在区间内）；gateway **不**泄露内部栈、内部服务名与 proto 字段名。

### 8.4 gRPC → JSON 兜底映射

当下游未返回明确业务码时，按下游 gRPC `Code` 兜底映射；业务码始终放入信封 `code`（非仅 `message`）：

| gRPC `Code` | 默认 JSON `code` | 默认 HTTP | 说明 |
|-------------|------------------|-----------|------|
| `OK` | `0` | 200 | 成功 |
| `INVALID_ARGUMENT` | `10002` | 400 | 参数校验失败；缺字段可细化为 `10001` |
| `FAILED_PRECONDITION` | `10002` | 400 | 状态不满足；若有更细业务码，以业务码为准 |
| `UNAUTHENTICATED` | `20001` / `20002` / `20003` | 401 | 依据 access/refresh/主体缺失细分 |
| `PERMISSION_DENIED` | `20004` / `20005` | 403 | 权限不足或设备/环境拒绝 |
| `NOT_FOUND` | `30001` | 404 | 资源不存在 |
| `RESOURCE_EXHAUSTED` | `10005` | 429 | 频率或配额限制 |
| `DEADLINE_EXCEEDED` | `90002` | 504 | 依赖超时 |
| `UNAVAILABLE` | `90002` | 504 | 依赖暂时不可用 |
| `INTERNAL` | `90001` | 500 | 内部错误 |
| `UNKNOWN` | `90001` | 500 | 未分类错误 |
| 其他未列举 | `90003` | 502 | 依赖返回不符合预期的错误 |

## 9. 分路由说明与 `data` 形状

### 9.1 `POST /api/v2/guest/session`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `device_id` | string | 是 |
| `client_platform` | string | 是 |
| `app_version` | string | 否 |

| `data` | 类型 |
|------|------|
| `session_id` | string |
| `created` | boolean |

**映射**：`EnsureGuestSessionRequest` / `EnsureGuestSessionResponse`；头字段可合并进 `RpcRequestContext`（若实现扩展）。

### 9.2 `POST /api/v2/auth/token/issue`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `account_proof` | object | 是 | 见 §5.11（三选一） |
| `device_fingerprint` | string | 否 |
| `client_platform` | string | 否 |
| `app_version` | string | 否 |
| `device_id` | string | 否 |

| `data` | 类型 | 说明 |
|------|------|------|
| `access_token` | string | |
| `refresh_token` | string | |
| `expires_in` | integer | **秒**；源自 proto `expires_in_seconds` |
| `session_id` | string | |
| `access_expires_at` | string | ISO 8601 UTC；源自 proto `access_expires_at` |

**映射**：`IssueTokenPair`；须在网关完成登录证明校验（OTP/OAuth 等）后再调 RPC。

`account_proof` 到内部 `IssueTokenPairRequest` 的映射固定为：

- `account_proof.user_id` → `IssueTokenPairRequest.user_id`
- `account_proof.phone_otp` → `IssueTokenPairRequest.phone_otp`
- `account_proof.oauth` → `IssueTokenPairRequest.oauth`
- `device_fingerprint` → `IssueTokenPairRequest.device_fingerprint`
- `client_platform` / `app_version` / `device_id` 由请求体字段写入对应内部上下文字段
- OTP / OAuth 的外部校验在网关完成；`user-server` 不重复承担第三方证明交换职责

### 9.3 `POST /api/v2/auth/token/refresh`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `refresh_token` | string | 是 |
| `request_context` | object | 否 | 仅以请求体为来源，不从请求头补齐 |

**流程**：`IntrospectRefreshToken`；失败返回 `20003`；成功则 `IssueTokenPair`（实现可合并 rotation 策略）。`data` 同 §9.2。

### 9.4 `DELETE /api/v2/auth/session`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `revoke_scope` | string | 否 | 默认 `single_session`；登出所有设备为 `all_user_sessions` |

**映射**：`RevokeSessionRequest`：`session_id` / `user_id` 从令牌解析注入，`scope` 由 `revoke_scope` 映射。成功 `data`: `{ "revoked": true }`。

### 9.5 `GET /api/v2/me/profile`

无 query。网关从令牌注入 `user_id` 或访客 `session_id`。

`data`: `{ "profile": { ... §5.4 } }` — 对应 `GetProfileResponse`。

### 9.6 `PATCH /api/v2/me/profile`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `display_name` | string | 否 |
| `avatar_url` | string | 否 |
| `locale` | string | 否 |
| `bio` | string | 否 |
| `notification_prefs` | object | 否 |

`data`: `{ "profile": { ... } }` — `UpdateProfileResponse`。

### 9.7 `GET /api/v2/me/preferences`

`data`: `{ "preferences": { ... §5.3 } }`。

### 9.8 `PUT /api/v2/me/preferences`

请求体：`preferences` 对象（§5.3）。`data` 同响应全量偏好。

### 9.9 `GET /api/v2/me/favorites`

| Query | 类型 |
|-------|------|
| `cursor` | string |
| `limit` | integer |
| `content_type` | string | 默认 `guide_card` |

`data`: `{ "items": [ favorite_item... ], "pagination": { ... } }`。

### 9.10 `POST /api/v2/me/favorites`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `guide_card_id` | string | 是 |

`data`: `{ "favorite_id": string, "already_favorited": boolean }`。

### 9.11 `DELETE /api/v2/me/favorites/{favorite_id}`

路径参数 `favorite_id`。`data`: `{ "removed": boolean }`。

### 9.12 `GET /api/v2/me/history`

Query：`cursor`, `limit`。主体归属由登录态或访客 `session_id` 推导。

`data`: `{ "items": [ history_item... ], "pagination": { ... } }`。

### 9.13 `POST /api/v2/me/history/events`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `content_ref` | object | 是 |
| `occurred_at` | string | 否 | 缺省为服务端收到时间 |
| `source_surface` | string | 否 |
| `client_event_id` | string | 否 | 幂等去重 |

`data`: `{ "recorded": boolean, "deduplicated": boolean }`。

### 9.14 `DELETE /api/v2/me/history`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `scope` | string | 是 | `all` 或 `before_time` |
| `before_time` | string | 条件 | `scope === before_time` 时 ISO 8601 |

`data`: `{ "removed_count": integer }`。

### 9.15 `POST /api/v2/me/feedback`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `target_type` | string | 是 |
| `target_id` | string | 是 |
| `rating` | number | 否 |
| `reason_codes` | string[] | 否 |
| `free_text` | string | 否 |
| `client_request_id` | string | 否 |

`data`: `{ "feedback_id": string }`。

### 9.16 `GET /api/v2/me/consent`

`data`: `{ "consent": { ... §5.8 } }`。

### 9.17 `PUT /api/v2/me/consent`

请求体：`consent` 对象（§5.8 字段子集可更新，实现须校验策略）。`data` 返回更新后 `consent`。

### 9.18 `GET /api/v2/me/summary`

`data`:

```json
{
  "profile": { },
  "counts": { "favorites_count": 0, "history_count": 0 },
  "consent": { }
}
```

对应 `GetMeSummaryResponse`。

### 9.19 `GET /api/v2/health`

`data`: `{ "status": string, "components": [ { "name": string, "ok": boolean, "detail": string } ] }` — 对应 `HealthCheckResponse` 的可公开子集（实现可裁剪内部细节）。

## 10. 映射与职责说明

| 方向 | 责任方 | 说明 |
|------|--------|------|
| JSON → RPC | Gateway | 校验类型与必填；枚举字符串 → proto enum；ISO 时间 → `google.protobuf.Timestamp` |
| RPC → JSON | Gateway | 过滤敏感字段；`Timestamp` → ISO 字符串；不泄露内部分枚举名 |
| 身份注入 | Gateway | 从 `IntrospectAccessToken` 结果注入 `user_id` / `session_id`，**禁止**信任客户端伪造的 `user_id` 请求字段（除显式公开路由） |
| 业务真相 | user-server | 网关不发明业务规则；冲突以域服务与契约为准 |

**禁止**：在网关引入与 `user_server_models.proto` / 公共契约 **含义冲突** 的新字段语义。

## 11. 幂等与去重

| 路由 | 建议 |
|------|------|
| `POST .../history/events` | 使用 `client_event_id` + 用户/会话维度去重（窗口由实现定义） |
| `POST .../feedback` | 可选 `client_request_id` 去重 |
| `POST .../favorites` | 与 `AddFavorite` 幂等语义一致：重复返回 `already_favorited: true` |

## 12. 与 BFF 页面接口的关系

多域只读拼屏的**可交付路由定义以本文件为准**；[pages.md](./pages.md) 保留聚合原则与编排补充说明。本文件中的细粒度用户路由与 `GetMeSummary` 均可被页面聚合复用。失败策略与 `warnings` 行为须符合 common-response 不变量。

## 13. 页面聚合路由（proto2）

以下页面路由由 `gateway` 作为 BFF 聚合实现，对外统一返回标准信封；下游仍调用各域内部 `proto` / RPC。

| 方法 | 路径 | 鉴权 | 说明 |
|------|------|------|------|
| GET | `/api/v2/pages/home_feed` | 无（可选 Bearer 或访客会话） | 未带主体时返回非个性化结果；带主体时可用于个性化与频控 |
| GET | `/api/v2/pages/guide_detail` | 无（可选 Bearer 或访客会话） | 主体仅用于个性化相关推荐、埋点与风控补充 |
| POST | `/api/v2/pages/redirect_prepare` | Bearer 或访客会话 | 必须具备主体，便于点击归因与幂等 |
| GET | `/api/v2/pages/me_summary` | Bearer 或访客会话 | 与 `/api/v2/me/summary` 一致 |
| POST | `/api/v2/backoffice/affiliate/partners` | Bearer（运营角色） | 联盟伙伴列表 |
| POST | `/api/v2/backoffice/affiliate/partners/add` | Bearer（运营角色） | 新增联盟伙伴（最小元信息） |
| POST | `/api/v2/backoffice/content/items` | Bearer（运营角色） | 内容管理最小列表 |
| POST | `/api/v2/backoffice/content/items/add` | Bearer（运营角色） | 新增导购内容并写入 platform/backoffice-backend CMS |
| POST | `/api/v2/backoffice/content/items/status` | Bearer（运营角色） | 内容发布态切换 |
| POST | `/api/v2/backoffice/governance/reviews` | Bearer（审核或运营） | 审核队列最小列表 |
| POST | `/api/v2/backoffice/governance/reviews/status` | Bearer（审核或运营） | 审核状态更新（通过/拒绝） |

### 13.1 聚合对象定义

#### `home_feed_item`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `recommendation_id` | string | 是 | 本次推荐结果 ID |
| `scene` | string | 是 | 推荐场景，通常为 `home_feed` |
| `rank` | integer | 是 | 1-based |
| `guide_card_id` | string | 是 | 导购卡片 ID |
| `guide_card` | object | 否 | 内联卡片，结构遵循 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) |
| `reason_tags` | array | 否 | 推荐解释短标签 |

#### `guide_detail_payload`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `guide_card` | object | 是 | 导购卡片完整展示结构 |
| `disclosures` | object | 否 | 商业披露摘要；默认从卡片和治理结果聚合 |
| `related` | array | 否 | 相关推荐列表，元素仅包含 `recommendation_id`、`scene`、`rank`、`guide_card_id`、`reason_tags` |

#### `redirect_prepare_payload`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `landing_url` | string | 是 | 最终受控跳转入口 |
| `click_id` | string | 否 | 若本次链路已分配点击 ID 则返回 |
| `expires_at` | string | 否 | ISO 8601 UTC |
| `attribution` | object | 否 | 回显的归因字段子集，遵循 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) |

### 13.2 `GET /api/v2/pages/home_feed`

**用途**：首页推荐流聚合接口。

**鉴权**：无（可选 Bearer 或访客会话）。

| Query 参数 | 类型 | 必填 | 说明 |
|------------|------|------|------|
| `cursor` | string | 否 | 分页游标 |
| `limit` | integer | 否 | 默认 20，最大 100 |
| `theme` | string | 否 | 可选主题过滤；值域见 `.cursor/rules/shared-contracts.mdc` |

| `data` | 类型 | 说明 |
|------|------|------|
| `items` | array of `home_feed_item` | 推荐条目列表 |
| `pagination` | object | 标准分页结构 |

**下游 RPC**

- `RecommendationService/QueryRecommendations`
- `ContentService/BatchGetGuideCards`
- 可选治理可见性快照过滤

**字段级映射（实现锚点）**

| JSON / 查询字段 | 下游来源 | 说明 |
|-----------------|----------|------|
| `theme` | `QueryRecommendationsRequest.filters.themes` | 使用主题 **slug**，非 `theme_id` |
| `cursor`, `limit` | `QueryRecommendationsRequest.cursor_limits` | 直接映射 |
| `items[].recommendation_id` | `QueryRecommendationsResponse.recommendation_id` | 同一页条目共享同一推荐结果 ID |
| `items[].scene` | `QueryRecommendationsResponse.scene` | |
| `items[].rank` | `QueryRecommendationsResponse.items[].rank` | |
| `items[].guide_card_id` | `QueryRecommendationsResponse.items[].guide_card_id` | |
| `items[].reason_tags` | `QueryRecommendationsResponse.items[].reason_tags` | |
| `items[].guide_card` | `ContentService/BatchGetGuideCards.cards[]` | 以 `guide_card_id` 批量 hydration；按 `rank` 重排 |
| `pagination` | `QueryRecommendationsResponse.cursor_pagination` | `next_cursor`、`has_more`、`limit` 一一对应 |

**治理过滤规则**

- 若启用治理过滤，网关以 `guide_card_id` / 逻辑内容 ID 读取可见性快照
- 非 `PUBLISHED` 内容从结果集中剔除
- 默认 **fail-closed**：治理依赖异常时，不返回应受限内容

### 13.3 `GET /api/v2/pages/guide_detail`

**用途**：导购详情页聚合接口。

**鉴权**：无（可选 Bearer 或访客会话）。

| Query 参数 | 类型 | 必填 | 说明 |
|------------|------|------|------|
| `guide_card_id` | string | 是 | 卡片 ID |
| `include_related` | boolean | 否 | 默认 `true` |

| `data` | 类型 | 说明 |
|------|------|------|
| `guide` | object | `guide_detail_payload.guide_card` |
| `disclosures` | object | 商业披露与治理提示 |
| `related` | array | 可选相关推荐 |

**下游 RPC**

- `ContentService/BatchGetGuideCards`
- `GovernanceCooperationService/BatchGetCooperationLabels`
- 可选 `RecommendationService/QueryRecommendations`

**字段级映射（实现锚点）**

| JSON / 查询字段 | 下游来源 | 说明 |
|-----------------|----------|------|
| `guide_card_id` | `BatchGetGuideCardsRequest.card_ids[]` | 单卡片查询 |
| `data.guide` | `BatchGetGuideCardsResponse.cards[0]` | 转为公开 `guide_card` 契约 |
| `data.disclosures` | `BatchGetCooperationLabelsResponse.labels_by_subject` | key 为 `COOPERATION_SUBJECT_TYPE_CARD:<guide_card_id>` |
| `data.related` | `QueryRecommendationsResponse.items[]` | 仅当 `include_related=true` 时调用 |

补充规则：

- `include_related=false` 时不得调用推荐 RPC
- `guide_card_id` 不存在时返回 `30001`
- `data.guide` 的字段语义以 `.cursor/rules/shared-contracts.mdc` 为准，网关负责从内部 `GuideCard` 裁剪
- `data.related` 仅返回 `recommendation_id`、`scene`、`rank`、`guide_card_id`、`reason_tags`；不内联 `guide_card`

### 13.4 `POST /api/v2/pages/redirect_prepare`

**用途**：用户点击“去购买”前，准备跳转链路。

**鉴权**：Bearer 或访客会话。

| 请求体字段 | 类型 | 必填 | 说明 |
|------------|------|------|------|
| `guide_card_id` | string | 是 | |
| `recommendation_id` | string | 否 | 若来自推荐流 |
| `scene` | string | 否 | 推荐场景 |
| `item_rank` | integer | 否 | 推荐位次 |
| `preferred_channel_code` | string | 否 | 客户端或用户偏好渠道 |

| `data` | 类型 | 说明 |
|------|------|------|
| `landing_url` | string | 受控跳转入口 |
| `click_id` | string | 可选 |
| `expires_at` | string | 可选 |
| `attribution` | object | 可选 |

**下游 RPC**

- `TrackingLinkService/AssembleTrackingLink`

**字段级映射（实现锚点）**

| JSON 字段 | 下游来源 | 说明 |
|-----------|----------|------|
| `guide_card_id` | `AssembleTrackingLinkRequest.content_ref.guide_card_id` | 必填 |
| `recommendation_id` | `AssembleTrackingLinkRequest.content_ref.recommendation_id` | 可选 |
| `scene` | `AssembleTrackingLinkRequest.content_ref.scene` | 可选 |
| `item_rank` | `AssembleTrackingLinkRequest.content_ref.item_rank` | 可选 |
| `preferred_channel_code` | 用于组装 `affiliate_context_ref` 或路由策略 | v1 若上游传入则必须透传到组链策略；未传时由服务端默认策略选择渠道 |
| `data.landing_url` | `AssembleTrackingLinkResponse.landing_url` | |
| `data.click_id` | `AssembleTrackingLinkResponse.attribution_echo.click_id` 或顶层 `click_id` | 以可用值为准 |
| `data.expires_at` | `AssembleTrackingLinkResponse.expires_at` | `Timestamp` → ISO 8601 UTC |
| `data.attribution` | `AssembleTrackingLinkResponse.attribution_echo` | 仅回显非敏感字段 |

### 13.5 `GET /api/v2/pages/me_summary`

**用途**：页面级“我的”摘要，与 `/api/v2/me/summary` 共享核心数据块。

**鉴权**：Bearer 或访客会话。

| `data` | 类型 | 说明 |
|------|------|------|
| `profile` | object | 见本文件 §5.4 |
| `counts` | object | 见本文件 §5.9 |
| `consent` | object | 见本文件 §5.8 |

**下游 RPC**

- `UserServerService/GetMeSummary`

### 13.6 `POST /api/v2/backoffice/*`（运营平台路由）

该组路由用于 `platform/backoffice-backend`（content / governance / affiliate 三模块）的运营后台能力，仍遵循标准信封与 `snake_case`。

| 路由 | `data` 结构 | 下游 |
|------|-------------|------|
| `POST /api/v2/backoffice/affiliate/partners` | `{ "items": [{ "partner_id", "display_name", "status", "primary_channel_code" }] }` | platform/backoffice-backend |
| `POST /api/v2/backoffice/affiliate/partners/add` | 同上（返回创建后列表） | platform/backoffice-backend |
| `POST /api/v2/backoffice/content/items` | `{ "items": [{ "content_id", "title", "theme", "status", "summary", "landing_url", "external_item_id" }] }` | platform/backoffice-backend |
| `POST /api/v2/backoffice/content/items/add` | 同上（返回创建后列表） | platform/backoffice-backend |
| `POST /api/v2/backoffice/content/items/status` | 同上（返回更新后列表） | platform/backoffice-backend |
| `POST /api/v2/backoffice/governance/reviews` | `{ "items": [{ "review_id", "subject_id", "status" }] }` | platform/backoffice-backend |
| `POST /api/v2/backoffice/governance/reviews/status` | 同上（返回更新后列表） | platform/backoffice-backend |

边界说明：

- 网关负责鉴权、字段校验与 JSON ↔ proto 映射。
- 审核与治理规则归 **governance** 模块；内容主数据归 **content** 模块；伙伴配置归 **affiliate** 模块（均在 `platform/backoffice-backend` 单进程）。
- v1 不提供跨域事务型写接口。

## 14. Gateway Pages Edge Proto

页面聚合相关的 `proto2` 见：

- `services/gateway/proto/gateway_pages_edge.proto`

该文件描述 `gateway` 自身的页面聚合入口消息，用于将公开 JSON 路由与内部 RPC 编排映射到一组可版本化的 edge message。

## 附录 A. 典型 HTTP 请求 / 响应示例

以下示例使用真实对外 JSON 形状，便于前端、测试与其他 Agent 直接联调。响应信封遵循 `.cursor/rules/shared-contracts.mdc`；时间字段使用 ISO 8601 UTC 字符串。

### A.1 `POST /api/v2/me/favorites`

#### HTTP Request

```http
POST /api/v2/me/favorites HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{
  "guide_card_id": "guide_card_1001"
}
```

#### HTTP Response

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {
    "favorite_id": "fav_01HSYQPDYX4B7C8WQ6ZK",
    "already_favorited": false
  },
  "meta": {
    "request_id": "req_01HSZ62M7C1E3N8S4D5K",
    "trace_id": "trace_01HSZ62M9A7Q4V0T2B6M",
    "server_time_ms": 1774699800000
  }
}
```

### A.2 `GET /api/v2/me/summary`

#### HTTP Request

```http
GET /api/v2/me/summary HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {
    "profile": {
      "user_id": "user_01HSYQK5PZ4K4J9R2M8D",
      "is_guest": false,
      "display_name": "Shinan",
      "avatar_url": "https://cdn.example.com/avatar/u_01.png",
      "locale": "zh-CN"
    },
    "counts": {
      "favorites_count": 12,
      "history_count": 37
    },
    "consent": {
      "personalization_allowed": true,
      "analytics_allowed": true,
      "marketing_allowed": false,
      "consent_version": "2026-03-privacy-v3",
      "updated_at": "2026-03-20T08:30:00Z",
      "jurisdiction": "CN"
    }
  },
  "meta": {
    "request_id": "req_01HSZ66T47Q9A4M1X8B2",
    "trace_id": "trace_01HSZ66W6P5G8S0Y9R1N",
    "server_time_ms": 1774699860000
  }
}
```

### A.3 `GET /api/v2/pages/home_feed`

#### HTTP Request

```http
GET /api/v2/pages/home_feed?cursor=&limit=2 HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {
    "items": [
      {
        "recommendation_id": "rec_01HSZ15H0N8HG9P5P2E0",
        "scene": "home_feed",
        "rank": 1,
        "guide_card_id": "guide_card_1001",
        "reason_tags": [
          "适合通勤",
          "近期热门"
        ]
      },
      {
        "recommendation_id": "rec_01HSZ15H0N8HG9P5P2E0",
        "scene": "home_feed",
        "rank": 2,
        "guide_card_id": "guide_card_1018",
        "reason_tags": [
          "主题相近"
        ]
      }
    ],
    "pagination": {
      "next_cursor": "cursor_home_feed_2",
      "has_more": true,
      "limit": 2
    }
  },
  "meta": {
    "request_id": "req_01HSZ6A29A7C3V4M8W0F",
    "trace_id": "trace_01HSZ6A4M1E5N9R6Q2B7",
    "server_time_ms": 1774699920000
  }
}
```

### A.4 `POST /api/v2/guest/session`

#### HTTP Request

```http
POST /api/v2/guest/session HTTP/1.1
Content-Type: application/json

{"device_id":"device_9f1b5e18","client_platform":"ios"}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"session_id":"sess_01HSYQBY1S7W4T1M7Q2B","created":true},"meta":{"request_id":"req_1","server_time_ms":1774699800000}}
```

### A.5 `POST /api/v2/auth/token/issue`

#### HTTP Request

```http
POST /api/v2/auth/token/issue HTTP/1.1
Content-Type: application/json

{"account_proof":{"user_id":"user_01HSYQK5PZ4K4J9R2M8D"}}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"access_token":"at_xxx","refresh_token":"rt_xxx","expires_in":3600,"session_id":"sess_01HSYQBY1S7W4T1M7Q2B","access_expires_at":"2026-03-28T13:00:00Z"},"meta":{"request_id":"req_2","server_time_ms":1774699800000}}
```

### A.6 `POST /api/v2/auth/token/refresh`

#### HTTP Request

```http
POST /api/v2/auth/token/refresh HTTP/1.1
Content-Type: application/json

{"refresh_token":"rt_xxx","request_context":{"client_platform":"ios","app_version":"1.4.2","device_id":"device_9f1b5e18"}}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"access_token":"at_yyy","refresh_token":"rt_yyy","expires_in":3600,"session_id":"sess_01HSYQBY1S7W4T1M7Q2B","access_expires_at":"2026-03-28T14:00:00Z"},"meta":{"request_id":"req_3","server_time_ms":1774699800000}}
```

### A.7 `DELETE /api/v2/auth/session`

#### HTTP Request

```http
DELETE /api/v2/auth/session HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{"revoke_scope":"single_session"}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"revoked":true},"meta":{"request_id":"req_4","server_time_ms":1774699800000}}
```

### A.8 `GET /api/v2/me/profile`

#### HTTP Request

```http
GET /api/v2/me/profile HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"profile":{"user_id":"user_01HSYQK5PZ4K4J9R2M8D","is_guest":false}},"meta":{"request_id":"req_5","server_time_ms":1774699800000}}
```

### A.9 `PATCH /api/v2/me/profile`

#### HTTP Request

```http
PATCH /api/v2/me/profile HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{"display_name":"A"}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"profile":{"user_id":"user_01HSYQK5PZ4K4J9R2M8D","is_guest":false,"display_name":"A"}},"meta":{"request_id":"req_6","server_time_ms":1774699800000}}
```

### A.10 `GET /api/v2/me/preferences`

#### HTTP Request

```http
GET /api/v2/me/preferences HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"preferences":{}},"meta":{"request_id":"req_7","server_time_ms":1774699800000}}
```

### A.11 `PUT /api/v2/me/preferences`

#### HTTP Request

```http
PUT /api/v2/me/preferences HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{"preferences":{"preferences_version":1}}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"preferences":{"preferences_version":1}},"meta":{"request_id":"req_8","server_time_ms":1774699800000}}
```

### A.12 `GET /api/v2/me/favorites`

#### HTTP Request

```http
GET /api/v2/me/favorites?limit=1 HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"items":[{"favorite_id":"fav_01HSYQPDYX4B7C8WQ6ZK","guide_card_id":"guide_card_1001","favorited_at":"2026-03-28T12:00:00Z"}],"pagination":{"next_cursor":null,"has_more":false,"limit":1}},"meta":{"request_id":"req_9","server_time_ms":1774699800000}}
```

### A.13 `DELETE /api/v2/me/favorites/{favorite_id}`

#### HTTP Request

```http
DELETE /api/v2/me/favorites/fav_01HSYQPDYX4B7C8WQ6ZK HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"removed":true},"meta":{"request_id":"req_10","server_time_ms":1774699800000}}
```

### A.14 `GET /api/v2/me/history`

#### HTTP Request

```http
GET /api/v2/me/history?limit=1 HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"items":[{"content_ref":{"type":"guide_card","guide_card_id":"guide_card_1001"},"last_seen_at":"2026-03-28T12:00:00Z"}],"pagination":{"next_cursor":null,"has_more":false,"limit":1}},"meta":{"request_id":"req_11","server_time_ms":1774699800000}}
```

### A.15 `POST /api/v2/me/history/events`

#### HTTP Request

```http
POST /api/v2/me/history/events HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{"content_ref":{"type":"guide_card","guide_card_id":"guide_card_1001"}}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"recorded":true,"deduplicated":false},"meta":{"request_id":"req_12","server_time_ms":1774699800000}}
```

### A.16 `DELETE /api/v2/me/history`

#### HTTP Request

```http
DELETE /api/v2/me/history HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{"scope":"all"}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"removed_count":0},"meta":{"request_id":"req_13","server_time_ms":1774699800000}}
```

### A.17 `POST /api/v2/me/feedback`

#### HTTP Request

```http
POST /api/v2/me/feedback HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{"target_type":"app","target_id":"app_v1"}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"feedback_id":"feedback_01HSZA7D2V5M8Q1N6R3K"},"meta":{"request_id":"req_14","server_time_ms":1774699800000}}
```

### A.18 `GET /api/v2/me/consent`

#### HTTP Request

```http
GET /api/v2/me/consent HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"consent":{"personalization_allowed":true,"consent_version":"2026-03-v1","updated_at":"2026-03-28T12:00:00Z"}},"meta":{"request_id":"req_15","server_time_ms":1774699800000}}
```

### A.19 `PUT /api/v2/me/consent`

#### HTTP Request

```http
PUT /api/v2/me/consent HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{"consent":{"personalization_allowed":true,"consent_version":"2026-03-v1","updated_at":"2026-03-28T12:00:00Z"}}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"consent":{"personalization_allowed":true,"consent_version":"2026-03-v1","updated_at":"2026-03-28T12:00:00Z"}},"meta":{"request_id":"req_16","server_time_ms":1774699800000}}
```

### A.20 `GET /api/v2/health`

#### HTTP Request

```http
GET /api/v2/health HTTP/1.1
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"status":"ok","components":[{"name":"gateway","ok":true,"detail":""}]},"meta":{"request_id":"req_17","server_time_ms":1774699800000}}
```

### A.21 `GET /api/v2/pages/guide_detail`

#### HTTP Request

```http
GET /api/v2/pages/guide_detail?guide_card_id=guide_card_1001 HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"guide":{"guide_card_id":"guide_card_1001","schema_version":1,"title":"T","theme":"clothing","published_at":"2026-03-28T08:00:00Z","updated_at":"2026-03-28T09:00:00Z","commercial_disclosure":{"is_commercial":false}}},"meta":{"request_id":"req_18","server_time_ms":1774699800000}}
```

### A.22 `POST /api/v2/pages/redirect_prepare`

#### HTTP Request

```http
POST /api/v2/pages/redirect_prepare HTTP/1.1
Authorization: Bearer <access_token>
Content-Type: application/json

{"guide_card_id":"guide_card_1001"}
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"landing_url":"https://track.example.com/r/abc"},"meta":{"request_id":"req_19","server_time_ms":1774699800000}}
```

### A.23 `GET /api/v2/pages/me_summary`

#### HTTP Request

```http
GET /api/v2/pages/me_summary HTTP/1.1
Authorization: Bearer <access_token>
```

#### HTTP Response

```json
{"success":true,"code":0,"message":"ok","data":{"profile":{"user_id":"user_01HSYQK5PZ4K4J9R2M8D","is_guest":false},"counts":{"favorites_count":0,"history_count":0},"consent":{"personalization_allowed":true,"consent_version":"2026-03-v1","updated_at":"2026-03-28T12:00:00Z"}},"meta":{"request_id":"req_20","server_time_ms":1774699800000}}
```

## 附录 B. 典型错误响应示例

错误响应同样遵循 `.cursor/rules/shared-contracts.mdc` 信封：`code !== 0` ⇔ `success === false`；`data` 多为 `null`；客户端以 `code` 分支。

### B.1 访问令牌过期（`20002`，HTTP 401）

```json
{"success":false,"code":20002,"message":"access token expired","data":null,"meta":{"request_id":"req_e1","server_time_ms":1774699800000}}
```

### B.2 参数校验失败（`10002`，HTTP 400，含字段级 `errors`）

```json
{
  "success": false,
  "code": 10002,
  "message": "validation failed",
  "data": null,
  "errors": [
    { "field": "guide_card_id", "code": 10001, "message": "required" }
  ],
  "meta": { "request_id": "req_e2", "server_time_ms": 1774699800000 }
}
```

### B.3 频率超限（`10005`，HTTP 429）

```json
{"success":false,"code":10005,"message":"rate limit exceeded","data":null,"meta":{"request_id":"req_e3","server_time_ms":1774699800000}}
```

### B.4 路由不存在（`10050`，HTTP 404，网关入口层）

```json
{"success":false,"code":10050,"message":"route not found","data":null,"meta":{"request_id":"req_e4","server_time_ms":1774699800000}}
```

### B.5 详情页内容已下架（透传 platform/backoffice-backend `30002`，HTTP 410/404）

```json
{"success":false,"code":30002,"message":"content unavailable","data":null,"meta":{"request_id":"req_e5","server_time_ms":1774699800000}}
```

### B.6 首页聚合部分降级（`success===true` + `warnings`）

关键下游（推荐+内容）成功、可降级依赖（相关推荐/治理增强）失败时，返回主数据并附 `warnings`，顶层仍 `code: 0`：

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {
    "items": [
      { "recommendation_id": "rec_01HSZ15H0N8HG9P5P2E0", "scene": "home_feed", "rank": 1, "guide_card_id": "guide_card_1001" }
    ],
    "pagination": { "next_cursor": null, "has_more": false, "limit": 20 }
  },
  "warnings": [
    { "code": 10053, "message": "governance enrichment degraded" }
  ],
  "meta": { "request_id": "req_e6", "server_time_ms": 1774699920000 }
}
```

### B.7 跳转准备关键依赖失败（`10054`，HTTP 502/504）

跳转准备无可降级路径，关键下游（tracking 组链）失败时整请求失败：

```json
{"success":false,"code":10054,"message":"tracking link assembly unavailable","data":null,"meta":{"request_id":"req_e7","server_time_ms":1774699800000}}
```
