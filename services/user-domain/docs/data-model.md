# user-domain — Data model (conceptual, v1)

Logical types for parallel implementation. Physical tables (MySQL/Redis) and indexes are internal; **cross-service** semantics must stay compatible with [docs/contracts/auth.md](../../../docs/contracts/auth.md) and recommendation context in [recommendation-domain/data-model.md](../../recommendation-domain/docs/data-model.md).

## 1. User

| Field | Type | Description |
|-------|------|-------------|
| `user_id` | string | Stable internal id; never recycle |
| `account_status` | enum | `active`, `suspended`, `deleted_pending`, etc. |
| `created_at` | datetime | |
| `updated_at` | datetime | |
| `primary_locale` | string | BCP 47 or product convention |
| `deleted_at` | datetime \| null | Soft delete for compliance |

**Rule**: PII vault details (phone hash, OAuth subject) stay internal; other domains receive `user_id` and non-sensitive profile fields only.

---

## 2. Session

Binds **tokens**, **device**, and optional **guest** identity.

| Field | Description |
|-------|-------------|
| `session_id` | Server-issued id; used in analytics correlation |
| `user_id` | Set when authenticated |
| `guest` | boolean |
| `device_id` | From client header; not sole security factor |
| `client_platform` | `web`, `ios`, `android`, mini-program keys |
| `created_at` / `last_active_at` | |
| `revoked_at` | null if active |

---

## 3. Token metadata (logical)

Implementation may store hashed refresh tokens only.

| Field | Description |
|-------|-------------|
| `access_jti` | Unique id for revocation lists |
| `refresh_fingerprint` | Hash of refresh token |
| `expires_at` | |
| `scopes` | string[] |

---

## 4. Profile

Product-facing subset; not a full CRM.

| Field | Type | Notes |
|-------|------|------|
| `display_name` | string | Moderation policy may apply |
| `avatar_url` | string | CDN ref |
| `bio` | string | Optional |
| `notification_prefs` | object | Channel toggles |

---

## 5. Preferences

| Field | Description |
|-------|-------------|
| `preferences_version` | Monotonic for cache busting |
| `theme_interests` | string[] | e.g. `clothing`, `food` — **taxonomy keys** aligned with product |
| `content_filters` | object | e.g. hide categories |
| `default_sort` | string | UX default only; recommendation may ignore |

---

## 6. Favorite

| Field | Type | Description |
|-------|------|-------------|
| `favorite_id` | string | Internal row id |
| `user_id` | string | |
| `guide_card_id` | string | References content-domain id |
| `favorited_at` | datetime | |

**Extension**: Additional `content_ref` types require contract with `content-domain` and gateway.

---

## 7. HistoryItem

User-visible “recent” row; distinct from raw click stream.

| Field | Description |
|-------|-------------|
| `user_id` or `session_id` | Owner |
| `guide_card_id` | |
| `last_seen_at` | |
| `first_seen_at` | optional |
| `source_surface` | optional enum: `feed`, `search`, `detail` |

Retention policy is product/compliance driven (TTL job).

---

## 8. Feedback

| Field | Description |
|-------|-------------|
| `feedback_id` | |
| `user_id` / `session_id` | |
| `target_type` | enum |
| `target_id` | string |
| `rating` | optional number |
| `reason_codes` | string[] |
| `free_text` | optional; PII scrubbing in pipelines |
| `created_at` | |

---

## 9. Consent

| Field | Description |
|-------|-------------|
| `personalization_allowed` | boolean |
| `analytics_allowed` | boolean |
| `marketing_allowed` | boolean |
| `consent_version` | string — policy doc version |
| `updated_at` | |
| `jurisdiction` | optional; for regional defaults |

**Consumer rule**: `recommendation-domain` must treat `personalization_allowed == false` as **no personalization signals** (session-only or popular paths only).

---

## 10. SignalBundleRef

Opaque handle produced by user-domain (or sync’d feature jobs) for recommenders.

| Field | Description |
|-------|-------------|
| `signal_bundle_ref` | Opaque string; unguessable |
| `bundle_version` | For invalidation |
| `expires_at` | |
| `scopes` | Which scenes or features may consume |

**Content**: May point to Redis/MySQL/feature-store rows holding coarse interests, recent interaction summaries, optional embedding ids — **never** recommendation scores.

---

## 11. Enums (illustrative)

| Enum | Values |
|------|--------|
| `account_status` | `active`, `suspended`, `deleted_pending` |
| `feedback_target_type` | `guide_card`, `recommendation_result`, `app`, `other` |

---

## 12. Relationships to other domains

| This domain | Other domain | Relation |
|-------------|--------------|----------|
| `guide_card_id` | `content-domain` | Must exist for favorites/history UX; may lazy-check |
| `signal_bundle_ref` | `recommendation-domain` | Consumed in context; no write-back |
| Events | `tracking-domain` | Optional input to history aggregation |
