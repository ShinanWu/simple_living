# tracking-server — 开发与交付

本文说明本服务在仓库内的**实现、构建、运行、测试与合并前检查**约定，以及须对照的公共文档。

## 1. 本服务文档

| 文档 | 用途 |
|------|------|
| [README.md](./README.md) | 边界与依赖 |
| [api.md](./api.md) | 内部 RPC / proto |
| [data-model.md](./data-model.md) | 实体与枚举 |
| [workflow.md](./workflow.md) | 流程 |
| [pages.md](./pages.md) | 页面消费 |
| [changelog.md](./changelog.md) | 契约变更 |

## 2. 公共文档

| 文档 | 用途 |
|------|------|
| [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) | `landing_url`、`click_id` 等共享语义 |
| [业务服务总览](../../README.md) | Bazel、端口、跨服务依赖、测试 |

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/tracking-server` |
| 二进制 | `//services/tracking-server:tracking_server` |

```bash
bazel build //services/tracking-server/...
bazel test //services/tracking-server/...
```

允许依赖：`//common/proto:catalog_cc_proto`（解析 snapshot JSON）。**禁止**依赖 `platform/backoffice-backend` BUILD 目标或拷贝其 `.proto`。

## 4. 运行与配置

默认 brpc 端口 `9105`（监听默认 `0.0.0.0`）。转链规格依赖 `backoffice-backend` 导出物（`-snapshot_dir`，同节点）。

```bash
bazel run //services/tracking-server:tracking_server -- \
  -port=9105 \
  -pg_conninfo="host=127.0.0.1 port=15432 dbname=tracking user=tracking password=$PG_PWD" \
  -redis_addr=127.0.0.1:6379 \
  -kafka_brokers=127.0.0.1:19092 \
  -snapshot_dir=/var/lib/simple-living/exports
```

完整配置项（flags / 等价 env）：

| flag | 必填 | 默认 | 说明 |
|------|------|------|------|
| `-port` | 否 | `9105` | brpc 监听端口 |
| `-pg_conninfo` | **是** | — | PostgreSQL 连接串（权威存储：link/click/conversion/outbox） |
| `-pg_pool_size` | 否 | `16` | PostgreSQL 连接池大小 |
| `-redis_addr` | 否 | 空 | 短 token 热读 + 点击幂等窗口；空则只用 PostgreSQL（功能正确，延迟略增） |
| `-redis_ttl_ms` | 否 | `600000` | Redis 热读 / 幂等窗口 TTL（短 token、`idempotency_key`、`device_dedup_key`） |
| `-kafka_brokers` | 否 | 空 | outbox relay 投递目标；空则只落 `tracking_outbox` 表不投递 |
| `-snapshot_dir` | **是** | `/var/lib/simple-living/exports` | 与 `backoffice-backend` 共享；读取 `affiliate_link_spec` 导出物 |
| `-rpc_timeout_ms` | 否 | `800` | 预留；assemble 路径以本地 snapshot 为主 |
| `-rpc_max_retry` | 否 | `1` | 下游重试次数（仅幂等只读校验） |
| `-log_level` | 否 | `info` | 日志级别 |

密钥 / 连接串 / token 签名密钥经环境变量或部署密钥注入，**不写入仓库**（见 deploy）。

## 5. 持久化与事件

- **权威存储**：PostgreSQL。`tracking_link`、`tracking_short_token`、`tracking_click`、`tracking_conversion` 保存转链、短 token、点击与转化事实；`tracking_commission_daily` 为佣金日聚合读模型。表结构、索引与迁移见 [`data-model.md`](./data-model.md) §5–§6。
- **事件出口**：业务写与 `tracking_outbox` 插入在同一事务提交，outbox relay 投递 Kafka 三主题 `tracking.link.created` / `tracking.click.acked` / `tracking.conversion.ingested`（at-least-once，消费方按 `event_id` 幂等，见 [`workflow.md`](./workflow.md) §8）。
- **Redis**：限流、短 token 热读与点击/转化幂等窗口；任何缓存不得成为权威。Redis 不可用时回落 PostgreSQL 唯一约束兜底。
- **禁止降级**：link/click/conversion 权威态、联调与验收路径都必须使用 PostgreSQL。

## 5.1 可观测性

- **健康检查**：HTTP `GET /healthz`（就绪含 PostgreSQL 连通性探测；Redis/Kafka 不可用降级为告警而非不就绪）。编排存活/就绪探针均用之。
- **指标**（Prometheus，端点 `/metrics` 或 brpc 内置）：每 RPC QPS / 错误率 / P50/P99 延迟；`ResolveRedirect` 点击写入吞吐与各 `redirect_state` 计数；affiliate 下游调用失败率与超时数；`IngestConversion` 的 `accepted`/`duplicate`/`unmatched` 计数与转化滞后；`tracking_outbox` 未投递积压条数；Redis 命中率；PostgreSQL 连接池使用率与慢查询数。
- **日志**：结构化（JSON），每条含 `request_id`、`trace_id`、RPC 名、`link_ref`/`click_id`/`conversion_id`、`channel_code`、`code`、耗时；错误日志含下游域与失败原因。**禁止记录** 原始 IP（仅 `ip_hash`）、完整 UA、partner 密钥。
- **追踪**：透传上游 `trace_id`（gateway 注入），对 PostgreSQL、Redis 与 affiliate RPC 建 span。

## 5.2 安全与鉴权边界

- **信任边界**：本域只接受集群内 `gateway` 与受信内部链路（affiliate→tracking relay、ops postback router）的 brpc 调用，不直接对公网暴露；身份由 `gateway` 解析后透传，本域不自行校验登录态（见 api §1.6）。
- **写/敏感接口**：`IngestConversion` 仅受信内部调用；`CONVERSION_SOURCE_MANUAL_ADJUSTMENT` 需 ops 角色（在 gateway/governance 校验）。佣金读视图按调用方注入的主体（ops/creator `user_id`）过滤，不放大范围。
- **密钥来源**：PostgreSQL 连接串、Redis 地址、token 签名密钥经部署密钥/环境注入，不写明文、不入请求体；客户端可见 URL 永不含 partner 密钥。

## 6. 测试（上线门槛）

最低期望：
- **健康**：`/healthz` 在 PostgreSQL 连通/断开下分别返回 200/非 200。
- **prepare 链路**：`AssembleTrackingLink` 在联调库下成功返回 `landing_url` + `short_token`，并落 `tracking_link`/`tracking_short_token` 与 `tracking.link.created` outbox 行；affiliate 失败时返回 `50002`，placement 冲突返回 `10002`。
- **redirect 先写点击**：`ResolveRedirect` 成功路径**先落 `tracking_click` 再返回** partner URL；过期 token 返回 `GONE`/`50003`；重复解析按 `(short_token, device_dedup_key)` 收敛为同一 `click_id`。
- **转化幂等**：`IngestConversion` 同 `(source, external_event_id)` 重放返回 `DUPLICATE` 且不重复计数；无 `click_id` 落 `UNMATCHED`。
- **读视图**：`GetCommissionSummary` / `ListCommissionItems` 从日聚合读模型返回，游标分页对齐共享契约。
- 断言字段名 / 错误码对齐 api.md 与 `.cursor/rules/shared-contracts.mdc`；目录与 Bazel 约定见 `services/README.md` §5。

## 7. 合并前检查清单

- [ ] `api.md` 与 `proto/` 一致；`changelog.md` 已更新
- [ ] `data-model.md` 与 PostgreSQL 表、索引、迁移、Redis 规划、Kafka outbox 一致
- [ ] 错误码使用本域 `50xxx` + 通用 `10xxx` + 系统 `90xxx`；幂等冲突/乐观锁为 `10007`（api §9）
- [ ] `bazel build` / `bazel test` 本包通过
- [ ] 通过 Bazel 依赖 `platform/backoffice-backend` proto（不复制 `.proto`）；对外可见字段与 `.cursor/rules/shared-contracts.mdc` 及 `gateway` 一致
- [ ] 优雅退出：SIGTERM/SIGINT → 停服务 → 关闭 PostgreSQL/Redis/Kafka 连接（见 `services/README.md` §4.3 与 graceful-shutdown 约定）
