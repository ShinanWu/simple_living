#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
DOC_FILE="$ROOT_DIR/services/gateway/docs/api.md"
PROTO_FILE="$ROOT_DIR/services/gateway/proto/gateway_pages_edge.proto"
SRC_FILE="$ROOT_DIR/services/gateway/src/gateway_edge_server_main.cpp"

[[ -f "$DOC_FILE" ]] || { echo "missing doc: $DOC_FILE"; exit 1; }
[[ -f "$PROTO_FILE" ]] || { echo "missing proto: $PROTO_FILE"; exit 1; }
[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

doc = Path("services/gateway/docs/api.md").read_text()
proto = Path("services/gateway/proto/gateway_pages_edge.proto").read_text()
src = Path("services/gateway/src/gateway_edge_server_main.cpp").read_text()

# Page routes documented for gateway pages must stay present.
for token in [
    "/api/v2/pages/home_feed",
    "/api/v2/pages/guide_detail",
    "/api/v2/pages/redirect_prepare",
    "/api/v2/pages/me_summary",
]:
    if token not in doc:
        raise SystemExit(f"missing documented route: {token}")
    if token not in src:
        raise SystemExit(f"missing implemented route: {token}")

# Golden response fields from docs must exist in edge proto.
proto_expectations = {
    "HomeFeedResponse": ["items", "pagination"],
    "HomeFeedItem": ["recommendation_id", "scene", "rank", "guide_card_id", "guide_card", "reason_tags"],
    "GuideDetailResponse": ["guide_card", "disclosures", "related"],
    "GuideCardDetail": ["guide_card_id", "title", "subtitle", "summary", "cover_url", "theme", "is_commercial"],
    "RedirectPrepareResponse": ["landing_url", "click_id", "expires_at", "attribution"],
    "AttributionEcho": ["click_id", "channel_code", "recommendation_id", "scene", "item_rank"],
    "MeSummaryResponse": ["profile", "counts", "consent"],
    "ProfileBlock": ["user_id", "is_guest", "display_name", "avatar_url", "locale"],
    "CountsBlock": ["favorites_count", "history_count"],
    "ConsentBlock": ["personalization_allowed", "analytics_allowed", "marketing_allowed", "consent_version"],
}

for message_name, fields in proto_expectations.items():
    if f"message {message_name}" not in proto:
        raise SystemExit(f"missing message in proto: {message_name}")
    for field in fields:
        if field not in proto:
            raise SystemExit(f"missing proto field {field} for {message_name}")

# Source mapping anchors must match documented data flow.
for token in [
    "rec_stub_.QueryRecommendations",
    "content_stub_.BatchGetGuideCards",
    "tracking_stub_.AssembleTrackingLink",
    "resp->mutable_pagination()->set_has_more",
    "detail->set_cover_url",
    "resp->mutable_attribution()->set_channel_code",
    'resp->mutable_profile()->set_is_guest(true)',
    'resp->mutable_counts()->set_favorites_count(0)',
]:
    if token not in src:
        raise SystemExit(f"missing mapping anchor in source: {token}")

print("gateway pages golden contract test passed")
PY
