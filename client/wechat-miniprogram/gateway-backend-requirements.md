# 微信小程序 → Gateway 接口需求（交后端 Agent）

> **前端立场**：小程序只依赖 `services/gateway/api.md` 描述的 **v2 REST 契约**。  
> **当前联调基址**：`http://8.152.103.12`（与 `config/env.ts` 一致）。  
> **验收脚本**：`client/tests/gateway_live_smoke.py`（公网）；小程序单测 `npm test`。

前端实现位置：`services/gateway/http.ts`。待下列 P0 在 gateway **按 api.md 落地并重新部署** 后，前端会切回文档路由（去掉对 legacy POST 路径的临时兼容）。

---

## P0 — 阻塞小程序完整主流程

### 1. 用户域路由与 `api.md` 对齐

**现状（公网 2026-06）**：`gateway_edge_server_main.cpp` 仍注册 legacy 路径（如 `POST /api/v2/me/favorites/list`），与 `api.md` §7 不一致；小程序按文档发 `GET`/`DELETE` 会收到 `405` / `404`。

**需求**：gateway HTTP 映射与 `api.md` §7 **完全一致**（方法 + 路径），至少包含小程序已接入的：

| 方法 | 路径 | 鉴权 | 小程序用途 |
|------|------|------|------------|
| `POST` | `/api/v2/guest/session` | 无 | 冷启动访客 |
| `POST` | `/api/v2/auth/token/issue` | 无 | 微信 / 手机号登录 |
| `POST` | `/api/v2/auth/token/refresh` | 无 | `20002` 单飞刷新 |
| `DELETE` | `/api/v2/auth/session` | Bearer | 退出登录 |
| `GET` | `/api/v2/me/favorites` | Bearer | 收藏列表 |
| `POST` | `/api/v2/me/favorites` | Bearer | 添加收藏 |
| `DELETE` | `/api/v2/me/favorites/{favorite_id}` | Bearer | 取消收藏 |
| `GET` | `/api/v2/me/history` | Bearer 或 `X-Guest-Session-Id` | 历史列表 |
| `POST` | `/api/v2/me/history/events` | Bearer 或访客 | 详情页写历史 |
| `DELETE` | `/api/v2/me/history` | Bearer 或访客 | 清空历史 |

**验收**：

```bash
# 访客历史（GET 必须可用）
curl -sS -X POST "$BASE/api/v2/guest/session" -H 'Content-Type: application/json' \
  -d '{"device_id":"accept","client_platform":"wechat_miniprogram"}' \
| jq -r '.data.session_id' | read SID
curl -sS "$BASE/api/v2/me/history?limit=5" -H "X-Guest-Session-Id: $SID"
# 期望：HTTP 200，标准信封，非 405
```

Legacy 路径可在 gateway 内做短期双写转发，但 **对外文档与客户端只认 api.md 路径**。

---

### 2. `GET /api/v2/pages/me_summary` — `counts` 类型

**现状**：`favorites_count` / `history_count` 返回 **字符串** `"0"`。

**需求**：按 `api.md`，两字段为 **integer**。

**示例**：

```json
"counts": { "favorites_count": 0, "history_count": 3 }
```

**小程序消费**：`pages/me/me` 顶栏统计数字。

---

### 3. `GET /api/v2/me/history` — `content_ref` 字段名

**现状**：列表项为 `content_ref.content_id`，无 `guide_card_id`。

**需求**：对外 JSON 统一为 `api.md` 语义，至少支持：

```json
{
  "items": [{
    "content_ref": { "type": "guide_card", "guide_card_id": "guide_clothing_tmall_919142939015" },
    "last_seen_at": "2026-06-07T10:00:00Z"
  }],
  "pagination": { "next_cursor": null, "has_more": false, "limit": 20 }
}
```

`last_seen_at` 须为合法 ISO8601 UTC（勿出现 `1970-01-01` 类占位时间戳）。

**小程序消费**：`pages/history/history` 列表与跳转详情。

---

### 4. `POST /api/v2/pages/redirect_prepare` — 非空跳转结果

**现状**：HTTP 200，但 `data.landing_url`、`data.click_id` 均为空字符串（样例：`guide_clothing_tmall_919142939015`）。

**需求**：

| 字段 | 要求 |
|------|------|
| `landing_url` | 必填，HTTPS，可直接由 `web-view` 或复制打开 |
| `click_id` | 必填，单次点击唯一 ID |

**请求体**（小程序已发送）：

```json
{
  "guide_card_id": "guide_clothing_tmall_919142939015",
  "recommendation_id": "rec_cms_…",
  "scene": "home_feed",
  "item_rank": 1
}
```

**失败语义**：无可用联盟链接时返回业务错误（如 `50002` / `50003`），**不要** `success=true` 且空 URL。

**小程序消费**：`pages/redirect-prepare/redirect-prepare`、`pages/web-outbound/web-outbound`。

**依赖说明**：若需 backoffice 在导购卡片 / affiliate 导出中写入 `landing_url`，请一并排期；gateway 只负责按契约返回。

---

### 5. `GET /api/v2/pages/guide_detail` — 封面与图集

**现状**：`guide_card.cover_url` 常为空；无 `gallery_urls` 或 `cover_media`。

**需求**：对已发布且可见的导购卡，至少返回以下之一（与 `api.md` §5.12 `cover_media` 对齐）：

```json
"guide_card": {
  "guide_card_id": "…",
  "title": "…",
  "summary": "…",
  "cover_url": "https://…",
  "cover_media": { "type": "image", "url": "https://…", "width": 800, "height": 800 },
  "gallery_urls": ["https://…"],
  "is_commercial": true,
  "commercial_disclosure": { "is_commercial": true, "disclosure_text_key": "…" }
}
```

**小程序消费**：首页卡片封面、`pages/guide-detail` 图集。

---

### 6. `POST /api/v2/auth/token/issue` — 微信小程序登录

**小程序发送**（`client_platform` 固定 `wechat_miniprogram`）：

```json
{
  "account_proof": {
    "oauth": {
      "provider": "wechat",
      "provider_subject": "<openid 或首次可空由服务端用 code 换>",
      "authorization_code": "<wx.login() code>"
    }
  },
  "client_platform": "wechat_miniprogram",
  "device_id": "<持久化 device_id>",
  "app_version": "1.0.0"
}
```

**需求**：

- 真机 `wx.login` 的 `code` 可换发 token 对（`access_token`、`refresh_token`、`expires_in`）。
- 联调环境提供 **手机号 OTP 固定 `verification_id` + 验证码** 文档或测试接口说明（无独立「发验证码」路由时，须在 `services/gateway/api.md` 或运维文档写明测试凭据）。

**小程序消费**：`pages/login/login`、收藏等需登录能力。

---

## P1 — 不阻塞首屏，但影响四主题验收

### 7. `GET /api/v2/pages/home_feed` — 四主题均有可推荐内容

**现状**：`theme=clothing` 有数据；`food` / `housing` / `transport` 返回空 `items`（接口可达）。

**需求**：每个主题至少 1 条 `published` + governance `visible` 的导购卡（运营/backoffice 侧发布；gateway 只需正确过滤返回）。

**Query**：`theme=clothing|food|housing|transport`，`limit`，`cursor`（游标分页）。

**小程序消费**：首页四 Tab 切换、上滑翻页与 `next_cursor` 加载更多。

---

### 8. `GET /api/v2/health`

**现状**：`GET /api/v2/health` → 404；`/api/v2/health/check` 方法行为与文档不一致。

**需求**：实现 `api.md` §9.19，`GET /api/v2/health`，标准信封 + `data.status`。

**用途**：运维与 `gateway_live_smoke` 探活（非小程序 UI 必需）。

---

## 已满足（无需改动，作回归基线）

| 接口 | 状态 |
|------|------|
| `POST /api/v2/guest/session` | ✅ |
| `GET /api/v2/pages/home_feed?theme=clothing` | ✅ |
| `GET /api/v2/pages/guide_detail` | ✅（缺封面见 P0-5） |
| `GET /api/v2/pages/me_summary` | ✅（counts 类型见 P0-2） |
| `POST /api/v2/me/history/events` | ✅ |
| `POST /api/v2/auth/token/refresh` | 未在本次公网实测，按文档保持 |

---

## 前端暂不依赖

- `GET /api/v2/me/profile` / `preferences` / `consent` 编辑
- 搜索页与 `scene=search`
- 独立「发送手机验证码」HTTP 路由

---

## 变更同步要求（给后端 Agent）

按仓库 **docs-first** 规则，gateway 行为变更须：

1. 更新 `services/gateway/api.md`（若对外语义变）
2. 重新部署公网 frp 入口后通知前端，跑：

```bash
BASE_URL=http://8.152.103.12 python3 client/tests/gateway_live_smoke.py
```

前端收到 P0 完成信号后，将 `http.ts` 切回纯 `api.md` 路由并移除 legacy 兼容。
