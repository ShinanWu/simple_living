# user-domain

## Purpose

The user domain is the **system of record** for **identity-linked state** that is not content, ranking, commerce, or governance policy: accounts, sessions and tokens (issuance lifecycle and introspection support for `gateway`), **profile and preferences**, **favorites**, **browsing history**, **feedback**, **consent flags** that gate personalization, and **opaque signal bundle references** consumed by `recommendation-domain`.

It enables parallel work by exposing stable **internal RPC / logical APIs** (`proto`); **client-facing JSON** and route shapes are owned by `gateway` and aligned with [docs/contracts/auth.md](../../../docs/contracts/auth.md) and shared envelopes.

## Boundaries

### In scope

| Area | Responsibility |
|------|----------------|
| **Account** | User registration identifiers, account status, login binding metadata (implementation-specific; contract stays logical). |
| **Session & tokens** | Issue/rotate/revoke access and refresh tokens; **introspection** for `gateway` (validity, `user_id`, roles, expiry). |
| **Profile** | Display name, avatar ref, locale, notification toggles (product-defined subset). |
| **Preferences** | Theme/category/taste selections used for product UX and as **inputs** to recommendation signals (not ranking logic). |
| **Favorites** | Saved `guide_card_id` (and optionally other content refs per contract) with timestamps. |
| **History** | Recent views / impressions summary **as stored user state** (not raw tracking logs—that remains `tracking-domain`). |
| **Feedback** | Structured feedback records (ratings, reasons) tied to user or session. |
| **Consent** | Personalization / analytics / marketing flags required by product and compliance; exposed to `gateway` and other domains as **flags only**. |
| **Signals for recommendation** | **References** to precomputed or cached signal bundles (`signal_bundle_ref`) and minimal **inline summaries** when contract requires—**no** ranking, recall, or model execution. |

### Out of scope (owned elsewhere)

| Concern | Owner |
|--------|--------|
| Recommendation recall, ranking, strategy, explanations | `recommendation-domain` |
| Canonical guide cards, editorial bodies, topics | `content-domain` |
| Affiliate APIs, commission params, channel rules | `affiliate-domain` |
| Click IDs, outbound jump URLs, attribution pipelines | `tracking-domain` |
| Moderation decisions, takedowns, commercial labeling at source | `governance-domain` |
| Public HTTP paths, JSON field names for clients | `gateway` + `docs/contracts/` |

## Dependencies

### Upstream (consumed)

- **Identity providers / OTP / OAuth adapters** (implementation detail; not modeled in public contracts).
- **Optional**: async jobs or feature pipelines that **write** materialized signals into stores this domain reads (still **owned** by user-domain’s data model for “what we expose to recommenders”).

### Downstream (consumers)

- **`gateway`**: auth, profile, favorites, history, feedback, consent; injects user context into downstream RPC.
- **`recommendation-domain`**: `consent` flags and `signal_bundle_ref` (and agreed summaries) inside `RecommendationRequestContext` per [recommendation-domain data-model](../../recommendation-domain/docs/data-model.md).

### Peer coordination

- **`tracking-domain`**: may emit events that **feed** history or feedback analytics; user-domain stores **user-visible** or **preference-relevant** aggregates per product, not the full raw event store.
- **`governance-domain`**: may influence **what** history/favorites can return (e.g. removed content IDs);默认由 `gateway` / 内容读取链路过滤不可见内容，`user-domain` 仅返回自身持有的用户状态与引用，不单独复制治理规则。

## Non-goals

- Storing full content payloads or editorial workflow.
- Computing recommendation scores or running ranking models.
- Generating affiliate or tracking URLs.

## Related documents

| Document | Contents |
|----------|----------|
| [development.md](./development.md) | 本服务实现与交付：构建、运行、测试、合并前检查与公共文档索引 |
| [../deploy/README.md](../deploy/README.md) | 单服务部署说明（QEMU，独立构建/分发/部署/回滚/验收） |
| [api.md](./api.md) | Internal logical RPC: auth introspection, profile, favorites, history, feedback, consent, signal bundle resolution |
| [data-model.md](./data-model.md) | Entities, enums, and cross-domain references |
| [pages.md](./pages.md) | Which screens consume user data via gateway |
| [workflow.md](./workflow.md) | Login, introspection, personalization gating, signal refresh |
| [changelog.md](./changelog.md) | Contract and documentation changes |
