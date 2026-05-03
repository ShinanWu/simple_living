# backoffice-web 详细设计（v2）

## 1. 设计目标

在 `affiliate/content/governance` 三域上提供可直接运行的运营工作台，满足：

- 列表可读：运营可快速查看当前业务状态
- 动作可执行：核心操作可在页面内直接完成
- 结果可追溯：每次操作都能看到请求结果与审计线索
- 失败可恢复：接口失败时可重试、可继续其他模块操作

## 2. 信息架构

单页三模块：

1. `联盟管理`：伙伴增查
2. `内容管理`：内容状态切换
3. `治理审核`：审核状态处理

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
- 所有写操作：写入“最近操作日志”

## 4. 字段与动作约束

### 4.1 联盟管理

- 输入：`partner_id`（可自动从名称生成）、`display_name`、`status`、`primary_channel_code`
- 校验：
  - `display_name` 必填，去首尾空白后不能为空
  - `partner_id` 仅小写字母、数字、下划线

### 4.2 内容管理

- 展示字段：`content_id`、`title`、`theme`、`status`
- 动作：`published <-> draft` 切换（v2 维持最小状态机）
- 扩展位：预留更多状态（`in_review/offline/archived`）但不在 v2 强制启用

### 4.3 治理审核

- 展示字段：`review_id`、`subject_id`、`status`
- 动作：`pending -> approved/rejected`
- 已终态项不可再次操作

## 5. 权限与审计（前端约定）

- v2 不在前端做强权限判断，权限由网关/后端返回结果约束
- 页面必须展示后端回传的：
  - `meta.request_id`
  - `message`
  - 操作时间
- 操作日志字段：
  - `action`（如 `partner.create`）
  - `target`（如 `pdd`）
  - `result`（`success`/`error`）
  - `request_id`
  - `timestamp`

## 6. 接口契约

仅使用以下 backoffice 路由：

- `GET /api/v2/backoffice/affiliate/partners`
- `POST /api/v2/backoffice/affiliate/partners/add`
- `GET /api/v2/backoffice/content/items`
- `PATCH /api/v2/backoffice/content/items/status`
- `GET /api/v2/backoffice/governance/reviews`
- `PATCH /api/v2/backoffice/governance/reviews/status`

## 7. 非目标（v2）

- 不做审批流编排
- 不做批量导入导出
- 不做复杂搜索 DSL
- 不做实时推送
