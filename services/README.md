# 业务服务总览

## 1. 服务划分原则

本项目服务按业务域划分，而不是按技术层或衣食住行主题重复拆分。

这样可以让：

- 业务规则更集中
- 文档和代码更贴近
- AI 或人工协作时边界更稳定

## 2. 服务清单

### `gateway`

- 统一入口
- 鉴权、限流、聚合、协议转换

### `user-domain`

- 用户账户
- 用户偏好
- 收藏、历史、反馈

### `content-domain`

- 导购内容
- 商品/服务卡片
- 榜单、专题、攻略
- 衣食住行内容主题管理

### `recommendation-domain`

- 召回与排序
- 个性化推荐
- 热门推荐
- 推荐解释

### `affiliate-domain`

- 渠道平台接入
- 佣金规则管理
- 渠道能力封装
- partner-specific URL 规则、签名与 token 能力

### `tracking-domain`

- 面向客户端的 `landing_url` 组装
- 点击追踪
- 渠道跳转
- 归因处理

### `governance-domain`

- 内容审核
- 商业标识
- 配置运营
- 风险控制与下架

## 3. 每个服务必须具备的文档

```text
services/<service>/docs/
├── README.md
├── development.md    # 本服务实现与交付（Bazel、端口、测试、合并前检查）
├── api.md
├── pages.md
├── data-model.md
├── workflow.md
└── changelog.md
```

## 4. 每个服务建议具备的实现目录

```text
services/<service>/
├── proto/
├── src/
├── tests/
└── BUILD.bazel
```

说明：

- `services/<service>/docs/` 是该服务的**人工维护契约入口**
- `services/<service>/` 是该服务的**实现与协议落地目录**
- `proto` 与实现代码应和服务边界一起内聚，不再额外放到 `server/` 之类的技术层目录

## 5. 设计要求

- 一个服务只负责一个稳定业务边界
- 一个服务内部允许文档、规则、代码紧密耦合
- 服务之间只能通过公开契约协作
- 公共能力尽量少而精，避免沉淀过多无边界代码到 `common`

## 6. 实现与交付文档从哪里读

每个服务的 **全周期说明**（本包 Bazel 目标、默认端口、`bazel run`/`test`、必读公共契约、PR Done 清单）在：

```text
services/<service>/docs/development.md
```

**跨服务公共约定**（端口全表、Bazel 跨域依赖矩阵、`gateway` HTTPS+JSON 与内部 brpc 分层、测试策略、CI）在 [`docs/engineering-conventions.md`](../docs/engineering-conventions.md)。**公共 JSON 语义**在 [`docs/contracts/`](../docs/contracts/)。

各服务 `docs/README.md` 已索引 `development.md`；无需额外「集中式开发目录」。
