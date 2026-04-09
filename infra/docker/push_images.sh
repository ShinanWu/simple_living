#!/usr/bin/env bash

set -euo pipefail

REGISTRY="${REGISTRY:-}"
IMAGE_TAG="${IMAGE_TAG:-latest}"
CONTAINER_CLI="${CONTAINER_CLI:-auto}"

if [[ -z "${REGISTRY}" ]]; then
  echo "REGISTRY is required for push, for example: REGISTRY=registry.example.com/simple-living" >&2
  exit 1
fi

REGISTRY="${REGISTRY%/}"

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
  "gateway"
  "user-domain"
  "content-domain"
  "recommendation-domain"
  "affiliate-domain"
  "tracking-domain"
  "governance-domain"
)

for service_name in "${SERVICES[@]}"; do
  image_name="${REGISTRY}/${service_name}:${IMAGE_TAG}"
  echo "Pushing ${image_name}"
  "${CONTAINER_CLI}" push "${image_name}"
done

echo "All images pushed successfully with ${CONTAINER_CLI}."
