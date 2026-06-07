# 产品规格

运营管理平台的产品能力与业务规则。页面字段、按钮与 API 映射见 [detail-design.md](./detail-design.md)；架构与部署见 [README.md](./README.md)。

契约权威：[`gateway/docs/backoffice-backend.md`](../../gateway/docs/backoffice-backend.md)、[`.cursor/rules/shared-contracts.mdc`](../../../.cursor/rules/shared-contracts.mdc)。

## 1. 定位

### 1.1 目标用户

| 角色 | 典型任务 |
|------|----------|
| 内容运营 | 创建/编辑导购内容，送审，发布与下架 |
| 审核员 | 审核队列处理，可见性裁决 |
| 联盟运营 | 维护联盟伙伴与渠道 |
| 平台管理员 | 全部运营操作 |

### 1.2 产品目标

- **列表可读**：三域状态一目了然，可过滤刷新。
- **动作可执行**：核心操作在运营台内完成。
- **结果可追溯**：写操作展示 `request_id`，写入最近操作日志。
- **失败可恢复**：单域失败不阻塞其他域；失败保留输入可重试。
- **治理门闸**：审核通过不自动发布；C 端可见性由治理裁决最终决定。

### 1.3 边界

| 负责 | 不负责 |
|------|--------|
| 三域运营编排与呈现（`backoffice-web`） | 业务持久化 → `backoffice-backend` |
| 经 gateway 调用 `/api/v2/backoffice/*` | JSON↔proto、鉴权 → `gateway` |
| | C 端读/推荐 → `recommendation-server` |
| | 转链执行 → `tracking-server` |
| | 用户账户 → `user-server` |

## 2. 功能模块

运营台（Operations Console）单页三 Tab + 操作日志侧栏：

| 模块 | 能力 | API 域 |
|------|------|--------|
| 内容管理 | 列表/过滤、CRUD、送审、发布、下架、归档、回滚 | `content` |
| 治理审核 | 审核队列、通过/拒绝、可见性裁决 | `governance` |
| 联盟管理 | 伙伴列表、新增 | `affiliate` |
| 最近操作 | 写操作摘要（action、target、request_id） | 各写接口 `meta` |

紧急不可见等风控在阶段一经 **可见性裁决**（`restricted` / `unpublished`）表达。

## 3. 角色与权限

由 gateway 校验；UI 不做强判权，按接口结果约束。

| 角色 | 权限 |
|------|------|
| `backoffice_admin` | 全部 |
| `content_operator` | 内容创建/编辑/送审/发布/下架/回滚 |
| `reviewer` | 审核与可见性裁决 |
| `partner_operator` | 伙伴管理 |

由 gateway 校验 Bearer token（`-backoffice_api_token`，默认 `simple-living-ops`）；UI 登录页换取 token 后持久化到 localStorage。

## 4. 状态与门闸

### 4.1 内容生命周期（`content_status`）

```
draft ──送审──> in_review ──显式发布──> published ──下架──> offline ──归档──> archived
  ^                │
  └──拒绝/补材──────┘（仍 in_review 直至发布；审核通过不自动 published）
```

### 4.2 审核单（`review status`）

`pending` → `in_review` → `approved` / `rejected` / `needs_info` → `completed`。  
`approved` **不**改变内容为 `published`。

### 4.3 可见性（`visibility_verdict`，与 content_status 正交）

| `state` | 含义 |
|---------|------|
| `published` | 允许 C 端展示（还须 content 已发布） |
| `restricted` | 限制展示 |
| `unpublished` | 不可见 |

**双门闸**：`content_status=published` 且 `visibility=published` 时 C 端才可见（fail-closed）。

## 5. 交付范围

### 阶段一（v1 正式版）

- 运营登录（`POST /api/v2/backoffice/auth/login`，Bearer 保护写接口）
- 三 Tab 运营台：`guide_card` 全生命周期（创建 → 送审 → 审核 → 显式发布 → 下架/归档）
- 治理可见性裁决，导出 `visibility_index` 读治理表（C 端 fail-closed）
- 联盟增查持久化（PostgreSQL，非 gateway 内存）
- 公网 `/backoffice/` + 同源 `/api/v2/backoffice/*`

### 阶段一不含

批量导入导出、定时发布 UI、四资源类型完整编辑、富文本、独立风控台

### 阶段二

运营登录、全资源类型 UI、审计落库、素材库/CDN

## 6. 审计

- **页面**：最近操作日志（`action` / `target` / `request_id`）
- **后端**：gateway → backoffice-backend 权威事件（见 gateway 文档 §8）

## 7. 错误语义

UI 展示四类错误（网络 / 未实现 / HTTP / 业务），不暴露栈。常见码：`10002` 校验、`10003` 不存在、`10007` 版本冲突。
