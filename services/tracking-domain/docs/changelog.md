# tracking-domain — Changelog

All notable changes to **documentation** in this directory are listed here. Implementation releases should reference this doc in their release notes when behavior is tied to these specs.

## [Unreleased]

- Initial documentation scaffold: README, API, pages, data model, workflow.

## [0.1.0] — 2026-03-28

- **Added:** Service boundary vs `affiliate-domain` (config vs tracking/attribution).
- **Added:** REST sketch for `links:assemble`, public `GET /t/{token}`, `conversions:ingest`, commission read APIs.
- **Added:** Redirect state enum and click → conversion flow diagram (text).
- **Added:** Logical entities: `TrackingLink`, `Click`, `Conversion`, aggregates.
- **Added:** Non-goals for on-platform trading and partner secret ownership.
