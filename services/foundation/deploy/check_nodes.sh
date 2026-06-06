#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LOCAL_QEMU_DIR="${LOCAL_QEMU_DIR:-${ROOT_DIR}/environments/local-qemu}"
ENV_FILE="${ENV_FILE:-${LOCAL_QEMU_DIR}/nodes.env}"
if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi
# shellcheck disable=SC1091
source "${LOCAL_QEMU_DIR}/lab-ports.env"

ssh_port="${NODE_SSH_PORT_FOUNDATION:-2211}"
ssh_opts=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5)

echo "Checking foundation node on ssh port ${ssh_port}..."
ssh "${ssh_opts[@]}" -p "${ssh_port}" ubuntu@127.0.0.1 "echo foundation-ready && command -v podman"

echo "Checking forwarded ports..."
for port in "${SERVICE_PORT_FOUNDATION_POSTGRES}" "${SERVICE_PORT_FOUNDATION_REDIS}" "${SERVICE_PORT_FOUNDATION_KAFKA}"; do
  if nc -z 127.0.0.1 "${port}"; then
    echo "127.0.0.1:${port} reachable"
  else
    echo "127.0.0.1:${port} not reachable yet"
  fi
done
