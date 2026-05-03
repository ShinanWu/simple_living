#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
LAB_DIR="${LAB_DIR:-${ROOT_DIR}/infra/lab}"
VM_DIR="${LAB_DIR}/vms"
BASE_IMAGE="${VM_DIR}/base-jammy-arm64.img"
SEED_HTTP_DIR="${VM_DIR}/seed-http"
SEED_HTTP_PORT="${SEED_HTTP_PORT:-8081}"
SEED_HTTP_PID="${VM_DIR}/seed-http.pid"
BASE_URL="${BASE_URL:-https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-arm64.img}"
SSH_PUBKEY_PATH="${SSH_PUBKEY_PATH:-$HOME/.ssh/github_shinanwu_ed25519.pub}"
QEMU_BIN="${QEMU_BIN:-$(command -v qemu-system-aarch64 || true)}"
QEMU_IMG_BIN="${QEMU_IMG_BIN:-$(command -v qemu-img || true)}"
MEMORY_MB="${MEMORY_MB:-1536}"
CPU_CORES="${CPU_CORES:-2}"
DISK_SIZE="${DISK_SIZE:-60G}"
if [[ -z "${QEMU_BIN}" || -z "${QEMU_IMG_BIN}" ]]; then echo "qemu-system-aarch64 and qemu-img are required." >&2; exit 1; fi
if [[ ! -f "${SSH_PUBKEY_PATH}" ]]; then echo "SSH public key not found: ${SSH_PUBKEY_PATH}" >&2; exit 1; fi
mkdir -p "${VM_DIR}" "${SEED_HTTP_DIR}"
if [[ ! -f "${BASE_IMAGE}" ]]; then curl -fL "${BASE_URL}" -o "${BASE_IMAGE}"; fi
KEY_CONTENT="$(<"${SSH_PUBKEY_PATH}")"
FIRMWARE="/opt/homebrew/Cellar/qemu/10.2.2/share/qemu/edk2-aarch64-code.fd"
NODES=("build:2209::" "nginx:2208:18081:8088")

for item in "${NODES[@]}"; do
  IFS=':' read -r name ssh_port host_app_port guest_app_port <<<"${item}"
  disk="${VM_DIR}/${name}.qcow2"; seed_dir="${VM_DIR}/seed-${name}"; seed_iso="${VM_DIR}/seed-${name}.iso"; pid_file="${VM_DIR}/${name}.pid"; log_file="${VM_DIR}/${name}.log"
  if [[ -f "${pid_file}" ]] && ps -p "$(cat "${pid_file}")" >/dev/null 2>&1; then echo "Node ${name} already running."; continue; fi
  if [[ ! -f "${disk}" ]]; then "${QEMU_IMG_BIN}" create -f qcow2 -F qcow2 -b "${BASE_IMAGE}" "${disk}" "${DISK_SIZE}" >/dev/null; fi
  rm -rf "${seed_dir}" "${seed_iso}" "${SEED_HTTP_DIR:?}/${name}"; mkdir -p "${seed_dir}" "${SEED_HTTP_DIR}/${name}"
  cat > "${seed_dir}/meta-data" <<EOF
instance-id: simple-living-${name}
local-hostname: simple-living-${name}
EOF
  cat > "${seed_dir}/user-data" <<EOF
#cloud-config
users:
  - default
  - name: ubuntu
    sudo: ALL=(ALL) NOPASSWD:ALL
    groups: [adm, sudo]
    shell: /bin/bash
    ssh_authorized_keys:
      - ${KEY_CONTENT}
  - name: root
    ssh_authorized_keys:
      - ${KEY_CONTENT}
    lock_passwd: true
    shell: /bin/bash
ssh_pwauth: false
disable_root: false
package_update: true
packages:
  - podman
  - curl
  - git
runcmd:
  - [ bash, -lc, "mkdir -p /root/simple_living" ]
EOF
  cp "${seed_dir}/meta-data" "${SEED_HTTP_DIR}/${name}/meta-data"; cp "${seed_dir}/user-data" "${SEED_HTTP_DIR}/${name}/user-data"
  hdiutil makehybrid -o "${seed_iso}" "${seed_dir}" -hfs -joliet -iso -default-volume-name cidata >/dev/null
  netdev_args="user,id=net0,hostfwd=tcp::${ssh_port}-:22"; if [[ -n "${host_app_port}" && -n "${guest_app_port}" ]]; then netdev_args="${netdev_args},hostfwd=tcp::${host_app_port}-:${guest_app_port}"; fi
  "${QEMU_BIN}" -name "simple-living-${name}" -machine virt,accel=hvf -cpu host -smp "${CPU_CORES}" -m "${MEMORY_MB}" -bios "${FIRMWARE}" -smbios "type=1,serial=ds=nocloud-net;s=http://10.0.2.2:${SEED_HTTP_PORT}/${name}/" -drive if=virtio,file="${disk}",format=qcow2 -drive if=virtio,media=cdrom,file="${seed_iso}",format=raw -netdev "${netdev_args}" -device virtio-net-pci,netdev=net0 -display none -serial "file:${log_file}" -daemonize -pidfile "${pid_file}" -D "${VM_DIR}/${name}.qemu.log"
done
if [[ -f "${SEED_HTTP_PID}" ]] && ps -p "$(cat "${SEED_HTTP_PID}")" >/dev/null 2>&1; then :; else nohup python3 -m http.server "${SEED_HTTP_PORT}" --directory "${SEED_HTTP_DIR}" >/dev/null 2>&1 & echo $! > "${SEED_HTTP_PID}"; fi
echo "Service-scoped lab nodes started."
