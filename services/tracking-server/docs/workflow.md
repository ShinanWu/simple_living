# tracking-server — Workflows

## 1. End-to-end: click → conversion

```text
┌─────────────┐   assemble      ┌──────────────────┐   consume URL    ┌─────────────────┐
│ Client UI   │ ──────────────► │ tracking-server  │ ◄────────────────│ User opens /t/… │
│ (any term.) │ ◄────────────── │ links:assemble   │                 └────────┬────────┘
└─────────────┘  landing_url   └────────┬─────────┘                          │
                                          │                                    │ GET /t/{token}
                                          │ resolve affiliate_context          ▼
                                          ▼                          ┌──────────────────┐
                               ┌──────────────────┐                  │ tracking edge    │
                               │ platform/backoffice-backend │ ◄── RPC/cache ──│ record Click     │
                               │ (partner URL)    │                  │ 302 partner URL  │
                               └──────────────────┘                  └──────────────────┘
                                                                          │
                                                                          ▼
                                                               (User completes order off-platform)
                                                                          │
                                                                          ▼
┌──────────────────┐   postback / poll   ┌──────────────────┐   ingest   ┌──────────────┐
│ Partner /        │ ───────────────────► │ platform/backoffice-backend │ ─────────► │ tracking-    │
│ Affiliate APIs   │                      │ (normalize)      │            │ domain       │
└──────────────────┘                      └──────────────────┘            │ conversions  │
                                                                            └──────┬───────┘
                                                                                   │
                                                                                   ▼
                                                                          Commission read models
                                                                          (views / aggregates)
```

**Happy path**

1. Client requests **`links:assemble`** with `content_ref`, `placement`, `affiliate_context_ref`.
2. Service resolves context via **platform/backoffice-backend** or receives the CMS `landing_url` bridge from gateway, builds **`TrackingLink`** + **`short_token`**, stores it in PostgreSQL, and returns `landing_url`.
3. User navigates to **`GET /t/{short_token}`**; edge validates token from PostgreSQL, creates **`Click`**, records an outbox event, resolves partner URL, responds **`302`**.
4. Later, **conversion** arrives (postback through **platform/backoffice-backend** worker or direct ingest). **tracking-server** matches on `click_id` or attribution sub-ids, writes **`Conversion`**, records an outbox event, and updates read models.

### 1.1 Redirect prepare chain (`AssembleTrackingLink`)

Tracking **owns `landing_url`**; partner-specific URL grammar/signing stays in `platform/backoffice-backend` (see [`../../platform/docs/backend-api.md`](../../platform/docs/backend-api.md) and api §11.1). One assemble call:

1. **Resolve context →** expand the opaque `affiliate_context_ref` into one canonical affiliate `LinkGenerationInput` (affiliate-owned resolver; tracking does not parse its bytes).
2. **Get spec →** call affiliate `ValidateLinkGenerationInput` (or a cached equivalent within partner TTL) to obtain `AffiliateLinkSpec`. RPC bounded by `-rpc_timeout_ms` / `-rpc_max_retry`; failure → `TRACKING_ERROR_CODE_AFFILIATE_SPEC_FAILED` (public `50002`).
3. **Validate placement →** request `placement` is source of truth; if the expanded context also carries `placement` and they differ → `TRACKING_ERROR_CODE_AFFILIATE_CONTEXT_INVALID` (public `10002`).
4. **Assemble `landing_url` + token →** tracking builds the HTTPS `landing_url` (`https://<tracking-host>/t/{short_token}`) and a tracking-owned signed token; **no partner secret** is embedded in the client-visible URL.
5. **Persist redirect context →** in one transaction write `tracking_link`, `tracking_short_token`, the non-secret `attribution_snapshot`, the materialized `spec_id`/`expires_at`, **and** a `tracking.link.created` row into `tracking_outbox` (see §8).
6. **Idempotency →** same `idempotency_key` within TTL returns the same `link_ref` / `landing_url` (no second link row).

### 1.2 `ResolveRedirect` — record click **before** returning partner URL

Order of operations is fixed so that no billable partner hop happens without a recorded click:

1. **Validate token** (`tracking_short_token`, Redis hot read then PostgreSQL authority): unknown → `invalid_token`; revoked → `revoked`; past `expires_at` → `expired` (public `50003` / disposition `GONE`).
2. **Persist click first** — insert `tracking_click` (and outbox `tracking.click.acked`) **in the same transaction**, then materialize `partner_url` from stored spec; only re-enter the affiliate path when stored spec material is expired/insufficient (api §11.1).
3. **Return** `302` to `partner_url` (or `JSON_BODY` for controlled clients). If partner materialization fails after the click is recorded, return a branded error while keeping the click with `redirect_state_at_click=degraded`.

This guarantees “click recorded ⇒ outbound attempted”, never the reverse.

**Unhappy paths**

- **Assembly failure:** client shows error; no `TrackingLink`.
- **Expired token:** `redirect_state=expired`; optional static “link expired” page.
- **Revoked content:** `redirect_state=revoked`.
- **Conversion without click:** store with `matched_confidence=unmatched` or infer per policy (documented explicitly in ops runbooks).

---

## 2. Redirect states

States describe **user-visible or edge-visible** outcomes for `GET /t/{token}` (and may be mirrored in analytics).

| State | HTTP | Meaning |
|-------|------|---------|
| `valid` | 302 to partner | Token OK, click recorded, partner URL resolved. |
| `expired` | 410 or 302 to branded expiry page | `expires_at` passed. |
| `revoked` | 410 / 404 | Content or campaign withdrawn. |
| `invalid_token` | 404 | Malformed or unknown token. |
| `rate_limited` | 429 | Edge throttle; client may retry with backoff. |
| `partner_resolve_failed` | 503 or branded error page | platform/backoffice-backend / partner resolution failed; click may still be recorded with `redirect_state_at_click=degraded` per policy. |
| `blocked_client` | 200 HTML explanation | Rare: client environment blocked outbound (mini-program policy); click may not fire — product may use `clicks:ack` if needed. |

**Note:** Exact HTTP mapping can be tuned with SEO/security; enum names are stable for logs.

---

## 3. Jump execution semantics

1. **Order of operations:** validate token → **persist click** (or no-op if duplicate) → resolve partner URL → respond.
2. **Idempotency / click dedup:** duplicate GETs from prefetch must not create multiple billable clicks; edge/http implementations may derive `ResolveRedirectRequest.device_dedup_key` from `Idempotent-Key`, stable device hints, or equivalent cookie/session material where supported. Dedup is enforced by the partial unique index `uq_click_dedup (short_token, dedup_key)` (data-model §5.1); a second insert collapses to the existing `click_id`.
3. **Short-token validation, expiry & replay protection:**
   - **Validation:** token integrity is checked via the signed claim (`sig`/`ver`) and confirmed against authoritative `tracking_short_token`; Redis is only a hot-read accelerator.
   - **Expiry:** `expires_at` past → `expired`; expired tokens never resolve a partner URL.
   - **Replay:** repeated resolves of the same token are **not** an error (a link may legitimately be opened twice), but they do **not** create extra clicks beyond the dedup policy; tokens are bound to a single `link_ref` and cannot be rebound. Token rotation bumps `ver` with a dual-validation window (data-model §3).
4. **Caching:** `Cache-Control: no-store` on jump responses.
5. **302 chains:** At most one hop through tracking; partner may append its own trackers (outside our control).

---

## 4. Attribution matching

| Priority | Match key | Confidence |
|----------|-----------|------------|
| 1 | `click_id` supplied by partner / pass-through | `exact` |
| 2 | `sub_id_*` equality to `attribution_snapshot` | `exact` or `inferred` |
| 3 | Fuzzy time + user + SKU (if ever used) | `inferred` — **off by default** |

Matching rules are versioned by implementation policy, but `matching_policy_version` is not part of the v1 public API / data-model contract unless added explicitly later.

---

## 5. Commission views

- **Estimated** commission may be computed at ingest from **platform/backoffice-backend** rate snapshots referenced by `affiliate_context_ref`.
- **Reported** commission comes from partner statements ingested later; may **adjust** rows idempotently by `external_event_id`.

---

## 6. Conversion ingest — idempotent intake

1. **Idempotency key:** a conversion fact is unique per `(source, external_event_id)` (data-model §5.1 `uq_conv_source_evt`). For partner order postbacks, `external_event_id` carries the partner `external_order_id`; the affiliate path uses `CommissionNormalizedEvent.event_id`.
2. **Replay:** a repeated event with the same key returns the existing `conversion_id` with `status=CONVERSION_INGEST_STATUS_DUPLICATE`; no second row, no double-count.
3. **Matching:** resolve `click_id` first (exact); else infer from attribution sub-ids per §4; else store as `unmatched`.
4. **Transactional outbox:** the `tracking_conversion` insert and the `tracking.conversion.ingested` outbox row commit in the **same** transaction (§8); the daily read model (`tracking_commission_daily`) is refreshed after commit.
5. **Reversals/adjustments** (`REVERSED`/`INVALID`) are **out of scope** for this v1 positive-conversion path (api §11.1); they are handled by a separate adjustment flow, not by re-ingesting through `IngestConversion`.

---

## 7. Non-goals

- Executing **payout** to users or partners.
- Storing **full** partner API responses indefinitely without redaction.
- **Guaranteeing** partner attribution accuracy (we guarantee **consistent handling** of declared events).

---

## 8. Domain events (Kafka outbox)

State changes are published via a **transactional outbox**: the business write and the `tracking_outbox` insert commit in the **same** PostgreSQL transaction; an independent relay (`services/foundation/kafka/scripts/outbox_relay.sh` or an in-service thread) polls unpublished rows, publishes to Kafka, then writes back `published_at`. **Delivery is at-least-once; every consumer MUST be idempotent on `event_id`.**

### 8.1 Topics and keys

| topic | trigger | event_key (partition) | primary consumers |
|-------|---------|-----------------------|-------------------|
| `tracking.link.created` | `AssembleTrackingLink` success | `link_ref` | analytics, ops monitoring, attribution backfill |
| `tracking.click.acked` | click persisted (`ResolveRedirect` / `AckClick`) | `click_id` | analytics, recommendation feedback, fraud signal sink |
| `tracking.conversion.ingested` | `IngestConversion` accepted (not `DUPLICATE`) | `conversion_id` | finance/commission analytics, ops dashboards, reconciliation |

Topic naming, partitions and retention follow `services/foundation/kafka/docs/README.md`; using the stable id as `event_key` keeps per-entity ordering within a partition.

### 8.2 Payload (JSON)

```json
{
  "event_id": "01HSZ3R7AD6M0N6F5J8AOUTBOX",
  "event_type": "tracking.conversion.ingested",
  "occurred_at": "2026-03-28T12:18:00Z",
  "conversion_id": "conv_01HSZ44D5J4P1M8H6X9V",
  "click_id": "click_01HSZ3Y8W7W5S5Y6R3B1",
  "source": "affiliate_server",
  "conversion_type": "order_paid",
  "channel_code": "pdd",
  "commission_minor": 1280,
  "currency": "CNY",
  "commission_estimate": false,
  "match_confidence": "exact"
}
```

- Always present: `event_id`, `event_type`, `occurred_at`, and the entity key (`link_ref` / `click_id` / `conversion_id`).
- `tracking.link.created` carries `link_ref`, `short_token`, `channel_code`, content anchors; `tracking.click.acked` carries `click_id`, `link_ref`, `short_token`, `occurred_at`, non-secret attribution.
- Compatibility: only additive optional fields; consumers ignore unknown fields. Breaking changes require a versioned topic (e.g. `tracking.conversion.ingested.v2`) and a `changelog.md` entry.
- Payloads carry **no partner secrets** — only the non-secret attribution subset.

---

## 9. Open operational questions

Captured for product/backend alignment (see parent README summary).
