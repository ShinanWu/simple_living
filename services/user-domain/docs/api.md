# user-domain — 内部 RPC API（proto2）

## 1. 文档范围

- **受众**：`gateway`、受信任的相邻域（如 `recommendation-domain`）通过 **gRPC（proto2）** 调用本文档定义的 RPC。
- **非目标**：对外 HTTP/JSON 路径与字段由 [gateway api.md](../../gateway/docs/api.md) 维护；本文件 **不描述** 终端可见 URL。
- **Proto 文件**：`services/user-domain/proto/user_domain_models.proto`（枚举与消息）、`user_domain_service.proto`（`UserDomainService`）。
- **包名**：`simple_living.user_domain`。
- **时间**：逻辑上使用 UTC；wire 上使用 `google.protobuf.Timestamp`（或下文已注明处）。

## 2. 服务定义

```
service UserDomainService {
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

**实现名**：`simple_living.user_domain.UserDomainService/<Method>`（语言插件可映射为不同桩名，须在仓库内统一）。

## 3. 枚举（wire 数值以 proto 为准）

| Proto 枚举 | 值（示意） | 语义 |
|------------|------------|------|
| `AccountStatus` | `ACCOUNT_STATUS_ACTIVE`, `SUSPENDED`, `DELETED_PENDING`, … | 账号生命周期（扩展时仅追加） |
| `ClientPlatform` | `WEB`, `IOS`, `ANDROID`, `WECHAT_MINIPROGRAM`, `DOUYIN_MINIPROGRAM` | 客户端平台 |
| `AuthTier` | `STANDARD`, `RISK_CHALLENGE` | 风控分层 |
| `FeedbackTargetType` | `GUIDE_CARD`, `RECOMMENDATION_RESULT`, `APP`, `OTHER` | 反馈对象 |
| `HistorySourceSurface` | `FEED`, `SEARCH`, `DETAIL` | 浏览来源 |
| `FavoriteContentType` | `GUIDE_CARD` | 收藏类型 |
| `ContentRefType` | `GUIDE_CARD` | 内容引用 |
| `RevokeSessionScope` | `SINGLE_SESSION`, `ALL_USER_SESSIONS` | 登出范围 |
| `ClearHistoryScope` | `ALL`, `BEFORE_TIME` | 清空历史策略 |

`UNSPECIFIED = 0` 在各枚举中表示未设置；实现应拒绝业务语义依赖 `UNSPECIFIED` 的写入请求（除非 RPC 明确允许缺省）。

## 4. 共享与嵌套消息

### 4.1 `RpcRequestContext`

| 字段 | 类型 | 说明 |
|------|------|------|
| `client_platform` | `ClientPlatform` | |
| `app_version` | string | |
| `device_id` | string | |

### 4.2 `ContentRef`

| 字段 | 类型 |
|------|------|
| `type` | `ContentRefType` |
| `guide_card_id` | string |

### 4.3 `NotificationPrefs`

| 字段 | 类型 |
|------|------|
| `email_enabled` | bool |
| `push_enabled` | bool |
| `sms_enabled` | bool |
| `in_app_enabled` | bool |

### 4.4 `UserProfile`

| 字段 | 类型 |
|------|------|
| `user_id` | string |
| `is_guest` | bool |
| `display_name` | string |
| `avatar_url` | string |
| `locale` | string |
| `bio` | string |
| `notification_prefs` | `NotificationPrefs` |

### 4.5 `ThemePreferences`

| 字段 | 类型 |
|------|------|
| `theme_interests` | repeated string |
| `content_filters` | map<string, string> |
| `default_sort` | string |

### 4.6 `UserPreferences`

| 字段 | 类型 |
|------|------|
| `preferences_version` | int64 |
| `structured` | `ThemePreferences` |

### 4.7 `PaginationCursor` / `PaginationCursorResult`

| 消息 | 字段 |
|------|------|
| `PaginationCursor` | `cursor`, `limit` |
| `PaginationCursorResult` | `next_cursor`, `has_more`, `limit` |

### 4.8 `ConsentState`

| 字段 | 类型 |
|------|------|
| `personalization_allowed` | bool |
| `analytics_allowed` | bool |
| `marketing_allowed` | bool |
| `consent_version` | string |
| `updated_at` | `Timestamp` |
| `jurisdiction` | string |

### 4.9 `SignalBundleRef`

| 字段 | 类型 |
|------|------|
| `signal_bundle_ref` | string |
| `expires_at` | `Timestamp` |
| `bundle_version` | string |
| `scopes` | repeated string |

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

| 字段 | 类型 |
|------|------|
| `opaque_payload_json` | string |
| `integrity_hash` | string |

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

### 5.3 `IssueTokenPair`

| `IssueTokenPairRequest` | 类型 |
|-------------------------|------|
| **oneof `identity`** | |
| `user_id` | string |
| `phone_otp` | `AccountProofPhoneOtp` |
| `oauth` | `AccountProofOAuth` |
| `device_fingerprint` | string |
| `client_platform` | `ClientPlatform` |
| `app_version` | string |
| `device_id` | string |
| `request_context` | `RpcRequestContext` |

`AccountProofPhoneOtp`: `phone_e164`, `otp_code`, `verification_id`。  
`AccountProofOAuth`: `provider`, `provider_subject`, `authorization_code`。

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

| `RevokeSessionResponse` | 类型 |
|-------------------------|------|
| `revoked` | bool |

### 5.5 `EnsureGuestSession`

| `EnsureGuestSessionRequest` | 类型 |
|-----------------------------|------|
| `device_id` | string |
| `client_platform` | `ClientPlatform` |
| `app_version` | string |

| `EnsureGuestSessionResponse` | 类型 |
|------------------------------|------|
| `session_id` | string |
| `created` | bool |

### 5.6 `GetProfile` / `UpdateProfile`

`GetProfileRequest` **oneof `subject`**：`user_id` | `session_id`。  
`GetProfileResponse`：`profile` (`UserProfile`)。

`UpdateProfileRequest`：`user_id`，可选 `display_name`, `avatar_url`, `locale`, `bio`, `notification_prefs`。  
`UpdateProfileResponse`：`profile`。

### 5.7 `GetPreferences` / `UpdatePreferences`

`GetPreferencesRequest`：`user_id`。  
`GetPreferencesResponse`：`preferences` (`UserPreferences`)。

`UpdatePreferencesRequest`：`user_id`, `preferences`。  
`UpdatePreferencesResponse`：`preferences`。

### 5.8 `ListFavorites` / `AddFavorite` / `RemoveFavorite`

`ListFavoritesRequest`：`user_id`, `page` (`PaginationCursor`), `content_type` (`FavoriteContentType`)。  
`ListFavoritesResponse`：`items` (`repeated FavoriteItem`), `pagination`。

`FavoriteItem`：`favorite_id`, `guide_card_id`, `favorited_at`。

`AddFavoriteRequest`：`user_id`, `guide_card_id`。  
`AddFavoriteResponse`：`favorite_id`, `already_favorited`。

`RemoveFavoriteRequest`：`user_id`, `favorite_id`, `guide_card_id`（`guide_card_id` 可选辅助）。  
`RemoveFavoriteResponse`：`removed`。

### 5.9 `ListHistory` / `RecordHistoryEvent` / `ClearHistory`

`ListHistoryRequest`：**oneof `owner`** `user_id` | `session_id`；`page`。  
`ListHistoryResponse`：`items` (`HistoryItem`), `pagination`。

`HistoryItem`：`content_ref`, `last_seen_at`, `first_seen_at`, `impression_count`, `source_surface`。

`RecordHistoryEventRequest`：**oneof `owner`**；`content_ref`, `occurred_at`, `source_surface`, `client_event_id`。  
`RecordHistoryEventResponse`：`recorded`, `deduplicated`。

`ClearHistoryRequest`：**oneof `owner`**；`scope`；`before_time`（当 `scope = BEFORE_TIME`）。  
`ClearHistoryResponse`：`removed_count`。

### 5.10 `SubmitFeedback`

`SubmitFeedbackRequest`：**oneof `actor`** `user_id` | `session_id`；`target_type`, `target_id`, `rating` (`google.protobuf.DoubleValue`), `reason_codes`, `free_text`, `client_request_id`。

`SubmitFeedbackResponse`：`feedback_id`。

### 5.11 `GetConsent` / `UpdateConsent`

`GetConsentRequest`：`user_id`。  
`GetConsentResponse`：`consent` (`ConsentState`)。

`UpdateConsentRequest`：`user_id`, `consent`。  
`UpdateConsentResponse`：`consent`。

### 5.12 `GetSignalBundleRef`

`GetSignalBundleRefRequest`：**oneof `subject`** `user_id` | `session_id`；`scene`。  
`GetSignalBundleRefResponse`：`ref` (`SignalBundleRef`)。

### 5.13 `ResolveSignalBundle`

`ResolveSignalBundleRequest`：`signal_bundle_ref`, `caller_service`。

`ResolveSignalBundleResponse`：`resolved`, `expires_at`, `payload` (`ResolveSignalBundlePayload`)。

### 5.14 `GetMeSummary`

`GetMeSummaryRequest`：**oneof `subject`** `user_id` | `session_id`。

`GetMeSummaryResponse`：`profile`, `counts` (`MeCounts`), `consent`。

### 5.15 `HealthCheck`

`HealthCheckRequest`：空。  
`HealthCheckResponse`：`status`, `components` (`repeated HealthComponentStatus`)。

## 6. 错误与状态约定

### 6.1 gRPC 状态码（传输层）

| gRPC `Code` | 典型场景 |
|-------------|----------|
| `OK` | 业务成功 |
| `INVALID_ARGUMENT` | 字段缺失、枚举非法、`UNSPECIFIED` 用于必填语义 |
| `UNAUTHENTICATED` | 令牌无效、会话不存在 |
| `PERMISSION_DENIED` | 已识别主体但无权 |
| `NOT_FOUND` | `favorite_id` / 用户资源不存在 |
| `ALREADY_EXISTS` | 若某写路径需显式冲突（少用） |
| `RESOURCE_EXHAUSTED` | 配额/限流 |
| `FAILED_PRECONDITION` | 状态机不允许 |
| `INTERNAL` | 未分类失败 |

### 6.2 业务码（建议经 `google.rpc.ErrorInfo` 或团队约定 metadata 传递）

与 [docs/contracts/error-codes.md](../../../docs/contracts/error-codes.md) 对齐的 **代表性** 映射（实现须在网关中转为对外 `code`）：

| 场景 | 建议业务码 | gRPC 辅助 |
|------|------------|-----------|
| 访问令牌无效/过期 | `20002` | `UNAUTHENTICATED` |
| 刷新令牌无效/过期 | `20003` | `UNAUTHENTICATED` |
| 设备或环境拒绝 | `20005` | `PERMISSION_DENIED` |
| 引用内容不存在（收藏等） | `30001` | `NOT_FOUND` |
| 参数不合法 | `10002` | `INVALID_ARGUMENT` |
| 依赖超时/错误 | `90002` / `90003` | `UNAVAILABLE` / `INTERNAL` |

**规则**：同一语义跨版本保持同一整数码；新增码仅追加。

## 7. 隐私、日志与幂等

- **禁止**在日志中落盘完整 `access_token` / `refresh_token` / `access_token` 明文；`free_text` 应限长并进入合规管道。
- `RecordHistoryEvent` 与 `SubmitFeedback` 建议结合 `client_event_id` / `client_request_id` 做窗口内去重。
- `AddFavorite` 对同一用户与 `guide_card_id` 重复调用须幂等（`already_favorited`）。
- `personalization_allowed == false` 时，`GetSignalBundleRef` / `ResolveSignalBundle` 不得向调用方暴露可用于强个性化的明文特征（策略由实现与治理评审约束）。

## 8. 与其他域的边界

- `guide_card_id` 的存在性以 `content-domain` 为准；本域可做懒校验或异步修复。
- `recommendation-domain` 消费 `signal_bundle_ref` 与同意状态；**不回写**推荐分数到本域。
- 对外 JSON 字段名仍由 `gateway` 以契约为准做最终裁剪与映射。
