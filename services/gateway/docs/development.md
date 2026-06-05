# Gateway — 开发与交付

本文说明本服务在仓库内的**实现、构建、本地运行、测试与合并前检查**约定，以及须对照的公共文档。

## 1. 本服务文档（均在当前 `docs/` 内）

| 顺序 | 文档 |
|------|------|
| 1 | [README.md](./README.md)（边界、职责、对外/对内协议） |
| 2 | [api.md](./api.md)（HTTPS+JSON 路由与 `data` 形状） |
| 3 | [workflow.md](./workflow.md)（阶段、metadata、超时与聚合） |
| 4 | [pages.md](./pages.md)（BFF 与页面聚合） |
| 5 | [data-model.md](./data-model.md)（网关侧上下文字段） |
| 6 | 变更记 [changelog.md](./changelog.md) |

## 2. 公共文档

| 文档 | 用途 |
|------|------|
| [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) | 公共 JSON 语义与二层契约模型、响应信封、错误码 |
| [业务服务总览](../../README.md) | Bazel、全端口表、跨服务依赖矩阵、测试与 CI（细节以该文为准） |

架构与依赖拓扑：[业务服务总览](../../README.md)。

## 3. 本仓库内实现边界（摘要）

- **终端**：只使用 **HTTPS + JSON**；`gateway/proto` 中 Edge service 为 **逻辑路由边界**，不是客户端直连的 wire API。完整说明见 [README.md §6](./README.md) 与 `services/README.md` §4.4。
- **下游**：经 **brpc** 调用各业务域；不得把业务规则下沉到错误映射之外的新增逻辑而不更新文档。

## 3.1 状态与基础设施（顶层设计）

`gateway` **不承载业务权威状态**；鉴权、限流、聚合所需的外部能力按 [`业务服务总览`](../../README.md) §4.3：**Redis**（如限流计数/短期缓存）、**MySQL / PostgreSQL**（若网关侧仅存审计/配置类数据，须在 `data-model.md` 写明）、**Kafka**（异步旁路，若有）。默认实现以**无状态多实例**为准。细则见 `services/README.md` §4.3。

## 4. Bazel 与源码位置

| 项 | 值 |
|----|-----|
| 包路径 | `//services/gateway` |
| 主二进制 | `//services/gateway:gateway_edge_server`（名称以 `BUILD.bazel` 为准） |
| Proto | `proto/*.proto` |
| 实现 | `src/` |
| 测试 | `tests/` |

构建：`bazel build //services/gateway/...`

## 5. 运行与联调（本服务）

| 项 | 约定 |
|----|------|
| 默认对外端口 | `8080`（`-port` 覆盖） |
| 下游 | 默认使用集群内 `brpc://<service>.simple-living.svc.cluster.local:<端口>`；本地联调可覆盖为 `brpc://127.0.0.1:<端口>`；端口全表见 `services/README.md` §4.2 |

示例（先启动各域 brpc，再启 gateway）：

```bash
bazel run //services/gateway:gateway_edge_server -- \
  -port=8080 \
  -user_server_addr=brpc://127.0.0.1:9101 \
  -recommendation_server_addr=brpc://127.0.0.1:9103 \
  -tracking_server_addr=brpc://127.0.0.1:9105 \
  -backoffice_backend_addr=brpc://127.0.0.1:9110
```

### 5.1 完整配置项表（flags / 等价 env；监听默认 `0.0.0.0`）

集群内地址默认采用 K8s Service DNS（`brpc://<service>.simple-living.svc.cluster.local:<port>`，见 [README.md §4.3](./README.md)）；本地联调可覆盖为 `brpc://127.0.0.1:<port>`。同时传入 flag 与 env 时**以 flag 为准**。

| flag | 等价 env | 必填 | 默认 | 说明 |
|------|----------|------|------|------|
| `-port` | — | 否 | `8080` | 对外 HTTP 接入端口（前置 Nginx 反代到此） |
| `-listen_addr` | — | 否 | `0.0.0.0` | 监听地址 |
| `-user_server_addr` | `GATEWAY_USER_SERVER_ADDR` | 否 | `brpc://user-server.simple-living.svc.cluster.local:9101` | user-server 下游 |
| `-recommendation_server_addr` | `GATEWAY_RECOMMENDATION_SERVER_ADDR` | 否 | `brpc://recommendation-server.simple-living.svc.cluster.local:9103` | recommendation-server（C 端读 + 推荐） |
| `-tracking_server_addr` | `GATEWAY_TRACKING_SERVER_ADDR` | 否 | `brpc://tracking-server.simple-living.svc.cluster.local:9105` | tracking-server 下游 |
| `-backoffice_backend_addr` | `GATEWAY_BACKOFFICE_BACKEND_ADDR` | 否 | `brpc://backoffice-backend.simple-living.svc.cluster.local:9110` | backoffice-backend（运营写） |
| `-redis_addr` | `GATEWAY_REDIS_ADDR` | 否 | 空 | 限流计数/短缓存/访客会话缓存/幂等窗口；空则退化为单实例本地近似，不影响正确性（见 [data-model.md §5.2](./data-model.md)） |
| `-rpc_timeout_ms` | — | 否 | `800` | 下游单 RPC 默认超时；写/内省类按 [workflow.md §2.5.2](./workflow.md) 调整 |
| `-rpc_max_retry` | — | 否 | `1` | 仅幂等只读下游的最大重试；写操作不自动重试 |
| `-ratelimit_ip_per_10s` | — | 否 | `120` | 单 IP 10s 窗口阈值，超限 `10005`/429（见 [README.md §7.5](./README.md)） |
| `-ratelimit_subject_per_60s` | — | 否 | `600` | 单 `user_id`/访客 `session_id` 60s 窗口阈值 |
| `-ratelimit_redirect_per_60s` | — | 否 | `30` | 跳转准备按主体 60s 窗口阈值 |
| `-log_level` | — | 否 | `info` | 日志级别（`debug`/`info`/`warn`/`error`） |

**TLS 说明**：v1 生产由前置 `Nginx` 终止 TLS，gateway 监听明文 HTTP 供 Nginx 反代（见 §5.2 / [README.md §6](./README.md)）；若部署形态要求 gateway 直接终止 TLS，则由部署层提供证书路径配置（部署注入，不写入仓库，见 [../deploy/README.md](../deploy/README.md)）。密钥/连接串经环境变量或部署密钥注入，**不写入仓库**。

### 5.2 可观测性

- **健康检查**：
  - 内部存活/就绪探针：HTTP `GET /healthz`（编排存活/就绪探针用之）。
  - 对外可公开健康面：`GET /api/v2/health`（标准信封 + 组件状态，可组合下游探活，见 [api.md §9.19](./api.md)）。
- **指标**（Prometheus，`/metrics` 或 brpc 内置）：
  - 入口维度：每路由 QPS、5xx 率、对外 `code` 分布、P50/P99 延迟、限流触发次数（`10005`）。
  - 下游维度：每下游 RPC QPS / 失败率 / 超时率 / P99、熔断状态。
  - 资源维度：连接池/并发使用、Redis 命中率（若启用）。
- **日志**：结构化（JSON），每条含 `request_id`、`trace_id`、method、path、对外 `code`、HTTP status、耗时、命中的下游与其结果概要；**禁止**记录敏感 body、令牌明文、内部栈与主机名。可按 `request_id` 串联一次请求的全部下游调用。
- **追踪**：接入阶段生成/绑定 `trace_id`，注入下游 metadata，使各业务域 span 归属同一 trace（见 [workflow.md §2.5.1](./workflow.md)）。

### 5.3 安全与鉴权边界

- **TLS 边界**：v1 由前置 `Nginx` 终止 TLS 并做连接级基础限流/反向代理；`Nginx` **不**承载业务字段语义、页面聚合或 JSON↔proto 映射（见 [README.md §6](./README.md)、`services/README.md` §4.4）。
- **令牌解析/校验**：网关校验 `Authorization: Bearer <access_token>`，通过 `IntrospectAccessToken` 解析主体；access/refresh 失效分别返回 `20002`/`20003`。令牌签发/撤销规则归 user-server，网关不发明账号体系。
- **标准化用户上下文注入**：身份**单一来源为请求头**（`Authorization` 或访客 `X-Guest-Session-Id`）；网关将解析出的 `user_id`/`session_id`/`is_guest`/`client_platform` 等标准化注入下游 metadata，**禁止**信任客户端体内伪造的 `acting_user_id`/`session_id`（见 [api.md §2.2/§10](./api.md)）。
- **运营后台鉴权**：`/api/v2/backoffice/*` 要求运营角色（`backoffice_admin`/`content_operator`/`reviewer`/`partner_operator`，见 [backoffice-backend.md §7](./backoffice-backend.md)）；权限不足返回 `20004`。
- **入口防护与限流**：按 IP / 主体 / 路由维度限流（阈值见 [README.md §7.5](./README.md)），超限 `10005`/429；对持续失败下游熔断，避免资源耗尽。
- **错误隔离**：对外错误只暴露契约内 `code`/`message`，不泄露内部服务名、proto 字段名与异常栈（见 [api.md §8.3](./api.md)）。

## 6. 允许的 Bazel 依赖（proto / cc_proto）

可依赖各业务域已导出的 `proto_library` / `cc_proto_library`，**不拷贝**他域 `.proto`。方向表见 `services/README.md` §4.1。

## 7. 测试（上线门槛）

- 目录：`tests/`；目标命名与 `cc_test` 约定见 `services/README.md` §5。
- **上线门槛**（缺一不可）：
  - **HTTP 成功路径**：至少一条 `success===true`、`code===0` 的对外响应（如 `GET /api/v2/health`、`GET /api/v2/me/summary`）。
  - **典型错误码**：覆盖入口校验/鉴权/限流/路由的代表性错误，至少 `10002`（参数校验）、`20002`（令牌过期）、`10005`（限流）、`10050`（路由不存在）；并验证下游领域码（如 `30001`/`30002`）按 [api.md §8.3](./api.md) 透传。
  - **信封 `code`/`success` 一致**：断言不变量 `code===0 ⇔ success===true`；`code!==0` 时 `data` 为 `null`（或契约允许的错误详情）。
  - **契约测试**：对外 JSON 字段名、分页结构（`data.items` + `data.pagination`）、错误码与 `.cursor/rules/shared-contracts.mdc` 及 [api.md](./api.md) 一致；JSON↔proto 映射回归。
  - **聚合降级**：模拟可降级下游失败，验证 `success===true` + `warnings`（`10053`）；模拟关键依赖失败，验证整页失败（`10054`）与治理 fail-closed。
- 测试夹具（mock）仅用于单测/契约回归，**不**作为运行时或验收契约；验收必须打真实 gateway。

```bash
bazel test //services/gateway/...
```

## 8. 合并前检查清单

- [ ] `docs/`（含 `api.md`）与实现及 `.cursor/rules/shared-contracts.mdc` 一致；`changelog.md` 已更新
- [ ] `api.md` 路由/错误码/示例与 `proto/` 及实现一致；网关入口码（`10050`–`10099`）与透传规则同步
- [ ] 配置项表（§5.1）、可观测性（§5.2）、安全边界（§5.3）与实现 flags/env、metrics、鉴权逻辑一致
- [ ] 无状态与外部依赖（**Redis** 键约定/TTL，无 PostgreSQL 业务表）与 [data-model.md §5](./data-model.md)、`services/README.md` §4.3 一致
- [ ] 上线测试门槛（§7）通过：HTTP 成功 + 典型错误码 + 信封一致 + 契约测试 + 聚合降级
- [ ] `bazel build //services/gateway/...` 通过
- [ ] `bazel test //services/gateway/...` 通过（或 PR 说明跳过原因与跟踪项）
- [ ] 交付含 **HTTPS+JSON 接入**（或 changelog 已说明分阶段计划）
- [ ] 默认端口与下游 flags 与 `services/README.md` §4.2 一致（若变更则同步该文与各域 `development.md`）

## 9. 协调范围

若修改对外 JSON 或错误语义：必改 `.cursor/rules/shared-contracts.mdc`（如适用）、本目录 `api.md`、`changelog.md`，并通知相关域文档 owner。跨域 proto 变更由提供方服务合并，gateway 只调 Bazel 依赖与映射代码。
