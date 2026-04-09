#!/usr/bin/env bash
set -euo pipefail

NAMESPACE="${1:-simple-living}"
TIMEOUT="${TIMEOUT:-180s}"

if ! command -v kubectl >/dev/null 2>&1; then
  echo "kubectl not found in PATH" >&2
  exit 1
fi

echo "Checking rollout status in namespace: ${NAMESPACE}"
kubectl -n "${NAMESPACE}" rollout status deploy/user-domain --timeout="${TIMEOUT}"
kubectl -n "${NAMESPACE}" rollout status deploy/content-domain --timeout="${TIMEOUT}"
kubectl -n "${NAMESPACE}" rollout status deploy/recommendation-domain --timeout="${TIMEOUT}"
kubectl -n "${NAMESPACE}" rollout status deploy/affiliate-domain --timeout="${TIMEOUT}"
kubectl -n "${NAMESPACE}" rollout status deploy/tracking-domain --timeout="${TIMEOUT}"
kubectl -n "${NAMESPACE}" rollout status deploy/governance-domain --timeout="${TIMEOUT}"
kubectl -n "${NAMESPACE}" rollout status deploy/gateway --timeout="${TIMEOUT}"

echo
echo "DNS/service quick checks:"
kubectl -n "${NAMESPACE}" get svc user-domain content-domain recommendation-domain affiliate-domain tracking-domain governance-domain gateway -o wide

echo
echo "Pods:"
kubectl -n "${NAMESPACE}" get pods -o wide

echo "Readiness check done."
