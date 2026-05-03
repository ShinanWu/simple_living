#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SRC_FILE="$ROOT_DIR/services/content-domain/src/content_domain_server_main.cpp"
PROTO_FILE="$ROOT_DIR/services/content-domain/proto/content_domain.proto"

[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }
[[ -f "$PROTO_FILE" ]] || { echo "missing proto: $PROTO_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

src = Path("services/content-domain/src/content_domain_server_main.cpp").read_text()
proto = Path("services/content-domain/proto/content_domain.proto").read_text()

for token in [
    "service ContentService",
    "rpc ListThemes",
    "rpc BatchGetGuideCards",
    "rpc UpsertGuideCard",
    "rpc PublishRevision",
]:
    if token not in proto:
        raise SystemExit(f"missing token in content-domain proto: {token}")

for token in [
    "class ContentServiceImpl",
    "server.AddService(&g_svc",
    "server.Start",
    "server.RunUntilAskedToQuit()",
]:
    if token not in src:
        raise SystemExit(f"missing token in content-domain source: {token}")

print("content-domain contract smoke test passed")
PY
