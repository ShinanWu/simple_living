#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

PG_HOST="${POSTGRES_HOST:-127.0.0.1}"
PG_PORT="${FOUNDATION_POSTGRES_HOST_PORT:-${POSTGRES_PORT:-15432}}"
REDIS_PORT="${FOUNDATION_REDIS_HOST_PORT:-${REDIS_PORT:-16379}}"
KAFKA_PORT="${FOUNDATION_KAFKA_HOST_PORT:-19092}"

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
  echo "SKIP Redis"
fi

if command -v nc >/dev/null 2>&1; then
  if nc -z "${PG_HOST}" "${KAFKA_PORT}" 2>/dev/null; then
    echo "OK  Kafka port ${PG_HOST}:${KAFKA_PORT} (tcp)"
  else
    echo "FAIL Kafka port ${PG_HOST}:${KAFKA_PORT}"
    fail=1
  fi
else
  echo "SKIP Kafka"
fi

exit "${fail}"
