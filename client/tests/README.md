# Gateway API Smoke Tests

本目录提供客户端视角的 gateway 路由烟雾测试，覆盖 `services/gateway/docs/api.md` 中用户与页面路由。

## 脚本

- `gateway_api_smoke.py`

## 使用方法

```bash
BASE_URL="http://127.0.0.1:8080" \
python3 client/tests/gateway_api_smoke.py
```

如需带登录态：

```bash
BASE_URL="https://api.example.com" \
ACCESS_TOKEN="<access_token>" \
REFRESH_TOKEN="<refresh_token>" \
FAVORITE_ID="<favorite_id>" \
python3 client/tests/gateway_api_smoke.py
```

## 说明

- 脚本默认允许常见业务返回（如 `200/400/401/403/404/429`），主要用于发现：
  - 不可达
  - 非 JSON 信封
  - 异常 5xx
- 这是 smoke test，不替代细粒度断言的集成测试。
