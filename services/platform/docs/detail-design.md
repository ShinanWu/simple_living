# 详细设计

运营管理平台的实现级设计：运营台 UI、gateway API 映射、后端写链路与代码位置。产品规则摘要见 [product-spec.md](./product-spec.md)。

## 1. 端到端结构

```text
运营人员浏览器
  GET  /backoffice/*     → backoffice-web（静态 SPA，:8088）
  POST /api/v2/backoffice/* → gateway（:8080）→ backoffice-backend（:9110）→ PostgreSQL
  发布/可见性/联盟变更后 → export active/ → recommendation-server / tracking-server
```

| 层 | 实现目录 | 文档 |
|----|----------|------|
| UI | `backoffice-web/` | 本章 §2–§5 |
| 对外契约 | `gateway/` | [backoffice-backend.md](../../gateway/docs/backoffice-backend.md) |
| 写面后端 | `backoffice-backend/` | [backend-workflow.md](./backend-workflow.md)、[backend-api.md](./backend-api.md) |

## 2. 运营台页面（backoffice-web）

**入口**：`/backoffice/` · 单页 `OperationsConsolePage` · 无 React Router。

```
┌─ 标题区 + 刷新全部 ─────────────────────────────────────┐
├─ 指标卡：已发布内容 / 审核待办 / 活跃伙伴 ────────────────┤
├─ Tab：[内容管理] [治理审核] [联盟管理] ──────────────────┤
│       （当前 Tab 工作区）                                  │
├─ 侧栏：最近操作（≤12 条，含 request_id）─────────────────┤
└─ 浮层：内容详情抽屉（基本信息 / 版本 / 可见性 / 审核）──┘
```

**全局状态**：首次三域并发加载；各域独立 `loading|success|empty|error`；顶部 banner 反馈写操作结果。

### 2.1 内容管理

**列表**：过滤 resource_kind、theme、content_status。列：标题、类型、主题、状态、版本、更新时间、操作。

| `content_status` | 操作 |
|------------------|------|
| `draft` | 详情、编辑、送审、删除（归档） |
| `in_review` | 详情 |
| `published` | 详情、提交修订、发布、下架、回滚（详情内） |
| `offline` | 详情、发布、归档 |
| `archived` | 详情 |

**编辑（阶段一）**：`guide_card` 字段完整；`editorial_content` / `topic` / `ranking_list` 可选类型但无专属表单。

- 通用：title（必填）、subtitle、summary、theme_ids、cover（URL 或 Base64）
- guide_card：landing_url（必填）、external_item_id、affiliate_refs、selling_points、commercial_disclosure_required

**详情抽屉**：版本历史与回滚确认；可见性只读 + 三个裁决按钮；审核状态（若有）。

**流程**：送审 → `in_review` → 治理通过（仍 in_review）→ 运营显式发布 → `published` → 后端导出 snapshot。

### 2.2 治理审核

**审核队列**：过滤 status；对 `pending`/`in_review` 可「通过」「拒绝」（`needs_info` 待补）。

**可见性表单**（Tab 底部）：输入 content_id、state（published/restricted/unpublished）、可选 reason_code；restricted/unpublished 二次确认。

**详情抽屉内可见性**：允许展示 / 限制展示 / 紧急不可见，同上 API。

### 2.3 联盟管理

表单：display_name（必填）→ 自动生成 partner_id、status、primary_channel_code。表格展示伙伴列表。

### 2.4 操作日志

写操作追加：`action`（如 `content.publish`、`visibility.set`）、`target`、`result`、`request_id`、时间。

## 3. Gateway API 映射

完整 JSON 定义见 [backoffice-backend.md](../../gateway/docs/backoffice-backend.md)。

| 运营动作 | HTTP | UI 调用 |
|----------|------|---------|
| 伙伴列表 | `POST .../affiliate/partners` | `getBackofficePartners` |
| 新增伙伴 | `POST .../affiliate/partners/add` | `postBackofficePartner` |
| 内容列表 | `POST .../content/items` | `getBackofficeContents` |
| 新增 | `POST .../content/items/add` | `postBackofficeContentItem` |
| 更新 | `POST .../content/items/update` | `updateBackofficeContent` |
| 详情 | `POST .../content/items/detail` | `getBackofficeContentDetail` |
| 送审 | `POST .../content/items/submit-review` | `submitBackofficeContentReview` |
| 发布 | `POST .../content/items/publish` | `publishBackofficeContent` |
| 状态 | `POST .../content/items/status` | `patchBackofficeContentStatus` |
| 回滚 | `POST .../content/items/rollback` | `rollbackBackofficeContent` |
| 审核列表 | `POST .../governance/reviews` | `getBackofficeReviews` |
| 审核裁决 | `POST .../governance/reviews/status` | `patchBackofficeReview` |
| 可见性 | `POST .../governance/visibility` | `setBackofficeGovernanceVisibility` |

类型镜像：`backoffice-web/src/gateway/types.ts`。

## 4. 后端写链路（backoffice-backend）

进程内三模块协作；gateway 不做跨域事务。

### 4.1 内容：创作 → 审核 → 发布

```text
gateway content/* → content 模块（Upsert / SubmitForReview / PublishRevision）
                  → governance（审核、SetVisibilityVerdict）
                  → 事务 + outbox → 导出 catalog_snapshot + visibility_index
                  → recommendation-server 热加载
```

### 4.2 紧急不可见

```text
gateway governance/visibility → SetVisibilityVerdict
                             → 优先 patch visibility_index（目标 ≤1s 热切换）
```

### 4.3 联盟

```text
gateway affiliate/* → affiliate 模块（UpsertPartner 等）
                    → 导出 affiliate_link_spec（无 signing secret）→ tracking-server
```

模块 RPC 与表结构见 [backend-api.md](./backend-api.md)、[backend-data-model.md](./backend-data-model.md)。

## 5. 实现索引

| Concern | 路径 |
|---------|------|
| 运营台页面 | `backoffice-web/src/pages/OperationsConsolePage.tsx` |
| 样式 | `backoffice-web/src/pages/OperationsConsolePage.css` |
| HTTP 客户端 | `backoffice-web/src/gateway/client.ts` |
| 后端入口 | `backoffice-backend/src/backoffice_backend_server_main.cpp` |
| content / governance / affiliate 服务 | `backoffice-backend/src/*_services.cpp` |
| gateway 路由注册 | `gateway/src/gateway_edge_server_main.cpp` |

## 6. 阶段差距

| 规格 | 现状 |
|------|------|
| 四资源类型完整编辑 | 仅 guide_card |
| 审核「补充材料」按钮 | 未做 |
| scheduled 定时发布 UI | 未做 |
| 版本回滚真实恢复 | 简化实现 |
| 审核行一键填 visibility content_id | 未做 |

## 7. 部署验收

公网 `http://<入口>/backoffice/`：

1. 登录页输入访问令牌（默认 lab：`simple-living-ops`）
2. 三 Tab 与指标卡正常
3. 新增 guide_card 草稿 → 送审 → 治理通过 → **仍为 in_review** → 显式发布 → published
4. 可见性裁决为「不可见」后 C 端不再展示
5. 新增联盟伙伴重启后仍在列表中

部署步骤见 [README.md](./README.md) §部署。
