#!/usr/bin/env bash
# Watch export_dir/active and fan-out to consumer guests after debounced changes.
set -euo pipefail

ENV_FILE="${SNAPSHOT_FANOUT_ENV:-/etc/simple-living/snapshot-fanout.env}"
if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi

: "${EXPORT_DIR:?EXPORT_DIR is required}"

PUSH_SCRIPT="${SNAPSHOT_FANOUT_PUSH_SCRIPT:-/usr/local/lib/simple-living/snapshot_fanout_push.sh}"
WATCH_DIR="${EXPORT_DIR}/active"
DEBOUNCE_SEC="${FANOUT_DEBOUNCE_SEC:-2}"

if ! command -v inotifywait >/dev/null 2>&1; then
  echo "snapshot fan-out watch: inotifywait not found (install inotify-tools)" >&2
  exit 1
fi

if [[ ! -x "${PUSH_SCRIPT}" ]]; then
  echo "snapshot fan-out watch: missing push script ${PUSH_SCRIPT}" >&2
  exit 1
fi

mkdir -p "${WATCH_DIR}"

echo "snapshot fan-out watch: initial push from ${WATCH_DIR}"
"${PUSH_SCRIPT}" || echo "snapshot fan-out watch: initial push failed (will retry on change)" >&2

echo "snapshot fan-out watch: listening on ${WATCH_DIR} (debounce ${DEBOUNCE_SEC}s)"
while inotifywait -r -e close_write,moved_to,create,delete "${WATCH_DIR}"; do
  sleep "${DEBOUNCE_SEC}"
  while inotifywait -r -e close_write,moved_to,create,delete --timeout 0.5 "${WATCH_DIR}" 2>/dev/null; do
    :
  done
  echo "snapshot fan-out watch: export change detected, pushing ..."
  if ! "${PUSH_SCRIPT}"; then
    echo "snapshot fan-out watch: push failed" >&2
  fi
done
