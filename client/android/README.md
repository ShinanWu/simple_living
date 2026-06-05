# Android Scaffold (Jetpack Compose)

This directory contains the Android client implementation based on `client/frontend-*.md` and the real gateway JSON contract.

## Included

- App shell with bottom navigation: `首页` / `我的`
- `首页` top tabs: `衣` / `食` / `住` / `行` mapped to `clothing` / `food` / `housing` / `transport`
- Shared sealed UI state: `loading` / `success` / `empty` / `error` / `offline`
- Page implementations:
  - `GuideDetail`
  - `RedirectPrepare`
  - `MeSummary`
- Gateway API definitions (page-level endpoints):
  - `GET /api/v2/pages/home_feed`
  - `GET /api/v2/pages/guide_detail`
  - `POST /api/v2/pages/redirect_prepare`
  - `GET /api/v2/pages/me_summary`
- Real gateway repository is the production runtime target; fake repository code is allowed only for unit tests and preview fixtures.

## Run

This directory now includes Gradle project files.

Use Android Studio (open `client/android`) or command line:

- `./gradlew :app:assembleDebug`
- `./gradlew :app:installDebug`

## Self-check

- `./gradlew :app:assembleDebug`
- `./gradlew :app:testDebugUnitTest`

## Notes

- Field names in API models use `snake_case` semantics from `.cursor/rules/shared-contracts.mdc`.
- Runtime repository must call gateway under `client/android/app/src/main/java/com/simpleliving/android/data/gateway`; fake data must stay inside tests/previews and cannot be used for acceptance.
