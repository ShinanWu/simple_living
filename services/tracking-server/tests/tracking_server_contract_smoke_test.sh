#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${TEST_SRCDIR:-}" ]]; then
  if [[ -d "${TEST_SRCDIR}/_main" ]]; then
    cd "${TEST_SRCDIR}/_main"
  else
    cd "${TEST_SRCDIR}"
  fi
else
  cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
fi

src="services/tracking-server/src/tracking_server_main.cpp"

if [[ ! -f "$src" ]]; then
  echo "missing source: $src" >&2
  exit 1
fi

if ! grep -Fq 'https://go.shaotang.com/r/' "$src"; then
  echo "missing landing_url fallback https://go.shaotang.com/r/{short_token}" >&2
  exit 1
fi

if ! grep -Fq 'candidate.rfind("https://", 0) == 0' "$src"; then
  echo "AssembleTrackingLink must enforce HTTPS landing_url (contract §3.4)" >&2
  exit 1
fi

if ! grep -Fq '!store_->InsertLink(lr)' "$src"; then
  echo "missing InsertLink persistence in AssembleTrackingLink" >&2
  exit 1
fi

insert_block="$(grep -A6 '!store_->InsertLink(lr)' "$src" || true)"
if ! echo "$insert_block" | grep -q 'SetFailed'; then
  echo "InsertLink failure must call SetFailed (FAILED_PRECONDITION), not silent return" >&2
  exit 1
fi

if ! echo "$insert_block" | grep -q 'InsertLink failed'; then
  echo "InsertLink failure must set clear error message" >&2
  exit 1
fi

echo "tracking-server contract smoke test passed"
