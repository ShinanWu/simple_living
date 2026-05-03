# Gateway（统一入口 / BFF）服务说明

## 1. 定位：边界层，而非业务域

`gateway` 是 **客户端与内部业务域之间的唯一对外边界**，承担协议转换、安全入口、流量治理与读路径聚合。它 **不拥有** 导购内容、推荐策略、联盟规则、用户画像、跳转归因等业务语义的所有权；这些语义分别由 `content-domain`、`recommendation-domain`、`affiliate-domain`、`user-domain`、`tracking-domain`、`governance-domain` 等维护。

类比：

- **业务域**：定义「是什么、能不能做、数据真相」。
- **Gateway**：定义「外部怎么进来、怎么被鉴权与限流、怎么把一次 HTTP 请求翻译成一组内部 RPC、怎么把多路结果拼成客户端可用的 JSON」。

与 [技术架构总览](../../../docs/architecture/README.md) 一致：客户端与 gateway 使用 **JSON**；gateway 与内部服务使用 **proto**（经 brpc）；**协议翻译集中在 gateway**，避免客户端直连内部服务形态。

## 2. 核心职责

| 类别 | 职责 |
|------|------|
| **统一对外访问面** | 所有终端（Web / iOS / Android / 小程序等）经同一入口访问后端；禁止客户端绕过 gateway 调用内部服务 |
| **鉴权入口** | 校验 `Authorization` 等凭证（格式与头约定见 [auth.md](../../../docs/contracts/auth.md)）；将「已通过鉴权的用户/访客上下文」注入下游 RPC 的 metadata；gateway **可以调用** `user-domain` 发放或刷新 token，但**不**拥有账号体系与 token 生命周期规则本身 |
| **路由** | 将 HTTP 路径与方法映射到具体内部 RPC 或聚合流程；路径与版本策略见 [api.md](./api.md) |
| **限流与基础防护** | 按 IP、用户、客户端标识、接口维度限流；当前统一由 gateway 与各域 brpc 承载，契约层约定「超限时的错误语义」对齐 [error-codes.md](../../../docs/contracts/error-codes.md) |
| **JSON ↔ proto 映射** | 对外 JSON 字段 **`snake_case`**，与 [contracts](../../../docs/contracts/README.md) 一致；请求/响应体与内部 `proto` 的字段级映射由 gateway 维护；**业务语义以各域 proto + 公共契约为准**，gateway 只做忠实转换与聚合层裁剪 |
| **读路径聚合（BFF）** | 为页面或客户端场景组合多次只读 RPC，减少往返；聚合规则与页面契约见 [pages.md](./pages.md) |
| **错误与信封统一** | 对外响应必须符合 [common-response.md](../../../docs/contracts/common-response.md)；将内部 RPC 错误码/状态映射为统一 `code` / `message`，不泄露内部栈与内部服务名 |
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
| `user-domain` | 登录态校验、用户摘要、偏好、收藏、历史、反馈等 |
| `content-domain` | 导购内容、专题、榜单、卡片详情 |
| `recommendation-domain` | 推荐列表、场景推荐、解释字段 |
| `affiliate-domain` | 渠道能力、佣金相关参数封装（对客户端仅暴露契约允许字段） |
| `tracking-domain` | 推广链接、点击 ID、跳转链 |
| `governance-domain` | 运营开关、可见性、合规标识、下架状态参与过滤或错误语义 |

Gateway **依赖各域已发布的 proto 与接口契约**，不复制、不私改他域 proto（参见 [contracts README](../../../docs/contracts/README.md) 二层模型）。

### 4.3 集群内地址规范（下游 brpc）

- 默认下游地址采用 Kubernetes Service DNS，格式为：`brpc://<service>.simple-living.svc.cluster.local:<port>`。
- 端口约定与 [`docs/engineering-conventions.md`](../../../docs/engineering-conventions.md) §4 保持一致：
  - `user-domain`: `9101`
  - `content-domain`: `9102`
  - `recommendation-domain`: `9103`
  - `affiliate-domain`: `9104`
  - `tracking-domain`: `9105`
  - `governance-domain`: `9106`
- 本地联调允许通过启动 flags 覆盖为 `brpc://127.0.0.1:<port>`，不改变开发体验。
- 运行时也可使用环境变量覆盖（例如 `GATEWAY_USER_DOMAIN_ADDR`）；如同时传入 flag，以 flag 为准。

当前实现状态（精确待办）：

- 已实现并支持 K8s DNS 默认值 + env/flag 覆盖的符号：`FLAGS_user_domain_addr`、`FLAGS_content_domain_addr`、`FLAGS_recommendation_domain_addr`、`FLAGS_tracking_domain_addr`（文件：`services/gateway/src/gateway_edge_server_main.cpp`）。
- 待补齐符号（文档已定义但当前二进制尚未声明）：`FLAGS_affiliate_domain_addr`、`FLAGS_governance_domain_addr`（建议文件：`services/gateway/src/gateway_edge_server_main.cpp`；建议接入点：`GatewayPagesEdgeV2Impl` 下游 channel 初始化与相关 handler 调用链）。

### 4.4 契约依赖

- **必须**遵守 [docs/contracts/](../../../docs/contracts/) 中的公共约定：`common-response`、`error-codes`、`pagination`、`auth`、`guide-card`、`recommendation`、`redirect-attribution` 等。
- 各 HTTP 路由的字段级定义以 [api.md](./api.md) 为准；[pages.md](./pages.md) 保留为 BFF 设计说明与路由编排补充。与内部 `proto` 不一致时，以 **先改文档、再改实现** 为准。

## 5. 文档与并行开发

- **实现与交付**：[development.md](./development.md)（文档阅读顺序、Bazel、联调、测试、合并前检查）。
- **单服务部署**：[../deploy/README.md](../deploy/README.md)（QEMU，独立构建/分发/部署/回滚/验收）。
- **运营后台后端能力**：[backoffice-backend.md](./backoffice-backend.md)（管理/审核 API、状态机、权限与审计约束）。
- 客户端与测试可仅依赖：`api.md`、公共契约与 mock；需要理解聚合编排时再参考 `pages.md`。
- Gateway 实现者可依赖：本文档、`workflow.md`、`data-model.md`、各域 proto。
- 域服务团队可假设：对外 JSON 由 gateway 翻译，域接口保持 proto 稳定演进并做好兼容性。

变更记录见 [changelog.md](./changelog.md)。

## 6. 对外协议实现说明（与架构对齐）

- **终端契约**：客户端只使用 **HTTPS + JSON**；路由与 `data` 形状以 [api.md](./api.md) 与 `docs/contracts/` 为准。
- **内部契约**：`gateway` 使用 **brpc** 调用各业务域；消息类型由各域 `proto` 定义。
- **仓库内 `proto` 中的 Edge service**：表示与 HTTP 路由对应的 **逻辑处理边界**，便于代码生成、类型复用与 handler 分层；**不是**客户端直连的 wire API。详见 [`docs/engineering-conventions.md`](../../../docs/engineering-conventions.md) §3。
- **交付要求**：`gateway` 二进制须包含 **HTTP 接入层**（TLS/路由/JSON 信封/下游 brpc 客户端）；若当前仅有 brpc Service 脚手架，须在实现阶段补齐 HTTP 层并在 [changelog.md](./changelog.md) 记录演进。

实现时的请求阶段划分、metadata 与超时策略见 [workflow.md](./workflow.md)。构建、运行与合并前检查见 [development.md](./development.md)；全仓端口与 Bazel 矩阵见 [`docs/engineering-conventions.md`](../../../docs/engineering-conventions.md)。
