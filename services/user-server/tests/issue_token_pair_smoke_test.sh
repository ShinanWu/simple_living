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

store=services/user-server/src/pg_user_store.cpp
grep -q 'test_phone_verification' "$store"
grep -q 'usr_wechat_' "$store"
grep -q 'EXTRACT(EPOCH FROM last_seen_at)' "$store"
grep -q 'access_expires_at' "$store"

# ListHistory / ListFavorites must be paginated (offset cursor + secondary sort)
grep -q 'ResolvePaginationWindow' "$store"
grep -q 'last_seen_at DESC NULLS LAST, content_id ASC' "$store"
grep -q 'favorited_at DESC, favorite_id ASC' "$store"

# GetMeSummary must count guest-session history via s:<session_id> owner key
grep -q 's:") + req.session_id()' "$store" \
  || { echo "GetMeSummary/history must support guest session owner key" >&2; exit 1; }

if command -v pg_isready >/dev/null 2>&1; then
  if pg_isready -h 127.0.0.1 -p 5432 -U simple -d simple_living >/dev/null 2>&1; then
    UNIT_TEST="$(find "${TEST_SRCDIR:-.}" -name pg_user_store_unit_test -type f 2>/dev/null | head -1 || true)"
    if [[ -n "${UNIT_TEST}" && -x "${UNIT_TEST}" ]]; then
      "${UNIT_TEST}"
    fi
  fi
fi

echo "issue_token_pair_smoke_test passed"
