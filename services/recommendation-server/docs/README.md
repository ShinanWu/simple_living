# recommendation-server

## 1. 服务目标

`recommendation-server` 是 **C 端导购内容读 + 推荐中心**，合并原 recommendation 域的召回排序能力，以及原 content / governance 域的 **C 端只读面**（经 snapshot 物化，不直连运营库）。

职责对齐产品「推荐中心」+ 导购内容消费：首页 feed、详情读、相关推荐、热门榜单。

## 2. 内部模块

| 模块 | 职责 |
|------|------|
| **catalog_read** | 从 `catalog_snapshot` + `visibility_index` mmap 提供 `BatchGetGuideCards`、`GetTopic`、`ListRankings` 等 |
| **recommendation** | `QueryRecommendations`、`GetPopularRecommendations`、解释字段；候选来自 snapshot 索引 + `user-server` 信号 |

## 3. 非职责

| 能力 | 归属 |
|------|------|
| 内容/审核/伙伴 **写入** | `platform/backoffice-backend` |
| 用户账户、收藏 | `user-server` |
| 转链、点击、归因 | `tracking-server` |
| 对外 JSON | `gateway` |

## 4. 数据权威与一致性

- **无 PostgreSQL 业务权威表**（v1）；策略配置与候选池索引可使用本地 RocksDB/内存 + 可选 PG 仅本服务私有表（见 data-model.md）。
- 导购内容真相来自 `platform/backoffice-backend` 导出物；本服务只读 mmap。
- 可见性以 `visibility_index` 为准，fail-closed。
- 新鲜度：全量发布 ≤ 5s；下架 ≤ 1s（与 backoffice 导出 SLO 对齐）。

## 5. 部署约束（v1）

- **必须与 `backoffice-backend` 同节点**；`-snapshot_dir` = backoffice `-export_dir`。
- 默认端口 `9103`；二进制目标 `//services/recommendation-server:recommendation_server`（实现阶段由 `recommendation_server` 重命名）。

## 6. 依赖

| 依赖 | 方式 |
|------|------|
| `user-server` | brpc：signal bundle / consent |
| `platform/backoffice-backend` | **仅文件导出**（禁止在线 RPC 读运营库） |

## 7. SLO（v1）

| 指标 | 目标 |
|------|------|
| `QueryRecommendations` 可用性 | ≥ 99.9% |
| `QueryRecommendations` P99（snapshot 命中） | ≤ 80ms |
| `BatchGetGuideCards` P99 | ≤ 20ms |
| snapshot 切换失败 | readiness 失败，不返回未校验数据 |

## 8. 文档索引

| 文档 | 说明 |
|------|------|
| [development.md](./development.md) | 构建、mmap 配置、测试 |
| [api.md](./api.md) | RPC（含 catalog_read + recommendation） |
| [data-model.md](./data-model.md) | snapshot 消费模型、本服务私有索引 |
| [workflow.md](./workflow.md) | 读路径、热加载、降级 |
| [pages.md](./pages.md) | C 端页面映射 |
| [changelog.md](./changelog.md) | 变更 |

## 9. 迁移

替代：

- `services/recommendation-server/`（已重命名本目录）
- C 端读路径原 content / governance 域在线 RPC（现为 snapshot + `CatalogReadService`）

写路径请阅 `services/platform/docs/`（[backend-workflow.md](../../platform/docs/backend-workflow.md)）。
