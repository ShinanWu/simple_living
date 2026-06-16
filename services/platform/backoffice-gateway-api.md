# 运营管理后台 — Gateway HTTP 契约

响应信封与通用错误码见 [`services/gateway/api.md`](../../gateway/api.md)。本文件描述 `/api/v2/backoffice/*` 运营路由的字段与流程。

## 1. 目标

本文定义 **运营管理后台的后端能力**（管理、审核、可见性、发布协同），并作为前端页面实现与联调验收的后端基础。
入口统一为 `gateway`，业务真相分别归属：

- **affiliate 模块**：伙伴与联盟规则
- **content 模块**：内容主数据与发布版本
- **governance 模块**：审核、可见性、策略与风险

（均运行于 `platform/backoffice-backend` 单进程。）

## 2. 商业化闭环

后端能力以"真实领域写入 + 可审计操作 + 可扩展权限"为目标，覆盖三条主线：

1. **联盟管理**：查询伙伴、新增伙伴
2. **内容运营**：内容 CRUD、提交审核、版本发布、上下架、回滚
3. **治理审核**：查询审核队列、提交审核结论（通过/拒绝）、可见性裁决

所有接口走标准信封（`success/code/message/data/meta`），字段 `snake_case`。

## 3. 路由

### 3.0 运营登录

| 模块 | 方法 + 路径 | 说明 |
|------|------|------|
| auth | `POST /api/v2/backoffice/auth/login` | 校验 `access_token`，返回 Bearer `token` 与 `role` |

其余写接口需 `Authorization: Bearer <token>`（gateway `-backoffice_api_token`）。

### 3.1 联盟管理

| 模块 | 方法 + 路径 | 说明 |
|------|------|------|
| affiliate | `POST /api/v2/backoffice/affiliate/partners` | 伙伴列表 |
| affiliate | `POST /api/v2/backoffice/affiliate/partners/add` | 新增伙伴 |

### 3.2 内容管理

| 模块 | 方法 + 路径 | 说明 |
|------|------|------|
| content | `POST /api/v2/backoffice/content/items` | 内容列表，支持资源类型、主题、状态过滤 |
| content | `POST /api/v2/backoffice/content/items/add` | 新增内容，写入 platform/backoffice-backend |
| content | `POST /api/v2/backoffice/content/items/update` | 更新内容（编辑或修订） |
| content | `POST /api/v2/backoffice/content/items/detail` | 获取内容详情（含版本历史） |
| content | `POST /api/v2/backoffice/content/items/submit-review` | 提交审核 |
| content | `POST /api/v2/backoffice/content/items/publish` | 发布指定版本 |
| content | `POST /api/v2/backoffice/content/items/status` | 内容状态变更 |
| content | `POST /api/v2/backoffice/content/items/rollback` | 版本回滚 |
| media | `POST /api/v2/backoffice/media/upload` | 上传封面等图片，落盘并返回公开 URL |
| media | `GET /media/backoffice/{asset_file}` | 读取已上传的静态图片（无需鉴权） |

封面字段 `cover_media.url` **必须**为可公开访问的绝对 URL（`http://` 或 `https://`），**禁止** inline `data:` URL 或站内相对路径。运营台上传封面时应先调用 media upload，将返回的 `url`（完整 HTTP 链接）写入内容。

gateway 通过 `-gateway_backoffice_media_public_base_url` 配置素材公网基址。生产使用 `https://shaotang.top` 与受信证书；lab 无域名时可设 `TLS_SELF_SIGN=1` 或显式 `GATEWAY_MEDIA_PUBLIC_BASE_URL`。

### 3.3 治理审核

| 模块 | 方法 + 路径 | 说明 |
|------|------|------|
| governance | `POST /api/v2/backoffice/governance/reviews` | 审核队列列表 |
| governance | `POST /api/v2/backoffice/governance/reviews/status` | 审核状态更新 |
| governance | `POST /api/v2/backoffice/governance/visibility` | 可见性裁决 |

## 4. 数据对象（对外 JSON）

### 4.1 `backoffice_partner`

| 字段 | 类型 | 说明 |
|------|------|------|
| `partner_id` | string | 伙伴唯一标识 |
| `display_name` | string | 展示名称 |
| `status` | enum | `draft` / `active` / `disabled` / `sunset` |
| `primary_channel_code` | string | 主渠道编码 |

### 4.2 `backoffice_content_item`

内容管理核心数据对象，支持四种资源类型。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | 内容唯一标识 |
| `resource_kind` | enum | 是 | `guide_card` / `editorial_content` / `topic` / `ranking_list` |
| `title` | string | 是 | 标题 |
| `subtitle` | string | 否 | 副标题 |
| `theme_ids` | string[] | 否 | 关联主题 ID 列表 |
| `tag_ids` | string[] | 否 | 标签 ID 列表 |
| `content_status` | enum | 是 | `draft` / `in_review` / `approved` / `published` / `scheduled` / `offline` / `archived` |
| `revision` | int64 | 是 | 当前版本号 |
| `published_revision` | int64 | 是 | 当前发布版本号 |
| `summary` | string | 否 | 推荐摘要 |
| `cover_media` | `MediaRef` | 否 | 封面媒体 |
| `landing_url` | string | 否 | 落地页链接 |
| `external_item_id` | string | 否 | 外部商品/服务 ID |
| `affiliate_refs` | `AffiliateRef[]` | 否 | 联盟渠道引用 |
| `commercial_disclosure_required` | bool | 否 | 是否需要商业披露 |
| `effective_from` | string | 否 | 生效时间 (ISO 8601) |
| `effective_to` | string | 否 | 失效时间 (ISO 8601) |
| `created_at` | string | 是 | 创建时间 (ISO 8601) |
| `updated_at` | string | 是 | 更新时间 (ISO 8601) |
| `created_by` | string | 否 | 创建者 |
| `updated_by` | string | 否 | 更新者 |

**`MediaRef`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `media_id` | string | 素材库 ID |
| `url` | string | CDN 地址 |
| `type` | string | `image` / `video` / `icon` |
| `width` | int | 可选 |
| `height` | int | 可选 |

**`AffiliateRef`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `channel` | string | 渠道标识（如 `PDD` / `DOUYIN`） |
| `external_item_id` | string | 渠道侧商品 ID |
| `external_shop_id` | string | 可选，店铺 ID |
| `payload` | map | 扩展字段 |

### 4.3 `backoffice_content_detail`

内容详情对象，含版本历史。

| 字段 | 类型 | 说明 |
|------|------|------|
| `content` | `backoffice_content_item` | 当前版本 |
| `revision_history` | `ContentRevision[]` | 版本历史 |
| `review_state` | `ReviewState` | 审核状态 |
| `visibility_verdict` | `VisibilityVerdict` | 可见性裁决 |

**`ContentRevision`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `revision` | int64 | 版本号 |
| `created_at` | string | 创建时间 (ISO 8601) |
| `created_by` | string | 创建者 |
| `change_summary` | string | 变更说明 |

**`ReviewState`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `review_id` | string | 审核单 ID |
| `status` | string | `pending` / `in_review` / `approved` / `rejected` / `needs_info` |
| `submitted_at` | string | 提交时间 (ISO 8601) |
| `reviewer_id` | string | 审核员 |
| `comment` | string | 审核意见 |

**`VisibilityVerdict`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `state` | string | `published` / `unpublished` / `restricted` |
| `reason_code` | string | 裁决原因码 |
| `source` | string | `review` / `manual_ops` / `policy` / `system` |
| `effective_from` | string | 生效时间 (ISO 8601) |
| `version` | int64 | 裁决版本号 |

### 4.4 `backoffice_review_item`

| 字段 | 类型 | 说明 |
|------|------|------|
| `review_id` | string | 审核单 ID |
| `subject_id` | string | 审核对象 ID |
| `resource_kind` | string | 资源类型 |
| `status` | enum | `pending` / `in_review` / `approved` / `rejected` / `needs_info` / `completed` |
| `priority` | int | 优先级 |
| `enqueue_reason` | string | 入审原因 |
| `enqueued_at` | string | 入审时间 (ISO 8601) |
| `reviewer_id` | string | 审核员 |
| `comment` | string | 审核意见 |
| `decided_at` | string | 裁决时间 (ISO 8601) |

### 4.5 请求/响应体定义

#### 4.5.0 `POST /api/v2/backoffice/content/items/add` — 新增内容

**必填**：`title`、`landing_url`（导购卡）。

`content_id` 由网关按 `guide_{theme}_{channel}_{external_item_id}` 生成：`external_item_id` 优先；否则从 `landing_url` 的 `id=` 查询参数提取商品 id；再否则对 URL 做截断规范化。

**冲突**：同一 `content_id` 已存在时返回 **`10007`**（HTTP 409），**不会**静默覆盖已有内容。请填写不同的落地页或外部商品 ID，或编辑已有条目。

**状态**：新建仅允许 `draft`（或省略 `initial_status`）；`published` 须先送审再发布。

---

#### 4.5.1 `POST /api/v2/backoffice/content/items/update` — 更新内容

**请求体** `BackofficeUpdateContentItemRequest`：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | 内容 ID |
| `revision` | int64 | 是 | 当前版本号（乐观锁） |
| `title` | string | 否 | 标题 |
| `subtitle` | string | 否 | 副标题 |
| `summary` | string | 否 | 推荐摘要 |
| `landing_url` | string | 否 | 落地页链接 |
| `external_item_id` | string | 否 | 外部商品 ID |
| `cover_url` | string | 否 | 封面图 URL |
| `theme` | string | 否 | 主题 |
| `change_summary` | string | 否 | 变更说明 |

**响应体** `BackofficeContentItemsResponse`：

同列表接口。

---

#### 4.5.2 `POST /api/v2/backoffice/content/items/detail` — 内容详情

**请求体** `BackofficeContentDetailRequest`：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | 内容 ID |

**响应体** `BackofficeContentDetailResponse`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `content` | `BackofficeContentItem` | 当前版本内容 |
| `revision_history` | `ContentRevision[]` | 版本历史 |
| `review_state` | `ReviewState` | 当前审核状态 |
| `visibility_verdict` | `VisibilityVerdict` | 可见性裁决 |

---

#### 4.5.3 `POST /api/v2/backoffice/content/items/submit-review` — 提交审核

**请求体** `BackofficeSubmitReviewRequest`：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | 内容 ID |
| `revision` | int64 | 是 | 要提交审核的版本号 |
| `change_summary` | string | 否 | 变更说明 |

**响应体** `BackofficeSubmitReviewResponse`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | bool | 是否成功 |
| `review_id` | string | 审核单 ID |
| `content_status` | string | 更新后的内容状态 |

---

#### 4.5.4 `POST /api/v2/backoffice/content/items/publish` — 发布版本

**请求体** `BackofficePublishRequest`：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | 内容 ID |
| `revision` | int64 | 是 | 要发布的版本号 |

**响应体** `BackofficePublishResponse`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | bool | 是否成功 |
| `content_id` | string | 内容 ID |
| `published_revision` | int64 | 已发布版本号 |
| `visibility_state` | string | 可见性状态 |

---

#### 4.5.5 `POST /api/v2/backoffice/content/items/rollback` — 版本回滚

**请求体** `BackofficeRollbackRequest`：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | 内容 ID |
| `target_revision` | int64 | 是 | 回滚目标版本号 |
| `change_summary` | string | 否 | 回滚说明 |

**响应体** `BackofficeRollbackResponse`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | bool | 是否成功 |
| `new_revision` | int64 | 新版本号（回滚后生成） |
| `content_id` | string | 内容 ID |

---

#### 4.5.6 `POST /api/v2/backoffice/media/upload` — 上传静态媒体

**请求体** `BackofficeMediaUploadRequest`（二选一）：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `data_url` | string | 否 | 浏览器 `FileReader` 生成的 data URL |
| `content_base64` | string | 否 | 纯 base64 正文（配合 `content_type`） |
| `content_type` | string | 否 | MIME，如 `image/jpeg` |
| `filename` | string | 否 | 原始文件名（仅审计） |

**响应体** `BackofficeMediaUploadResponse`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `asset_id` | string | 素材 ID |
| `url` | string | 返回 **绝对 HTTPS URL**（如 `https://shaotang.top/media/backoffice/...`） |
| `content_type` | string | 存储 MIME |

文件落盘目录由 gateway `-gateway_backoffice_media_dir` 控制（默认 `/var/lib/simple-living/media/backoffice`）。公网访问域名由 `-gateway_backoffice_media_public_base_url` 配置（必填，上传接口依赖此项）。`GET /media/backoffice/{filename}` 由 gateway 直接返回二进制，不经 JSON 信封。

**入口 nginx**：`client_max_body_size` 须 ≥ **8m**（运营台以 base64 data URL 提交，约 3MB 原图即可逼近默认 1m 限制）。见 `services/proxy/src/nginx/gateway*.conf.example`。

---

#### 4.5.7 `POST /api/v2/backoffice/governance/visibility` — 可见性裁决

**请求体** `BackofficeSetVisibilityRequest`：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | 内容 ID |
| `state` | string | 是 | `published` / `unpublished` / `restricted` |
| `reason_code` | string | 否 | 裁决原因码 |
| `effective_from` | string | 否 | 生效时间 (ISO 8601) |

**响应体** `BackofficeSetVisibilityResponse`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | bool | 是否成功 |
| `content_id` | string | 内容 ID |
| `visibility_state` | string | 更新后的可见性状态 |

---

## 5. 状态机

`content_status`（内容编辑态）与 `visibility_verdict`（C 端可见性）为**两个独立维度**；产品语义见 `services/platform/product-spec.md` §4。

### 5.1 内容生命周期（`content_status`）

```
                    submit-review
         draft ──────────────────> in_review
           ^                           │
           │ reject / needs_info       │ 治理 approved
           └───────────────────────────┤
                                       v
                                  approved ──publish（显式）──> published ──offline──> offline ──archive──> archived
```

| 状态 | 说明 | 可执行操作 |
|------|------|------------|
| `draft` | 草稿 | 编辑、提交审核 |
| `in_review` | 已送审，等待治理裁决 | 详情 |
| `approved` | 治理已通过，待运营发布 | 详情、发布 |
| `published` | 已执行发布 | 下架、提交修订、回滚版本 |
| `scheduled` | 定时发布 | 取消定时、编辑生效时间 |
| `offline` | 已下架 | 重新发布、归档 |
| `archived` | 已归档 | 仅查看 |

### 5.2 审核状态机（审核单 `status`）

```
pending ──受理──> in_review ──裁决──> approved ──> completed
                                      │
                                      ├── rejected ──> completed
                                      │
                                      └── needs_info ─> completed
```

审核 `approved` 将 `content_status` 置为 `approved`（待发布），**不会**自动变为 `published`；发布须调用 `content/items/publish`。

### 5.3 可见性裁决（`visibility_verdict.state`）

| 值 | 含义 |
|----|------|
| `published` | 允许 C 端展示（仍须 `content_status` 为已发布态） |
| `restricted` | 限制展示 |
| `unpublished` | 不可见（紧急下架） |

**双门闸（fail-closed）**

- 审核通过（`approved`）**不自动发布**，也 **不自动** 将可见性设为 `published`。
- 可见性随内容生命周期同步：`content/items/publish` → `visibility.state=published`；`content/items/status` 转 `offline` / `archived` / `draft` → `visibility.state=unpublished`。这是发布即上线、下架即隐藏的默认语义。
- `governance/visibility` 裁决用于在两次生命周期转换之间 **显式覆盖**（如 `restricted` 或紧急 `unpublished` 下架）；下一次 publish / status 转换会按上一条重新同步可见性。
- 即使 `content_status = published`，若 `visibility.state ≠ published`，C 端不可见（门闸仍然生效）。
- `SetVisibilityVerdict` 为显式操作，需 `reviewer` 或 `backoffice_admin` 角色。

### 5.4 领域协作规则

- gateway **不**承载跨域事务；多模块编排由 `backoffice-backend` 进程内完成。
- 内容主数据、审核态、可见性、联盟配置 **不得** 以 gateway 内存为权威；必须持久化于 `backoffice-backend` PostgreSQL。
- 联盟伙伴数据权威在 `backoffice-backend.affiliate`；gateway 不得长期以进程内内存替代。

## 6. 权限模型（业务级）

| 角色 | 权限 |
|------|------|
| `backoffice_admin` | 全部运营操作 |
| `content_operator` | 内容创建、编辑、提交审核、发布/下架 |
| `reviewer` | 审核裁决、可见性裁决 |
| `partner_operator` | 伙伴管理 |

## 7. 审计与可观测性

建议事件名：

- `backoffice.partner.created`
- `backoffice.content.created`
- `backoffice.content.updated`
- `backoffice.content.status_changed`
- `backoffice.content.submitted_for_review`
- `backoffice.content.published`
- `backoffice.content.rolled_back`
- `backoffice.review.status_changed`
- `backoffice.visibility.changed`

所有写操作必须在响应中回传 `meta.request_id` 与 `meta.trace_id`，用于审计追踪。

## 8. C 端 snapshot 交付（backoffice 责任）

运营写入经导出进入 C 端读路径；完整字段、主题字典与验收见 **[backoffice-delivery-spec.md](./backoffice-delivery-spec.md)**。

摘要：

| 导出 | backoffice 须保证 |
|------|-------------------|
| `catalog_snapshot` | published 卡片含 `theme_ids`、`selling_points`/`subtitle`、`cover_media.url`、`affiliate_refs[].payload.landing_url` |
| `visibility_index` | 显式 `published` 可见性；与 `content_status=published` 双门闸 |
| `affiliate_link_spec` | 活跃伙伴能力（tracking 渠道规格） |

**封面 URL**：`media/upload` 返回完整公网绝对 URL；lab 见 §3.2 `gateway_backoffice_media_public_base_url`。

**四主题 lab 最低数据**：`theme_1`～`theme_4` 各 ≥1 可见 published 卡；默认由 `dev_content_seeds` + 启动时 `EnsureSeedGuideCards` 补齐。

## 9. 相关文档

| 文档 | 说明 |
|------|------|
| [backoffice-delivery-spec.md](./backoffice-delivery-spec.md) | C 端数据输入、snapshot 字段、验收清单 |
| [README.md](./README.md) | 平台总览、架构与部署 |
| [product-spec.md](./product-spec.md) | 产品规格 |
| [detail-design.md](./detail-design.md) | 详细设计 |
| [api.md](./api.md) | 后端 RPC 契约 |
