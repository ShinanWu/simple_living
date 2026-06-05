#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl apply -f "${ROOT_DIR}/services/user-server/deploy/k8s/service-user-server.yaml"
kubectl apply -f "${ROOT_DIR}/services/user-server/deploy/k8s/deployment-user-server.yaml"
