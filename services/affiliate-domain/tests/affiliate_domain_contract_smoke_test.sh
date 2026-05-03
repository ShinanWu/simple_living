#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SRC_FILE="$ROOT_DIR/services/affiliate-domain/src/affiliate_domain_server_main.cpp"
PROTO_FILE="$ROOT_DIR/services/affiliate-domain/proto/affiliate_domain.proto"

[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }
[[ -f "$PROTO_FILE" ]] || { echo "missing proto: $PROTO_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

src = Path("services/affiliate-domain/src/affiliate_domain_server_main.cpp").read_text()
proto = Path("services/affiliate-domain/proto/affiliate_domain.proto").read_text()

for token in [
    "service AffiliatePartnerService",
    "service AffiliateCommissionRuleService",
    "service AffiliateIntakeService",
    "rpc ValidateLinkGenerationInput",
    "rpc RegisterReportBatch",
]:
    if token not in proto:
        raise SystemExit(f"missing token in affiliate-domain proto: {token}")

for token in [
    "class AffiliatePartnerServiceImpl",
    "class AffiliateCommissionRuleServiceImpl",
    "class AffiliateIntakeServiceImpl",
    "server.AddService(&g_partner",
    "server.AddService(&g_rule",
    "server.AddService(&g_intake",
    "server.RunUntilAskedToQuit()",
]:
    if token not in src:
        raise SystemExit(f"missing token in affiliate-domain source: {token}")

print("affiliate-domain contract smoke test passed")
PY
