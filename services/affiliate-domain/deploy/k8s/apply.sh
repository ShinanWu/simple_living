#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl apply -f "${ROOT_DIR}/services/affiliate-domain/deploy/k8s/service-affiliate-domain.yaml"
kubectl apply -f "${ROOT_DIR}/services/affiliate-domain/deploy/k8s/deployment-affiliate-domain.yaml"
