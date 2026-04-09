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
REGISTRY="${REGISTRY:-}"
IMAGE_TAG="${IMAGE_TAG:-latest}"

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

container_port_for_service() {
  case "$1" in
    gateway) echo "8080" ;;
    user-domain) echo "9101" ;;
    content-domain) echo "9102" ;;
    recommendation-domain) echo "9103" ;;
    affiliate-domain) echo "9104" ;;
    tracking-domain) echo "9105" ;;
    governance-domain) echo "9106" ;;
    *) echo "" ;;
  esac
}

declare -a FAILED=()

for service in "${SERVICES[@]}"; do
  host="$(host_for_service "${service}")"
  ssh_port="$(ssh_port_for_service "${service}")"
  port="$(container_port_for_service "${service}")"

  if [[ -z "${host}" ]]; then
    echo "FAIL: missing node IP for service=${service}" >&2
    FAILED+=("${service}(missing-host)")
    continue
  fi

  image="localhost/${service}:${IMAGE_TAG}"
  if [[ -n "${REGISTRY}" ]]; then
    image="${REGISTRY}/${service}:${IMAGE_TAG}"
  fi

  container_name="simple-living-${service}"

  echo "==> Deploy ${service} to ${host} with image ${image}"
  gateway_env=""
  if [[ "${service}" == "gateway" ]]; then
    gateway_env="-e GATEWAY_USER_DOMAIN_ADDR=10.0.2.2:19101 -e GATEWAY_CONTENT_DOMAIN_ADDR=10.0.2.2:19102 -e GATEWAY_RECOMMENDATION_DOMAIN_ADDR=10.0.2.2:19103 -e GATEWAY_TRACKING_DOMAIN_ADDR=10.0.2.2:19105"
  fi
  remote_cmd="if [[ '${image}' == localhost/* ]]; then if command -v docker >/dev/null 2>&1; then c='docker'; elif command -v podman >/dev/null 2>&1; then c='podman'; elif sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; else echo 'missing docker/podman'; exit 2; fi; else if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c='docker'; elif command -v podman >/dev/null 2>&1; then c='podman'; else echo 'missing docker/podman'; exit 2; fi; fi; (\$c rm -f '${container_name}' >/dev/null 2>&1 || true) && \$c run -d --name '${container_name}' --restart unless-stopped -p '${port}:${port}' ${gateway_env} '${image}' >/dev/null && \$c start '${container_name}' >/dev/null 2>&1 && [ \"\$(\$c inspect -f '{{.State.Status}}' '${container_name}' 2>/dev/null || echo unknown)\" = \"running\" ]"
  if [[ -n "${REGISTRY}" ]]; then
    remote_cmd="if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c='docker'; elif command -v podman >/dev/null 2>&1; then c='podman'; else echo 'missing docker/podman'; exit 2; fi; \$c pull '${image}' && (\$c rm -f '${container_name}' >/dev/null 2>&1 || true) && \$c run -d --name '${container_name}' --restart unless-stopped -p '${port}:${port}' ${gateway_env} '${image}' >/dev/null && \$c start '${container_name}' >/dev/null 2>&1 && [ \"\$(\$c inspect -f '{{.State.Status}}' '${container_name}' 2>/dev/null || echo unknown)\" = \"running\" ]"
  fi
  if ssh -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${ssh_port}" -i "${SSH_KEY_PATH}" \
    "${SSH_USER}@${host}" \
    "${remote_cmd}"; then
    echo "OK: deployed ${service} on ${host}"
  else
    echo "FAIL: deploy ${service} on ${host}"
    FAILED+=("${service}(${host})")
  fi
done

if [[ "${#FAILED[@]}" -gt 0 ]]; then
  echo
  echo "Deploy failures:"
  printf ' - %s\n' "${FAILED[@]}"
  exit 2
fi

echo
echo "All services deployed."
