#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-}"
if [[ -z "${BASE_URL}" ]]; then
  echo "Usage: $0 <base_url>  (example: https://api.example.com)" >&2
  exit 1
fi

echo "Checking ingress endpoint: ${BASE_URL}"

TARGET_URL="${BASE_URL%/}/healthz"
if ! curl -fsS -m 10 -o /tmp/simple_living_healthz.json -H "Accept: application/json" "${TARGET_URL}"; then
  TARGET_URL="${BASE_URL%/}/api/v2/health/check"
  if ! curl -fsS -m 10 -X POST -o /tmp/simple_living_healthz.json -H "Accept: application/json" -H "Content-Type: application/json" -d "{}" "${TARGET_URL}"; then
    TARGET_URL="${BASE_URL%/}/api/v2/health"
    curl -fsS -m 10 -o /tmp/simple_living_healthz.json -H "Accept: application/json" "${TARGET_URL}" || {
      echo "Health endpoint check failed: ${BASE_URL%/}/healthz, ${BASE_URL%/}/api/v2/health/check, and ${BASE_URL%/}/api/v2/health" >&2
      exit 1
    }
  fi
fi

echo "Health endpoint reachable at: ${TARGET_URL}"
echo "Response preview:"
python3 - <<'PY'
import json
from pathlib import Path

p = Path("/tmp/simple_living_healthz.json")
text = p.read_text(encoding="utf-8", errors="ignore").strip()
try:
    obj = json.loads(text)
    print(json.dumps(obj, ensure_ascii=False, indent=2)[:1200])
except Exception:
    print(text[:1200])
PY

echo "Ingress check done."
