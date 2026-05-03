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

### `proxy`

- 用户入口流量接入（Nginx / FRP）
- TLS 终止与反向代理入口规则
- 边缘连通性与入口健康检查脚本

### `platform/backoffice-web`

- 运营管理后台 Web（platform 侧）
- 覆盖 affiliate/content/governance 最小运营动作
- 单服务部署入口：`services/platform/backoffice-web/deploy/`

### `foundation/postgres`

- 关系型存储运行服务
- 默认库与账号用于本地/联调环境

### `foundation/redis`

- 缓存运行服务
- 会话与热数据缓存基础能力

### `foundation/kafka`

- 异步消息运行服务（Kafka 协议兼容）
- 事件回传与解耦链路基础能力

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

```text
services/<service>/deploy/
├── README.md
├── start_nodes.sh
├── check_nodes.sh
├── deploy_service.sh
└── stop_nodes.sh
```

## 4. 每个服务必须具备的实现目录

业务域服务（gateway 与各 domain）：

```text
services/<service>/
├── proto/
├── src/
├── tests/
└── BUILD.bazel
```

基础运行服务（postgres/redis/kafka/proxy）：

```text
services/<service>/
├── docs/
├── deploy/
├── tests/
└── BUILD.bazel
```

说明：

- `services/<service>/docs/` 是该服务的**人工维护契约入口**
- `services/<service>/` 是该服务的**实现与协议落地目录**
- `services/<service>/deploy/` 是该服务的**独立部署入口**（单服务构建/分发/部署）
- `proto` 与实现代码应和服务边界一起内聚，不再额外放到 `server/` 之类的技术层目录

> 目录约束按“服务类型”执行：业务域服务需包含 `proto/src/tests/BUILD.bazel`；基础运行服务需包含 `docs/deploy/tests/BUILD.bazel`。

## 5. 设计要求

- 一个服务只负责一个稳定业务边界
- 一个服务内部允许文档、规则、代码紧密耦合
- 服务之间只能通过公开契约协作
- 公共能力尽量少而精，避免沉淀过多无边界共享代码
- 服务之间禁止通过源码目录相互引用实现细节（例如直接 include 他域 `src/` 私有头文件）

## 6. 实现与交付文档从哪里读

每个服务的 **全周期说明**（本包 Bazel 目标、默认端口、`bazel run`/`test`、必读公共契约、PR Done 清单）在：

```text
services/<service>/docs/development.md
```

**跨服务公共约定**（端口全表、Bazel 跨域依赖矩阵、`gateway` HTTPS+JSON 与内部 brpc 分层、测试策略、CI）在 [`docs/engineering-conventions.md`](../docs/engineering-conventions.md)。**公共 JSON 语义**在 [`docs/contracts/`](../docs/contracts/)。

各服务 `docs/README.md` 已索引 `development.md`；无需额外「集中式开发目录」。
