# Backoffice — 微信小程序 C 端数据交付规格

> **职责边界**：`platform/backoffice-backend` 是导购内容、治理可见性、联盟配置的**写面权威**；C 端只经 `gateway` 读 snapshot 与推荐聚合结果，不直连 backoffice。  
> **C 端对外契约**：[`services/gateway/api.md`](../gateway/api.md)（`reason_text` 聚合见 §13.1）。  
> **运营 HTTP**：[`backoffice-gateway-api.md`](./backoffice-gateway-api.md)。**内部 RPC**：[`api.md`](./api.md)。

---

## 1. 交付目标

| C 端接口 | backoffice 须保证的数据前提 |
|----------|----------------------------|
| `GET /api/v2/pages/home_feed` | 四主题各 ≥1 张 **published + visible** 导购卡；卡片含 `selling_points`/`subtitle`、非空 `cover_media.url` |
| `GET /api/v2/pages/guide_detail` | 同上卡片在 catalog snapshot 中字段完整（§5.12） |
| `POST /api/v2/pages/redirect_prepare` | 卡片 `affiliate_refs[].payload.landing_url` 非空 HTTPS（tracking 从 snapshot 解析） |

**双门闸（fail-closed）**：`content_status = published` **且** `visibility.state = published` 才进入 C 端可见集合；见 [`backoffice-gateway-api.md`](./backoffice-gateway-api.md) §5.3。

---

## 2. 主题字典（与 gateway / 小程序一致）

| `theme_id`（卡片 `theme_ids`、DB `theme_id`、推荐过滤） | slug（`home_feed?theme=`） | 展示名 |
|--------------------------------------------------------|----------------------------|--------|
| `theme_1` | `clothing` | 衣 |
| `theme_2` | `food` | 食 |
| `theme_3` | `housing` | 住 |
| `theme_4` | `transport` | 行 |

- 卡片 **`theme_ids`** 必须使用上表 **`theme_id`** 字符串（非 slug）。
- 运营台与 `ListGuideCards.theme_id` 过滤使用同一套 id；写入时 slug（`food` 等）在 store 层规范化为 `theme_2` 等。

---

## 3. 导购卡字段 — C 端必填语义

导出 bundle **`catalog_snapshot`**（`active/catalog.json`）中每张 **published** 卡片须满足：

| 字段 | 要求 | 消费方 |
|------|------|--------|
| `card_id` | 稳定唯一 | gateway / tracking |
| `title` | 非空 | home_feed、guide_detail |
| `subtitle` | 建议非空 | `reason_text` 回退（gateway §13.1） |
| `selling_points[]` | 建议 ≥1 条中文短句 | `reason_text` 回退 |
| `theme_ids[]` | 含且仅含规范 `theme_id` | recommendation 按主题召回 |
| `content_status` | `CONTENT_LIFECYCLE_STATUS_PUBLISHED`（3） | 导出门闸 |
| `cover_media.url` | **绝对 URL**（lab 可 HTTP，生产 HTTPS）；禁止 `data:` | home_feed `cover_url`、guide_detail |
| `cover_media.type` | `MEDIA_TYPE_IMAGE` | gateway 映射 `image` |
| `affiliate_refs[]` | ≥1 条 | redirect |
| `affiliate_refs[].channel` | 如 `tmall` | 渠道识别 |
| `affiliate_refs[].external_item_id` | 外部商品 ID | 联盟 |
| `affiliate_refs[].payload.landing_url` | **非空 HTTPS** 联盟落地页 | tracking `AssembleTrackingLink` |
| `commercial_disclosure_required` | 商业内容 `true` | 披露聚合 |

**`reason_text`** 由 gateway 从推荐 explanation → `selling_points` → `subtitle` 聚合；backoffice **不**单独存 `reason_text`，但须维护回退来源字段。

---

## 4. Snapshot 导出 bundle

| Bundle | 文件 | 内容 | 消费方 |
|--------|------|------|--------|
| `catalog_snapshot` | `active/catalog.json` | `{ "cards": [ GuideCard… ] }`，仅 **published** | recommendation-server（BatchGet）、tracking-server |
| `visibility_index` | `active/visibility.json` | `{ "visible_ids": [ card_id… ] }` | recommendation-server 可见性过滤 |
| `affiliate_link_spec` | `active/affiliate_spec.json` | 伙伴能力快照 | tracking-server |

**挂载**：`-export_dir` 与 `recommendation-server` / `tracking-server` `-snapshot_dir` **同路径**（默认 `/var/lib/simple-living/exports/`）。多节点 lab 需 snapshot fan-out 或同盘。

**触发**：写事务 outbox + 进程启动 `RefreshNow()`；运营发布/可见性变更后须可观测到新 manifest 版本。

---

## 5. Lab 种子数据（`dev_content_seeds`）

首次启动或缺失 seed `card_id` 时，`EnsureSeedGuideCards` 写入 PostgreSQL；对已存在 seed 卡片**仅补齐**空 `cover_media.url` / 空 `landing_url`（不覆盖运营已编辑内容）。

| 主题 | 最少卡片数 | seed `card_id` 前缀 |
|------|------------|---------------------|
| `theme_1` | 3 | `guide_clothing_tmall_*` |
| `theme_2` | 1 | `guide_food_tmall_*` |
| `theme_3` | 1 | `guide_housing_tmall_*` |
| `theme_4` | 1 | `guide_transport_tmall_*` |

启动后 `EnsurePublishedVisibilityFromContent` 为 `status=3` 卡片写入 `visibility=published`（若尚无裁决）。

---

## 6. 运营 SOP（非 seed 环境）

1. **联盟**：`POST .../affiliate/partners/add` 配置渠道伙伴（lab 已有 tmall seed）。
2. **内容**：`content/items/add` → 填写 `theme_ids`、`selling_points`、`landing_url` / `affiliate_refs`。
3. **封面**：`POST .../media/upload` → 将返回的 **完整公网 URL** 写入 `cover_media.url`。
4. **审核**：`submit-review` → governance `reviews/status` 通过 → `content/items/publish`。
5. **可见性**：`publish` 已默认将可见性置为 `published`，正常发布无需额外操作；仅在需要限制/紧急下架时用 `governance/visibility` 覆盖为 `restricted` / `unpublished`。
6. **验收**：检查 `active/catalog.json` 与 `visibility.json`；跑 `client/tests/gateway_live_smoke.py` 四主题 `home_feed`。

---

## 7. 验收清单

```bash
# 1. 导出目录有四主题 published 卡片
jq '.cards[] | select(.content_status=="CONTENT_LIFECYCLE_STATUS_PUBLISHED") | {id:.card_id, themes:.theme_ids, cover:.cover_media.url, landing:.affiliate_refs[0].payload.landing_url}' \
  /var/lib/simple-living/exports/active/catalog.json

# 2. 可见性索引包含上述 card_id
jq '.visible_ids' /var/lib/simple-living/exports/active/visibility.json

# 3. C 端四主题 feed（经 gateway）
BASE=https://shaotang.top
for t in clothing food housing transport; do
  echo "== $t =="
  curl -sS "$BASE/api/v2/pages/home_feed?theme=$t&limit=3" | jq '.data.items | length, .[0].guide_card.cover_url // .[0].guide_card.title'
done

# 4. redirect 非空 landing（示例 card_id 替换为 catalog 中实际值）
curl -sS -X POST "$BASE/api/v2/pages/redirect_prepare" -H 'Content-Type: application/json' \
  -d '{"guide_card_id":"guide_clothing_tmall_919142939015","scene":"home_feed","item_rank":1}' \
  | jq '.data.landing_url, .data.click_id'
```

**通过标准**：

- 四主题 `home_feed` 各 `items.length >= 1`；
- `items[].reason_text` 键存在（可为空字符串；有 `selling_points` 时建议非空）；
- `guide_card.cover_url` 或详情 `cover_media.url` 非空；
- `redirect_prepare` 成功时 `landing_url` 非空。

---

## 8. 关联服务依赖（非 backoffice 实现，但联调阻塞项）

| 服务 | 依赖 backoffice 产出 | 待办（若未满足） |
|------|----------------------|------------------|
| **recommendation-server** | PG `content_guide_card` + snapshot 可见性；按 `theme_id` 过滤候选 | `SyncCandidatesFromContentTable` 已读共享 PG；确认 snapshot fan-out |
| **tracking-server** | catalog 中 `landing_url` | 读 snapshot；无 URL 时 lab 兜底 `go.shaotang.com` |
| **gateway** | 聚合 `reason_text`、theme slug↔id | 见 `services/gateway/api.md` §13 |

---

## 9. 相关文档

| 文档 | 说明 |
|------|------|
| [backoffice-gateway-api.md](./backoffice-gateway-api.md) | 运营 HTTP 与状态机 |
| [api.md](./api.md) | 内部 RPC |
| [detail-design.md](./detail-design.md) | 写链路与导出 |
| [services/gateway/api.md](../gateway/api.md) | C 端对外 JSON 契约 |
