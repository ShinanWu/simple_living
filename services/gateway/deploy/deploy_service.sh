#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

export DEPLOY_ROOT_DIR="${ROOT_DIR}"
export DEPLOY_SERVICE_NAME="gateway"
export DEPLOY_BAZEL_TARGET="//services/gateway:gateway_edge_server"
export DEPLOY_BAZEL_BIN_REL="services/gateway/gateway_edge_server"
export DEPLOY_DOCKERFILE_REL="services/gateway/deploy/Dockerfile.cpp-service"
export DEPLOY_TARGET_HOST="${NODE_IP_GATEWAY}"
export DEPLOY_TARGET_SSH_PORT="${NODE_SSH_PORT_GATEWAY:-${SSH_PORT:-22}}"
export DEPLOY_BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT:-22}}"
export DEPLOY_CONTAINER_NAME="simple-living-gateway"
export DEPLOY_DOCKER_NETWORK="bridge"
export DEPLOY_PORT_MAP="-p 8080:8080"
export DEPLOY_EXTRA_RUN_ARGS="-e GATEWAY_USER_SERVER_ADDR=10.0.2.2:19101 -e GATEWAY_RECOMMENDATION_SERVER_ADDR=10.0.2.2:19103 -e GATEWAY_TRACKING_SERVER_ADDR=10.0.2.2:19105 -e GATEWAY_BACKOFFICE_BACKEND_ADDR=10.0.2.2:19110 -e EXPORT_DIR=/var/lib/simple-living/exports"
export DEPLOY_SERVER_FLAGS=""

exec bash "${ROOT_DIR}/tools/deploy_cpp_service.sh"
