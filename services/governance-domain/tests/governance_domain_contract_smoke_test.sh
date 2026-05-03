#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SRC_FILE="$ROOT_DIR/services/governance-domain/src/governance_domain_server_main.cpp"
PROTO_FILE="$ROOT_DIR/services/governance-domain/proto/governance_domain.proto"

[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }
[[ -f "$PROTO_FILE" ]] || { echo "missing proto: $PROTO_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

src = Path("services/governance-domain/src/governance_domain_server_main.cpp").read_text()
proto = Path("services/governance-domain/proto/governance_domain.proto").read_text()

for token in [
    "service GovernanceReviewService",
    "service GovernanceCooperationService",
    "service GovernanceVisibilityService",
    "service GovernanceOpsConfigService",
    "service GovernanceRiskService",
    "service GovernancePolicyService",
]:
    if token not in proto:
        raise SystemExit(f"missing token in governance-domain proto: {token}")

for token in [
    "class GovernanceReviewServiceImpl",
    "class GovernanceCooperationServiceImpl",
    "class GovernanceVisibilityServiceImpl",
    "class GovernanceOpsConfigServiceImpl",
    "class GovernanceRiskServiceImpl",
    "class GovernancePolicyServiceImpl",
    "server.AddService(&g_review",
    "server.AddService(&g_coop",
    "server.AddService(&g_vis",
    "server.AddService(&g_ops",
    "server.AddService(&g_risk",
    "server.AddService(&g_policy",
    "server.RunUntilAskedToQuit()",
]:
    if token not in src:
        raise SystemExit(f"missing token in governance-domain source: {token}")

print("governance-domain contract smoke test passed")
PY
