# Gateway 数据与模型约定

## 1. Gateway 不维护业务真相模型

Gateway **没有** 独立的业务域实体表；不定义导购内容、推荐条目、订单、佣金等业务「主数据」结构。这些由 `content-domain`、`recommendation-domain`、`affiliate-domain`、`tracking-domain`、`user-domain`、`governance-domain` 各自在 proto 与 `services/<domain>/docs/data-model.md` 中定义。

本文档只描述 **gateway 边界上需要稳定约定的三类「薄」模型**：对外 JSON 视图、内部请求上下文、路由与映射元数据。

## 2. 对外 JSON 视图（canonical）

- **顶层响应**：严格对应 [common-response.md](../../../docs/contracts/common-response.md)。
- **分页**： [pagination.md](../../../docs/contracts/pagination.md)。
- **错误码**： [error-codes.md](../../../docs/contracts/error-codes.md)。
- **业务负载**：
  - 导购卡片展示：[guide-card.md](../../../docs/contracts/guide-card.md)
  - 推荐结果：[recommendation.md](../../../docs/contracts/recommendation.md)
  - 跳转与归因：[redirect-attribution.md](../../../docs/contracts/redirect-attribution.md)
  - 用户态摘要字段（若返回）：[auth.md](../../../docs/contracts/auth.md) 第 5 节

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

**注意**：具体令牌解析、用户存在性校验由 **user-domain** 等实现；gateway 只做入口校验与上下文注入，不在此文档重复账号模型。

## 4. 路由与映射元数据（实现侧）

实现层通常维护：

- **路由表**：HTTP method + path → handler → 一个或多个 RPC 调用描述（含超时、重试策略标记）。
- **错误映射表**：上游 RPC 错误 → 对外 `code` + `message` 模板。
- **字段映射表**（可选代码生成）：JSON 字段 ↔ proto 字段；变更需走文档与 [changelog.md](./changelog.md)。

以上不属于「业务数据模型」，但属于 gateway 工程必须版本化的配置/代码资产。

## 5. 缓存与临时状态

若 gateway 引入缓存（如热点卡片、公开展示内容）：

- 缓存键必须包含 **版本或与契约兼容的版本戳**；
- 失效与一致性以上游域为准；
- 不在文档层将缓存对象定义为新的业务实体。

## 6. 并行开发依赖

| 角色 | 应阅读的模型来源 |
|------|------------------|
| 客户端 | `docs/contracts/*` + gateway `api.md` / `pages.md` |
| Gateway 开发 | 本文档 + 各域 proto + contracts |
| 域服务开发 | 本服务 proto + data-model；**不**依赖 gateway 内部映射表 |
