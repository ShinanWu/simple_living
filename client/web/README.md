# Web Scaffold (`client/web`)

Minimal React + TypeScript scaffold (source-only) aligned with `client/frontend-*.md`.

## Included

- App shell with bottom nav: `首页` / `我的`
- Home top tabs (`衣/食/住/行`) mapped to:
  - `clothing`
  - `food`
  - `housing`
  - `transport`
- Shared page state model: `loading` / `success` / `empty` / `error` / `offline`
- Placeholder pages:
  - `GuideDetail`
  - `RedirectPrepare`
  - `MeSummary`
- Gateway abstraction and typed contracts for:
  - `GET /api/v2/pages/home_feed`
  - `GET /api/v2/pages/guide_detail`
  - `POST /api/v2/pages/redirect_prepare`
  - `GET /api/v2/pages/me_summary`
- Fake repository with basic mock behaviors (success/empty/error/offline)

## Structure

- `src/App.tsx`: app shell + simple flow controller
- `src/components/`: shell and shared state view
- `src/pages/`: page-level placeholders
- `src/gateway/`: API types, HTTP client interface, fake repository
- `src/types/`: shared domain/UI types

## Run now

```bash
npm install
npm run dev
```

## Self-check

```bash
npm run typecheck
npm run test
npm run build
```

## Next integration steps

1. Switch from `FakeGatewayRepository` to `HttpGatewayApiClient`.
2. Add real styling and route transitions.
3. Add e2e checks for `首页 -> 详情 -> 跳转准备`.
