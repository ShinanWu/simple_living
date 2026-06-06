#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
exec bash "${ROOT_DIR}/environments/local-qemu/scripts/stop_qemu_guest.sh" build backoffice-backend
