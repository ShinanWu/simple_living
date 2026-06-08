# recommendation-server

C 端导购内容读（snapshot）+ 推荐中心：召回、排序、解释。从 `backoffice-backend` 导出的 mmap snapshot 读 catalog/可见性，**不直连运营库**。

- **RPC 契约**：[api.md](./api.md)
- **部署**：[deploy/README.md](./deploy/README.md)（须与 backoffice **同节点**共享 `-snapshot_dir`）
- **终端 JSON**：[`services/gateway/api.md`](../gateway/api.md) §13

## 职责

| 模块 | 说明 |
|------|------|
| catalog_read | `BatchGetGuideCards` 等，读 `catalog_snapshot` + `visibility_index` |
| recommendation | `QueryRecommendations`、`GetPopularRecommendations`；候选来自 snapshot + `user-server` 信号 |

## 非职责

运营写、治理审核、联盟配置、对外 HTTP/JSON（gateway）、直连 `backoffice-backend` brpc。

## 依赖

| 服务 | 文档 | 用途 |
|------|------|------|
| `platform/backoffice-backend` | [api.md](../platform/api.md) | snapshot 导出（文件，非 RPC） |
| `user-server` | [api.md](../user-server/api.md) | 信号 bundle、同意门控 |
| `gateway` | [api.md](../gateway/api.md) | 对外映射 |

**硬约束**：`-snapshot_dir` = backoffice `-export_dir`（默认 `/var/lib/simple-living/exports/`）。

## 本地开发

```bash
bazel build //services/recommendation-server/...
bazel test //services/recommendation-server/...

bazel run //services/recommendation-server:recommendation_server -- \
  -port=9103 \
  -snapshot_dir=/var/lib/simple-living/exports \
  -user_server_addr=brpc://127.0.0.1:9101
```

| flag | 必填 | 说明 |
|------|------|------|
| `-snapshot_dir` | 是 | 与 backoffice 导出目录相同 |
| `-user_server_addr` | 否 | 默认 `brpc://127.0.0.1:9101` |
| `-snapshot_poll_ms` | 否 | manifest 轮询，默认 500 |

允许 Bazel 依赖：`user-server` proto、`//common/proto:catalog_cc_proto`。禁止依赖 `backoffice-backend` BUILD 目标。

## SLO

| 指标 | 目标 |
|------|------|
| `QueryRecommendations` P99（snapshot 命中） | ≤ 80ms |
| snapshot 切换失败 | readiness 失败，不返回未校验数据 |
