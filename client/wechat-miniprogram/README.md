# 微信小程序（少糖 C 端）

## 1. 交付定位

- **当前主发布端**：原生微信小程序（WXML + TypeScript）。
- **原生 App 路线**：iOS / Android 与小程序共享 gateway 契约，按商业化计划分阶段补齐真实网络层与端能力。
- 业务语义与 [frontend-page-specs.md](../frontend-page-specs.md)、[frontend-login-interaction.md](../frontend-login-interaction.md) 一致；`client_platform` 固定为 `wechat_miniprogram`。

## 2. 页面与路由

| 页面 | 路径 | 说明 |
|------|------|------|
| 首页 | `pages/home/home` | 四主题 Tab、单卡推荐、手势切换 |
| 导购详情 | `pages/guide-detail/guide-detail` | 图集、摘要、收藏、去购买 |
| 跳转准备 | `pages/redirect-prepare/redirect-prepare` | `redirect_prepare`、复制/外链打开 |
| 我的 | `pages/me/me` | `me_summary`、登录/退出、入口 |
| 登录 | `pages/login/login` | 微信一键登录、手机号 OTP |
| 收藏 | `pages/favorites/favorites` | 列表（需登录） |
| 历史 | `pages/history/history` | 列表（访客/登录） |
| 外链承载 | `pages/web-outbound/web-outbound` | `web-view` 打开 `landing_url`（需配置业务域名） |

## 3. Gateway 与运行模式

- 仅调用 `gateway` HTTPS JSON；字段 `snake_case`。
- **默认真实链路**：启动时写入 `gateway_base_url`（默认见 `config/env.ts`，与 `environments/local-qemu/nodes.example.env` 的 `FRP_CUSTOM_DOMAIN` 对齐），即公网 **frp → nginx → gateway**。
- **本机 QEMU 转发**：仅调试时可覆盖为 `http://127.0.0.1:8080`（`nginx` 来宾上的 `gateway`）。
- **覆盖地址**：`wx.setStorageSync('gateway_base_url', 'http://你的公网入口')`。现阶段验收不得使用 Mock 或 Mac 本机替身。

鉴权与存储：

- 访客：`POST /api/v2/guest/session` → `X-Guest-Session-Id`
- 登录：`Authorization: Bearer` + 本地持久化 token 对
- `20002` 单飞 refresh；`20003` 清空并回访客

## 4. 微信登录说明

生产路径：

1. `wx.login()` 取得一次性 `code`
2. `POST /api/v2/auth/token/issue`，`account_proof.oauth`：`provider=wechat`，`authorization_code=code`
3. `provider_subject`：优先使用本地已缓存的 `openid`；首次登录可由 **user-server / 网关** 根据 `code` 解析（客户端在联调模式可手填，见登录页「高级联调」）

手机号 OTP：网关 v1 未暴露独立「发验证码」路由；联调使用测试环境 `verification_id` / 固定验证码。

## 5. 本地开发

1. 安装 [微信开发者工具](https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html)
2. 打开目录：`client/wechat-miniprogram`
3. AppID：测试号或自有小程序 AppID（`project.config.json` 中 `appid`）
4. 编译：工具内勾选「使用 TypeScript」；或使用 `npm run typecheck`

若导航栏仍显示旧名「简单生活」：菜单 **工具 → 清除缓存 → 清除全部** 后点 **编译**；代码已在 `app.onLaunch` 中调用 `wx.setNavigationBarTitle('少糖')` 覆盖。真机顶部若仍不对，需在 [微信公众平台](https://mp.weixin.qq.com/) 修改小程序**正式名称**（与 `navigationBarTitleText` 无关）。

### 5.1 自测（无需开发者工具）

```bash
cd client/wechat-miniprogram
npm install
npm test
```

覆盖：错误文案映射、手机号 E.164、主题枚举、HTTP 信封解析逻辑，以及离线测试专用的内存 Gateway。

### 5.2 联调真实 Gateway

```bash
cp environments/local-qemu/nodes.example.env environments/local-qemu/nodes.env
# 按各服务 deploy/README 与 services/README.md 部署各 *-server、backoffice-backend、gateway、proxy
bash services/proxy/deploy/verify_proxy.sh
```

开发者工具：**详情 → 本地设置 → 不校验合法域名**。公网入口见 `nodes.example.env`；切换环境时覆盖本地 `gateway_base_url` 存储。

发布/再发一条导购按各服务 `deploy/README` 与 [services/README.md](../../services/README.md) 部署。

## 6. 相关文档

- 品牌命名：[项目说明（品牌与命名）](../../README.md)
- 分端交互：[interaction-notes.md](./interaction-notes.md)
- 三端映射：[frontend-platform-mapping.md](../frontend-platform-mapping.md)
- Gateway API：[services/gateway/docs/api.md](../../services/gateway/docs/api.md)
