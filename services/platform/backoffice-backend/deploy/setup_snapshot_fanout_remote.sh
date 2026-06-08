#!/usr/bin/env bash
# Install inotify snapshot fan-out on the backoffice guest (local-qemu lab).
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"
# shellcheck disable=SC1091
source "${ROOT_DIR}/environments/local-qemu/lab-ports.env"

: "${SSH_USER:?SSH_USER is required}"
: "${SSH_KEY_PATH:?SSH_KEY_PATH is required}"
: "${NODE_IP_BACKOFFICE:?NODE_IP_BACKOFFICE is required}"

if [[ "${ENABLE_SNAPSHOT_FANOUT:-0}" != "1" ]]; then
  echo "ENABLE_SNAPSHOT_FANOUT is not 1; skipping snapshot fan-out setup."
  exit 0
fi

EXPORT_DIR="${EXPORT_DIR:-/var/lib/simple-living/exports}"
SNAPSHOT_DIR="${SNAPSHOT_DIR:-/var/lib/simple-living/exports}"
BACKOFFICE_SSH_PORT="${NODE_SSH_PORT_BACKOFFICE:-${SSH_PORT:-22}}"
FANOUT_GATEWAY_HOST="${FANOUT_GATEWAY_HOST:-${LAB_QEMU_GATEWAY_HOST:-10.0.2.2}}"
FANOUT_KEY_NAME="simple_living_fanout"
FANOUT_KEY_REMOTE="~/.ssh/${FANOUT_KEY_NAME}"

SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

FANOUT_PORTS=()
if [[ -n "${NODE_SSH_PORT_RECOMMENDATION:-}" ]]; then
  FANOUT_PORTS+=("${NODE_SSH_PORT_RECOMMENDATION}")
fi
if [[ -n "${NODE_SSH_PORT_TRACKING:-}" ]]; then
  FANOUT_PORTS+=("${NODE_SSH_PORT_TRACKING}")
fi
if [[ "${#FANOUT_PORTS[@]}" -eq 0 ]]; then
  echo "No consumer SSH ports configured (NODE_SSH_PORT_RECOMMENDATION / NODE_SSH_PORT_TRACKING)" >&2
  exit 1
fi

STAGING="$(mktemp -d)"
trap 'rm -rf "${STAGING}"' EXIT

cp "${ROOT_DIR}/services/platform/backoffice-backend/scripts/snapshot_fanout_push.sh" "${STAGING}/"
cp "${ROOT_DIR}/services/platform/backoffice-backend/scripts/snapshot_fanout_watch.sh" "${STAGING}/"
cp "${ROOT_DIR}/services/platform/backoffice-backend/deploy/install_snapshot_fanout.sh" "${STAGING}/"
cp "${ROOT_DIR}/services/platform/backoffice-backend/deploy/simple-living-snapshot-fanout.service" "${STAGING}/"

cat > "${STAGING}/snapshot-fanout.env" <<EOF
EXPORT_DIR=${EXPORT_DIR}
SNAPSHOT_DIR=${SNAPSHOT_DIR}
FANOUT_GATEWAY_HOST=${FANOUT_GATEWAY_HOST}
FANOUT_SSH_USER=${SSH_USER}
FANOUT_SSH_IDENTITY_FILE=/home/${SSH_USER}/.ssh/${FANOUT_KEY_NAME}
FANOUT_SSH_PORTS="${FANOUT_PORTS[*]}"
FANOUT_DEBOUNCE_SEC=${FANOUT_DEBOUNCE_SEC:-2}
EOF

REMOTE_STAGING="/tmp/simple-living-snapshot-fanout-$$"
ssh "${SSH_OPTS[@]}" -p "${BACKOFFICE_SSH_PORT}" -i "${SSH_KEY_PATH}" \
  "${SSH_USER}@${NODE_IP_BACKOFFICE}" "mkdir -p '${REMOTE_STAGING}' ~/.ssh && chmod 700 ~/.ssh"

scp "${SSH_OPTS[@]}" -P "${BACKOFFICE_SSH_PORT}" -i "${SSH_KEY_PATH}" \
  "${STAGING}/snapshot_fanout_push.sh" \
  "${STAGING}/snapshot_fanout_watch.sh" \
  "${STAGING}/install_snapshot_fanout.sh" \
  "${STAGING}/simple-living-snapshot-fanout.service" \
  "${STAGING}/snapshot-fanout.env" \
  "${SSH_USER}@${NODE_IP_BACKOFFICE}:${REMOTE_STAGING}/"

scp "${SSH_OPTS[@]}" -P "${BACKOFFICE_SSH_PORT}" -i "${SSH_KEY_PATH}" \
  "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_BACKOFFICE}:.ssh/${FANOUT_KEY_NAME}"

ssh "${SSH_OPTS[@]}" -p "${BACKOFFICE_SSH_PORT}" -i "${SSH_KEY_PATH}" \
  "${SSH_USER}@${NODE_IP_BACKOFFICE}" \
  "chmod 600 ~/.ssh/${FANOUT_KEY_NAME} && bash '${REMOTE_STAGING}/install_snapshot_fanout.sh' '${REMOTE_STAGING}'"

ssh "${SSH_OPTS[@]}" -p "${BACKOFFICE_SSH_PORT}" -i "${SSH_KEY_PATH}" \
  "${SSH_USER}@${NODE_IP_BACKOFFICE}" "rm -rf '${REMOTE_STAGING}'"

echo "Snapshot fan-out enabled on ${NODE_IP_BACKOFFICE}:${BACKOFFICE_SSH_PORT}."
