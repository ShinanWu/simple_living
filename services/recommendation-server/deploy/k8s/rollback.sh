#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl delete -f "${ROOT_DIR}/services/recommendation-server/deploy/k8s/deployment-recommendation-server.yaml" --ignore-not-found
kubectl delete -f "${ROOT_DIR}/services/recommendation-server/deploy/k8s/service-recommendation-server.yaml" --ignore-not-found
