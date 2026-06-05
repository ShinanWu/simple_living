# backoffice-backend

## 1. 服务目标

`services/platform/backoffice-backend` 是 **运营管理后台的后端服务**，合并 content、governance、affiliate 的**权威写面**（OLTP），供 `backoffice-web` 经 `gateway` 的 `/api/v2/backoffice/*` 低频访问。

本服务**不**承接 C 端高频读。发布/审核/下架/伙伴配置变更后，通过**版本化 snapshot 导出物**供同节点的 `recommendation-server`（导购内容读 + 推荐）与 `tracking-server`（转链规格）热加载消费。

## 2. 内部模块

| 模块 | 职责 | PostgreSQL schema |
|------|------|-------------------|
| **content** | 导购卡片、专题、榜单、攻略、主题 CMS 写入 | `content` |
| **governance** | 审核队列、可见性最终门闸、商业披露元数据、运营配置 | `governance` |
| **affiliate** | partner 能力、佣金规则、链接生成输入校验（密钥隔离） | `affiliate` |

模块间发布/审核/可见性在**进程内**协作。对外通过独立 proto service 注册：`ContentService`（定义于 [`common/proto/content_service.proto`](../../../common/proto/content_service.proto)，数据模型见 `catalog.proto`）、本服务 `governance_server` / `affiliate_server`。

## 3. 非职责

| 能力 | 归属 |
|------|------|
| C 端导购内容读、推荐召回排序 | `recommendation-server`（读 snapshot） |
| 转链执行、点击写入、归因 | `tracking-server` |
| 用户账户、收藏 | `user-server` |
| JSON↔proto、页面聚合 | `gateway` |
| 运营后台 UI | `platform/backoffice-web` |

## 4. 上下游

### 上游（写入/查询）

| 来源 | 说明 |
|------|------|
| `gateway` | `/api/v2/backoffice/*` 转发至本服务对应模块 RPC |
| `platform/backoffice-web` | 经 gateway 间接调用 |

### 下游（数据面，非 RPC）

| 消费方 | 机制 | 导出 bundle |
|--------|------|-------------|
| `recommendation-server` | 同节点热加载 | `catalog_snapshot`、`visibility_index` |
| `tracking-server` | 同节点热加载 | `affiliate_link_spec`（**不含明文密钥**） |

## 5. 部署约束（v1）

- **必须与 `recommendation-server` 同节点 co-locate**（共享 `-export_dir`，默认 `/var/lib/simple-living/exports/`）。
- 建议与 `tracking-server` 同节点以便读取 `affiliate_link_spec`。
- 默认端口 `9110`；Bazel 目标 `//services/platform/backoffice-backend:backoffice_backend_server`。

## 6. 文档索引

| 文档 | 说明 |
|------|------|
| [development.md](./development.md) | 构建、运行、配置、可观测、安全 |
| [../deploy/README.md](../deploy/README.md) | 部署与回滚 |
| [api.md](./api.md) | 内部 RPC + gateway backoffice 映射 |
| [data-model.md](./data-model.md) | 三 schema DDL 与迁移 |
| [workflow.md](./workflow.md) | 运营写链路 + 导出发布 |
| [pages.md](./pages.md) | 经 gateway 间接消费的页面映射 |
| [changelog.md](./changelog.md) | 契约变更 |
