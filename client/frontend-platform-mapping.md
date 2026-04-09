# 三端实现映射（Web / iOS / Android）

## 1. 必须一致项（不可分叉）

- 页面结构：`首页(衣食住行 Tab)`、`导购详情`、`跳转准备`、`我的`
- 登录能力：`微信登录`、`手机号注册/登录`
- 主题值：`clothing`、`food`、`housing`、`transport`
- 状态模型：`loading` / `success` / `empty` / `error` / `offline`
- 错误码处理分级与重试策略（见 `./frontend-gateway-interaction.md`）

## 2. 分端映射（允许差异）

| 交互点 | Web | iOS | Android |
|--------|-----|-----|---------|
| 底部导航 | 固定底栏（首页/我的） | Tab Bar（首页/我的） | Bottom Navigation（首页/我的） |
| 顶部主题切换 | Segment/Tab 容器 | Segmented Control 或顶部 Tab | TabRow/TabLayout |
| 返回行为 | 浏览器返回 + 页内返回按钮 | 导航栈返回 + 左滑返回（系统手势） | 系统返回键 + 页内返回 |
| 登录面板呈现 | 弹层（Modal/Sheet） | 底部弹层或全屏 Sheet | BottomSheet 或全屏页 |
| 微信授权拉起 | JS-SDK/跳转授权页（按平台环境） | WeChat SDK | WeChat SDK |
| 手机号验证码输入 | 数字键盘输入 + 倒计时重发 | NumberPad + 倒计时重发 | NumberKeyboard + 倒计时重发 |

## 3. 三端最小实现清单（登录）

- 接入 `POST /api/v2/guest/session`，保证访客可浏览路径
- 接入 `POST /api/v2/auth/token/issue`，支持 `account_proof.oauth` 与 `account_proof.phone_otp`
- 接入 `POST /api/v2/auth/token/refresh` 与 `DELETE /api/v2/auth/session`
- 实现单飞刷新（single-flight refresh），避免并发重复刷新
- 统一错误码分支：`20002`、`20003`、`20005`、`20006`、`10005`
