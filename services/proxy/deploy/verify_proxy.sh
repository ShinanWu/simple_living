#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/infra/lab/nodes.env}"
if [[ ! -f "${ENV_FILE}" ]]; then
  echo "ERROR: env file not found: ${ENV_FILE} (copy infra/lab/nodes.example.env)" >&2
  exit 1
fi

# shellcheck disable=SC1090
source "${ENV_FILE}"

: "${SSH_USER:?SSH_USER is required}"
: "${SSH_KEY_PATH:?SSH_KEY_PATH is required}"
: "${NODE_IP_NGINX:?NODE_IP_NGINX is required}"

SSH_PORT="${SSH_PORT:-22}"
NGINX_SSH_PORT="${NODE_SSH_PORT_NGINX:-${SSH_PORT}}"
KEY_PATH="${SSH_KEY_PATH/#\~/${HOME}}"

SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=12 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

echo "==> 1) Nginx 节点本机 HTTP /healthz（容器映射到宿主机 80）"
ssh "${SSH_OPTS[@]}" -p "${NGINX_SSH_PORT}" -i "${KEY_PATH}" "${SSH_USER}@${NODE_IP_NGINX}" \
  "curl -fsS -m 12 http://127.0.0.1/healthz"

if [[ "${ENABLE_FRP:-0}" == "1" && "${FRP_EMBEDDED:-0}" != "1" && -n "${FRP_SERVER_ADDR:-}" ]]; then
  if [[ -z "${FRP_AUTH_TOKEN:-}" ]]; then
    echo "SKIP 公网: FRP_AUTH_TOKEN 为空（与 VPS /etc/frp/frps.toml 中 auth.token 对齐后重试）" >&2
  else
    echo "==> 2) 经远端 frps 的 HTTP /healthz"
    PUB_HOST="${FRP_CUSTOM_DOMAIN:-${FRP_SERVER_ADDR}}"
    if [[ "${FRP_PROXY_TYPE:-http}" == "http" ]]; then
      curl -fsS -m 20 "http://${PUB_HOST}/healthz"
    else
      curl -fsS -m 20 "http://${FRP_SERVER_ADDR}:${FRP_REMOTE_PORT:-10080}/healthz"
    fi
  fi
else
  echo "SKIP 公网: ENABLE_FRP!=1 或 FRP_EMBEDDED=1 或未设置 FRP_SERVER_ADDR" >&2
fi

echo "==> 3) 容器内 frpc 日志（末尾，无则跳过）"
ssh "${SSH_OPTS[@]}" -p "${NGINX_SSH_PORT}" -i "${KEY_PATH}" "${SSH_USER}@${NODE_IP_NGINX}" \
  "if sudo -n podman exec simple-living-proxy true 2>/dev/null; then c='sudo -n podman'; elif sudo -n docker exec simple-living-proxy true 2>/dev/null; then c='sudo -n docker'; elif podman exec simple-living-proxy true 2>/dev/null; then c=podman; elif docker exec simple-living-proxy true 2>/dev/null; then c=docker; else echo '(无 simple-living-proxy 容器)'; exit 0; fi; \$c exec simple-living-proxy sh -c 'tail -20 /var/log/frp/frpc.log 2>/dev/null || echo no_frpc_log'"

echo "OK: verify_proxy.sh finished"
