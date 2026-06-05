#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl apply -f "${ROOT_DIR}/services/tracking-server/deploy/k8s/service-tracking-server.yaml"
kubectl apply -f "${ROOT_DIR}/services/tracking-server/deploy/k8s/deployment-tracking-server.yaml"
