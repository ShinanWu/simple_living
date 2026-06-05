# tracking-server — Client surfaces & integration

This document maps **where** tracking appears per terminal so UI and client agents can implement consistently. Business copy and compliance badges live in **content** / **governance** docs; here we only define **integration contracts**.

## 1. Shared principles

1. **Single entry:** User-facing outbound taps always go through **`landing_url`**（或平台等价入口，但仍需命中一次 tracking 跳转链路）。
2. **No secret parameters in clients:** Clients send **`affiliate_context_ref`** to `links:assemble`; never embed partner signing secrets.
3. **Attribution stability:** `placement` and `content_ref` must match server definitions used in analytics.
4. **Mini-program constraints:** If outbound rules require opening external browser or specific APIs, follow platform capability flags from **platform/backoffice-backend** responses (tracking consumes the outcome of assembly only).

## 2. Web (H5)

| Surface | User action | Client behavior |
|---------|-------------|-----------------|
| Guide card CTA | Click “去购买” / partner CTA | Call assemble API → `window.location` or `<a href>` to `landing_url`. |
| Article inline link | Tap partner link | Same; ensure `placement` distinguishes inline vs card. |
| Fallback | Assembly fails | Show error state; do not deep-link directly to partner domain without tracking. |

**Notes:** Use `rel="nofollow sponsored"` etc. per governance. Tracking domain sets redirect cookies only if allowed by cookie policy.

## 3. Native apps (iOS / Android)

| Surface | Behavior |
|---------|----------|
| Feed / detail CTA | `assemble` → prefer **universal link** / **app link** if `redirect_hint` provided; else open `landing_url` in **SFSafariView / Chrome Custom Tabs** (recommended) or system browser per product policy. |
| Push notification | Deep link may carry `short_token` path directly to `GET /t/...` (pre-assembled campaign links). |

**Optional:** `POST .../clicks:ack` after WebView `onPageStarted` if product requires stronger click guarantees.

## 4. Mini program

| Step | Behavior |
|------|----------|
| CTA tap | Call backend `assemble` via mini-program `request` API. |
| Open outbound | Use platform-supported jump (e.g. web-view to `landing_url` or official `openEmbeddedMiniProgram` / external link APIs per vendor rules). |
| Attribution | Same `placement` values as Web where possible; add `mini_program`-specific placements when needed. |

**Critical:** Some platforms restrict raw redirects; **tracking-server** remains authoritative that the **first network hit** to tracking is recorded — clients must not skip it.

## 5. Backend / batch agents

| Job | Integration |
|-----|-------------|
| Nightly reconciliation | Read `commissions` views; compare with **platform/backoffice-backend** exports. |
| Postback router | `POST .../conversions:ingest` from affiliate ingest workers. |

## 6. Observability hooks (client)

Clients should attach **correlation id** (from gateway) on `assemble` requests. Log locally: `link_ref`, `click_id` (if returned via `fmt=json` debug channel — production may omit).

## 7. Out of scope on this page

- Visual design of CTAs (`platform/backoffice-backend` / design system).
- Which partners appear on a card (`platform/backoffice-backend` affiliate 模块导出至 snapshot)。
