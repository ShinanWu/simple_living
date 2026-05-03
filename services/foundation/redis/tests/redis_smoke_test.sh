#!/usr/bin/env bash
set -euo pipefail

if command -v redis-cli >/dev/null 2>&1; then
  redis-cli -h 127.0.0.1 -p 6379 ping | rg -q "^PONG$"
else
  docker run --rm --network host redis:7-alpine \
    redis-cli -h 127.0.0.1 -p 6379 ping | rg -q "^PONG$"
fi

echo "redis smoke test passed"
