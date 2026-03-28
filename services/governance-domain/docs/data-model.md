# governance-domain — 数据模型

> 逻辑模型，用于并行开发对齐；物理表名、索引与分库分表实现时再定。

## 1. 标识与通用字段

建议所有可审计实体包含：

- `id`（ULID/雪花）
- `created_at`, `updated_at`
- `created_by`, `updated_by`（操作者主体 ID）
- `audit_trace_id`（可选，关联网关 trace）

---

## 2. 核心实体

### 2.1 ReviewQueueItem（审核队列项）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 主键 |
| `content_id` | string | 引用 content-domain |
| `content_version` | string/int | 与内容版本对齐，用于幂等 |
| `status` | enum | `pending`, `in_review`, `completed`, `cancelled` |
| `priority` | int | 数字越大越优先（约定实现时固定） |
| `enqueue_reason` | string | 如新稿、举报、策略命中 |
| `channel_hints` | string[] | 可选，影响审核 SLA 或分配 |

### 2.2 ReviewDecision（审核结论）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 主键 |
| `queue_item_id` | string | 关联队列项 |
| `content_id` | string | 冗余，便于查询 |
| `outcome` | enum | `approved`, `rejected`, `needs_info` |
| `comment` | text | 审核意见 |
| `reviewer_id` | string | 审核员 |
| `policy_evaluation_id` | string | 可选，关联策略评估快照 |
| `decided_at` | timestamp | 结论时间 |

### 2.3 VisibilityVerdict（可见性裁决）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 主键 |
| `content_id` | string | 唯一约束（当前生效一行）或版本表 |
| `state` | enum | `published`, `unpublished`, `restricted`, … |
| `reason_code` | string | 标准原因码，供分析与客户端提示 |
| `source` | enum | `review`, `manual_ops`, `policy`, `system` |
| `source_ref_id` | string | 如审核单 ID、策略版本 ID |
| `effective_from` | timestamp | 生效时间 |
| `version` | int | 单调递增，便于缓存失效 |

**合并规则**（与 content-domain 的关系）在 `workflow.md` 定义；模型层只存本域裁决。

### 2.4 CooperationLabel（商业合作标签）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | 主键 |
| `subject_type` | enum | `content`, `card`, `topic`, … |
| `subject_id` | string | |
| `cooperation_type` | string | 业务字典，如 `affiliate`, `brand_sponsored` |
| `disclosure_template_id` | string | 关联披露模板 |
| `effective_from`, `effective_to` | timestamp | 可选，支持活动期 |
| `metadata` | json | 扩展：合同编号内部字段等（注意脱敏） |

### 2.5 DisclosureTemplate（披露模板）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | |
| `locale` | string | 如 `zh-CN` |
| `template_key` | string | C 端取文案的 key |
| `body` | text | 支持占位符，实现时定义 |
| `version` | int | |

### 2.6 OpsConfigRelease（运营配置发布）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | |
| `config_key` | string | 如 `governance.feature_flags` |
| `version` | int | 对单个 `config_key` 单调递增，避免全局版本号放大无关变更影响 |
| `payload` | json | 校验后的 JSON |
| `released_at` | timestamp | |
| `released_by` | string | |

草稿表可单独 `ops_config_draft`，或复用 GitOps；文档层保留 **发布版本** 概念即可。

### 2.7 RiskFlag（风险标记）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | |
| `subject_type` | enum | `content`, `account`, `channel`, … |
| `subject_id` | string | |
| `flag_code` | string | 字典：`misleading`, `complaint_spike`, … |
| `severity` | enum | `low`, `medium`, `high`, `critical` |
| `source` | string | `manual`, `external_risk_engine`, … |
| `expires_at` | timestamp | 可空表示长期 |
| `active` | bool | 软删除或解除 |

### 2.8 PolicyVersion（策略版本，可选独立存储）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | |
| `version` | string | 语义化或日历版本 |
| `ruleset` | json / ref | 规则 DSL 或远程引用 |
| `status` | enum | `draft`, `active`, `deprecated` |
| `effective_from` | timestamp | |

v1 默认将 `PolicyVersion` 与 `OpsConfigRelease` 分开维护：前者承载可审计的治理规则版本，后者承载运营配置与开关。

### 2.9 PolicyEvaluationSnapshot（策略评估快照，可选）

用于审核意见与追责：评估输入摘要 + 输出 `violations[]` 的不可变记录。

---

## 3. 关系示意（文本 ER）

```text
Content (external ref: content_id)
    ├── 1:N  ReviewQueueItem
    ├── 1:N  ReviewDecision
    ├── 1:1  VisibilityVerdict (current) 或 1:N 历史版本表
    ├── 1:N  CooperationLabel (按 subject 也可能挂 card/topic)
    └── N:M  RiskFlag (via subject)

DisclosureTemplate
    └── 1:N  CooperationLabel (logical)

OpsConfigRelease —— standalone by config_key + version

PolicyVersion —— optional; links to PolicyEvaluationSnapshot
```

---

## 4. 索引与查询热点（实现提示）

- `ReviewQueueItem(status, priority DESC, enqueued_at)`：待审列表。
- `ReviewDecision(content_id, decided_at DESC)`：历史。
- `VisibilityVerdict(content_id)` 或 `(content_id, version DESC)`：当前态。
- `CooperationLabel(subject_type, subject_id)`：批量查询。
- `RiskFlag(subject_type, subject_id, active)`：策略与详情页。

---

## 5. 与契约文档的对应关系

- 导购卡片上的披露字段：在 `docs/contracts/guide-card.md` 标注 **来源 = governance-domain**。
- 推荐过滤：在 `docs/contracts/recommendation.md`（若存在）注明 **可见性以 governance 裁决为准**。
