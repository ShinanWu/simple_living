#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${TEST_SRCDIR:-}" ]]; then
  if [[ -d "${TEST_SRCDIR}/_main" ]]; then
    cd "${TEST_SRCDIR}/_main"
  else
    cd "${TEST_SRCDIR}"
  fi
else
  cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
fi

python3 - <<'PY'
from pathlib import Path

main_src = Path("services/platform/backoffice-backend/src/backoffice_backend_server_main.cpp").read_text()
export_src = Path("services/platform/backoffice-backend/src/export_publisher.cpp").read_text()
catalog_proto = Path("common/proto/catalog.proto").read_text()
content_service_proto = Path("common/proto/content_service.proto").read_text()

for token in [
    "RegisterContentModule",
    "RegisterGovernanceModule",
    "RegisterAffiliateModule",
    "SnapshotExportCoordinator",
    "export_dir",
]:
    if token not in main_src:
        raise SystemExit(f"missing token in backoffice main: {token}")

for token in [
    "PublishCatalog",
    "PublishVisibility",
    "PublishAffiliateSpec",
    "catalog.json",
    "visibility.json",
    "affiliate_spec.json",
]:
    if token not in export_src:
        raise SystemExit(f"missing token in export_publisher: {token}")

for token in [
    "message GuideCard",
    "enum ContentLifecycleStatus",
]:
    if token not in catalog_proto:
        raise SystemExit(f"missing token in catalog.proto: {token}")

for token in [
    "service ContentService",
    "rpc PublishRevision",
    "rpc UpsertGuideCard",
    'import "catalog.proto"',
]:
    if token not in content_service_proto:
        raise SystemExit(f"missing token in content_service.proto: {token}")

seeds = Path("services/platform/backoffice-backend/src/dev_content_seeds.h").read_text()
content_src = Path("services/platform/backoffice-backend/src/content_services.cpp").read_text()
for theme_id in ("theme_1", "theme_2", "theme_3", "theme_4"):
    if theme_id not in seeds:
        raise SystemExit(f"dev_content_seeds.h missing theme seed: {theme_id}")
for token in ("kPublishedGuides", "BuildGuideCardFromSeed"):
    if token not in content_src:
        raise SystemExit(f"content_services missing seed builder token: {token}")
for prefix in ("guide_clothing_tmall", "guide_food_tmall", "guide_housing_tmall", "guide_transport_tmall"):
    if prefix not in seeds:
        raise SystemExit(f"dev_content_seeds.h missing card prefix: {prefix}")

# theme_1 must have >=3 seed cards (delivery-spec §5)
if seeds.count("kThemeClothing,") < 3:
    raise SystemExit("dev_content_seeds.h must seed >=3 clothing (theme_1) cards")

store_src = Path("services/platform/backoffice-backend/src/pg_content_store.cpp").read_text()
# theme_ids must be normalized into the persisted proto, not only the DB column
if "set_theme_ids(i, NormalizeThemeId" not in store_src:
    raise SystemExit("UpsertGuideCardLocked must normalize proto theme_ids via NormalizeThemeId")
# MergeSeedBackfill must only backfill cover + landing (never selling_points/subtitle)
merge_start = store_src.find("void MergeSeedBackfill(")
merge_body = store_src[merge_start:store_src.find("\n}\n", merge_start)]
if "add_selling_points" in merge_body or "set_subtitle" in merge_body:
    raise SystemExit("MergeSeedBackfill must not backfill selling_points/subtitle (delivery-spec §5)")

print("backoffice-backend contract smoke ok")
PY
