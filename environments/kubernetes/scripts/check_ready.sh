#!/usr/bin/env bash
set -euo pipefail

NAMESPACE="${1:-simple-living}"
TIMEOUT="${TIMEOUT:-180s}"
DEPLOYMENTS="${DEPLOYMENTS:-}"

if ! command -v kubectl >/dev/null 2>&1; then
  echo "kubectl not found in PATH" >&2
  exit 1
fi

echo "Checking rollout status in namespace: ${NAMESPACE}"

if [[ -n "${DEPLOYMENTS}" ]]; then
  for dep in ${DEPLOYMENTS}; do
    kubectl -n "${NAMESPACE}" rollout status "deploy/${dep}" --timeout="${TIMEOUT}"
  done
else
  mapfile -t all_deployments < <(kubectl -n "${NAMESPACE}" get deploy -o jsonpath='{range .items[*]}{.metadata.name}{"\n"}{end}')
  for dep in "${all_deployments[@]}"; do
    kubectl -n "${NAMESPACE}" rollout status "deploy/${dep}" --timeout="${TIMEOUT}"
  done
fi

echo
echo "DNS/service quick checks:"
kubectl -n "${NAMESPACE}" get svc -o wide

echo
echo "Pods:"
kubectl -n "${NAMESPACE}" get pods -o wide

echo "Readiness check done."
