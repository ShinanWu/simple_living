#!/usr/bin/env bash
set -euo pipefail

RUNTIME_DIR="${FRP_DESKTOP_RUNTIME_DIR:-${XDG_RUNTIME_DIR:-/tmp}/simple-living-frpc-desktop}"
PID_FILE="${RUNTIME_DIR}/frpc.pid"
PLIST_LABEL="com.simple-living.frpc-desktop"
LAUNCHD_TARGET="gui/$(id -u)/${PLIST_LABEL}"

if launchctl print "${LAUNCHD_TARGET}" >/dev/null 2>&1; then
  launchctl bootout "${LAUNCHD_TARGET}" 2>/dev/null || true
  echo "已卸载 LaunchAgent (${PLIST_LABEL})"
fi

if [[ ! -f "${PID_FILE}" ]]; then
  if pgrep -f "${RUNTIME_DIR}/frpc-desktop.toml" >/dev/null 2>&1; then
    pkill -f "${RUNTIME_DIR}/frpc-desktop.toml" || true
    echo "已停止桌面 frpc 进程"
  else
    echo "未找到运行中的桌面 frpc"
  fi
  exit 0
fi

PID="$(cat "${PID_FILE}")"
if kill -0 "${PID}" >/dev/null 2>&1; then
  kill "${PID}"
  echo "已停止桌面 frpc (pid=${PID})"
else
  echo "进程 ${PID} 已不存在"
fi
rm -f "${PID_FILE}"
