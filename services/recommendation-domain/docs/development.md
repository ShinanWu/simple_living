# recommendation-domain — 开发与交付

本文说明本服务在仓库内的**实现、构建、运行、测试与合并前检查**约定，以及须对照的公共文档。

## 1. 本服务文档

| 文档 | 用途 |
|------|------|
| [README.md](./README.md) | 边界与依赖 |
| [api.md](./api.md) | 内部 RPC / proto |
| [data-model.md](./data-model.md) | 实体与枚举 |
| [workflow.md](./workflow.md) | 流程 |
| [pages.md](./pages.md) | 页面消费 |
| [changelog.md](./changelog.md) | 契约变更 |

## 2. 公共文档

| 文档 | 用途 |
|------|------|
| [docs/contracts/recommendation.md](../../../docs/contracts/recommendation.md) | `scene`、结果条目与导购卡引用 |
| [docs/contracts/pagination.md](../../../docs/contracts/pagination.md) | 分页形状 |
| [docs/engineering-conventions.md](../../../docs/engineering-conventions.md) | Bazel、端口、跨服务依赖、测试 |

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/recommendation-domain` |
| 二进制 | `//services/recommendation-domain:recommendation_domain_server` |

```bash
bazel build //services/recommendation-domain/...
bazel test //services/recommendation-domain/...
```

允许的提供方 Bazel 依赖：`user-domain`、`content-domain`；可选 `governance-domain`。不在本目录复制上述域的 `.proto`。

## 4. 运行

| 项 | 值 |
|----|-----|
| 默认 brpc 端口 | `9103`（`-port` 覆盖） |

```bash
bazel run //services/recommendation-domain:recommendation_domain_server -- -port=9103
```

若运行时调用下游，配置对应 `-*_domain_addr`（命名见 `docs/engineering-conventions.md` §4）。

## 5. 持久化与缓存（顶层设计）

- **MySQL** 或 **PostgreSQL** + **Redis**（特征、索引、离线任务与 Kafka 协同见 [`data-model.md`](./data-model.md)、[`workflow.md`](./workflow.md)）。细则见 `docs/engineering-conventions.md` §4.1。

## 6. 测试

最低期望：`HealthCheck`（若有）；一条核心推荐查询 RPC 在 **库表或联调环境** 下的成功路径。见 `docs/engineering-conventions.md` §5。

## 7. 合并前检查清单

- [ ] `api.md` 与 `proto/` 一致；`changelog.md` 已更新
- [ ] `data-model.md` 与 **MySQL 或 PostgreSQL** + **Redis**（及 Kafka，若适用）一致
- [ ] `bazel build` / `bazel test` 本包通过
- [ ] Bazel 仅通过目标依赖消费 user/content/governance proto；`scene` 等与 `docs/contracts/recommendation.md` 一致
