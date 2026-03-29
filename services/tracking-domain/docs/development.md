# tracking-domain — 开发与交付

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
| [docs/contracts/redirect-attribution.md](../../../docs/contracts/redirect-attribution.md) | `landing_url`、`click_id` 等共享语义 |
| [docs/engineering-conventions.md](../../../docs/engineering-conventions.md) | Bazel、端口、跨服务依赖、测试 |

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/tracking-domain` |
| 二进制 | `//services/tracking-domain:tracking_domain_server` |

```bash
bazel build //services/tracking-domain/...
bazel test //services/tracking-domain/...
```

允许的提供方 Bazel 依赖：`affiliate-domain`（链接规格、签名能力等）。不复制 `affiliate-domain` 的 `.proto`。

## 4. 运行

| 项 | 值 |
|----|-----|
| 默认 brpc 端口 | `9105`（`-port` 覆盖） |

```bash
bazel run //services/tracking-domain:tracking_domain_server -- \
  -port=9105 \
  -affiliate_domain_addr=brpc://127.0.0.1:9104
```

## 5. 测试

最低期望：`HealthCheck`（若有）；`AssembleTrackingLink` 或 `ResolveRedirect` 之一 stub 路径。见 `docs/engineering-conventions.md` §5。

## 6. 合并前检查清单

- [ ] `api.md` 与 `proto/` 一致；`changelog.md` 已更新
- [ ] `bazel build` / `bazel test` 本包通过
- [ ] 通过 Bazel 依赖 `affiliate-domain` proto；对外可见字段与 `docs/contracts/redirect-attribution.md` 及 `gateway` 一致
