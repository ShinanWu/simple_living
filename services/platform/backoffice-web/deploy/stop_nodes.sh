#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
LOCAL_QEMU_DIR="${LOCAL_QEMU_DIR:-${ROOT_DIR}/environments/local-qemu}"
VM_DIR="${LOCAL_QEMU_DIR}/vms"
SEED_HTTP_PID="${VM_DIR}/seed-http.pid"
[[ -d "${VM_DIR}" ]] || { echo "No VM directory found: ${VM_DIR}"; exit 0; }
pid_file="${VM_DIR}/build.pid"
if [[ -f "${pid_file}" ]]; then
  pid="$(cat "${pid_file}")"
  if ps -p "${pid}" >/dev/null 2>&1; then
    kill "${pid}" || true
  fi
  rm -f "${pid_file}"
fi
pid_file="${VM_DIR}/nginx.pid"
if [[ -f "${pid_file}" ]]; then
  pid="$(cat "${pid_file}")"
  if ps -p "${pid}" >/dev/null 2>&1; then
    kill "${pid}" || true
  fi
  rm -f "${pid_file}"
fi
if [[ -f "${SEED_HTTP_PID}" ]]; then
  running_left=0
  for f in "${VM_DIR}"/*.pid; do
    [[ -e "${f}" ]] || continue
    running_left=1
    break
  done
  if [[ "${running_left}" -eq 0 ]]; then
    spid="$(cat "${SEED_HTTP_PID}")"
    if ps -p "${spid}" >/dev/null 2>&1; then kill "${spid}" || true; fi
    rm -f "${SEED_HTTP_PID}"
  fi
fi
echo "Service-scoped stop done."
