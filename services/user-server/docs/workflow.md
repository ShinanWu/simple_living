# user-server — 核心流程

序列级行为描述。箭头为逻辑调用；传输层为内部 RPC 除非另有说明。

## 1. 已认证请求路径（网关）

```text
Client → 网关 [Authorization: Bearer]
    → user.IntrospectAccessToken
    → { valid, user_id, session_id, … }
    → 网关注入用户上下文到下游请求
```

**失败**：`valid == false` 或错误 `20002` → 网关按错误码映射为 401；可选 `WWW-Authenticate` 行为由产品决定。

**访客**：若无 bearer，网关可按设备策略调用 `EnsureGuestSession`，然后传递 `session_id`，`user_id` 为空。

---

## 2. 登录 / 令牌签发

```text
Client → 网关 [credentials / OAuth code / SMS verify — 不在本文范围]
    → user.IssueTokenPair
    → 网关返回 tokens + 按产品策略设置 session cookie
    → 客户端按契约存储 access + refresh
```

**轮换**：若使用 refresh rotation，`IssueTokenPair` 响应包含新 refresh；客户端必须替换旧值；`RevokeSession` 在登出时使服务端会话失效。

---

## 3. 个性化门控（推荐/搜索前）

```text
网关组装推荐/搜索上下文：
    → user.GetConsent (短 TTL 缓存)
    → if personalization_allowed:
          user.GetSignalBundleRef (或约定的内联摘要)
      else:
          省略个性化信号；下游使用会话级或热门兜底
    → 下游服务查询(context with consent + ref)
```

**不变量**：下游服务默认不直接从本域读取同意状态，除非契约显式添加；**默认**是网关在请求上下文中提供标志。

---

## 4. 收藏写入 + 读取

```text
用户点击收藏：
    Client → 网关 → user.AddFavorite(user_id, content_id, content_type)
    → success

收藏列表：
    Client → 网关 → user.ListFavorites
    → 网关或客户端 → 内容服务.BatchGetContent(ids)
```

排序：`favorited_at` desc 除非产品另有指定。

---

## 5. 历史记录

**方案 A（客户端驱动）**：

```text
Client → 网关 → user.RecordHistoryEvent(session_id|user_id, content_ref, occurred_at)
```

**方案 B（追踪驱动，v1 范围外的可选补充）**：

```text
追踪管道 → 异步消费者 → user-server 聚合 upsert HistoryItem
```

**v1 落定**：用户可见「最近浏览」的**唯一主写源是客户端驱动的 `RecordHistoryEvent`**（经网关），按 `(owner, content_id)` 聚合、按 `client_event_id` 去重，避免重复计数。追踪管道写入为 v1 范围外的可选增强；若后续启用，须以本域聚合 upsert 为汇聚点并复用同一去重键，不另立第二套主数据源。

---

## 6. 同意变更传播

```text
Client → 网关 → user.UpdateConsent
    → 失效 GetConsent 缓存
    → 提升 signal bundle 版本或使 GetSignalBundleRef 过期
    → 后续推荐/搜索看到新标志
```

---

## 7. 账号暂停或删除

```text
管理后台或合规任务 → user.account_status 更新
    → IntrospectAccessToken 返回 invalid 或受限 tier
    → 网关阻断受保护路由
```

删除后的历史/反馈数据保留遵循合规策略；实现调度清除任务。

---

## 8. 多应用场景

```text
新应用接入：
    1. 在服务配置中注册 app_id 及认证方式
    2. 网关配置该应用路由与鉴权规则
    3. 应用通过网关调用 API，app_id 由网关注入
    4. 用户数据按 app_id 隔离存储
```

**跨应用场景**：
- 同一用户在多个应用中共享账号（相同 `user_id`）时，资料/偏好/收藏等数据仍按 `app_id` 隔离
- 若需跨应用数据共享（如统一收藏），需在契约中显式定义

---

## 9. 幂等、补偿与一致性落定（v1）

### 9.1 写接口幂等与冲突（确定值）

| 接口 | 幂等/冲突机制 | 结果 |
|------|---------------|------|
| `IssueTokenPair`（手机 OTP） | 核验 `user_otp_challenge`：`verification_id` 未过期、`attempts < max_attempts`、`consumed_at` 为空；成功后置 `consumed_at` 防复用 | 重复使用已消费 OTP → `20006`；超尝试 → `20005` |
| `EnsureGuestSession` | `(app_id, device_id)` 唯一映射到当前活跃访客会话；命中则复用 | 复用返回 `created=false`，新建返回 `created=true` |
| `AddFavorite` | `user_favorite` 唯一约束 `(app_id, user_id, content_id)` | 已存在返回 `already_favorited=true`，不报错 |
| `RecordHistoryEvent` | `(scope_kind='HISTORY_EVENT', owner_key, client_event_id)` 24h 去重；否则按 `(app_id, owner_key, content_id)` 聚合 upsert | 命中去重 → `recorded=false, deduplicated=true`；聚合 `impression_count+=1`、`first_seen_at=min`、`last_seen_at=max` |
| `SubmitFeedback` | `(scope_kind='FEEDBACK', actor, client_request_id)` 24h 去重，存首次 `feedback_id` 于 `result_ref` | 命中去重返回首次 `feedback_id` |
| `UpdatePreferences` | `user_preferences.preferences_version` 乐观锁：请求显式携带且 `< 当前版本` → 拒绝；否则整文档替换并 `version+1` | 陈旧写入冲突统一返回 **`10007`**（`ABORTED`） |
| `UpdateConsent` | 当前态 upsert + `user_consent_history` 追加；末写生效（last-write-wins） | 无版本冲突码；每次变更必追加审计行 |
| `RevokeSession` | 目标已撤销/不存在按幂等处理 | 返回 `revoked=false`，不报错 |

> **乐观锁统一约定**：本域所有“版本化整文档写”冲突一律用共享冲突码 **`10007`**（对应 gRPC `ABORTED`），与 `.cursor/rules/shared-contracts.mdc` 一致；详见 api.md §6.2 与 §9。

### 9.2 多表写的事务与补偿

- **令牌签发**：`user_session`、`user_refresh_token`（如新建账户还含 `user_account`/`user_identity`）与 `user_outbox` 在**同一 PostgreSQL 事务**提交；任一失败整体回滚，调用方可安全重试（外部 OTP 已消费的情况由 `verification_id` 幂等兜底）。
- **refresh 轮换**：换发新 refresh 与作废旧 refresh（置 `revoked_at`、记 `rotated_from_jti`）在同一事务内完成；若检测到**已轮换 token 被重放**（命中 `revoked_at` 且非空 `rotated_from_jti`），置 `reused_at`、撤销整条会话链并发 `user.session_revoked` 安全事件。
- **跨服务不使用 2PC**：账户删除、同意变更等需要下游配合的动作，本域只保证本地事务 + `user_outbox` 事件；下游按 `event_id` 幂等消费实现最终一致（见 §10）。失败由 relay 重投，不阻塞主链路。

### 9.3 缓存与失效（Redis 仅缓存）

- **缓存对象**：会话/access token 内省结果、`GetConsent` 结果、`GetSignalBundleRef` 引用为热点缓存候选；缓存值带版本戳（会话 `revoked_at`、`preferences_version`、`consent.updated_at`、`bundle_version`）。
- **失效规则**：
  - `RevokeSession` / 账户暂停 / refresh 重放撤销：写库提交后删除该 `session_id`/`user_id` 的内省缓存；后续内省回源得到 `valid=false`。
  - `UpdateConsent`：写库提交后失效该 `(app_id,user_id)` 的同意缓存，并提升关联 `signal_bundle_ref` 的 `bundle_version` 或使其 `expires_at` 立即过期；经 `user.consent_updated` 通知网关/下游清各自缓存（目标 ≤ 2s，见 README SLO）。
  - `UpdatePreferences`：失效偏好相关缓存并（如启用）提升 signal bundle 版本。
- **权威性**：任何缓存缺失都可由 PostgreSQL 重建；缓存不可作为撤销/同意的最终真相，回源结果优先。

---

## 10. 领域事件契约（Kafka outbox）

用户态变更通过**事务性 outbox** 发布：业务写与 `user_outbox` 插入在同一 PostgreSQL 事务内提交；独立 relay 进程（`services/foundation/kafka/scripts/outbox_relay.sh` 或服务内线程）轮询未投递行投递 Kafka 后回写 `published_at`。**投递语义 at-least-once，消费方必须按 `event_id` 幂等**。空 `-kafka_brokers` 时只落 `user_outbox` 表不投递（见 development.md）。

### 12.1 Topic 与 key

| topic | 触发 | event_key | 主要消费方 |
|-------|------|-----------|------------|
| `user.consent_updated` | `UpdateConsent` 成功 | `user_id` | recommendation（个性化门控刷新）、gateway/缓存失效、governance（合规审计） |
| `user.preferences_updated` | `UpdatePreferences` 成功 | `user_id` | recommendation（刷新偏好/信号）、缓存失效 |
| `user.session_revoked` | `RevokeSession` / refresh 重放撤销 / 账户暂停 | `user_id` | gateway（令牌缓存/denylist 失效）、recommendation（会话级状态清理） |
| `user.signal_bundle_invalidated` | 同意/偏好变更导致 ref 失效或 `bundle_version` 提升 | `user_key` | recommendation（信号缓存失效与重取） |
| `user.account_deleted` | 注销冷静期到期硬清除 | `user_id` | recommendation、tracking、governance（擦除/去标识化派生数据，履行删除权） |

topic 命名、分区与保留策略遵循 `services/foundation/kafka/docs/README.md`；同一 `event_key` 保证分区内有序。

### 12.2 Payload（JSON）

```json
{
  "event_id": "ULID",
  "event_type": "user.consent_updated",
  "occurred_at": "2026-03-28T10:00:00Z",
  "app_id": "simple_living",
  "user_id": "user_01HSYQK5PZ4K4J9R2M8D",
  "consent": {
    "personalization_allowed": true,
    "analytics_allowed": true,
    "marketing_allowed": false,
    "consent_version": "2026-03-privacy-v3"
  }
}
```

- 必含：`event_id`、`event_type`、`occurred_at`、`app_id`、`user_id`（或 `user_key`）。
- 各 topic 附带最小语义字段：`consent_updated` 带 `consent` 快照（**不含** PII）；`session_revoked` 带 `session_id`/`scope`/`reason`；`preferences_updated` 带 `preferences_version`；`signal_bundle_invalidated` 带 `user_key`/`scene`/`bundle_version`；`account_deleted` 带 `deleted_at`。
- **隐私**：payload **禁止**包含手机号、token、OTP、`free_text` 等明文 PII；同意快照仅含布尔标志与版本。
- 兼容性：只新增可选字段，消费方忽略未知字段；破坏性变更须新 topic 版本（如 `user.consent_updated.v2`）并在 `changelog.md` 标注受影响消费方。

---

## 11. 可观测性

- 使用 `session_id`、`user_id`（在非受信汇中做哈希处理）和网关 `request_id` 关联日志。
- 指标：令牌内省 QPS、同意读取比例、收藏/历史写入速率、`GetSignalBundleRef` 命中率。
- 完整可观测性（health、Prometheus 指标项、结构化日志字段、trace 透传）见 [development.md](./development.md) §可观测性。

---

## 12. 非目标

- 跨服务 **2PC** 事务（如收藏 + 内容存在性）：使用最终一致性或带定义失败 UX 的同步校验。
- 在 user-server 内运行推荐或内容管道。
