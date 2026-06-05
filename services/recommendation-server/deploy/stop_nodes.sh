#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
exec bash "${ROOT_DIR}/services/platform/backoffice-backend/deploy/stop_nodes.sh" "$@"
