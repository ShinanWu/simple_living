# tracking-domain

## Purpose

`tracking-domain` is the **core business domain** for measurable affiliate outcomes on a **pure content / guidance platform**: it turns editorial and recommendation surfaces into **trackable outbound journeys** and **attributable conversions**, without hosting commerce or on-platform trading.

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

## Coordination with `affiliate-domain`

| Concern | Owner |
|---------|--------|
| Partner API credentials, capability flags, rate limits, SKU/listing eligibility | **affiliate-domain** |
| Canonical partner-specific link templates after business validation | **affiliate-domain** (or shared contract); **tracking-domain** consumes outputs |
| **How** the user leaves the platform (tracking URL, token, state machine) | **tracking-domain** |
| **What** happened after leave (click, conversion, commission view) | **tracking-domain** |

`tracking-domain` **must not** own external partner configuration stores; it calls or reads **stable interfaces** from `affiliate-domain` (or gateway aggregates) for anything that is “partner capability config.”

## Relationship to cross-cutting contracts

Shared enums and field dictionaries for redirects and attribution are maintained under `docs/contracts/` (especially `redirect-attribution.md`). This service doc remains authoritative for **tracking-domain behavior, state machine, and internal APIs**; cross-domain field semantics follow the shared contracts.

## Document map

| File | Contents |
|------|----------|
| [api.md](./api.md) | Self-contained internal RPC/proto contract, messages, enums, errors, and integration rules |
| [pages.md](./pages.md) | Client surfaces and integration points per terminal |
| [data-model.md](./data-model.md) | Entities, fields, indexes (logical) |
| [workflow.md](./workflow.md) | Redirect states and click → conversion E2E |
| [changelog.md](./changelog.md) | Document revisions |

## Non-goals

Also repeated in [workflow.md](./workflow.md#non-goals).

### Summary

- On-platform checkout, wallet, escrow, or order management.
- Owning **partner app keys**, OAuth, or **capability matrices** for TikTok / PDD / etc. (`affiliate-domain`).
- Legal/compliance policy authoring (use `governance-domain` + product/compliance docs); tracking implements **technical** retention and audit hooks only.
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
