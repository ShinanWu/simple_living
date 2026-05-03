# 运营管理后台后端（文档先行）

## 1. 目标

本文定义 **运营管理后台的后端能力**（管理、审核、可见性、发布协同），并作为前端页面实现与联调验收的后端基础。  
入口统一为 `gateway`，业务真相分别归属：

- `affiliate-domain`：伙伴与联盟规则
- `content-domain`：内容主数据与发布版本
- `governance-domain`：审核、可见性、策略与风险

## 2. v1 最小闭环

v1 以“可用后端 API + 可审计操作”为目标，覆盖三条主线：

1. **联盟管理**：查询伙伴、新增伙伴
2. **内容运营**：查询内容项、变更发布态
3. **治理审核**：查询审核队列、提交审核结论（通过/拒绝）

所有接口走标准信封（`success/code/message/data/meta`），字段 `snake_case`。

## 3. 路由（v2）

| 模块 | 方法 + 路径 | 说明 |
|------|------|------|
| affiliate | `GET /api/v2/backoffice/affiliate/partners` | 伙伴列表 |
| affiliate | `POST /api/v2/backoffice/affiliate/partners/add` | 新增伙伴 |
| content | `GET /api/v2/backoffice/content/items` | 内容列表 |
| content | `PATCH /api/v2/backoffice/content/items/status` | 内容状态更新 |
| governance | `GET /api/v2/backoffice/governance/reviews` | 审核队列列表 |
| governance | `PATCH /api/v2/backoffice/governance/reviews/status` | 审核状态更新 |

## 4. 数据对象（对外 JSON）

### 4.1 `backoffice_partner`

- `partner_id`
- `display_name`
- `status` (`draft|active|disabled|sunset`)
- `primary_channel_code`

### 4.2 `backoffice_content_item`

- `content_id`
- `title`
- `theme`
- `status`（v2 最小实现：`draft|published`）

### 4.3 `backoffice_review_item`

- `review_id`
- `subject_id`
- `status`（v2 最小实现：`pending|approved|rejected`）

## 5. 领域协作规则

- 审核通过 **不自动发布**
- 发布可见性最终以 `governance-domain` 裁决为准
- gateway 不承载跨域事务；跨域一致性通过流程与补偿保证

## 6. 状态机（v1）

- 审核：`pending -> in_review -> (approved | rejected | needs_info) -> completed`
- 内容：`draft -> in_review -> published -> offline -> archived`

## 7. 权限模型（业务级）

- `backoffice_admin`
- `reviewer`
- `content_operator`
- `partner_operator`

## 8. 审计与可观测性

建议事件名：

- `backoffice.partner.created`
- `backoffice.content.status_changed`
- `backoffice.review.status_changed`

## 9. 开发顺序（严格文档先行）

1. 锁定路由与字段（本文件 + `api.md`）
2. 锁定下游 RPC 对应关系（`affiliate/content/governance`）
3. 锁定错误码与状态机
4. 再进入 proto 与实现开发
