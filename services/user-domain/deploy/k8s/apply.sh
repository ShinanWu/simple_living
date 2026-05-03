#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl apply -f "${ROOT_DIR}/services/user-domain/deploy/k8s/service-user-domain.yaml"
kubectl apply -f "${ROOT_DIR}/services/user-domain/deploy/k8s/deployment-user-domain.yaml"
