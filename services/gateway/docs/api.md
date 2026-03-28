# Gateway 对外 HTTP API（v2 交付面）

## 1. 文档范围与版本

- **范围**：终端 → `gateway` 的 **HTTPS + JSON** 约定；本文件包含 **可直接实现** 的细粒度用户路由与页面聚合路由、请求/响应 `data` 形状、嵌套对象、枚举、错误码与到各域 RPC 的映射说明。
- **路径版本**：`/api/v2/...` 与内部 `simple_living.user_domain` / `simple_living.gateway.user` proto 对齐；破坏性变更通过新路径或迁移期在 changelog 说明。
- **字段命名**：JSON 一律 **`snake_case`**，与全局契约一致。
- **时间**：业务时间字段在 JSON 中为 **ISO 8601 UTC 字符串**（例 `2026-03-28T12:00:00Z`）；机器时间戳仍可由网关写入信封 `meta.server_time_ms`（Unix 毫秒）。
- **Proto 骨架**：`services/gateway/proto/gateway_user_http_messages.proto`、`gateway_user_edge.proto`、`gateway_pages_edge.proto`；下游类型见各域 `services/<service>/proto/`。

## 2. 传输与通用头

| 项 | 约定 |
|----|------|
| 协议 | HTTPS |
| 请求体 | `Content-Type: application/json`（本文件所列 POST/PATCH/PUT/DELETE 带体路由） |
| 编码 | UTF-8 |

| 请求头 | 必填 | 说明 |
|--------|------|------|
| `Authorization` | 按路由 | 已登录路由使用 `Bearer <access_token>` |
| `X-Request-Id` | 否 | 可参与生成；响应 `meta.request_id` **以网关最终值为准** |
| `X-Client-Platform` | 推荐 | 见 §6 `client_platform` |
| `X-Client-Version` | 推荐 | 应用版本号，如 `1.4.2` |
| `X-Device-Id` | 推荐 | 稳定设备标识；访客会话与风控辅助 |

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

### 5.10 `request_context`（可选 JSON 体字段，对齐头信息）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_platform` | string | 否 | 可与 `X-Client-Platform` 合并策略：体优先或头优先须在实现中固定 |
| `app_version` | string | 否 | 对齐 `X-Client-Version` |
| `device_id` | string | 否 | 对齐 `X-Device-Id` |

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
| `content_ref_type` | `guide_card` | 扩展需协同 content-domain |
| `favorite_content_type` | `guide_card` | 列表筛选；默认 `guide_card` |
| `feedback_target_type` | `guide_card`, `recommendation_result`, `app`, `other` | |
| `history_source_surface` | `feed`, `search`, `detail` | |
| `clear_history_scope` | `all`, `before_time` | `before_time` 须配合 `before_time` 时间字段 |
| `revoke_scope`（DELETE `/auth/session` 体字段） | `single_session`, `all_user_sessions` | 映射 `RevokeSessionScope` |

**映射到 proto**：网关将字符串枚举转为 `user_domain_models.proto` 中对应 `enum` 数值（实现侧维护表；新增值仅追加）。

## 7. 路由总表（用户相关 v2）

| 方法 | 路径 | 鉴权 | 默认 limit | 主下游 RPC（`UserDomainService`） |
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

**内部（非表列 HTTP）**：`IntrospectAccessToken`、`GetSignalBundleRef`、`ResolveSignalBundle` 由网关在入口或 BFF 流程中直接调用 `UserDomainService`，**不**对终端暴露独立路径。

## 8. 错误码与 HTTP 状态

客户端以 JSON **`code`** 为准。常用映射：

| `code` | 语义 | 典型 HTTP |
|--------|------|-----------|
| `0` | 成功 | 200 |
| `10001` | 缺少必填参数 | 400 |
| `10002` | 参数校验失败 | 400 |
| `10005` | 请求频率超限 | 429 |
| `20001` | 未登录或会话缺失 | 401 |
| `20002` | 访问令牌无效或过期 | 401 |
| `20003` | 刷新令牌无效或过期 | 401 |
| `20004` | 权限不足 | 403 |
| `20005` | 设备或环境被拒绝 | 403 |
| `30001` | 资源不存在 | 404 |
| `90001` | 内部错误 | 500 |
| `90002` | 依赖超时 | 504 |
| `90003` | 依赖错误 | 502 |

**gRPC → JSON**：将 `NOT_FOUND`/`INVALID_ARGUMENT`/… 映射到上表；业务码放入信封 `code`（非仅 `message`）。

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
| `request_context` | object | 否 | §5.10 |

| `data` | 类型 | 说明 |
|------|------|------|
| `access_token` | string | |
| `refresh_token` | string | |
| `expires_in` | integer | **秒**；源自 proto `expires_in_seconds` |
| `session_id` | string | |
| `access_expires_at` | string | ISO 8601 UTC；源自 proto `access_expires_at` |

**映射**：`IssueTokenPair`；须在网关完成登录证明校验（OTP/OAuth 等）后再调 RPC。

### 9.3 `POST /api/v2/auth/token/refresh`

| 请求体字段 | 类型 | 必填 |
|------------|------|------|
| `refresh_token` | string | 是 |
| `request_context` | object | 否 |

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
| 业务真相 | user-domain | 网关不发明业务规则；冲突以域服务与契约为准 |

**禁止**：在网关引入与 `user_domain_models.proto` / 公共契约 **含义冲突** 的新字段语义。

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

### 13.1 聚合对象定义

#### `home_feed_item`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `recommendation_id` | string | 是 | 本次推荐结果 ID |
| `scene` | string | 是 | 推荐场景，通常为 `home_feed` |
| `rank` | integer | 是 | 1-based |
| `guide_card_id` | string | 是 | 导购卡片 ID |
| `guide_card` | object | 否 | 内联卡片，结构遵循 [guide-card.md](../../../docs/contracts/guide-card.md) |
| `reason_tags` | array | 否 | 推荐解释短标签 |

#### `guide_detail_payload`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `guide_card` | object | 是 | 导购卡片完整展示结构 |
| `disclosures` | object | 否 | 商业披露摘要；默认从卡片和治理结果聚合 |
| `related` | array | 否 | 相关推荐列表，元素使用 `home_feed_item` 的子集 |

#### `redirect_prepare_payload`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `landing_url` | string | 是 | 最终受控跳转入口 |
| `click_id` | string | 否 | 若本次链路已分配点击 ID 则返回 |
| `expires_at` | string | 否 | ISO 8601 UTC |
| `attribution` | object | 否 | 回显的归因字段子集，遵循 [redirect-attribution.md](../../../docs/contracts/redirect-attribution.md) |

### 13.2 `GET /api/v2/pages/home_feed`

**用途**：首页推荐流聚合接口。

| Query 参数 | 类型 | 必填 | 说明 |
|------------|------|------|------|
| `cursor` | string | 否 | 分页游标 |
| `limit` | integer | 否 | 默认 20，最大 100 |
| `theme` | string | 否 | 可选主题过滤 |

| `data` | 类型 | 说明 |
|------|------|------|
| `items` | array of `home_feed_item` | 推荐条目列表 |
| `pagination` | object | 标准分页结构 |

**下游 RPC**

- `RecommendationService/QueryRecommendations`
- `ContentService/BatchGetGuideCards`
- 可选治理可见性快照过滤

### 13.3 `GET /api/v2/pages/guide_detail`

**用途**：导购详情页聚合接口。

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

### 13.4 `POST /api/v2/pages/redirect_prepare`

**用途**：用户点击“去购买”前，准备跳转链路。

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

### 13.5 `GET /api/v2/pages/me_summary`

**用途**：页面级“我的”摘要，与 `/api/v2/me/summary` 共享核心数据块。

| `data` | 类型 | 说明 |
|------|------|------|
| `profile` | object | 见本文件 §5.4 |
| `counts` | object | 见本文件 §5.9 |
| `consent` | object | 见本文件 §5.8 |

**下游 RPC**

- `UserDomainService/GetMeSummary`

## 14. Gateway Pages Edge Proto

页面聚合相关的 `proto2` 见：

- `services/gateway/proto/gateway_pages_edge.proto`

该文件描述 `gateway` 自身的页面聚合入口消息，用于将公开 JSON 路由与内部 RPC 编排映射到一组可版本化的 edge message。
