# Gateway 文档变更记录

本文档记录 **gateway 服务契约与文档** 的变更，便于多端与多域并行开发时对齐版本。

## 版本说明

- **文档版本**与运行中的 gateway 二进制版本可不同步，但对外承诺以 **已发布的文档 + contracts** 为准。
- 破坏性变更必须递增对外 API 版本路径（如 `/api/v2`）或经过书面迁移期（在本 changelog 说明）。

---

## [1.0.6] - 2026-04-20

### 文档补充（运营后台最小闭环）

- `api.md` 新增 `/api/v2/backoffice/*` 最小路由组说明：联盟伙伴、内容发布态、治理审核队列。
- `pages.md` 在页面聚合总表新增“运营后台（最小）”条目，明确仍由 gateway 作为 BFF 分路由到 `affiliate-domain` / `content-domain` / `governance-domain`。
- 保持网关边界不变：不承接跨域事务，不转移业务所有权。

### 实现同步（运营后台 API）

- `proto/gateway_pages_edge.proto` 新增 backoffice 聚合消息与 RPC：伙伴管理、内容状态、审核队列。
- `src/gateway_edge_server_main.cpp` 新增 `/api/v2/backoffice/*` 路由映射与处理逻辑，提供最小可用运营后台后端能力（POST-only）。

### 文档补充（先文档后开发）

- 新增 `backoffice-backend.md`：明确运营管理后台后端目标、v1 路由、对象定义、审核/发布状态机、权限模型与审计要求。
- 术语重命名：`ops` 统一更名为 `backoffice`，避免与运维（Ops/SRE）概念混淆。

---

## [1.0.5] - 2026-04-14

### 契约调整（头部最小化，body 为业务真源）

- HTTP 头收敛为鉴权优先：业务上下文不再依赖 `X-Client-*` 头。
- `RefreshTokenHttpRequest` 恢复 `request_context` 入参，作为请求体业务上下文来源。
- `src/gateway_edge_server_main.cpp` 调整为：刷新令牌链路仅读取体内 `request_context`，不再从请求头补齐。
- 保留“身份主体只来自鉴权头”的规则，不恢复体内 `acting_user_id` / `session_id`。

---

## [1.0.4] - 2026-04-14

### 契约重构（单一真源）

- 移除“请求头 + 体内字段融合”策略：身份主体与客户端上下文统一由请求头提供，不再接受体内 `request_context`。
- `gateway_user_http_messages.proto` 删除冲突来源字段：`RefreshTokenHttpRequest.request_context` 以及历史/反馈/收藏相关请求中的 `acting_user_id`、`session_id` 入口。
- `src/gateway_edge_server_main.cpp` 改为仅基于请求头解析主体与上下文，移除体内主体覆盖逻辑，消除字段优先级歧义。
- `api.md` 与示例请求同步更新为“请求头唯一真源”规范。

---

## [1.0.3] - 2026-04-08

### 实现同步

- `proto/gateway_user_edge.proto` / `gateway_user_http_messages.proto`：对齐对外 JSON（`account_proof`、`guest` 的 `client_platform` 字符串、`SubmitFeedbackHttpRequest.target_type` 字符串、`ClearHistoryHttpRequest.scope` 字符串等）；为 brpc **restful 路径唯一**约束增加 `MeProfileHttp` / `MePreferencesHttp` / `MeHistoryHttp` / `MeConsentHttp`（在 handler 内按 HTTP 方法分发）；新增 `GetHealth`（`HealthCheckRequest`/`Response`）。
- `src/gateway_edge_server_main.cpp`：注册 `/api/v2/...` restful 映射；通过预填 `Controller::response_attachment()` 输出与 `docs/contracts/common-response.md` 一致的 JSON 信封；Bearer / `X-Guest-Session-Id` 解析并注入下游 `user_id`/`session_id`（不信任客户端伪造的登录主体字段）。
- 对外路由进一步统一为 **POST-only**：使用动作化路径区分语义（如 `/api/v2/me/profile/get|update`、`/api/v2/me/favorites/list|add|remove`、`/api/v2/auth/session/revoke`、`/api/v2/health/check`），旧的 GET/PUT/PATCH/DELETE 语义入口不再接受；`favorite_id` 改为 `remove` 请求体字段。

---

## [1.0.2] - 2026-04-06

### 更新

- `README.md`：新增“集群内地址规范（下游 brpc）”，默认约定 `brpc://<service>.simple-living.svc.cluster.local:<port>`，并补充本地联调覆盖策略（flag/env）。
- `development.md`：对齐运行说明，明确集群内默认地址与本地 `127.0.0.1` 覆盖方式。

### 实现同步

- `src/gateway_edge_server_main.cpp`：下游默认地址切换到 K8s Service DNS（`user/content/recommendation/tracking`），并新增环境变量覆盖（仅在未显式传入 flag 时生效）：
  - `GATEWAY_USER_DOMAIN_ADDR`
  - `GATEWAY_CONTENT_DOMAIN_ADDR`
  - `GATEWAY_RECOMMENDATION_DOMAIN_ADDR`
  - `GATEWAY_TRACKING_DOMAIN_ADDR`

## [1.0.1] - 2026-03-29

### 新增

- `README.md`：§6「对外协议实现说明」——终端 HTTPS+JSON、`gateway/proto` Edge 为逻辑边界、须含 HTTP 接入层；引用 [`docs/engineering-conventions.md`](../../../docs/engineering-conventions.md)。
- `development.md`：Gateway 实现与交付（文档阅读顺序、Bazel、联调 flags、测试、合并前检查）。

### 说明

- 跨服务工程约定集中在 [`docs/engineering-conventions.md`](../../../docs/engineering-conventions.md)；各服务在 `services/<service>/docs/development.md` 说明本域实现与交付细节。

## [1.0.0] - 2026-03-28

### 新增

- 初始化 `services/gateway/docs/` 文档集：`README.md`、`api.md`、`pages.md`、`data-model.md`、`workflow.md`、`changelog.md`。
- 明确 gateway **边界层** 定位：统一对外入口、鉴权入口、路由、限流、JSON ↔ proto 翻译、读路径 BFF 聚合；**不**作为业务域数据与规则所有者。
- 对齐 [docs/architecture/README.md](../../../docs/architecture/README.md) 与 [docs/contracts/README.md](../../../docs/contracts/README.md)：客户端 JSON（`snake_case`）、内部 proto、公共契约引用关系。
- `api.md`：路径版本建议、HTTP 方法、请求头、响应信封、错误与 HTTP 状态码策略、JSON/proto 职责划分。
- `pages.md`：BFF 原则、典型页面占位表、聚合失败策略与分页约定。
- `data-model.md`：对外 canonical 引用 contracts；gateway 侧请求上下文逻辑字段；无业务主数据模型。
- `workflow.md`：端到端处理阶段、metadata 传播、聚合超时与部分失败、观测要点。

### 兼容性与后续工作

- 具体路由表、RPC 方法全名、错误映射表需在 proto 与实现落地后回填 `api.md` / `pages.md` 附录。
- 若限流统一采用「仅 200 + 信封」或「429 + 信封」，需在下一文档版本锁定一种默认策略并更新 `api.md`。
