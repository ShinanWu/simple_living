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
      "${REMOTE_RUNTIME}; \$c pull docker.m.daocloud.io/library/postgres:16-alpine >/dev/null 2>&1 || true; \$c tag docker.m.daocloud.io/library/postgres:16-alpine postgres:16-alpine >/dev/null 2>&1 || true; \$c volume create simple_living_pg >/dev/null; \$c rm -f simple-living-postgres >/dev/null 2>&1 || true; \$c run --pull=never -d --name simple-living-postgres --restart unless-stopped -e POSTGRES_USER=simple -e POSTGRES_PASSWORD=simple -e POSTGRES_DB=simple_living -p 5432:5432 -v simple_living_pg:/var/lib/postgresql/data postgres:16-alpine >/dev/null"
    ;;
  down)
    ssh "${SSH_OPTS[@]}" -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
      "${REMOTE_RUNTIME}; \$c rm -f simple-living-postgres >/dev/null 2>&1 || true"
    ;;
  down-v)
    ssh "${SSH_OPTS[@]}" -p "${BUILD_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BUILD}" \
      "${REMOTE_RUNTIME}; \$c rm -f simple-living-postgres >/dev/null 2>&1 || true; \$c volume rm -f simple_living_pg >/dev/null 2>&1 || true"
    ;;
  *)
    echo "usage: $0 [up|down|down-v]" >&2
    exit 1
    ;;
esac
