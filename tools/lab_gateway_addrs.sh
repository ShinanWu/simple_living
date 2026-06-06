#!/usr/bin/env bash
# Emit gateway → downstream brpc addresses for the local QEMU lab.
# shellcheck disable=SC2034
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "${ROOT_DIR}/environments/local-qemu/lab-ports.env"

host="${LAB_QEMU_GATEWAY_HOST:-10.0.2.2}"
# brpc::Channel::Init expects host:port for literal IPs (no brpc:// scheme).
export GATEWAY_USER_SERVER_ADDR="${host}:${SERVICE_PORT_USER_SERVER}"
export GATEWAY_RECOMMENDATION_SERVER_ADDR="${host}:${SERVICE_PORT_RECOMMENDATION_SERVER}"
export GATEWAY_TRACKING_SERVER_ADDR="${host}:${SERVICE_PORT_TRACKING_SERVER}"
export GATEWAY_BACKOFFICE_BACKEND_ADDR="${host}:${SERVICE_PORT_BACKOFFICE_BACKEND}"
