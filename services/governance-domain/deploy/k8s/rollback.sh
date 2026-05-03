#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
kubectl delete -f "${ROOT_DIR}/services/governance-domain/deploy/k8s/deployment-governance-domain.yaml" --ignore-not-found
kubectl delete -f "${ROOT_DIR}/services/governance-domain/deploy/k8s/service-governance-domain.yaml" --ignore-not-found
