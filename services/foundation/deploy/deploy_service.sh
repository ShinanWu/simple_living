#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ACTION="${1:-up}"
FOUNDATION_DIR="${ROOT_DIR}/services/foundation"

usage() {
  echo "usage: $0 [up|down|down-v|health]" >&2
  echo "  up      — start postgres + redis + kafka containers on foundation guest" >&2
  echo "  down    — stop all foundation containers (keep volumes)" >&2
  echo "  down-v  — stop all foundation containers and remove volumes" >&2
  echo "  health  — run services/foundation/scripts/health_check.sh" >&2
  echo "" >&2
  echo "Per-component lifecycle: services/foundation/<component>/deploy/deploy_service.sh" >&2
  exit 1
}

run_component() {
  local component="$1"
  local subaction="$2"
  bash "${FOUNDATION_DIR}/${component}/deploy/deploy_service.sh" "${subaction}"
}

case "${ACTION}" in
  up)
    run_component postgres up
    run_component redis up
    run_component kafka up
    ;;
  down)
    run_component kafka down
    run_component redis down
    run_component postgres down
    ;;
  down-v)
    run_component kafka down-v
    run_component redis down-v
    run_component postgres down-v
    ;;
  health)
    bash "${FOUNDATION_DIR}/scripts/health_check.sh"
    ;;
  *)
    usage
    ;;
esac
