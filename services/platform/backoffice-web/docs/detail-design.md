# backoffice-web 详细设计（v3）

## 1. 设计目标

在 `affiliate/content/governance` 三域上提供可直接运行的运营工作台，满足：

- 列表可读：运营可快速查看当前业务状态
- 动作可执行：核心操作可在页面内直接完成
- 结果可追溯：每次操作都能看到请求结果与审计线索
- 失败可恢复：接口失败时可重试、可继续其他模块操作
- 完整内容生命周期：支持内容创建、编辑、审核、发布、下架、归档、版本回滚

## 2. 信息架构

单页三模块：

1. `联盟管理`：伙伴增查
2. `内容管理`：内容 CRUD、状态流转、版本管理、审核提交、发布
3. `治理审核`：审核状态处理、可见性裁决

页面结构：

- 顶部：标题、运行模式、全局提示
- 主体：三域 tab + 当前 tab 工作区
- 右侧（或底部）：最近操作日志（request_id、动作、结果）

## 3. 交互与状态模型

### 3.1 全局状态

- `initializing`：首次并发加载三个模块
- `ready`：可操作
- `partial_error`：某模块加载失败但页面可继续

### 3.2 模块状态

每个模块都独立维护：

- `loading`
- `success`
- `empty`
- `error`

互不阻塞：例如 `governance` 失败，不影响 `affiliate` 新增伙伴。

### 3.3 反馈策略

- 成功操作：显示成功提示 + 更新列表
- 失败操作：显示错误提示 + 保留输入
- 所有写操作：写入"最近操作日志"

## 4. 字段与动作约束

### 4.1 联盟管理

- 输入：`partner_id`（可自动从名称生成）、`display_name`、`status`、`primary_channel_code`
- 校验：
  - `display_name` 必填，去首尾空白后不能为空
  - `partner_id` 仅小写字母、数字、下划线

### 4.2 内容管理

内容管理模块分为两个视图：**内容列表视图** 和 **内容编辑视图**。

#### 4.2.1 内容列表视图

展示所有内容的列表，支持过滤和操作。

**展示字段**：

| 字段 | 说明 |
|------|------|
| `content_id` | 内容唯一标识 |
| `resource_kind` | 资源类型（guide_card / editorial_content / topic / ranking_list） |
| `title` | 标题 |
| `theme_ids` | 关联主题 |
| `content_status` | 生命周期状态 |
| `revision` | 当前版本号 |
| `published_revision` | 已发布版本号 |
| `created_at` / `updated_at` | 时间戳 |

**过滤条件**：

- `resource_kind`：资源类型过滤（全部 / guide_card / editorial_content / topic / ranking_list）
- `theme`：主题过滤
- `content_status`：状态过滤（全部 / draft / in_review / published / scheduled / offline / archived）

**列表操作**（根据当前状态动态展示）：

| 当前状态 | 可执行操作 |
|----------|-----------|
| `draft` | 编辑、提交审核、删除 |
| `in_review` | 查看审核状态 |
| `published` | 下架、提交修订、回滚版本 |
| `scheduled` | 取消发布、编辑生效时间 |
| `offline` | 重新发布、归档 |
| `archived` | 仅查看 |

#### 4.2.2 内容编辑视图

用于新增或编辑内容。根据 `resource_kind` 展示不同字段。

**通用字段**（所有资源类型）：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `resource_kind` | select | 是 | 资源类型 |
| `title` | text | 是 | 标题 |
| `subtitle` | text | 否 | 副标题 |
| `summary` | textarea | 否 | 推荐摘要 |
| `theme_ids` | multi-select | 否 | 关联主题 |
| `tag_ids` | multi-select | 否 | 标签 |
| `cover_media` | media-picker | 否 | 封面媒体（URL + 类型） |
| `initial_status` | select | 是 | 初始状态（draft / published） |

**guide_card 专属字段**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `landing_url` | url | 是 | 落地页链接 |
| `external_item_id` | text | 否 | 外部商品 ID |
| `affiliate_refs` | list | 否 | 联盟渠道引用（channel + external_item_id） |
| `selling_points` | text[] | 否 | 卖点列表 |
| `commercial_disclosure_required` | checkbox | 否 | 是否需要商业披露 |

**editorial_content 专属字段**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `blocks` | editorial-blocks | 是 | 内容块列表（段落/图片/嵌入卡片） |
| `author_display_name` | text | 否 | 作者名 |

**topic 专属字段**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `modules` | topic-modules | 是 | 专题模块列表 |
| `seo_slug` | text | 否 | SEO 路径 |

**ranking_list 专属字段**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `entries` | ranking-entries | 是 | 榜单条目（rank + card_id/content_id + blurb） |
| `rule_summary` | text | 是 | 规则说明 |
| `update_cadence` | select | 是 | 更新节奏 |

**生效时间控制**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `effective_from` | datetime | 否 | 生效时间 |
| `effective_to` | datetime | 否 | 失效时间 |

**表单校验规则**：

- `title` 必填，去首尾空白后不能为空
- `landing_url` 对 guide_card 必填，需为合法 URL
- `resource_kind` 必填
- `theme_ids` 至少选择一个（建议）
- URL 字段需通过格式校验

#### 4.2.3 内容详情抽屉

点击内容列表中的某一项，打开侧边详情抽屉，展示：

1. **基本信息**：所有字段只读展示
2. **版本历史**：列出所有 revision，支持回滚操作
3. **审核状态**：若已提交审核，展示审核单状态和审核意见
4. **可见性裁决**：展示当前治理可见性裁决结果

**版本回滚操作**：

- 仅 `published` / `offline` 状态可回滚
- 回滚目标必须为历史已发布版本
- 回滚需填写变更说明
- 回滚后生成新版本号

#### 4.2.4 提交审核流程

1. 从列表或详情页点击"提交审核"
2. 弹出确认对话框，展示提交版本号和变更摘要
3. 确认后调用 `submit-review` API
4. 成功后内容状态变为 `in_review`，列表和详情页更新
5. 审核结果在"治理审核"模块处理

#### 4.2.5 发布流程

1. 从列表或详情页点击"发布"
2. 系统检查治理可见性裁决
   - 若治理已放行（`visibility.state === published`），直接发布
   - 若治理未放行，提示需先完成审核
3. 确认后调用 `publish` API
4. 成功后内容状态变为 `published`

### 4.3 治理审核

- 展示字段：`review_id`、`subject_id`、`resource_kind`、`status`、`priority`、`enqueue_reason`
- 动作：`pending/in_review -> approved/rejected/needs_info`
- 已终态项不可再次操作
- 审核通过后需显式触发发布（不自动发布）
- 支持可见性手动裁决（`SetVisibilityVerdict`）

## 5. 内容状态机

```
draft ──submit──> in_review ──approve──> published
  │                    │                     │
  │<──reject───────────┘                     ├──offline──> archived
  │                                          │
  └──────────────────edit────────────────────┘
```

| 状态 | 说明 | 可执行操作 |
|------|------|------------|
| `draft` | 草稿，不可见 | 编辑、提交审核、删除 |
| `in_review` | 审核中 | 查看审核状态 |
| `published` | 已发布，可见 | 下架、提交修订、回滚版本 |
| `scheduled` | 定时发布 | 取消发布、编辑生效时间 |
| `offline` | 已下架 | 重新发布、归档 |
| `archived` | 已归档 | 仅查看 |

## 6. 权限与审计（前端约定）

- v3 不在前端做强权限判断，权限由网关/后端返回结果约束
- 页面必须展示后端回传的：
  - `meta.request_id`
  - `message`
  - 操作时间
- 操作日志字段：
  - `action`（如 `content.create`、`content.submit_review`、`content.publish`）
  - `target`（如 `guide_card_1001`）
  - `result`（`success`/`error`）
  - `request_id`
  - `timestamp`

## 7. 接口契约

仅使用以下 backoffice 路由：

### 7.1 联盟管理

- `POST /api/v2/backoffice/affiliate/partners`
- `POST /api/v2/backoffice/affiliate/partners/add`

### 7.2 内容管理

- `POST /api/v2/backoffice/content/items` — 内容列表
- `POST /api/v2/backoffice/content/items/add` — 新增内容
- `POST /api/v2/backoffice/content/items/update` — 更新内容
- `POST /api/v2/backoffice/content/items/detail` — 内容详情
- `POST /api/v2/backoffice/content/items/submit-review` — 提交审核
- `POST /api/v2/backoffice/content/items/publish` — 发布版本
- `POST /api/v2/backoffice/content/items/status` — 状态变更
- `POST /api/v2/backoffice/content/items/rollback` — 版本回滚

### 7.3 治理审核

- `POST /api/v2/backoffice/governance/reviews` — 审核队列
- `POST /api/v2/backoffice/governance/reviews/status` — 审核状态更新
- `POST /api/v2/backoffice/governance/visibility` — 可见性裁决

## 8. 页面布局

### 8.1 内容管理 Tab

```
┌─────────────────────────────────────────────────────┐
│ 内容管理                                            │
│ [新增内容] [刷新]                                   │
├─────────────────────────────────────────────────────┤
│ 过滤：[资源类型▼] [主题▼] [状态▼]                   │
├─────────────────────────────────────────────────────┤
│ 内容列表                                            │
│ ┌──────┬──────────┬──────┬────────┬────────┬──────┐ │
│ │ 标题 │ 类型     │ 主题 │ 状态   │ 版本   │ 操作 │ │
│ ├──────┼──────────┼──────┼────────┼────────┼──────┤ │
│ │ ...  │ ...      │ ...  │ draft  │ rev:3  │ 编辑 │ │
│ │      │          │      │        │ pub:2  │ 提交 │ │
│ ├──────┼──────────┼──────┼────────┼────────┼──────┤ │
│ │ ...  │ ...      │ ...  │pub     │ rev:5  │ 下架 │ │
│ │      │          │      │        │ pub:5  │ 回滚 │ │
│ └──────┴──────────┴──────┴────────┴────────┴──────┘ │
└─────────────────────────────────────────────────────┘
```

### 8.2 内容编辑页面（弹窗/侧边）

```
┌─────────────────────────────────────────┐
│ 新增/编辑内容                           │
├─────────────────────────────────────────┤
│ 资源类型  [guide_card ▼]                │
│ 标题      [________________]            │
│ 副标题    [________________]            │
│ 推荐摘要  [________________]            │
│             [________________]          │
│ 主题      [clothing ▼] [food ▼] ...     │
│ 封面URL   [________________]            │
│                                         │
│ --- guide_card 专属 ---                 │
│ 落地页URL [________________]            │
│ 外部商品ID[________________]            │
│ 联盟渠道  [+] PDD [_______]             │
│           [+] DOUYIN [_______]          │
│ 卖点      [+] [________________]        │
│ 商业披露  [x] 需要                      │
│                                         │
│ 初始状态  [draft ▼]                     │
│ 生效时间  [____-__-__ __:__]            │
│ 失效时间  [____-__-__ __:__]            │
│                                         │
│ [取消] [保存草稿] [保存并发布]          │
└─────────────────────────────────────────┘
```

## 9. 非目标（v3）

- 不做审批流编排（已有治理审核流程）
- 不做批量导入导出
- 不做复杂搜索 DSL
- 不做实时推送
- 不做富文本编辑器（使用基础 textarea，富文本为后续版本目标）

## 10. 接口与生命周期实现说明（v3）

### 10.1 Gateway ↔ 下游服务映射

| 后端接口 | 下游 RPC | 备注 |
|----------|----------|------|
| `POST /content/items/update` | `content_server.UpsertGuideCard` / `UpsertEditorialContent` 等 | 读取现有卡片 → 修改字段 → `expected_revision` 乐观锁写回 |
| `POST /content/items/detail` | `content_server.BatchGetGuideCards` | 加载指定 content_id 详情（含 cover_media、selling_points、affiliate_refs） |
| `POST /content/items/submit-review` | `content_server.SubmitForReview` | 资源类型 + 资源 ID + 版本进入审核队列 |
| `POST /content/items/publish` | `content_server.PublishRevision` + `governance.SetVisibilityVerdict(PUBLISHED)` | 状态变 published，治理侧标记可见 |
| `POST /content/items/rollback` | `content_server.UpsertGuideCard` | 基于当前 revision 写入新 revision（content_server 未提供 RollbackContentRevision，使用 Upsert 模拟新版本） |
| `POST /governance/visibility` | `governance_server.SetVisibilityVerdict` | 状态包括 PUBLISHED / RESTRICTED / UNPUBLISHED |

### 10.2 状态机（v3）

```
draft ──submit──> in_review ──publish──> published ──offline──> offline ──archive──> archived
  │                  │                     │
  │                  │                     └──rollback──> draft (新 revision)
  └─update (任意)────┴─────────────────────┘
```

- `update` 仅修改字段，不改变 `content_status`；当 `content_status=published` 时，前端会引导用户改用「提交修订」（submit-review）
- `submit-review` 实际进入 `in_review`，等待治理裁决后由 `publish` 推动到 `published`
- `rollback` 不真的"回退到旧版本"，而是把当前内容以新 revision 重写（content_server 暂未提供真正的版本回滚 RPC）

### 10.3 错误处理（v3）

前端 `HttpGatewayApiClient.request()` 区分三种错误：

1. **网络错误**（fetch throw）：`code=-1, message="网络请求失败"`
2. **HTTP 非 2xx**：
   - 响应体含 `Fail to find method`（brpc 默认错误）：`code=-2, message="该功能尚未实现：<path>"`
   - 其它：`code=response.status, message="请求失败: <body>"`
3. **业务错误**（HTTP 200 但 `success=false`）：透传 `code` 与 `message`

业务错误码（来自 Gateway）：

- `10002`：参数缺失（`content_id required` / `title and landing_url required`）
- `10003`：资源不存在
- `10004`：乐观锁冲突（revision mismatch）

### 10.4 封面图（v3）

- 优先使用本地粘贴/上传图片（Base64 Data URL）→ 写入 `cover_media.url`
- 其次使用 `cover_url`（URL 文本输入）
- 两者互斥，后端读取时按 `cover_media` → `cover_url` 顺序处理
