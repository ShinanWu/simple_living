#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
# shellcheck disable=SC1091
source "${ROOT_DIR}/environments/local-qemu/lab-ports.env"
exec bash "${ROOT_DIR}/environments/local-qemu/scripts/start_qemu_guest.sh" \
  "nginx:2208:${SERVICE_PORT_PROXY_HTTP}:${SERVICE_PORT_PROXY_HTTP}|${SERVICE_PORT_GATEWAY_HTTP}-${SERVICE_PORT_GATEWAY_HTTP},${SERVICE_PORT_BACKOFFICE_WEB}-${SERVICE_PORT_BACKOFFICE_WEB},11080-10080,17000-7000"
