#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BASE_DIR="${ROOT_DIR}/infra/k8s/base"

if ! command -v kubectl >/dev/null 2>&1; then
  echo "kubectl not found in PATH" >&2
  exit 1
fi

echo "[1/4] Applying namespace/config..."
kubectl apply -f "${BASE_DIR}/namespace.yaml"
kubectl apply -f "${BASE_DIR}/configmap-common.yaml"
kubectl apply -f "${BASE_DIR}/secret-template.yaml"

echo "[2/4] Applying services..."
kubectl apply -f "${BASE_DIR}/service-user-domain.yaml"
kubectl apply -f "${BASE_DIR}/service-content-domain.yaml"
kubectl apply -f "${BASE_DIR}/service-recommendation-domain.yaml"
kubectl apply -f "${BASE_DIR}/service-affiliate-domain.yaml"
kubectl apply -f "${BASE_DIR}/service-tracking-domain.yaml"
kubectl apply -f "${BASE_DIR}/service-governance-domain.yaml"
kubectl apply -f "${BASE_DIR}/service-gateway.yaml"

echo "[3/4] Applying deployments..."
kubectl apply -f "${BASE_DIR}/deployment-user-domain.yaml"
kubectl apply -f "${BASE_DIR}/deployment-content-domain.yaml"
kubectl apply -f "${BASE_DIR}/deployment-recommendation-domain.yaml"
kubectl apply -f "${BASE_DIR}/deployment-affiliate-domain.yaml"
kubectl apply -f "${BASE_DIR}/deployment-tracking-domain.yaml"
kubectl apply -f "${BASE_DIR}/deployment-governance-domain.yaml"
kubectl apply -f "${BASE_DIR}/deployment-gateway.yaml"

echo "[4/4] Current workload summary..."
kubectl -n simple-living get svc,deploy,pod

echo "Apply done."
