#!/usr/bin/env bash
set -euo pipefail

ENABLE_FRP="${ENABLE_FRP:-1}"
FRP_EMBEDDED="${FRP_EMBEDDED:-0}"
FRP_PROXY_TYPE="${FRP_PROXY_TYPE:-http}"
FRP_SERVER_ADDR="${FRP_SERVER_ADDR:-}"
FRP_SERVER_PORT="${FRP_SERVER_PORT:-7000}"
FRP_AUTH_TOKEN="${FRP_AUTH_TOKEN:-}"
FRP_USER="${FRP_USER:-phase1-local-gateway}"
FRP_PROXY_NAME="${FRP_PROXY_NAME:-gateway-http}"
FRP_LOCAL_IP="${FRP_LOCAL_IP:-127.0.0.1}"
FRP_LOCAL_PORT="${FRP_LOCAL_PORT:-80}"
FRP_CUSTOM_DOMAIN="${FRP_CUSTOM_DOMAIN:-}"
FRP_REMOTE_PORT="${FRP_REMOTE_PORT:-10080}"

FRP_PID=""
FRPS_PID=""

cleanup() {
  if [[ -n "${FRP_PID}" ]] && kill -0 "${FRP_PID}" >/dev/null 2>&1; then
    kill "${FRP_PID}" >/dev/null 2>&1 || true
  fi
  if [[ -n "${FRPS_PID}" ]] && kill -0 "${FRPS_PID}" >/dev/null 2>&1; then
    kill "${FRPS_PID}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT INT TERM

wait_frps_control() {
  local i=0
  while [[ "${i}" -lt 50 ]]; do
    if (echo >/dev/tcp/127.0.0.1/"${FRP_SERVER_PORT}") >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
    i=$((i + 1))
  done
  echo "[proxy] warning: frps control port ${FRP_SERVER_PORT} not accepting yet" >&2
}

start_embedded_frps() {
  if [[ -z "${FRP_AUTH_TOKEN}" ]]; then
    if command -v openssl >/dev/null 2>&1; then
      FRP_AUTH_TOKEN="$(openssl rand -hex 16)"
    else
      FRP_AUTH_TOKEN="$(head -c 24 /dev/urandom | base64 | tr -dc 'A-Za-z0-9' | head -c 32)"
    fi
    echo "[proxy] FRP_EMBEDDED=1: generated FRP_AUTH_TOKEN (set explicitly in prod)"
  fi

  cat > /tmp/frps.toml <<EOF
bindAddr = "0.0.0.0"
bindPort = ${FRP_SERVER_PORT}
auth.method = "token"
auth.token = "${FRP_AUTH_TOKEN}"
EOF

  frps -c /tmp/frps.toml >/var/log/frp/frps.log 2>&1 &
  FRPS_PID="$!"
  echo "[proxy] frps started (pid=${FRPS_PID}, control=${FRP_SERVER_PORT})"
  wait_frps_control
}

write_frpc_http() {
  cat > /tmp/frpc.toml <<EOF
serverAddr = "${FRP_SERVER_ADDR}"
serverPort = ${FRP_SERVER_PORT}

auth.method = "token"
auth.token = "${FRP_AUTH_TOKEN}"

user = "${FRP_USER}"

[[proxies]]
name = "${FRP_PROXY_NAME}"
type = "http"
localIP = "${FRP_LOCAL_IP}"
localPort = ${FRP_LOCAL_PORT}
customDomains = ["${FRP_CUSTOM_DOMAIN}"]
EOF
}

write_frpc_tcp() {
  cat > /tmp/frpc.toml <<EOF
serverAddr = "${FRP_SERVER_ADDR}"
serverPort = ${FRP_SERVER_PORT}

auth.method = "token"
auth.token = "${FRP_AUTH_TOKEN}"

user = "${FRP_USER}"

[[proxies]]
name = "${FRP_PROXY_NAME}-tcp"
type = "tcp"
localIP = "${FRP_LOCAL_IP}"
localPort = ${FRP_LOCAL_PORT}
remotePort = ${FRP_REMOTE_PORT}
EOF
}

start_frpc_background() {
  frpc -c /tmp/frpc.toml >/var/log/frp/frpc.log 2>&1 &
  FRP_PID="$!"
  echo "[proxy] frpc started (pid=${FRP_PID}, type=${FRP_PROXY_TYPE})"
}

start_frp_if_needed() {
  if [[ "${ENABLE_FRP}" != "1" ]]; then
    echo "[proxy] frp disabled (ENABLE_FRP=${ENABLE_FRP})"
    return
  fi

  if [[ "${FRP_EMBEDDED}" == "1" ]]; then
    FRP_SERVER_ADDR="127.0.0.1"
    if [[ "${FRP_PROXY_TYPE}" == "http" && -z "${FRP_CUSTOM_DOMAIN}" ]]; then
      FRP_PROXY_TYPE=tcp
    fi
    start_embedded_frps
    if [[ "${FRP_PROXY_TYPE}" == "tcp" ]]; then
      write_frpc_tcp
    else
      if [[ -z "${FRP_CUSTOM_DOMAIN}" ]]; then
        echo "[proxy] FRP_EMBEDDED=1 with FRP_PROXY_TYPE=http requires FRP_CUSTOM_DOMAIN" >&2
        exit 1
      fi
      write_frpc_http
    fi
    start_frpc_background
    echo "[proxy] embedded frp: public TCP entry on container port ${FRP_REMOTE_PORT} (map host -p ${FRP_REMOTE_PORT}:${FRP_REMOTE_PORT})"
    return
  fi

  if [[ -z "${FRP_SERVER_ADDR}" || -z "${FRP_AUTH_TOKEN}" ]]; then
    echo "[proxy] frp not started: set FRP_EMBEDDED=1 or provide FRP_SERVER_ADDR and FRP_AUTH_TOKEN"
    return
  fi

  if [[ "${FRP_PROXY_TYPE}" == "tcp" ]]; then
    write_frpc_tcp
    start_frpc_background
    return
  fi

  if [[ -z "${FRP_CUSTOM_DOMAIN}" ]]; then
    echo "[proxy] frp http mode not started: missing FRP_CUSTOM_DOMAIN (or use FRP_PROXY_TYPE=tcp)"
    return
  fi

  write_frpc_http
  start_frpc_background
}

start_frp_if_needed

exec nginx -g "daemon off;"
