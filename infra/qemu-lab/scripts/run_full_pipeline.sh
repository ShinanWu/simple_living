#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

echo "[1/6] checking qemu nodes..."
bash "${SCRIPT_DIR}/check_nodes.sh"

echo "[2/6] building binaries and images on build node..."
bash "${SCRIPT_DIR}/build_on_qemu.sh"

echo "[3/6] distributing images to service nodes..."
bash "${SCRIPT_DIR}/distribute_images.sh"

echo "[4/6] deploying containers by service..."
bash "${SCRIPT_DIR}/deploy_by_service.sh"

echo "[5/6] ingress check..."
bash "${ROOT_DIR}/infra/ingress/scripts/check_ingress.sh" "${BASE_URL:-http://127.0.0.1:18080}"

echo "[6/6] gateway api smoke tests..."
BASE_URL="${BASE_URL:-http://127.0.0.1:18080}" python3 "${ROOT_DIR}/client/tests/gateway_api_smoke.py"

echo "Full pipeline done."
