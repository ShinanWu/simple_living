# user-domain — 开发与交付

本文说明本服务在仓库内的**实现、构建、运行、测试与合并前检查**约定，以及须对照的公共文档。

## 1. 本服务文档

| 文档 | 用途 |
|------|------|
| [README.md](./README.md) | 边界与依赖 |
| [api.md](./api.md) | 内部 RPC / proto 契约（与 `proto/` 对齐） |
| [data-model.md](./data-model.md) | 实体与枚举 |
| [workflow.md](./workflow.md) | 流程 |
| [pages.md](./pages.md) | 经 gateway 暴露的页面消费关系 |
| [changelog.md](./changelog.md) | 契约变更 |

## 2. 公共文档

| 文档 | 用途 |
|------|------|
| [docs/contracts/auth.md](../../../docs/contracts/auth.md) | 鉴权与令牌语义（gateway 映射依据） |
| [docs/engineering-conventions.md](../../../docs/engineering-conventions.md) | Bazel、端口表、禁止 fork proto、测试与 CI |

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/user-domain` |
| 二进制 | `//services/user-domain:user_domain_server` |
| Proto | `proto/*.proto` |

```bash
bazel build //services/user-domain/...
bazel test //services/user-domain/...
```

## 4. 运行

| 项 | 值 |
|----|-----|
| 默认 brpc 端口 | `9101`（`-port` 覆盖） |
| 下游依赖 | 默认无业务域 brpc 依赖 |

```bash
bazel run //services/user-domain:user_domain_server -- -port=9101
```

无持久化时可使用 **dev stub**（内存存储）；健康检查与降级语义须在 `api.md` 说明。

## 5. 跨服务依赖

本域默认无业务域 proto 依赖。消费方（`gateway`、`recommendation-domain`）在 Bazel 中依赖本域导出目标 `//services/user-domain:user_domain_proto` 等；**禁止** 其他域复制本域 `.proto`。

## 6. 测试

最低期望：`HealthCheck`（若已定义）；`IssueTokenPair` 或 `EnsureGuestSession` 之一 happy path（可内存存储）。详见 `docs/engineering-conventions.md` §5。

## 7. 合并前检查清单

- [ ] `api.md` 与 `proto/` 一致；`changelog.md` 已更新
- [ ] `bazel build` / `bazel test` 本包通过
- [ ] 默认端口 `9101` 与 `docs/engineering-conventions.md` §4 一致（若调整则同步该文与 `services/gateway/docs/development.md`）
