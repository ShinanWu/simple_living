# tracking-domain — Data model

Logical model only; physical storage (MySQL / Redis / ClickHouse) is an implementation choice. Names are **canonical field names** for APIs and contracts.

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
| `affiliate_context_ref` | string | Opaque; maps to affiliate-domain context. |
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
| `source` | enum | `partner_postback` \| `affiliate_domain` \| `manual_adjustment`. |
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

**Rule:** Anything in `attribution_snapshot` must be **non-secret** (no API keys). Partner auth stays in **affiliate-domain**.

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

## 5. Retention & PII (technical)

- `ip_hash`, `user_agent`: TTL and hashing per governance/compliance docs.
- `raw_payload_ref`: encrypted at rest; access audited.

---

## 6. Non-owned tables

Partner SKU lists, commission **rate cards**, OAuth tokens — stored under **affiliate-domain** or partner integration DB; **not** duplicated here except as opaque refs.
