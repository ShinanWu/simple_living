# recommendation-server — 开发与交付

## 1. 本服务文档

| 文档 | 用途 |
|------|------|
| [README.md](./README.md) | 边界、模块、snapshot |
| [api.md](./api.md) | RPC |
| [data-model.md](./data-model.md) | snapshot 消费与本服务索引 |
| [workflow.md](./workflow.md) | 热加载与读路径 |
| [pages.md](./pages.md) | 页面消费 |
| [changelog.md](./changelog.md) | 变更 |

## 2. 公共文档

| 文档 | 用途 |
|------|------|
| [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) | 推荐结果、导购卡、scene、错误码 |
| [业务服务总览](../../README.md) | 端口、拓扑 |
| [backoffice-backend 导出](../../platform/docs/backend-workflow.md) | manifest 与 bundle 格式 |
| [user-server api.md](../../user-server/docs/api.md) | signal bundle |

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/recommendation-server` |
| 二进制 | `//services/recommendation-server:recommendation_server` |

```bash
bazel build //services/recommendation-server/...
bazel test //services/recommendation-server/...
```

允许依赖：`user-server` proto；`//common/proto:catalog_cc_proto` 与 `content_service_cc_proto`。**禁止**依赖 `platform/backoffice-backend` 的 BUILD 目标（读 snapshot 文件，不拉写面 proto 包）。

## 4. 运行与配置

| Flag / 环境变量 | 默认 | 必填 | 说明 |
|-----------------|------|------|------|
| `-port` | `9103` | 否 | brpc |
| `-user_server_addr` | `brpc://127.0.0.1:9101` | 否 | 用户信号 |
| `-snapshot_dir` | `/var/lib/simple-living/exports` | **是** | 与 backoffice `-export_dir` 相同 |
| `-snapshot_poll_ms` | `500` | 否 | manifest 轮询 |
| `-mmap_max_catalog_mb` | `512` | 否 | 单 bundle 上限 |

同节点：与 `platform/backoffice-backend` 共享 `snapshot_dir` 挂载。

## 5. 测试

- 单元：visibility 过滤、排序、manifest 切换
- 集成：fixture snapshot 目录 + `QueryRecommendations` happy path
- 契约：与 `api.md` 错误码一致

## 6. 合并前检查

- [ ] 无对 content/governance brpc 调用
- [ ] snapshot 格式与 backoffice `data-model.md` 一致
- [ ] gateway 读路径指向 `-recommendation_server_addr`
- [ ] changelog 记录架构变更
