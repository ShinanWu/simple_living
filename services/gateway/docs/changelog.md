# Gateway 文档变更记录

本文档记录 **gateway 服务契约与文档** 的变更，便于多端与多域并行开发时对齐版本。

## 版本说明

- **文档版本**与运行中的 gateway 二进制版本可不同步，但对外承诺以 **已发布的文档 + contracts** 为准。
- 破坏性变更必须递增对外 API 版本路径（如 `/api/v2`）或经过书面迁移期（在本 changelog 说明）。

---

## [Unreleased]

### 文档补强（商业化上线标准 / DoD）

仅文档补强，**不改对外契约语义**：保留并增量补全到「可正式上线」深度，对齐 `.cursor/rules/service-doc-standard.mdc` 与 `.cursor/rules/shared-contracts.mdc`，跨域只引用对方 `api.md`，不复制他域 proto。

- `README.md`：新增 §7「服务等级目标（SLO）与容量假设」——对外可用性、网关自身/端到端延迟目标（首页/详情/跳转准备 P99）、整体 QPS 与单实例容量假设、限流阈值表（IP/主体/跳转/写路径，超限 `10005`）。
- `api.md`：错误码章节结构化——新增 §8.1 网关入口层错误码（`10050`–`10099`：路由不存在/方法不允许/体解析失败/聚合部分降级/关键依赖失败），§8.3 业务域错误码透传规则（30xxx/40xxx/50xxx/60xxx 引用各域 `api.md`），§8.4 gRPC 兜底映射改为「无明确业务码时」语义；新增「附录 B 典型错误响应示例」（`20002`/`10002`/`10005`/`10050`/`30002` 透传/聚合降级 `warnings`/`10054`）。
- `data-model.md`：§5 明确 gateway **无 PostgreSQL 业务表**（权威在各域），新增 §5.2 Redis 运行时状态键约定与 TTL（限流计数、短缓存、访客会话缓存、幂等窗口、可选令牌内省缓存），统一 `gw:` 前缀；v1 网关不直接生产/消费 Kafka 事件。
- `workflow.md`：§2.5 细化 metadata/`request_id`/`trace_id` 注入与标准化用户上下文、单 RPC 超时预算表、重试/降级/熔断策略；§2.6 新增逐路由聚合编排表（关键 vs 可降级依赖、deadline 目标），对齐 `api.md` §13 与 `README.md` §7.3。
- `development.md`：新增 §5.1 完整配置项表（`-port`/TLS 边界/六个下游地址 + env/`-redis_addr`/`-rpc_timeout_ms`/`-rpc_max_retry`/限流参数/`-log_level`）、§5.2 可观测性（`/healthz`、`/api/v2/health`、入口/下游/资源 metrics、按 `request_id` 串联日志、trace 透传）、§5.3 安全与鉴权边界（令牌解析、标准化上下文注入、运营角色、入口防护、TLS 由前置 Nginx 终止）；§7 测试改为上线门槛（HTTP 成功 + 典型错误码 + 信封一致 + 契约测试 + 聚合降级）；合并前检查清单同步扩充。
- `deploy/README.md`：补充回滚要点（无状态、无库迁移、下游兼容）、验收（`/healthz` + `/api/v2/health` + 成功/错误冒烟）、§7 日志、§8 排障表与定位顺序。

---

## [1.0.6] - 2026-04-20

### 文档补充（运营后台）

- `api.md` 新增 `/api/v2/backoffice/*` 路由组说明：联盟伙伴、内容发布态、治理审核队列。
- `pages.md` 在页面聚合总表新增“运营后台”条目，明确仍由 gateway 作为 BFF 分路由到 `platform/backoffice-backend`（content / governance / affiliate 三模块）。
- 保持网关边界不变：不承接跨域事务，不转移业务所有权。

### 实现同步（运营后台 API）

- `proto/gateway_pages_edge.proto` 新增 backoffice 聚合消息与 RPC：伙伴管理、内容状态、审核队列。
- `src/gateway_edge_server_main.cpp` 新增 `/api/v2/backoffice/*` 路由映射与处理逻辑，提供运营后台后端能力（POST-only）。

### 文档补充（先文档后开发）

- 新增 `backoffice-backend.md`：明确运营管理后台后端目标、路由、对象定义、审核/发布状态机、权限模型与审计要求。
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
- `src/gateway_edge_server_main.cpp`：注册 `/api/v2/...` restful 映射；通过预填 `Controller::response_attachment()` 输出与 `.cursor/rules/shared-contracts.mdc` 一致的 JSON 信封；Bearer / `X-Guest-Session-Id` 解析并注入下游 `user_id`/`session_id`（不信任客户端伪造的登录主体字段）。
- 对外路由进一步统一为 **POST-only**：使用动作化路径区分语义（如 `/api/v2/me/profile/get|update`、`/api/v2/me/favorites/list|add|remove`、`/api/v2/auth/session/revoke`、`/api/v2/health/check`），旧的 GET/PUT/PATCH/DELETE 语义入口不再接受；`favorite_id` 改为 `remove` 请求体字段。

---

## [1.0.2] - 2026-04-06

### 更新

- `README.md`：新增“集群内地址规范（下游 brpc）”，默认约定 `brpc://<service>.simple-living.svc.cluster.local:<port>`，并补充本地联调覆盖策略（flag/env）。
- `development.md`：对齐运行说明，明确集群内默认地址与本地 `127.0.0.1` 覆盖方式。

### 实现同步

- `src/gateway_edge_server_main.cpp`：下游默认地址切换到 K8s Service DNS（`user/content/recommendation/tracking`），并新增环境变量覆盖（仅在未显式传入 flag 时生效）：
  - `GATEWAY_USER_SERVER_ADDR`
  - `GATEWAY_BACKOFFICE_BACKEND_ADDR`
  - `GATEWAY_RECOMMENDATION_SERVER_ADDR`
  - `GATEWAY_TRACKING_SERVER_ADDR`

## [1.0.1] - 2026-03-29

### 新增

- `README.md`：§6「对外协议实现说明」——终端 HTTPS+JSON、`gateway/proto` Edge 为逻辑边界、须含 HTTP 接入层；引用 [`业务服务总览`](../../README.md)。
- `development.md`：Gateway 实现与交付（文档阅读顺序、Bazel、联调 flags、测试、合并前检查）。

### 说明

- 跨服务工程约定集中在 [`业务服务总览`](../../README.md)；各服务在 `services/<service>/docs/development.md` 说明本域实现与交付细节。

## [1.0.0] - 2026-03-28

### 新增

- 初始化 `services/gateway/docs/` 文档集：`README.md`、`api.md`、`pages.md`、`data-model.md`、`workflow.md`、`changelog.md`。
- 明确 gateway **边界层** 定位：统一对外入口、鉴权入口、路由、限流、JSON ↔ proto 翻译、读路径 BFF 聚合；**不**作为业务域数据与规则所有者。
- 对齐 [业务服务总览](../../README.md) 与 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc)：客户端 JSON（`snake_case`）、内部 proto、公共契约引用关系。
- `api.md`：路径版本建议、HTTP 方法、请求头、响应信封、错误与 HTTP 状态码策略、JSON/proto 职责划分。
- `pages.md`：BFF 原则、典型页面聚合表、聚合失败策略与分页约定。
- `data-model.md`：对外 canonical 引用 contracts；gateway 侧请求上下文逻辑字段；无业务主数据模型。
- `workflow.md`：端到端处理阶段、metadata 传播、聚合超时与部分失败、观测要点。

### 兼容性与后续工作

- 具体路由表、RPC 方法全名、错误映射表需在 proto 与实现落地后回填 `api.md` / `pages.md` 附录。
- 若限流统一采用「仅 200 + 信封」或「429 + 信封」，需在下一文档版本锁定一种默认策略并更新 `api.md`。
