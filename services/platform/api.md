# 后端 API（内部 RPC）

`backoffice-backend` 内部 brpc + proto 契约；对外 HTTPS+JSON 经 gateway `/api/v2/backoffice/*`。

- 终端 JSON（经 gateway 暴露）：`services/gateway/api.md`
- 对外 HTTP：[backoffice-gateway-api.md](./backoffice-gateway-api.md)
- 产品语义：[product-spec.md](./product-spec.md)

默认监听 `0.0.0.0:9110`。

## 服务注册

| Service | 模块 | Proto |
|---------|------|-------|
| `ContentService` | content | `common/proto/content_service.proto` |
| `GovernanceReviewService`、`GovernanceVisibilityService` | governance | `backoffice-backend/proto/governance_server.proto` |
| `AffiliatePartnerService` 等 | affiliate | `backoffice-backend/proto/affiliate_server.proto` |

## 核心 RPC

### content

| RPC | 幂等 | 说明 |
|-----|------|------|
| `UpsertGuideCard` | `idempotency_key` | 创建/更新导购卡片 |
| `UpsertTopic` / `UpsertRanking` | 同上 | 专题/榜单 |
| `SubmitForReview` | — | 送审 |
| `PublishRevision` | `idempotency_key` + `expected_version` | 发布；冲突 `10007` |
| `ListGuideCards` / `BatchGetGuideCards` | — | 运营列表/详情 |

### governance

| RPC | 说明 |
|-----|------|
| `EnqueueReview` / `SubmitReviewDecision` | 审核入队/裁决 |
| `ListReviewQueueItems` | 审核队列 |
| `SetVisibilityVerdict` | 可见性门闸 |

### affiliate

| RPC | 说明 |
|-----|------|
| `GetPartnerCapabilitySnapshot` | 伙伴能力快照 |
| `UpsertPartner`（store 层） | partner 元信息持久化 |
| `CreateCommissionRuleSetVersion` | 佣金规则版本 |

运营列表类 HTTP（`ListPartners`）须在 gateway 对接本服务 store，不得以 gateway 内存替代。

## 错误码

| 码 | 含义 |
|----|------|
| `10007` | 乐观锁/版本冲突 |
| `30001`–`30999` | 内容不存在、状态非法 |
| `60001`–`60999` | 治理合规/策略限制 |
| `50001`–`50999` | 联盟渠道/规则 |

## gateway HTTP 映射

| HTTP | RPC |
|------|-----|
| `POST .../content/items` | `ListGuideCards` 等 |
| `POST .../content/items/add` | `UpsertGuideCard` |
| `POST .../content/items/update` | `UpsertGuideCard` |
| `POST .../content/items/detail` | `BatchGetGuideCards` |
| `POST .../content/items/submit-review` | `SubmitForReview` |
| `POST .../content/items/publish` | `PublishRevision` + 可见性协作 |
| `POST .../content/items/status` | 状态变更 |
| `POST .../content/items/rollback` | 新版本写回 |
| `POST .../governance/reviews` | `ListReviewQueueItems` |
| `POST .../governance/reviews/status` | `SubmitReviewDecision` |
| `POST .../governance/visibility` | `SetVisibilityVerdict` |
| `POST .../affiliate/partners` | 伙伴列表 |
| `POST .../affiliate/partners/add` | `UpsertPartner` |

鉴权：gateway 注入运营角色；本服务信任 gateway 身份断言（见 [backoffice-backend/deploy/README.md](./backoffice-backend/deploy/README.md)）。

## 导出控制（内部）

| 操作 | 说明 |
|------|------|
| 写后同步导出 | 内容/治理/联盟写事务成功后，同步调用 `RefreshNow()` 重建全量 snapshot（staging→active 原子切换） |
| 进程启动导出 | 服务启动时执行一次 `RefreshNow()`，保证 active 与当前 DB 一致 |
| `GetExportManifest` | 查询 active 版本与各 bundle sha256 |

导出与写链路详见 [detail-design.md](./detail-design.md) §4。

### C 端 snapshot 字段（交付摘要）

完整验收与主题字典见 [backoffice-delivery-spec.md](./backoffice-delivery-spec.md)。

| Bundle | 关键字段 |
|--------|----------|
| `catalog_snapshot` | `GuideCard`：`theme_ids`、`selling_points`、`cover_media.url`、`affiliate_refs[].payload.landing_url`、`content_status=published` |
| `visibility_index` | `visible_ids[]`，与 governance `published` 一致 |
| `affiliate_link_spec` | 伙伴 `partner_id` / `channel_code` |

`recommendation-server` 另从共享 PG `content_guide_card`（`status=3`）同步候选池；snapshot 与 PG 须同源（同一 backoffice 写面）。
