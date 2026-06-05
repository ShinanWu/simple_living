# tracking-server — Data model

Logical model with the current PostgreSQL v1 implementation. Names are **canonical field names** for APIs and contracts.

## 1. Core entities

### 1.1 `TrackingLink` (assembled instance)

Represents one **assembled** outbound entry before or after first use.

| Field | Type | Description |
|-------|------|-------------|
| `link_ref` | string (UUID) | Primary key for assembly row. |
| `short_token` | string | Public opaque token embedded in `/t/{short_token}`. |
| `user_id` | string \| null | If authenticated; nullable for guest flows if allowed. |
| `device_id` | string \| null | Stable device id from auth/device service. |
| `content_ref` | object | `{ "content_id", "guide_card_id", ... }` — mirrors assemble request. |
| `placement` | string | Normalized placement key. |
| `affiliate_context_ref` | string | Opaque; maps to platform/backoffice-backend context. |
| `expires_at` | timestamp | URL expiry. |
| `status` | enum | `active` \| `expired` \| `revoked`. |
| `created_at` | timestamp | |

### 1.2 `Click`

Immutable fact: user initiated jump.

| Field | Type | Description |
|-------|------|-------------|
| `click_id` | string (UUID) | Primary key. |
| `link_ref` | string | FK → TrackingLink. |
| `short_token` | string | Denormalized for edge lookups. |
| `occurred_at` | timestamp | Server accept time. |
| `user_id` | string \| null | |
| `device_id` | string \| null | |
| `ip_hash` | string \| null | Privacy-preserving digest per retention policy. |
| `user_agent` | string \| null | Truncated/stripped per policy. |
| `attribution_snapshot` | object | Copy of resolved attribution at click time (see §3). |
| `redirect_state_at_click` | enum | Usually `pending` → becomes `resolved` after 302; edge may log `blocked_client` if WebView prevented. |

**Indexes (logical):** `(short_token, occurred_at)`, `(user_id, occurred_at)`, `(link_ref)` unique for idempotent click policy variant A.

### 1.3 `Conversion`

Business outcome attributed to traffic.

| Field | Type | Description |
|-------|------|-------------|
| `conversion_id` | string (UUID) | |
| `external_event_id` | string | Idempotency from source. |
| `source` | enum | `partner_postback` \| `affiliate_server` \| `manual_adjustment`. |
| `conversion_type` | enum | `order_paid`, … |
| `occurred_at` | timestamp | Partner time. |
| `ingested_at` | timestamp | |
| `click_id` | string \| null | |
| `matched_confidence` | enum | `exact` \| `inferred` \| `unmatched`. |
| `amount_minor` | int \| null | |
| `currency` | string \| null | ISO 4217 |
| `commission_minor` | int \| null | |
| `commission_estimate` | bool | |
| `attribution_snapshot` | object | As resolved at ingest. |
| `raw_payload_ref` | string \| null | |

**Unique:** `(source, external_event_id)`.

### 1.4 `CommissionDailyAggregate` (read model)

| Field | Type | Description |
|-------|------|-------------|
| `bucket_date` | date | |
| `user_id` | string \| null | Or creator account id if multi-tenant. |
| `currency` | string | |
| `reported_minor` | int | |
| `estimated_minor` | int | |
| `order_count` | int | |

Materialized by stream or batch jobs from `Conversion`.

### 1.5 `TrackingEventOutbox`

V1 uses a PostgreSQL outbox table before introducing a dedicated Kafka relay process.

| Field | Type | Description |
|-------|------|-------------|
| `event_id` | int | Monotonic primary key. |
| `topic` | string | Kafka topic name, e.g. `tracking.click.acked`. |
| `payload` | string | Minimal event payload or stable ID. |
| `published` | bool | Whether relay has published the event. |
| `created_at` | timestamp | |

---

## 2. Attribution payload (canonical)

Carried in signed token or stored as `attribution_snapshot` on Click/Conversion.

| Field | Required | Description |
|-------|----------|-------------|
| `click_id` | after click | Filled post hoc on conversion match. |
| `link_ref` | on click row | Ties to assembly. |
| `content_id` | yes | |
| `guide_card_id` | no | When applicable. |
| `placement` | yes | |
| `channel_code` | yes | Logical channel (e.g. `pdd`, `douyin`) from affiliate context resolution. |
| `campaign_slot` | no | Free-form A/B or ops slot. |
| `sub_id_1` … `sub_id_n` | no | Partner subid slots; **names** in shared contract. |

**Rule:** Anything in `attribution_snapshot` must be **non-secret** (no API keys). Partner auth stays in **platform/backoffice-backend**.

---

## 3. Redirect token payload (edge)

Opaque to clients; decoded only by tracking edge.

| Claim | Description |
|-------|-------------|
| `link_ref` | |
| `exp` | Expiry (unix) |
| `sig` / HMAC | Integrity |
| `ver` | Token format version |

Rotation: bump `ver` with dual validation window during rollout.

---

## 4. Entity relationships

```text
TrackingLink 1 —— * Click
Click 1 —— 0..1 Conversion  (via exact match)
Conversion * —— 0..1 Click (inferred matches may be many-to-one policy-dependent; default is 1:1 for paid order)
```

---

## 5. PostgreSQL physical model (authoritative storage)

> v1 authority is PostgreSQL (foundation node, connected via `-pg_conninfo`). All `*_id` / `*_ref` are `TEXT` (application-generated ULID/UUID strings). Time columns are `TIMESTAMPTZ`. Enums land as `TEXT` + `CHECK` (not PG `enum`, so values can be appended). Monetary values are integer minor units (`amount_minor` / `commission_minor`) + ISO 4217 `currency`; never floats. Redis is a cache / idempotency-window accelerator only and never the source of truth.

### 5.1 Tables and key DDL

```sql
-- 转链实例（assembled outbound link）
CREATE TABLE tracking_link (
  link_ref              TEXT PRIMARY KEY,
  short_token           TEXT NOT NULL UNIQUE,            -- public routing key in /t/{short_token}
  user_id               TEXT,                            -- nullable for guest
  device_id             TEXT,                            -- minimized; see §7
  content_id            TEXT,
  guide_card_id         TEXT,
  recommendation_id     TEXT,
  scene                 TEXT,
  item_rank             INT,
  placement             TEXT NOT NULL,
  channel_code          TEXT NOT NULL,                   -- resolved outbound channel (e.g. pdd, douyin)
  affiliate_context_ref TEXT NOT NULL,                   -- opaque, affiliate-owned
  spec_id               TEXT,                            -- materialized AffiliateLinkSpec reference (see api §11.1)
  attribution_snapshot  JSONB NOT NULL DEFAULT '{}',     -- non-secret AttributionPayload (§2)
  status                TEXT NOT NULL DEFAULT 'active'
                        CHECK (status IN ('active','expired','revoked')),
  expires_at            TIMESTAMPTZ NOT NULL,
  created_at            TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_link_expires        ON tracking_link(expires_at) WHERE status='active';
CREATE INDEX idx_link_content        ON tracking_link(guide_card_id, created_at DESC);
CREATE INDEX idx_link_user_created   ON tracking_link(user_id, created_at DESC) WHERE user_id IS NOT NULL;

-- 点击事实（append-only）
CREATE TABLE tracking_click (
  click_id                TEXT PRIMARY KEY,
  link_ref                TEXT NOT NULL REFERENCES tracking_link(link_ref),
  short_token             TEXT NOT NULL,                  -- denormalized for edge lookup
  occurred_at             TIMESTAMPTZ NOT NULL DEFAULT now(),
  user_id                 TEXT,
  device_id               TEXT,
  ip_hash                 TEXT,                            -- privacy-preserving digest, never raw IP
  user_agent              TEXT,                            -- truncated per policy
  attribution_snapshot    JSONB NOT NULL DEFAULT '{}',
  redirect_state_at_click TEXT NOT NULL DEFAULT 'resolved'
                          CHECK (redirect_state_at_click IN ('pending','resolved','degraded','blocked_client')),
  dedup_key               TEXT,                            -- (short_token, device_dedup_key) idempotent click policy
  created_at              TIMESTAMPTZ NOT NULL DEFAULT now()
);
-- core query indexes: by click_id (PK), by time, by content/channel dimensions
CREATE INDEX idx_click_token_time   ON tracking_click(short_token, occurred_at DESC);
CREATE INDEX idx_click_user_time    ON tracking_click(user_id, occurred_at DESC) WHERE user_id IS NOT NULL;
CREATE INDEX idx_click_link         ON tracking_click(link_ref);
CREATE INDEX idx_click_occurred     ON tracking_click(occurred_at DESC);
-- at-most-one primary click per dedup policy key (see api §10); partial unique skips NULL dedup_key
CREATE UNIQUE INDEX uq_click_dedup  ON tracking_click(short_token, dedup_key) WHERE dedup_key IS NOT NULL;

-- 转化事实（partner / affiliate normalized / manual）
CREATE TABLE tracking_conversion (
  conversion_id        TEXT PRIMARY KEY,
  source               TEXT NOT NULL
                       CHECK (source IN ('partner_postback','affiliate_server','manual_adjustment')),
  external_event_id    TEXT NOT NULL,                    -- source idempotency key; partner order postbacks carry external_order_id here
  external_order_id    TEXT,                             -- shared-contract field when distinct from external_event_id
  conversion_type      TEXT NOT NULL
                       CHECK (conversion_type IN ('order_paid','lead','install')),
  occurred_at          TIMESTAMPTZ NOT NULL,             -- partner time
  ingested_at          TIMESTAMPTZ NOT NULL DEFAULT now(),
  click_id             TEXT REFERENCES tracking_click(click_id),
  match_confidence     TEXT NOT NULL DEFAULT 'unmatched'
                       CHECK (match_confidence IN ('exact','inferred','unmatched')),
  amount_minor         BIGINT,
  currency             TEXT,                             -- ISO 4217
  commission_minor     BIGINT,
  commission_estimate  BOOLEAN NOT NULL DEFAULT false,
  attribution_snapshot JSONB NOT NULL DEFAULT '{}',
  raw_payload_ref      TEXT,                             -- pointer to encrypted blob; not stored inline
  created_at           TIMESTAMPTZ NOT NULL DEFAULT now()
);
-- idempotency: a conversion fact is unique per (source, external_event_id); see api §10 / §9.2
CREATE UNIQUE INDEX uq_conv_source_evt ON tracking_conversion(source, external_event_id);
CREATE INDEX idx_conv_click       ON tracking_conversion(click_id);
CREATE INDEX idx_conv_occurred    ON tracking_conversion(occurred_at DESC);
CREATE INDEX idx_conv_type_time   ON tracking_conversion(conversion_type, occurred_at DESC);

-- 佣金日聚合读模型（由 Conversion 物化；读路径只读此表）
CREATE TABLE tracking_commission_daily (
  bucket_date      DATE   NOT NULL,
  user_id          TEXT   NOT NULL DEFAULT '',           -- '' = all-up bucket; or creator/tenant id
  channel_code     TEXT   NOT NULL DEFAULT '',
  currency         TEXT   NOT NULL,
  reported_minor   BIGINT NOT NULL DEFAULT 0,            -- sum where commission_estimate=false
  estimated_minor  BIGINT NOT NULL DEFAULT 0,            -- sum where commission_estimate=true
  order_count      INT    NOT NULL DEFAULT 0,
  refreshed_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (bucket_date, user_id, channel_code, currency)
);
CREATE INDEX idx_comm_daily_date ON tracking_commission_daily(bucket_date DESC);

-- 短 token 表（PostgreSQL 权威；Redis 仅热读副本，见 §5.3）
CREATE TABLE tracking_short_token (
  short_token  TEXT PRIMARY KEY,
  link_ref     TEXT NOT NULL REFERENCES tracking_link(link_ref) ON DELETE CASCADE,
  token_ver    INT  NOT NULL DEFAULT 1,                  -- rotation version (claim `ver`)
  expires_at   TIMESTAMPTZ NOT NULL,
  revoked      BOOLEAN NOT NULL DEFAULT false,
  created_at   TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_short_token_expires ON tracking_short_token(expires_at) WHERE revoked=false;

-- 领域事件 outbox（事务内写入，relay 投递 Kafka，见 workflow §8）
CREATE TABLE tracking_outbox (
  event_id     TEXT PRIMARY KEY,                         -- ULID; also the consumer idempotency key
  topic        TEXT NOT NULL,                            -- tracking.link.created | tracking.click.acked | tracking.conversion.ingested
  event_key    TEXT NOT NULL,                            -- partition key (link_ref / click_id / conversion_id)
  payload      JSONB NOT NULL,
  created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
  published_at TIMESTAMPTZ                               -- NULL = not yet relayed
);
CREATE INDEX idx_tracking_outbox_unpublished ON tracking_outbox(created_at) WHERE published_at IS NULL;
```

> `TrackingEventOutbox` from §1.5 is the logical view of `tracking_outbox`; the physical table uses a string `event_id` (ULID) so it doubles as the consumer idempotency key, and `published_at` (NULL = unpublished) instead of a boolean.

### 5.2 Read model materialization

- `tracking_commission_daily` is rebuilt incrementally from `tracking_conversion` after each accepted ingest (stream or batch). It is a **read model**, safe to drop and rebuild; never written to by the interactive path.
- Reported vs estimated split mirrors `commission_estimate`; `GetCommissionSummary` / `ListCommissionItems` read this table, not raw conversions, to meet the read-view SLO.

### 5.3 Short token: authority vs hot read

- PostgreSQL `tracking_short_token` is authoritative for validity, expiry and revocation.
- Redis (when `-redis_addr` set) caches `short_token → {link_ref, spec ref, expires_at}` for edge resolution and holds the idempotency window for click dedup. A Redis miss/outage falls back to PostgreSQL; Redis is never the source of truth.

---

## 6. Migration strategy

- Migration files live in `services/foundation/migrations/`, named `NNNN_tracking_<change>.sql` (e.g. `0005_tracking_init.sql`; `NNNN` is a globally increasing sequence in the foundation migrations dir — pick the next unused number), applied in order and idempotently (`CREATE TABLE IF NOT EXISTS` / `CREATE INDEX IF NOT EXISTS` / `ALTER ... IF NOT EXISTS`).
- Applied versions are tracked in the shared `schema_migrations` registry (`services/foundation/migrations/0000_schema_registry.sql`); the service applies unapplied migrations on startup.
- **Backward compatibility:** enum `CHECK` sets are append-only; columns are added nullable or with defaults; published columns do not change meaning. Breaking changes require a new migration + a `changelog.md` entry + notifying `gateway` and the affiliate→tracking consumer.
- **Empty-DB init:** first start creates all tables above; tracking has no editorial seed, so no business seed data is required. The outbox starts empty.

---

## 7. Retention & PII (technical)

- **Click retention:** `tracking_click` kept ≥ 90 days hot for attribution windows, then archived/aggregated; daily aggregates (`tracking_commission_daily`) are kept long-term.
- **Conversion retention:** `tracking_conversion` kept long-term (financial reconciliation); `raw_payload_ref` blobs are encrypted at rest, access-audited, and purged on the partner-agreed retention window.
- **Outbox:** `tracking_outbox` rows with `published_at` set are purgeable after ≥ 30 days.
- **PII minimization:** never store raw client IP — only `ip_hash` (salted digest); `user_agent` is truncated; `device_id` is stored only when needed for dedup/attribution and is treated as personal data under governance/compliance retention. No partner secrets or raw credentials are ever persisted here.
- Hashing salts and retention windows are governed by `platform/backoffice-backend` + compliance docs; tracking implements the **technical** enforcement only.

---

## 8. Non-owned tables

Partner SKU lists, commission **rate cards**, OAuth tokens — stored under **platform/backoffice-backend** or partner integration DB; **not** duplicated here except as opaque refs (`affiliate_context_ref`, `spec_id`).
