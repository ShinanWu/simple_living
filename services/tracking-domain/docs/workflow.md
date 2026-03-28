# tracking-domain — Workflows

## 1. End-to-end: click → conversion

```text
┌─────────────┐   assemble      ┌──────────────────┐   consume URL    ┌─────────────────┐
│ Client UI   │ ──────────────► │ tracking-domain  │ ◄────────────────│ User opens /t/… │
│ (any term.) │ ◄────────────── │ links:assemble   │                 └────────┬────────┘
└─────────────┘  landing_url   └────────┬─────────┘                          │
                                          │                                    │ GET /t/{token}
                                          │ resolve affiliate_context          ▼
                                          ▼                          ┌──────────────────┐
                               ┌──────────────────┐                  │ tracking edge    │
                               │ affiliate-domain │ ◄── RPC/cache ──│ record Click     │
                               │ (partner URL)    │                  │ 302 partner URL  │
                               └──────────────────┘                  └──────────────────┘
                                                                          │
                                                                          ▼
                                                               (User completes order off-platform)
                                                                          │
                                                                          ▼
┌──────────────────┐   postback / poll   ┌──────────────────┐   ingest   ┌──────────────┐
│ Partner /        │ ───────────────────► │ affiliate-domain │ ─────────► │ tracking-    │
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
2. Service resolves context via **affiliate-domain**, builds **`TrackingLink`** + **`short_token`**, returns `landing_url`.
3. User navigates to **`GET /t/{short_token}`**; edge validates token, creates **`Click`** (idempotent), resolves partner URL, responds **`302`**.
4. Later, **conversion** arrives (postback through **affiliate-domain** worker or direct ingest). **tracking-domain** matches on `click_id` or attribution sub-ids, writes **`Conversion`**, updates aggregates.

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
| `partner_resolve_failed` | 503 or branded error page | affiliate-domain / partner resolution failed; click may still be recorded with `redirect_state_at_click=degraded` per policy. |
| `blocked_client` | 200 HTML explanation | Rare: client environment blocked outbound (mini-program policy); click may not fire — product may use `clicks:ack` if needed. |

**Note:** Exact HTTP mapping can be tuned with SEO/security; enum names are stable for logs.

---

## 3. Jump execution semantics

1. **Order of operations:** validate token → **persist click** (or no-op if duplicate) → resolve partner URL → respond.
2. **Idempotency:** duplicate GETs from prefetch must not create multiple billable clicks; edge/http implementations may derive `ResolveRedirectRequest.device_dedup_key` from `Idempotent-Key`, stable device hints, or equivalent cookie/session material where supported.
3. **Caching:** `Cache-Control: no-store` on jump responses.
4. **302 chains:** At most one hop through tracking; partner may append its own trackers (outside our control).

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

- **Estimated** commission may be computed at ingest from **affiliate-domain** rate snapshots referenced by `affiliate_context_ref`.
- **Reported** commission comes from partner statements ingested later; may **adjust** rows idempotently by `external_event_id`.

---

## 6. Non-goals

- Executing **payout** to users or partners.
- Storing **full** partner API responses indefinitely without redaction.
- **Guaranteeing** partner attribution accuracy (we guarantee **consistent handling** of declared events).

---

## 7. Open operational questions

Captured for product/backend alignment (see parent README summary).
