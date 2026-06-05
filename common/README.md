# common（跨服务共享资源）

仓库级共享契约与工具，**不承载业务服务逻辑**。各 `services/*` 通过 Bazel 依赖本目录目标，禁止拷贝/fork proto。

| 子目录 | 用途 |
|--------|------|
| [`proto/`](./proto/) | 跨服务 protobuf：导购数据模型（`catalog`）、读 RPC（`content_service`）等 |
| [`util/`](./util/) | （预留）无状态 C++ 辅助库 |
| [`config/`](./config/) | （预留）共享配置 schema / 常量 |

## 依赖规则

| 消费方 | 允许依赖 |
|--------|----------|
| `recommendation-server` | `//common/proto:catalog_cc_proto`；读 RPC 另加 `content_service_cc_proto` |
| `tracking-server` | `//common/proto:catalog_cc_proto`（解析 snapshot JSON） |
| `gateway` | `catalog_cc_proto` + `content_service_cc_proto` + 各服务 RPC proto |
| `platform/backoffice-backend` | `catalog_cc_proto` + `content_service_cc_proto`（写面实现完整 `ContentService`） |

**禁止** C 端读服务依赖 `platform/backoffice-backend` 的 BUILD 目标；snapshot 字段形状以 `catalog.proto` 为准。
