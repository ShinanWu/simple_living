#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

REGISTRY="${REGISTRY:-}"
IMAGE_TAG="${IMAGE_TAG:-latest}"
CONTAINER_CLI="${CONTAINER_CLI:-auto}"
BASE_IMAGE="${BASE_IMAGE:-debian:bookworm-slim}"

if [[ -n "${REGISTRY}" ]]; then
  REGISTRY="${REGISTRY%/}"
fi

if [[ "${CONTAINER_CLI}" == "auto" ]]; then
  if command -v docker >/dev/null 2>&1; then
    CONTAINER_CLI="docker"
  elif command -v podman >/dev/null 2>&1; then
    CONTAINER_CLI="podman"
  else
    echo "Neither docker nor podman found in PATH." >&2
    exit 1
  fi
fi

if ! command -v "${CONTAINER_CLI}" >/dev/null 2>&1; then
  echo "${CONTAINER_CLI} is required but not found in PATH." >&2
  exit 1
fi

SERVICES=(
  "gateway:services/gateway/gateway_edge_server"
  "user-domain:services/user-domain/user_domain_server"
  "content-domain:services/content-domain/content_domain_server"
  "recommendation-domain:services/recommendation-domain/recommendation_domain_server"
  "affiliate-domain:services/affiliate-domain/affiliate_domain_server"
  "tracking-domain:services/tracking-domain/tracking_domain_server"
  "governance-domain:services/governance-domain/governance_domain_server"
)

for item in "${SERVICES[@]}"; do
  service_name="${item%%:*}"
  binary_relpath="${item#*:}"
  binary_path="${REPO_ROOT}/bazel-bin/${binary_relpath}"

  if [[ ! -f "${binary_path}" ]]; then
    echo "Missing binary: ${binary_path}" >&2
    echo "Please run bazel build for all service binaries first." >&2
    exit 1
  fi

  image_name="${service_name}:${IMAGE_TAG}"
  if [[ -n "${REGISTRY}" ]]; then
    image_name="${REGISTRY}/${image_name}"
  fi

  build_ctx="$(mktemp -d)"
  trap 'rm -rf "${build_ctx}"' EXIT

  cp "${binary_path}" "${build_ctx}/server"
  cp "${SCRIPT_DIR}/Dockerfile.service" "${build_ctx}/Dockerfile.service"

  echo "Building ${image_name} from ${binary_relpath}"
  "${CONTAINER_CLI}" build \
    --build-arg "BASE_IMAGE=${BASE_IMAGE}" \
    -f "${build_ctx}/Dockerfile.service" \
    -t "${image_name}" \
    "${build_ctx}"

  rm -rf "${build_ctx}"
  trap - EXIT
done

echo "All images built successfully with ${CONTAINER_CLI}."
