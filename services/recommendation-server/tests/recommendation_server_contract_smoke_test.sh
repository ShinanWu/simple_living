#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SRC_FILE="$ROOT_DIR/services/recommendation-server/src/recommendation_server_main.cpp"
PROTO_FILE="$ROOT_DIR/services/recommendation-server/proto/recommendation_server_service.proto"

[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }
[[ -f "$PROTO_FILE" ]] || { echo "missing proto: $PROTO_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

src = Path("services/recommendation-server/src/recommendation_server_main.cpp").read_text()
proto = Path("services/recommendation-server/proto/recommendation_server_service.proto").read_text()

for token in [
    "service RecommendationService",
    "rpc QueryRecommendations",
    "rpc GetPopularRecommendations",
    "rpc ExplainRecommendations",
    "rpc HealthCheck",
]:
    if token not in proto:
        raise SystemExit(f"missing token in recommendation-server proto: {token}")

for token in [
    "class RecommendationServiceImpl",
    "server.AddService(&g_svc",
    "server.Start",
    "server.RunUntilAskedToQuit()",
]:
    if token not in src:
        raise SystemExit(f"missing token in recommendation-server source: {token}")

print("recommendation-server contract smoke test passed")
PY
