#!/usr/bin/env bash
set -euo pipefail

# Roll the proxy (nginx + frp) back to a previously built image tag.
# Usage:
#   bash services/proxy/deploy/rollback.sh <image_tag>
#   IMAGE_TAG=v2026.04.30 bash services/proxy/deploy/rollback.sh
#
# This is a thin, explicit wrapper over deploy_service.sh: it re-deploys the
# requested image tag and then runs verify_proxy.sh as a post-rollback check.
# Secrets (FRP_AUTH_TOKEN, etc.) are still injected via the caller's env, never
# read from the repo.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TARGET_TAG="${1:-${IMAGE_TAG:-}}"
if [[ -z "${TARGET_TAG}" ]]; then
  echo "ERROR: rollback target image tag is required." >&2
  echo "Usage: bash services/proxy/deploy/rollback.sh <image_tag>" >&2
  echo "   or: IMAGE_TAG=<image_tag> bash services/proxy/deploy/rollback.sh" >&2
  exit 1
fi

if [[ "${TARGET_TAG}" == "latest" ]]; then
  echo "WARNING: rolling back to 'latest' defeats the purpose; pass a pinned historical tag." >&2
fi

echo "==> Rolling proxy back to image tag: ${TARGET_TAG}"
IMAGE_TAG="${TARGET_TAG}" bash "${SCRIPT_DIR}/deploy_service.sh"

echo "==> Post-rollback ingress verification"
if ! bash "${SCRIPT_DIR}/verify_proxy.sh"; then
  echo "ERROR: post-rollback verification failed; inspect nginx/frpc logs (see docs/README.md §6 logs)." >&2
  exit 2
fi

echo "OK: proxy rolled back to ${TARGET_TAG} and verified."
