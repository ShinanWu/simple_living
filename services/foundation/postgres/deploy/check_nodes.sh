#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "${ROOT_DIR}/environments/local-qemu/nodes.env" 2>/dev/null || true

ssh_port="${NODE_SSH_PORT_FOUNDATION:-2211}"
ssh_opts=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5)

echo "Checking foundation node on ssh port ${ssh_port}..."
ssh "${ssh_opts[@]}" -p "${ssh_port}" ubuntu@127.0.0.1 "echo foundation-ready && command -v podman"

echo "Checking forwarded ports..."
for port in "${FOUNDATION_POSTGRES_HOST_PORT:-15432}" "${FOUNDATION_REDIS_HOST_PORT:-16379}" "${FOUNDATION_KAFKA_HOST_PORT:-19092}"; do
  if nc -z 127.0.0.1 "${port}"; then
    echo "127.0.0.1:${port} reachable"
  else
    echo "127.0.0.1:${port} not reachable yet"
  fi
done
