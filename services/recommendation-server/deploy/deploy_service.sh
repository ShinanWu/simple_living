#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

: "${NODE_IP_RECOMMENDATION:?NODE_IP_RECOMMENDATION is required}"

PG_CONNINFO="${PG_CONNINFO:-host=${POSTGRES_HOST:-10.0.2.2} port=${POSTGRES_PORT:-5432} dbname=${POSTGRES_DB:-simple_living} user=${POSTGRES_USER:-simple} password=${POSTGRES_PASSWORD:-simple} connect_timeout=5}"
SNAPSHOT_DIR="${SNAPSHOT_DIR:-/var/lib/simple-living/exports}"

export DEPLOY_ROOT_DIR="${ROOT_DIR}"
export DEPLOY_SERVICE_NAME="recommendation-server"
export DEPLOY_BAZEL_TARGET="//services/recommendation-server:recommendation_server"
export DEPLOY_BAZEL_BIN_REL="services/recommendation-server/recommendation_server"
export DEPLOY_DOCKERFILE_REL="services/recommendation-server/deploy/Dockerfile.cpp-service"
export DEPLOY_TARGET_HOST="${NODE_IP_RECOMMENDATION}"
export DEPLOY_TARGET_SSH_PORT="${NODE_SSH_PORT_RECOMMENDATION:-${SSH_PORT:-22}}"
export DEPLOY_BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT:-22}}"
export DEPLOY_CONTAINER_NAME="simple-living-recommendation-server"
export DEPLOY_DOCKER_NETWORK="host"
export DEPLOY_EXTRA_RUN_ARGS="-v ${SNAPSHOT_DIR}:${SNAPSHOT_DIR}"
export DEPLOY_REMOTE_PREP_CMD="sudo mkdir -p '${SNAPSHOT_DIR}' &&"
export DEPLOY_SERVER_FLAGS="-port=${SERVICE_PORT_RECOMMENDATION_SERVER:-9103} -pg_conninfo='${PG_CONNINFO}' -snapshot_dir='${SNAPSHOT_DIR}'"

exec bash "${ROOT_DIR}/tools/deploy_cpp_service.sh"
