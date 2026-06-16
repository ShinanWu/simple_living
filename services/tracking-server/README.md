# tracking-server

点击记录、受控跳转、归因快照、转化回传、佣金读视图。`landing_url` 组装与 short-token 解析在本域；联盟密钥与链接规格来自 `backoffice-backend` snapshot。

- **RPC 契约**：[api.md](./api.md)
- **部署**：[deploy/README.md](./deploy/README.md)
- **终端 JSON**：[`services/gateway/api.md`](../gateway/api.md) §5.13、`§13.4`

## 职责

跳转链组装、点击去重、redirect 解析、转化 ingest、佣金 dashboard 读模型。

`AssembleTrackingLink` 成功时 **必须** 返回非空 HTTPS `landing_url`（请求 URL → snapshot 卡片 landing → `https://go.shaotang.com/r/{short_token}`）；`InsertLink` 失败时返回 gRPC `FAILED_PRECONDITION`，禁止空响应成功。详见 [api.md §3.1/§3.4](./api.md)。

## 非职责

伙伴 API 凭据、联盟 URL 语法真相、对外 HTTP/JSON（gateway）、法律结算。

## 依赖

| 服务 | 文档 | 用途 |
|------|------|------|
| `platform/backoffice-backend` | [api.md](../platform/api.md) | `affiliate_link_spec` snapshot（文件） |
| `gateway` | [api.md](../gateway/api.md) | 对外映射 |

**同节点**：`-snapshot_dir` 与 backoffice 导出目录一致（读 `affiliate_link_spec`）。

## 本地开发

```bash
bazel build //services/tracking-server/...
bazel test //services/tracking-server/...

bazel run //services/tracking-server:tracking_server -- \
  -port=9105 \
  -pg_conninfo="host=127.0.0.1 port=5432 dbname=tracking user=tracking password=$PG_PWD" \
  -snapshot_dir=/var/lib/simple-living/exports
```

| flag | 必填 | 说明 |
|------|------|------|
| `-pg_conninfo` | 是 | PostgreSQL |
| `-snapshot_dir` | 是 | 联盟 link spec 导出 |
| `-redis_addr` | 否 | 限流/去重缓存 |
| `-kafka_brokers` | 否 | outbox / 转化消费 |

允许依赖 `//common/proto:catalog_cc_proto`。禁止依赖 `backoffice-backend` BUILD 目标。

## 安全与密钥

连接串、HMAC 签名密钥、伙伴 API 凭据均经部署环境或 secret backend 注入；不得写入仓库、请求体或对外 snapshot。`IngestConversion` 仅允许集群内可信调用方。

## SLO

| RPC | P99 |
|-----|-----|
| `AssembleTrackingLink` | ≤ 120ms |
| `ResolveRedirect`（缓存命中） | ≤ 80ms |
