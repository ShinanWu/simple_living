#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

SSH_PORT="${SSH_PORT:-22}"
FAILED=()

check_node() {
  local label="$1"
  local host="$2"
  local port="$3"
  if [[ -z "${host}" ]]; then
    echo "SKIP: ${label} not set"
    return 0
  fi
  echo "==> checking ${label} (${host}:${port})"
  if ssh -o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
      -p "${port}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${host}" "echo ok" >/dev/null 2>&1; then
    echo "OK: ${label}"
  else
    echo "FAIL: ${label}"
    FAILED+=("${label}(${host}:${port})")
  fi
}

check_node "NODE_IP_BUILD" "${NODE_IP_BUILD:-}" "${NODE_SSH_PORT_BUILD:-${SSH_PORT}}"
check_node "NODE_IP_BACKOFFICE" "${NODE_IP_BACKOFFICE:-}" "${NODE_SSH_PORT_BACKOFFICE:-${SSH_PORT}}"

if ((${#FAILED[@]} > 0)); then
  echo "Failed nodes: ${FAILED[*]}"
  exit 1
fi
echo "All configured nodes reachable."
