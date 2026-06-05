# backoffice-backend — 开发与交付

## 1. 文档索引

| 文档 | 用途 |
|------|------|
| [README.md](./README.md) | 边界、模块、导出概览 |
| [api.md](./api.md) | RPC 与 gateway 映射 |
| [data-model.md](./data-model.md) | DDL 与导出模型 |
| [workflow.md](./workflow.md) | 写链路 + 导出 |
| [changelog.md](./changelog.md) | 变更记录 |

公共：`services/README.md`、`.cursor/rules/shared-contracts.mdc`。

## 2. Bazel（目标，实现阶段落地）

| 项 | 值 |
|----|-----|
| 包 | `//services/platform/backoffice-backend` |
| 二进制 | `//services/platform/backoffice-backend:backoffice_backend_server` |

```bash
bazel build //services/platform/backoffice-backend/...
bazel test //services/platform/backoffice-backend/...
```

本服务**不**依赖 `recommendation-server` 的 proto；读侧仅消费文件导出物。

## 3. 运行与配置

| Flag / 环境变量 | 默认 | 必填 | 说明 |
|-----------------|------|------|------|
| `-port` | `9110` | 否 | brpc 监听 |
| `-listen_addr` | `0.0.0.0` | 否 | 绑定地址 |
| `-pg_conninfo` | — | **是** | PostgreSQL |
| `-export_dir` | `/var/lib/simple-living/exports` | 否 | snapshot 输出根目录 |
| `-export_poll_ms` | `500` | 否 | 导出 worker 轮询 outbox |
| `-kafka_brokers` | — | 否 | 事件发布（可空则仅写 outbox 表） |
| `-secret_backend_uri` | — | **是**（affiliate） | 签名密钥来源 |

同节点约束：`export_dir` 必须与 `recommendation-server` 的 `-snapshot_dir` 指向同一挂载。

## 4. 优雅退出

`server.RunUntilAskedToQuit()`；退出顺序：停 brpc → 等待导出 worker 完成当前 staging → 关闭 PG 连接池 → flush outbox（best-effort）。

## 5. 可观测性

| 类型 | 项 |
|------|-----|
| health | brpc `HealthCheck`；另检查 PG 连通与最近 `export_job.status=active` 滞后 < 阈值 |
| metrics | `backoffice_write_qps`、`export_lag_seconds`、`export_fail_total`、各模块 RPC 延迟 |
| logs | 结构化 JSON：`request_id`、`module`、`rpc`、`actor` |

## 6. 安全

- 信任边界：仅接受来自 `gateway` 服务账号的运营身份断言（mTLS 或内网 ACL v1 可简化为 localhost + 端口隔离）。
- affiliate 密钥：进程内独立 `AffiliateSecretProvider`；日志脱敏；禁止 dump 到导出文件。
- PostgreSQL 使用最小权限角色；三 schema 可共连接串，应用层命名空间隔离。

## 7. 合并前检查

- [ ] proto 与 `api.md` 一致
- [ ] DDL 与 `data-model.md` 一致
- [ ] gateway `backoffice-backend.md` 下游地址指向 `-backoffice_backend_addr`
- [ ] 导出 manifest 格式与 `recommendation-server` 文档一致
- [ ] changelog 记录对三旧域的替代关系
