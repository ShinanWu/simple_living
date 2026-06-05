# 前端与 Gateway 交互设计

## 1. 交互边界

- 前端只调用 `gateway` 对外 HTTP/JSON 接口
- 前端不依赖内部服务 `proto`
- 字段语义遵循 `.cursor/rules/shared-contracts.mdc` 与 `services/gateway/docs/api.md`

## 2. 页面到接口映射

| 页面 | 主接口 |
|------|--------|
| 登录 Login | `POST /api/v2/guest/session`、`POST /api/v2/auth/token/issue`（`phone_otp` / `oauth.wechat`）、`POST /api/v2/auth/token/refresh`、`DELETE /api/v2/auth/session` |
| 首页 Home（四主题 Tab） | `GET /api/v2/pages/home_feed` |
| 导购详情 GuideDetail | `GET /api/v2/pages/guide_detail` |
| 跳转准备 RedirectPrepare | `POST /api/v2/pages/redirect_prepare` |
| 我的 Me | `GET /api/v2/pages/me_summary` |

## 3. 请求与状态规则

- JSON 字段统一 `snake_case`
- 分页统一 `cursor` + `limit`，响应统一 `data.pagination`
- 状态模型统一：`loading` / `success` / `empty` / `error` / `offline`
- 登录态统一：无 token 时先申请 `POST /api/v2/guest/session`，受保护操作再触发登录并换发 token
- 错误分支统一以 `code` 为准；登录相关重点关注 `20002`、`20003`、`20005`、`20006`
