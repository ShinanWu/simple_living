# backoffice-backend

运营管理写面：导购内容、治理审核、联盟配置。PostgreSQL 为权威；经文件导出向 C 端读路径提供 snapshot。

- **RPC 契约**：[api.md](../api.md)
- **运营 HTTP**：[backoffice-gateway-api.md](../backoffice-gateway-api.md)（经 gateway 暴露）
- **部署**：[deploy/README.md](./deploy/README.md)
- **终端 JSON**：[`services/gateway/api.md`](../../gateway/api.md)

## 职责

| 模块 | 说明 |
|------|------|
| content | 导购卡、主题、素材与发布流水线 |
| governance | 可见性、审核与运营裁决 |
| affiliate | 联盟链接规格、渠道配置（密钥经 secret backend） |
| export | `catalog_snapshot`、`visibility_index`、`affiliate_link_spec` 等同节点导出 |

## 非职责

C 端推荐排序、对外 HTTPS/JSON（gateway）、点击归因（tracking-server）、用户状态（user-server）。

## 依赖

| 方向 | 服务 | 文档 |
|------|------|------|
| 消费方 | `gateway` | [api.md](../../gateway/api.md)、[backoffice-gateway-api.md](../backoffice-gateway-api.md) |
| 导出消费 | `recommendation-server`、`tracking-server` | 同挂载 `-export_dir` / `-snapshot_dir` |
| 基础设施 | PostgreSQL、Redis、Kafka | [foundation/postgres](../../foundation/postgres/README.md) 等 |

## 本地开发

```bash
bazel build //services/platform/backoffice-backend/...
bazel test //services/platform/backoffice-backend/...

bazel run //services/platform/backoffice-backend:backoffice_backend_server -- \
  -port=9110 \
  -pg_conninfo="host=127.0.0.1 port=5432 dbname=platform user=platform password=$PG_PWD" \
  -export_dir=/var/lib/simple-living/exports
```

| flag | 必填 | 说明 |
|------|------|------|
| `-port` | 否 | 默认 `9110` |
| `-pg_conninfo` | 是 | PostgreSQL |
| `-export_dir` | 是 | snapshot 导出根目录 |
| `-secret_backend_uri` | 否 | 联盟密钥等机密来源 |

机密与签名材料经部署环境或 secret backend 注入，不入库、不进 snapshot、不进仓库。

## SLO

| 路径 | P99 |
|------|-----|
| 运营写 RPC（单实体） | ≤ 200ms |
| 全量导出（增量） | 分钟级；滞后由 manifest `active_version` 观测 |
