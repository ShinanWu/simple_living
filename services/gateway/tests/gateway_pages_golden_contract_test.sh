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

# guide_detail must output theme slug (clothing/...), not internal theme_id
if ! grep -Fq "detail->set_theme(bo::ThemeFromThemeId(card.theme_ids(0)));" "$src"; then
  echo "guide_detail theme must be slug via bo::ThemeFromThemeId" >&2
  exit 1
fi
if grep -Fq "detail->set_theme(card.theme_ids(0));" "$src"; then
  echo "guide_detail still leaks internal theme_id" >&2
  exit 1
fi

# redirect_prepare must echo tracking expires_at
if ! grep -Fq "*resp->mutable_expires_at() = tresp.expires_at();" "$src"; then
  echo "redirect_prepare must map tracking expires_at" >&2
  exit 1
fi

for required in \
  "/api/v2/pages/home_feed       => GetHomeFeed" \
  "/api/v2/pages/guide_detail    => GetGuideDetail" \
  "/api/v2/pages/redirect_prepare => PrepareRedirect" \
  "/api/v2/me/profile                 => MeProfileHttp" \
  "/api/v2/me/preferences             => MePreferencesHttp" \
  "/api/v2/me/consent                 => MeConsentHttp"; do
  if ! grep -Fq "$required" "$src"; then
    echo "missing route mapping: $required" >&2
    exit 1
  fi
done

# profile/preferences/consent must be method-dispatched on fixed paths, not POST subpaths
for forbidden in \
  "/api/v2/me/profile/*" \
  "/api/v2/me/preferences/*" \
  "/api/v2/me/consent/*"; do
  if grep -Fq "$forbidden" "$src"; then
    echo "subpath-style route still registered: $forbidden" >&2
    exit 1
  fi
done

echo "gateway pages golden contract test passed"

