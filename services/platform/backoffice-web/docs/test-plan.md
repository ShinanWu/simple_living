# backoffice-web 测试与验收计划（v3）

## 1. 测试范围

- 页面渲染与交互（unit/component）
- 网关契约调用与错误处理（integration-lite）
- QEMU 部署链路（smoke）

## 2. 单元测试最低覆盖

### 2.1 联盟管理

1. 页面可渲染三域 tab
2. 联盟新增成功后列表更新
3. 接口失败时展示错误提示并允许重试

### 2.2 内容管理

4. 内容列表加载并正确展示各字段（content_id、resource_kind、title、status、revision 等）
5. 内容新增成功后列表更新
6. 内容编辑（update）提交正确请求体
7. 内容状态切换后列表更新（draft→in_review→published→offline→archived）
8. 提交审核后状态正确变为 in_review
9. 发布操作调用正确 API 并更新列表
10. 版本回滚操作调用正确 API
11. 内容详情加载展示版本历史和审核状态
12. 根据内容状态动态展示正确操作按钮
13. 表单校验：title 为空时阻止提交
14. 表单校验：guide_card 缺少 landing_url 时阻止提交
15. 表单校验：URL 格式不正确时阻止提交
16. 资源类型切换时正确展示/隐藏专属字段
17. 列表过滤按 resource_kind、theme、status 正确过滤

### 2.3 治理审核

18. 审核通过/拒绝后列表更新
19. 已终态项不显示操作按钮
20. 审核操作包含 review_id 和 resource_kind

### 2.4 操作日志

21. 操作日志追加并包含 `request_id`
22. 内容操作日志 action 包含正确语义（content.create、content.update、content.submit_review、content.publish、content.status_changed、content.rollback）

## 3. 手工回归清单

### 3.1 通用

- 首次加载：三域列表均可见
- 切 tab：状态不串扰
- 全局刷新：三域列表同时刷新
- 操作日志：每次写操作正确记录

### 3.2 联盟管理

- 新增伙伴：输入合法时成功、非法时阻止提交
- 伙伴列表：展示 partner_id、display_name、status、channel

### 3.3 内容管理

- 内容列表：展示所有字段，包括 resource_kind、title、status、revision
- 过滤功能：按资源类型、主题、状态过滤正确
- 新增内容：
  - guide_card：填写必填字段后成功创建
  - 缺少 title 或 landing_url 时阻止提交
  - URL 格式不正确时阻止提交
- 内容编辑：
  - 点击编辑打开编辑视图
  - 修改后保存成功
- 状态流转：
  - draft 状态：显示"编辑"、"提交审核"按钮
  - in_review 状态：显示"查看审核状态"，不可编辑
  - published 状态：显示"下架"、"回滚"按钮
  - offline 状态：显示"重新发布"、"归档"按钮
  - archived 状态：仅展示，无操作按钮
- 提交审核：
  - 弹出确认对话框
  - 成功后状态变为 in_review
- 发布：
  - 检查治理可见性
  - 成功后状态变为 published
- 版本回滚：
  - 打开详情抽屉查看版本历史
  - 选择历史版本回滚
  - 回滚后生成新版本

### 3.4 治理审核

- 审核动作：仅 pending/in_review 项显示操作按钮
- 通过/拒绝：成功后列表更新
- 已处理项显示"已处理"，不可再次操作

### 3.5 错误恢复

- 列表加载失败：显示错误提示，可点击刷新
- 写操作失败：显示错误提示，保留输入，允许重试

## 4. QEMU 验收步骤

1. 节点启动与检查
   - `bash services/platform/backoffice-web/deploy/start_nodes.sh`
   - `bash services/platform/backoffice-web/deploy/check_nodes.sh`
2. 构建并部署
   - `bash services/platform/backoffice-web/deploy/deploy_service.sh`
3. 基础可达性
   - `curl -fsS "http://127.0.0.1:8088" | sed -n '1,10p'`
4. 网关联通性（至少命中一个 backoffice API）
   - 页面执行"新增伙伴"并确认列表更新
   - 页面执行"新增内容"并确认列表更新
   - 页面执行"提交审核"并确认状态流转
   - 记录一次 `request_id` 用于追踪

## 5. 内容管理专项验收

### 5.1 完整生命周期测试

1. 创建 guide_card 草稿
2. 编辑草稿内容
3. 提交审核 → 状态变为 in_review
4. 在治理审核模块通过审核
5. 发布内容 → 状态变为 published
6. 下架内容 → 状态变为 offline
7. 归档内容 → 状态变为 archived

### 5.2 版本管理测试

1. 创建内容并发布（revision=1, published_revision=1）
2. 编辑内容并提交审核（revision=2）
3. 审核通过并发布（revision=2, published_revision=2）
4. 回滚到 revision=1
5. 确认回滚后生成新版本号

### 5.3 治理联动测试

1. 提交审核后，治理审核队列出现对应审核单
2. 审核通过后，内容状态仍为 in_review（不自动发布）
3. 显式触发发布后，内容状态变为 published
4. 可见性裁决可手动设置

## 6. 关键流程 E2E 上线门槛

上线前必须在「连真实 gateway」的环境（QEMU lab 或预发）跑通以下端到端路径，每条记录一次 `request_id` 以便对账。任一条失败即阻塞上线。

| # | 流程 | 步骤要点 | 通过判据 |
|---|------|----------|----------|
| E1 | 内容发布（全生命周期） | 新增 `guide_card` 草稿 → 编辑 → 提交审核 → 治理通过 → 显式发布 → 下架 → 归档 | 各步状态正确流转（`draft→in_review→published→offline→archived`），`published_revision` 更新，审核通过后 **不自动发布**，发布后 `visibility_state=published` |
| E2 | 审核通过 / 拒绝 | 提交审核后在治理模块分别执行「通过」与「拒绝」 | 通过：审核单 `approved`、内容仍 `in_review` 待显式发布；拒绝：审核单 `rejected`、内容回到可编辑态；已终态项不再显示操作按钮 |
| E3 | 伙伴配置 | 新增合法伙伴、提交非法（空 `display_name`）被阻止 | 合法：列表新增成功并展示 `partner_id/display_name/status/channel`；非法：前端阻止提交 |
| E4 | 审计查询 | 完成上述写操作后查看「最近操作日志」 | 每次写操作均落条目，含正确 `action` 语义、`target`、`result` 与后端回传的 `request_id`（必要时 `trace_id`） |
| E5 | 版本回滚 | 发布两个版本后回滚到历史版本 | 回滚调用 `rollback` 并生成新 `revision`，详情抽屉版本历史可见 |
| E6 | 错误恢复 | 触发一次失败请求（如缺参 `10002` / 版本冲突） | 展示可读错误提示、保留输入、可重试；不暴露后端栈信息 |

## 7. 交付门槛

- 文档与实现一致
- 单元测试通过（在可用 Node 环境）
- QEMU 部署与基础访问通过
- 至少完成三域各一条真实可操作路径
- 内容管理完整生命周期路径通过
- §6 关键流程 E2E 上线门槛 E1–E6 全部通过

## 8. v3 端到端验收记录（2026-06-01）

### 8.1 部署链路

- ✅ Gateway（`simple-living-gateway`）已构建并部署，监听 `8080`
- ✅ backoffice-web（`simple-living-backoffice-web`）已构建并部署，监听 `8088`
- ✅ Nginx 反代 `/backoffice/` → backoffice-web，`/api/v2/backoffice/*` → gateway

### 8.2 Gateway 接口冒烟（curl）

| 接口 | 路径 | 结果 |
|------|------|------|
| 创建内容 | `POST /api/v2/backoffice/content/items/add` | `success=true`，返回新 content_id + 完整列表 |
| 更新内容 | `POST /api/v2/backoffice/content/items/update` | `success=true`，title 字段被正确更新 |
| 详情 | `POST /api/v2/backoffice/content/items/detail` | `success=true`，返回单条详情 |
| 提交审核 | `POST /api/v2/backoffice/content/items/submit-review` | `success=true`，`review_id=qi_*`，`content_status=in_review` |
| 发布 | `POST /api/v2/backoffice/content/items/publish` | `success=true`，`published_revision` 已更新 |
| 回滚 | `POST /api/v2/backoffice/content/items/rollback` | `success=true`，`new_revision` 已更新 |
| 治理可见性 | `POST /api/v2/backoffice/governance/visibility` | `success=true`，`visibility_state=published` |
| 错误路径 | 缺失 `content_id` | 返回 `code=10002` `message="content_id required"`，HTTP 400 |
| 错误路径 | `content_id="test"` 不存在 | 返回 `code=10003` `message="content not found"`，HTTP 404 |

### 8.3 前端集成要点

- 入口域名：`http://<FRP_CUSTOM_DOMAIN>/backoffice/`（见 `environments/local-qemu/nodes.env`）
- `HttpGatewayApiClient.request()` 区分 `网络错误(-1)` / `接口未实现(-2)` / `HTTP错误(status)` / `业务错误(code)`，用户看到的不再是统一的"网络请求失败"
- 表单提交失败时，错误信息回填到列表页右上角的全局提示，便于运营直接看到

### 8.4 已知遗留（v3）

- `content_server` 未提供 `RollbackContentRevision` / `UpdateContentStatus` RPC，当前 `rollback` 用 `UpsertGuideCard` 写新 revision 模拟
- `publish` 后写入 visibility 是 fire-and-forget 异步，治理侧失败不会回滚发布结果
- 前端"批量操作""富文本编辑器"按 v3 设计目标不实现
