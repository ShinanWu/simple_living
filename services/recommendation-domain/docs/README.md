# recommendation-domain

## Purpose

The recommendation domain selects and orders **affiliate-guided content** (articles, lists, cards, topics) for a user or anonymous session. It owns **strategy**, **recall**, **ranking**, **explanations**, and delivery of **personalized** and **popular** recommendation surfaces.

It does **not** store canonical user profiles, author content, or execute commerce. It consumes **signals and identifiers** from `user-domain` and **publishable content metadata** from `content-domain`, then returns **ordered references** plus **explainable metadata** for clients and gateways to render.

## Boundaries

### In scope

- Recommendation **strategy** selection and versioning (which pipelines run for a scene).
- **Recall**: candidate generation from indexes, rules, and cross-domain fetches (within agreed contracts).
- **Ranking**: scoring, blending, diversity, and business constraints applied to candidates.
- **Explanations**: human-readable or structured “why shown” fields tied to policy (not raw model internals unless productized).
- **Personalized** recommendations (logged-in or pseudonymous session with allowed signals).
- **Popular / trending** recommendations (session-agnostic or lightly contextual).
- Observability hooks for recommendation quality and safety (logging correlation IDs, strategy IDs, outcome summaries).

### Out of scope (owned elsewhere)

| Concern | Owner |
|--------|--------|
| User account, credentials, PII vault, long-term preference storage | `user-domain` |
| Content authoring, editorial workflow, raw bodies, media assets | `content-domain` |
| Affiliate API integration, commission parameters, channel-specific rules | `affiliate-domain` |
| Click tracking, attribution, outbound jump URLs | `tracking-domain` |
| Moderation, takedowns, commercial labeling policy enforcement at source | `governance-domain` |
| On-platform checkout, wallet, or payment | **Non-goal (product)** |

## Dependencies

### Upstream (consumed)

- **`user-domain`**: identity/session resolution, consent flags, feature bundles allowed for ranking (e.g. coarse interests, recent interactions summary), optional embedding or taste vectors **as exported artifacts**—not authored here.
- **`content-domain`**: content IDs, types, themes, freshness, eligibility flags, structured facets used for filtering and features.

### Downstream (consumers)

- **`gateway`**: aggregates recommendation responses for mobile/web APIs.
- **Clients**: render cards using IDs returned here plus detail fetches from `content-domain`.

### Peer coordination

- **`governance-domain`**: hard exclusions (blocked IDs, regional rules) may be enforced via shared indexes, push updates, or synchronous checks—contract must stay explicit in `api.md` / `workflow.md`.
- **`tracking-domain`**: impression/click URLs are **not** generated here; this domain may attach opaque `trace` / `placement` keys for correlation only.

## Inputs and outputs

### Inputs (conceptual)

- **Who**: user id, device/session id, auth state.
- **Where**: page or API surface (`scene`), locale, channel, app version.
- **What**: requested slot count, content type filters, category/topic constraints from the caller.
- **Signals**: allowed personalization signals from `user-domain`; inventory/eligibility snapshots from `content-domain` or shared search indexes.

### Outputs (conceptual)

- **Ordered list** of **content references** (IDs + type + minimal display hints if contract allows).
- **Strategy metadata**: strategy id/version, pipeline name, experiment bucket (if any).
- **Per-item explanation** fields (where product requires): e.g. “Because you viewed …”, “Popular in …”.
- **Diagnostics** (internal or debug channel): recall sources, filter reasons—never a substitute for product explanations.

## Non-goals

- Operating a marketplace or processing payments on-platform.
- Owning the **source of truth** for user profiles or content bodies.
- Promising **real-time** freshness stronger than documented SLAs without explicit contracts.
- Returning **fully rendered** UI payloads; keep responses reference-oriented unless gateway contract explicitly requires denormalized snippets.

## Related documents

| Document | Contents |
|----------|----------|
| [development.md](./development.md) | Implementation and delivery: build, run, test, merge checklist, shared doc index |
| [api.md](./api.md) | Self-contained internal RPC/proto contract, request/response messages, enums, and errors |
| [data-model.md](./data-model.md) | Context, scene, strategy, results, explanations, ranking features |
| [pages.md](./pages.md) | How surfaces use recommendations |
| [workflow.md](./workflow.md) | Request lifecycle and strategy rollout |
| [changelog.md](./changelog.md) | Versioned documentation changes |
