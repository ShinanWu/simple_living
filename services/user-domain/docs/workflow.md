# user-domain — Core workflows

Sequence-level behavior for parallel implementation. Arrows are logical; transport is internal RPC unless noted.

## 1. Authenticated request path (gateway)

```text
Client → gateway [Authorization: Bearer]
    → user.IntrospectAccessToken
    → { valid, user_id, session_id, … }
    → gateway injects user context into downstream protos (content / recommendation / tracking / …)
```

**Failure**: `valid == false` or error `20002` → gateway maps to 401 per [error-codes.md](../../../docs/contracts/error-codes.md); optional `WWW-Authenticate` behavior product-specific.

**Guest**: If no bearer, gateway may call `EnsureGuestSession` once per device policy, then pass `session_id` with `user_id` empty.

---

## 2. Login / token issuance

```text
Client → gateway [credentials / OAuth code / SMS verify — out of scope here]
    → user.IssueTokenPair
    → gateway returns tokens + sets session cookie policy if any (product)
    → client stores access + refresh per contract
```

**Rotation**: If using refresh rotation, `IssueTokenPair` response includes new refresh; client must replace; `RevokeSession` on logout invalidates server-side session.

---

## 3. Personalization gating (before recommendation)

```text
gateway assembling recommendation context:
    → user.GetConsent (cached with short TTL)
    → if personalization_allowed:
          user.GetSignalBundleRef (or inline agreed summary)
      else:
          omit personalization signals; recommendation uses session-only or popular fallback
    → recommendation.QueryRecommendations(context with consent + ref)
```

**Invariant**: `recommendation-domain` does not fetch consent directly from user stores unless explicitly added to contract; **default** is gateway supplies flags in `RecommendationRequestContext`.

---

## 4. Favorites write + read for a screen

```text
User taps save on card:
    Client → gateway → user.AddFavorite(user_id, guide_card_id)
    → success

Favorites screen:
    Client → gateway → user.ListFavorites
    → gateway or client → content.BatchGetGuideCards(ids)
```

Ordering: `favorited_at` desc unless product specifies.

---

## 5. History capture

**Option A (client-driven)**:

```text
Client → gateway → user.RecordHistoryEvent(session_id|user_id, content_ref, occurred_at)
```

**Option B (tracking-driven)**:

```text
tracking pipeline → async consumer → user-domain aggregate upsert HistoryItem
```

Product picks one primary source to avoid double counting; document the choice in release notes.

---

## 6. Consent change propagation

```text
Client → gateway → user.UpdateConsent
    → invalidate GetConsent cache
    → bump signal bundle version or expire GetSignalBundleRef
    → subsequent recommendations see new flags
```

---

## 7. Account suspension or deletion

```text
Admin or compliance job → user.account_status update
    → IntrospectAccessToken returns invalid or restricted tier
    → gateway blocks protected routes
```

Data retention for history/feedback after deletion follows [compliance.md](../../../docs/compliance.md); implementation schedules purges.

---

## 8. Observability

- Correlate logs with `session_id`, `user_id` (hashed in untrusted sinks), and `request_id` from gateway.
- Metrics: token introspection QPS, consent read ratio, favorite/history write rates, `GetSignalBundleRef` hit rate.

---

## 9. Non-goals

- Cross-domain **2PC** transactions (e.g. favorite + content existence): use eventual consistency or sync validate with defined failure UX.
- Running recommendation or content pipelines inside user-domain.
