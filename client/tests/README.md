# Gateway API Smoke Tests（C 端）

本目录提供 **C 端**视角的 gateway 路由烟雾测试，覆盖 `services/gateway/api.md` 中用户与页面路由。运营后台验收见 `services/platform/detail-design.md`。

## 脚本与清单

| 资源 | 用途 |
|------|------|
| `gateway_api_smoke.py` | 可执行烟雾脚本 |
| `gateway-e2e-checklist.md` | C 端手工 E2E 核对表 |
| `../frontend-mock-and-acceptance.md` | UI 五态 × 错误码验收（非 HTTP 清单） |

微信小程序逻辑单测：`client/wechat-miniprogram/scripts/run-tests.ts`（`npm test`）。

## 使用方法

```bash
BASE_URL="http://127.0.0.1:8080" \
python3 client/tests/gateway_api_smoke.py
```

公网入口基址见 `environments/local-qemu/nodes.env` 的 `FRP_CUSTOM_DOMAIN`，勿在 client 文档硬编码 IP。

如需带登录态：

```bash
BASE_URL="https://api.example.com" \
ACCESS_TOKEN="<access_token>" \
REFRESH_TOKEN="<refresh_token>" \
FAVORITE_ID="<favorite_id>" \
python3 client/tests/gateway_api_smoke.py
```

## 说明

- 脚本默认允许常见业务返回（如 `200/400/401/403/404/429`），主要用于发现不可达、非 JSON 信封、异常 5xx。
- 这是 smoke test，不替代细粒度断言的集成测试。
