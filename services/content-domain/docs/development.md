# content-domain — 开发与交付

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
| [docs/contracts/guide-card.md](../../../docs/contracts/guide-card.md) | 导购卡公共语义（gateway 对齐） |
| [docs/engineering-conventions.md](../../../docs/engineering-conventions.md) | Bazel、端口、跨服务依赖、测试 |

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/content-domain` |
| 二进制 | `//services/content-domain:content_domain_server` |

```bash
bazel build //services/content-domain/...
bazel test //services/content-domain/...
```

允许的提供方 Bazel 依赖：`governance-domain`；可选 `affiliate-domain`。见 `docs/engineering-conventions.md` §2.2。

## 4. 运行

| 项 | 值 |
|----|-----|
| 默认 brpc 端口 | `9102`（`-port` 覆盖） |

```bash
bazel run //services/content-domain:content_domain_server -- -port=9102
```

## 5. 持久化与缓存（顶层设计）

- **MySQL** 或 **PostgreSQL**（与架构总览统一选型）+ **Redis**（读多写少场景）；表结构见 [`data-model.md`](./data-model.md)。细则见 `docs/engineering-conventions.md` §4.1。

## 6. 测试

最低期望：`HealthCheck`（若有）；至少一条核心读 RPC 在 **库表或联调环境** 下的成功返回。见 `docs/engineering-conventions.md` §5。

## 7. 合并前检查清单

- [ ] `api.md` 与 `proto/` 一致；`changelog.md` 已更新
- [ ] `data-model.md` 与 **MySQL 或 PostgreSQL** + **Redis** 选型及迁移一致
- [ ] `bazel build` / `bazel test` 本包通过
- [ ] 端口与 Bazel 对外依赖与工程约定一致；破坏性 proto 变更已评估 `gateway` / `recommendation-domain` 消费者
