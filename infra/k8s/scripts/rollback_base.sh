#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BASE_DIR="${ROOT_DIR}/infra/k8s/base"
DELETE_NAMESPACE="${DELETE_NAMESPACE:-false}"

if ! command -v kubectl >/dev/null 2>&1; then
  echo "kubectl not found in PATH" >&2
  exit 1
fi

echo "Rolling back base resources..."
kubectl delete -f "${BASE_DIR}/deployment-gateway.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/deployment-governance-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/deployment-tracking-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/deployment-affiliate-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/deployment-recommendation-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/deployment-content-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/deployment-user-domain.yaml" --ignore-not-found

kubectl delete -f "${BASE_DIR}/service-gateway.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/service-governance-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/service-tracking-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/service-affiliate-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/service-recommendation-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/service-content-domain.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/service-user-domain.yaml" --ignore-not-found

kubectl delete -f "${BASE_DIR}/secret-template.yaml" --ignore-not-found
kubectl delete -f "${BASE_DIR}/configmap-common.yaml" --ignore-not-found

if [[ "${DELETE_NAMESPACE}" == "true" ]]; then
  kubectl delete -f "${BASE_DIR}/namespace.yaml" --ignore-not-found
fi

echo "Rollback done. DELETE_NAMESPACE=${DELETE_NAMESPACE}"
