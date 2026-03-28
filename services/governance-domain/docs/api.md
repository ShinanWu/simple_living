# governance-domain — 内部 RPC 契约（proto2）

> 本文档为 `governance-domain` **内部** brpc/gRPC 风格 RPC 的单一事实来源：含完整 RPC 列表、请求/响应消息、嵌套类型与枚举。客户端对外 JSON 由 `gateway` 定义；本域不将本 `proto` 形状直接暴露给 C 端。

---

## 1. 通用约定

### 1.1 鉴权与幂等

- **鉴权**：运营/审核/裁决类 RPC 需运营角色；机机调用建议 mTLS 或服务账号。
- **幂等**：写 RPC 须携带 `client_request_id`（或等价 `idempotency_key`）；重复提交返回同一业务结果。
- **审计**：关键写请求可携带 `audit_trace_id`，与网关 `trace_id` 对齐。

### 1.2 分页

列表类 RPC 使用 **游标分页**，语义对齐 `docs/contracts/pagination.md`：

| 位置 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 请求 | `cursor` | string | 可选；上一页 `pagination.next_cursor` |
| 请求 | `limit` | int32 | 可选；默认与上限 **100** |
| 响应 | `pagination.next_cursor` | string \| null | 无更多时为 `null` |
| 响应 | `pagination.has_more` | bool | |
| 响应 | `pagination.limit` | int32 | 回显 |

### 1.3 v1 决策（与 `workflow.md` 一致）

- **可见性为最终门闸**：对用户是否展示以 **`VisibilityVerdict.state == PUBLISHED`** 为准；推荐、gateway、tracking 均不得在非 `published` 下暴露内容。
- **审核通过不自动发布**：`SubmitReviewDecision` 的 `APPROVED` **仅记录审核结论**；是否对用户可见依赖 **`SetVisibilityVerdict`**（或批量）等显式裁决及内容域发布流程，**不得**因审核通过隐式等价于上线。

### 1.4 时间类型

文中 **timestamp** 在 `proto` 中为 `google.protobuf.Timestamp`。

---

## 2. 枚举定义

### 2.1 `ReviewQueueItemStatus`

| 名称 | 说明 |
|------|------|
| `REVIEW_QUEUE_ITEM_STATUS_PENDING` | 待处理 |
| `REVIEW_QUEUE_ITEM_STATUS_IN_REVIEW` | 审核中 |
| `REVIEW_QUEUE_ITEM_STATUS_COMPLETED` | 已完成 |
| `REVIEW_QUEUE_ITEM_STATUS_CANCELLED` | 已取消 |

### 2.2 `ReviewOutcome`

| 名称 | 说明 |
|------|------|
| `REVIEW_OUTCOME_APPROVED` | 通过（**不自动发布**，见 §1.3） |
| `REVIEW_OUTCOME_REJECTED` | 驳回 |
| `REVIEW_OUTCOME_NEEDS_INFO` | 需补充材料 |

### 2.3 `VisibilityState`

| 名称 | 说明 |
|------|------|
| `VISIBILITY_STATE_PUBLISHED` | 可对用户展示（仍受内容域生效时间等约束） |
| `VISIBILITY_STATE_UNPUBLISHED` | 下架/不可展示 |
| `VISIBILITY_STATE_RESTRICTED` | 受限展示（实现可定义细分策略） |

### 2.4 `VisibilityVerdictSource`

| 名称 | 说明 |
|------|------|
| `VISIBILITY_VERDICT_SOURCE_REVIEW` | 与审核流关联 |
| `VISIBILITY_VERDICT_SOURCE_MANUAL_OPS` | 人工运营 |
| `VISIBILITY_VERDICT_SOURCE_POLICY` | 策略驱动 |
| `VISIBILITY_VERDICT_SOURCE_SYSTEM` | 系统任务 |

### 2.5 `CooperationSubjectType`

| 名称 | 说明 |
|------|------|
| `COOPERATION_SUBJECT_TYPE_CONTENT` | 图文/攻略 |
| `COOPERATION_SUBJECT_TYPE_CARD` | 导购卡片 |
| `COOPERATION_SUBJECT_TYPE_TOPIC` | 专题 |
| `COOPERATION_SUBJECT_TYPE_RANKING` | 榜单 |

### 2.6 `RiskSubjectType`

| 名称 | 说明 |
|------|------|
| `RISK_SUBJECT_TYPE_CONTENT` | 内容 |
| `RISK_SUBJECT_TYPE_ACCOUNT` | 账号 |
| `RISK_SUBJECT_TYPE_CHANNEL` | 渠道/流量入口 |

### 2.7 `RiskSeverity`

| 名称 | 说明 |
|------|------|
| `RISK_SEVERITY_LOW` | |
| `RISK_SEVERITY_MEDIUM` | |
| `RISK_SEVERITY_HIGH` | |
| `RISK_SEVERITY_CRITICAL` | |

### 2.8 `PolicyVersionStatus`

| 名称 | 说明 |
|------|------|
| `POLICY_VERSION_STATUS_DRAFT` | 草稿 |
| `POLICY_VERSION_STATUS_ACTIVE` | 生效 |
| `POLICY_VERSION_STATUS_DEPRECATED` | 废弃 |

### 2.9 `PolicyViolationSeverity`（评估输出）

| 名称 | 说明 |
|------|------|
| `POLICY_VIOLATION_SEVERITY_INFO` | 提示 |
| `POLICY_VIOLATION_SEVERITY_WARNING` | 警告 |
| `POLICY_VIOLATION_SEVERITY_BLOCKING` | 阻断 |

### 2.10 `RequiredActionType`（评估输出）

| 名称 | 说明 |
|------|------|
| `REQUIRED_ACTION_TYPE_ADD_DISCLOSURE` | 必须附加披露 |
| `REQUIRED_ACTION_TYPE_UNPUBLISH` | 建议/必须下架 |
| `REQUIRED_ACTION_TYPE_ENQUEUE_REVIEW` | 重新入审 |

---

## 3. 通用消息

### 3.1 `PaginationCursor`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `next_cursor` | string | 否 | optional；无下一页可省略 |
| `has_more` | bool | 是 | |
| `limit` | int32 | 是 | |

### 3.2 `ContentRef`（队列与裁决主键）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | 与内容域主键对齐的**逻辑内容 ID**（卡片/图文/专题/榜单在治理侧统一以 string 引用，前缀或 kind 可由实现约定） |
| `content_version` | string | 否 | 与内容版本对齐，用于幂等；可与 `revision` 字符串化一致 |

---

## 4. 审核（Review）

### 4.1 `ReviewQueueItem`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | 队列项 ID |
| `content_id` | string | 是 | |
| `content_version` | string | 否 | 幂等键组成部分 |
| `status` | `ReviewQueueItemStatus` | 是 | |
| `priority` | int32 | 是 | 越大越优先 |
| `enqueue_reason` | string | 是 | 如新稿、举报、策略命中 |
| `channel_hints` | repeated string | 否 | SLA/分配提示 |
| `enqueued_at` | timestamp | 否 | |

### 4.2 `ReviewDecision`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | |
| `queue_item_id` | string | 是 | |
| `content_id` | string | 是 | 冗余查询 |
| `outcome` | `ReviewOutcome` | 是 | |
| `comment` | string | 否 | 审核意见 |
| `reviewer_id` | string | 是 | |
| `policy_evaluation_id` | string | 否 | 关联策略快照 |
| `decided_at` | timestamp | 是 | |

### 4.3 `ReviewState`（聚合态）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | |
| `latest_queue_item` | `ReviewQueueItem` | 否 | 当前或最近队列项 |
| `latest_decision` | `ReviewDecision` | 否 | 最近一条结论 |

### 4.4 RPC：`EnqueueReview`

**`EnqueueReviewRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | 幂等；建议键 `content_id + content_version` |
| `content_id` | string | 是 | |
| `content_version` | string | 否 | |
| `priority` | int32 | 否 | |
| `enqueue_reason` | string | 是 | |
| `channel_hints` | repeated string | 否 | |
| `audit_trace_id` | string | 否 | |

**`EnqueueReviewResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `queue_item` | `ReviewQueueItem` | 是 | |

### 4.5 RPC：`ListReviewQueueItems`

**`ListReviewQueueItemsRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `status` | `ReviewQueueItemStatus` | 否 | 筛选；默认 `PENDING`+`IN_REVIEW` 可由实现约定 |
| `content_id_prefix` | string | 否 | 可选筛选 |
| `cursor` | string | 否 | |
| `limit` | int32 | 否 | |

**`ListReviewQueueItemsResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `items` | repeated `ReviewQueueItem` | 是 | |
| `pagination` | `PaginationCursor` | 是 | |

### 4.6 RPC：`SubmitReviewDecision`

**`SubmitReviewDecisionRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | |
| `queue_item_id` | string | 是 | |
| `outcome` | `ReviewOutcome` | 是 | **通过不触发自动发布** |
| `comment` | string | 否 | |
| `reviewer_id` | string | 是 | |
| `policy_evaluation_id` | string | 否 | |

**`SubmitReviewDecisionResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `decision` | `ReviewDecision` | 是 | |

### 4.7 RPC：`ListReviewDecisions`

**`ListReviewDecisionsRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | |
| `cursor` | string | 否 | |
| `limit` | int32 | 否 | |

**`ListReviewDecisionsResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `decisions` | repeated `ReviewDecision` | 是 | 时间倒序 |
| `pagination` | `PaginationCursor` | 是 | |

### 4.8 RPC：`GetReviewState`

**`GetReviewStateRequest`**: `content_id` (string, 必填)

**`GetReviewStateResponse`**: `state` (`ReviewState`)

---

## 5. 商业合作标识（Cooperation）

### 5.1 `CooperationLabel`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | |
| `subject_type` | `CooperationSubjectType` | 是 | |
| `subject_id` | string | 是 | |
| `cooperation_type` | string | 是 | 业务字典，如 `affiliate`, `brand_sponsored` |
| `disclosure_template_id` | string | 是 | |
| `effective_from` | timestamp | 否 | |
| `effective_to` | timestamp | 否 | |
| `metadata` | map<string, string> | 否 | 扩展；注意脱敏 |

### 5.2 `DisclosureTemplate`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | |
| `locale` | string | 是 | 如 `zh-CN` |
| `template_key` | string | 是 | C 端文案 key |
| `body` | string | 是 | 可含占位符 |
| `version` | int32 | 是 | |

### 5.3 RPC：`UpsertCooperationLabel`

**`UpsertCooperationLabelRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | |
| `label` | `CooperationLabel` | 是 | `id` 空为创建 |

**`UpsertCooperationLabelResponse`**: `label` (`CooperationLabel`)

### 5.4 RPC：`BatchGetCooperationLabels`

**`BatchGetCooperationLabelsRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `keys` | repeated `SubjectKey` | 是 | 批量上限与网关约定 |

**`SubjectKey`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `subject_type` | `CooperationSubjectType` | 是 | |
| `subject_id` | string | 是 | |

**`BatchGetCooperationLabelsResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `labels_by_subject` | map<string, `CooperationLabels`> | 是 | key 建议 `subject_type:subject_id` |

**`CooperationLabels`**: `repeated CooperationLabel labels`

### 5.5 RPC：`ListDisclosureTemplates`

**`ListDisclosureTemplatesRequest`**: `locale` (string, 可选), `cursor`, `limit`

**`ListDisclosureTemplatesResponse`**: `templates` (repeated `DisclosureTemplate`), `pagination` (`PaginationCursor`)

---

## 6. 可见性（Visibility）

### 6.1 `VisibilityVerdict`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | |
| `content_id` | string | 是 | |
| `state` | `VisibilityState` | 是 | |
| `reason_code` | string | 是 | 标准原因码 |
| `source` | `VisibilityVerdictSource` | 是 | |
| `source_ref_id` | string | 否 | 审核单/策略版本等 |
| `effective_from` | timestamp | 是 | |
| `version` | int64 | 是 | 单调递增，缓存失效 |

### 6.2 RPC：`SetVisibilityVerdict`

**`SetVisibilityVerdictRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | |
| `content_id` | string | 是 | |
| `state` | `VisibilityState` | 是 | |
| `reason_code` | string | 是 | |
| `source` | `VisibilityVerdictSource` | 是 | |
| `source_ref_id` | string | 否 | |
| `effective_from` | timestamp | 否 | 默认现在 |
| `expected_version` | int64 | 否 | 可选 CAS |

**`SetVisibilityVerdictResponse`**: `verdict` (`VisibilityVerdict`)

### 6.3 `VisibilityVerdictMutation`（批量单项）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | |
| `state` | `VisibilityState` | 是 | |
| `reason_code` | string | 是 | |
| `source` | `VisibilityVerdictSource` | 是 | |
| `source_ref_id` | string | 否 | |
| `effective_from` | timestamp | 否 | 默认现在 |
| `expected_version` | int64 | 否 | 可选 CAS |

### 6.4 RPC：`BatchSetVisibilityVerdict`

**`BatchSetVisibilityVerdictRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | |
| `items` | repeated `VisibilityVerdictMutation` | 是 | 批量上限由网关/服务配置 |

**`BatchSetVisibilityVerdictResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `verdicts` | repeated `VisibilityVerdict` | 是 | 与成功项对齐 |
| `failed_content_ids` | repeated string | 否 | 部分失败时 |

### 6.5 RPC：`GetVisibilityVerdict`

**`GetVisibilityVerdictRequest`**: `content_id` (string)

**`GetVisibilityVerdictResponse`**: `verdict` (`VisibilityVerdict`)

### 6.6 RPC：`EvaluateVisibility`

**`EvaluateVisibilityRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | |
| `channel` | string | 否 | |
| `user_segment` | string | 否 | |

**`EvaluateVisibilityResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `allowed` | bool | 是 | 是否允许对用户展示 |
| `reason_codes` | repeated string | 是 | |
| `verdict` | `VisibilityVerdict` | 否 | 当前生效裁决快照 |

---

## 7. 运营配置（OpsConfig）

### 7.1 `OpsConfigSnapshot`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `config_key` | string | 是 | 如 `governance.feature_flags` |
| `version` | int32 | 是 | 对 `config_key` 单调递增 |
| `payload_json` | string | 是 | 校验后的 JSON 字符串 |
| `released_at` | timestamp | 是 | |
| `released_by` | string | 是 | |

### 7.2 `OpsConfigDraft`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | |
| `config_key` | string | 是 | |
| `payload_json` | string | 是 | |
| `created_by` | string | 否 | |
| `updated_at` | timestamp | 否 | |

### 7.3 RPC：`GetOpsConfigSnapshot`

**`GetOpsConfigSnapshotRequest`**: `config_key` (string), `version` (int32, 可选，缺省为当前生效)

**`GetOpsConfigSnapshotResponse`**: `snapshot` (`OpsConfigSnapshot`)

### 7.4 RPC：`CreateOpsConfigDraft`

**`CreateOpsConfigDraftRequest`**: `client_request_id`, `config_key`, `payload_json`, `created_by` (可选)

**`CreateOpsConfigDraftResponse`**: `draft` (`OpsConfigDraft`)

### 7.5 RPC：`ReleaseOpsConfig`

**`ReleaseOpsConfigRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | |
| `draft_id` | string | 是 | |
| `released_by` | string | 是 | |

**`ReleaseOpsConfigResponse`**: `snapshot` (`OpsConfigSnapshot`)

### 7.6 RPC：`RollbackOpsConfigRelease`

**`RollbackOpsConfigReleaseRequest`**: `client_request_id`, `config_key`, `target_version` (int32), `released_by`

**`RollbackOpsConfigReleaseResponse`**: `snapshot` (`OpsConfigSnapshot`)

---

## 8. 风险标记（Risk）

### 8.1 `RiskFlag`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | |
| `subject_type` | `RiskSubjectType` | 是 | |
| `subject_id` | string | 是 | |
| `flag_code` | string | 是 | |
| `severity` | `RiskSeverity` | 是 | |
| `source` | string | 是 | 如 `manual`, `external_risk_engine` |
| `expires_at` | timestamp | 否 | 空为长期 |
| `active` | bool | 是 | |
| `created_at` | timestamp | 否 | |

### 8.2 RPC：`CreateRiskFlag`

**`CreateRiskFlagRequest`**: `client_request_id`, `subject_type`, `subject_id`, `flag_code`, `severity`, `source`, `expires_at` (可选)

**`CreateRiskFlagResponse`**: `flag` (`RiskFlag`)

### 8.3 RPC：`RevokeRiskFlag`

**`RevokeRiskFlagRequest`**: `client_request_id`, `flag_id` (string), `revoked_by` (string)

**`RevokeRiskFlagResponse`**: `flag` (`RiskFlag`, `active=false`)

### 8.4 RPC：`ListRiskFlags`

**`ListRiskFlagsRequest`**: `subject_type`, `subject_id`, `active_only` (bool, 默认 true), `cursor`, `limit`

**`ListRiskFlagsResponse`**: `flags` (repeated `RiskFlag`), `pagination` (`PaginationCursor`)

### 8.5 RPC：`BatchGetRiskFlagsBySubject`

**`BatchGetRiskFlagsBySubjectRequest`**: `keys` (repeated `RiskSubjectKey`), `active_only` (bool)

**`RiskSubjectKey`**: `subject_type`, `subject_id`

**`BatchGetRiskFlagsBySubjectResponse`**: `flags_by_subject` (map<string, `RiskFlagList`>)

**`RiskFlagList`**: `repeated RiskFlag flags`

---

## 9. 策略（Policy）

### 9.1 `PolicyViolation`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | string | 是 | |
| `message` | string | 否 | |
| `severity` | `PolicyViolationSeverity` | 是 | |

### 9.2 `RequiredAction`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `type` | `RequiredActionType` | 是 | |
| `detail` | string | 否 | |

### 9.3 `PolicyEvaluationContext`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 否 | |
| `subject_type` | `CooperationSubjectType` | 否 | |
| `subject_id` | string | 否 | |
| `attributes` | map<string, string> | 否 | 内容属性摘要 |
| `channel` | string | 否 | |

### 9.4 `PolicyVersion`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | |
| `version_label` | string | 是 | 语义化或日历版本 |
| `ruleset_ref` | string | 否 | DSL 或远程引用 |
| `ruleset_json` | string | 否 | 内联 JSON |
| `status` | `PolicyVersionStatus` | 是 | |
| `effective_from` | timestamp | 是 | |

### 9.5 RPC：`EvaluatePolicy`

**`EvaluatePolicyRequest`**: `context` (`PolicyEvaluationContext`), `policy_version_id` (string, 可选)

**`EvaluatePolicyResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `evaluation_id` | string | 是 | 快照 ID，可写入 `ReviewDecision.policy_evaluation_id` |
| `violations` | repeated `PolicyViolation` | 是 | |
| `required_actions` | repeated `RequiredAction` | 是 | |

### 9.6 RPC：`CreatePolicyVersion`

**`CreatePolicyVersionRequest`**: `client_request_id`, `version` (`PolicyVersion` 除 `id` 外字段)

**`CreatePolicyVersionResponse`**: `version` (`PolicyVersion`)

### 9.7 RPC：`SimulatePolicy`

**`SimulatePolicyRequest`**: 同 `EvaluatePolicyRequest`

**`SimulatePolicyResponse`**: 同 `EvaluatePolicyResponse`（**无副作用**）

---

## 10. 服务定义

### 10.1 `GovernanceReviewService`

| RPC | 请求 | 响应 |
|-----|------|------|
| `EnqueueReview` | `EnqueueReviewRequest` | `EnqueueReviewResponse` |
| `ListReviewQueueItems` | `ListReviewQueueItemsRequest` | `ListReviewQueueItemsResponse` |
| `SubmitReviewDecision` | `SubmitReviewDecisionRequest` | `SubmitReviewDecisionResponse` |
| `ListReviewDecisions` | `ListReviewDecisionsRequest` | `ListReviewDecisionsResponse` |
| `GetReviewState` | `GetReviewStateRequest` | `GetReviewStateResponse` |

### 10.2 `GovernanceCooperationService`

| RPC | 请求 | 响应 |
|-----|------|------|
| `UpsertCooperationLabel` | `UpsertCooperationLabelRequest` | `UpsertCooperationLabelResponse` |
| `BatchGetCooperationLabels` | `BatchGetCooperationLabelsRequest` | `BatchGetCooperationLabelsResponse` |
| `ListDisclosureTemplates` | `ListDisclosureTemplatesRequest` | `ListDisclosureTemplatesResponse` |

### 10.3 `GovernanceVisibilityService`

| RPC | 请求 | 响应 |
|-----|------|------|
| `SetVisibilityVerdict` | `SetVisibilityVerdictRequest` | `SetVisibilityVerdictResponse` |
| `BatchSetVisibilityVerdict` | `BatchSetVisibilityVerdictRequest` | `BatchSetVisibilityVerdictResponse` |
| `GetVisibilityVerdict` | `GetVisibilityVerdictRequest` | `GetVisibilityVerdictResponse` |
| `EvaluateVisibility` | `EvaluateVisibilityRequest` | `EvaluateVisibilityResponse` |

### 10.4 `GovernanceOpsConfigService`

| RPC | 请求 | 响应 |
|-----|------|------|
| `GetOpsConfigSnapshot` | `GetOpsConfigSnapshotRequest` | `GetOpsConfigSnapshotResponse` |
| `CreateOpsConfigDraft` | `CreateOpsConfigDraftRequest` | `CreateOpsConfigDraftResponse` |
| `ReleaseOpsConfig` | `ReleaseOpsConfigRequest` | `ReleaseOpsConfigResponse` |
| `RollbackOpsConfigRelease` | `RollbackOpsConfigReleaseRequest` | `RollbackOpsConfigReleaseResponse` |

### 10.5 `GovernanceRiskService`

| RPC | 请求 | 响应 |
|-----|------|------|
| `CreateRiskFlag` | `CreateRiskFlagRequest` | `CreateRiskFlagResponse` |
| `RevokeRiskFlag` | `RevokeRiskFlagRequest` | `RevokeRiskFlagResponse` |
| `ListRiskFlags` | `ListRiskFlagsRequest` | `ListRiskFlagsResponse` |
| `BatchGetRiskFlagsBySubject` | `BatchGetRiskFlagsBySubjectRequest` | `BatchGetRiskFlagsBySubjectResponse` |

### 10.6 `GovernancePolicyService`

| RPC | 请求 | 响应 |
|-----|------|------|
| `EvaluatePolicy` | `EvaluatePolicyRequest` | `EvaluatePolicyResponse` |
| `CreatePolicyVersion` | `CreatePolicyVersionRequest` | `CreatePolicyVersionResponse` |
| `SimulatePolicy` | `SimulatePolicyRequest` | `SimulatePolicyResponse` |

---

## 11. 错误语义（与 `docs/contracts/error-codes.md` 对齐）

内部 RPC 失败时携带整数 `code`（应用详情或约定映射），与下列语义一致（节选）：

| code | 语义 |
|------|------|
| `0` | 成功 |
| `10001` / `10002` | 参数缺失 / 校验失败 |
| `20004` | 权限不足 |
| `30001` | 资源不存在 |
| `60001` | 合规拦截 |
| `60002` | 地域或策略限制 |
| `90001`–`90003` | 系统/依赖 |

**主链路建议**：推荐与 gateway 默认消费 **物化可见性快照**，`EvaluateVisibility` 为低频兜底；同步依赖异常时默认 **fail-closed**。

**扩展码**：治理域内细分码在实现期写入 `error-codes.md` 并 `changelog.md` 记录。

---

## 12. 事件（非 RPC，载荷要点）

Kafka 等异步载荷命名实现时定稿，建议主题与字段：

| 事件 | 载荷要点 |
|------|----------|
| `governance.review.decision_made` | `content_id`, `outcome`, `reviewer_id` |
| `governance.visibility.changed` | `content_id`, `state`, `reason_code` |
| `governance.cooperation.label_updated` | `subject_type`, `subject_id` |
| `governance.ops_config.released` | `config_key`, `version` |
| `governance.risk.flag_changed` | `subject_type`, `subject_id`, `flag_code`, `active` |

事件 Schema 若对外共享，放入 `docs/contracts/` 专篇。

---

## 13. `proto` 映射

本文件与 `services/governance-domain/proto/governance_domain.proto` 对齐；字段名 **snake_case**，多服务可在同一文件以 `service` 块划分。
