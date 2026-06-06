#!/usr/bin/env bash
# Relay pending rows from tracking_event_outbox to Kafka (lab / single-node).
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

PG_HOST="${POSTGRES_HOST:-127.0.0.1}"
# shellcheck disable=SC1091
source "${ROOT_DIR}/environments/local-qemu/lab-ports.env"
PG_PORT="${POSTGRES_PORT:-${SERVICE_PORT_FOUNDATION_POSTGRES}}"
export PGPASSWORD="${POSTGRES_PASSWORD:-simple}"
BOOTSTRAP="${KAFKA_BOOTSTRAP_SERVERS:-${PG_HOST}:${SERVICE_PORT_FOUNDATION_KAFKA}}"
BATCH="${BATCH:-50}"

if ! command -v psql >/dev/null 2>&1; then
  echo "psql required" >&2
  exit 2
fi

rows="$(psql -h "${PG_HOST}" -p "${PG_PORT}" -U "${POSTGRES_USER:-simple}" -d "${POSTGRES_DB:-simple_living}" -At -c \
  "SELECT event_id, topic, payload FROM tracking_event_outbox WHERE published = FALSE ORDER BY event_id LIMIT ${BATCH}")"

if [[ -z "${rows}" ]]; then
  echo "No pending outbox events."
  exit 0
fi

while IFS=$'\t' read -r event_id topic payload; do
  [[ -z "${event_id}" ]] && continue
  if command -v kcat >/dev/null 2>&1; then
    printf '%s' "${payload}" | kcat -P -b "${BOOTSTRAP}" -t "${topic}" -k "${event_id}" >/dev/null
  elif command -v kafka-console-producer.sh >/dev/null 2>&1; then
    printf '%s\n' "${payload}" | kafka-console-producer.sh --bootstrap-server "${BOOTSTRAP}" --topic "${topic}" >/dev/null
  else
    echo "WARN no kcat/kafka-console-producer; marking published without Kafka (${event_id})"
  fi
  psql -h "${PG_HOST}" -p "${PG_PORT}" -U "${POSTGRES_USER:-simple}" -d "${POSTGRES_DB:-simple_living}" -c \
    "UPDATE tracking_event_outbox SET published = TRUE WHERE event_id = ${event_id}" >/dev/null
  echo "RELAY ${topic} event_id=${event_id}"
done <<< "${rows}"
