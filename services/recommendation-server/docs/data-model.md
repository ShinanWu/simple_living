# recommendation-server — Data model (v1)

§1–§8 定义对外契约用的**逻辑类型**；§9 起为 **v1 权威存储的具体物理模型**：以 **PostgreSQL 为权威**（场景配置、策略、候选池/召回集、特征、推荐结果元数据、outbox），**Redis 仅缓存**推荐结果与特征，**Kafka 仅异步**（消费 content 事件、产出本域事件）。任何缓存/进程内态不得作为跨实例权威。对外 ID 统一字符串 `*_id`，与 `.cursor/rules/shared-contracts.mdc` 命名一致。

## 1. RecommendationRequestContext

Envelope describing **who**, **where**, and **environment** for ranking and compliance.

| Field | Description |
|-------|-------------|
| `user_id` | Authenticated user, if any |
| `session_id` | Stable anonymous or device-bound session |
| `locale` | Language/region for copy and eligible catalog |
| `channel` | App store, mini-program, web, etc. |
| `app_version` | Client version for capability gating |
| `device_tier` | Optional coarse performance hint |
| `consent` | Flags from `user-server`: e.g. personalization allowed, ad personalization |
| `signal_bundle_ref` | Opaque handle to pre-resolved signals (preferred) or inline summary per contract |

**Rule**: No raw PII beyond what gateway already normalized; sensitive fields pass by reference when possible.

---

## 2. Scene

A **scene** is a stable key mapping to **default strategy**, **recall sources**, **limits**, and **explanation policy**.

Examples (illustrative and aligned with `.cursor/rules/shared-contracts.mdc`):

| Scene key | Typical use |
|-----------|-------------|
| `home_feed` | Mixed personalized feed |
| `home_popular` | Trending strip |
| `theme_feed` | Theme-browse ranking |
| `guide_detail_related` | Related content under an article/card |
| `cold_start` | Default flow for new users or missing personalization context |

Properties:

| Property | Description |
|----------|-------------|
| `key` | Globally unique string |
| `default_strategy_id` | Fallback when experiment not assigned |
| `max_items` | Upper bound per request |
| `personalization_mode` | `full`, `session_only`, `none` |

---

## 3. Strategy

A **strategy** is a versioned configuration: which recall lanes run, merge rules, ranking model handle, and explanation templates.

| Field | Description |
|-------|-------------|
| `id` | Strategy identifier |
| `version` | Monotonic semver or integer revision |
| `pipeline` | Named DAG: recall → filter → rank → post-process |
| `experiment_key` | Optional A/B bucket key for analytics |
| `updated_at` | Last config publish time |

Strategies are **owned** by recommendation-server; **content** of templates may reference copy keys maintained with product.

---

## 4. RecommendationItem (result card reference)

Minimal **reference** to content; hydration is `platform/backoffice-backend` responsibility unless gateway contract adds denormalized fields. Shared external shape should stay aligned with `.cursor/rules/shared-contracts.mdc`.

| Field | Type | Description |
|-------|------|-------------|
| `guide_card_id` | string | Stable guide card identifier for cross-domain and client correlation |
| `content_ref` | object | Optional richer reference when the item points to non-card content such as topic or article |
| `score` | number | Optional final score for debugging (may be omitted in prod) |
| `rank` | integer | 1-based ranking position, aligned with contracts and attribution fields |
| `recall_sources` | string[] | Optional coarse tags: `cf`, `content_similar`, `editorial`, `popular` |
| `placement` | string | Slot id within scene (for analytics correlation) |
| `trace_ref` | string | Opaque token for impression joining with `tracking-server` |

---

## 5. Explanation fields

Attached per item (either inline in `query`/`popular` or via `explain` API).

| Field | Description |
|-------|-------------|
| `summary` | Short user-visible string, e.g. “Similar to what you saved” |
| `reason_codes` | Stable machine-readable codes for client logic / QA |
| `entities` | Optional referenced topic/category ids safe to show |
| `confidence_band` | Optional `high` / `medium` / `low` for UI softening |
| `policy_version` | Version of explanation policy for compliance audit |

**Constraint**: Explanations must align with **allowed** reason codes for the jurisdiction/product; no fabricated claims.

---

## 6. Ranking features (high level)

Features are **derived** at request time from context + candidate metadata. Grouped for implementation planning; not an exhaustive list.

| Group | Examples |
|-------|----------|
| **User / session** | Recency-weighted interactions, category affinity buckets, saved items count (from `user-server` bundle) |
| **Content** | Freshness, quality score, theme match, language match (`platform/backoffice-backend`) |
| **Graph / similarity** | Embedding distance, co-click/co-save stats |
| **Popularity** | Global or segmented CTR, save rate, velocity |
| **Diversity / fairness** | Max per author, per category cap, duplicate near-neighbor suppression |
| **Governance** | Hard suppress flags, required commercial labels already on content record |

Feature computation **lives** in recommendation-server pipelines; **sources** remain authoritative in peer domains.

---

## 7. Pagination model

- **Cursor-based** preferred for feeds (`next_cursor` opaque).
- **Offset** allowed only for bounded surfaces (e.g. short “related” strip) to keep caches predictable.

---

## 8. Relationships diagram (logical)

```text
RecommendationRequestContext + Scene
           │
           ▼
    Strategy (versioned)
           │
           ├─► Recall ──► Candidate set (content ids)
           │
           ├─► Filter (eligibility, governance)
           │
           ├─► Rank (features → scores)
           │
           └─► Explain (policy + reasons)
                     │
                     ▼
              RecommendationItem[]
```

---

## 9. PostgreSQL 物理模型（权威存储）

> v1 权威存储为 PostgreSQL（foundation 节点，`-pg_conninfo` 连接）。所有 `*_id` 为 `TEXT`（应用层生成 ULID/UUID 字符串）。时间统一 `TIMESTAMPTZ`。枚举以 `TEXT` + `CHECK` 落库（不使用 PG enum，便于追加值，与 `scene` 仅追加约定一致）。数组用 `TEXT[]`，复杂/可演进结构用 `JSONB`。运营/配置写入型实体统一含审计列：`created_at/updated_at TIMESTAMPTZ NOT NULL DEFAULT now()`、`created_by/updated_by TEXT`、`revision BIGINT NOT NULL DEFAULT 1`（乐观锁）。
>
> **权威 vs 派生**：本域不拥有用户画像与内容主数据；候选池/特征是**从 content 事件与 user 信号派生的本地只读视图**，可重建（见 §11 重建流程）。

### 9.1 场景配置 `recommendation_scene_config`

每个 `scene` 的默认策略、限额、个性化模式与解释策略。`scene` 取值域与 `.cursor/rules/shared-contracts.mdc` / api.md §1 一致（仅追加）。

```sql
CREATE TABLE recommendation_scene_config (
  scene                TEXT PRIMARY KEY
    CHECK (scene IN ('home_feed','home_popular','theme_feed','guide_detail_related','search','cold_start')),
  default_strategy_id  TEXT NOT NULL,                 -- 指向 recommendation_strategy.strategy_id
  default_strategy_ver TEXT NOT NULL,                 -- 该 scene 当前生效版本
  max_items            INT  NOT NULL DEFAULT 100 CHECK (max_items > 0 AND max_items <= 100),
  default_limit        INT  NOT NULL DEFAULT 20  CHECK (default_limit > 0 AND default_limit <= 100),
  personalization_mode TEXT NOT NULL DEFAULT 'full'
                       CHECK (personalization_mode IN ('full','session_only','none')),
  requires_auth        BOOLEAN NOT NULL DEFAULT false,-- true 时访客触发 20001
  recall_lanes         TEXT[] NOT NULL DEFAULT '{}',  -- 启用的召回 lane：cf/content_similar/editorial/popular
  explanation_policy   JSONB NOT NULL DEFAULT '{}',   -- 允许的 reason_codes、policy_version、置信带规则
  enabled              BOOLEAN NOT NULL DEFAULT true,
  revision   BIGINT NOT NULL DEFAULT 1,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  created_by TEXT, updated_by TEXT
);
```

### 9.2 策略 `recommendation_strategy`

版本化策略；`(strategy_id, version)` 不可变，发布即冻结，回滚指向旧版本。

```sql
CREATE TABLE recommendation_strategy (
  strategy_id    TEXT NOT NULL,
  version        TEXT NOT NULL,                       -- semver 或单调整型修订（域内统一）
  pipeline       TEXT NOT NULL,                       -- 命名 DAG：recall-filter-rank-rerank
  experiment_key TEXT,                                -- 可选 A/B 桶键
  model_ref      TEXT,                                -- 排序模型/规则集 artifact 引用（opaque）
  config         JSONB NOT NULL DEFAULT '{}',         -- 召回/排序/多样性参数
  reason_codes   TEXT[] NOT NULL DEFAULT '{}',        -- 该策略允许产出的稳定 reason_codes 全集
  status         TEXT NOT NULL DEFAULT 'STAGING'
                 CHECK (status IN ('STAGING','CANARY','ACTIVE','DEPRECATED')),
  published_at   TIMESTAMPTZ,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  created_by TEXT, updated_by TEXT,
  PRIMARY KEY (strategy_id, version)
);
CREATE INDEX idx_strategy_status ON recommendation_strategy(status) WHERE status IN ('CANARY','ACTIVE');
```

### 9.3 候选池 / 召回集 `recommendation_candidate`

由 `content.published` / `content.offlined` 事件**幂等驱动**（见 workflow §5）的本地可召回视图；不是内容主数据副本，仅保留召回/排序/过滤所需的最小派生字段。游标翻页与按主题召回的核心列表索引建在此表。

```sql
CREATE TABLE recommendation_candidate (
  guide_card_id     TEXT PRIMARY KEY,                 -- 与 platform/backoffice-backend 同一 ID 空间
  content_type      TEXT NOT NULL DEFAULT 'guide_card'
                    CHECK (content_type IN ('guide_card')),  -- v1 guide_card-first，仅追加
  themes            TEXT[] NOT NULL DEFAULT '{}',     -- 主题 slug（与 content ThemeSummary.slug 对齐）
  tags              TEXT[] NOT NULL DEFAULT '{}',     -- 与 content tag_ids 同一 ID 空间
  languages         TEXT[] NOT NULL DEFAULT '{}',
  quality_score     DOUBLE PRECISION NOT NULL DEFAULT 0, -- content 侧质量分快照（派生）
  freshness_at      TIMESTAMPTZ NOT NULL,             -- 内容发布/更新时间，用于新鲜度与游标
  eligibility       TEXT NOT NULL DEFAULT 'ELIGIBLE'
                    CHECK (eligibility IN ('ELIGIBLE','SUPPRESSED','OFFLINE')), -- 治理/下架软状态
  published_revision BIGINT NOT NULL DEFAULT 0,       -- 来自 content 事件，缓存失效版本戳
  facets            JSONB NOT NULL DEFAULT '{}',      -- 可演进的召回/排序辅助特征
  last_event_id     TEXT,                             -- 最近应用的 content 事件，用于幂等比对
  updated_at        TIMESTAMPTZ NOT NULL DEFAULT now()
);
-- 主题召回 + 新鲜度游标：按主题筛选后以 (freshness_at, guide_card_id) 倒序稳定翻页
CREATE INDEX idx_candidate_themes      ON recommendation_candidate USING GIN (themes);
CREATE INDEX idx_candidate_tags        ON recommendation_candidate USING GIN (tags);
CREATE INDEX idx_candidate_fresh_cursor
  ON recommendation_candidate(eligibility, freshness_at DESC, guide_card_id DESC);
CREATE INDEX idx_candidate_quality
  ON recommendation_candidate(eligibility, quality_score DESC, guide_card_id DESC);
```

### 9.4 热门候选 `recommendation_popularity`

`GetPopularRecommendations` 用的窗口化热度（由 tracking 点击/曝光事件或离线任务回填，见 workflow §5.3）。

```sql
CREATE TABLE recommendation_popularity (
  guide_card_id  TEXT NOT NULL REFERENCES recommendation_candidate(guide_card_id) ON DELETE CASCADE,
  window         TEXT NOT NULL CHECK (window IN ('24h','7d','30d')),
  theme          TEXT NOT NULL DEFAULT '',            -- '' 表示全局；否则按主题 slug 分桶
  score          DOUBLE PRECISION NOT NULL DEFAULT 0, -- 归一化热度（CTR/save/velocity 综合）
  rank_hint      INT,                                 -- 预排名（可空，运行期可重排）
  computed_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (guide_card_id, window, theme)
);
CREATE INDEX idx_popularity_rank
  ON recommendation_popularity(window, theme, score DESC, guide_card_id DESC);
```

### 9.5 特征 `recommendation_feature`

请求期排序所需的**预计算特征快照**（实体可为 card 或 user/session bundle 引用）；在线特征只读，源权威仍在对应域。

```sql
CREATE TABLE recommendation_feature (
  entity_kind  TEXT NOT NULL CHECK (entity_kind IN ('GUIDE_CARD','USER','SESSION')),
  entity_id    TEXT NOT NULL,                         -- card_id 或 signal_bundle_ref / user_id（脱敏引用）
  feature_set  TEXT NOT NULL,                         -- 命名特征组，如 'card_quality_v1'、'user_affinity_v1'
  features     JSONB NOT NULL DEFAULT '{}',
  computed_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  ttl_at       TIMESTAMPTZ,                           -- 过期后视为缺失并触发重算/降级
  PRIMARY KEY (entity_kind, entity_id, feature_set)
);
CREATE INDEX idx_feature_ttl ON recommendation_feature(ttl_at) WHERE ttl_at IS NOT NULL;
```

> **隐私**：`entity_kind='USER'` 仅存 `signal_bundle_ref` 或脱敏 `user_id`，**不存原始 PII**；`features` 内仅放粗粒度桶/分数。

### 9.6 推荐结果元数据 `recommendation_result`

落库**仅为曝光-点击归因串联与 `ExplainRecommendations` 复算**，非读路径热路径来源（读结果由 Redis 缓存 + 在线计算）。保留期短（见 §12）。

```sql
CREATE TABLE recommendation_result (
  recommendation_id  TEXT PRIMARY KEY,                -- 对外曝光串联 ID（shared-contracts）
  scene              TEXT NOT NULL
    CHECK (scene IN ('home_feed','home_popular','theme_feed','guide_detail_related','search','cold_start')),
  strategy_id        TEXT NOT NULL,
  strategy_version   TEXT NOT NULL,
  experiment_key     TEXT,
  context_fingerprint TEXT NOT NULL,                  -- locale/channel/theme/cursor 等非敏感维度哈希
  item_card_ids      TEXT[] NOT NULL DEFAULT '{}',    -- 有序 guide_card_id（rank = 数组下标+1）
  revision_watermark BIGINT NOT NULL DEFAULT 0,       -- 生成时所见最大 published_revision，用于缓存失效
  generated_at       TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_result_generated ON recommendation_result(generated_at);
```

### 9.7 事件幂等 `recommendation_consumed_event`

消费 content/tracking 事件的去重表，按 `event_id` 幂等（at-least-once 投递）。

```sql
CREATE TABLE recommendation_consumed_event (
  event_id     TEXT PRIMARY KEY,                      -- 来源事件 event_id
  source_topic TEXT NOT NULL,                         -- content.published / content.offlined / tracking.click ...
  consumed_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_consumed_at ON recommendation_consumed_event(consumed_at);
```

### 9.8 领域事件 outbox `recommendation_outbox`

本域若产出事件（如 `recommendation.strategy_activated`、`recommendation.candidate_pool_rebuilt`），事务内写 outbox，由 relay 投递 Kafka（见 workflow §6）。

```sql
CREATE TABLE recommendation_outbox (
  event_id     TEXT PRIMARY KEY,
  topic        TEXT NOT NULL,                         -- recommendation.<event>
  event_key    TEXT NOT NULL,                         -- 分区 key（如 scene 或 strategy_id）
  payload      JSONB NOT NULL,
  created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
  published_at TIMESTAMPTZ                            -- NULL=未投递
);
CREATE INDEX idx_rec_outbox_unpublished ON recommendation_outbox(created_at) WHERE published_at IS NULL;
```

---

## 10. Redis 缓存与失效

Redis 仅缓存，不持久化权威。任何缓存值都带**版本戳**，由 content 事件驱动失效，避免下架/回滚后仍曝光旧内容。

| 缓存键 | 值 | TTL | 失效方式 |
|--------|----|-----|----------|
| `rec:result:{scene}:{ctx_fp}:{strategy_ver}:{cursor}` | 序列化结果（item 列表 + `recommendation_id` + `revision_watermark`） | feed 30–60s；`home_popular` 5–15min | TTL + 版本戳校验：读取时若 `revision_watermark < ` 候选池当前最大 `published_revision` 则视为脏，回源重算 |
| `rec:candidate:{scene}:{theme}` | 召回集 card_id 列表 + 各自 `published_revision` | 60s | TTL + `content.published`/`content.offlined` 事件按 `guide_card_id` 主动剔除/标脏 |
| `rec:feature:{entity_kind}:{entity_id}:{feature_set}` | 特征快照 | 对齐 `recommendation_feature.ttl_at` | TTL；源更新时按 key 失效 |
| `rec:popular:{window}:{theme}` | 热门有序列表 | 5–15min | TTL + 离线/事件刷新后整键替换 |

失效规则：

- **版本戳优先**：缓存值内嵌生成时的 `revision_watermark`/`published_revision`；命中后与候选池版本比较，过期即回源。`published_revision` 语义与 `platform/backoffice-backend` 一致（来自 `content.published` 事件）。
- **事件主动失效**：消费 `content.offlined` 时立即将对应 `guide_card_id` 从 `rec:candidate:*` 标脏并把候选 `eligibility=OFFLINE`，下次召回不再纳入。
- **缓存缺失不影响正确性**：Redis 不可用时回源 PostgreSQL + 在线计算，仅延迟升高（命中率 SLO 不达标告警）。

---

## 11. 迁移策略

- 迁移文件位于 `services/foundation/migrations/`，命名 `NNNN_recommendation_<change>.sql`（如 `0004_recommendation_init.sql`、`0009_recommendation_add_popularity.sql`；`NNNN` 为 foundation 迁移目录内全局递增序号，落地时取下一个未用编号），按序号幂等执行（`CREATE TABLE IF NOT EXISTS` / `ALTER TABLE ... ADD COLUMN IF NOT EXISTS` / `CREATE INDEX IF NOT EXISTS`）。
- 服务启动时执行未应用迁移，已应用版本记录在 `schema_migrations`（见 `services/foundation/migrations/0000_schema_registry.sql`）。
- **向后兼容**：`scene`、`personalization_mode`、`status`、`eligibility` 等 `CHECK` 枚举**只追加值不删除/改义**；列只新增可空或带默认；已发布列不改语义。破坏性变更需新迁移 + `changelog.md` 标注 + 通知 `gateway` 消费方。
- **空库初始化**：首次启动且 `recommendation_scene_config` 为空时，迁移内置 `INSERT ... ON CONFLICT DO NOTHING` 写入六个 scene 的默认配置与一个默认策略；导购内容种子由 `backoffice-backend` 写入 PostgreSQL 并导出 snapshot，读侧不再硬编码 fallback。
- **候选池/特征可重建**：候选池、热度、特征均为派生数据，可丢弃后通过 ① 回放 Kafka `content.*` 事件或 ② 调用 `platform/backoffice-backend` 只读接口全量回填重建；重建不影响 content/user 权威，故迁移可在不阻塞写权威的前提下重置这些派生表。

---

## 12. 数据保留与隐私

| 表 | 保留 | 说明 |
|----|------|------|
| `recommendation_scene_config` / `recommendation_strategy` | 长期 | 配置与策略历史版本保留供回滚与审计 |
| `recommendation_candidate` / `recommendation_popularity` / `recommendation_feature` | 派生、可重建 | 随 content/tracking 事件滚动更新；`recommendation_feature` 按 `ttl_at` 过期清理 |
| `recommendation_result` | ≥ 30 天后可清理 | 仅用于曝光归因与 Explain 复算；与 tracking 点击归因窗口对齐 |
| `recommendation_consumed_event` / `recommendation_outbox`（已投递） | ≥ 30 天后可清理 | 幂等去重与事件投递记录 |

隐私：

- 本域**不存储原始 PII**。用户侧特征只以 `signal_bundle_ref` 或脱敏 `user_id` 引用（来自 `user-server`），`features` 仅放粗粒度桶/分数。
- `context_fingerprint`、`RequestContextEcho.query` 等须脱敏并限长，不落明文搜索词的可逆原文。
- `created_by/updated_by` 仅为运营/系统账号 ID，由 `gateway` 注入，不含明文凭据。

---

## 13. 与契约目录的关系

- 对外结果字段（`recommendation_id`、`scene`、`items[]`、`rank`、`reason_tags`、分页）以 `.cursor/rules/shared-contracts.mdc` 为准；若本文件与契约冲突，**以契约为准**并更新本文件与迁移。
- 候选池 `themes`/`tags` 与 `platform/backoffice-backend` 主题 slug、tag_id 同一 ID 空间；字典权威在 `platform/backoffice-backend`，本域只缓存派生视图（见 api.md §2.2）。
