#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

proto = Path("services/gateway/proto/gateway_pages_edge.proto").read_text()
src = Path("services/gateway/src/gateway_edge_server_main.cpp").read_text()

for msg in [
    "message BackofficePartner",
    "message BackofficeContentItem",
    "message BackofficeReviewItem",
    "message BackofficePartnersResponse",
    "message BackofficeContentItemsResponse",
    "message BackofficeReviewsResponse",
]:
    if msg not in proto:
        raise SystemExit(f"missing proto message: {msg}")

for rpc in [
    "rpc GetBackofficeAffiliatePartners",
    "rpc PostBackofficeAffiliatePartner",
    "rpc GetBackofficeContentItems",
    "rpc PatchBackofficeContentItemStatus",
    "rpc GetBackofficeGovernanceReviews",
    "rpc PatchBackofficeGovernanceReviewStatus",
]:
    if rpc not in proto:
        raise SystemExit(f"missing rpc: {rpc}")

for route in [
    "/api/v2/backoffice/affiliate/partners => GetBackofficeAffiliatePartners",
    "/api/v2/backoffice/affiliate/partners/add => PostBackofficeAffiliatePartner",
    "/api/v2/backoffice/content/items => GetBackofficeContentItems",
    "/api/v2/backoffice/content/items/status => PatchBackofficeContentItemStatus",
    "/api/v2/backoffice/governance/reviews => GetBackofficeGovernanceReviews",
    "/api/v2/backoffice/governance/reviews/status => PatchBackofficeGovernanceReviewStatus",
]:
    if route not in src:
        raise SystemExit(f"missing route mapping: {route}")

for anchor in [
    "MutableBackofficeState()",
    "state.partners.insert(state.partners.begin(), created)",
    "item.set_status(req->status())",
]:
    if anchor not in src:
        raise SystemExit(f"missing implementation anchor: {anchor}")

print("gateway backoffice contract smoke test passed")
PY
