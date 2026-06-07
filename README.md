# 少糖

> 一个面向衣、食、住、行场景的导购资讯平台，核心目标是用更少决策成本帮助用户完成更优选择。

对外品牌名为 **少糖**（曾用名「简单生活」因商标原因停用，主旨不变）。命名约定见下文「品牌与命名」。

## 产品定位

`少糖` 只做两件事：

- 为用户提供可信、易理解、可直接决策的导购内容
- 将用户跳转到第三方平台完成最终转化并获得渠道佣金

平台不自建交易、不承接支付、不承担履约，只专注于 `内容筛选 + 推荐决策 + 跳转转化`。

## 核心原则

- **纯导购**：平台只提供推荐与跳转，不介入站内交易闭环
- **文档先行**：产品、接口、字段、页面、依赖关系先写清楚，再进入开发
- **业务优先**：服务按业务域拆分，文档、代码、接口围绕业务能力组织
- **多端一致**：各端共享同一套业务定义与交互规则；**v1 C 端以微信小程序为主交付**
- **并行开发**：各端和各服务通过文档契约协作，允许多人或多 agent 同时推进

## 目标用户

- 忙碌的都市白领
- 希望减少选择成本的年轻用户
- 注重性价比和效率的消费人群

## 商业模式

- **渠道佣金**：通过拼多多多多进宝、抖音精选联盟等渠道生成推广链接并获得佣金
- **导购转化**：围绕高转化场景持续优化内容、推荐和跳转链路
- **商业合作透明化**：所有合作推荐必须明确标识，确保用户认知清晰

## 核心业务闭环

```text
内容选品 -> 结构化导购内容 -> 个性化推荐 -> 渠道转链 -> 第三方成交 -> 佣金结算 -> 数据复盘优化
```

## 产品边界

平台做：选品与内容组织、用户偏好建模、推荐与排序、渠道链接生成、跳转追踪与归因分析、用户反馈收集。

平台不做：商家入驻交易系统、平台内订单系统、平台内支付与结算、平台内履约管理。

## 产品能力模型

- **用户中心**：注册登录、偏好与兴趣标签、收藏/历史/反馈
- **导购内容中心**：商品/服务资料、导购卡片/榜单/专题/攻略、衣食住行四大主题
- **推荐中心**：首页推荐、分类推荐、个性化排序、热门与趋势
- **渠道接入中心**：拼多多/抖音等联盟接入与其他渠道扩展能力
- **跳转与归因中心**：推广链接生成、点击追踪、渠道回传处理、佣金效果分析
- **运营与治理中心**：内容审核、商业合作标识、风险内容下架、数据看板与策略调整

## 多端一致性

所有客户端共享同一套业务定义，差异只允许出现在终端交互形态，不允许出现在业务语义上。必须一致的项：页面信息架构、核心用户流程、字段定义、推荐解释口径、跳转规则、商业合作标识规则。

## 品牌与命名

- **对外品牌名**：`少糖`（曾用对外名 `简单生活` 因商标被注册停用，不再作为 App/小程序/用户可见文案）
- **用户可见文案**：App/小程序名「少糖」；登录页「登录少糖」；默认昵称「少糖用户」；首页 feed 收尾「少糖就先到这里吧」
- **内部工程名（与对外品牌解耦，可继续使用）**：仓库目录、Bazel 模块、`simple_living` C++ 命名空间、Kubernetes `simple-living` Service DNS、iOS `SimpleLivingCore`
- 对外 HTTP 客户端标识（`User-Agent`、统计）建议用 `Shaotang/<version>`，与 `client_platform` 契约字段无关

## 合规基线（事实）

平台定位为导购资讯与跳转平台，不直接承接交易、支付与履约。主要合规依据：《个人信息保护法》《数据安全法》《广告法》《消费者权益保护法》《电子商务法》。基线要求：数据加密存储传输、最小必要采集、用户可查询/删除/撤回授权、商业合作内容必须可标识可追踪可审计、推荐内容可核验来源、关键跳转节点向用户说明将前往第三方平台。

## 目录结构（强约束）

```text
simple_living/
├── README.md                 # 本文件：产品事实、文档地图、目录约束
├── client/                   # 多端客户端与前端契约（布局因端而异，见 client/frontend-README.md）
│   ├── wechat-miniprogram/   # v1 C 端主交付
│   ├── ios/ · android/       # 第二阶段原生端
│   └── frontend-*.md         # 跨端 IA / 页面 / Gateway 交互
├── common/                   # 跨服务共享：proto / util / 配置（见 common/README.md）
├── services/                 # 业务服务（准独立仓库，见 services/README.md）
│   ├── gateway/
│   ├── user-server/
│   ├── recommendation-server/
│   ├── tracking-server/
│   ├── foundation/           # postgres · redis · kafka
│   ├── proxy/                # nginx · frp 入口
│   └── platform/
│       ├── backoffice-backend/   # 运营写面 + snapshot 导出
│       └── backoffice-web/
├── environments/             # 运行环境（非业务代码）
│   ├── local-qemu/           # Mac QEMU 联调：nodes.env、vms/
│   └── kubernetes/           # 集群公共基座
├── build_defs/               # Bazel 公共编译选项（brpc_copts.bzl）
├── tools/                    # 共享部署/代码生成脚本
└── BUILD.bazel · MODULE.bazel
```

## 技术选型

- **客户端**：SwiftUI / Jetpack Compose / React 或 Vue / 微信小程序
- **对外接口**：客户端统一通过前置 `Nginx` + `gateway` 访问；`Nginx` 承担 TLS 终止、基础限流等通用网络能力，`gateway` 仍是唯一业务入口并对外提供 HTTPS + JSON
- **服务端**：C++ + brpc
- **内部通信**：`gateway` 与各业务域、各业务域之间统一使用 `proto`
- **构建系统**：Bazel
- **容器与编排**：Docker + Kubernetes
- **K8s 使用范围**：容器编排、服务发现、Ingress、NetworkPolicy、灰度与回滚
- **服务治理**：本地 Phase 1 由 `gateway` 与各域 `brpc` 承载限流、超时、重试；生产集群阶段补齐 Ingress / NetworkPolicy / 证书 / 灰度发布
- **可观测性**：Prometheus + Grafana + ELK + Jaeger
- **交付链路**：GitLab CI + ArgoCD + Terraform

## 文档地图

顶层不再维护独立 `docs/` 目录；按主题在下表查找 **唯一来源（SoT）**。

| 主题 | 去哪里读 |
|------|----------|
| 产品定位 / 品牌 / 合规基线 | 本文件上文各章节 |
| 跨服务架构 / 端口 / 依赖 | [`services/README.md`](./services/README.md) |
| 共享 proto / util / 配置 | [`common/README.md`](./common/README.md) |
| 对外 JSON 契约（字段语义） | [`.cursor/rules/shared-contracts.mdc`](./.cursor/rules/shared-contracts.mdc) |
| 文档先行与变更顺序 | [`.cursor/rules/docs-first-delivery.mdc`](./.cursor/rules/docs-first-delivery.mdc) |
| 架构边界（gateway/BFF/域归属） | [`.cursor/rules/architecture-boundaries.mdc`](./.cursor/rules/architecture-boundaries.mdc) · [`.cursor/rules/domain-ownership.mdc`](./.cursor/rules/domain-ownership.mdc) |
| 本地 QEMU 联调 | [`environments/local-qemu/README.md`](./environments/local-qemu/README.md) · `environments/local-qemu/nodes.example.env` |
| K8s 集群基座 | [`environments/kubernetes/README.md`](./environments/kubernetes/README.md) |
| Gateway 对外 API | [`services/gateway/docs/api.md`](./services/gateway/docs/api.md) |
| 各服务 API / 数据模型 / 部署 | `services/<service>/docs/`（自治维护） |
| 前端 IA / 页面 / 跨端共识 | [`client/frontend-README.md`](./client/frontend-README.md) 及索引内链接 |
| 微信小程序（C 端 v1） | [`client/wechat-miniprogram/README.md`](./client/wechat-miniprogram/README.md) |
| 运营管理平台 | [`services/platform/docs/README.md`](./services/platform/docs/README.md) |
| Foundation（PG/Redis/Kafka） | [`services/foundation/README.md`](./services/foundation/README.md) |
| 公网入口 / frp | [`services/proxy/docs/README.md`](./services/proxy/docs/README.md) |
| 共享部署脚本 | [`tools/README.md`](./tools/README.md) |

## 服务自治规则（硬约束）

- 每个服务目录必须自包含：`docs/`、`proto/`、`src/`、`tests/`、`deploy/`、`BUILD.bazel`。
- 服务迭代、发布、回滚默认在各自目录完成，不通过“全服务统一脚本”触发批量构建。
- 跨服务通信只依赖共享契约规则 `.cursor/rules/shared-contracts.mdc` 与各服务 `docs/api.md`，禁止依赖他域实现路径。
