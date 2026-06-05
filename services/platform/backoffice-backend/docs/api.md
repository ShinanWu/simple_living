# backoffice-backend — API

内部 brpc + proto 为本服务权威契约；对外 HTTPS+JSON 经 `gateway` `/api/v2/backoffice/*` 映射，字段语义见 `.cursor/rules/shared-contracts.mdc` 与 `services/gateway/docs/backoffice-backend.md`。

默认监听 `0.0.0.0:9110`（`platform/backoffice-backend`）。

## 1. 服务注册（proto，按模块）

| Service | 模块 | 说明 |
|---------|------|------|
| `ContentService` | content | 卡片/专题/榜单/主题 CMS 写与运营读（`common/proto/content_service.proto`） |
| `GovernanceReviewService` 等 | governance | 审核队列、裁决、可见性（`proto/governance_server.proto`） |
| `AffiliatePartnerService` 等 | affiliate | partner、佣金规则、能力矩阵（`proto/affiliate_server.proto`） |

> 导购数据模型与 `ContentService` RPC 定义于 [`common/proto/`](../../../common/proto/)（`catalog.proto`、`content_service.proto`）；governance/affiliate 写面 proto 仍在本服务 `proto/`。各消费方通过 `//common/proto:*_cc_proto` 依赖，禁止拷贝/fork。

## 2. 核心 RPC（写面）

### 2.1 content 模块

| RPC | 幂等 | 说明 |
|-----|------|------|
| `UpsertGuideCard` | `idempotency_key` | 创建/更新导购卡片 |
| `UpsertTopic` / `UpsertRanking` | 同上 | 专题/榜单 |
| `PublishContent` | `idempotency_key` + `expected_version` | 发布；冲突 `10007` |
| `ListContentItems` | — | 运营列表（低频） |
| `GetContentDetail` | — | 含版本历史 |

### 2.2 governance 模块

| RPC | 说明 |
|-----|------|
| `SubmitReview` | 内容送审 |
| `UpdateReviewStatus` | 通过/拒绝/补充材料 |
| `SetVisibility` | 可见性最终门闸（`published`/`restricted`/`unpublished`） |
| `ListReviews` | 审核队列 |

### 2.3 affiliate 模块

| RPC | 说明 |
|-----|------|
| `UpsertPartner` | partner 元信息 |
| `UpsertCommissionRuleSet` | 佣金规则版本 |
| `ListPartners` | 运营列表 |
| `ValidateLinkGenerationInput` | 供 tracking 转链前校验（v1 可由 snapshot 替代在线 RPC） |

## 3. 错误码（本服务区间）

与共享契约对齐；模块沿用原域区间合并：

| 码 | 模块 | 含义 |
|----|------|------|
| `10007` | content/governance | 乐观锁/版本冲突 |
| `30001`–`30999` | content | 内容不存在、状态非法等 |
| `60001`–`60999` | governance | 合规拦截、策略限制 |
| `50001`–`50999` | affiliate | 渠道不可用、规则缺失 |

完整列表在实现前从原三域 `api.md` 合并迁入本文件 changelog。

## 4. gateway Backoffice HTTP 映射

| HTTP（gateway） | 本服务 RPC |
|-----------------|------------|
| `POST /api/v2/backoffice/content/items` | `ContentBackofficeService/ListContentItems` |
| `POST /api/v2/backoffice/content/items/add` | `UpsertGuideCard` |
| `POST /api/v2/backoffice/content/items/publish` | `PublishContent` |
| `POST /api/v2/backoffice/governance/reviews` | `ListReviews` |
| `POST /api/v2/backoffice/governance/reviews/status` | `UpdateReviewStatus` |
| `POST /api/v2/backoffice/governance/visibility` | `SetVisibility` |
| `POST /api/v2/backoffice/affiliate/partners` | `ListPartners` |
| `POST /api/v2/backoffice/affiliate/partners/add` | `UpsertPartner` |

鉴权：gateway 注入运营角色（`backoffice_admin` 等）；本服务信任 gateway 已校验的身份断言（见 development.md 安全节）。

## 5. 导出控制（内部，非 brpc）

| 操作 | 接口形态 | 说明 |
|------|----------|------|
| 触发全量导出 | 管理 RPC `TriggerExport` 或 outbox 自动 | 运维/发布流水线 |
| 查询导出版本 | `GetExportManifest` | 返回 active `version` 与各 bundle sha256 |

读服务**不**调用上述 RPC 获取业务数据，只读文件系统 manifest + mmap。

## 6. 示例（UpsertGuideCard 请求骨架）

```json
{
  "idempotency_key": "bo-card-20250605-001",
  "guide_card": {
    "guide_card_id": "gc_demo_001",
    "title": "示例卡片",
    "theme": "food",
    "status": "draft"
  },
  "expected_version": 0
}
```

成功：`code=0`；发布后再由导出 worker 刷新 snapshot，C 端经 `recommendation-server` 可见。
