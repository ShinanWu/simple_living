#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

PG_HOST="${POSTGRES_HOST:-127.0.0.1}"
PG_PORT="${FOUNDATION_POSTGRES_HOST_PORT:-${POSTGRES_PORT:-15432}}"
PG_USER="${POSTGRES_USER:-simple}"
PG_DB="${POSTGRES_DB:-simple_living}"
export PGPASSWORD="${POSTGRES_PASSWORD:-simple}"

if ! command -v psql >/dev/null 2>&1; then
  echo "psql not found; install PostgreSQL client or run from a host with psql" >&2
  exit 2
fi

PSQL=(psql -h "${PG_HOST}" -p "${PG_PORT}" -U "${PG_USER}" -d "${PG_DB}" -v ON_ERROR_STOP=1)

apply_file() {
  local f="$1"
  local ver
  ver="$(basename "${f}")"
  if "${PSQL[@]}" -tAc "SELECT 1 FROM schema_migrations WHERE version='${ver}'" 2>/dev/null | grep -q 1; then
    echo "SKIP ${ver}"
    return 0
  fi
  echo "APPLY ${ver}"
  "${PSQL[@]}" -f "${f}"
  "${PSQL[@]}" -c "INSERT INTO schema_migrations (version) VALUES ('${ver}') ON CONFLICT DO NOTHING;"
}

"${PSQL[@]}" -f "${ROOT_DIR}/services/foundation/migrations/0000_schema_registry.sql"

shopt -s nullglob
for f in "${ROOT_DIR}"/services/foundation/migrations/*.sql; do
  [[ "$(basename "${f}")" == "0000_schema_registry.sql" ]] && continue
  apply_file "${f}"
done
for dir in "${ROOT_DIR}"/services/*/migrations "${ROOT_DIR}"/services/platform/*/migrations; do
  [[ -d "${dir}" ]] || continue
  for f in "${dir}"/*.sql; do
    apply_file "${f}"
  done
done

echo "Migrations complete."
