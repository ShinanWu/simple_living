# Gateway 数据与模型约定

## 1. Gateway 不维护业务真相模型

Gateway **没有** 独立的业务域实体表；不定义导购内容、推荐条目、订单、佣金等业务「主数据」结构。这些由 `platform/backoffice-backend`、`recommendation-server`、`tracking-server`、`user-server` 各自在 proto 与 `services/<service>/docs/data-model.md` 中定义。

本文档只描述 **gateway 边界上需要稳定约定的三类「薄」模型**：对外 JSON 视图、内部请求上下文、路由与映射元数据。

## 2. 对外 JSON 视图（canonical）

- **顶层响应**：严格对应 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc)。
- **分页**： [共享契约规则](../../../.cursor/rules/shared-contracts.mdc)。
- **错误码**： [共享契约规则](../../../.cursor/rules/shared-contracts.mdc)。
- **业务负载**：
  - 导购卡片展示：[共享契约规则](../../../.cursor/rules/shared-contracts.mdc)
  - 推荐结果：[共享契约规则](../../../.cursor/rules/shared-contracts.mdc)
  - 跳转与归因：[共享契约规则](../../../.cursor/rules/shared-contracts.mdc)
  - 用户态摘要字段（若返回）：[共享契约规则](../../../.cursor/rules/shared-contracts.mdc)

Gateway 的「数据模型」工作 = **将上述契约与内部 proto 字段对齐**，并在 [api.md](./api.md) / [pages.md](./pages.md) 中引用。

## 3. 请求上下文（逻辑模型）

以下为 **逻辑字段**，实现上可映射为 brpc metadata 或上下文结构体，名称可按代码规范调整，语义须一致：

| 逻辑字段 | 来源 | 用途 |
|----------|------|------|
| `request_id` | 客户端头或网关生成 | 响应 `meta`、日志、排障 |
| `trace_id` | 追踪系统注入/生成 | 分布式追踪串联 |
| `user_id` | 鉴权成功后解析令牌 | 下游 RPC 用户维度 |
| `is_guest` | 无登录态或访客令牌 | 下游降级与统计 |
| `client_platform` | 请求体字段（顶层或 `request_context`） | 限流、推荐 scene、审计 |
| `client_version` | 请求体字段（如 `app_version` 或 `request_context.app_version`） | 兼容性与灰度 |
| `device_id` | 请求体字段（顶层或 `request_context`） | 访客归因与风控辅助 |

**注意**：具体令牌解析、用户存在性校验由 **user-server** 等实现；gateway 只做入口校验与上下文注入，不在此文档重复账号模型。

## 4. 路由与映射元数据（实现侧）

实现层通常维护：

- **路由表**：HTTP method + path → handler → 一个或多个 RPC 调用描述（含超时、重试策略标记）。
- **错误映射表**：上游 RPC 错误 → 对外 `code` + `message` 模板。
- **字段映射表**（可选代码生成）：JSON 字段 ↔ proto 字段；变更需走文档与 [changelog.md](./changelog.md)。

以上不属于「业务数据模型」，但属于 gateway 工程必须版本化的配置/代码资产。

## 5. 持久化态：无业务表，仅 Redis 运行时状态

### 5.1 关系型存储（PostgreSQL）：无业务表

- Gateway **不拥有任何 PostgreSQL 业务表**：用户、内容、推荐、联盟、跳转、治理的权威状态全部在各业务域（见各域 `data-model.md`）。
- v1 网关侧**不**落地审计/配置类关系表；运营审计以下游域记录（如 content/governance 的 actor 字段与事件）+ 网关访问日志（见 [development.md](./development.md) 可观测性）承载。若未来需要网关侧审计表，**须先在本节补 schema 再实现**（先文档后开发）。
- backoffice 内容、发布态、审核结论、伙伴配置等运营数据**不得**保存在 gateway 内存或本地状态；gateway 只做 JSON↔proto 映射与跨域编排。

### 5.2 Redis 运行时状态（非权威，可重建）

Gateway 默认**无状态多实例**。Redis 仅承载可重建、可丢失的运行时状态，**绝不**作为跨实例业务真相；未配置 `-redis_addr` 时网关功能正确性不受影响（限流退化为单实例本地近似，幂等窗口退化为尽力而为）。

键统一前缀 `gw:`，分号分段，便于按业务隔离与批量过期。约定如下：

| 用途 | 键模式 | 值 | TTL | 说明 |
|------|--------|-----|-----|------|
| 限流计数 | `gw:rl:{dim}:{id}:{window_start}` | 整数计数（`INCR`） | = 限流窗口（如 10s / 60s） | `dim` ∈ `ip` / `user` / `session` / `route`；窗口对齐固定时间桶，超阈值返回 `10005` |
| 短期热点读缓存 | `gw:cache:{route}:{version}:{param_hash}` | 序列化 JSON 片段 | 5–60s（默认 30s） | 仅缓存公开、非个性化只读响应（如非个性化首页/详情）；`version` 随契约/卡片版本变更失效 |
| 访客会话映射缓存 | `gw:guest:{session_id}` | `device_id` / `client_platform` 等访客上下文快照 | ≤ 24h | user-server 为访客会话权威来源；此处仅作入口热缓存，未命中回源 `EnsureGuestSession` |
| 幂等去重窗口 | `gw:idem:{route}:{subject}:{client_event_id}` | 首次处理结果摘要或占位标记 | 5–15min（默认 10min） | 用于 `history/events`（`client_event_id`）、`feedback`（`client_request_id`）等去重；窗口外重放视为新请求 |
| 令牌内省短缓存（可选） | `gw:tok:{access_token_hash}` | 内省结果（`user_id`/`session_id`/过期点） | ≤ `access_token` 剩余有效期且 ≤ 60s | 降低 `IntrospectAccessToken` 调用；令牌撤销以 user-server 为准，TTL 取短以收敛失效延迟 |

约定与约束：

- 键设计要求：限流/缓存键必须包含**维度标识 + 版本/窗口戳**；缓存键的 `version` 必须随上游契约或卡片 `schema_version` 变化而失效。
- 一致性与失效以上游域为准；任何 Redis 命中都不得覆盖下游返回的权威结果。
- 不在文档层将上述运行时键定义为新的业务实体；它们是网关工程态，不进入对外 JSON 契约。
- Kafka：v1 网关**不**直接生产/消费领域事件；异步事件由各业务域负责（见各域 `workflow.md`）。

## 6. 并行开发依赖

| 角色 | 应阅读的模型来源 |
|------|------------------|
| 客户端 | `.cursor/rules/shared-contracts.mdc` + gateway `api.md` / `pages.md` |
| Gateway 开发 | 本文档 + 各域 proto + contracts |
| 域服务开发 | 本服务 proto + data-model；**不**依赖 gateway 内部映射表 |
