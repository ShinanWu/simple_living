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

declare -a SERVICES=(
  "gateway"
  "user-domain"
  "content-domain"
  "recommendation-domain"
  "affiliate-domain"
  "tracking-domain"
  "governance-domain"
)

host_for_service() {
  case "$1" in
    gateway) echo "${NODE_IP_GATEWAY:-}" ;;
    user-domain) echo "${NODE_IP_USER:-}" ;;
    content-domain) echo "${NODE_IP_CONTENT:-}" ;;
    recommendation-domain) echo "${NODE_IP_RECOMMENDATION:-}" ;;
    affiliate-domain) echo "${NODE_IP_AFFILIATE:-}" ;;
    tracking-domain) echo "${NODE_IP_TRACKING:-}" ;;
    governance-domain) echo "${NODE_IP_GOVERNANCE:-}" ;;
    *) echo "" ;;
  esac
}

ssh_port_for_service() {
  case "$1" in
    gateway) echo "${NODE_SSH_PORT_GATEWAY:-${SSH_PORT}}" ;;
    user-domain) echo "${NODE_SSH_PORT_USER:-${SSH_PORT}}" ;;
    content-domain) echo "${NODE_SSH_PORT_CONTENT:-${SSH_PORT}}" ;;
    recommendation-domain) echo "${NODE_SSH_PORT_RECOMMENDATION:-${SSH_PORT}}" ;;
    affiliate-domain) echo "${NODE_SSH_PORT_AFFILIATE:-${SSH_PORT}}" ;;
    tracking-domain) echo "${NODE_SSH_PORT_TRACKING:-${SSH_PORT}}" ;;
    governance-domain) echo "${NODE_SSH_PORT_GOVERNANCE:-${SSH_PORT}}" ;;
    *) echo "${SSH_PORT}" ;;
  esac
}

declare -a FAILED=()

for service in "${SERVICES[@]}"; do
  host="$(host_for_service "${service}")"
  ssh_port="$(ssh_port_for_service "${service}")"
  if [[ -z "${host}" ]]; then
    echo "SKIP: missing node IP for service=${service}"
    continue
  fi

  container_name="simple-living-${service}"
  echo "==> Stop ${container_name} on ${host}"
  remote_cmd="if command -v docker >/dev/null 2>&1; then c=docker; elif command -v podman >/dev/null 2>&1; then c=podman; else echo 'missing docker/podman'; exit 2; fi; \$c rm -f '${container_name}' >/dev/null 2>&1 || true"
  if ssh -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${ssh_port}" -i "${SSH_KEY_PATH}" \
    "${SSH_USER}@${host}" \
    "${remote_cmd}"; then
    echo "OK: stopped ${container_name} on ${host}"
  else
    echo "FAIL: stop ${container_name} on ${host}"
    FAILED+=("${service}(${host})")
  fi
done

if [[ "${#FAILED[@]}" -gt 0 ]]; then
  echo
  echo "Stop failures:"
  printf ' - %s\n' "${FAILED[@]}"
  exit 2
fi

echo
echo "All service containers are stopped (or not present)."
