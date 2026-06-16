#!/usr/bin/env bash
# 为 FRP_CUSTOM_DOMAIN 签发/续期 Let's Encrypt 证书，写入 nginx 来宾 TLS 目录并重启 proxy 容器。
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ENV_FILE="${ENV_FILE:-${ROOT_DIR}/environments/local-qemu/nodes.env}"
# shellcheck disable=SC1090
source "${ENV_FILE}"

: "${SSH_USER:?SSH_USER is required}"
: "${SSH_KEY_PATH:?SSH_KEY_PATH is required}"
: "${NODE_IP_NGINX:?NODE_IP_NGINX is required}"
: "${FRP_CUSTOM_DOMAIN:?FRP_CUSTOM_DOMAIN is required}"

NGINX_SSH_PORT="${NODE_SSH_PORT_NGINX:-${SSH_PORT:-22}}"
TLS_HOST_DIR="${TLS_HOST_DIR:-/var/lib/simple-living/tls}"
ACME_HOST_DIR="${ACME_HOST_DIR:-/var/lib/simple-living/acme-webroot}"
CERTBOT_EMAIL="${CERTBOT_EMAIL:-shinanwu@126.com}"
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=20 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null)

ssh "${SSH_OPTS[@]}" -p "${NGINX_SSH_PORT}" -i "${SSH_KEY_PATH}" "${SSH_USER}@${NODE_IP_NGINX}" bash -s <<EOF
set -euo pipefail
TLS_HOST_DIR='${TLS_HOST_DIR}'
ACME_HOST_DIR='${ACME_HOST_DIR}'
DOMAIN='${FRP_CUSTOM_DOMAIN}'
EMAIL='${CERTBOT_EMAIL}'

sudo mkdir -p "\${TLS_HOST_DIR}" "\${ACME_HOST_DIR}"
sudo chmod 755 "\${ACME_HOST_DIR}"

if ! command -v certbot >/dev/null 2>&1; then
  sudo apt-get update -qq
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y certbot
fi

sudo certbot certonly --webroot -w "\${ACME_HOST_DIR}" -d "\${DOMAIN}" \
  --non-interactive --agree-tos -m "\${EMAIL}" \
  --preferred-challenges http

sudo cp "/etc/letsencrypt/live/\${DOMAIN}/fullchain.pem" "\${TLS_HOST_DIR}/fullchain.pem"
sudo cp "/etc/letsencrypt/live/\${DOMAIN}/privkey.pem" "\${TLS_HOST_DIR}/privkey.pem"
sudo chmod 644 "\${TLS_HOST_DIR}/fullchain.pem"
sudo chmod 600 "\${TLS_HOST_DIR}/privkey.pem"

if sudo -n podman inspect simple-living-proxy >/dev/null 2>&1; then
  ctr='sudo -n podman'
elif sudo -n docker inspect simple-living-proxy >/dev/null 2>&1; then
  ctr='sudo -n docker'
elif podman inspect simple-living-proxy >/dev/null 2>&1; then
  ctr=podman
else
  ctr=docker
fi

\$ctr restart simple-living-proxy
sleep 2
\$ctr inspect -f '{{.State.Status}}' simple-living-proxy
EOF

echo "TLS cert issued for ${FRP_CUSTOM_DOMAIN} and simple-living-proxy restarted."
