#!/usr/bin/env bash
# One-shot snapshot sync from Mac (troubleshooting). Normal lab flow uses backoffice-guest
# inotify fan-out (ENABLE_SNAPSHOT_FANOUT=1); see backoffice-backend/deploy/README.md §6.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

: "${SSH_USER:?SSH_USER is required}"
: "${SSH_KEY_PATH:?SSH_KEY_PATH is required}"
: "${NODE_IP_BACKOFFICE:?NODE_IP_BACKOFFICE is required}"

SNAPSHOT_DIR="${SNAPSHOT_DIR:-/var/lib/simple-living/exports}"
BACKOFFICE_SSH_PORT="${NODE_SSH_PORT_BACKOFFICE:-${SSH_PORT:-22}}"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

TARGETS=()
if [[ -n "${NODE_IP_RECOMMENDATION:-}" ]]; then
  TARGETS+=("${NODE_IP_RECOMMENDATION}:${NODE_SSH_PORT_RECOMMENDATION:-${SSH_PORT:-22}}")
fi
if [[ -n "${NODE_IP_TRACKING:-}" ]]; then
  TARGETS+=("${NODE_IP_TRACKING}:${NODE_SSH_PORT_TRACKING:-${SSH_PORT:-22}}")
fi

if [[ "${#TARGETS[@]}" -eq 0 ]]; then
  echo "No snapshot consumer nodes configured in ${ENV_FILE}" >&2
  exit 1
fi

STAGING="$(mktemp -d)"
trap 'rm -rf "${STAGING}"' EXIT

echo "Pulling snapshot from backoffice (${NODE_IP_BACKOFFICE}:${BACKOFFICE_SSH_PORT}) ..."
rsync -az -e "ssh ${SSH_OPTS[*]} -p ${BACKOFFICE_SSH_PORT} -i ${SSH_KEY_PATH}" \
  "${SSH_USER}@${NODE_IP_BACKOFFICE}:${SNAPSHOT_DIR}/" "${STAGING}/"

for target in "${TARGETS[@]}"; do
  host="${target%%:*}"
  port="${target##*:}"
  echo "Pushing snapshot to ${host}:${port} ..."
  ssh "${SSH_OPTS[@]}" -p "${port}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${host}" \
    "sudo mkdir -p '${SNAPSHOT_DIR}' && sudo chown -R ${SSH_USER}:${SSH_USER} '${SNAPSHOT_DIR}'"
  rsync -az -e "ssh ${SSH_OPTS[*]} -p ${port} -i ${SSH_KEY_PATH}" \
    "${STAGING}/" "${SSH_USER}@${host}:${SNAPSHOT_DIR}/"
done

echo "Snapshot sync complete (${SNAPSHOT_DIR})."
