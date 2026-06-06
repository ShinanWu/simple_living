#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
# shellcheck disable=SC1091
source "${ROOT_DIR}/environments/local-qemu/lab-ports.env"
exec bash "${ROOT_DIR}/environments/local-qemu/scripts/start_qemu_guest.sh" \
  "build:2209::" \
  "user-server:2202:${SERVICE_PORT_USER_SERVER}:${SERVICE_PORT_USER_SERVER}"
