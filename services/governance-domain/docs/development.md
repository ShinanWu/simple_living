# governance-domain — 开发与交付

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
| [docs/engineering-conventions.md](../../../docs/engineering-conventions.md) | Bazel、端口、跨服务依赖、测试 |

披露、可见性等若出现在对外 JSON，须与 `docs/contracts/` 及 `services/gateway/docs/api.md` 一致。

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/governance-domain` |
| 二进制 | `//services/governance-domain:governance_domain_server` |

```bash
bazel build //services/governance-domain/...
bazel test //services/governance-domain/...
```

允许的提供方 Bazel 依赖：`content-domain`；可选 `user-domain`。见 `docs/engineering-conventions.md` §2.2。

## 4. 运行

| 项 | 值 |
|----|-----|
| 默认 brpc 端口 | `9106`（`-port` 覆盖） |

```bash
bazel run //services/governance-domain:governance_domain_server -- -port=9106
```

## 5. 持久化与缓存（顶层设计）

- **MySQL** 或 **PostgreSQL** + **Redis**（审核队列、可见性裁决、策略版本等见 [`data-model.md`](./data-model.md)）。细则见 `docs/engineering-conventions.md` §4.1。

## 6. 测试

最低期望：`HealthCheck`（若有）；一条审核/可见性相关核心 RPC 在 **库表或联调环境** 下的成功路径。见 `docs/engineering-conventions.md` §5。

## 7. 合并前检查清单

- [ ] `api.md` 与 `proto/` 一致；`changelog.md` 已更新
- [ ] `data-model.md` 与 **MySQL 或 PostgreSQL** + **Redis** 选型及迁移一致
- [ ] `bazel build` / `bazel test` 本包通过
- [ ] Bazel 依赖仅通过目标引用 content/user proto；变更已评估 `content-domain` / `recommendation-domain` / `gateway`
