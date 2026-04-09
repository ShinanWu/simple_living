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
BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT}}"
IMAGE_TAG="${IMAGE_TAG:-latest}"
REGISTRY="${REGISTRY:-}"
BASE_IMAGE="${BASE_IMAGE:-debian:bookworm-slim}"
QEMU_BUILD_WORKDIR="${QEMU_BUILD_WORKDIR:-/root/simple_living}"

BAZEL_TARGETS='//services/gateway:gateway_edge_server //services/user-domain:user_domain_server //services/content-domain:content_domain_server //services/recommendation-domain:recommendation_domain_server //services/affiliate-domain:affiliate_domain_server //services/tracking-domain:tracking_domain_server //services/governance-domain:governance_domain_server'
BAZEL_BIN="${BAZEL_BIN:-/usr/bin/bazel-3}"
BAZEL_EXTRA_FLAGS="${BAZEL_EXTRA_FLAGS:---experimental_repository_downloader_retries=8 --http_timeout_scaling=5 --http_connector_retry_max_timeout=120s}"

echo "Building on QEMU node: ${NODE_IP_BUILD}"
ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
  "cd '${QEMU_BUILD_WORKDIR}' && ${BAZEL_BIN} build ${BAZEL_EXTRA_FLAGS} ${BAZEL_TARGETS}"

echo "Building images on QEMU node: ${NODE_IP_BUILD}"
ssh -o BatchMode=yes -o ConnectTimeout=10 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
  "cd '${QEMU_BUILD_WORKDIR}' && REGISTRY='${REGISTRY}' IMAGE_TAG='${IMAGE_TAG}' BASE_IMAGE='${BASE_IMAGE}' CONTAINER_CLI=auto bash infra/docker/build_images.sh"

echo "Build and image packaging complete on ${NODE_IP_BUILD}."
