# affiliate-domain — 开发与交付

本文说明本服务在仓库内的**实现、构建、运行、测试与合并前检查**约定，以及须对照的公共文档。

## 1. 本服务文档

| 文档 | 用途 |
|------|------|
| [README.md](./README.md) | 边界与依赖 |
| [api.md](./api.md) | 内部 RPC / proto |
| [data-model.md](./data-model.md) | 实体与枚举 |
| [workflow.md](./workflow.md) | 流程 |
| [pages.md](./pages.md) | 页面消费（经 gateway） |
| [changelog.md](./changelog.md) | 契约变更 |

## 2. 公共文档

| 文档 | 用途 |
|------|------|
| [docs/engineering-conventions.md](../../../docs/engineering-conventions.md) | Bazel、端口、跨服务依赖、测试 |

联盟相关对外 JSON 由 `gateway` 裁剪后暴露时，须与 `docs/contracts/` 及 `services/gateway/docs/api.md` 一致。

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/affiliate-domain` |
| 二进制 | `//services/affiliate-domain:affiliate_domain_server` |

```bash
bazel build //services/affiliate-domain/...
bazel test //services/affiliate-domain/...
```

默认无业务域 proto 依赖；`tracking-domain` 等消费方依赖本域导出目标。

## 4. 运行

| 项 | 值 |
|----|-----|
| 默认 brpc 端口 | `9104`（`-port` 覆盖） |

```bash
bazel run //services/affiliate-domain:affiliate_domain_server -- -port=9104
```

## 5. 测试

最低期望：`HealthCheck`（若有）；一条与 partner 能力或链接规格相关的核心读/算 RPC stub。见 `docs/engineering-conventions.md` §5。

## 6. 合并前检查清单

- [ ] `api.md` 与 `proto/` 一致；`changelog.md` 已更新
- [ ] `bazel build` / `bazel test` 本包通过
- [ ] 破坏性变更已评估 `tracking-domain`、`gateway` 消费者
