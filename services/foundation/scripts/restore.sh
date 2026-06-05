#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 1 ]]; then
  echo "usage: $0 <backup.sql.gz>" >&2
  exit 1
fi
BACKUP="$1"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

FOUNDATION_SSH_PORT="${NODE_SSH_PORT_FOUNDATION:-${SSH_PORT:-22}}"
FOUNDATION_HOST="${NODE_IP_FOUNDATION:-127.0.0.1}"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)
REMOTE_RUNTIME="if sudo -n docker version >/dev/null 2>&1; then c='sudo -n docker'; elif sudo -n podman version >/dev/null 2>&1; then c='sudo -n podman'; elif command -v docker >/dev/null 2>&1; then c=docker; else c=podman; fi"

echo "Restoring ${BACKUP} into simple-living-postgres on ${FOUNDATION_HOST}"
gunzip -c "${BACKUP}" | ssh "${SSH_OPTS[@]}" -p "${FOUNDATION_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${FOUNDATION_HOST}" \
  "${REMOTE_RUNTIME}; \$c exec -i simple-living-postgres psql -U simple -d simple_living"
echo "Restore complete."
