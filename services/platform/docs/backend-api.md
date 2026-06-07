# 后端 API（内部 RPC）

`backoffice-backend` 内部 brpc + proto 契约；对外 HTTPS+JSON 经 gateway `/api/v2/backoffice/*`。

- 公共 JSON：`.cursor/rules/shared-contracts.mdc`
- 对外 HTTP：[`gateway/docs/backoffice-backend.md`](../../gateway/docs/backoffice-backend.md)
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

鉴权：gateway 注入运营角色；本服务信任 gateway 身份断言（见 [backend-development.md](./backend-development.md)）。

## 导出控制（内部）

| 操作 | 说明 |
|------|------|
| outbox 自动触发 | 写事务后异步导出 |
| `GetExportManifest` | 查询 active 版本与各 bundle sha256 |

详见 [backend-workflow.md](./backend-workflow.md)、[backend-data-model.md](./backend-data-model.md)。
