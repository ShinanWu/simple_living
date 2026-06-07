# Gateway C 端 E2E 清单

仅覆盖 **C 端**页面与用户路由；运营后台验收见 `services/platform/docs/detail-design.md`。

运行时基址：`GATEWAY_BASE_URL`（本地联调默认 `http://127.0.0.1:8080`；公网入口见 `environments/local-qemu/nodes.env` 的 `FRP_CUSTOM_DOMAIN`）。

| 场景 | 端点 | 期望 |
|------|------|------|
| 健康检查 | `GET /api/v2/health/check` | `success=true` |
| 首页 feed | `GET /api/v2/pages/home_feed?theme=clothing` | 仅 `published` + governance `visible` 卡片 |
| 详情 | `GET /api/v2/pages/guide_detail?guide_card_id=...` | 未发布或不可见返回 404 业务码 |
| 跳转准备 | `POST /api/v2/pages/redirect_prepare` | 返回 `click_id`、`landing_url` |
| 访客会话 | `POST /api/v2/guest/session` | 返回 guest session |
| 收藏列表 | `GET /api/v2/me/favorites/list` | 需 `Authorization` 或 guest header |

微信小程序、iOS、Android 均通过同一契约访问 gateway，不使用 runtime mock。

可执行烟雾脚本：`client/tests/gateway_api_smoke.py`（`BASE_URL` 环境变量）。
