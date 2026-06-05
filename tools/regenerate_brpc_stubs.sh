#!/usr/bin/env bash
# Regenerate generated stub servers (hand-edited mains are NOT overwritten).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
PY=tools/generate_brpc_stub_server.py
python3 "$PY" -s common/proto/content_service.proto:content_server \
  -o services/platform/backoffice-backend/src/content_services.cpp --port 9110 --comment backoffice-backend-content
python3 "$PY" -s services/recommendation-server/proto/recommendation_server_service.proto:recommendation_server_service \
  -o services/recommendation-server/src/recommendation_server_main.cpp --port 9103 --comment recommendation-server
python3 "$PY" -s services/platform/backoffice-backend/proto/affiliate_server.proto:affiliate_server \
  -o services/platform/backoffice-backend/src/affiliate_services.cpp --port 9110 --comment backoffice-backend-affiliate
python3 "$PY" -s services/tracking-server/proto/tracking_server.proto:tracking_server \
  -o services/tracking-server/src/tracking_server_main.cpp --port 9105 --comment tracking-server
python3 "$PY" -s services/platform/backoffice-backend/proto/governance_server.proto:governance_server \
  -o services/platform/backoffice-backend/src/governance_services.cpp --port 9110 --comment backoffice-backend-governance
python3 "$PY" -s services/gateway/proto/gateway_user_edge.proto:gateway_user_edge \
  -s services/gateway/proto/gateway_pages_edge.proto:gateway_pages_edge \
  -o services/gateway/src/gateway_edge_server_main.cpp --port 8080 --comment gateway-edge
echo "Done."
