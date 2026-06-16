#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi

FRP_SERVER_ADDR="${FRP_SERVER_ADDR:-}"
FRP_SERVER_PORT="${FRP_SERVER_PORT:-7000}"
FRP_AUTH_TOKEN="${FRP_AUTH_TOKEN:-}"
FRP_DESKTOP_USER="${FRP_DESKTOP_USER:-mac-desktop}"
FRP_DESKTOP_PROXY_NAME="${FRP_DESKTOP_PROXY_NAME:-desktop-vnc}"
FRP_DESKTOP_LOCAL_IP="${FRP_DESKTOP_LOCAL_IP:-127.0.0.1}"
FRP_DESKTOP_VNC_LOCAL_PORT="${FRP_DESKTOP_VNC_LOCAL_PORT:-5900}"
FRP_DESKTOP_VNC_REMOTE_PORT="${FRP_DESKTOP_VNC_REMOTE_PORT:-15900}"

RUNTIME_DIR="${FRP_DESKTOP_RUNTIME_DIR:-${XDG_RUNTIME_DIR:-/tmp}/simple-living-frpc-desktop}"
PID_FILE="${RUNTIME_DIR}/frpc.pid"
CONFIG_FILE="${RUNTIME_DIR}/frpc-desktop.toml"
LOG_FILE="${RUNTIME_DIR}/frpc.log"

usage() {
  cat <<'EOF'
用法：在 Mac 上启动桌面 VNC 的 frpc 隧道（独立于 gateway 容器 frpc）。

前置：
  1. 系统设置 → 通用 → 共享 → 屏幕共享（开启并设置强密码）
  2. 远端 frps 已运行，且防火墙放行 FRP_DESKTOP_VNC_REMOTE_PORT（默认 15900）
  3. FRP_AUTH_TOKEN 与 frps auth.token 一致（仅通过环境变量传入，勿写入 nodes.env）

示例：
  FRP_AUTH_TOKEN=your_token \
  FRP_SERVER_ADDR=8.152.103.12 \
  bash services/proxy/deploy/start_desktop_frpc.sh

可选环境变量：FRP_DESKTOP_VNC_LOCAL_PORT、FRP_DESKTOP_VNC_REMOTE_PORT、FRP_DESKTOP_USER

持久化（LaunchAgent，登录后自动重连）：
  bash services/proxy/deploy/start_desktop_frpc.sh --launchd
EOF
}

install_launchd() {
  local frpc_bin plist_dst plist_src
  frpc_bin="$(command -v frpc)"
  plist_src="${ROOT_DIR}/services/proxy/deploy/com.simple-living.frpc-desktop.plist.example"
  plist_dst="${HOME}/Library/LaunchAgents/com.simple-living.frpc-desktop.plist"
  mkdir -p "${HOME}/Library/LaunchAgents"
  sed \
    -e "s|__FRPC_CONFIG__|${CONFIG_FILE}|g" \
    -e "s|__FRPC_LOG__|${LOG_FILE}|g" \
    -e "s|/opt/homebrew/bin/frpc|${frpc_bin}|g" \
    "${plist_src}" > "${plist_dst}"
  launchctl bootout "gui/$(id -u)/com.simple-living.frpc-desktop" 2>/dev/null || true
  launchctl bootstrap "gui/$(id -u)" "${plist_dst}"
  launchctl enable "gui/$(id -u)/com.simple-living.frpc-desktop"
  launchctl kickstart -k "gui/$(id -u)/com.simple-living.frpc-desktop"
  sleep 1
  if pgrep -f "frpc -c ${CONFIG_FILE}" >/dev/null 2>&1; then
    pgrep -f "frpc -c ${CONFIG_FILE}" | head -1 > "${PID_FILE}"
    echo "LaunchAgent 已安装并启动 (plist=${plist_dst})"
  else
    echo "ERROR: LaunchAgent 启动失败，查看 ${LOG_FILE}" >&2
    exit 1
  fi
}

USE_LAUNCHD=0
if [[ "${1:-}" == "--launchd" ]]; then
  USE_LAUNCHD=1
elif [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ -z "${FRP_SERVER_ADDR}" || -z "${FRP_AUTH_TOKEN}" ]]; then
  echo "ERROR: 需要 FRP_SERVER_ADDR 与 FRP_AUTH_TOKEN（见 services/proxy/README.md §8）" >&2
  usage >&2
  exit 1
fi

if ! command -v frpc >/dev/null 2>&1; then
  echo "ERROR: 未找到 frpc，请先安装：brew install frp" >&2
  exit 1
fi

if [[ -f "${PID_FILE}" ]] && kill -0 "$(cat "${PID_FILE}")" >/dev/null 2>&1; then
  echo "frpc 已在运行 (pid=$(cat "${PID_FILE}"))，日志: ${LOG_FILE}"
  exit 0
fi

mkdir -p "${RUNTIME_DIR}"
chmod 700 "${RUNTIME_DIR}"

cat > "${CONFIG_FILE}" <<EOF
serverAddr = "${FRP_SERVER_ADDR}"
serverPort = ${FRP_SERVER_PORT}

auth.method = "token"
auth.token = "${FRP_AUTH_TOKEN}"

user = "${FRP_DESKTOP_USER}"

[[proxies]]
name = "${FRP_DESKTOP_PROXY_NAME}"
type = "tcp"
localIP = "${FRP_DESKTOP_LOCAL_IP}"
localPort = ${FRP_DESKTOP_VNC_LOCAL_PORT}
remotePort = ${FRP_DESKTOP_VNC_REMOTE_PORT}
EOF
chmod 600 "${CONFIG_FILE}"

if [[ "${USE_LAUNCHD}" == "1" ]]; then
  install_launchd
  echo "  本地 VNC: ${FRP_DESKTOP_LOCAL_IP}:${FRP_DESKTOP_VNC_LOCAL_PORT}"
  echo "  公网入口: ${FRP_SERVER_ADDR}:${FRP_DESKTOP_VNC_REMOTE_PORT}"
  echo "  日志: ${LOG_FILE}"
  echo "  停止: bash services/proxy/deploy/stop_desktop_frpc.sh"
  exit 0
fi

nohup frpc -c "${CONFIG_FILE}" >>"${LOG_FILE}" 2>&1 &
FRPC_PID=$!
disown "${FRPC_PID}" 2>/dev/null || true
echo "${FRPC_PID}" > "${PID_FILE}"

sleep 0.5
if ! kill -0 "$(cat "${PID_FILE}")" >/dev/null 2>&1; then
  echo "ERROR: frpc 启动失败，查看 ${LOG_FILE}" >&2
  tail -20 "${LOG_FILE}" >&2 || true
  rm -f "${PID_FILE}"
  exit 1
fi

echo "桌面 VNC frpc 已启动 (pid=$(cat "${PID_FILE}"))"
echo "  本地 VNC: ${FRP_DESKTOP_LOCAL_IP}:${FRP_DESKTOP_VNC_LOCAL_PORT}"
echo "  公网入口: ${FRP_SERVER_ADDR}:${FRP_DESKTOP_VNC_REMOTE_PORT}"
echo "  日志: ${LOG_FILE}"
echo "  停止: bash services/proxy/deploy/stop_desktop_frpc.sh"
echo "  持久化: bash services/proxy/deploy/start_desktop_frpc.sh --launchd"
