#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BASE_DIR="${ROOT_DIR}/environments/kubernetes/base"

if ! command -v kubectl >/dev/null 2>&1; then
  echo "kubectl not found in PATH" >&2
  exit 1
fi

echo "[1/2] Applying cluster base resources..."
kubectl apply -f "${BASE_DIR}/namespace.yaml"
kubectl apply -f "${BASE_DIR}/configmap-common.yaml"
kubectl apply -f "${BASE_DIR}/secret-template.yaml"

echo "[2/2] Current base summary..."
kubectl get ns simple-living
kubectl -n simple-living get configmap common-env
kubectl -n simple-living get secret app-secrets

echo "Base apply done. Service workloads are managed in services/<service>/deploy/k8s."
