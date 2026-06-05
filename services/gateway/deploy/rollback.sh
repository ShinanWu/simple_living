#!/usr/bin/env bash
# Roll back gateway container to the previous saved image tag (lab).
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

TARGET_HOST="${NODE_IP_GATEWAY}"
SSH_PORT="${SSH_PORT:-22}"
TARGET_SSH_PORT="${NODE_SSH_PORT_GATEWAY:-${SSH_PORT}}"
PREV_TAG="${PREV_IMAGE_TAG:-previous}"

REMOTE_CMD="if sudo -n docker version >/dev/null 2>&1; then ctr='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then ctr='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then ctr=docker; else ctr=podman; fi; \
\$ctr rm -f simple-living-gateway >/dev/null 2>&1 || true; \
\$ctr run --pull=never -d --name simple-living-gateway --restart unless-stopped -p 8080:8080 \
  -e GATEWAY_USER_SERVER_ADDR=10.0.2.2:19101 \
  -e GATEWAY_RECOMMENDATION_SERVER_ADDR=10.0.2.2:19103 \
  -e GATEWAY_TRACKING_SERVER_ADDR=10.0.2.2:19105 \
  -e GATEWAY_BACKOFFICE_BACKEND_ADDR=10.0.2.2:19110 \
  -e EXPORT_DIR=/var/lib/simple-living/exports \
  localhost/gateway:${PREV_TAG} >/dev/null"

ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  -p "${TARGET_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${TARGET_HOST}" "${REMOTE_CMD}"
echo "Gateway rolled back to tag ${PREV_TAG} on ${TARGET_HOST}"
