#!/usr/bin/env bash
# Shared QEMU image build + deploy for C++ brpc services.
# Each services/*/deploy/deploy_service.sh sets DEPLOY_* variables then execs this script.
set -euo pipefail

: "${DEPLOY_ROOT_DIR:?DEPLOY_ROOT_DIR is required}"
if [[ -z "${SERVICE_PORT_USER_SERVER:-}" ]]; then
  # shellcheck disable=SC1091
  source "${DEPLOY_ROOT_DIR}/environments/local-qemu/lab-ports.env"
fi
: "${DEPLOY_SERVICE_NAME:?DEPLOY_SERVICE_NAME is required}"
: "${DEPLOY_BAZEL_TARGET:?DEPLOY_BAZEL_TARGET is required}"
: "${DEPLOY_BAZEL_BIN_REL:?DEPLOY_BAZEL_BIN_REL is required}"
: "${DEPLOY_DOCKERFILE_REL:?DEPLOY_DOCKERFILE_REL is required}"
: "${DEPLOY_TARGET_HOST:?DEPLOY_TARGET_HOST is required}"
: "${DEPLOY_TARGET_SSH_PORT:?DEPLOY_TARGET_SSH_PORT is required}"
: "${DEPLOY_CONTAINER_NAME:?DEPLOY_CONTAINER_NAME is required}"
: "${SSH_USER:?SSH_USER is required}"
: "${SSH_KEY_PATH:?SSH_KEY_PATH is required}"
: "${NODE_IP_BUILD:?NODE_IP_BUILD is required}"

DEPLOY_IMAGE_REPO="${DEPLOY_IMAGE_REPO:-${DEPLOY_SERVICE_NAME}}"
DEPLOY_IMAGE_TAG="${DEPLOY_IMAGE_TAG:-latest}"
DEPLOY_BASE_IMAGE="${DEPLOY_BASE_IMAGE:-debian:bookworm-slim}"
DEPLOY_BUILD_SSH_PORT="${DEPLOY_BUILD_SSH_PORT:-22}"
DEPLOY_QEMU_BUILD_WORKDIR="${DEPLOY_QEMU_BUILD_WORKDIR:-/home/ubuntu/simple_living}"
DEPLOY_EXTRA_RUN_ARGS="${DEPLOY_EXTRA_RUN_ARGS:-}"
DEPLOY_SERVER_FLAGS="${DEPLOY_SERVER_FLAGS:-}"
DEPLOY_DOCKER_NETWORK="${DEPLOY_DOCKER_NETWORK:-host}"
DEPLOY_PORT_MAP="${DEPLOY_PORT_MAP:-}"
DEPLOY_REMOTE_PREP_CMD="${DEPLOY_REMOTE_PREP_CMD:-}"

IMAGE_NAME="localhost/${DEPLOY_IMAGE_REPO}:${DEPLOY_IMAGE_TAG}"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

NETWORK_ARGS=()
if [[ "${DEPLOY_DOCKER_NETWORK}" == "host" ]]; then
  NETWORK_ARGS+=(--network host)
elif [[ -n "${DEPLOY_PORT_MAP}" ]]; then
  # shellcheck disable=SC2206
  NETWORK_ARGS+=(${DEPLOY_PORT_MAP})
fi

REMOTE_BUILD_CMD="cd '${DEPLOY_QEMU_BUILD_WORKDIR}' && if [[ -f .bazelversion ]]; then export USE_BAZEL_VERSION=\$(tr -d '\n' < .bazelversion); fi; if command -v bazelisk >/dev/null 2>&1; then bazelisk build ${BAZEL_BUILD_ARGS:-} ${DEPLOY_BAZEL_TARGET}; elif command -v bazel >/dev/null 2>&1; then bazel build ${BAZEL_BUILD_ARGS:-} ${DEPLOY_BAZEL_TARGET}; else echo 'missing bazel/bazelisk'; exit 2; fi && build_ctx=\$(mktemp -d) && cp bazel-bin/${DEPLOY_BAZEL_BIN_REL} \${build_ctx}/server && cp ${DEPLOY_DOCKERFILE_REL} \${build_ctx}/Dockerfile.cpp-service && if command -v docker >/dev/null 2>&1; then ctr=docker; else ctr=podman; fi && \$ctr build --build-arg BASE_IMAGE='${DEPLOY_BASE_IMAGE}' -f \${build_ctx}/Dockerfile.cpp-service -t '${IMAGE_NAME}' \${build_ctx} && rm -rf \${build_ctx}"

ssh "${SSH_OPTS[@]}" -p "${DEPLOY_BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" "${REMOTE_BUILD_CMD}"

if [[ "${DEPLOY_TARGET_HOST}" != "${NODE_IP_BUILD}" || "${DEPLOY_TARGET_SSH_PORT}" != "${DEPLOY_BUILD_SSH_PORT}" ]]; then
  ssh "${SSH_OPTS[@]}" -p "${DEPLOY_BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
    "if command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c save '${IMAGE_NAME}'" \
    | ssh "${SSH_OPTS[@]}" -p "${DEPLOY_TARGET_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${DEPLOY_TARGET_HOST}" \
      "if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi; \$c load"
fi

RUN_NETWORK=""
if [[ "${#NETWORK_ARGS[@]}" -gt 0 ]]; then
  RUN_NETWORK="${NETWORK_ARGS[*]} "
fi

REMOTE_DEPLOY_CMD="if sudo -n docker version >/dev/null 2>&1; then ctr='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then ctr='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then ctr=docker; elif command -v podman >/dev/null 2>&1; then ctr=podman; else echo 'missing docker/podman'; exit 2; fi; ${DEPLOY_REMOTE_PREP_CMD} (\$ctr rm -f '${DEPLOY_CONTAINER_NAME}' >/dev/null 2>&1 || true) && \$ctr run --pull=never -d --name '${DEPLOY_CONTAINER_NAME}' --restart unless-stopped ${RUN_NETWORK}${DEPLOY_EXTRA_RUN_ARGS} '${IMAGE_NAME}' ${DEPLOY_SERVER_FLAGS} >/dev/null && [ \$(\$ctr inspect -f '{{.State.Status}}' '${DEPLOY_CONTAINER_NAME}' 2>/dev/null || echo unknown) = running ]"

ssh "${SSH_OPTS[@]}" -p "${DEPLOY_TARGET_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${DEPLOY_TARGET_HOST}" "${REMOTE_DEPLOY_CMD}"

echo "Deployed ${DEPLOY_SERVICE_NAME} (${IMAGE_NAME}) on ${DEPLOY_TARGET_HOST}"
