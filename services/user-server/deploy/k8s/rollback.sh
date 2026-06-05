#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl delete -f "${ROOT_DIR}/services/user-server/deploy/k8s/deployment-user-server.yaml" --ignore-not-found
kubectl delete -f "${ROOT_DIR}/services/user-server/deploy/k8s/service-user-server.yaml" --ignore-not-found
