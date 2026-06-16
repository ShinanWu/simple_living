#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi

FRP_SERVER_ADDR="${FRP_SERVER_ADDR:-}"
FRP_DESKTOP_VNC_LOCAL_PORT="${FRP_DESKTOP_VNC_LOCAL_PORT:-5900}"
FRP_DESKTOP_VNC_REMOTE_PORT="${FRP_DESKTOP_VNC_REMOTE_PORT:-15900}"
RUNTIME_DIR="${FRP_DESKTOP_RUNTIME_DIR:-${XDG_RUNTIME_DIR:-/tmp}/simple-living-frpc-desktop}"
PID_FILE="${RUNTIME_DIR}/frpc.pid"
LOG_FILE="${RUNTIME_DIR}/frpc.log"

echo "==> 1) 本地屏幕共享端口 ${FRP_DESKTOP_VNC_LOCAL_PORT}"
if (echo >/dev/tcp/127.0.0.1/"${FRP_DESKTOP_VNC_LOCAL_PORT}") >/dev/null 2>&1; then
  echo "OK: 127.0.0.1:${FRP_DESKTOP_VNC_LOCAL_PORT} 可连接（屏幕共享已监听）"
else
  echo "FAIL: 127.0.0.1:${FRP_DESKTOP_VNC_LOCAL_PORT} 不可达" >&2
  echo "      请在 系统设置 → 通用 → 共享 中开启「屏幕共享」并设置强密码" >&2
  exit 1
fi

echo "==> 2) 桌面 frpc 进程"
FRPC_PID=""
if [[ -f "${PID_FILE}" ]]; then
  FRPC_PID="$(cat "${PID_FILE}")"
fi
if [[ -n "${FRPC_PID}" ]] && kill -0 "${FRPC_PID}" >/dev/null 2>&1; then
  echo "OK: frpc 运行中 (pid=${FRPC_PID})"
elif pgrep -f "frpc -c ${RUNTIME_DIR}/frpc-desktop.toml" >/dev/null 2>&1; then
  FRPC_PID="$(pgrep -f "frpc -c ${RUNTIME_DIR}/frpc-desktop.toml" | head -1)"
  echo "OK: frpc 运行中 (pid=${FRPC_PID})"
else
  echo "FAIL: 桌面 frpc 未运行，先执行 start_desktop_frpc.sh" >&2
  exit 1
fi

if [[ -f "${LOG_FILE}" ]]; then
  if rg -q "login to server success" "${LOG_FILE}" 2>/dev/null; then
    echo "OK: frpc 已成功登录 frps"
  else
    echo "WARN: 日志中未见 login to server success，请检查 ${LOG_FILE}" >&2
  fi
fi

if [[ -n "${FRP_SERVER_ADDR}" ]]; then
  echo "==> 3) 公网 TCP 入口 ${FRP_SERVER_ADDR}:${FRP_DESKTOP_VNC_REMOTE_PORT}"
  if python3 -c "import socket; s=socket.socket(); s.settimeout(5); r=s.connect_ex(('${FRP_SERVER_ADDR}', ${FRP_DESKTOP_VNC_REMOTE_PORT})); s.close(); raise SystemExit(0 if r == 0 else 1)" 2>/dev/null; then
    echo "OK: 公网端口可连接（VNC 客户端可连此地址）"
  else
    echo "WARN: 公网端口暂不可达，请在 frps VPS 安全组/防火墙放行 TCP ${FRP_DESKTOP_VNC_REMOTE_PORT}" >&2
  fi
else
  echo "SKIP 公网: 未设置 FRP_SERVER_ADDR"
fi

echo "OK: verify_desktop_vnc.sh finished"
