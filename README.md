# 简单生活

> 一个面向衣、食、住、行场景的导购资讯平台，核心目标是用更少决策成本帮助用户完成更优选择。

## 产品定位

`简单生活` 只做两件事：

- 为用户提供可信、易理解、可直接决策的导购内容
- 将用户跳转到第三方平台完成最终转化并获得渠道佣金

平台不自建交易、不承接支付、不承担履约，只专注于 `内容筛选 + 推荐决策 + 跳转转化`。

## 核心原则

- **纯导购**：平台只提供推荐与跳转，不介入站内交易闭环
- **文档先行**：产品、接口、字段、页面、依赖关系先写清楚，再进入开发
- **业务优先**：服务按业务域拆分，文档、代码、接口围绕业务能力组织
- **多端一致**：iOS、Android、Web、小程序共享同一套业务定义与交互规则
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

## 目录结构（强约束）

```text
simple_living/
├── docs/                                  # 顶层设计与跨服务公共内容（唯一上层文档入口）
│   ├── product-design.md                  # 产品顶层设计
│   ├── compliance.md                      # 合规基线
│   ├── document-first-development.md      # 文档先行与并行开发规范
│   ├── engineering-conventions.md        # 跨服务工程约定（Bazel、端口、测试、gateway 分层）
│   ├── contracts/                         # 跨服务公共契约（JSON 语义等）
│   │   └── README.md
│   ├── ...                                # 顶层设计文档（不放服务私有实现细节）
│   └── architecture/
│       └── README.md                      # 技术与业务架构总览
│
├── client/                                # 多端客户端（每个端内自含 docs/src/tests/deploy）
│   ├── frontend-README.md                 # 前端详细设计总览
│   ├── cross-platform-interaction-consensus.md
│   ├── frontend-principles.md
│   ├── frontend-information-architecture.md
│   ├── frontend-page-specs.md
│   ├── frontend-gateway-interaction.md
│   ├── frontend-platform-mapping.md
│   ├── frontend-mock-and-acceptance.md
│   ├── ios/
│   │   └── interaction-notes.md
│   ├── android/
│   │   └── interaction-notes.md
│   ├── web/                                # 预留（当前无 C 端 Web 开发计划）
│   └── mini-program/
│
├── services/                              # 服务主目录（每个服务都是“准独立仓库”）
│   ├── README.md                          # 业务服务总览
│   ├── gateway/                           # 统一入口
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   ├── deploy/
│   │   └── BUILD.bazel
│   ├── user-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   ├── deploy/
│   │   └── BUILD.bazel
│   ├── content-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   ├── deploy/
│   │   └── BUILD.bazel
│   ├── recommendation-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   ├── deploy/
│   │   └── BUILD.bazel
│   ├── affiliate-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   ├── deploy/
│   │   └── BUILD.bazel
│   ├── tracking-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   ├── deploy/
│   │   └── BUILD.bazel
│   ├── platform/
│   │   ├── README.md
│   │   └── backoffice-web/
│   │       ├── src/
│   │       ├── deploy/
│   │       └── Dockerfile
│   └── governance-domain/
│       ├── docs/
│       ├── proto/
│       ├── src/
│       ├── tests/
│       ├── deploy/
│       └── BUILD.bazel
│
├── infra/                                 # 公共基础设施脚本（不承载服务业务逻辑）
├── third_party/                           # 第三方依赖
├── BUILD.bazel / MODULE.bazel             # 顶层构建入口（公共）
└── ...                                    # 其他上层目录仅允许公共内容，禁止放服务私有实现
```

## 技术选型

- **客户端**：SwiftUI / Jetpack Compose / React 或 Vue / 微信小程序
- **对外接口**：客户端统一通过前置 `Nginx` + `gateway` 访问；`Nginx` 承担 TLS 终止、基础限流等通用网络能力，`gateway` 仍是唯一业务入口并对外提供 HTTPS + JSON
- **服务端**：C++ + brpc
- **内部通信**：`gateway` 与各业务域、各业务域之间统一使用 `proto`
- **构建系统**：Bazel
- **容器与编排**：Docker + Kubernetes
- **K8s 使用范围**：仅用于容器编排与服务注册发现（Service + CoreDNS）
- **服务治理**：当前由 `gateway` 与各域 `brpc` 统一承载（限流、超时、重试等）；暂不启用 Ingress / NetworkPolicy 等额外治理能力
- **可观测性**：Prometheus + Grafana + ELK + Jaeger
- **交付链路**：GitLab CI + ArgoCD + Terraform

## 关键文档

- [产品设计文档](./docs/product-design.md)
- [技术架构总览](./docs/architecture/README.md)
- [文档先行开发规范](./docs/document-first-development.md)
- [工程约定（Bazel / 端口 / 测试 / gateway 分层）](./docs/engineering-conventions.md)（与各服务 `services/<service>/docs/development.md` 配合）
- [前端详细设计总览](./client/frontend-README.md)
- [前端与 Gateway 交互设计](./client/frontend-gateway-interaction.md)
- [合规基线](./docs/compliance.md)
- [服务契约总览](./docs/contracts/README.md)
- [业务服务总览](./services/README.md)

## 服务自治规则（硬约束）

- 每个服务目录必须自包含：`docs/`、`proto/`、`src/`、`tests/`、`deploy/`、`BUILD.bazel`。
- 服务迭代、发布、回滚默认在各自目录完成，不通过“全服务统一脚本”触发批量构建。
- 跨服务通信只依赖 `docs/contracts/` 与各服务 `docs/api.md`，禁止依赖他域实现路径。
