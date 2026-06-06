#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
if [[ ! -f "${ENV_FILE}" ]]; then
  echo "ERROR: env file not found: ${ENV_FILE}" >&2
  exit 1
fi

# If the caller exported FRP_AUTH_TOKEN before running this script, keep it
# after sourcing nodes.env (so secrets can stay out of the env file).
_HAD_ENV_FRP_AUTH=0
if [[ "${FRP_AUTH_TOKEN+set}" == "set" ]]; then
  _HAD_ENV_FRP_AUTH=1
  _ENV_FRP_AUTH_TOKEN="${FRP_AUTH_TOKEN}"
fi

# shellcheck disable=SC1090
source "${ENV_FILE}"

if [[ "${_HAD_ENV_FRP_AUTH}" == "1" ]]; then
  FRP_AUTH_TOKEN="${_ENV_FRP_AUTH_TOKEN}"
fi
: "${SSH_USER:?SSH_USER is required}"
: "${SSH_KEY_PATH:?SSH_KEY_PATH is required}"
: "${NODE_IP_BUILD:?NODE_IP_BUILD is required}"
: "${NODE_IP_NGINX:?NODE_IP_NGINX is required}"

IMAGE_REPO="proxy"
IMAGE_TAG="${IMAGE_TAG:-latest}"
IMAGE_NAME="localhost/${IMAGE_REPO}:${IMAGE_TAG}"
SSH_PORT="${SSH_PORT:-22}"
BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT}}"
NGINX_SSH_PORT="${NODE_SSH_PORT_NGINX:-${SSH_PORT}}"
QEMU_BUILD_WORKDIR="${QEMU_BUILD_WORKDIR:-/home/ubuntu/simple_living}"

ENABLE_FRP="${ENABLE_FRP:-1}"
FRP_EMBEDDED="${FRP_EMBEDDED:-0}"
FRP_PROXY_TYPE="${FRP_PROXY_TYPE:-http}"
FRP_SERVER_ADDR="${FRP_SERVER_ADDR:-}"
FRP_SERVER_PORT="${FRP_SERVER_PORT:-7000}"
FRP_AUTH_TOKEN="${FRP_AUTH_TOKEN:-}"
FRP_CUSTOM_DOMAIN="${FRP_CUSTOM_DOMAIN:-}"
FRP_USER="${FRP_USER:-phase1-local-gateway}"
FRP_REMOTE_PORT="${FRP_REMOTE_PORT:-10080}"

DOCKER_PUBLISH_PORTS="-p 80:80"
if [[ "${FRP_EMBEDDED}" == "1" ]]; then
  DOCKER_PUBLISH_PORTS="-p 80:80 -p ${FRP_SERVER_PORT}:${FRP_SERVER_PORT} -p ${FRP_REMOTE_PORT}:${FRP_REMOTE_PORT}"
fi

SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

REMOTE_BUILD_CMD="cd '${QEMU_BUILD_WORKDIR}' && if command -v docker >/dev/null 2>&1; then ctr=docker; else ctr=podman; fi && \$ctr build --network=host --pull=false -f services/proxy/deploy/Dockerfile.proxy -t '${IMAGE_NAME}' ."
ssh "${SSH_OPTS[@]}" -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" "${REMOTE_BUILD_CMD}"

ssh "${SSH_OPTS[@]}" -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
  "if command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c save '${IMAGE_NAME}'" | \
ssh "${SSH_OPTS[@]}" -p "${NGINX_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_NGINX}" \
  "if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c load"

PROXY_GATEWAY_UPSTREAM_HOST="${PROXY_GATEWAY_UPSTREAM_HOST:-host.containers.internal}"
PROXY_BACKOFFICE_UPSTREAM_HOST="${PROXY_BACKOFFICE_UPSTREAM_HOST:-host.containers.internal}"

REMOTE_DEPLOY_CMD="if sudo -n docker version >/dev/null 2>&1; then ctr='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then ctr='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then ctr=docker; else ctr=podman; fi; (\$ctr rm -f simple-living-proxy >/dev/null 2>&1 || true); (\$ctr rm -f simple-living-nginx-proxy >/dev/null 2>&1 || true); \$ctr run --pull=never -d --name simple-living-proxy --restart unless-stopped ${DOCKER_PUBLISH_PORTS} -e ENABLE_FRP='${ENABLE_FRP}' -e FRP_EMBEDDED='${FRP_EMBEDDED}' -e FRP_PROXY_TYPE='${FRP_PROXY_TYPE}' -e FRP_SERVER_ADDR='${FRP_SERVER_ADDR}' -e FRP_SERVER_PORT='${FRP_SERVER_PORT}' -e FRP_AUTH_TOKEN='${FRP_AUTH_TOKEN}' -e FRP_CUSTOM_DOMAIN='${FRP_CUSTOM_DOMAIN}' -e FRP_USER='${FRP_USER}' -e FRP_REMOTE_PORT='${FRP_REMOTE_PORT}' -e PROXY_GATEWAY_UPSTREAM_HOST='${PROXY_GATEWAY_UPSTREAM_HOST}' -e PROXY_BACKOFFICE_UPSTREAM_HOST='${PROXY_BACKOFFICE_UPSTREAM_HOST}' '${IMAGE_NAME}' >/dev/null && [ \$(\$ctr inspect -f '{{.State.Status}}' simple-living-proxy 2>/dev/null || echo unknown) = running ]"
ssh "${SSH_OPTS[@]}" -p "${NGINX_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_NGINX}" "${REMOTE_DEPLOY_CMD}"

echo "Proxy image deployed on ${NODE_IP_NGINX} (container: simple-living-proxy)"
