#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

PG_CONNINFO="${PG_CONNINFO:-host=${POSTGRES_HOST:-10.0.2.2} port=${POSTGRES_PORT:-15432} dbname=${POSTGRES_DB:-simple_living} user=${POSTGRES_USER:-simple} password=${POSTGRES_PASSWORD:-simple} connect_timeout=5}"
KAFKA_BROKERS="${KAFKA_BOOTSTRAP_SERVERS:-10.0.2.2:19092}"
SNAPSHOT_DIR="${SNAPSHOT_DIR:-/var/lib/simple-living/exports}"

export DEPLOY_ROOT_DIR="${ROOT_DIR}"
export DEPLOY_SERVICE_NAME="tracking-server"
export DEPLOY_BAZEL_TARGET="//services/tracking-server:tracking_server"
export DEPLOY_BAZEL_BIN_REL="services/tracking-server/tracking_server"
export DEPLOY_DOCKERFILE_REL="services/tracking-server/deploy/Dockerfile.cpp-service"
export DEPLOY_TARGET_HOST="${NODE_IP_TRACKING}"
export DEPLOY_TARGET_SSH_PORT="${NODE_SSH_PORT_TRACKING:-${SSH_PORT:-22}}"
export DEPLOY_BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT:-22}}"
export DEPLOY_CONTAINER_NAME="simple-living-tracking-server"
export DEPLOY_DOCKER_NETWORK="host"
export DEPLOY_EXTRA_RUN_ARGS="-v ${SNAPSHOT_DIR}:${SNAPSHOT_DIR}"
export DEPLOY_REMOTE_PREP_CMD="sudo mkdir -p '${SNAPSHOT_DIR}' &&"
export DEPLOY_SERVER_FLAGS="-pg_conninfo='${PG_CONNINFO}' -kafka_brokers='${KAFKA_BROKERS}' -snapshot_dir='${SNAPSHOT_DIR}'"

exec bash "${ROOT_DIR}/tools/deploy_cpp_service.sh"
