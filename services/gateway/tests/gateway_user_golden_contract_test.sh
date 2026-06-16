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

src="services/gateway/src/gateway_edge_server_main.cpp"

for forbidden in \
  "/api/v2/me/favorites/list" \
  "/api/v2/me/favorites/add" \
  "/api/v2/me/favorites/remove" \
  "/api/v2/me/history/*" \
  "/api/v2/auth/session/revoke" \
  "/api/v2/me/summary/get" \
  "/api/v2/health/check"; do
  if grep -Fq "$forbidden" "$src"; then
    echo "legacy route still registered: $forbidden" >&2
    exit 1
  fi
done

for required in \
  "/api/v2/me/favorites               => GetMeFavorites" \
  "/api/v2/me/favorites/*             => DeleteMeFavorite" \
  "/api/v2/me/history                 => MeHistoryHttp" \
  "/api/v2/auth/session               => DeleteAuthSession" \
  "/api/v2/health                     => GetHealth" \
  "/api/v2/me/summary                 => GetMeSummary"; do
  if ! grep -Fq "$required" "$src"; then
    echo "missing route mapping: $required" >&2
    exit 1
  fi
done

echo "gateway user golden contract test passed"
