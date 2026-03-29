# Affiliate domain

## Purpose

The **affiliate domain** defines how the platform integrates with **external affiliate programs** (e.g. Pinduoduo, Douyin, and future partners). It is responsible for **partner capability abstraction**, **commission rule configuration**, **inputs required to generate partner-compliant affiliate links**, and **rules for ingesting partner callbacks and raw commission or performance reports**.

Revenue attribution depends on correct partner contracts, link formats, and settlement data—not on how clicks are recorded in our systems.

## Scope (owns)

| Area | Responsibility |
|------|----------------|
| Partner integrations | Contractual and technical surfaces per partner: auth, APIs, link specs, reporting formats, idempotency expectations. |
| Capability abstraction | Normalized view of what a partner can do (deep link, coupon, campaign id, product id mapping, reporting latency, etc.). |
| Commission rules | Business rules for how we interpret partner terms: rates, tiers, exclusions, validity windows, currency, and dispute-relevant metadata. |
| Link generation inputs | Validated **inputs** (campaign, product, user segment flags allowed by policy, sub-ids) that downstream systems use to request or build partner URLs—without owning redirect or click execution. |
| Callback & raw report intake | **Rules** for accepting webhooks, file drops, or API pulls: schemas, authentication, deduplication keys, reconciliation triggers, and escalation when data is incomplete. |

## Out of scope (does not own)

| Area | Owning domain (reference) |
|------|-------------------------|
| Click tracking execution, session correlation, attribution stitching to internal analytics | **tracking-domain** |
| User-facing redirect pages, short links as a product surface, “go” landing UX | **tracking-domain** (or a dedicated routing surface aligned with it) |
| Merchant catalog or editorial content | Product / content domains (as defined in architecture) |
| Payout execution to end users | Finance / operations (affiliate-domain supplies **reconciled** commission signals as inputs) |

## Consumers

- **tracking-domain**: Consumes link-generation **contracts** and partner identifiers; performs redirects and records clicks per platform policy.
- **Operations / finance**: Consumes normalized commission events and exception queues for settlement.
- **Product**: Configures campaigns and reads capability matrices (“what we can do per partner”).

## Principles

1. **Partner as a plug-in**: New partners extend the same capability and intake models; avoid one-off fields leaking across the whole system.
2. **Contracts before code**: API shapes, enums, and report field mappings are versioned and documented here before implementation diverges.
3. **Separation of intent vs execution**: Affiliate-domain specifies **what** must appear on a link and **what** constitutes a valid report row; it does not own **when** a user is redirected or how clicks are stored.

## Document map

| File | Contents |
|------|----------|
| [development.md](./development.md) | Implementation and delivery: build, run, test, merge checklist, shared doc index |
| [api.md](./api.md) | Self-contained internal RPC/proto contract, partner integration messages, enums, and errors |
| [pages.md](./pages.md) | Configuration and admin surfaces (not end-user redirect pages). |
| [data-model.md](./data-model.md) | Core entities, identifiers, and partner-normalized structures. |
| [workflow.md](./workflow.md) | Lifecycles: onboarding a partner, publishing a campaign link spec, reconciling reports. |
| [changelog.md](./changelog.md) | Domain-level contract and policy changes. |

## Glossary

- **Partner**: External affiliate network or commerce platform that pays commission on qualified actions.
- **Capability**: A boolean or enumerated ability of a partner (e.g. supports `sub_id`, provides daily order report).
- **Commission rule**: Our interpretation of partner commercial terms, used to explain or validate incoming settlement data—not the legal contract text itself.
- **Link generation input**: Validated payload used to produce a partner-specific affiliate URL or deep link; creation of the final URL may live in this domain or in a thin adapter, but **serving** it is not.
