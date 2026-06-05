#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

export DEPLOY_ROOT_DIR="${ROOT_DIR}"
export DEPLOY_SERVICE_NAME="user-server"
export DEPLOY_BAZEL_TARGET="//services/user-server:user_server"
export DEPLOY_BAZEL_BIN_REL="services/user-server/user_server"
export DEPLOY_DOCKERFILE_REL="services/user-server/deploy/Dockerfile.cpp-service"
export DEPLOY_TARGET_HOST="${NODE_IP_USER}"
export DEPLOY_TARGET_SSH_PORT="${NODE_SSH_PORT_USER:-${SSH_PORT:-22}}"
export DEPLOY_BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT:-22}}"
export DEPLOY_CONTAINER_NAME="simple-living-user-server"
export DEPLOY_DOCKER_NETWORK="host"
export DEPLOY_SERVER_FLAGS=""

exec bash "${ROOT_DIR}/tools/deploy_cpp_service.sh"
