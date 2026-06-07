# 后端工作流

`backoffice-backend` 进程内写链路与导出发布。端到端页面/API 见 [detail-design.md](./detail-design.md)。

## 运营写主链路

### 内容：创作 → 审核 → 发布

```text
backoffice-web → gateway(/api/v2/backoffice/content/*)
  → content 模块（Upsert）
  → governance（SubmitReview / 审核态）
  → 可见性裁决（Approve 不自动 Publish）
  → content（Publish，服从可见性）
  → 事务 + outbox
  → 导出 catalog_snapshot + visibility_index
  → recommendation-server 热加载
```

幂等：写接口携带 `idempotency_key`；乐观锁冲突返回 `10007`。

### 紧急下架 / 可见性限制

```text
gateway(/api/v2/backoffice/governance/visibility)
  → governance SetVisibilityVerdict
  → 优先 patch visibility_index（目标 ≤ 1s）
```

读路径 fail-closed：snapshot 不可见则 recommendation-server 不得返回。

### 联盟

```text
gateway(/api/v2/backoffice/affiliate/*)
  → affiliate UpsertPartner / CommissionRule
  → 导出 affiliate_link_spec（脱敏）→ tracking-server
```

## 导出发布

| Bundle | 触发 |
|--------|------|
| `catalog_snapshot` | 内容发布、专题/榜单变更 |
| `visibility_index` | 审核/可见性/下架 |
| `affiliate_link_spec` | partner/规则变更 |

步骤：PG 事务 → outbox → staging/ → 校验 → active/ → 读服务热切换。

| 场景 | 行为 |
|------|------|
| 导出失败 | 保持上一版 active；写 RPC 仍成功；告警 `export_lag_seconds` |
| 读服务无 manifest | readiness 失败，不 serve 空 catalog |
| 下架优先 | 可仅 patch visibility_index |

## 领域事件（Kafka，at-least-once）

| Topic | Key |
|-------|-----|
| `platform.content.published` | `content_id` |
| `platform.governance.visibility_changed` | `content_id` |
| `platform.affiliate.spec_changed` | `partner_id` |

读服务不依赖 Kafka 做在线读。
