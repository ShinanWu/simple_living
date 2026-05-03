#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl apply -f "${ROOT_DIR}/services/gateway/deploy/k8s/service-gateway.yaml"
kubectl apply -f "${ROOT_DIR}/services/gateway/deploy/k8s/deployment-gateway.yaml"
