# Gateway（统一入口 / BFF）服务说明

## 1. 定位：边界层，而非业务域

`gateway` 是 **客户端与内部业务域之间的唯一对外边界**，承担协议转换、安全入口、流量治理与读路径聚合。它 **不拥有** 导购内容、推荐策略、联盟规则、用户画像、跳转归因等业务语义的所有权；这些语义分别由 `recommendation-server`（C 端读+推荐）、`platform/backoffice-backend`（运营写）、`user-server`、`tracking-server` 等维护。

类比：

- **业务域**：定义「是什么、能不能做、数据真相」。
- **Gateway**：定义「外部怎么进来、怎么被鉴权与限流、怎么把一次 HTTP 请求翻译成一组内部 RPC、怎么把多路结果拼成客户端可用的 JSON」。

与 [业务服务总览](../../README.md) 一致：客户端与 gateway 使用 **JSON**；gateway 与内部服务使用 **proto**（经 brpc）；**协议翻译集中在 gateway**，避免客户端直连内部服务形态。

## 2. 核心职责

| 类别 | 职责 |
|------|------|
| **统一对外访问面** | 所有终端（Web / iOS / Android / 小程序等）经同一入口访问后端；禁止客户端绕过 gateway 调用内部服务 |
| **鉴权入口** | 校验 `Authorization` 等凭证（格式与头约定见 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc)）；将「已通过鉴权的用户/访客上下文」注入下游 RPC 的 metadata；gateway **可以调用** `user-server` 发放或刷新 token，但**不**拥有账号体系与 token 生命周期规则本身 |
| **路由** | 将 HTTP 路径与方法映射到具体内部 RPC 或聚合流程；路径与版本策略见 [api.md](./api.md) |
| **限流与基础防护** | 按 IP、用户、客户端标识、接口维度限流；当前统一由 gateway 与各域 brpc 承载，契约层约定「超限时的错误语义」对齐 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) |
| **JSON ↔ proto 映射** | 对外 JSON 字段 **`snake_case`**，与 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) 一致；请求/响应体与内部 `proto` 的字段级映射由 gateway 维护；**业务语义以各域 proto + 公共契约为准**，gateway 只做忠实转换与聚合层裁剪 |
| **读路径聚合（BFF）** | 为页面或客户端场景组合多次只读 RPC，减少往返；聚合规则与页面契约见 [pages.md](./pages.md) |
| **错误与信封统一** | 对外响应必须符合 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc)；将内部 RPC 错误码/状态映射为统一 `code` / `message`，不泄露内部栈与内部服务名 |
| **请求上下文传播** | 生成或透传 `request_id`、`trace_id`，写入响应 `meta` 与下游 metadata，见 [workflow.md](./workflow.md) |

## 3. 非目标（明确不做）

- 不持久化业务主数据（用户、内容、推荐日志等）；必要时仅缓存为性能优化，且缓存内容仍以上游域为准。
- 不实现推荐算法、内容审核规则、联盟佣金计算、跳转归因计算等业务规则。
- 不把「仅内部服务之间需要」的 proto 结构暴露为对外 JSON；对外形态以接口文档 + 公共契约为准。
- 不承担客户端 UI 逻辑；仅提供数据与约定好的聚合视图。

## 4. 上游与下游依赖

### 4.1 上游（调用方）

- 各端客户端、未来可能的开放合作伙伴（若开放，仍经 gateway 与独立路由策略）。

### 4.2 下游（被调用方）

| 下游 | 典型用途 |
|------|----------|
| `user-server` | 登录态校验、用户摘要、偏好、收藏、历史、反馈等 |
| `recommendation-server` | C 端导购内容读（snapshot）、推荐列表、场景推荐、解释字段 |
| `tracking-server` | 推广链接、点击 ID、跳转链 |
| `platform/backoffice-backend` | 运营写路径 `/api/v2/backoffice/*`（内容、治理、联盟配置） |

Gateway **依赖各域已发布的 proto 与接口契约**，不复制、不私改他域 proto（参见 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) 二层模型）。

### 4.3 集群内地址规范（下游 brpc）

- 默认下游地址采用 Kubernetes Service DNS，格式为：`brpc://<service>.simple-living.svc.cluster.local:<port>`。
- 端口约定与 [`业务服务总览`](../../README.md) §4.2 保持一致：
  - `user-server`: `9101`
  - `recommendation-server`: `9103`
  - `tracking-server`: `9105`
  - `platform/backoffice-backend`: `9110`
- 本地联调允许通过启动 flags 覆盖为 `brpc://127.0.0.1:<port>`，不改变开发体验。
- 运行时也可使用环境变量覆盖（例如 `GATEWAY_USER_SERVER_ADDR`）；如同时传入 flag，以 flag 为准。

下游 flags（`services/gateway/src/gateway_edge_server_main.cpp`）：

- `-user_server_addr`、`-recommendation_server_addr`、`-tracking_server_addr`、`-backoffice_backend_addr`
- C 端读：`recommendation-server`；运营写：`backoffice-backend`

### 4.4 契约依赖

- **必须**遵守 [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) 中的公共约定：`common-response`、`error-codes`、`pagination`、`auth`、`guide-card`、`recommendation`、`redirect-attribution` 等。
- 各 HTTP 路由的字段级定义以 [api.md](./api.md) 为准；[pages.md](./pages.md) 保留为 BFF 设计说明与路由编排补充。与内部 `proto` 不一致时，以 **先改文档、再改实现** 为准。

## 5. 文档与并行开发

- **实现与交付**：[development.md](./development.md)（文档阅读顺序、Bazel、联调、测试、合并前检查）。
- **单服务部署**：[../deploy/README.md](../deploy/README.md)（QEMU，独立构建/分发/部署/回滚/验收）。
- **运营后台后端能力**：[backoffice-backend.md](./backoffice-backend.md)（管理/审核 API、状态机、权限与审计约束）。
- 客户端与验收测试可仅依赖：`api.md`、公共契约与真实 gateway 响应；mock 只能作为测试夹具，不能作为运行时或验收契约。
- Gateway 实现者可依赖：本文档、`workflow.md`、`data-model.md`、各域 proto。
- 域服务团队可假设：对外 JSON 由 gateway 翻译，域接口保持 proto 稳定演进并做好兼容性。

变更记录见 [changelog.md](./changelog.md)。

## 6. 对外协议实现说明（与架构对齐）

- **终端契约**：客户端只使用 **HTTPS + JSON**；路由与 `data` 形状以 [api.md](./api.md) 与 `.cursor/rules/shared-contracts.mdc` 为准。
- **内部契约**：`gateway` 使用 **brpc** 调用各业务域；消息类型由各域 `proto` 定义。
- **仓库内 `proto` 中的 Edge service**：表示与 HTTP 路由对应的 **逻辑处理边界**，便于代码生成、类型复用与 handler 分层；**不是**客户端直连的 wire API。详见 [`业务服务总览`](../../README.md) §4.4。
- **交付要求**：`gateway` 二进制须包含 **HTTP 接入层**（TLS/路由/JSON 信封/下游 brpc 客户端）；若当前仅有 brpc Service 脚手架，须在实现阶段补齐 HTTP 层并在 [changelog.md](./changelog.md) 记录演进。

实现时的请求阶段划分、metadata 与超时策略见 [workflow.md](./workflow.md)。构建、运行与合并前检查见 [development.md](./development.md)；全仓端口与 Bazel 矩阵见 [`业务服务总览`](../../README.md)。

## 7. 服务等级目标（SLO）与容量假设

Gateway 是唯一公网 BFF，其 SLO 是**对外承诺面**；端到端延迟 = 网关自身开销 + 下游域延迟之和（聚合路由取并行分支的尾部）。下表为 v1 目标，前置 `Nginx`（TLS 终止/连接治理）与各域 SLO 见各自文档。

### 7.1 可用性

| 指标 | 目标（v1） |
|------|-----------|
| 对外接口整体可用性（信封 `success` 可返回，含降级响应） | ≥ 99.9% |
| 鉴权与会话类路由（`/api/v2/auth/*`、`/api/v2/guest/session`） | ≥ 99.9% |
| 页面聚合路由（依赖多下游，按降级策略仍可返回） | ≥ 99.5% |

可用性以「网关能按 `.cursor/rules/shared-contracts.mdc` 信封返回结果（含合法错误码与降级 `warnings`）」计入；返回 `90001`/`90002`/`90003` 的请求计为失败窗口的一部分。

### 7.2 延迟目标（网关自身开销，不含下游处理时间）

| 接口类型 | P50 | P99 |
|----------|-----|-----|
| 网关入口/路由/鉴权/映射自身开销 | ≤ 3ms | ≤ 15ms |

### 7.3 端到端延迟目标（含下游，正常负载）

| 路由 | P99（端到端） | 主要构成 |
|------|--------------|----------|
| `GET /api/v2/pages/home_feed`（首页推荐） | ≤ 250ms | recommendation + content 批量 hydration + 可选治理过滤 |
| `GET /api/v2/pages/guide_detail`（导购详情） | ≤ 200ms | content 主数据 + 治理披露 + 可选相关推荐 |
| `POST /api/v2/pages/redirect_prepare`（跳转准备） | ≤ 200ms | tracking 组链（含 affiliate 链接规格/签名） |
| 细粒度 `/api/v2/me/*` 单域读 | ≤ 120ms | user-server 单次 RPC |

超出延迟预算时优先触发下游超时与降级（见 [workflow.md](./workflow.md) §2.6），不让尾部延迟拖垮整请求。

### 7.4 吞吐与容量假设（v1）

| 维度 | 假设 |
|------|------|
| 整体对外 QPS | ≤ 2000（读占比 ≥ 95%） |
| 单实例承载 | ≤ 800 QPS（无状态，水平扩展） |
| 首页推荐流 QPS | ≤ 800 |
| 跳转准备 QPS | ≤ 200 |

容量为无状态多实例假设：扩容以增加 gateway 副本 + 扩下游容量为主；网关不缓存业务真相，仅做短期/热点缓存（见 [data-model.md](./data-model.md) §5）。

### 7.5 限流阈值（默认建议，可经 flag 覆盖）

| 维度 | 默认阈值（v1） | 超限码 |
|------|----------------|--------|
| 单 IP | 120 req / 10s | `10005`（HTTP 429） |
| 单 `user_id` / 访客 `session_id` | 600 req / 60s | `10005` |
| 跳转准备 `POST /api/v2/pages/redirect_prepare`（按主体） | 30 req / 60s | `10005` |
| 写类路由（favorites/feedback/history events，按主体） | 120 req / 60s | `10005` |

阈值为运营可调参数（见 [development.md](./development.md) 配置项表），超限统一返回 `10005` + HTTP 429，并符合通用响应信封。前置 `Nginx` 可承担粗粒度连接级限流，业务维度限流语义以 gateway 为准。
