# 多端实现映射（iOS / Android / 微信小程序）

> **Web C 端**：无开发计划；下表 Web 列仅作预留对照，与运营后台 `backoffice-web` 无关。

## 0. 商业化交付说明

- **主发布端**：微信小程序（`client/wechat-miniprogram/`），必须接真实公网 gateway。
- **原生端**：iOS / Android 保留同一页面与契约模型，按商业化计划分阶段补齐真实网络层、登录、收藏、历史与跳转。

## 1. 必须一致项（不可分叉）

- 页面结构：`首页(衣食住行 Tab)`、`导购详情`、`跳转准备`、`我的`
- 登录能力：`微信登录`、`手机号注册/登录`
- 主题值：`clothing`、`food`、`housing`、`transport`（见 `.cursor/rules/shared-contracts.mdc`）
- 状态模型：`loading` / `success` / `empty` / `error` / `offline`
- 错误码处理分级与重试策略（见 `./frontend-gateway-interaction.md`）
- 网关 `client_platform`：小程序固定 `wechat_miniprogram`

## 2. 分端映射（允许差异）

| 交互点 | Web | iOS | Android | 微信小程序 |
|--------|-----|-----|---------|------------|
| 顶栏 | 搜索框 + 我的图标 | 搜索框 + 我的图标 | 搜索框 + 我的图标 | 搜索框 + 我的图标 |
| 底部主题切换 | Segment/Tab 容器 | 底部胶囊栏 | 底部胶囊栏 | 底部固定四主题胶囊栏 |
| 返回行为 | 浏览器返回 + 页内返回按钮 | 导航栈返回 + 左滑返回（系统手势） | 系统返回键 + 页内返回 | `navigateBack` + 导航栏返回 |
| 登录面板呈现 | 弹层（Modal/Sheet） | 底部弹层或全屏 Sheet | BottomSheet 或全屏页 | 独立登录页 |
| 微信授权拉起 | JS-SDK/跳转授权页（按平台环境） | WeChat SDK | WeChat SDK | `wx.login` + `issue_token` |
| 手机号验证码输入 | 数字键盘输入 + 倒计时重发 | NumberPad + 倒计时重发 | NumberKeyboard + 倒计时重发 | `input type=number` + 60s 倒计时 |
| 外链跳转 | `window.open` / 同页跳转 | `SFSafariViewController` / Universal Link | Custom Tabs / Intent | `web-view` 页 + 复制链接兜底 |

## 3. 各端正式登录清单

- 接入 `POST /api/v2/guest/session`，保证访客可浏览路径
- 接入 `POST /api/v2/auth/token/issue`，支持 `account_proof.oauth` 与 `account_proof.phone_otp`
- 接入 `POST /api/v2/auth/token/refresh` 与 `DELETE /api/v2/auth/session`
- 实现单飞刷新（single-flight refresh），避免并发重复刷新
- 统一错误码分支：`20002`、`20003`、`20005`、`20006`、`10005`

## 4. 微信小程序补充

- 详见 [wechat-miniprogram/interaction-notes.md](./wechat-miniprogram/interaction-notes.md)
- 收藏 / 历史列表为 v1 已实现扩展页，语义与 gateway `me/*` 路由一致
