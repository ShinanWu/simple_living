#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QEMU_LAB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENV_FILE="${ENV_FILE:-${QEMU_LAB_DIR}/nodes.env}"

if [[ ! -f "${ENV_FILE}" ]]; then
  echo "ERROR: env file not found: ${ENV_FILE}" >&2
  echo "Hint: copy infra/qemu-lab/nodes.example.env to infra/qemu-lab/nodes.env and fill values." >&2
  exit 1
fi

# shellcheck disable=SC1090
source "${ENV_FILE}"

: "${SSH_USER:?SSH_USER is required in env file}"
: "${SSH_KEY_PATH:?SSH_KEY_PATH is required in env file}"
SSH_PORT="${SSH_PORT:-22}"

NODE_KEYS=(
  "NODE_IP_BUILD"
  "NODE_IP_GATEWAY"
  "NODE_IP_USER"
  "NODE_IP_CONTENT"
  "NODE_IP_RECOMMENDATION"
  "NODE_IP_AFFILIATE"
  "NODE_IP_TRACKING"
  "NODE_IP_GOVERNANCE"
  "NODE_IP_NGINX"
  "NODE_IP_FRPC"
)

get_ssh_port_for_key() {
  local key="$1"
  case "${key}" in
    NODE_IP_BUILD) echo "${NODE_SSH_PORT_BUILD:-${SSH_PORT}}" ;;
    NODE_IP_GATEWAY) echo "${NODE_SSH_PORT_GATEWAY:-${SSH_PORT}}" ;;
    NODE_IP_USER) echo "${NODE_SSH_PORT_USER:-${SSH_PORT}}" ;;
    NODE_IP_CONTENT) echo "${NODE_SSH_PORT_CONTENT:-${SSH_PORT}}" ;;
    NODE_IP_RECOMMENDATION) echo "${NODE_SSH_PORT_RECOMMENDATION:-${SSH_PORT}}" ;;
    NODE_IP_AFFILIATE) echo "${NODE_SSH_PORT_AFFILIATE:-${SSH_PORT}}" ;;
    NODE_IP_TRACKING) echo "${NODE_SSH_PORT_TRACKING:-${SSH_PORT}}" ;;
    NODE_IP_GOVERNANCE) echo "${NODE_SSH_PORT_GOVERNANCE:-${SSH_PORT}}" ;;
    NODE_IP_NGINX) echo "${NODE_SSH_PORT_NGINX:-${SSH_PORT}}" ;;
    NODE_IP_FRPC) echo "${NODE_SSH_PORT_FRPC:-${SSH_PORT}}" ;;
    *) echo "${SSH_PORT}" ;;
  esac
}

configured=0
FAILED=()
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)
for key in "${NODE_KEYS[@]}"; do
  host="${!key:-}"
  if [[ -z "${host}" ]]; then
    continue
  fi
  configured=$((configured + 1))
  port="$(get_ssh_port_for_key "${key}")"
  echo "==> Checking ${key} (${host}:${port})"
  ok=0
  for _ in $(seq 1 12); do
    if ssh "${SSH_OPTS[@]}" -p "${port}" -i "${SSH_KEY_PATH}" \
      "${SSH_USER}@${host}" \
      "((command -v docker >/dev/null 2>&1) || (command -v podman >/dev/null 2>&1)) && command -v systemctl >/dev/null 2>&1 && command -v curl >/dev/null 2>&1"; then
      ok=1
      break
    fi
    sleep 5
  done
  if [[ "${ok}" -eq 1 ]]; then
    echo "OK: ${key} (${host})"
  else
    echo "FAIL: ${key} (${host})"
    FAILED+=("${key}(${host}:${port})")
  fi
done

if [[ "${configured}" -eq 0 ]]; then
  echo "ERROR: no node IPs configured in ${ENV_FILE}" >&2
  exit 1
fi

if [[ "${#FAILED[@]}" -gt 0 ]]; then
  echo
  echo "Unavailable nodes:"
  printf ' - %s\n' "${FAILED[@]}"
  exit 2
fi

echo
echo "All nodes are ready."
