#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
ACTION="${1:-up}"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/infra/lab/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"
SSH_PORT="${SSH_PORT:-22}"
BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT}}"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)
REMOTE_RUNTIME="if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi"

case "${ACTION}" in
  up)
    ssh "${SSH_OPTS[@]}" -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
      "${REMOTE_RUNTIME}; \$c pull docker.m.daocloud.io/redpandadata/redpanda:v24.2.11 >/dev/null 2>&1 || true; \$c tag docker.m.daocloud.io/redpandadata/redpanda:v24.2.11 docker.redpanda.com/redpandadata/redpanda:v24.2.11 >/dev/null 2>&1 || true; \$c volume create simple_living_kafka >/dev/null; \$c rm -f simple-living-kafka >/dev/null 2>&1 || true; \$c run --pull=never -d --name simple-living-kafka --restart unless-stopped -p 9092:9092 -v simple_living_kafka:/var/lib/redpanda/data docker.redpanda.com/redpandadata/redpanda:v24.2.11 redpanda start --kafka-addr=0.0.0.0:9092 --advertise-kafka-addr=127.0.0.1:9092 --pandaproxy-addr=0.0.0.0:8082 --schema-registry-addr=0.0.0.0:8081 --smp=1 --memory=1G --mode=dev-container --default-log-level=warn >/dev/null"
    ;;
  down)
    ssh "${SSH_OPTS[@]}" -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
      "${REMOTE_RUNTIME}; \$c rm -f simple-living-kafka >/dev/null 2>&1 || true"
    ;;
  down-v)
    ssh "${SSH_OPTS[@]}" -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
      "${REMOTE_RUNTIME}; \$c rm -f simple-living-kafka >/dev/null 2>&1 || true; \$c volume rm -f simple_living_kafka >/dev/null 2>&1 || true"
    ;;
  *)
    echo "usage: $0 [up|down|down-v]" >&2
    exit 1
    ;;
esac
