#!/usr/bin/env bash
# Regenerate generated stub servers (hand-edited user_domain_server_main.cpp is NOT overwritten).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
PY=tools/generate_brpc_stub_server.py
python3 "$PY" -s services/content-domain/proto/content_domain.proto:content_domain \
  -o services/content-domain/src/content_domain_server_main.cpp --port 8002 --comment content-domain
python3 "$PY" -s services/recommendation-domain/proto/recommendation_domain_service.proto:recommendation_domain_service \
  -o services/recommendation-domain/src/recommendation_domain_server_main.cpp --port 8003 --comment recommendation-domain
python3 "$PY" -s services/affiliate-domain/proto/affiliate_domain.proto:affiliate_domain \
  -o services/affiliate-domain/src/affiliate_domain_server_main.cpp --port 8004 --comment affiliate-domain
python3 "$PY" -s services/tracking-domain/proto/tracking_domain.proto:tracking_domain \
  -o services/tracking-domain/src/tracking_domain_server_main.cpp --port 8005 --comment tracking-domain
python3 "$PY" -s services/governance-domain/proto/governance_domain.proto:governance_domain \
  -o services/governance-domain/src/governance_domain_server_main.cpp --port 8006 --comment governance-domain
python3 "$PY" -s services/gateway/proto/gateway_user_edge.proto:gateway_user_edge \
  -s services/gateway/proto/gateway_pages_edge.proto:gateway_pages_edge \
  -o services/gateway/src/gateway_edge_server_main.cpp --port 8080 --comment gateway-edge
echo "Done. Edit services/user-domain/src/user_domain_server_main.cpp by hand if RPC set changes."
