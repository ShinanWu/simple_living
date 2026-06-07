# 后端开发与交付

`backoffice-backend` 构建、运行、配置与可观测性。产品与 design 见 [README.md](./README.md)、[detail-design.md](./detail-design.md)。

## Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/platform/backoffice-backend` |
| 二进制 | `//services/platform/backoffice-backend:backoffice_backend_server` |

```bash
bazel build //services/platform/backoffice-backend/...
bazel test //services/platform/backoffice-backend/...
```

本服务不依赖 `recommendation-server` 的 proto；读侧仅消费文件导出物。

## 运行与配置

| Flag | 默认 | 必填 | 说明 |
|------|------|------|------|
| `-port` | `9110` | 否 | brpc 监听 |
| `-listen_addr` | `0.0.0.0` | 否 | 绑定地址 |
| `-pg_conninfo` | — | **是** | PostgreSQL |
| `-export_dir` | `/var/lib/simple-living/exports` | 否 | snapshot 输出根目录 |
| `-export_poll_ms` | `500` | 否 | 导出 worker 轮询 outbox |
| `-kafka_brokers` | — | 否 | 事件发布（可空则仅写 outbox 表） |
| `-secret_backend_uri` | — | **是**（affiliate） | 签名密钥来源 |

同节点约束：`export_dir` 必须与 `recommendation-server` 的 `-snapshot_dir` 指向同一挂载。

部署脚本见 `backoffice-backend/deploy/`；拓扑见 [README.md](./README.md) §部署。

## 优雅退出

`server.RunUntilAskedToQuit()`；退出顺序：停 brpc → 等待导出 worker 完成当前 staging → 关闭 PG 连接池 → flush outbox（best-effort）。

## 可观测性

| 类型 | 项 |
|------|-----|
| health | brpc `HealthCheck`；PG 连通与 `export_job.status=active` 滞后 |
| metrics | `backoffice_write_qps`、`export_lag_seconds`、`export_fail_total`、各模块 RPC 延迟 |
| logs | 结构化 JSON：`request_id`、`module`、`rpc`、`actor` |

## 安全

- 信任边界：仅接受来自 gateway 的运营身份断言（v1 可简化为 localhost + 端口隔离）。
- affiliate 密钥：进程内 `AffiliateSecretProvider`；日志脱敏；禁止写入导出文件。
- PostgreSQL 最小权限；三 schema 共连接串，应用层隔离。

## 合并前检查

- [ ] proto 与 [backend-api.md](./backend-api.md) 一致
- [ ] DDL 与 [backend-data-model.md](./backend-data-model.md) 一致
- [ ] gateway 下游地址指向 `-backoffice_backend_addr`
- [ ] 导出 manifest 与 `recommendation-server` 文档一致
