# user-server — Changelog

Documentation and logical RPC contract changes for this service. Implementation releases should reference an entry here when behavior visible to the gateway or peer services changes.

## Format

Each entry: **date (UTC)**, **version tag** (doc semver or `draft`), **summary**, **compatibility**.

---

## Unreleased — 文档商业化补全（上线 DoD）

仅文档/契约说明增量补强，不改 `proto` 线格式，无破坏性接口变更；目标是开发者仅凭 `docs/` 即可并行开发出可上线的 user-server。

- **README**：新增「SLO 与容量假设」——鉴权关键路径可用性 ≥99.95%、内省/读/写延迟目标、同意变更生效 ≤2s 及 v1 容量假设。
- **data-model**：新增 §14 具体 PostgreSQL 物理模型 DDL（账户/身份/OTP/会话/访客设备/refresh_token/偏好/收藏/历史/反馈/同意+审计历史/signal_bundle_ref/去重键/`user_outbox`，枚举 `TEXT+CHECK`、数组 `TEXT[]`、复杂结构 `JSONB`、核心列表游标索引）；§15 迁移策略（`services/foundation/migrations/NNNN_user_*.sql`、`schema_migrations` 记录、向后兼容、空库初始化）；§16 数据保留与隐私（PII 最小化/脱敏加密、各表保留周期、软删除/注销冷静期、对齐《个人信息保护法》的查询/导出/删除/撤回同意）。
- **workflow**：新增 §9 幂等/补偿/缓存失效落定（各写接口幂等键与冲突码、令牌签发与 refresh 轮换重放检测的事务/补偿、Redis 失效规则）；§10 事务性 outbox 领域事件契约（topic `user.*`、event_key、JSON payload、at-least-once + 消费方 `event_id` 幂等、payload 禁含 PII）。
- **api**：新增 §9 幂等/乐观锁/冲突落定（陈旧写入统一 `10007`）与逐接口鉴权要求/身份来源表；明确本域为会话与令牌签发/刷新/撤销/OTP 核验的真实边界。
- **development**：新增完整配置项表（`-port`/`-pg_conninfo`/`-pg_pool_size`/`-redis_addr`/`-redis_ttl_ms`/`-kafka_brokers`/`-rpc_timeout_ms`/`-rpc_max_retry`/`-log_level` 等，含默认/必填）与机密注入；可观测性（`/healthz`、Prometheus 指标、结构化日志 `request_id`/`trace_id`、trace 透传）；安全与鉴权边界；优雅退出；上线级测试门槛与合并前检查清单补强。
- **deploy**：补配置/机密注入、迁移执行、分步回滚与验证、健康检查/冒烟验收命令、故障排查表、日志位置、优雅退出说明。
- **错误码对齐**：鉴权与会话 `20xxx`、通用 `10xxx`、系统 `90xxx`，乐观锁/陈旧写入统一 `10007`，与 `.cursor/rules/shared-contracts.mdc` 一致。
- **实现期对齐项（不阻塞文档并行开发）**：现有 `migrations/0001_user_init.sql`（hex blob / 整型 `content_type`）为早期引导脚本，须按 §14 DDL 迁移到 foundation 迁移序列并同步 `src/pg_user_store.*`；详见 data-model §15「与现状差异」。
- **Compatibility**：Non-breaking（文档与运维补强；线格式与 RPC 语义不变）。

---

## [0.2.0] — 2026-05-29

- **Changed**: 重新定位为通用用户管理服务，去除业务域耦合
- **Changed**: 内容引用从 `guide_card_id` 通用化为 `content_id` + `content_type` 模式
- **Changed**: `RpcRequestContext` 新增 `app_id` 字段，支持多应用数据隔离
- **Changed**: `FavoriteItem` 新增 `content_type` 字段
- **Changed**: `AddFavoriteRequest` 使用 `content_id` + `content_type` 替代 `guide_card_id`
- **Changed**: `RemoveFavoriteRequest` 使用 `content_id` 替代 `guide_card_id`
- **Changed**: `GetSignalBundleRef.scene` 允许值改为按应用注册，不再硬编码
- **Changed**: `ResolveSignalBundlePayload.opaque_payload_json` 中 `recent_guide_card_ids` 改为 `recent_content_ids`
- **Changed**: 调用方授权矩阵通用化（不再硬编码特定服务名）
- **Changed**: HealthCheck `status` 值改为 `SERVING` / `DEGRADED` / `NOT_SERVING`
- **Added**: 多应用架构说明（README §多应用架构、api.md §8.4）
- **Added**: 多应用数据隔离规则（data-model.md §12）
- **Added**: 多应用场景流程（workflow.md §8）
- **Compatibility**: **Breaking** — `guide_card_id` → `content_id` 字段重命名；`app_id` 新增字段；HealthCheck status 值变更

---

## [0.1.0] — 2026-03-28

- **Added**: Initial service docs (`README`, `api`, `data-model`, `pages`, `workflow`, `changelog`).
- **Scope**: Defines ownership for account/session, token introspection for gateway, profile/preferences, favorites, history, feedback, consent flags, and signal bundle references.
- **Compatibility**: N/A (greenfield documentation).

---

## 已落定（v1）

以下早期保留项已在本次商业化补全中落定，不再悬空：

- **`20xxx` 错误码细分**：已在 api.md §6.2 列出本域使用的 `20001`–`20006` 与触发条件；新增仅追加。
- **`ResolveSignalBundle` 暴露面**：面向**受信下游服务**消费 `signal_bundle_ref`（api.md §8.1、§9.2），非终端访客直连。
- **历史主数据源**：v1 唯一主写源为客户端驱动的 `RecordHistoryEvent`（workflow §5）；追踪管道写入为 v1 范围外可选增强。

## Reserved（v1 范围外，不阻塞当前开发）

- 多应用账号共享策略（同一手机号在多应用共享 vs 独立 `user_id`）：v1 由服务配置决定（README「多应用架构」），跨应用统一收藏等共享语义留待后续契约显式定义。
