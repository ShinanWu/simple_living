#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SRC_FILE="$ROOT_DIR/services/tracking-server/src/tracking_server_main.cpp"
PROTO_FILE="$ROOT_DIR/services/tracking-server/proto/tracking_server.proto"

[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }
[[ -f "$PROTO_FILE" ]] || { echo "missing proto: $PROTO_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

src = Path("services/tracking-server/src/tracking_server_main.cpp").read_text()
proto = Path("services/tracking-server/proto/tracking_server.proto").read_text()

for token in [
    "service TrackingLinkService",
    "service TrackingRedirectService",
    "service TrackingClickService",
    "service TrackingConversionService",
    "service TrackingCommissionViewService",
]:
    if token not in proto:
        raise SystemExit(f"missing token in tracking-server proto: {token}")

for token in [
    "class TrackingLinkServiceImpl",
    "class TrackingRedirectServiceImpl",
    "class TrackingClickServiceImpl",
    "class TrackingConversionServiceImpl",
    "class TrackingCommissionViewServiceImpl",
    "server.AddService(&g_link",
    "server.AddService(&g_redirect",
    "server.AddService(&g_click",
    "server.AddService(&g_conv",
    "server.AddService(&g_commission",
    "server.RunUntilAskedToQuit()",
]:
    if token not in src:
        raise SystemExit(f"missing token in tracking-server source: {token}")

print("tracking-server contract smoke test passed")
PY
