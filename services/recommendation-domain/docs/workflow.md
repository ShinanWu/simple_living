# recommendation-domain — Workflows (v1)

## 1. Recommendation request flow

End-to-end flow for the internal `QueryRecommendations` RPC (public HTTP/JSON routes are owned by `gateway`; `popular` skips optional user-signal steps).

```text
Client / BFF
    │
    ▼
Gateway ── auth, rate limit, attach request_id
    │
    ▼
recommendation-domain: validate scene + context
    │
    ├─► user-domain (if needed): resolve signal_bundle_ref / consent
    │
    ├─► Load Strategy (scene → strategy id/version, experiments)
    │
    ├─► Recall stage
    │       ├─ index / ANN / rules
    │       └─ optional content-domain batch metadata fetch (ids only)
    │
    ├─► Filter stage
    │       ├─ eligibility (type, locale, category)
    │       └─ governance flags (blocked, regional)
    │
    ├─► Rank stage
    │       └─ feature assembly → model / heuristic → ordered list
    │
    ├─► Post-process
    │       ├─ diversity, business caps
    │       └─ optional explanation generation (inline or deferred)
    │
    ▼
Response: strategy + items [+ pagination]
    │
    ▼
Client hydrates content via content-domain; tracking-domain URLs on impressions/clicks
```

### Sequence notes

1. **Fail-open policy**: If `user-domain` is slow, use cached signal bundle or degrade to session/popular per SLA (documented per scene).
2. **Fail-closed policy**: If `governance-domain` mandates hard block and cannot be verified, prefer empty list or editorial fallback per product.
3. **Id dependency**: Recall never assumes it holds the full content record—only ids and features needed for ranking.

---

## 2. Strategy update flow

How a new ranking strategy or pipeline version reaches production without breaking clients.

```text
Author (PM / algo owner)
    │
    ▼
Define strategy spec (config + model artifact ref + reason code set)
    │
    ▼
Staging: register Strategy { id, version+1 }
    │
    ├─► shadow traffic / offline eval (optional, tooling outside this doc)
    │
    ▼
Canary: map small % of scenes → new version via experiment_key
    │
    ▼
Monitor: latency, empty rate, CTR proxy, error budget
    │
    ▼
Full rollout: update default_strategy_id for scene OR set 100% experiment
    │
    ▼
Deprecation: old version read-only; explain API rejects very old versions after TTL
```

### Responsibilities

| Step | Owner |
|------|--------|
| Strategy JSON / DAG definition | recommendation-domain team |
| Model artifact registry & validation | recommendation-domain + MLOps |
| Scene → default strategy mapping | recommendation-domain config |
| Experiment bucketing key semantics | product + analytics contract |
| User-visible explanation copy review | product / compliance |

### Rollback

- Instant: flip scene mapping to previous `strategy.version` or prior experiment split.
- Clients must tolerate unknown `reason_codes` by hiding explanations gracefully.

---

## 3. Explain request flow (deferred)

1. Client receives list from `query` / `popular` with `strategy` metadata.
2. On expand or viewport exposure, client calls `explain` with subset of items.
3. Service validates strategy version, loads cached explanation features or recomputes lightweight template fill.
4. Returns parallel `explanations` array; mismatched version → `404` → client skips explain UI.

---

## 4. Observability

- Every response logs: `request_id`, `scene`, `strategy.id`, `strategy.version`, item count, latency breakdown (recall / rank).
- Join impressions via `trace_ref` + `placement` with `tracking-domain` pipelines (downstream).
