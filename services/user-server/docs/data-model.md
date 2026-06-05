# user-server — 数据模型（v1）

本服务的数据模型设计为通用用户管理，不绑定特定业务场景。各应用通过 `app_id` 隔离数据。物理表结构与索引为实现内部细节；**跨服务**语义必须与契约文档保持一致。

## 1. User

| 字段 | 类型 | 说明 |
|-------|------|------|
| `user_id` | string | 稳定内部 ID，永不回收 |
| `account_status` | enum | `active`, `suspended`, `deleted_pending` |
| `created_at` | datetime | |
| `updated_at` | datetime | |
| `primary_locale` | string | BCP 47 或产品约定 |
| `deleted_at` | datetime \| null | 合规软删除 |

**规则**：PII 信息（手机号哈希、OAuth subject 等）保留在本服务内部；其他服务仅接收 `user_id` 和非敏感资料字段。

---

## 2. Session

绑定**令牌**、**设备**和可选**访客**身份。

| 字段 | 说明 |
|-------|------|
| `session_id` | 服务端签发 ID，用于分析关联 |
| `user_id` | 认证后设置 |
| `is_guest` | boolean |
| `device_id` | 来自客户端，非唯一安全因子 |
| `client_platform` | `web`, `ios`, `android`, 小程序等 |
| `app_id` | 所属应用标识 |
| `created_at` / `last_active_at` | |
| `revoked_at` | null 表示活跃 |

---

## 3. Token 元数据（逻辑）

实现可仅存储哈希后的 refresh token。

| 字段 | 说明 |
|-------|------|
| `access_jti` | 唯一 ID，用于撤销列表 |
| `refresh_fingerprint` | refresh token 的哈希 |
| `expires_at` | |
| `scopes` | string[] |

---

## 4. Profile

产品面向的资料子集，非完整 CRM。

| 字段 | 类型 | 说明 |
|-------|------|------|
| `display_name` | string | 可能受审核策略约束 |
| `avatar_url` | string | CDN 引用 |
| `bio` | string | 可选 |
| `notification_prefs` | object | 通知渠道开关 |

---

## 5. Preferences

| 字段 | 说明 |
|-------|------|
| `preferences_version` | 单调递增，用于缓存失效与乐观并发控制 |
| `theme_interests` | string[] | 兴趣标签键，各应用定义自己的标签体系 |
| `content_filters` | object | 如隐藏分类 |
| `default_sort` | string | UX 默认排序；下游服务可忽略 |

---

## 6. Favorite

| 字段 | 类型 | 说明 |
|-------|------|------|
| `favorite_id` | string | 内部行 ID |
| `user_id` | string | |
| `content_id` | string | 通用内容标识 |
| `content_type` | enum | 内容类型鉴别 |
| `app_id` | string | 所属应用 |
| `favorited_at` | datetime | |

**扩展**：新增内容类型需在 `ContentRefType` / `FavoriteContentType` 枚举中注册，无需修改消息结构。

---

## 7. HistoryItem

用户可见的"最近浏览"行，区别于原始点击流。

| 字段 | 说明 |
|-------|------|
| `owner_key` | `user_id` 或 `session_id` |
| `content_id` | 通用内容标识 |
| `content_type` | 内容类型鉴别 |
| `app_id` | 所属应用 |
| `last_seen_at` | |
| `first_seen_at` | 可选 |
| `impression_count` | 聚合计数 |
| `source_surface` | 可选枚举：`feed`, `search`, `detail` |

数据保留策略由产品/合规驱动（TTL 任务）。

---

## 8. Feedback

| 字段 | 说明 |
|-------|------|
| `feedback_id` | |
| `actor_user_id` / `actor_session_id` | |
| `target_type` | enum |
| `target_id` | string |
| `rating` | 可选数值 |
| `reason_codes` | string[] |
| `free_text` | 可选；PII 清洗在管道中处理 |
| `app_id` | 所属应用 |
| `created_at` | |

---

## 9. Consent

| 字段 | 说明 |
|-------|------|
| `personalization_allowed` | boolean |
| `analytics_allowed` | boolean |
| `marketing_allowed` | boolean |
| `consent_version` | string — 合规文档版本 |
| `updated_at` | 服务端生成 |
| `jurisdiction` | 可选；区域性默认值 |
| `app_id` | 所属应用 |

**消费规则**：下游服务必须将 `personalization_allowed == false` 视为**禁止使用个性化信号**（仅限会话级或热门路径）。

---

## 10. SignalBundleRef

本服务产出（或同步特征任务产出）的不透明引用，供下游服务消费。

| 字段 | 说明 |
|-------|------|
| `signal_bundle_ref` | 不透明字符串，不可猜测 |
| `bundle_version` | 用于失效通知 |
| `expires_at` | |
| `scopes` | 允许消费此引用的场景或服务 |
| `app_id` | 所属应用 |

**内容**：可指向 Redis/数据库/特征存储行，持有粗粒度兴趣、近期交互摘要、可选嵌入 ID——**不含**推荐分数。

---

## 11. 枚举（示意）

| 枚举 | 值 | 扩展规则 |
|------|-----|----------|
| `account_status` | `active`, `suspended`, `deleted_pending` | 仅追加 |
| `feedback_target_type` | `guide_card`, `recommendation_result`, `app`, `other` | 各应用可追加 |
| `content_ref_type` | `guide_card` | 各应用可追加 |
| `favorite_content_type` | `guide_card` | 各应用可追加 |
| `client_platform` | `web`, `ios`, `android`, `wechat_miniprogram`, `douyin_miniprogram` | 各应用按需使用 |

---

## 12. 多应用数据隔离

| 维度 | 隔离方式 |
|------|----------|
| 收藏 | 按 `(app_id, user_id, content_id)` 隔离 |
| 历史 | 按 `(app_id, owner_key, content_id)` 隔离 |
| 偏好 | 按 `(app_id, user_id)` 隔离 |
| 反馈 | 按 `app_id` 标记归属 |
| 同意 | 按 `(app_id, user_id)` 隔离 |
| 信号 | 按 `(app_id, user_key)` 隔离 |
| 账号/会话 | 可跨应用共享或独立，由配置决定 |

---

## 13. 与其他服务的关系

| 本服务 | 其他服务 | 关系 |
|--------|----------|------|
| `content_id` | 内容服务 | 收藏/历史引用的内容须存在；可懒校验 |
| `signal_bundle_ref` | 推荐服务 / 搜索服务等 | 下游消费；不回写 |
| 事件 | 追踪服务 | 可选输入到历史聚合 |

---

## 14. PostgreSQL 物理模型（权威存储）

> v1 权威存储为 PostgreSQL（foundation 节点，`-pg_conninfo` 连接）。所有 `*_id` 为 `TEXT`（应用层生成 ULID/UUID 字符串）。时间统一 `TIMESTAMPTZ`（UTC）。枚举以 `TEXT` + `CHECK` 落库（不使用 PG enum，便于仅追加）。数组用 `TEXT[]`，复杂/可演进结构用 `JSONB`。`app_id` 为多应用隔离维度，缺省值 `''` 表示默认应用（向后兼容，见 api.md §8.4）。
>
> **PII 与密钥**：手机号、OAuth subject、token、OTP 等敏感数据**禁止明文落库**。手机号以 HMAC 摘要做唯一查找键（`*_hash`），明文如需保留则经 KMS/pgcrypto 加密为 `BYTEA`（`*_enc`）；token / OTP 仅存哈希或指纹。加解密密钥、HMAC 盐由部署密钥注入，不写入仓库（见 development.md §安全）。
>
> Redis 仅作会话/同意/`signal_bundle_ref` 等热点缓存与限流，**不得作为跨实例权威**；任何缓存值都可由下表数据重建。Kafka 仅承载异步领域事件（见 workflow.md §10）。

### 14.1 账户、身份与会话

```sql
-- 账户主体（最小化 PII；展示资料随主体存放）
CREATE TABLE user_account (
  user_id            TEXT PRIMARY KEY,
  account_status     TEXT NOT NULL DEFAULT 'ACTIVE'
                     CHECK (account_status IN ('ACTIVE','SUSPENDED','DELETED_PENDING')),
  primary_locale     TEXT,
  display_name       TEXT,
  avatar_url         TEXT,                                   -- 对象存储引用，非二进制
  bio                TEXT,
  notification_prefs JSONB NOT NULL DEFAULT '{}'::jsonb,     -- NotificationPrefs（整对象替换）
  created_at         TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at         TIMESTAMPTZ NOT NULL DEFAULT now(),
  deleted_at         TIMESTAMPTZ                             -- 合规软删除：置位进入注销冷静期
);
CREATE INDEX idx_account_pending_delete ON user_account(deleted_at)
  WHERE account_status = 'DELETED_PENDING';

-- 登录身份绑定（PII：手机号 / OAuth）。脱敏与加密存储，明文禁止落库。
CREATE TABLE user_identity (
  identity_id     TEXT PRIMARY KEY,
  user_id         TEXT NOT NULL REFERENCES user_account(user_id) ON DELETE CASCADE,
  identity_type   TEXT NOT NULL CHECK (identity_type IN ('PHONE','OAUTH')),
  phone_hash      TEXT,                 -- HMAC-SHA256(phone_e164)：唯一查找键
  phone_enc       BYTEA,                -- 可选，KMS/pgcrypto 加密的手机号明文
  oauth_provider  TEXT,                 -- 小写码表：wechat/apple/douyin/google…
  oauth_subject   TEXT,                 -- 渠道侧不可逆主体标识
  created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
  CHECK ( (identity_type='PHONE' AND phone_hash IS NOT NULL)
       OR (identity_type='OAUTH' AND oauth_provider IS NOT NULL AND oauth_subject IS NOT NULL) )
);
CREATE UNIQUE INDEX uq_identity_phone ON user_identity(phone_hash) WHERE identity_type='PHONE';
CREATE UNIQUE INDEX uq_identity_oauth ON user_identity(oauth_provider, oauth_subject) WHERE identity_type='OAUTH';
CREATE INDEX idx_identity_user ON user_identity(user_id);

-- 手机号 OTP 校验挑战（短存活；OTP 仅存哈希，明文不落库；SMS 下发为 provider 实现）
CREATE TABLE user_otp_challenge (
  verification_id TEXT PRIMARY KEY,     -- 即 AccountProofPhoneOtp.verification_id
  phone_hash      TEXT NOT NULL,
  otp_hash        TEXT NOT NULL,        -- HMAC/sha256(otp_code)
  app_id          TEXT NOT NULL DEFAULT '',
  attempts        INT  NOT NULL DEFAULT 0,
  max_attempts    INT  NOT NULL DEFAULT 5,
  expires_at      TIMESTAMPTZ NOT NULL, -- 建议 5 分钟
  consumed_at     TIMESTAMPTZ,          -- 成功核验后置位，禁止复用
  created_at      TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_otp_phone   ON user_otp_challenge(phone_hash, created_at DESC);
CREATE INDEX idx_otp_expiry  ON user_otp_challenge(expires_at);

-- 会话（user_id 为空表示访客；撤销以 revoked_at 标记，access token 内省据此判活）
CREATE TABLE user_session (
  session_id      TEXT PRIMARY KEY,
  user_id         TEXT REFERENCES user_account(user_id) ON DELETE CASCADE,  -- NULL=访客
  is_guest        BOOLEAN NOT NULL DEFAULT FALSE,
  app_id          TEXT NOT NULL DEFAULT '',
  client_platform TEXT CHECK (client_platform IN
                    ('WEB','IOS','ANDROID','WECHAT_MINIPROGRAM','DOUYIN_MINIPROGRAM')),
  device_id       TEXT,
  auth_tier       TEXT NOT NULL DEFAULT 'STANDARD' CHECK (auth_tier IN ('STANDARD','RISK_CHALLENGE')),
  scopes          TEXT[] NOT NULL DEFAULT '{}',
  created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
  last_active_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  revoked_at      TIMESTAMPTZ                       -- NULL=活跃
);
CREATE INDEX idx_session_user_active ON user_session(user_id) WHERE revoked_at IS NULL;
CREATE INDEX idx_session_device      ON user_session(app_id, device_id);

-- 访客设备 → 当前访客会话映射（EnsureGuestSession 幂等键）
CREATE TABLE user_guest_device (
  app_id     TEXT NOT NULL DEFAULT '',
  device_id  TEXT NOT NULL,
  session_id TEXT NOT NULL REFERENCES user_session(session_id) ON DELETE CASCADE,
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (app_id, device_id)
);

-- refresh token（仅存指纹/哈希；支持轮换链与重放检测；access token 短效无状态，撤销靠会话 revoked_at）
CREATE TABLE user_refresh_token (
  refresh_jti       TEXT PRIMARY KEY,            -- token 标识（不可逆）
  session_id        TEXT NOT NULL REFERENCES user_session(session_id) ON DELETE CASCADE,
  user_id           TEXT REFERENCES user_account(user_id) ON DELETE CASCADE,
  token_fingerprint TEXT NOT NULL UNIQUE,        -- HMAC/sha256(refresh_token)，明文不落库
  scopes            TEXT[] NOT NULL DEFAULT '{}',
  issued_at         TIMESTAMPTZ NOT NULL DEFAULT now(),
  expires_at        TIMESTAMPTZ NOT NULL,
  rotated_from_jti  TEXT,                         -- 轮换来源，构成轮换链
  revoked_at        TIMESTAMPTZ,                  -- 撤销 / 被轮换后置位
  reused_at         TIMESTAMPTZ                   -- 已轮换 token 被重放：触发安全撤销与告警
);
CREATE INDEX idx_refresh_session ON user_refresh_token(session_id);
CREATE INDEX idx_refresh_active  ON user_refresh_token(user_id) WHERE revoked_at IS NULL;
CREATE INDEX idx_refresh_expiry  ON user_refresh_token(expires_at);
```

### 14.2 偏好、收藏、历史、反馈

```sql
-- 偏好（整文档替换；preferences_version 为乐观锁版本，单调递增）
CREATE TABLE user_preferences (
  app_id              TEXT NOT NULL DEFAULT '',
  user_id             TEXT NOT NULL REFERENCES user_account(user_id) ON DELETE CASCADE,
  preferences_version BIGINT NOT NULL DEFAULT 0,
  theme_interests     TEXT[] NOT NULL DEFAULT '{}',
  content_filters     JSONB  NOT NULL DEFAULT '{}'::jsonb,   -- map<string,string>
  default_sort        TEXT,
  updated_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (app_id, user_id)
);

-- 收藏（唯一约束保证幂等；游标按收藏时间倒序翻页）
CREATE TABLE user_favorite (
  favorite_id  TEXT PRIMARY KEY,
  app_id       TEXT NOT NULL DEFAULT '',
  user_id      TEXT NOT NULL REFERENCES user_account(user_id) ON DELETE CASCADE,
  content_id   TEXT NOT NULL,
  content_type TEXT NOT NULL DEFAULT 'GUIDE_CARD' CHECK (content_type IN ('GUIDE_CARD')),
  favorited_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (app_id, user_id, content_id)
);
CREATE INDEX idx_favorite_list ON user_favorite(app_id, user_id, favorited_at DESC, favorite_id DESC);

-- 历史（聚合“最近浏览”；owner_key = user_id 或访客 session_id）
CREATE TABLE user_history (
  app_id           TEXT NOT NULL DEFAULT '',
  owner_key        TEXT NOT NULL,
  content_id       TEXT NOT NULL,
  content_type     TEXT NOT NULL DEFAULT 'GUIDE_CARD' CHECK (content_type IN ('GUIDE_CARD')),
  first_seen_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
  last_seen_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
  impression_count INT NOT NULL DEFAULT 1,
  source_surface   TEXT NOT NULL DEFAULT 'FEED' CHECK (source_surface IN ('FEED','SEARCH','DETAIL')),
  PRIMARY KEY (app_id, owner_key, content_id)
);
-- 列表游标：last_seen_at DESC，时间相同按 content_id ASC 稳定排序（对齐 api.md §5.9）
CREATE INDEX idx_history_list ON user_history(app_id, owner_key, last_seen_at DESC, content_id ASC);

-- 反馈（结构化；rating 量程 1.0~5.0；free_text 限长，PII 清洗在管道）
CREATE TABLE user_feedback (
  feedback_id       TEXT PRIMARY KEY,
  app_id            TEXT NOT NULL DEFAULT '',
  actor_user_id     TEXT,
  actor_session_id  TEXT,
  target_type       TEXT NOT NULL CHECK (target_type IN
                      ('GUIDE_CARD','RECOMMENDATION_RESULT','APP','OTHER')),
  target_id         TEXT NOT NULL,
  rating            DOUBLE PRECISION CHECK (rating IS NULL OR (rating >= 1.0 AND rating <= 5.0)),
  reason_codes      TEXT[] NOT NULL DEFAULT '{}',
  free_text         TEXT,
  client_request_id TEXT,
  created_at        TIMESTAMPTZ NOT NULL DEFAULT now(),
  CHECK (actor_user_id IS NOT NULL OR actor_session_id IS NOT NULL)
);
CREATE INDEX idx_feedback_target ON user_feedback(target_type, target_id, created_at DESC);
CREATE INDEX idx_feedback_actor  ON user_feedback(actor_user_id, created_at DESC);
```

### 14.3 同意、信号引用、幂等与 outbox

```sql
-- 同意当前态（版本化；支持查询/导出/删除，对齐《个人信息保护法》）
CREATE TABLE user_consent (
  app_id                  TEXT NOT NULL DEFAULT '',
  user_id                 TEXT NOT NULL REFERENCES user_account(user_id) ON DELETE CASCADE,
  personalization_allowed BOOLEAN NOT NULL DEFAULT FALSE,
  analytics_allowed       BOOLEAN NOT NULL DEFAULT FALSE,
  marketing_allowed       BOOLEAN NOT NULL DEFAULT FALSE,
  consent_version         TEXT NOT NULL,         -- 合规文档版本
  jurisdiction            TEXT,                  -- 由网关/合规链路推导
  updated_at              TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (app_id, user_id)
);

-- 同意变更审计历史（不可变追加；满足可追溯/可证明同意）
CREATE TABLE user_consent_history (
  consent_event_id        TEXT PRIMARY KEY,
  app_id                  TEXT NOT NULL DEFAULT '',
  user_id                 TEXT NOT NULL,
  personalization_allowed BOOLEAN NOT NULL,
  analytics_allowed       BOOLEAN NOT NULL,
  marketing_allowed       BOOLEAN NOT NULL,
  consent_version         TEXT NOT NULL,
  jurisdiction            TEXT,
  source                  TEXT,                  -- user / admin / compliance
  recorded_at             TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_consent_hist_user ON user_consent_history(app_id, user_id, recorded_at DESC);

-- signal_bundle_ref（不透明引用；payload 为粗粒度兴趣/近期摘要，不含推荐分数）
CREATE TABLE user_signal_bundle_ref (
  app_id            TEXT NOT NULL DEFAULT '',
  user_key          TEXT NOT NULL,              -- user_id 或访客 session_id
  scene             TEXT NOT NULL,              -- 已注册的 snake_case 场景
  signal_bundle_ref TEXT NOT NULL,              -- 不可猜测的不透明引用
  bundle_version    TEXT NOT NULL DEFAULT 'v1',
  scopes            TEXT[] NOT NULL DEFAULT '{}',
  payload           JSONB  NOT NULL DEFAULT '{}'::jsonb,
  expires_at        TIMESTAMPTZ NOT NULL,
  created_at        TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (app_id, user_key, scene)
);
CREATE UNIQUE INDEX uq_signal_ref     ON user_signal_bundle_ref(signal_bundle_ref);
CREATE INDEX        idx_signal_expiry ON user_signal_bundle_ref(expires_at);

-- 幂等/去重键（历史事件、反馈写入的窗口去重，对齐 api.md §7）
CREATE TABLE user_dedup_key (
  scope_kind  TEXT NOT NULL CHECK (scope_kind IN ('HISTORY_EVENT','FEEDBACK')),
  owner_key   TEXT NOT NULL,            -- owner / actor（user_id 或 session_id）
  client_key  TEXT NOT NULL,            -- client_event_id / client_request_id
  result_ref  TEXT,                     -- 首次成功结果（如 feedback_id）
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (scope_kind, owner_key, client_key)
);
CREATE INDEX idx_dedup_created ON user_dedup_key(created_at);

-- 事务性 outbox（领域事件；与业务写同事务提交，relay 投递 Kafka，见 workflow §10）
CREATE TABLE user_outbox (
  event_id     TEXT PRIMARY KEY,
  topic        TEXT NOT NULL,           -- user.<event>
  event_key    TEXT NOT NULL,           -- 分区 key（user_id / user_key）
  payload      JSONB NOT NULL,
  created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
  published_at TIMESTAMPTZ              -- NULL=未投递
);
CREATE INDEX idx_user_outbox_unpublished ON user_outbox(created_at) WHERE published_at IS NULL;
```

### 14.4 逻辑实体 ↔ 物理表对照

| 逻辑实体（§1–§10） | 物理表 |
|--------------------|--------|
| User | `user_account` |
| 登录身份（手机/OAuth） | `user_identity`、`user_otp_challenge` |
| Session | `user_session`、`user_guest_device` |
| Token 元数据 | `user_refresh_token`（access 短效无状态，撤销靠 `user_session.revoked_at`） |
| Profile | `user_account`（展示字段） |
| Preferences | `user_preferences` |
| Favorite | `user_favorite` |
| HistoryItem | `user_history`、去重 `user_dedup_key` |
| Feedback | `user_feedback`、去重 `user_dedup_key` |
| Consent | `user_consent`、`user_consent_history` |
| SignalBundleRef | `user_signal_bundle_ref` |
| 领域事件 | `user_outbox` |

## 15. 迁移策略

- **迁移文件位置与命名**：所有 schema 变更以 SQL 迁移落地，文件位于 `services/foundation/migrations/`，命名 `NNNN_user_<change>.sql`（如 `0002_user_init.sql`、`0008_user_add_refresh_reuse.sql`），按全局序号顺序执行。序号在 foundation 迁移目录内全局唯一（与其他域共用同一序列，落地时取下一个未用编号，避免冲突）。
- **幂等执行**：每个迁移用 `CREATE TABLE IF NOT EXISTS` / `CREATE INDEX IF NOT EXISTS` / `ALTER TABLE ... ADD COLUMN IF NOT EXISTS`，可重复执行不报错。
- **应用记录**：已应用版本写入全局 `schema_migrations` 表（见 `services/foundation/migrations/0000_schema_registry.sql`）；服务启动或独立迁移任务执行未应用迁移（见 development.md §运行与 deploy/README）。
- **空库初始化**：首次启动对空库执行全部 `NNNN_user_*.sql` 建表建索引即可对外服务；本域**不预置业务种子数据**（账户由真实注册/访客流程产生）。多应用场景下，`app_id` 注册表与认证/同意策略属配置（development.md §多应用配置），不随迁移注入。
- **向后兼容**：枚举仅追加 `CHECK` 取值，不删除/不改义；列只新增可空或带默认值；已发布列不改语义与类型。破坏性变更须新迁移 + `changelog.md` 标注 + 通知 `gateway`、`recommendation-server` 消费方。
- **与现状差异**：仓库内 `services/user-server/migrations/0001_user_init.sql` 为早期本地引导脚本（偏好/同意以 hex blob、`content_type` 为整型）。本节 DDL 为商业化上线权威结构；实现迁移到 foundation 迁移序列时须同步调整 `src/pg_user_store.*`，并以本文件为准。该差异在 `changelog.md` Unreleased 跟踪，属实现期对齐项，不阻塞依据本文档的并行开发。

## 16. 数据保留与隐私（对齐《个人信息保护法》）

### 16.1 最小化与脱敏

- **采集最小化**：本域只持有支撑账号、会话、用户态功能所必需的字段；不存储完整内容正文、原始点击流或第三方画像。
- **敏感字段处理**：手机号以 `phone_hash`（HMAC）唯一查找、明文按需加密为 `phone_enc`；OAuth `oauth_subject` 视为敏感标识；OTP 仅存 `otp_hash`；refresh token 仅存 `token_fingerprint`；**严禁**在日志或事件 payload 落明文 token / 手机号 / OTP（见 api.md §7、workflow §10）。
- **下游最小暴露**：跨服务只传 `user_id` 与非敏感资料/标志；PII 不离开本域边界。

### 16.2 保留周期（v1 落定，具体值可由配置覆盖但须有定值）

| 数据 | 保留策略 |
|------|----------|
| `user_otp_challenge` | 过期 + 24h 后清理（仅作短期防重放与风控） |
| `user_session`（已撤销/过期） | 末活跃后 90 天清理 |
| `user_refresh_token`（已撤销/过期） | 过期/撤销后 30 天清理（保留 `reused_at` 安全证据期内不删） |
| `user_history` | 滚动保留窗口默认 180 天（`last_seen_at` 之前的行后台清理）；用户可主动 `ClearHistory` |
| `user_dedup_key` | 过窗即可清理：保留 ≥ 去重窗口（24h），上限 7 天 |
| `user_outbox`（已投递） | 投递后保留 ≥ 30 天再清理（便于补投/审计） |
| `user_feedback` | 默认保留 2 年；到期后清空/匿名化 `free_text`，保留聚合统计 |
| `user_consent` / `user_consent_history` | 同意凭证保留 ≥ 3 年（可证明合规）；账户删除后仍保留审计历史（去标识化关联） |
| `user_account` / `user_identity`（已删除账户） | `DELETED_PENDING` 软删冷静期默认 15 天，到期硬清除身份/PII |

### 16.3 软删除、注销与数据主体权利

- **软删除/注销**：账户删除请求将 `account_status` 置 `DELETED_PENDING`、`deleted_at` 置位，立即撤销其全部会话与 refresh token，停止个性化与营销。冷静期内可恢复；到期由后台任务硬清除 `user_identity`（手机/OAuth）、伪名化 `user_history`/`user_feedback` 的 `owner_key`/`actor_*`，并发 `user.account_deleted` 事件供下游擦除派生数据（workflow §10）。
- **可访问/可导出**：支持按 `user_id` 导出个人数据集合（资料、偏好、收藏、历史、同意当前态与历史）；导出由网关/运营受信链路触发，本域提供查询面。
- **可删除/可撤回同意**：`UpdateConsent` 即时生效并写 `user_consent_history`；撤回个性化同意后，`GetSignalBundleRef`/`ResolveSignalBundle` 按 api.md §7 降级或拒绝。
- **管辖区**：`jurisdiction` 由网关/合规链路推导（如 `CN`），用于按区域应用默认值与保留差异；普通调用方不得改写。
