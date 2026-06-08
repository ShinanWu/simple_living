#!/usr/bin/env bash
# Push export_dir from the local backoffice guest to snapshot consumer guests.
set -euo pipefail

ENV_FILE="${SNAPSHOT_FANOUT_ENV:-/etc/simple-living/snapshot-fanout.env}"
if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi

: "${EXPORT_DIR:?EXPORT_DIR is required}"
: "${SNAPSHOT_DIR:?SNAPSHOT_DIR is required}"
: "${FANOUT_SSH_USER:?FANOUT_SSH_USER is required}"
: "${FANOUT_SSH_IDENTITY_FILE:?FANOUT_SSH_IDENTITY_FILE is required}"
: "${FANOUT_GATEWAY_HOST:?FANOUT_GATEWAY_HOST is required}"
: "${FANOUT_SSH_PORTS:?FANOUT_SSH_PORTS is required}"

if [[ ! -d "${EXPORT_DIR}/active" ]]; then
  echo "snapshot fan-out: missing ${EXPORT_DIR}/active" >&2
  exit 1
fi

SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=15 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

for port in ${FANOUT_SSH_PORTS}; do
  echo "snapshot fan-out: pushing to ${FANOUT_GATEWAY_HOST}:${port} ..."
  ssh "${SSH_OPTS[@]}" -p "${port}" -i "${FANOUT_SSH_IDENTITY_FILE}" \
    "${FANOUT_SSH_USER}@${FANOUT_GATEWAY_HOST}" \
    "sudo mkdir -p '${SNAPSHOT_DIR}' && sudo chown -R ${FANOUT_SSH_USER}:${FANOUT_SSH_USER} '${SNAPSHOT_DIR}'"
  rsync -az -e "ssh ${SSH_OPTS[*]} -p ${port} -i ${FANOUT_SSH_IDENTITY_FILE}" \
    "${EXPORT_DIR}/" "${FANOUT_SSH_USER}@${FANOUT_GATEWAY_HOST}:${SNAPSHOT_DIR}/"
done

echo "snapshot fan-out: push complete (${EXPORT_DIR} -> consumers)."
