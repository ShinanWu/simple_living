#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

SERVICE_NAME="backoffice-web"
IMAGE_REPO="simple-living-backoffice-web"
IMAGE_TAG="${IMAGE_TAG:-latest}"
TARGET_HOST="${NODE_IP_BACKOFFICE_WEB:-${NODE_IP_NGINX}}"
SSH_PORT="${SSH_PORT:-22}"
TARGET_SSH_PORT="${NODE_SSH_PORT_BACKOFFICE_WEB:-${NODE_SSH_PORT_NGINX:-${SSH_PORT}}}"
BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT}}"
QEMU_BUILD_WORKDIR="${QEMU_BUILD_WORKDIR:-/home/ubuntu/simple_living}"
IMAGE_NAME="localhost/${IMAGE_REPO}:${IMAGE_TAG}"

REMOTE_BUILD_CMD="cd '${QEMU_BUILD_WORKDIR}' && if command -v docker >/dev/null 2>&1; then ctr=docker; else ctr=podman; fi && \$ctr build --network=host --pull=false -f 'services/platform/backoffice-web/Dockerfile' -t '${IMAGE_NAME}' 'services/platform/backoffice-web'"
ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" "${REMOTE_BUILD_CMD}"

if [[ "${TARGET_HOST}" != "${NODE_IP_BUILD}" || "${TARGET_SSH_PORT}" != "${BUILD_SSH_PORT}" ]]; then
  ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" "if command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c save '${IMAGE_NAME}'" | ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${TARGET_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${TARGET_HOST}" "if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c load"
fi

REMOTE_DEPLOY_CMD="if sudo -n docker version >/dev/null 2>&1; then ctr='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then ctr='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then ctr=docker; elif command -v podman >/dev/null 2>&1; then ctr=podman; else echo 'missing docker/podman'; exit 2; fi; (\$ctr rm -f 'simple-living-backoffice-web' >/dev/null 2>&1 || true) && \$ctr run --pull=never -d --name 'simple-living-backoffice-web' --restart unless-stopped -p '8088:8088' '${IMAGE_NAME}' >/dev/null && [ \$(\$ctr inspect -f '{{.State.Status}}' 'simple-living-backoffice-web' 2>/dev/null || echo unknown) = running ]"
ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${TARGET_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${TARGET_HOST}" "${REMOTE_DEPLOY_CMD}"

echo "Deployed ${SERVICE_NAME} on ${TARGET_HOST}"
