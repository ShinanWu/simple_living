#!/usr/bin/env bash
# recommendation-server co-locates with backoffice-backend on NODE_IP_BACKOFFICE.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
exec bash "${ROOT_DIR}/services/platform/backoffice-backend/deploy/start_nodes.sh" "$@"
