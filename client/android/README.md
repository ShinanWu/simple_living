# Android Scaffold (Jetpack Compose)

This directory contains a lightweight Android scaffold for parallel frontend development based on `client/frontend-*.md`.

## Included

- App shell with bottom navigation: `首页` / `我的`
- `首页` top tabs: `衣` / `食` / `住` / `行` mapped to `clothing` / `food` / `housing` / `transport`
- Shared sealed UI state: `loading` / `success` / `empty` / `error` / `offline`
- Placeholder screens:
  - `GuideDetail`
  - `RedirectPrepare`
  - `MeSummary`
- Gateway API definitions (page-level endpoints):
  - `GET /api/v2/pages/home_feed`
  - `GET /api/v2/pages/guide_detail`
  - `POST /api/v2/pages/redirect_prepare`
  - `GET /api/v2/pages/me_summary`
- Fake repository implementation for local UI development

## Run

This directory now includes Gradle project files.

Use Android Studio (open `client/android`) or command line:

- `./gradlew :app:assembleDebug`
- `./gradlew :app:installDebug`

## Self-check

- `./gradlew :app:assembleDebug`
- `./gradlew :app:testDebugUnitTest`

## Notes

- Field names in API models use `snake_case` semantics from docs/contracts.
- Repository is fake; swap with real network layer under `client/android/app/src/main/java/com/simpleliving/android/data/gateway` when gateway integration starts.
