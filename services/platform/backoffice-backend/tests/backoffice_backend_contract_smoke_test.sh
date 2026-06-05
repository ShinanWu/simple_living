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

print("backoffice-backend contract smoke ok")
PY
