#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"
# shellcheck disable=SC1091
source "${ROOT_DIR}/tools/lab_gateway_addrs.sh"

export DEPLOY_ROOT_DIR="${ROOT_DIR}"
export DEPLOY_SERVICE_NAME="gateway"
export DEPLOY_BAZEL_TARGET="//services/gateway:gateway_edge_server"
export DEPLOY_BAZEL_BIN_REL="services/gateway/gateway_edge_server"
export DEPLOY_DOCKERFILE_REL="services/gateway/deploy/Dockerfile.cpp-service"
export DEPLOY_TARGET_HOST="${NODE_IP_NGINX:-${NODE_IP_GATEWAY}}"
export DEPLOY_TARGET_SSH_PORT="${NODE_SSH_PORT_NGINX:-${NODE_SSH_PORT_GATEWAY:-${SSH_PORT:-22}}}"
export DEPLOY_BUILD_SSH_PORT="${NODE_SSH_PORT_BUILD:-${SSH_PORT:-22}}"
export DEPLOY_CONTAINER_NAME="simple-living-gateway"
export DEPLOY_DOCKER_NETWORK="bridge"
export DEPLOY_PORT_MAP="-p ${SERVICE_PORT_GATEWAY_HTTP:-8080}:${SERVICE_PORT_GATEWAY_HTTP:-8080}"
export DEPLOY_REMOTE_PREP_CMD="sudo mkdir -p /var/lib/simple-living/media/backoffice && sudo chmod 1777 /var/lib/simple-living/media/backoffice; "
export DEPLOY_EXTRA_RUN_ARGS="-e GATEWAY_USER_SERVER_ADDR=${GATEWAY_USER_SERVER_ADDR} -e GATEWAY_RECOMMENDATION_SERVER_ADDR=${GATEWAY_RECOMMENDATION_SERVER_ADDR} -e GATEWAY_TRACKING_SERVER_ADDR=${GATEWAY_TRACKING_SERVER_ADDR} -e GATEWAY_BACKOFFICE_BACKEND_ADDR=${GATEWAY_BACKOFFICE_BACKEND_ADDR} -e EXPORT_DIR=/var/lib/simple-living/exports -v /var/lib/simple-living/media:/var/lib/simple-living/media"
if [[ -z "${GATEWAY_MEDIA_PUBLIC_BASE_URL:-}" ]]; then
  if [[ "${ENABLE_INGRESS_HTTPS:-0}" == "1" && -n "${FRP_CUSTOM_DOMAIN:-}" ]]; then
    if [[ -n "${FRP_HTTPS_CUSTOM_DOMAIN:-}" ]]; then
      MEDIA_PUBLIC_BASE_URL="https://${FRP_HTTPS_CUSTOM_DOMAIN}"
    elif [[ "${FRP_CUSTOM_DOMAIN}" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
      MEDIA_PUBLIC_BASE_URL="https://$(echo "${FRP_CUSTOM_DOMAIN}" | tr '.' '-')nip.io"
    else
      MEDIA_PUBLIC_BASE_URL="https://${FRP_CUSTOM_DOMAIN}"
    fi
  else
    MEDIA_PUBLIC_BASE_URL="http://${FRP_CUSTOM_DOMAIN:-}"
  fi
else
  MEDIA_PUBLIC_BASE_URL="${GATEWAY_MEDIA_PUBLIC_BASE_URL}"
fi
export DEPLOY_SERVER_FLAGS="-gateway_backoffice_media_public_base_url=${MEDIA_PUBLIC_BASE_URL}"

exec bash "${ROOT_DIR}/tools/deploy_cpp_service.sh"
