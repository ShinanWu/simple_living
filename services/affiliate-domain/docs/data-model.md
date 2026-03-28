# Affiliate domain — data model (conceptual)

Conceptual entities for **business clarity** and **contract stability**. Physical schemas (SQL, documents) belong in implementation; field names here are **suggested** for API and event design.

## 1. Core entities

### 1.1 Partner

Represents one **integration** with an external affiliate program.

| Attribute | Description |
|-----------|-------------|
| `partner_id` | Internal stable id (immutable). |
| `slug` | Human-readable unique key for URLs and configs. |
| `display_name` | Marketing / admin label. |
| `lifecycle_status` | `draft` \| `active` \| `sunset` \| `disabled`. |
| `platform_kind` | Normalized enum, e.g. `marketplace`, `short_video_commerce`, `other`—allows UI and policy without partner-specific forks. |

### 1.2 PartnerCapabilitySet

Snapshot of **what this partner supports**, versioned when partner APIs change.

| Attribute | Description |
|-----------|-------------|
| `capability_set_id` | Unique id. |
| `partner_id` | FK to Partner. |
| `effective_from` / `effective_to` | Business validity window. |
| `flags` | Set of normalized capabilities (see §2). |
| `constraints` | Structured link and reporting limits (lengths, charset, required params). |

### 1.3 CommissionRuleSet

**Our** interpretation of commercial terms for a partner, versioned and effective-dated.

| Attribute | Description |
|-----------|-------------|
| `rule_set_id` | Unique id. |
| `partner_id` | FK to Partner. |
| `version` | Monotonic per partner. |
| `effective_from` / `effective_to` | No overlap for published versions without override process. |
| `structure` | Tiered / flat / category matrix—details in attached business appendix per partner. |
| `approval` | Approver identity and timestamp (references external workflow). |

### 1.4 LinkGenerationInput

Validated **intent** to build a partner-compliant URL. May be persisted for audit or computed transiently—**business contract** is the shape and validation rules.

| Attribute | Description |
|-----------|-------------|
| `input_id` | Correlation id (optional if embedded in redirect request). |
| `partner_id` | Target partner. |
| `campaign_ref` | Opaque reference to product/campaign object owned elsewhere. |
| `product_refs` | Zero or more mapped offer or SKU ids per partner contract. |
| `sub_ids` | Normalized slots (e.g. `sub1`…`subn`) filled per capability. |
| `context` | Allowed metadata: geo, placement type, compliance mode—no PII beyond what policy allows. |

### 1.5 AffiliateLinkSpec

Output of link generation: **specification** for building the final URL (see [api.md](./api.md) §2.2).

| Attribute | Description |
|-----------|-------------|
| `spec_id` | Unique id for this generation. |
| `partner_id` | FK. |
| `expires_at` | Business expiry. |
| `template` | Base + parameter binding model. |
| `token_handles` | References to short-lived partner tokens if applicable. |

### 1.6 RawPartnerReportBatch

Inbound **raw** artifact before normalization.

| Attribute | Description |
|-----------|-------------|
| `batch_id` | Unique id. |
| `partner_id` | FK. |
| `channel` | `api` \| `webhook` \| `sftp` \| `manual_upload`. |
| `received_at` | Ingestion time. |
| `checksum` | For deduplication. |
| `payload_ref` | Storage reference (not inline in events). |

### 1.7 NormalizedCommissionEvent

Canonical row for **downstream** settlement and analytics.

| Attribute | Description |
|-----------|-------------|
| `event_id` | Idempotent key (see [api.md](./api.md) §2.3). |
| `partner_id` | FK. |
| `partner_order_id` | Partner-native id when present. |
| `status` | `pending` \| `confirmed` \| `reversed` \| `invalid`. |
| `amount`, `currency` | Normalized. |
| `occurred_at` | Partner-asserted or mapped. |
| `correlation` | Bag for sub-ids / click id / campaign ref—opaque to affiliate click logic. |
| `rule_set_id` | Which commission rules were applied for validation. |

## 2. Normalized capability flags (illustrative)

Partners expose different features; flags are **additive**.

| Flag | Meaning |
|------|---------|
| `DEEP_LINK` | Supports app deep link or universal link flow. |
| `SUB_ID_SLOTS` | Number of sub-id parameters supported. |
| `COUPON_ATTACHMENT` | Coupon can be bound in link or metadata. |
| `REAL_TIME_POSTBACK` | Webhook for order/commission notifications. |
| `BATCH_ORDER_REPORT` | Periodic downloadable or API report. |
| `SKU_LEVEL` | Reporting at SKU granularity. |

Exact enum is **versioned**; new partners only use published flags or register an extension process.

## 3. Identifiers and correlation

| Id | Scope | Owner |
|----|-------|-------|
| `partner_id` | Global internal | affiliate-domain |
| `event_id` | Global | affiliate-domain (generation rules documented per partner) |
| `click_id` / session ids | Global | **tracking-domain** (may appear inside `correlation`) |

**Rule**: Affiliate-domain **never** generates click ids; it **may store** them for reconciliation when partners echo them back.

## 4. Relationships (summary)

- Partner **1—N** PartnerCapabilitySet (versioned over time).
- Partner **1—N** CommissionRuleSet (non-overlapping effective periods when published).
- LinkGenerationInput **N—1** Partner; produces **1** AffiliateLinkSpec per successful validation.
- RawPartnerReportBatch **1—N** NormalizedCommissionEvent (after mapping and validation).

## 5. Retention (business policy placeholders)

- Raw batches: retained long enough for **audit and dispute**; exact duration is legal/ops decision.
- Normalized events: retained per **finance** policy; reversals must remain traceable to original event.
