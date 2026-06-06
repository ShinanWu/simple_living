#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
# shellcheck disable=SC1091
source "${ROOT_DIR}/environments/local-qemu/lab-ports.env"
exec bash "${ROOT_DIR}/environments/local-qemu/scripts/start_qemu_guest.sh" \
  "build:2209::" \
  "recommendation-server:2204:${SERVICE_PORT_RECOMMENDATION_SERVER}:${SERVICE_PORT_RECOMMENDATION_SERVER}"
