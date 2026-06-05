# backoffice-backend — Changelog

## 2025-06-05 — 导购 proto 迁入 common/

- `content_server.proto` 拆为 `common/proto/catalog.proto`（数据模型）+ `content_service.proto`（`ContentService` RPC）。
- 各消费方改依赖 `//common/proto:*_cc_proto`，不再经本服务 BUILD 导出导购 proto。

## 2025-06-05 — 初始架构

- 新建 `platform/backoffice-backend`：content + governance + affiliate 写面单进程。
- snapshot 导出：`catalog_snapshot`、`visibility_index`、`affiliate_link_spec`。
- gateway 运营写路径单一下游：`-backoffice_backend_addr`（`9110`）。
