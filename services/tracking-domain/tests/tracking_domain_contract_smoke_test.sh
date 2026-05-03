#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SRC_FILE="$ROOT_DIR/services/tracking-domain/src/tracking_domain_server_main.cpp"
PROTO_FILE="$ROOT_DIR/services/tracking-domain/proto/tracking_domain.proto"

[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }
[[ -f "$PROTO_FILE" ]] || { echo "missing proto: $PROTO_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

src = Path("services/tracking-domain/src/tracking_domain_server_main.cpp").read_text()
proto = Path("services/tracking-domain/proto/tracking_domain.proto").read_text()

for token in [
    "service TrackingLinkService",
    "service TrackingRedirectService",
    "service TrackingClickService",
    "service TrackingConversionService",
    "service TrackingCommissionViewService",
]:
    if token not in proto:
        raise SystemExit(f"missing token in tracking-domain proto: {token}")

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
        raise SystemExit(f"missing token in tracking-domain source: {token}")

print("tracking-domain contract smoke test passed")
PY
