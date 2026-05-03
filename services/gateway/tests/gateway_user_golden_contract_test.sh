#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
DOC_FILE="$ROOT_DIR/services/gateway/docs/api.md"
EDGE_PROTO="$ROOT_DIR/services/gateway/proto/gateway_user_edge.proto"
HTTP_PROTO="$ROOT_DIR/services/gateway/proto/gateway_user_http_messages.proto"
SRC_FILE="$ROOT_DIR/services/gateway/src/gateway_edge_server_main.cpp"

[[ -f "$DOC_FILE" ]] || { echo "missing doc: $DOC_FILE"; exit 1; }
[[ -f "$EDGE_PROTO" ]] || { echo "missing proto: $EDGE_PROTO"; exit 1; }
[[ -f "$HTTP_PROTO" ]] || { echo "missing proto: $HTTP_PROTO"; exit 1; }
[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

doc = Path("services/gateway/docs/api.md").read_text()
edge = Path("services/gateway/proto/gateway_user_edge.proto").read_text()
http = Path("services/gateway/proto/gateway_user_http_messages.proto").read_text()
src = Path("services/gateway/src/gateway_edge_server_main.cpp").read_text()

# User-facing routes must be documented.
for token in [
    "/api/v2/guest/session",
    "/api/v2/auth/token/issue",
    "/api/v2/auth/token/refresh",
    "/api/v2/me/favorites",
    "/api/v2/me/history/events",
    "/api/v2/me/summary",
]:
    if token not in doc:
        raise SystemExit(f"missing documented user route: {token}")

# Restful implementation anchors must stay present in source.
for token in [
    "/api/v2/guest/session",
    "/api/v2/auth/token/issue",
    "/api/v2/auth/token/refresh",
    "/api/v2/me/favorites/add",
    "/api/v2/me/favorites/remove",
    "/api/v2/me/history/events",
    "/api/v2/me/summary/get",
]:
    if token not in src:
        raise SystemExit(f"missing implemented user route: {token}")

# Edge service rpc surface.
for token in [
    "rpc PostGuestSession",
    "rpc PostAuthTokenIssue",
    "rpc PostAuthTokenRefresh",
    "rpc DeleteAuthSession",
    "rpc GetMeSummary",
]:
    if token not in edge:
        raise SystemExit(f"missing user edge rpc: {token}")

# HTTP message fields aligned with docs.
for token in [
    "message GuestSessionHttpRequest",
    "device_id",
    "client_platform",
    "message TokenIssueHttpRequest",
    "account_proof",
    "device_fingerprint",
    "message RefreshTokenHttpRequest",
    "refresh_token",
    "message AddFavoriteHttpRequest",
    "guide_card_id",
    "message RecordHistoryEventHttpRequest",
    "content_ref",
]:
    if token not in http:
        raise SystemExit(f"missing user http proto token: {token}")

# Implementation anchors for auth/identity and me flows.
for token in [
    "ResolveActor",
    "IntrospectAccessToken",
    "IntrospectRefreshToken",
    "IssueTokenPair",
    "RevokeSession",
    "GetProfile",
    "UpdatePreferences",
    "AddFavorite",
    "RecordHistoryEvent",
    "GetMeSummary",
]:
    if token not in src:
        raise SystemExit(f"missing user mapping anchor in source: {token}")

# Public JSON validation and auth failure handling should stay wired.
for token in [
    'FinishBizError(outer, 10002, "Validation failed"',
    'FinishBizError(outer, 20001, "Unauthorized"',
    'FinishBizError(outer, 20003, "Refresh token invalid or expired"',
]:
    if token not in src:
        raise SystemExit(f"missing user error-handling anchor: {token}")

print("gateway user golden contract test passed")
PY
