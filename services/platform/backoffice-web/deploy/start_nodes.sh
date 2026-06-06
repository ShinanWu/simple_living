#!/usr/bin/env bash
# backoffice-web runs on the nginx QEMU guest (with proxy + gateway).
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
exec bash "${ROOT_DIR}/services/proxy/deploy/start_nodes.sh"
