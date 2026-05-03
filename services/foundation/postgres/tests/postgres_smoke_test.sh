#!/usr/bin/env bash
set -euo pipefail

if command -v pg_isready >/dev/null 2>&1; then
  pg_isready -h 127.0.0.1 -p 5432 -U simple -d simple_living
else
  docker run --rm --network host postgres:16-alpine \
    pg_isready -h 127.0.0.1 -p 5432 -U simple -d simple_living
fi

echo "postgres smoke test passed"
