#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
ACTION="${1:-up}"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"
# shellcheck disable=SC1091
source "${ROOT_DIR}/environments/local-qemu/lab-ports.env"
KAFKA_ADVERTISED="${LAB_QEMU_GATEWAY_HOST:-10.0.2.2}:${SERVICE_PORT_FOUNDATION_KAFKA:-9092}"
SSH_PORT="${SSH_PORT:-22}"
FOUNDATION_SSH_PORT="${NODE_SSH_PORT_FOUNDATION:-${SSH_PORT}}"
FOUNDATION_HOST="${NODE_IP_FOUNDATION:-127.0.0.1}"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)
REMOTE_RUNTIME="if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi"

case "${ACTION}" in
  up)
    ssh "${SSH_OPTS[@]}" -p "${FOUNDATION_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${FOUNDATION_HOST}" \
      "${REMOTE_RUNTIME}; \$c pull docker.m.daocloud.io/apache/kafka:3.8.0 >/dev/null; \$c volume create simple_living_kafka >/dev/null 2>&1 || true; \$c rm -f simple-living-kafka >/dev/null 2>&1 || true; \$c run --pull=never -d --name simple-living-kafka --restart unless-stopped -p 9092:9092 -e KAFKA_NODE_ID=1 -e KAFKA_PROCESS_ROLES=broker,controller -e KAFKA_LISTENERS=PLAINTEXT://0.0.0.0:9092,CONTROLLER://0.0.0.0:9093 -e KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://${KAFKA_ADVERTISED} -e KAFKA_CONTROLLER_LISTENER_NAMES=CONTROLLER -e KAFKA_LISTENER_SECURITY_PROTOCOL_MAP=CONTROLLER:PLAINTEXT,PLAINTEXT:PLAINTEXT -e KAFKA_CONTROLLER_QUORUM_VOTERS=1@127.0.0.1:9093 -e KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR=1 -e KAFKA_TRANSACTION_STATE_LOG_REPLICATION_FACTOR=1 -e KAFKA_TRANSACTION_STATE_LOG_MIN_ISR=1 -v simple_living_kafka:/var/lib/kafka/data docker.m.daocloud.io/apache/kafka:3.8.0 >/dev/null"
    ;;
  down)
    ssh "${SSH_OPTS[@]}" -p "${FOUNDATION_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${FOUNDATION_HOST}" \
      "${REMOTE_RUNTIME}; \$c rm -f simple-living-kafka >/dev/null 2>&1 || true"
    ;;
  down-v)
    ssh "${SSH_OPTS[@]}" -p "${FOUNDATION_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${FOUNDATION_HOST}" \
      "${REMOTE_RUNTIME}; \$c rm -f simple-living-kafka >/dev/null 2>&1 || true; \$c volume rm -f simple_living_kafka >/dev/null 2>&1 || true"
    ;;
  health)
    bash "${ROOT_DIR}/services/foundation/scripts/health_check.sh"
    ;;
  relay)
    bash "${ROOT_DIR}/services/foundation/kafka/scripts/outbox_relay.sh"
    ;;
  *)
    echo "usage: $0 [up|down|down-v|health|relay]" >&2
    exit 1
    ;;
esac
