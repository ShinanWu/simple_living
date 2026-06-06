#!/usr/bin/env bash
# Roll back gateway container to the previous saved image tag (lab).
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"
# shellcheck disable=SC1091
source "${ROOT_DIR}/tools/lab_gateway_addrs.sh"

TARGET_HOST="${NODE_IP_NGINX:-${NODE_IP_GATEWAY}}"
TARGET_SSH_PORT="${NODE_SSH_PORT_NGINX:-${NODE_SSH_PORT_GATEWAY:-${SSH_PORT:-22}}}"
PREV_TAG="${PREV_IMAGE_TAG:-previous}"
PORT="${SERVICE_PORT_GATEWAY_HTTP:-8080}"

REMOTE_CMD="if sudo -n docker version >/dev/null 2>&1; then ctr='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then ctr='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then ctr=docker; else ctr=podman; fi; \
\$ctr rm -f simple-living-gateway >/dev/null 2>&1 || true; \
\$ctr run --pull=never -d --name simple-living-gateway --restart unless-stopped -p ${PORT}:${PORT} \
  -e GATEWAY_USER_SERVER_ADDR=${GATEWAY_USER_SERVER_ADDR} \
  -e GATEWAY_RECOMMENDATION_SERVER_ADDR=${GATEWAY_RECOMMENDATION_SERVER_ADDR} \
  -e GATEWAY_TRACKING_SERVER_ADDR=${GATEWAY_TRACKING_SERVER_ADDR} \
  -e GATEWAY_BACKOFFICE_BACKEND_ADDR=${GATEWAY_BACKOFFICE_BACKEND_ADDR} \
  -e EXPORT_DIR=/var/lib/simple-living/exports \
  localhost/gateway:${PREV_TAG} >/dev/null"

ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  -p "${TARGET_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${TARGET_HOST}" "${REMOTE_CMD}"
echo "Gateway rolled back to tag ${PREV_TAG} on ${TARGET_HOST}"
