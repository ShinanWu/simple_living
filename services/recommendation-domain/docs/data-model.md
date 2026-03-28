# recommendation-domain — Data model (conceptual, v1)

This document defines **logical** types for contracts and parallel implementation. Storage schemas (Redis, feature store, OLAP) are internal.

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
| `consent` | Flags from `user-domain`: e.g. personalization allowed, ad personalization |
| `signal_bundle_ref` | Opaque handle to pre-resolved signals (preferred) or inline summary per contract |

**Rule**: No raw PII beyond what gateway already normalized; sensitive fields pass by reference when possible.

---

## 2. Scene

A **scene** is a stable key mapping to **default strategy**, **recall sources**, **limits**, and **explanation policy**.

Examples (illustrative and aligned with `docs/contracts/recommendation.md`):

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

Strategies are **owned** by recommendation-domain; **content** of templates may reference copy keys maintained with product.

---

## 4. RecommendationItem (result card reference)

Minimal **reference** to content; hydration is `content-domain` responsibility unless gateway contract adds denormalized fields. Shared external shape should stay aligned with `docs/contracts/recommendation.md`.

| Field | Type | Description |
|-------|------|-------------|
| `guide_card_id` | string | Stable guide card identifier for cross-domain and client correlation |
| `content_ref` | object | Optional richer reference when the item points to non-card content such as topic or article |
| `score` | number | Optional final score for debugging (may be omitted in prod) |
| `rank` | integer | 1-based ranking position, aligned with contracts and attribution fields |
| `recall_sources` | string[] | Optional coarse tags: `cf`, `content_similar`, `editorial`, `popular` |
| `placement` | string | Slot id within scene (for analytics correlation) |
| `trace_ref` | string | Opaque token for impression joining with `tracking-domain` |

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
| **User / session** | Recency-weighted interactions, category affinity buckets, saved items count (from `user-domain` bundle) |
| **Content** | Freshness, quality score, theme match, language match (`content-domain`) |
| **Graph / similarity** | Embedding distance, co-click/co-save stats |
| **Popularity** | Global or segmented CTR, save rate, velocity |
| **Diversity / fairness** | Max per author, per category cap, duplicate near-neighbor suppression |
| **Governance** | Hard suppress flags, required commercial labels already on content record |

Feature computation **lives** in recommendation-domain pipelines; **sources** remain authoritative in peer domains.

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
