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
: "${NODE_IP_BUILD:?NODE_IP_BUILD is required in env file}"
SSH_PORT="${SSH_PORT:-22}"
IMAGE_TAG="${IMAGE_TAG:-latest}"
QEMU_BUILD_WORKDIR="${QEMU_BUILD_WORKDIR:-/root/simple_living}"
BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT}}"

SERVICES=(
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

for service in "${SERVICES[@]}"; do
  target="$(host_for_service "${service}")"
  target_port="$(ssh_port_for_service "${service}")"
  if [[ -z "${target}" ]]; then
    echo "Skip ${service}: target host missing"
    continue
  fi
  if [[ "${target}" == "${NODE_IP_BUILD}" && "${target_port}" == "${BUILD_SSH_PORT}" ]]; then
    echo "Skip ${service}: already on build node ${NODE_IP_BUILD}"
    continue
  fi

  image="localhost/${service}:${IMAGE_TAG}"
  echo "Streaming image ${image} from ${NODE_IP_BUILD} -> ${target}"
  ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
    "if command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c save '${image}'" \
    | ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${target_port}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${target}" \
        "if command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c load"
done

echo "Image distribution done."
