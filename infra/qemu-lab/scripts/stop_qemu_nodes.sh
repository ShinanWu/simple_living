#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VM_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)/vms"
SEED_HTTP_PID="${VM_DIR}/seed-http.pid"

if [[ ! -d "${VM_DIR}" ]]; then
  echo "No VM directory found: ${VM_DIR}"
  exit 0
fi

for pid_file in "${VM_DIR}"/*.pid; do
  [[ -e "${pid_file}" ]] || continue
  pid="$(cat "${pid_file}")"
  name="$(basename "${pid_file}" .pid)"
  if ps -p "${pid}" >/dev/null 2>&1; then
    echo "Stopping ${name} (pid ${pid})"
    kill "${pid}" || true
  fi
  rm -f "${pid_file}"
done

if [[ -f "${SEED_HTTP_PID}" ]]; then
  spid="$(cat "${SEED_HTTP_PID}")"
  if ps -p "${spid}" >/dev/null 2>&1; then
    echo "Stopping seed HTTP server (pid ${spid})"
    kill "${spid}" || true
  fi
  rm -f "${SEED_HTTP_PID}"
fi

echo "QEMU nodes stop signal sent."
