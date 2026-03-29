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

## 推荐目录结构

```text
simple_living/
├── docs/                                  # 文档主入口
│   ├── product-design.md                  # 产品顶层设计
│   ├── compliance.md                      # 合规基线
│   ├── document-first-development.md      # 文档先行与并行开发规范
│   ├── engineering-conventions.md        # 跨服务工程约定（Bazel、端口、测试、gateway 分层）
│   ├── contracts/                         # 跨服务公共契约（JSON 语义等）
│   │   └── README.md
│   └── architecture/
│       └── README.md                      # 技术与业务架构总览
│
├── client/                                # 多端客户端
│   ├── ios/
│   ├── android/
│   ├── web/
│   └── mini-program/
│
├── services/                              # 服务主目录（文档 + proto + 实现）
│   ├── README.md                          # 业务服务总览
│   ├── gateway/                           # 统一入口
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   └── BUILD.bazel
│   ├── user-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   └── BUILD.bazel
│   ├── content-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   └── BUILD.bazel
│   ├── recommendation-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   └── BUILD.bazel
│   ├── affiliate-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   └── BUILD.bazel
│   ├── tracking-domain/
│   │   ├── docs/
│   │   ├── proto/
│   │   ├── src/
│   │   ├── tests/
│   │   └── BUILD.bazel
│   └── governance-domain/
│       ├── docs/
│       ├── proto/
│       ├── src/
│       ├── tests/
│       └── BUILD.bazel
│
├── common/                                # 跨服务公共库（仅保留真正公共能力）
├── infrastructure/                        # 基础设施适配
├── third_party/                           # 第三方依赖
│
├── data/                                  # 数据与脚本
├── ops/                                   # 部署与运维
└── platform/                              # 企业效能平台
```

## 技术选型

- **客户端**：SwiftUI / Jetpack Compose / React 或 Vue / 微信小程序
- **对外接口**：客户端统一通过 `gateway` 使用 HTTPS + JSON
- **服务端**：C++ + brpc
- **内部通信**：`gateway` 与各业务域、各业务域之间统一使用 `proto`
- **构建系统**：Bazel
- **容器与编排**：Docker + Kubernetes
- **服务治理**：Istio
- **可观测性**：Prometheus + Grafana + ELK + Jaeger
- **交付链路**：GitLab CI + ArgoCD + Terraform

## 关键文档

- [产品设计文档](./docs/product-design.md)
- [技术架构总览](./docs/architecture/README.md)
- [文档先行开发规范](./docs/document-first-development.md)
- [工程约定（Bazel / 端口 / 测试 / gateway 分层）](./docs/engineering-conventions.md)（与各服务 `services/<service>/docs/development.md` 配合）
- [合规基线](./docs/compliance.md)
- [服务契约总览](./docs/contracts/README.md)
- [业务服务总览](./services/README.md)
