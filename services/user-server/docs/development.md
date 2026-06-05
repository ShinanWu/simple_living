# user-server — 开发与交付

本文说明本服务在仓库内的**实现、构建、运行、测试与合并前检查**约定。

## 1. 本服务文档

| 文档 | 用途 |
|------|------|
| [README.md](./README.md) | 定位、设计原则与边界 |
| [api.md](./api.md) | 内部 RPC / proto 契约 |
| [data-model.md](./data-model.md) | 实体与枚举 |
| [workflow.md](./workflow.md) | 核心流程 |
| [pages.md](./pages.md) | 典型集成模式 |
| [changelog.md](./changelog.md) | 契约变更 |

## 2. 公共文档

| 文档 | 用途 |
|------|------|
| [共享契约规则](../../../.cursor/rules/shared-contracts.mdc) | 鉴权与令牌语义（网关映射依据） |
| [业务服务总览](../../README.md) | Bazel、端口表、禁止 fork proto、测试与 CI |

## 3. Bazel

| 项 | 值 |
|----|-----|
| 包 | `//services/user-server` |
| 二进制 | `//services/user-server:user_server` |
| Proto | `proto/*.proto` |

```bash
bazel build //services/user-server/...
bazel test //services/user-server/...
```

## 4. 运行与配置

```bash
bazel run //services/user-server:user_server -- \
  -port=9101 \
  -pg_conninfo="host=127.0.0.1 port=15432 dbname=user user=user password=$PG_PWD"
```

完整配置项（flags / 等价 env；监听默认 `0.0.0.0`，端口 `9101` 与 `services/README.md` §4.2 对齐）：

| flag | 必填 | 默认 | 说明 |
|------|------|------|------|
| `-port` | 否 | `9101` | brpc 监听端口 |
| `-pg_conninfo` | **是** | — | PostgreSQL 连接串（权威存储） |
| `-pg_pool_size` | 否 | `16` | PostgreSQL 连接池大小 |
| `-redis_addr` | 否 | 空 | 会话/同意/内省热点缓存与限流；空则不启用缓存（不影响正确性，回源 PostgreSQL） |
| `-redis_ttl_ms` | 否 | `60000` | 缓存默认 TTL（内省/同意等热点读） |
| `-kafka_brokers` | 否 | 空 | `user_outbox` relay 投递目标；空则只落 outbox 表不投递（见 workflow §10） |
| `-rpc_timeout_ms` | 否 | `800` | 出站下游调用超时（v1 本域无强制业务域下游，预留给可选风控/SMS adapter） |
| `-rpc_max_retry` | 否 | `1` | 出站只读幂等调用重试次数 |
| `-log_level` | 否 | `info` | 日志级别（`debug`/`info`/`warn`/`error`） |
| `-otp_ttl_ms` | 否 | `300000` | OTP 校验挑战有效期（手机登录） |
| `-access_ttl_ms` | 否 | `7200000` | access token 有效期（短效；对外以 `expires_in` 暴露，不暴露内部命名） |
| `-refresh_ttl_ms` | 否 | `2592000000` | refresh token 有效期（长效可撤销，30 天） |

**机密配置（不经明文 flag，从环境变量 / 部署密钥注入，禁止写入仓库）**：

| 变量 | 用途 |
|------|------|
| `USER_TOKEN_HMAC_KEY` | access/refresh token 签名或指纹 HMAC 密钥 |
| `USER_PII_ENC_KEY`（或 KMS key id） | 手机号等 PII 字段加密（`phone_enc`），与 pgcrypto/KMS 配合 |
| `USER_PHONE_HASH_SALT` | 手机号 HMAC 查找键盐值（`phone_hash`） |
| `PG_PWD` | 注入到 `-pg_conninfo` 的数据库口令 |

密钥轮换、TTL 精确值与 token 编码形态为 provider-owned 实现细节（见 api.md §1）；调用方只依赖对外暴露的 `expires_in` / `access_expires_at` / `rotation_required` 语义。

迁移执行：服务启动或独立迁移任务对 `-pg_conninfo` 目标执行 `services/foundation/migrations/NNNN_user_*.sql`（幂等，记录于 `schema_migrations`，见 data-model §15）。

## 4.1 可观测性

- **健康检查**：HTTP `GET /healthz`（就绪含 PostgreSQL 连通性探测；Redis/Kafka 不可用时按降级而非不就绪处理）；编排存活/就绪探针均用之。另有内部 `HealthCheck` RPC（api §5.15）返回 `SERVING`/`DEGRADED`/`NOT_SERVING` 与组件明细。
- **指标**（Prometheus，端点 `/metrics` 或 brpc 内置）：每 RPC QPS / 错误率 / P50/P99 延迟；`IntrospectAccessToken`/`IntrospectRefreshToken` 命中率与负例率；`IssueTokenPair`/`RevokeSession` 速率；refresh **重放检测计数**（安全告警）；OTP 核验成功/失败率；PostgreSQL 连接池使用率与慢查询计数；`user_outbox` 未投递积压条数；Redis 缓存命中率。
- **日志**：结构化（JSON），每条含 `request_id`、`trace_id`、RPC 名、`app_id`、主体类别（user/guest，**对 `user_id` 在非受信汇做哈希**）、`code`、耗时；错误日志含失败原因与下游。**禁止**落盘完整 token / 手机号 / OTP / `free_text` 明文（见 api §7、data-model §16.1）。
- **追踪**：透传上游 `trace_id`（网关注入），对 PostgreSQL、Redis 与可选出站 RPC 建 span。

## 4.2 安全与鉴权边界

- **信任边界**：本域只接受集群内 `gateway` 与授权下游服务的 brpc 调用（mTLS / 服务网格身份或等价机制），不直接对公网暴露。终端用户/访客身份由 `gateway` 解析后透传，本域不自行校验对外登录态。
- **会话/令牌权威**：本域是会话与令牌**签发、刷新、撤销、重放检测、OTP 核验**的真实边界；网关只做 token 透传与对外 JSON 映射。`IssueTokenPair.user_id` 直签、代查/代写任意 `user_id`、账户删除/导出等越权动作仅对白名单服务身份开放，其余 `PERMISSION_DENIED` / `20004`（见 api §8.1、§9.2）。
- **PII 与机密**：手机号/OAuth subject/token/OTP 按 data-model §16 脱敏加密存储；加解密密钥与 HMAC 盐由密钥管理注入，不写仓库、不进日志/事件 payload。
- **个性化门控**：`personalization_allowed=false` 时 `GetSignalBundleRef`/`ResolveSignalBundle` 降级或拒绝（api §7）。

## 5. 持久化与缓存

- **权威存储**：PostgreSQL。启动时通过 `-pg_conninfo` 连接 foundation 节点 PostgreSQL 并执行未应用迁移；表结构、索引与保留策略见 [`data-model.md`](./data-model.md) §14–§16。
- **缓存**：Redis 仅作会话/access token 内省、同意、`signal_bundle_ref` 等热点缓存与限流；缓存值带版本戳，任何缓存缺失可由 PostgreSQL 重建，**不得作为撤销/同意的最终真相**（失效规则见 workflow §9.3）。
- **消息**：Kafka 仅承载异步领域事件（`user_outbox` → `user.*`，见 workflow §10）；不用于同步读写权威路径。
- **禁止降级**：会话/令牌/同意权威状态、联调与验收路径必须使用 PostgreSQL。细则见 `services/README.md` §4.3。

## 5.1 优雅退出

- 进程遵循优雅退出：收到 `SIGTERM`/`SIGINT` 后停止接收新请求，等待在途 RPC 完成，再依次关闭 outbox relay、Redis、PostgreSQL 连接（先停对外、再关依赖）。
- 正常停机以退出码 `0` 结束；配置错误/启动失败用非零退出码。详见仓库优雅退出规则与 deploy/README。

## 6. 跨服务依赖

本域默认无业务域 proto 依赖。消费方（网关、下游服务等）在 Bazel 中依赖本域导出目标 `//services/user-server:user_server_proto` 等；**禁止** 其他服务复制本域 `.proto`。

## 7. 多应用配置

本服务支持多应用接入，配置方式：

- **应用注册**：在服务配置中注册 `app_id` 及其认证方式、同意策略、允许的场景值等
- **数据隔离**：收藏、历史、偏好等用户数据按 `app_id` 隔离
- **默认应用**：v1 兼容未携带 `app_id` 的请求，使用默认应用配置；新接入应用必须携带 `app_id`

## 8. 测试（上线门槛）

最低期望（对接真实库或联调库，断言字段名/错误码对齐 api.md 与 `.cursor/rules/shared-contracts.mdc`）：

- **健康**：`/healthz` 在 PostgreSQL 连通/断开下分别返回 200 / 非 200。
- **登录闭环**：`IssueTokenPair`（手机 OTP happy path）签发令牌 → `IntrospectAccessToken` 返回 `valid=true` → `RevokeSession` 后内省 `valid=false`。
- **refresh 轮换与重放**：`IntrospectRefreshToken` 在轮换窗口返回 `rotation_required=true`；**已轮换 token 重放被检测**并撤销整条会话链。
- **访客**：`EnsureGuestSession` 同设备重复调用返回 `created=false`（复用）。
- **收藏幂等**：`AddFavorite` 重复调用返回 `already_favorited=true`；`ListFavorites` 游标翻页与空列表（`has_more=false`）正确。
- **历史聚合/去重**：`RecordHistoryEvent` 聚合 `impression_count`、命中 `client_event_id` 去重返回 `deduplicated=true`。
- **偏好乐观锁**：`UpdatePreferences` 携带陈旧 `preferences_version` 返回 **`10007`**（`ABORTED`）。
- **同意与事件**：`UpdateConsent` 追加 `user_consent_history` 行并落 `user.consent_updated` 到 `user_outbox`。
- **隐私**：日志/事件 payload 不含 token / 手机号 / OTP 明文（可加断言或采样校验）。

目录与 Bazel 约定见 `services/README.md` §5。

## 9. 合并前检查清单

- [ ] `api.md` 与 `proto/` 一致；契约/行为变更已记 `changelog.md`
- [ ] `data-model.md` §14 DDL 与实现 schema、`services/foundation/migrations/NNNN_user_*.sql` 一致（含索引与 `CHECK` 枚举）
- [ ] 错误码对齐：鉴权/会话用 `20xxx`、通用 `10xxx`、系统 `90xxx`；乐观锁冲突统一 `10007`
- [ ] 领域事件（`user_outbox` → `user.*`）payload 不含 PII，消费方按 `event_id` 幂等
- [ ] 机密（token HMAC、PII 加密键、`PG_PWD`）经密钥注入，未入库；日志无明文 PII
- [ ] `bazel build` / `bazel test` 本包通过
- [ ] 默认端口 `9101` 与 `services/README.md` §4.2 一致（若调整则同步该文与网关文档）
- [ ] 新增 `app_id` 相关变更已更新配置与文档
