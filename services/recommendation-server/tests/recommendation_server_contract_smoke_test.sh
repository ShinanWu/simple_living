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

python3 - <<'PY'
from pathlib import Path

catalog_src = Path("services/recommendation-server/src/catalog_read_service.cpp").read_text()
api_doc = Path("services/recommendation-server/api.md").read_text()
fixture = Path("services/recommendation-server/tests/fixtures/catalog.json")

for token in [
    "has_theme_id()",
    "theme_id().empty()",
    "CardHasThemeId",
    "theme_ids",
    "filter_by_theme",
    # cursor pagination on ListGuideCards
    "req->cursor()",
    "set_next_cursor",
    "set_has_more",
    # ListThemes must expose the canonical four themes (not theme_clothing)
    '"theme_1", "clothing"',
    '"theme_4", "transport"',
]:
    if token not in catalog_src:
        raise SystemExit(f"missing token in catalog_read_service.cpp: {token}")

if "theme_clothing" in catalog_src:
    raise SystemExit("ListThemes must use canonical theme_1..4, not legacy theme_clothing")

for token in [
    "ListGuideCards",
    "BatchGetGuideCards",
    "theme_1",
    "clothing",
    "theme_2",
    "food",
    "theme_3",
    "housing",
    "theme_4",
    "transport",
    "可见且已发布",
    "limit",
]:
    if token not in api_doc:
        raise SystemExit(f"missing token in api.md: {token}")

if not fixture.is_file():
    raise SystemExit("missing fixture: services/recommendation-server/tests/fixtures/catalog.json")

import json
body = fixture.read_text()
for theme_id in ("theme_1", "theme_2", "theme_3", "theme_4"):
    if theme_id not in body:
        raise SystemExit(f"fixture catalog.json missing card for {theme_id}")

fixture_cards = json.loads(body).get("cards", [])
for card in fixture_cards:
    refs = card.get("affiliate_refs") or []
    landing = refs[0].get("payload", {}).get("landing_url") if refs else ""
    if not landing:
        raise SystemExit(f"fixture card {card.get('card_id')} missing affiliate_refs[].payload.landing_url")

print("recommendation-server contract smoke ok")
PY
