#!/usr/bin/env bash
# Run on the backoffice QEMU guest (via setup_snapshot_fanout_remote.sh).
set -euo pipefail

INSTALL_ROOT="${SNAPSHOT_FANOUT_INSTALL_ROOT:-/usr/local/lib/simple-living}"
ENV_PATH="${SNAPSHOT_FANOUT_ENV_PATH:-/etc/simple-living/snapshot-fanout.env}"
SERVICE_NAME="simple-living-snapshot-fanout.service"
STAGING_DIR="${1:?staging dir with scripts and unit file is required}"

sudo mkdir -p "${INSTALL_ROOT}" /etc/simple-living
sudo install -m 0755 "${STAGING_DIR}/snapshot_fanout_push.sh" "${INSTALL_ROOT}/"
sudo install -m 0755 "${STAGING_DIR}/snapshot_fanout_watch.sh" "${INSTALL_ROOT}/"
sudo install -m 0644 "${STAGING_DIR}/snapshot-fanout.env" "${ENV_PATH}"
sudo install -m 0644 "${STAGING_DIR}/${SERVICE_NAME}" "/etc/systemd/system/${SERVICE_NAME}"

if ! command -v inotifywait >/dev/null 2>&1; then
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends inotify-tools rsync openssh-client
fi

sudo systemctl daemon-reload
sudo systemctl enable "${SERVICE_NAME}"
sudo systemctl restart "${SERVICE_NAME}"
sudo systemctl --no-pager --full status "${SERVICE_NAME}" || true

echo "snapshot fan-out installed (${SERVICE_NAME})."
