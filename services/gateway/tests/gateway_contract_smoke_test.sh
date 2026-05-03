#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SRC_FILE="$ROOT_DIR/services/gateway/src/gateway_edge_server_main.cpp"
PROTO_FILE="$ROOT_DIR/services/gateway/proto/gateway_pages_edge.proto"

[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }
[[ -f "$PROTO_FILE" ]] || { echo "missing proto: $PROTO_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

src = Path("services/gateway/src/gateway_edge_server_main.cpp").read_text()
proto = Path("services/gateway/proto/gateway_pages_edge.proto").read_text()

for token in [
    "/api/v2/pages/home_feed",
    "/api/v2/pages/guide_detail",
    "/api/v2/pages/redirect_prepare",
    "QueryRecommendations",
    "BatchGetGuideCards",
    "AssembleTrackingLink",
]:
    if token not in src:
        raise SystemExit(f"missing token in gateway source: {token}")

for token in [
    "rpc GetHomeFeed",
    "rpc GetGuideDetail",
    "rpc PrepareRedirect",
]:
    if token not in proto:
        raise SystemExit(f"missing token in gateway proto: {token}")

# Envelope shape must preserve public JSON contract.
for token in [
    '\\"success\\":',
    '\\"code\\":',
    '\\"message\\":\\"',
    '\\"data\\":',
    '\\"meta\\":{\\"request_id\\":\\"',
    '\\"server_time_ms\\":',
]:
    if token not in src:
        raise SystemExit(f"missing JSON envelope token: {token}")

# Home feed mapping must keep recommendation/page fields wired.
for token in [
    'fi->set_recommendation_id',
    'fi->set_scene("home_feed")',
    'fi->set_rank',
    'fi->set_guide_card_id',
    'fi->add_reason_tags',
    'resp->mutable_pagination()->set_next_cursor',
]:
    if token not in src:
        raise SystemExit(f"missing home_feed mapping token: {token}")

# Guide detail must map content fields back to edge contract.
for token in [
    'detail->set_guide_card_id',
    'detail->set_title',
    'detail->set_subtitle',
    'detail->set_is_commercial',
    'detail->set_cover_url',
]:
    if token not in src:
        raise SystemExit(f"missing guide_detail mapping token: {token}")

# Redirect prepare must produce landing_url/click_id/attribution echo.
for token in [
    'resp->set_landing_url',
    'resp->set_click_id',
    'resp->mutable_attribution()->set_channel_code',
    'resp->mutable_attribution()->set_scene',
    'resp->mutable_attribution()->set_item_rank',
]:
    if token not in src:
        raise SystemExit(f"missing redirect mapping token: {token}")
PY

echo "gateway contract smoke test passed"
