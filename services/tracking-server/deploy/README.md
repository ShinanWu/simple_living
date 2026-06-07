# tracking-server 独立部署说明（QEMU）

## 1. 执行入口

```bash
bash services/tracking-server/deploy/start_nodes.sh
bash services/tracking-server/deploy/check_nodes.sh
bash services/tracking-server/deploy/deploy_service.sh
```

仅处理 `tracking-server`，不会构建或发布其他服务。

部署前确认依赖就绪：PostgreSQL（已应用 `NNNN_tracking_*.sql` 迁移）、Redis（可选）、Kafka（outbox relay）、与 `backoffice-backend` 同节点共享 `-snapshot_dir`。配置：`-pg_conninfo`、`-redis_addr`、`-kafka_brokers`、token 签名密钥（见 docs/development.md §4）。

## 2. 回滚

```bash
# 回滚到指定镜像版本（重新部署旧 tag）
IMAGE_TAG=v2026.04.27 bash services/tracking-server/deploy/deploy_service.sh
```

停止当前服务相关节点：

```bash
bash services/tracking-server/deploy/stop_nodes.sh
```

回滚要点：
- **优雅退出**：先发 SIGTERM 让在途 `ResolveRedirect`/`IngestConversion` 完成并关闭 PostgreSQL/Redis/Kafka 连接，再切流量（见 graceful-shutdown 约定）。
- **数据兼容**：仅回滚二进制；不回滚已应用的 PostgreSQL 迁移（迁移向后兼容，旧版本可读新表）。如回滚跨越破坏性迁移，先评估 `changelog.md` 标注。
- **outbox 不丢**：回滚不清空 `tracking_outbox`；relay 重启后继续投递未发布行（消费方按 `event_id` 幂等，重复无害）。

## 3. 验收

```bash
# 健康检查（就绪含 PostgreSQL 连通性）
ssh -p 2206 ubuntu@127.0.0.1 "curl -fsS http://127.0.0.1:9105/healthz"
```

冒烟验收（联调/预发）：
- `AssembleTrackingLink` 返回 `landing_url` + `short_token`，DB 落 `tracking_link` / `tracking_short_token` 与一条 `tracking.link.created` outbox 行。
- `GET /t/{short_token}` 先落 `tracking_click` 再 302 到 partner URL；过期 token 返回 `GONE`/`50003`。
- `IngestConversion` 同 `(source, external_event_id)` 重放返回 `DUPLICATE`，不重复计数。
- `/metrics` 暴露 RPC 延迟、点击吞吐、outbox 积压、affiliate 下游失败率等关键指标。

## 4. 排障

| 症状 | 排查 |
|------|------|
| `/healthz` 非 200 | 检查 `-pg_conninfo` 与 PostgreSQL 可达性、迁移是否已应用（`schema_migrations`）。 |
| assemble 报 `50002` | `affiliate_link_spec` snapshot 缺失或过期；查 `-snapshot_dir` 与 backoffice-backend 导出任务。 |
| 跳转报 `50003`/`GONE` | 短 token 过期或被吊销；查 `tracking_short_token.expires_at`/`revoked`。 |
| 点击重复计数 | 检查 `device_dedup_key` 传入与 `uq_click_dedup` 唯一索引；Redis 幂等窗口是否生效。 |
| 转化未入库/重复 | 查 `uq_conv_source_evt` 唯一约束与 `external_event_id` 来源；`UNMATCHED` 为正常态非错误。 |
| outbox 积压上升 | relay 进程/`-kafka_brokers` 连通性；查 `tracking_outbox` 中 `published_at IS NULL` 行数与告警阈值。 |

## 5. 日志

- 位置：容器 stdout/stderr（由编排采集）；结构化 JSON，含 `request_id`/`trace_id`/RPC 名/`link_ref`/`click_id`/`conversion_id`/`code`/耗时。
- 合规：日志**不含**原始 IP（仅 `ip_hash`）、完整 UA、partner 密钥。
- 关联：用 `trace_id` 串联 gateway → tracking → affiliate 调用链。

## 6. 跨服务协作原则

- 仅通过契约文档协作：`.cursor/rules/shared-contracts.mdc` 与 `services/*/docs/api.md`（跨域只引用 `../../platform/docs/backend-api.md`）。
- 不直接依赖其他服务源码与内部实现细节。

## 7. K8s（服务内部署入口）

```bash
bash services/tracking-server/deploy/k8s/apply.sh
bash services/tracking-server/deploy/k8s/rollback.sh
```
