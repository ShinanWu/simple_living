# Gateway 请求处理流程

## 1. 总览

单次客户端请求在 gateway 内建议经过以下阶段；实现可合并步骤，但 **语义不得缺失**：

```text
接入 → 解析上下文 → 鉴权 → 限流 → 路由 → (可选)并行 RPC → 聚合/映射 → 统一信封响应 → 观测落地
```

## 2. 阶段说明

### 2.1 接入与解析

1. 终止 TLS，解析 HTTP 路径、方法、query/body。
2. 读取 [auth.md](../../../docs/contracts/auth.md) 约定头，生成或接受 `request_id`（与 `X-Request-Id` 关系见契约）。
3. 绑定或生成 `trace_id`，注入分布式追踪（与 Jaeger 等对齐时，`meta.trace_id` 可与之一致）。

### 2.2 鉴权（入口）

- 对标记为「需登录」的路由：校验 `Authorization`，失败返回约定错误码（如 `20002` / `20003`），响应仍使用 [common-response.md](../../../docs/contracts/common-response.md) 信封。
- 访客允许的路由：无令牌或访客令牌均可，**下游仍需携带 `device_id` 等** 供推荐与风控使用。
- **不在 gateway 内实现注册登录业务流程**；令牌签发、刷新由 user-domain 等提供 RPC，gateway 可调用轻量 `Introspect` 类接口（具体以 user-domain 契约为准）。

### 2.3 限流

- 维度示例：IP、`user_id`、`device_id`、接口路径、`client_platform`。
- 触发限流：HTTP `429` 或业务信封 + 限流 `code`（项目内择一并文档化）；`message` 可读即可。
- 当前阶段限流统一在 gateway / brpc 实现；暂不启用 Ingress 层业务限流，避免多层语义分裂。

### 2.4 路由与参数校验

- 匹配路由表 → 绑定 handler。
- 对 JSON body / query 做 **语法与必填** 校验；业务规则校验仍以下游返回为准，但 **明显非法输入** 可在网关提前返回 `10002` 等校验类错误（见 [error-codes.md](../../../docs/contracts/error-codes.md)）。

### 2.5 下游调用与上下文传播

对每个内部 RPC：

- **必须**携带 metadata：`request_id`、`trace_id`、`user_id`（或空）、`is_guest`、`client_platform`、`client_version`、`device_id`（若存在）等，键名与编码方式在实现中统一（建议使用 brpc 接受的 metadata 键）。
- **超时**：单 RPC 超时 ≤ 路由级预算；页面聚合见下节。
- **重试**：仅对幂等读且安全的情况可配置重试；写操作默认不重试或由 idempotency 策略约束。

### 2.6 页面聚合（BFF）

对 [pages.md](./pages.md) 定义的路由：

1. 按依赖图 **并行** 发起多个 RPC（无强依赖时）。
2. 有依赖时（例如先取 ID 列表再批量取详情）：分阶段并行。
3. **超时预算**：整请求 deadline 应小于客户端超时；子调用分摊时间，避免尾部延迟叠加。
4. **部分失败**：按该页面在 `pages.md` 声明的策略输出：要么 `success: false` 统一错误码，要么 `success: true` + 部分字段降级 + 可选 `warnings`。

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
