#!/usr/bin/env bash
set -euo pipefail

if command -v rpk >/dev/null 2>&1; then
  rpk cluster health --brokers 127.0.0.1:9092 | rg -q "Healthy"
else
  docker run --rm --network host docker.redpanda.com/redpandadata/redpanda:v24.2.11 \
    rpk cluster health --brokers 127.0.0.1:9092 | rg -q "Healthy"
fi

echo "kafka compatibility smoke test passed"
