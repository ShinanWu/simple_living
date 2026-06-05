# Gateway 请求处理流程

## 1. 总览

单次客户端请求在 gateway 内建议经过以下阶段；实现可合并步骤，但 **语义不得缺失**：

```text
接入 → 解析上下文 → 鉴权 → 限流 → 路由 → (可选)并行 RPC → 聚合/映射 → 统一信封响应 → 观测落地
```

## 2. 阶段说明

### 2.1 接入与解析

1. 终止 TLS，解析 HTTP 路径、方法、query/body。
2. 读取 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) 约定头，生成或接受 `request_id`（与 `X-Request-Id` 关系见契约）。
3. 绑定或生成 `trace_id`，注入分布式追踪（与 Jaeger 等对齐时，`meta.trace_id` 可与之一致）。

### 2.2 鉴权（入口）

- 对标记为「需登录」的路由：校验 `Authorization`，失败返回约定错误码（如 `20002` / `20003`），响应仍使用 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) 信封。
- 访客允许的路由：无令牌或访客令牌均可，**下游仍需携带 `device_id` 等** 供推荐与风控使用。
- **不在 gateway 内实现注册登录业务流程**；令牌签发、刷新由 user-server 等提供 RPC，gateway 可调用轻量 `Introspect` 类接口（具体以 user-server 契约为准）。

### 2.3 限流

- 维度示例：IP、`user_id`、`device_id`、接口路径、`client_platform`。
- 触发限流：HTTP `429` 或业务信封 + 限流 `code`（项目内择一并文档化）；`message` 可读即可。
- 本地 Phase 1 限流统一在 gateway / brpc 实现；生产集群阶段由 Ingress / gateway / domain 三层协同，但业务限流语义仍以 gateway 和领域服务契约为准。

### 2.4 路由与参数校验

- 匹配路由表 → 绑定 handler。
- 对 JSON body / query 做 **语法与必填** 校验；业务规则校验仍以下游返回为准，但 **明显非法输入** 可在网关提前返回 `10002` 等校验类错误（见 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc)）。

### 2.5 下游调用与上下文传播

#### 2.5.1 metadata 注入（每个内部 RPC 必带）

`request_id`、`trace_id` 在接入阶段（§2.1）确定，并在**响应 `meta` 与每一次下游 brpc 调用**中一致透传：

- `request_id`：客户端 `X-Request-Id` 可参与生成，**最终值以网关为准**；写入响应 `meta.request_id` 与下游 metadata，全链路串联（见 [development.md](./development.md) 可观测性）。
- `trace_id`：绑定/生成后注入分布式追踪上下文与下游 metadata，使各域 span 归属同一 trace。
- 标准化 actor / 用户上下文：`user_id`（或空）、`is_guest`、`session_id`（访客）、`client_platform`、`client_version`、`device_id`（若存在）由网关从**鉴权头**解析后注入，下游据此做用户维度逻辑，不信任客户端伪造的 `user_id` 体字段。
- 键名与编码在实现中统一（建议 brpc 接受的 metadata 键，如 `x-request-id`/`x-trace-id`/`x-actor-user-id` 等），新增键须评估兼容性并在本文档或契约补充。

#### 2.5.2 超时预算（单 RPC）

| 调用类型 | 默认单 RPC 超时（`-rpc_timeout_ms`） | 重试（`-rpc_max_retry`） |
|----------|--------------------------------------|--------------------------|
| 幂等只读 RPC（`Get*`/`List*`/`BatchGet*`/`Query*`） | 600–800ms | ≤ 1（仅幂等读，退避后重试不同实例） |
| 写 / 状态变更 RPC（token、favorites、feedback、backoffice 写） | 800–1200ms | 0（不自动重试，依赖 idempotency 键去重） |
| 鉴权内省（`IntrospectAccessToken`/`IntrospectRefreshToken`） | 300–500ms | ≤ 1 |

单 RPC 超时必须 **≤ 所在路由级 deadline**；具体毫秒值由配置项覆盖（见 [development.md](./development.md)）。

#### 2.5.3 重试与降级

- **重试**：仅对幂等读且下游可安全重入时启用，次数 ≤ `-rpc_max_retry`，配退避；写操作默认 **0 重试**，由 idempotency 策略（见 `api.md` §11 与 [data-model.md](./data-model.md) §5.2 幂等窗口）保证去重。
- **降级**：依赖失败时按路由声明的失败策略（§2.6）选择「整页失败」或「字段降级 + `warnings`」；治理可见性过滤遵循 **fail-closed**（依赖异常时不返回应受限内容，见 `api.md` §13.2）。
- **熔断**：对持续失败/超时的下游可短路返回 `90002`/`90003`，避免线程与连接耗尽拖垮整入口。

### 2.6 页面聚合（BFF）

对 [pages.md](./pages.md) 与 [api.md](./api.md) §13 定义的路由：

1. 按依赖图 **并行** 发起多个 RPC（无强依赖时）。
2. 有依赖时（例如先取推荐 ID 列表再批量 hydration 卡片）：分阶段并行。
3. **超时预算**：整请求 deadline 应小于客户端超时，并对齐 [README.md](./README.md) §7.3 端到端目标；分阶段时各阶段预算之和 ≤ 路由 deadline，避免尾部延迟叠加。
4. **部分失败**：按下表逐路由声明的策略输出：要么 `success: false` 统一错误码（关键依赖失败用入口码 `10054`），要么 `success: true` + 字段降级 + 可选 `warnings`（`10053`）。

#### 2.6.1 逐路由聚合编排（与 `api.md` §13 一致）

| 路由 | 编排（阶段） | 关键依赖（失败即整页失败） | 可降级依赖（失败 → `warnings`/空） | 路由 deadline 目标 |
|------|--------------|----------------------------|-----------------------------------|--------------------|
| `GET /api/v2/pages/home_feed` | ① `Recommendation/QueryRecommendations` → ② `Content/BatchGetGuideCards` 批量 hydration → ③ 可选治理可见性过滤 | ①②（无推荐或无卡片则整页失败 `10054`） | ③ 治理增强（异常时 fail-closed 剔除受限项，可附 `warnings`） | ≤ 250ms |
| `GET /api/v2/pages/guide_detail` | ① `Content/BatchGetGuideCards`（单卡）∥ ② `Governance/BatchGetCooperationLabels`；③ 可选 `Recommendation/QueryRecommendations`（`include_related=true`） | ①（卡片不存在 → `30001`） | ②披露增强、③相关推荐（失败则 `related` 为空 + `warnings`） | ≤ 200ms |
| `POST /api/v2/pages/redirect_prepare` | ① `Tracking/AssembleTrackingLink`（内部含 affiliate 链接规格/签名） | ①（无可降级路径，失败 → `10054`） | 无 | ≤ 200ms |
| `GET /api/v2/pages/me_summary` | ① `User/GetMeSummary` | ① | 无 | ≤ 120ms |
| `POST /api/v2/backoffice/*` | 单域转发（content/affiliate/governance），不做跨域分布式写事务 | 目标域调用 | 无（写路径不降级） | ≤ 800ms |

并行分支共享同一 deadline；某分支为关键依赖时其超时即触发整页失败，可降级分支超时则按降级策略产出。

### 2.7 JSON ↔ proto 映射与响应组装

- 成功：将 proto 转为 JSON，字段 `snake_case`，过滤内部字段。
- 失败：映射 RPC 错误到对外 `code`/`message`；`data` 一般为 `null` 除非契约定义错误详情。
- 填充 `meta.request_id`、`meta.trace_id`、`meta.server_time_ms`。

### 2.8 观测与审计

- 访问日志：至少 path、method、status、`request_id`、耗时、上游服务结果概要（不含敏感 body）。
- 指标：QPS、延迟分位、错误率、限流次数、各下游熔断状态。
- 与架构中「可观测性内建」一致，细节在实现与运维文档展开。

## 3. 与错误处理的关系

- 客户端 **以 `code` 分支**，不以 HTTP 文案或裸 HTTP 码为唯一依据（与 contracts 一致）。
- Gateway **不**将内部异常栈、主机名、SQL 等返回给客户端。

## 4. 并行开发注意点

- 域服务应 **只依赖 metadata 语义**，不依赖 gateway 框架类型。
- 新增 metadata 键须评估兼容性，并在本文档或 contracts 补充说明。
