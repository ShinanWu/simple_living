#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/infra/lab/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

SERVICE_NAME="tracking-domain"
IMAGE_REPO="tracking-domain"
IMAGE_TAG="${IMAGE_TAG:-latest}"
BASE_IMAGE="${BASE_IMAGE:-debian:bookworm-slim}"
TARGET_HOST="${NODE_IP_TRACKING}"
SSH_PORT="${SSH_PORT:-22}"
TARGET_SSH_PORT="${NODE_SSH_PORT_TRACKING:-${SSH_PORT}}"
BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT}}"
QEMU_BUILD_WORKDIR="${QEMU_BUILD_WORKDIR:-/home/ubuntu/simple_living}"
IMAGE_NAME="localhost/${IMAGE_REPO}:${IMAGE_TAG}"

REMOTE_BUILD_CMD="cd '${QEMU_BUILD_WORKDIR}' && if [[ -f .bazelversion ]]; then export USE_BAZEL_VERSION=\$(tr -d '\n' < .bazelversion); fi; if command -v bazelisk >/dev/null 2>&1; then bazelisk build ${BAZEL_BUILD_ARGS:-} //services/tracking-domain:tracking_domain_server; elif command -v bazel >/dev/null 2>&1; then bazel build ${BAZEL_BUILD_ARGS:-} //services/tracking-domain:tracking_domain_server; else echo 'missing bazel/bazelisk'; exit 2; fi && build_ctx=\$(mktemp -d) && cp bazel-bin/services/tracking-domain/tracking_domain_server \${build_ctx}/server && cp services/tracking-domain/deploy/Dockerfile.cpp-service \${build_ctx}/Dockerfile.cpp-service && if command -v docker >/dev/null 2>&1; then ctr=docker; else ctr=podman; fi && \$ctr build --build-arg BASE_IMAGE='${BASE_IMAGE}' -f \${build_ctx}/Dockerfile.cpp-service -t '${IMAGE_NAME}' \${build_ctx} && rm -rf \${build_ctx}"
ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" "${REMOTE_BUILD_CMD}"

if [[ "${TARGET_HOST}" != "${NODE_IP_BUILD}" || "${TARGET_SSH_PORT}" != "${BUILD_SSH_PORT}" ]]; then
  ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" "if command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c save '${IMAGE_NAME}'" | ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${TARGET_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${TARGET_HOST}" "if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c load"
fi

EXTRA_RUN_ARGS=""
REMOTE_DEPLOY_CMD="if sudo -n docker version >/dev/null 2>&1; then ctr='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then ctr='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then ctr=docker; elif command -v podman >/dev/null 2>&1; then ctr=podman; else echo 'missing docker/podman'; exit 2; fi; (\$ctr rm -f 'simple-living-tracking-domain' >/dev/null 2>&1 || true) && \$ctr run --pull=never -d --name 'simple-living-tracking-domain' --restart unless-stopped -p '9105:9105' ${EXTRA_RUN_ARGS} '${IMAGE_NAME}' >/dev/null && [ \$(\$ctr inspect -f '{{.State.Status}}' 'simple-living-tracking-domain' 2>/dev/null || echo unknown) = running ]"
ssh -o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${TARGET_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${TARGET_HOST}" "${REMOTE_DEPLOY_CMD}"

echo "Deployed ${SERVICE_NAME} on ${TARGET_HOST}"
