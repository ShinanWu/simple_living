#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
if [[ ! -f "${ENV_FILE}" ]]; then echo "ERROR: env file not found: ${ENV_FILE}" >&2; exit 1; fi
# shellcheck disable=SC1090
source "${ENV_FILE}"
: "${SSH_USER:?SSH_USER is required in env file}"
: "${SSH_KEY_PATH:?SSH_KEY_PATH is required in env file}"
SSH_PORT="${SSH_PORT:-22}"
FAILED=()
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

check_ssh() {
  local label="$1" host="$2" port="$3"
  if [[ -z "${host}" ]]; then
    echo "SKIP: ${label} not set"
    return 0
  fi
  echo "==> checking ${label} (${host}:${port})"
  local ok=0
  for _ in $(seq 1 12); do
    if ssh "${SSH_OPTS[@]}" -p "${port}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${host}" \
      "((command -v docker >/dev/null 2>&1) || (command -v podman >/dev/null 2>&1)) && command -v curl >/dev/null 2>&1"; then
      ok=1
      break
    fi
    sleep 5
  done
  if [[ "${ok}" -ne 1 ]]; then
    FAILED+=("${label}(${host}:${port})")
  fi
}

check_ssh "NODE_IP_BUILD" "${NODE_IP_BUILD:-}" "${NODE_SSH_PORT_BUILD:-${SSH_PORT}}"
check_ssh "NODE_IP_RECOMMENDATION" "${NODE_IP_RECOMMENDATION:-}" "${NODE_SSH_PORT_RECOMMENDATION:-${SSH_PORT}}"

if [[ "${#FAILED[@]}" -gt 0 ]]; then
  echo "Unavailable nodes:"
  printf " - %s\n" "${FAILED[@]}"
  exit 2
fi
echo "recommendation-server lab nodes are ready."
