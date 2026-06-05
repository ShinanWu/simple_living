# 登录交互逻辑（v1）

## 1. 目标与范围

- 本文定义前端登录相关交互流程、状态和异常处理，供 Web/iOS/Android 统一实现。
- 前端仅调用 `gateway` 对外 JSON 接口；登录业务真相由 `user-server` 持有。
- v1 支持两类登录：手机号注册/登录（自有方式）与微信登录。
- 登录面板默认突出微信主 CTA，同时保留手机号注册/登录入口并可用。

## 2. 设计原则（抖音风格参考）

- 登录触发尽量后置：先允许访客浏览，再在需要登录的动作上触发登录引导。
- 主路径单焦点：登录面板主 CTA 默认为“微信一键登录”，手机号入口作为同层可用备选。
- 操作反馈即时：点击、授权中、成功回跳、失败重试都有明确状态。
- 风险可控：服务端返回拒绝或风控码时，不暴露内部细节，给可执行提示。
- 易扩展：登录方式以 `provider` 配置化扩展，不重写主流程。

## 3. 信息架构与入口

| 场景 | 登录要求 | 交互策略 |
|------|----------|----------|
| 首页浏览、详情浏览 | 可访客 | 不强制登录，使用访客会话 |
| 收藏、偏好编辑、同意设置 | 必须登录 | 触发登录面板，成功后继续原动作 |
| 去购买（redirect_prepare） | 访客或登录均可 | 默认放行；若风控要求再触发登录 |
| 我的页基础信息 | 可访客 | 展示访客态，提供显式“去登录”入口 |

## 4. 与 Gateway 接口映射

### 4.1 访客会话

- `POST /api/v2/guest/session`
- 用途：启动时或无主体时创建 `X-Guest-Session-Id`，支持访客可用路径。

### 4.2 手机号注册/登录发令牌

- `POST /api/v2/auth/token/issue`
- 请求体 `account_proof.phone_otp`：
  - `phone_e164`: 标准手机号（如 `+8613800138000`）
  - `otp_code`: 短信验证码
  - `verification_id`: 发送验证码时返回的一次性校验 ID
- 适用场景：新用户注册即登录、老用户验证码登录（后端按手机号归并账号）。
- 成功后保存 `access_token`、`refresh_token`、`session_id`、`access_expires_at`。

### 4.3 微信登录发令牌

- `POST /api/v2/auth/token/issue`
- 请求体 `account_proof.oauth` 固定使用：
  - `provider: "wechat"`
  - `provider_subject`: 微信侧用户唯一标识
  - `authorization_code`: 微信授权码（一次性）
- 成功后保存 `access_token`、`refresh_token`、`session_id`、`access_expires_at`。

### 4.4 刷新与登出

- 刷新：`POST /api/v2/auth/token/refresh`
- 登出：`DELETE /api/v2/auth/session`

## 5. 登录主流程（前端）

```text
App 启动
  -> 若无 access_token，确保 guest session 可用
  -> 用户触发需登录动作（如收藏）
  -> 打开登录面板（微信主按钮 + 手机号入口）
  -> 用户选择登录方式：
       A. 微信：拉起授权，拿到 authorization_code，调用 issue_token(oauth.wechat)
       B. 手机号：输入手机号 -> 获取验证码 -> 输入 otp_code，调用 issue_token(phone_otp)
  -> 成功：写入 token，关闭面板，继续之前动作
  -> 失败：按错误码提示 + 保持在登录面板
```

## 6. 登录页/面板状态定义

- `idle`：展示登录方式列表（v1 可选微信、手机号）。
- `authorizing`：拉起微信授权中，按钮禁用、显示进度。
- `phone_input`：手机号输入态，校验 E.164 或本地格式后标准化。
- `otp_sending`：请求验证码中，防重复点击。
- `otp_input`：验证码输入态，展示倒计时与重发入口。
- `issuing_token`：向网关换取 token，禁止重复提交。
- `success`：登录成功，关闭面板并回跳来源页。
- `error_retryable`：可重试错误（网络波动、超时、临时依赖异常）。
- `error_blocking`：不可立即重试（风控拒绝、环境不满足）。

## 7. 错误码与前端策略

| code | 含义 | 前端动作 |
|------|------|----------|
| `20006` | 登录凭证无效/过期 | 提示“授权已失效，请重新登录”，回到 `idle` |
| `20005` | 设备或环境被拒绝 | 提示“当前环境暂不可登录”，保留退出 |
| `10001`/`10002` | 参数缺失或非法 | 记录埋点，提示通用失败，可重试 |
| `10005` | 频率限制 | 提示稍后再试，短时禁用按钮 |
| `90002`/`90003` | 依赖异常/超时 | 提示网络繁忙，可重试 |

说明：前端分支以 `code` 为准，不依赖 `message` 做逻辑判断。

## 8. Token 生命周期策略

- 请求携带：优先 `Authorization: Bearer <access_token>`。
- 401 且 `code=20002`：尝试一次 `token/refresh`。
- 刷新成功：重放原请求一次。
- 刷新失败（`20003` 或再次失败）：清理本地 token，回退访客态并触发登录引导。
- 并发保护：同一时刻仅允许一个 refresh 流程，其余请求等待结果。

## 9. 手机号登录交互约束（v1）

- 手机号路径支持“注册即登录”和“验证码登录”统一流程。
- 手机号输入后必须标准化为 `phone_e164` 再提交。
- 验证码重发应有倒计时（建议 60s），并限制短时间高频请求。
- 当手机号已绑定账号时走登录；未绑定时由服务端创建账号并返回 token 对。

## 10. 可扩展登录方式设计

- 登录方式配置建议：
  - `id`：如 `wechat`, `phone_otp`, `apple`
  - `enabled`：是否可用
  - `priority`：展示优先级
  - `capabilities`：是否需要客户端 SDK、是否支持静默授权
- v1 发布配置：`wechat.enabled=true`，`phone_otp.enabled=true`。
- 新增方式时，仅新增 provider 适配和配置，不改“授权 -> issue token -> 回跳”主状态机。

## 11. 埋点与验收建议

- 关键埋点：登录面板曝光、微信点击、手机号发送验证码、验证码提交、授权成功/失败、issue_token 成功/失败、刷新成功/失败。
- 验收最小集：
  - 访客进入首页与我的页可正常浏览
  - 收藏动作可拉起登录，微信和手机号两种方式成功后都能自动完成收藏
  - access 过期可自动刷新；refresh 过期会回到访客态
  - 手机号登录支持验证码发送、校验、失败重试和倒计时重发

## 12. 相关文档

- 登录场景测试夹具与验收扩展：[frontend-mock-and-acceptance.md](./frontend-mock-and-acceptance.md) §4
- 三端统一错误文案与 CTA：[frontend-login-error-copy.md](./frontend-login-error-copy.md)
