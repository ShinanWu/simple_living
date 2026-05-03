# Mock 数据与验收矩阵

## 1. 目标

给 Web/iOS/Android 提供统一联调输入与验收口径，支持并行开发而不依赖口头同步。

## 2. 验收矩阵（页面 x 状态 x 错误）

| 页面 | 必测状态 | 必测错误码 |
|------|----------|------------|
| 首页（四主题） | loading/success/empty/error/offline | `90002`, `90001` |
| 导购详情 | loading/success/empty/error/offline | `30001`, `90001` |
| 跳转准备 | loading/success/error/offline | `10005`, `20001`, `90002` |
| 我的 | loading/success/error/offline | `20002`, `90001` |
| 登录（微信 / 手机号） | idle/authorizing/otp/issuing_token/success/error | `20006`, `10005`, `10002`, `20005`, `90002`, `20003` |

## 3. 三端并行验收清单

- 三端对同一错误码采取同级别处理（轻提示/内联/阻断）
- 三端四主题 Tab 的顺序、默认主题和切换行为一致
- 三端分页与重试策略一致，不出现重复请求放大
- 登录：微信与手机号两条路径均能换发 token；`20002` 触发的 refresh 与 `20003` 的清空回访客态一致

### 3.1 首页改版专项验收（2026-04-14）

- 首页信息架构为：顶部信息区（标题+提示）+ 中部单卡片 + 底部固定 `衣/食/住/行` 主题栏。
- 底部主题栏需支持两种切换方式：点按切换、左右滑语义切换；切换后卡片流刷新并回到当前主题首卡。
- 卡片交互保持一致：上滑下一条、非第一页下滑上一条、第一页下滑触发二段提示并可松手刷新。
- 拖拽过程中不得误触卡片跳转详情；拖拽结束后可恢复点击进入详情。

## 4. 登录联调 Mock（Gateway 信封）

以下示例遵循 [common-response.md](../docs/contracts/common-response.md) 与 [gateway api.md](../services/gateway/docs/api.md) 顶层形状；字段名均为 `snake_case`。

**说明**：`POST /api/v2/auth/token/issue` 的 `phone_otp.verification_id` 依赖「验证码下发」步骤；若网关暂未暴露独立下发路由，联调可使用测试环境提供的固定 `verification_id` / 固定验证码，或 Mock 服务返回下列响应。

### 4.1 访客会话成功

`POST /api/v2/guest/session`

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {
    "session_id": "sess_guest_01mock9j6t0n3a8m2q4b7",
    "created": true
  },
  "meta": { "request_id": "req_guest_001", "server_time_ms": 1774699800000 }
}
```

### 4.2 发 token 成功 — 手机号 OTP

`POST /api/v2/auth/token/issue`

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {
    "access_token": "at_mock_phone_xxx",
    "refresh_token": "rt_mock_phone_yyy",
    "expires_in": 3600,
    "session_id": "sess_01mockyqby1s7w4t1m7q2b",
    "access_expires_at": "2026-04-03T14:00:00Z"
  },
  "meta": { "request_id": "req_issue_phone_001", "server_time_ms": 1774699800000 }
```

请求体示例（与网关 `account_proof.phone_otp` 对齐）：

```json
{
  "account_proof": {
    "phone_otp": {
      "phone_e164": "+8613800138000",
      "otp_code": "123456",
      "verification_id": "verify_01mockyq8y8y2r7a4h7s0x"
    }
  },
  "client_platform": "ios",
  "device_id": "device_mock_9f1b5e18",
  "request_context": {
    "client_platform": "ios",
    "app_version": "1.0.0",
    "device_id": "device_mock_9f1b5e18"
  }
}
```

### 4.3 发 token 成功 — 微信 OAuth

`POST /api/v2/auth/token/issue`

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {
    "access_token": "at_mock_wx_xxx",
    "refresh_token": "rt_mock_wx_yyy",
    "expires_in": 3600,
    "session_id": "sess_01mockyqby1s7w4t1m7q2b",
    "access_expires_at": "2026-04-03T14:00:00Z"
  },
  "meta": { "request_id": "req_issue_wx_001", "server_time_ms": 1774699800000 }
}
```

请求体示例：

```json
{
  "account_proof": {
    "oauth": {
      "provider": "wechat",
      "provider_subject": "wx_openid_mock_7f3a9c",
      "authorization_code": "wx_auth_code_mock_once"
    }
  },
  "client_platform": "ios",
  "device_id": "device_mock_9f1b5e18"
}
```

### 4.4 发 token 失败 — 凭证无效（OTP / 微信码过期或错）

```json
{
  "success": false,
  "code": 20006,
  "message": "invalid credentials",
  "data": null,
  "meta": { "request_id": "req_issue_fail_001", "server_time_ms": 1774699800000 }
}
```

### 4.5 发 token 失败 — 限流

```json
{
  "success": false,
  "code": 10005,
  "message": "rate limited",
  "data": null,
  "meta": { "request_id": "req_issue_rl_001", "server_time_ms": 1774699800000 }
}
```

### 4.6 发 token 失败 — 参数校验

```json
{
  "success": false,
  "code": 10002,
  "message": "validation failed",
  "data": null,
  "errors": [
    { "field": "account_proof.phone_otp.phone_e164", "code": 10002, "message": "invalid e164" }
  ],
  "meta": { "request_id": "req_issue_val_001", "server_time_ms": 1774699800000 }
}
```

### 4.7 发 token 失败 — 设备/环境拒绝

```json
{
  "success": false,
  "code": 20005,
  "message": "device rejected",
  "data": null,
  "meta": { "request_id": "req_issue_dev_001", "server_time_ms": 1774699800000 }
}
```

### 4.8 刷新 token 成功

`POST /api/v2/auth/token/refresh`

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {
    "access_token": "at_mock_refreshed_xxx",
    "refresh_token": "rt_mock_rotated_yyy",
    "expires_in": 3600,
    "session_id": "sess_01mockyqby1s7w4t1m7q2b",
    "access_expires_at": "2026-04-03T15:00:00Z"
  },
  "meta": { "request_id": "req_refresh_ok_001", "server_time_ms": 1774699800000 }
}
```

### 4.9 刷新 token 失败 — refresh 无效或过期

```json
{
  "success": false,
  "code": 20003,
  "message": "refresh token invalid",
  "data": null,
  "meta": { "request_id": "req_refresh_fail_001", "server_time_ms": 1774699800000 }
}
```

### 4.10 依赖超时（登录/刷新均可能出现）

```json
{
  "success": false,
  "code": 90002,
  "message": "dependency timeout",
  "data": null,
  "meta": { "request_id": "req_issue_to_001", "server_time_ms": 1774699800000 }
}
```

登录失败时的用户可见文案建议见 [frontend-login-error-copy.md](./frontend-login-error-copy.md)。
