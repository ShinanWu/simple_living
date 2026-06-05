# tracking-server

## Purpose

`tracking-server` is the **core business domain** for measurable affiliate outcomes on a **pure content / guidance platform**: it turns editorial and recommendation surfaces into **trackable outbound journeys** and **attributable conversions**, without hosting commerce or on-platform trading.

It enables **web, native apps, and mini-program** clients plus **backend agents** to work in parallel from a single source of truth for redirect semantics, APIs, and data shapes.

## Ownership (in scope)

| Capability | Description |
|------------|-------------|
| **Redirect link assembly** | Build **client-consumable** tracking URLs or deep-link payloads that preserve attribution and route through the controlled jump path. |
| **Click tracking** | Persist **click events** with stable identifiers and correlation fields for downstream attribution. |
| **Jump execution semantics** | Define **redirect states**, resolution order, expiry, idempotency, and failure behavior for the hop from app/Web → tracking → partner. |
| **Attribution fields** | Own the **canonical attribution payload** (IDs, placement, campaign-like slots) carried on links and echoed on conversions. |
| **Conversion event intake** | Accept **postback / server-side** conversion notifications (and optional client-assisted signals where allowed), normalize and store them. |
| **Commission tracking views** | Expose **read models** for estimated or reported commission tied to clicks/conversions (not payout execution). |

## Coordination with `platform/backoffice-backend`

| Concern | Owner |
|---------|--------|
| Partner API credentials, capability flags, rate limits, SKU/listing eligibility | **platform/backoffice-backend** |
| Canonical partner-specific link templates after business validation | **platform/backoffice-backend** (or shared contract); **tracking-server** consumes outputs |
| **How** the user leaves the platform (tracking URL, token, state machine) | **tracking-server** |
| **What** happened after leave (click, conversion, commission view) | **tracking-server** |

`tracking-server` **must not** own external partner configuration stores; it calls or reads **stable interfaces** from `platform/backoffice-backend` (or gateway aggregates) for anything that is “partner capability config.”

## Relationship to cross-cutting contracts

Shared enums and field dictionaries for redirects and attribution are maintained under `.cursor/rules/shared-contracts.mdc`. This service doc remains authoritative for **tracking-server behavior, state machine, and internal APIs**; cross-domain field semantics follow the shared contracts.

## Service level objectives (SLO) & capacity assumptions

`tracking-server` sits on two latency-sensitive interactive paths (link prepare, redirect) and one asynchronous path (conversion ingest). Targets below are v1 commitments; PostgreSQL is the authority, Redis is hot-read cache for short tokens / idempotency, Kafka is the async event bus.

| Path / interface | Indicator | v1 target |
|------------------|-----------|-----------|
| `AssembleTrackingLink` (redirect prepare) | Availability | ≥ 99.9% |
| `AssembleTrackingLink` | Latency (no redirect-time affiliate refresh) | P50 ≤ 20ms，P99 ≤ 120ms |
| `AssembleTrackingLink` | Latency when affiliate `ValidateLinkGenerationInput` is invoked | P99 ≤ 300ms (bounded by `-rpc_timeout_ms`) |
| `ResolveRedirect` (click write + partner URL) | Availability | ≥ 99.95% (user-visible outbound hop) |
| `ResolveRedirect` | Latency (token hit cache, spec materialized from stored state) | P50 ≤ 15ms，P99 ≤ 80ms |
| `ResolveRedirect` | Sustained click write throughput | ≥ 500 clicks/s per instance (single primary write path) |
| `IngestConversion` (postback processing) | Availability | ≥ 99.9% |
| `IngestConversion` | Processing latency (consumer → committed `tracking_conversion` row) | P99 ≤ 500ms |
| `IngestConversion` | Conversion lag (partner `occurred_at` → ingested) | ≤ 5min steady state; bounded only by partner/affiliate delivery |
| Commission read views | Latency | P99 ≤ 150ms (served from daily aggregate read model) |

**Capacity assumptions (v1):** active tracking links ≤ 1M rolling 30 days; clicks ≤ 5M / day; conversions ≤ 100k / day; commission dashboard read QPS ≤ 50. Beyond these, scale the redirect read path with Redis token cache and read replicas first; the click/conversion write path stays single-writer-authoritative on PostgreSQL. Outbox relay backlog alert threshold and retention are in [data-model.md](./data-model.md#7-retention--pii-technical) and [development.md](./development.md).

## Document map

| File | Contents |
|------|----------|
| [development.md](./development.md) | Implementation and delivery: build, run, test, merge checklist, shared doc index |
| [../deploy/README.md](../deploy/README.md) | Single-service deployment guide (QEMU, build/distribute/deploy/rollback/verify) |
| [api.md](./api.md) | Self-contained internal RPC/proto contract, messages, enums, errors, and integration rules |
| [pages.md](./pages.md) | Client surfaces and integration points per terminal |
| [data-model.md](./data-model.md) | Entities, fields, indexes (logical) |
| [workflow.md](./workflow.md) | Redirect states and click → conversion E2E |
| [changelog.md](./changelog.md) | Document revisions |

## Non-goals

Also repeated in [workflow.md](./workflow.md#non-goals).

### Summary

- On-platform checkout, wallet, escrow, or order management.
- Owning **partner app keys**, OAuth, or **capability matrices** for TikTok / PDD / etc. (`platform/backoffice-backend`).
- Legal/compliance policy authoring (use `platform/backoffice-backend` + product/compliance docs); tracking implements **technical** retention and audit hooks only.
- Real-time fraud verdicts as a product feature (may log signals; dedicated risk service is out of scope here).
- Paying creators or merchants (payout rails).

## Glossary

| Term | Meaning |
|------|---------|
| **Landing URL** | HTTPS URL hosted under the tracking domain used as the outbound entry from clients; public JSON contracts use the field name `landing_url`. |
| **Jump** | Single user-visible navigation from client to partner (may include 302 chain). |
| **Click** | Immutable (or append-only) record that a user initiated a jump with a given attribution context. |
| **Conversion** | Partner-reported or verified outcome (e.g. paid order) linked to attribution. |
| **Attribution token** | Signed or opaque carrier embedded in the tracking URL carrying stable IDs and slots. |
