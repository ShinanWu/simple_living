#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SRC_FILE="$ROOT_DIR/services/user-domain/src/pg_user_store.cpp"
HDR_FILE="$ROOT_DIR/services/user-domain/src/pg_user_store.h"

[[ -f "$SRC_FILE" ]] || { echo "missing source: $SRC_FILE"; exit 1; }
[[ -f "$HDR_FILE" ]] || { echo "missing header: $HDR_FILE"; exit 1; }

python3 - <<'PY'
from pathlib import Path

src = Path("services/user-domain/src/pg_user_store.cpp").read_text()
hdr = Path("services/user-domain/src/pg_user_store.h").read_text()

for token in [
    "void IssueTokenPair",
    "void EnsureGuestSession",
    "void GetMeSummary",
]:
    if token not in hdr:
        raise SystemExit(f"missing token in user-domain header: {token}")

for token in [
    "CREATE TABLE IF NOT EXISTS user_account",
    "CREATE TABLE IF NOT EXISTS user_session",
    "CREATE TABLE IF NOT EXISTS guest_device",
    "CREATE TABLE IF NOT EXISTS user_favorite",
    "CREATE TABLE IF NOT EXISTS user_history",
]:
    if token not in src:
        raise SystemExit(f"missing token in user-domain source: {token}")

# Token/session path must support issue + introspect + revoke.
for token in [
    'resp->set_access_token',
    'resp->set_refresh_token',
    'resp->set_session_id',
    'resp->set_valid(false)',
    'resp->set_valid(true)',
    'resp->set_revoked',
]:
    if token not in src:
        raise SystemExit(f"missing token/session behavior token: {token}")

# Guest + me summary path must remain wired.
for token in [
    'resp->set_created(false)',
    'resp->set_created(true)',
    'resp->mutable_counts()->set_favorites_count',
    'resp->mutable_counts()->set_history_count',
    'resp->mutable_profile()->CopyFrom',
]:
    if token not in src:
        raise SystemExit(f"missing guest/me-summary behavior token: {token}")
PY

echo "user-domain store smoke test passed"
