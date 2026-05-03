#!/usr/bin/env bash
set -euo pipefail

TARGET="${1:-}"
if [[ -z "${TARGET}" ]]; then
  echo "usage: $0 <https://domain-or-ip>" >&2
  exit 1
fi

bash "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/src/scripts/check_ingress.sh" "${TARGET}"
echo "proxy ingress smoke test passed"
