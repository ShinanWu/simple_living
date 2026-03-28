# Affiliate domain — pages & configuration surfaces

This document lists **operator-facing and internal configuration surfaces** owned or primarily consumed by **affiliate-domain**. It explicitly excludes **end-user redirect pages**, short-link landing UX, and click dashboards owned by **tracking-domain**.

## 1. Design principles

1. **Clarity over flexibility**: Operators see partner-native fields only behind a normalized label; dangerous combinations are blocked by capability rules.
2. **Auditability**: Every change to commission rules, partner credentials policy, or intake mapping has **who / when / why**.
3. **Parallel safe**: Multiple squads can work on different partners without sharing the same configuration namespace keys.

## 2. Page inventory

### 2.1 Partner registry

**Audience**: Internal admin, partnerships.

**Purpose**: Register and lifecycle-manage integrated partners.

| Section | Content |
|---------|---------|
| Identity | Display name, internal `partner_id`, external account ids (reference only). |
| Status | `draft`, `active`, `sunset`, `disabled`. |
| Environments | Sandbox vs production credentials **references** (not secret values in UI). |
| Contacts | Escalation for API outages and settlement disputes. |

### 2.2 Capability matrix (read-heavy)

**Audience**: Product, engineering, operations.

**Purpose**: Single screen answering “Can we do X on partner P?”

Displays normalized capabilities from [api.md](./api.md) §2.1 with partner-specific footnotes where normalization leaks.

### 2.3 Commission rules

**Audience**: Finance-aligned admin, partnerships.

**Purpose**: Maintain **versioned** commission interpretation per partner.

| UI element | Behavior |
|------------|----------|
| Effective range | Start/end datetime; no gaps for active partners without explicit approval. |
| Rate structure | Flat, tiered, category-based—expressed in business terms; technical encoding is implementation. |
| Exclusions | Categories, SKUs, or traffic types excluded from commission. |
| Evidence | Link to contract doc or ticket. |

### 2.4 Link policy templates

**Audience**: Product, growth.

**Purpose**: Define allowed **link generation inputs** for campaigns (which sub-ids, which product mappings, which disclosure mode).

Must align with **tracking-domain** campaign objects: this surface defines **affiliate constraints** that those objects must satisfy.

### 2.5 Report & callback intake configuration

**Audience**: Engineering operations, finance.

**Purpose**: Configure how raw partner data enters the system.

| Section | Content |
|---------|---------|
| Channel | Webhook URL registration status, SFTP path, API poll schedule (business-level). |
| Field mapping | Partner column → normalized field; required vs optional. |
| Deduplication key | Partner natural keys for orders or commissions. |
| Failure handling | Retry policy **description**; quarantine thresholds. |

### 2.6 Reconciliation & exceptions queue

**Audience**: Operations, finance.

**Purpose**: Human workflow for rows that fail validation, mapping, or rule application.

States (illustrative): `new`, `investigating`, `adjusted`, `rejected`, `released`.

**Note**: This is **not** click debugging; click issues live in tracking-domain.

## 3. Permissions (business-level)

| Role | Typical access |
|------|----------------|
| Partner admin | Partner registry, credentials policy, intake mapping. |
| Commercial | Commission rules (submit), read capabilities. |
| Finance | Commission rules (approve), reconciliation queue. |
| Product | Link policy templates, read-only capabilities. |

Exact RBAC is an implementation concern; **separation of “edit rule” vs “approve rule”** is a domain requirement.

## 4. Non-goals

- **Public** partner dashboards for merchants (unless product assigns them elsewhere).
- **Real-time** click maps or funnel analytics (tracking-domain).
- **Creative** asset management for ads (out of scope unless later unified under content domain).
