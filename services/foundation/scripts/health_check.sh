#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"
# shellcheck disable=SC1091
source "${ROOT_DIR}/environments/local-qemu/lab-ports.env"

PG_HOST="${POSTGRES_HOST:-127.0.0.1}"
# Mac 本机执行时经 host-forward 探测，不走 10.0.2.2（slirp 仅 QEMU 来宾可达）。
if [[ "${PG_HOST}" == "${LAB_QEMU_GATEWAY_HOST:-10.0.2.2}" ]] && [[ "$(uname -s)" == "Darwin" ]]; then
  PG_HOST="127.0.0.1"
fi
PG_PORT="${POSTGRES_PORT:-${SERVICE_PORT_FOUNDATION_POSTGRES}}"
REDIS_PORT="${REDIS_PORT:-${SERVICE_PORT_FOUNDATION_REDIS}}"
KAFKA_PORT="${SERVICE_PORT_FOUNDATION_KAFKA}"

fail=0

if command -v pg_isready >/dev/null 2>&1; then
  if pg_isready -h "${PG_HOST}" -p "${PG_PORT}" -U "${POSTGRES_USER:-simple}" -d "${POSTGRES_DB:-simple_living}" >/dev/null 2>&1; then
    echo "OK  PostgreSQL ${PG_HOST}:${PG_PORT}"
  else
    echo "FAIL PostgreSQL ${PG_HOST}:${PG_PORT}"
    fail=1
  fi
elif command -v nc >/dev/null 2>&1; then
  if nc -z "${PG_HOST}" "${PG_PORT}" 2>/dev/null; then
    echo "OK  PostgreSQL port ${PG_PORT} (tcp)"
  else
    echo "FAIL PostgreSQL port ${PG_PORT}"
    fail=1
  fi
else
  echo "SKIP PostgreSQL (no pg_isready/nc)"
fi

if command -v redis-cli >/dev/null 2>&1; then
  if redis-cli -h "${PG_HOST}" -p "${REDIS_PORT}" ping 2>/dev/null | grep -q PONG; then
    echo "OK  Redis ${PG_HOST}:${REDIS_PORT}"
  else
    echo "FAIL Redis ${PG_HOST}:${REDIS_PORT}"
    fail=1
  fi
elif command -v nc >/dev/null 2>&1; then
  if nc -z "${PG_HOST}" "${REDIS_PORT}" 2>/dev/null; then
    echo "OK  Redis port ${REDIS_PORT} (tcp)"
  else
    echo "FAIL Redis port ${REDIS_PORT}"
    fail=1
  fi
else
  echo "SKIP Redis (no redis-cli/nc)"
fi

if command -v nc >/dev/null 2>&1; then
  if nc -z "${PG_HOST}" "${KAFKA_PORT}" 2>/dev/null; then
    echo "OK  Kafka ${PG_HOST}:${KAFKA_PORT} (tcp)"
  else
    echo "FAIL Kafka ${PG_HOST}:${KAFKA_PORT}"
    fail=1
  fi
else
  echo "SKIP Kafka (no nc)"
fi

exit "${fail}"
