# recommendation-server — Changelog

## 2025-06-05 — 初始架构

- C 端导购读（`catalog_read`）+ 推荐编排单进程 `recommendation_server`。
- 内容真相来自 `backoffice-backend` snapshot（`-snapshot_dir`），不 RPC 运营库。
- 必须与 `backoffice-backend` 同节点共享导出目录。
