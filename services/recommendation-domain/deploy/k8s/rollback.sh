#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl delete -f "${ROOT_DIR}/services/recommendation-domain/deploy/k8s/deployment-recommendation-domain.yaml" --ignore-not-found
kubectl delete -f "${ROOT_DIR}/services/recommendation-domain/deploy/k8s/service-recommendation-domain.yaml" --ignore-not-found
