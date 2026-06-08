# user-server — 内部 RPC API（proto2）

## 1. 文档范围

- **受众**：API 网关、受信任的下游服务通过 **gRPC（proto2）** 调用本文档定义的 RPC。
- **非目标**：对外 HTTP/JSON 路径与字段由 API 网关维护；本文件 **不描述** 终端可见 URL。
- **Proto 文件**：`services/user-server/proto/user_server_models.proto`（枚举与消息）、`user_server_service.proto`（`UserServerService`）。
- **包名**：`simple_living.user_server`。
- **时间**：逻辑上使用 UTC；wire 上使用 `google.protobuf.Timestamp`。
- **调用信任前提**：内部 RPC 必须运行在受信任服务身份之上（如 mTLS、服务网格身份或等价机制）。文中出现的调用方标识（如 `caller_service`）同时用于审计与授权校验，必须与实际认证到的服务身份一致，不能仅信任业务参数本身。
- **非契约实现细节**：token 的内部编码形态（如 opaque / JWT）、密钥管理、精确 TTL 数值与轮换窗口属于 `user-server` provider-owned 实现参数；调用方只能依赖本文档中显式返回的 `expires_in_seconds`、`access_expires_at` 与 `rotation_required` 语义，不得依赖 token 内部结构。
- **多应用支持**：本服务支持多个应用接入。`app_id` 通过 `RpcRequestContext` 传递，用于数据隔离与配置路由。详见 §8.4。

## 2. 服务定义

```
service UserServerService {
  rpc IntrospectAccessToken(IntrospectAccessTokenRequest) returns (IntrospectAccessTokenResponse);
  rpc IntrospectRefreshToken(IntrospectRefreshTokenRequest) returns (IntrospectRefreshTokenResponse);
  rpc IssueTokenPair(IssueTokenPairRequest) returns (IssueTokenPairResponse);
  rpc RevokeSession(RevokeSessionRequest) returns (RevokeSessionResponse);
  rpc EnsureGuestSession(EnsureGuestSessionRequest) returns (EnsureGuestSessionResponse);

  rpc GetProfile(GetProfileRequest) returns (GetProfileResponse);
  rpc UpdateProfile(UpdateProfileRequest) returns (UpdateProfileResponse);
  rpc GetPreferences(GetPreferencesRequest) returns (GetPreferencesResponse);
  rpc UpdatePreferences(UpdatePreferencesRequest) returns (UpdatePreferencesResponse);

  rpc ListFavorites(ListFavoritesRequest) returns (ListFavoritesResponse);
  rpc AddFavorite(AddFavoriteRequest) returns (AddFavoriteResponse);
  rpc RemoveFavorite(RemoveFavoriteRequest) returns (RemoveFavoriteResponse);

  rpc ListHistory(ListHistoryRequest) returns (ListHistoryResponse);
  rpc RecordHistoryEvent(RecordHistoryEventRequest) returns (RecordHistoryEventResponse);
  rpc ClearHistory(ClearHistoryRequest) returns (ClearHistoryResponse);

  rpc SubmitFeedback(SubmitFeedbackRequest) returns (SubmitFeedbackResponse);

  rpc GetConsent(GetConsentRequest) returns (GetConsentResponse);
  rpc UpdateConsent(UpdateConsentRequest) returns (UpdateConsentResponse);

  rpc GetSignalBundleRef(GetSignalBundleRefRequest) returns (GetSignalBundleRefResponse);
  rpc ResolveSignalBundle(ResolveSignalBundleRequest) returns (ResolveSignalBundleResponse);

  rpc GetMeSummary(GetMeSummaryRequest) returns (GetMeSummaryResponse);

  rpc HealthCheck(HealthCheckRequest) returns (HealthCheckResponse);
}
```

## 3. 枚举（wire 数值以 proto 为准）

| Proto 枚举 | 值（示意） | 语义 |
|------------|------------|------|
| `AccountStatus` | `ACCOUNT_STATUS_ACTIVE`, `SUSPENDED`, `DELETED_PENDING`, … | 账号生命周期（扩展时仅追加） |
| `ClientPlatform` | `WEB`, `IOS`, `ANDROID`, `WECHAT_MINIPROGRAM`, `DOUYIN_MINIPROGRAM` | 客户端平台；各应用按需使用，未使用的值不影响协议兼容性 |
| `AuthTier` | `STANDARD`, `RISK_CHALLENGE` | 风控分层 |
| `FeedbackTargetType` | `GUIDE_CARD`, `RECOMMENDATION_RESULT`, `APP`, `OTHER` | 反馈对象类型；各应用可追加自有值 |
| `HistorySourceSurface` | `FEED`, `SEARCH`, `DETAIL` | 浏览来源 |
| `FavoriteContentType` | `GUIDE_CARD` | 收藏内容类型；各应用可追加自有值 |
| `ContentRefType` | `GUIDE_CARD` | 内容引用类型；各应用可追加自有值 |
| `RevokeSessionScope` | `SINGLE_SESSION`, `ALL_USER_SESSIONS` | 登出范围 |
| `ClearHistoryScope` | `ALL`, `BEFORE_TIME` | 清空历史策略 |

`UNSPECIFIED = 0` 在各枚举中表示未设置；实现应拒绝业务语义依赖 `UNSPECIFIED` 的写入请求（除非 RPC 明确允许缺省）。

`AccountStatus` 作为 provider-owned 生命周期枚举预留，当前 proto / api.md 暂无对外暴露字段直接引用它；调用方不应假定当前 RPC 会返回该状态，后续若接入将通过新增字段显式发布。

**扩展规则**：枚举值仅追加，不修改、不删除已有值的语义。新应用接入时如需新的内容类型或反馈类型，在对应枚举中追加即可。

## 4. 共享与嵌套消息

### 4.1 `RpcRequestContext`

| 字段 | 类型 | 说明 |
|------|------|------|
| `app_id` | string | 应用标识，用于多应用数据隔离与配置路由；由网关注入，调用方不得自行填写 |
| `client_platform` | `ClientPlatform` | |
| `app_version` | string | |
| `device_id` | string | |

### 4.2 `ContentRef`

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `ContentRefType` | 内容类型鉴别 |
| `content_id` | string | 通用内容标识，具体含义由 `type` 解释 |

`ContentRef` 是本服务对内容引用的通用抽象。`type` 标识内容类型（如 `GUIDE_CARD`），`content_id` 为该类型下的唯一标识。新增内容类型只需在 `ContentRefType` 枚举中追加值，无需修改消息结构。

### 4.3 `NotificationPrefs`

| 字段 | 类型 | 说明 |
|------|------|------|
| `email_enabled` | bool | |
| `push_enabled` | bool | |
| `sms_enabled` | bool | |
| `in_app_enabled` | bool | |

### 4.4 `UserProfile`

| 字段 | 类型 | 说明 |
|------|------|------|
| `user_id` | string | |
| `is_guest` | bool | |
| `display_name` | string | |
| `avatar_url` | string | |
| `locale` | string | |
| `bio` | string | |
| `notification_prefs` | `NotificationPrefs` | |

### 4.5 `ThemePreferences`

| 字段 | 类型 | 说明 |
|------|------|------|
| `theme_interests` | repeated string | 兴趣标签键，各应用定义自己的标签体系 |
| `content_filters` | map<string, string> | 内容过滤规则 |
| `default_sort` | string | UX 默认排序；下游服务可忽略 |

### 4.6 `UserPreferences`

| 字段 | 类型 | 说明 |
|------|------|------|
| `preferences_version` | int64 | 单调递增版本号，用于乐观并发控制 |
| `structured` | `ThemePreferences` | |

### 4.7 `PaginationCursor` / `PaginationCursorResult`

| 消息 | 字段 |
|------|------|
| `PaginationCursor` | `cursor`, `limit` |
| `PaginationCursorResult` | `next_cursor`, `has_more`, `limit` |

`PaginationCursor.cursor` 为空字符串表示首页。默认 `limit = 20`，最大 `limit = 100`；超限应返回 `INVALID_ARGUMENT` / `10002`。
若响应 `has_more = false`，则 `next_cursor` 应为空或缺省；若 `has_more = true`，则 `next_cursor` 必须可用于下一页请求。

### 4.8 `ConsentState`

| 字段 | 类型 | 说明 |
|------|------|------|
| `personalization_allowed` | bool | |
| `analytics_allowed` | bool | |
| `marketing_allowed` | bool | |
| `consent_version` | string | 合规文档版本 |
| `updated_at` | `Timestamp` | 服务端生成，请求侧必须省略 |
| `jurisdiction` | string | 管辖区；由网关/合规链路推导，普通调用方不得改写 |

### 4.9 `SignalBundleRef`

| 字段 | 类型 | 说明 |
|------|------|------|
| `signal_bundle_ref` | string | 不透明引用，不可猜测 |
| `expires_at` | `Timestamp` | |
| `bundle_version` | string | 用于失效通知 |
| `scopes` | repeated string | 允许消费此引用的场景或服务 |

### 4.10 `HealthComponentStatus`

| 字段 | 类型 |
|------|------|
| `name` | string |
| `ok` | bool |
| `detail` | string |

### 4.11 `MeCounts`

| 字段 | 类型 |
|------|------|
| `favorites_count` | int64 |
| `history_count` | int64 |

### 4.12 `ResolveSignalBundlePayload`

| 字段 | 类型 | 说明 |
|------|------|------|
| `opaque_payload_json` | string | 调用方不可自行扩展 schema 的不透明 JSON |
| `integrity_hash` | string | `sha256:<lower_hex>`，覆盖 `opaque_payload_json` 原始 UTF-8 字节 |

## 5. RPC 请求 / 响应

### 5.1 `IntrospectAccessToken`

| `IntrospectAccessTokenRequest` | 类型 |
|--------------------------------|------|
| `access_token` | string |
| `request_context` | `RpcRequestContext` |

| `IntrospectAccessTokenResponse` | 类型 |
|---------------------------------|------|
| `valid` | bool |
| `user_id` | string |
| `session_id` | string |
| `token_expires_at` | `Timestamp` |
| `scopes` | repeated string |
| `auth_tier` | `AuthTier` |

令牌无效/过期时，返回 **gRPC `OK` + `valid = false`**，其余业务字段应为空；只有请求格式错误、调用方未认证或依赖故障时才返回非 `OK` 传输层状态。

### 5.2 `IntrospectRefreshToken`

| `IntrospectRefreshTokenRequest` | 类型 |
|---------------------------------|------|
| `refresh_token` | string |
| `request_context` | `RpcRequestContext` |

| `IntrospectRefreshTokenResponse` | 类型 |
|----------------------------------|------|
| `valid` | bool |
| `user_id` | string |
| `session_id` | string |
| `rotation_required` | bool |

刷新令牌无效/过期时，同样返回 **gRPC `OK` + `valid = false`**，其余字段应为空。

`rotation_required = true` 表示该 refresh token 已进入服务端定义的轮换窗口或命中安全策略，调用方应在本次成功换发后废弃旧 refresh token，只保留最新返回的一组 token。

### 5.3 `IssueTokenPair`

| `IssueTokenPairRequest` | 类型 |
|-------------------------|------|
| **oneof `identity`** | |
| `user_id` | string |
| `phone_otp` | `AccountProofPhoneOtp` |
| `oauth` | `AccountProofOAuth` |
| **oneof 外字段** | |
| `device_fingerprint` | string |
| `client_platform` | `ClientPlatform` |
| `app_version` | string |
| `device_id` | string |
| `request_context` | `RpcRequestContext` |

`AccountProofPhoneOtp`: `phone_e164`, `otp_code`, `verification_id`。
`AccountProofOAuth`: `provider`, `provider_subject`, `authorization_code`。

`provider` 不是 proto 枚举，作为**服务自管字符串码表**处理；调用方必须使用 `user-server` 已注册的登录提供方编码（推荐小写 ASCII，如 `wechat`、`apple`、`douyin`、`google`），未知值返回 `INVALID_ARGUMENT` / `10002`。

`identity` 必须且仅能选择一个分支。`identity.user_id` 仅对受信内部服务身份开放（如网关的既有会话续签、账号迁移或显式授权后台）；普通业务调用方不得用任意 `user_id` 直接签发令牌。
`client_platform` 与 `device_id` 在 视为语义必填；缺失应返回 `INVALID_ARGUMENT`。
若顶层 `client_platform` / `app_version` / `device_id` 与 `request_context` 中同名字段同时出现，则必须保持一致；不一致返回 `INVALID_ARGUMENT` / `10002`。调用方应以顶层字段为主，`request_context` 仅作透传与风险补充。

登录 proof 字段缺失或格式非法返回 `INVALID_ARGUMENT`（`10001` / `10002`）；proof 语义无效、已过期或与账号不匹配时，推荐返回 `UNAUTHENTICATED` 并在网关侧映射为 `20006`；设备、环境或风控拒绝时映射为 `20005`。

| `IssueTokenPairResponse` | 类型 |
|--------------------------|------|
| `access_token` | string |
| `refresh_token` | string |
| `expires_in_seconds` | int32 |
| `session_id` | string |
| `access_expires_at` | `Timestamp` |

### 5.4 `RevokeSession`

| `RevokeSessionRequest` | 类型 |
|------------------------|------|
| `session_id` | string |
| `user_id` | string |
| `scope` | `RevokeSessionScope` |

当 `scope = REVOKE_SESSION_SCOPE_SINGLE_SESSION` 时，`session_id` 必填。
当 `scope = REVOKE_SESSION_SCOPE_ALL_USER_SESSIONS` 时，`user_id` 必填，`session_id` 应省略；`revoked = true` 表示至少一次有效会话撤销已执行，若目标用户当前无有效会话则返回 `revoked = false`。

字段组合不合法返回 `INVALID_ARGUMENT`；目标会话不存在或已失效时返回 `OK + revoked = false`；若目标会话不属于请求声明的 `user_id` 或调用方无权撤销该主体会话，则返回 `PERMISSION_DENIED`，并建议在网关侧映射为 `20004`。

| `RevokeSessionResponse` | 类型 |
|-------------------------|------|
| `revoked` | bool |

### 5.5 `EnsureGuestSession`

| `EnsureGuestSessionRequest` | 类型 |
|-----------------------------|------|
| `device_id` | string |
| `client_platform` | `ClientPlatform` |
| `app_version` | string |

`device_id` 与 `client_platform` 为语义必填；空字符串或 `CLIENT_PLATFORM_UNSPECIFIED` 视为非法。`app_version` 可缺省。

`EnsureGuestSession` 返回的 `session_id` 就是访客态主体载体；后续 `GetProfile`、`ListHistory`、`GetMeSummary`、`GetSignalBundleRef` 等支持 `session_id` 分支的 RPC 可直接使用它。对外 Bearer token 不是访客链路必需条件，网关应优先使用 gateway api.md §2.1 中的访客会话头语义。

| `EnsureGuestSessionResponse` | 类型 |
|------------------------------|------|
| `session_id` | string |
| `created` | bool |

### 5.6 `GetProfile` / `UpdateProfile`

`GetProfileRequest` **oneof `subject`**：`user_id` | `session_id`。
`GetProfileResponse`：`profile` (`UserProfile`)。

`UpdateProfileRequest`：`user_id`，可选 `display_name`, `avatar_url`, `locale`, `bio`, `notification_prefs`。未出现的顶层字段表示**保持原值**；若出现 `notification_prefs`，则按**整对象替换**该子结构，不做字段级 merge。若 `user_id` 对应主体不存在，返回 `NOT_FOUND`，并建议在网关侧映射为 `30001`。
`UpdateProfileResponse`：`profile`。

### 5.7 `GetPreferences` / `UpdatePreferences`

`GetPreferencesRequest`：`user_id`。
`GetPreferencesResponse`：`preferences` (`UserPreferences`)。若用户存在但尚无持久化偏好记录，返回默认空文档：`preferences_version = 0`，`structured` 省略或为空。

`UpdatePreferencesRequest`：`user_id`, `preferences`。 `preferences` 按**整文档替换**处理；调用方应提交完整目标状态，而不是只传局部 patch。`preferences.preferences_version` 由服务端单调递增生成；请求侧可省略，若显式携带且小于当前版本，服务端应返回 `ABORTED`，并建议在网关映射为共享冲突码 `10007`。若用户存在但此前无偏好记录，则首次写入按 create-or-replace 处理。
`UpdatePreferencesResponse`：`preferences`。

### 5.8 `ListFavorites` / `AddFavorite` / `RemoveFavorite`

`ListFavoritesRequest`：`user_id`, `page` (`PaginationCursor`), `content_type` (`FavoriteContentType`)。`page` 缺省时等价于 `{ cursor: "", limit: 20 }`；`content_type` 缺省时返回所有类型。
`ListFavoritesResponse`：`items` (`repeated FavoriteItem`), `pagination` (`PaginationCursorResult`)。

`FavoriteItem`：`favorite_id`, `content_id`, `content_type`, `favorited_at`。

`AddFavoriteRequest`：`user_id`, `content_id`, `content_type`。`content_type` 缺省时由服务端根据内容服务注册信息推断或使用默认值。
`AddFavoriteResponse`：`favorite_id`, `already_favorited`。

`RemoveFavoriteRequest`：`user_id`, `favorite_id`, `content_id`（`content_id` 可选辅助）。`favorite_id` 与 `content_id` 至少提供一个；若两者同时提供，必须指向同一收藏记录，否则返回 `INVALID_ARGUMENT`。目标收藏不存在时返回 `OK + removed = false`；若 `user_id` 主体不存在，则返回 `NOT_FOUND`。
`RemoveFavoriteResponse`：`removed`。

### 5.9 `ListHistory` / `RecordHistoryEvent` / `ClearHistory`

`ListHistoryRequest`：**oneof `owner`** `user_id` | `session_id`；`page`。`page` 缺省时等价于 `{ cursor: "", limit: 20 }`。
`ListHistoryResponse`：`items` (`HistoryItem`), `pagination` (`PaginationCursorResult`)。默认按 `last_seen_at DESC` 排序；若时间相同，再按 `content_ref.content_id ASC` 做稳定排序。

`HistoryItem`：`content_ref`, `last_seen_at`, `first_seen_at`, `impression_count`, `source_surface`。

`RecordHistoryEventRequest`：**oneof `owner`**；`content_ref`, `occurred_at`, `source_surface`, `client_event_id`。`content_ref` 与 `occurred_at` 为语义必填；对同一 `owner + content_ref` 的非去重事件，服务端应执行聚合 upsert：`first_seen_at = min(existing, occurred_at)`、`last_seen_at = max(existing, occurred_at)`、`impression_count += 1`。
`RecordHistoryEventResponse`：`recorded`, `deduplicated`。若命中 `(owner, client_event_id)` 去重窗口，则返回 `recorded = false`、`deduplicated = true`；正常写入返回 `recorded = true`、`deduplicated = false`。

`ClearHistoryRequest`：**oneof `owner`**；`scope`；`before_time`（当 `scope = CLEAR_HISTORY_SCOPE_BEFORE_TIME`）。当 `scope = CLEAR_HISTORY_SCOPE_BEFORE_TIME` 时，删除条件按 `last_seen_at <= before_time` 计算；时间比较以存储后的 UTC 时间戳精度为准。
`ClearHistoryResponse`：`removed_count`。

### 5.10 `SubmitFeedback`

`SubmitFeedbackRequest`：**oneof `actor`** `user_id` | `session_id`；`target_type`, `target_id`, `rating` (`google.protobuf.DoubleValue`), `reason_codes`, `free_text`, `client_request_id`。`actor`、`target_type`、`target_id`、`client_request_id` 为语义必填，且 `rating` / `reason_codes` / `free_text` 至少提供一种。若提供 `rating`，量程约定为 `1.0` 到 `5.0`（允许半星/小数）；超范围返回 `INVALID_ARGUMENT`。
`SubmitFeedbackResponse`：`feedback_id`。

### 5.11 `GetConsent` / `UpdateConsent`

`GetConsentRequest`：`user_id`。
`GetConsentResponse`：`consent` (`ConsentState`)。若用户存在但尚无显式同意记录，则返回 `OK` 且 `consent` 省略；调用方应将其解释为"尚未形成显式持久化同意态"，并按产品/合规默认策略处理。

`UpdateConsentRequest`：`user_id`, `consent`。`consent.updated_at` 为**服务端生成的只读字段**，请求侧必须省略。`consent.jurisdiction` 由网关/合规链路按受信上下文推导；普通内部调用方不得任意改写。若此前无显式同意记录，则按 upsert 创建第一条记录；若 `user_id` 主体不存在，则返回 `NOT_FOUND`。
`UpdateConsentResponse`：`consent`。

### 5.12 `GetSignalBundleRef`

`GetSignalBundleRefRequest`：**oneof `subject`** `user_id` | `session_id`；`scene`。`scene` 为语义必填，使用稳定的 `snake_case` 场景字符串。允许的 `scene` 值由接入应用在服务配置中注册；未注册的值返回 `INVALID_ARGUMENT`，并建议在网关侧映射为 `40001`。
`GetSignalBundleRefResponse`：`ref` (`SignalBundleRef`)。

### 5.13 `ResolveSignalBundle`

`ResolveSignalBundleRequest`：`signal_bundle_ref`, `caller_service`。`caller_service` 既用于审计也参与授权；服务端必须校验它与已认证的内部服务身份一致，未注册调用方返回 `PERMISSION_DENIED`。

`ResolveSignalBundleResponse`：`resolved`, `expires_at`, `payload` (`ResolveSignalBundlePayload`)。当 `resolved = false` 时，`payload` 应为空。：`signal_bundle_ref` 语法非法返回 `INVALID_ARGUMENT`；语法合法但 ref 未知、已过期、不可解析或因个性化限制被弱化为不可用时，返回 `OK + resolved = false`，而不是传输层错误。

`ResolveSignalBundlePayload.opaque_payload_json` 为**调用方不可自行扩展 schema 的不透明 JSON 字符串**；调用方只能按其自身与 `user-server` 的约定解释。
`ResolveSignalBundlePayload.integrity_hash` 采用 `sha256:<lower_hex>` 形式，覆盖 `opaque_payload_json` 的原始 UTF-8 字节，用于审计与传输完整性校验。

面向下游服务的 最小可依赖 JSON 键集合为：`scene`（string）、`subject_kind`（`user` / `guest`）、`theme_interests`（string array）、`content_filters`（object<string,string>）、`recent_content_ids`（string array，可为空）。新增键仅追加，既有键语义不得重写。

### 5.14 `GetMeSummary`

`GetMeSummaryRequest`：**oneof `subject`** `user_id` | `session_id`。
`GetMeSummaryResponse`：`profile`, `counts` (`MeCounts`), `consent`。当 `subject = session_id` 且主体为访客时，`consent` 在 可省略；省略表示当前无可持久复用的登录同意态，调用方不得将其解释为已明确授权个性化。

### 5.15 `HealthCheck`

`HealthCheckRequest`：空。
`HealthCheckResponse`：`status`, `components` (`repeated HealthComponentStatus`)。`status` 取值约定为 `SERVING`、`DEGRADED`、`NOT_SERVING`：

- `SERVING`：所有关键组件 `ok = true`
- `DEGRADED`：核心链路可用，但至少一个非阻断组件异常，或存在已知降级
- `NOT_SERVING`：鉴权 / 会话 / 主存储等关键组件不可用，调用方应视为不可服务

## 6. 错误与状态约定

### 6.1 gRPC 状态码（传输层）

| gRPC `Code` | 典型场景 |
|-------------|----------|
| `OK` | 业务成功 |
| `INVALID_ARGUMENT` | 字段缺失、枚举非法、`UNSPECIFIED` 用于必填语义 |
| `UNAUTHENTICATED` | 调用方未认证、会话不存在、或除 `Introspect*` 之外的鉴权凭据无效 |
| `PERMISSION_DENIED` | 已识别主体但无权 |
| `NOT_FOUND` | 用户主体、子资源或引用资源不存在 |
| `ALREADY_EXISTS` | 若某写路径需显式冲突（少用） |
| `ABORTED` | 乐观锁/版本冲突等并发写冲突 |
| `RESOURCE_EXHAUSTED` | 配额/限流 |
| `FAILED_PRECONDITION` | 状态机不允许 |
| `UNAVAILABLE` | 依赖服务超时、暂时不可达 |
| `INTERNAL` | 未分类失败 |

`IntrospectAccessToken` / `IntrospectRefreshToken` 是本节的显式例外：令牌无效/过期使用各自响应中的 `valid = false` 表达，而不是返回 `UNAUTHENTICATED`。

### 6.2 业务码（统一经 `google.rpc.ErrorInfo` 传递）

| 场景 | 建议业务码 | gRPC 辅助 |
|------|------------|-----------|
| 缺少语义必填字段 | `10001` | `INVALID_ARGUMENT` |
| 未登录 / 会话缺失（含 `session_id` 无效或过期） | `20001` | `UNAUTHENTICATED` |
| 登录凭证无效、已过期或校验失败（OTP/OAuth proof 等） | `20006` | `UNAUTHENTICATED` |
| 权限不足（已识别主体但无权） | `20004` | `PERMISSION_DENIED` |
| 访问令牌无效/过期 | `20002` | `UNAUTHENTICATED` |
| 刷新令牌无效/过期 | `20003` | `UNAUTHENTICATED` |
| 设备或环境拒绝 | `20005` | `PERMISSION_DENIED` |
| 场景未知或不支持（如 `GetSignalBundleRef.scene`） | `40001` | `INVALID_ARGUMENT` |
| 用户或引用资源不存在 | `30001` | `NOT_FOUND` |
| 参数不合法 | `10002` | `INVALID_ARGUMENT` |
| 限流 / 配额超限 | `10005` | `RESOURCE_EXHAUSTED` |
| 版本冲突或陈旧写入（如 `UpdatePreferences`） | `10007` | `ABORTED` |
| 内部错误 | `90001` | `INTERNAL` |
| 依赖超时/错误 | `90002` / `90003` | `UNAVAILABLE` / `INTERNAL` |

**规则**：同一语义保持同一整数码；新增码仅追加。

说明：上表主要面向登录态接口、网关映射和真正的鉴权失败场景；`IntrospectAccessToken` / `IntrospectRefreshToken` 在令牌负例时仍按 §5.1 / §5.2 返回 `OK + valid = false`，不直接产出 20xxx 业务码。

细分规则：`10001` 用于"缺少语义必填字段"，`10002` 用于"字段已提供但校验失败或取值非法"；`20001` 用于"未登录 / 会话缺失"，`20002` / `20003` 用于"已提供 access / refresh token 但无效或过期"。

业务码载体规则：

- user-server 在返回非 `OK` gRPC 状态时，应通过 `google.rpc.ErrorInfo` 传递业务码。
- `ErrorInfo.reason` 使用稳定机器语义；整数业务码放入 `ErrorInfo.metadata["business_code"]`。
- 网关读取 `business_code` 作为对外 JSON `code` 的一等来源；若缺失，再按本表与 gRPC `Code` 做兜底映射。
- 不为同一错误同时维护另一套自定义 metadata 键。

### 6.3 主体解析与错误优先级

- `user_id` 语法合法但资源不存在：返回 `NOT_FOUND`。
- `session_id` 无效、过期或无法解析为当前会话：返回 `UNAUTHENTICATED`。
- 调用方已认证，但试图访问其无权代查/代写的主体：返回 `PERMISSION_DENIED`。
- 对列表型读接口不使用"空列表掩盖主体不存在"；主体解析先于列表查询。

## 7. 隐私、日志与幂等

- **禁止**在日志中落盘完整 `access_token` / `refresh_token` 明文；`free_text` 应限长并进入合规管道。
- `RecordHistoryEvent` 与 `SubmitFeedback` 建议结合 `client_event_id` / `client_request_id` 做窗口内去重；推荐去重窗口为 24 小时，键分别为 `(owner, client_event_id)` 与 `(actor, client_request_id)`。
- `AddFavorite` 对同一用户与 `content_id` 重复调用须幂等（`already_favorited`）。
- `personalization_allowed == false` 时，`GetSignalBundleRef` / `ResolveSignalBundle` 不得向调用方暴露可用于强个性化的明文特征。允许返回"弱化 ref / 弱化 payload"，但不得返回包含强个性化信号的 bundle；若实现选择直接拒绝，应返回 `PERMISSION_DENIED`，并建议在网关侧映射为策略限制类外部码 `60002`。

## 8. 集成与边界

### 8.1 调用方授权矩阵

调用方主体绑定所依赖的 metadata 语义与注入责任，遵循网关统一规范：下游至少携带 `request_id`、`trace_id`、`user_id`（或空）、`is_guest`、`client_platform`、`client_version`、`device_id`（若存在）、`app_id` 等统一 metadata。

| 调用方 | 允许 RPC 范围 | 约束 |
|--------|---------------|------|
| API 网关 | 全量终端用户相关 RPC | 可代表用户/访客态调用；越权主体访问返回 `PERMISSION_DENIED` |
| 下游服务（推荐、搜索等） | `GetConsent`, `GetSignalBundleRef`, `ResolveSignalBundle`, `HealthCheck` | 只读；不得代写用户资源 |
| 其他内部服务 | 默认仅 `HealthCheck` | 其余访问需显式服务级授权 |
| 显式授权后台/运维服务 | 按授权白名单开放 | 若使用 `IssueTokenPair.user_id`、代查/代写任意 `user_id`，必须在白名单中 |

访客态只允许使用支持 `session_id` / guest subject 的 RPC：`GetProfile`、`ListHistory`、`RecordHistoryEvent`、`ClearHistory`、`SubmitFeedback`、`GetSignalBundleRef`、`GetMeSummary`。`ResolveSignalBundle` 属于受信内部服务消费 `signal_bundle_ref` 的后续解析步骤，不是终端访客主体直接调用的接口。

仅接受 `user_id` 的 RPC（如偏好、收藏、同意状态）默认是**登录用户专属能力**；网关不得为访客伪造 `user_id` 调用。`GetProfile(session_id)` 中即使返回 guest 资料，也不表示该会话天然拥有可持久复用的登录 `user_id`。

### 8.2 语义必填与缺省规则

proto2 中的 `optional` 不代表业务上可省略；未列出的字段除非在对应 RPC 小节另有说明，否则按"缺省即空值/不更新"处理。

| RPC | 语义必填 / 缺省 |
|-----|-----------------|
| `IntrospectAccessToken` | `access_token` 必填；负例返回 `OK + valid=false` |
| `IntrospectRefreshToken` | `refresh_token` 必填；负例返回 `OK + valid=false` |
| `IssueTokenPair` | `identity` 必选一；`client_platform`、`device_id` 必填 |
| `RevokeSession` | `scope` 必填；`SINGLE_SESSION` 时 `session_id` 必填，`ALL_USER_SESSIONS` 时 `user_id` 必填 |
| `EnsureGuestSession` | `device_id`、`client_platform` 必填；`app_version` 可缺省 |
| `GetProfile` / `GetMeSummary` | `subject` 必选一 |
| `UpdateProfile` | `user_id` 必填；其余字段缺省表示不更新 |
| `GetPreferences` / `GetConsent` | `user_id` 必填 |
| `UpdatePreferences` / `UpdateConsent` | `user_id` 必填；更新体按对应 RPC 语义校验 |
| `ListFavorites` | `user_id` 必填；`page` 缺省时使用首页默认分页 |
| `ListHistory` | `owner` 必选一；`page` 缺省时使用首页默认分页 |
| `AddFavorite` | `user_id`、`content_id` 必填 |
| `RemoveFavorite` | `user_id` 必填，且 `favorite_id` / `content_id` 至少一项 |
| `RecordHistoryEvent` | `owner`、`content_ref`、`occurred_at` 必填 |
| `ClearHistory` | `owner`、`scope` 必填；`BEFORE_TIME` 时 `before_time` 必填 |
| `SubmitFeedback` | `actor`、`target_type`、`target_id`、`client_request_id` 必填，且内容三选一：`rating` / `reason_codes` / `free_text` |
| `GetSignalBundleRef` | `subject`、`scene` 必填，且 `scene` 受已注册场景约束 |
| `ResolveSignalBundle` | `signal_bundle_ref`、`caller_service` 必填；ref 语法非法返回 `INVALID_ARGUMENT`，语法合法但不可用返回 `OK + resolved=false` |

### 8.3 Introspect 上下文规则

- `IntrospectAccessToken.request_context` 与 `IntrospectRefreshToken.request_context` 为**可选风险上下文**，不是合法性校验的必填条件。
- 若缺省 `request_context`，服务端仍应完成令牌真伪判断，但可降级风险判定、设备绑定校验或审计丰富度。
- 若显式传入 `request_context`，其中字段应满足各自基本格式约束；明显非法值可返回 `INVALID_ARGUMENT`。

### 8.4 多应用支持

本服务支持多个应用接入，核心机制：

- **`app_id`**：通过 `RpcRequestContext.app_id` 传递，由网关注入。调用方不得自行填写。
- **数据隔离**：收藏、历史、偏好、反馈等用户数据按 `app_id` 隔离存储；账号与会话可跨应用共享或独立，由配置决定。
- **场景注册**：`GetSignalBundleRef.scene` 的允许值按 `app_id` 注册；未注册的 scene 返回 `INVALID_ARGUMENT`。
- **认证方式**：各应用可启用不同的认证方式组合（手机 OTP、OAuth 提供方等），由服务配置管理。
- **同意策略**：隐私同意的默认值与合规要求可按 `app_id` 定制。

- 若请求中未携带 `app_id`，服务端使用默认应用配置；新接入应用必须携带 `app_id`。

## 9. 幂等、乐观锁与冲突落定

本节把分散在各 RPC 小节的写语义集中落定，供实现与测试直接对照；不引入新字段，只固化既有约定。

### 9.1 幂等键与冲突码

| 接口 | 幂等键 / 并发控制 | 命中/冲突行为 |
|------|-------------------|---------------|
| `AddFavorite` | `(app_id, user_id, content_id)` 唯一 | 已收藏 → `already_favorited=true`，`OK` |
| `RecordHistoryEvent` | `(owner, client_event_id)` 24h 去重 | 命中 → `recorded=false, deduplicated=true`，`OK` |
| `SubmitFeedback` | `(actor, client_request_id)` 24h 去重 | 命中 → 返回首次 `feedback_id`，`OK` |
| `UpdatePreferences` | `preferences.preferences_version` 乐观锁（整文档替换） | 携带版本 `< 当前版本` → **`10007`** / `ABORTED` |
| `EnsureGuestSession` | `(app_id, device_id)` 复用活跃访客会话 | 复用 → `created=false`；新建 → `created=true` |
| `RevokeSession` | 目标已撤销/不存在视为幂等 | `revoked=false`，`OK`（非错误） |
| `UpdateConsent` | 当前态 upsert + 审计历史追加，末写生效 | 无冲突码；每次必追加 `user_consent_history` |
| `IssueTokenPair`（OTP） | `verification_id` 单次消费（`consumed_at`） | 复用已消费 OTP → `20006`；超尝试/风控 → `20005` |

> **统一约定**：所有“版本化整文档写”的陈旧写入/版本冲突一律返回业务码 **`10007`**（gRPC `ABORTED`），与 §6.2 和 `services/gateway/api.md` 完全一致；本域不为该语义再造其他码。

### 9.2 逐接口鉴权要求与身份来源

身份由 **网关注入并透传**；本域不直连客户端，不自行校验登录态明文凭据（OTP/OAuth 的 proof 校验属本域账号能力，不同于"判断调用方是否登录"）。下表标注各 RPC 的调用方资格（详见 §8.1 授权矩阵）。

| RPC | 允许调用方 | 主体要求 | 备注 |
|-----|-----------|----------|------|
| `IntrospectAccessToken` / `IntrospectRefreshToken` | 网关 | 无（输入即 token） | 负例 `OK + valid=false`，不产 20xxx |
| `IssueTokenPair` | 网关；`identity.user_id` 分支仅白名单受信服务 | proof 自带身份 | 普通调用方不得用任意 `user_id` 签发 |
| `RevokeSession` | 网关；越权撤销他人会话 → `20004` | 需匹配目标 `user_id` | 撤销范围见 §5.4 |
| `EnsureGuestSession` | 网关 | 访客（设备态） | — |
| `GetProfile` / `GetMeSummary` / `ListHistory` / `RecordHistoryEvent` / `ClearHistory` / `SubmitFeedback` / `GetSignalBundleRef` | 网关（可代表登录用户或访客 `session_id`） | 登录或访客均可 | 支持 `session_id` 分支 |
| `UpdateProfile` / `GetPreferences` / `UpdatePreferences` / `ListFavorites` / `AddFavorite` / `RemoveFavorite` / `GetConsent` / `UpdateConsent` | 网关（仅登录用户 `user_id`） | **登录专属** | 网关不得为访客伪造 `user_id` |
| `ResolveSignalBundle` | 受信下游服务（如推荐） | 服务身份；`caller_service` 须与认证身份一致 | 未注册调用方 → `PERMISSION_DENIED` |
| `HealthCheck` | 任意内部服务 / 编排探针 | 无 | — |

> 本域是**会话与令牌签发/刷新/撤销的真实边界**：网关只做 token 透传与映射，签发、轮换、撤销、重放检测、OTP 核验的权威判定都在本域完成（见 workflow §2、§9.2）。运营/合规类越权动作（代查代写任意 `user_id`、账户删除/导出）须经白名单服务身份，普通调用方一律 `PERMISSION_DENIED` / `20004`。

## 附录 A. 典型请求 / 响应示例

以下示例使用 **proto-text** 形式展示 `proto` 消息，便于直接对应内部 RPC 报文结构；实际 wire 传输仍以 `proto2 + gRPC` 为准。枚举展示为**完整 proto 符号名**，`Timestamp` 使用消息形态示意。

### A.1 `IssueTokenPair`

#### Request (`textproto`)

```textproto
phone_otp {
  phone_e164: "+8613800138000"
  otp_code: "123456"
  verification_id: "verify_01HSYQ8Y8Y2R7A4H7S0X"
}
device_fingerprint: "fp_ios_7f3e1c5a"
client_platform: CLIENT_PLATFORM_IOS
app_version: "1.4.2"
device_id: "device_9f1b5e18"
request_context {
  app_id: "simple_living"
  client_platform: CLIENT_PLATFORM_IOS
  app_version: "1.4.2"
  device_id: "device_9f1b5e18"
}
```

#### Response (`textproto`)

```textproto
access_token: "at_eyJhbGciOi..."
refresh_token: "rt_eyJhbGciOi..."
expires_in_seconds: 7200
session_id: "sess_01HSYQBY1S7W4T1M7Q2B"
access_expires_at {
  seconds: 1774699200
}
```

### A.2 `GetMeSummary`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
```

#### Response (`textproto`)

```textproto
profile {
  user_id: "user_01HSYQK5PZ4K4J9R2M8D"
  is_guest: false
  display_name: "Shinan"
  avatar_url: "https://cdn.example.com/avatar/u_01.png"
  locale: "zh-CN"
  notification_prefs {
    email_enabled: false
    push_enabled: true
    sms_enabled: false
    in_app_enabled: true
  }
}
counts {
  favorites_count: 12
  history_count: 37
}
consent {
  personalization_allowed: true
  analytics_allowed: true
  marketing_allowed: false
  consent_version: "2026-03-privacy-v3"
}
```

### A.3 `AddFavorite`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
content_id: "content_1001"
content_type: CONTENT_REF_TYPE_GUIDE_CARD
```

#### Response (`textproto`)

```textproto
favorite_id: "fav_01HSYQPDYX4B7C8WQ6ZK"
already_favorited: false
```

### A.4 `IntrospectAccessToken`

#### Request (`textproto`)

```textproto
access_token: "at_eyJhbGciOi..."
request_context {
  app_id: "simple_living"
  client_platform: CLIENT_PLATFORM_IOS
  app_version: "1.4.2"
  device_id: "device_9f1b5e18"
}
```

#### Response (`textproto`)

```textproto
valid: true
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
session_id: "sess_01HSYQBY1S7W4T1M7Q2B"
token_expires_at {
  seconds: 1774700400
}
scopes: "session:read"
scopes: "session:write"
auth_tier: AUTH_TIER_STANDARD
```

### A.5 `IntrospectRefreshToken`

#### Request (`textproto`)

```textproto
refresh_token: "rt_eyJhbGciOi..."
request_context {
  app_id: "simple_living"
  client_platform: CLIENT_PLATFORM_IOS
  app_version: "1.4.2"
  device_id: "device_9f1b5e18"
}
```

#### Response (`textproto`)

```textproto
valid: true
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
session_id: "sess_01HSYQBY1S7W4T1M7Q2B"
rotation_required: true
```

### A.6 `RevokeSession`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
session_id: "sess_01HSYQBY1S7W4T1M7Q2B"
scope: REVOKE_SESSION_SCOPE_SINGLE_SESSION
```

#### Response (`textproto`)

```textproto
revoked: true
```

### A.7 `EnsureGuestSession`

#### Request (`textproto`)

```textproto
device_id: "device_9f1b5e18"
client_platform: CLIENT_PLATFORM_IOS
app_version: "1.4.2"
```

#### Response (`textproto`)

```textproto
session_id: "sess_guest_01HSZ9J6T0N3A8M2Q4B7"
created: true
```

### A.8 `GetProfile`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
```

#### Response (`textproto`)

```textproto
profile {
  user_id: "user_01HSYQK5PZ4K4J9R2M8D"
  is_guest: false
  display_name: "Shinan"
  locale: "zh-CN"
}
```

### A.9 `UpdateProfile`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
display_name: "Shinan"
locale: "zh-CN"
notification_prefs {
  push_enabled: true
  in_app_enabled: true
}
```

#### Response (`textproto`)

```textproto
profile {
  user_id: "user_01HSYQK5PZ4K4J9R2M8D"
  is_guest: false
  display_name: "Shinan"
  locale: "zh-CN"
  notification_prefs {
    push_enabled: true
    in_app_enabled: true
  }
}
```

### A.10 `GetPreferences`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
```

#### Response (`textproto`)

```textproto
preferences {
  preferences_version: 3
  structured {
    theme_interests: "clothing"
    default_sort: "latest"
  }
}
```

### A.11 `UpdatePreferences`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
preferences {
  preferences_version: 4
  structured {
    theme_interests: "clothing"
    theme_interests: "food"
    default_sort: "latest"
  }
}
```

#### Response (`textproto`)

```textproto
preferences {
  preferences_version: 4
  structured {
    theme_interests: "clothing"
    theme_interests: "food"
    default_sort: "latest"
  }
}
```

### A.12 `ListFavorites`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
page {
  cursor: ""
  limit: 20
}
content_type: FAVORITE_CONTENT_TYPE_GUIDE_CARD
```

#### Response (`textproto`)

```textproto
items {
  favorite_id: "fav_01HSYQPDYX4B7C8WQ6ZK"
  content_id: "content_1001"
  content_type: CONTENT_REF_TYPE_GUIDE_CARD
  favorited_at {
    seconds: 1774695600
  }
}
pagination {
  has_more: false
  limit: 20
}
```

### A.13 `RemoveFavorite`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
favorite_id: "fav_01HSYQPDYX4B7C8WQ6ZK"
content_id: "content_1001"
```

#### Response (`textproto`)

```textproto
removed: true
```

### A.14 `ListHistory`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
page {
  cursor: ""
  limit: 20
}
```

#### Response (`textproto`)

```textproto
items {
  content_ref {
    type: CONTENT_REF_TYPE_GUIDE_CARD
    content_id: "content_1001"
  }
  last_seen_at {
    seconds: 1774697400
  }
}
pagination {
  has_more: false
  limit: 20
}
```

### A.15 `RecordHistoryEvent`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
content_ref {
  type: CONTENT_REF_TYPE_GUIDE_CARD
  content_id: "content_1001"
}
occurred_at {
  seconds: 1774697400
}
source_surface: HISTORY_SOURCE_SURFACE_FEED
client_event_id: "evt_01HSZA1B5V6Q8N2M1P3R"
```

#### Response (`textproto`)

```textproto
recorded: true
deduplicated: false
```

### A.16 `ClearHistory`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
scope: CLEAR_HISTORY_SCOPE_ALL
```

#### Response (`textproto`)

```textproto
removed_count: 37
```

### A.17 `SubmitFeedback`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
target_type: FEEDBACK_TARGET_TYPE_APP
target_id: "app_v1"
reason_codes: "ux_like"
free_text: "喜欢整体风格"
client_request_id: "fb_01HSZA6E4K9Q3P2M5N7T"
```

#### Response (`textproto`)

```textproto
feedback_id: "feedback_01HSZA7D2V5M8Q1N6R3K"
```

### A.18 `GetConsent`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
```

#### Response (`textproto`)

```textproto
consent {
  personalization_allowed: true
  analytics_allowed: true
  marketing_allowed: false
  consent_version: "2026-03-privacy-v3"
}
```

### A.19 `UpdateConsent`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
consent {
  personalization_allowed: true
  analytics_allowed: true
  marketing_allowed: false
  consent_version: "2026-03-privacy-v3"
}
```

#### Response (`textproto`)

```textproto
consent {
  personalization_allowed: true
  analytics_allowed: true
  marketing_allowed: false
  consent_version: "2026-03-privacy-v3"
  updated_at {
    seconds: 1773976200
  }
  jurisdiction: "CN"
}
```

### A.20 `GetSignalBundleRef`

#### Request (`textproto`)

```textproto
user_id: "user_01HSYQK5PZ4K4J9R2M8D"
scene: "home_feed"
```

#### Response (`textproto`)

```textproto
ref {
  signal_bundle_ref: "sigref_01HSZ12V0N6A8Q0X1N2M"
  expires_at {
    seconds: 1774702800
  }
  bundle_version: "v2"
  scopes: "recommendation"
}
```

### A.21 `ResolveSignalBundle`

#### Request (`textproto`)

```textproto
signal_bundle_ref: "sigref_01HSZ12V0N6A8Q0X1N2M"
caller_service: "recommendation-service"
```

#### Response (`textproto`)

```textproto
resolved: true
expires_at {
  seconds: 1774702800
}
payload {
  opaque_payload_json: "{\"theme_interests\":[\"clothing\"]}"
  integrity_hash: "sha256:ab12cd34"
}
```

### A.22 `HealthCheck`

#### Request (`textproto`)

```textproto
# empty
```

#### Response (`textproto`)

```textproto
status: "SERVING"
components {
  name: "primary-db"
  ok: true
  detail: ""
}
```
