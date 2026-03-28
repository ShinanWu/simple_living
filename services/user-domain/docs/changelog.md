# user-domain — Changelog

Documentation and logical RPC contract changes for this service. Implementation releases should reference an entry here when behavior visible to `gateway` or peer domains changes.

## Format

Each entry: **date (UTC)**, **version tag** (doc semver or `draft`), **summary**, **compatibility**.

---

## [0.1.0] — 2026-03-28

- **Added**: Initial service docs (`README`, `api`, `data-model`, `pages`, `workflow`, `changelog`).
- **Scope**: Defines ownership for account/session, token introspection for gateway, profile/preferences, favorites, history, feedback, consent flags, and signal bundle references for `recommendation-domain`.
- **Compatibility**: N/A (greenfield documentation).

---

## Pending / reserved

- Error code fine-graining inside `20xxx` for user-specific cases (document when assigned).
- Whether `ResolveSignalBundle` is exposed to `recommendation-domain` or signals stay gateway-only.
- History ingestion: single source (client vs tracking pipeline) as a locked decision.
