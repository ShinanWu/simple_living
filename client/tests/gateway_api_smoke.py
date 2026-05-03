#!/usr/bin/env python3
"""
Gateway API smoke test runner.

Covers the user v2 routes and pages routes listed in services/gateway/docs/api.md.
This script is tolerant to auth/business-state differences and focuses on:
1) endpoint reachability
2) JSON envelope shape
3) no unexpected 5xx on basic requests
"""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any


@dataclass
class Case:
    name: str
    method: str
    path: str
    body: dict[str, Any] | None = None
    auth: str = "none"  # none | optional | required
    allowed_http: tuple[int, ...] = (200, 400, 401, 403, 404, 409, 429)


BASE_URL = os.getenv("BASE_URL", "http://127.0.0.1:8080").rstrip("/")
ACCESS_TOKEN = os.getenv("ACCESS_TOKEN", "")
REFRESH_TOKEN = os.getenv("REFRESH_TOKEN", "refresh_token_placeholder")
FAVORITE_ID = os.getenv("FAVORITE_ID", "fav_placeholder")
TIMEOUT = float(os.getenv("REQUEST_TIMEOUT", "10"))


def make_headers(auth: str) -> dict[str, str]:
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json",
    }
    if auth in ("optional", "required") and ACCESS_TOKEN:
        headers["Authorization"] = f"Bearer {ACCESS_TOKEN}"
    return headers


def do_request(c: Case) -> tuple[int, str]:
    url = f"{BASE_URL}{c.path}"
    data = None
    if c.body is not None:
        data = json.dumps(c.body).encode("utf-8")
    req = urllib.request.Request(
        url=url,
        data=data,
        method=c.method,
        headers=make_headers(c.auth),
    )
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
            return r.status, r.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="replace")
    except urllib.error.URLError as e:
        return 0, f"network_error: {e}"
    except OSError as e:
        return 0, f"network_error: {e}"


def is_valid_envelope(text: str) -> bool:
    try:
        obj = json.loads(text)
    except Exception:
        return False
    return isinstance(obj, dict) and "code" in obj and "message" in obj and "success" in obj


def build_cases() -> list[Case]:
    return [
        Case("guest_session", "POST", "/api/v2/guest/session", {"device_id": "dev-001", "client_platform": "web"}),
        Case("token_issue", "POST", "/api/v2/auth/token/issue", {"account_proof": {"user_id": "user_demo"}}),
        Case(
            "token_refresh",
            "POST",
            "/api/v2/auth/token/refresh",
            {
                "refresh_token": REFRESH_TOKEN,
                "request_context": {
                    "client_platform": "web",
                    "app_version": "smoke-test",
                    "device_id": "smoke-device-001",
                },
            },
        ),
        Case("auth_session_delete", "POST", "/api/v2/auth/session/revoke", {"revoke_scope": "single_session"}, auth="required"),
        Case("me_profile_get", "POST", "/api/v2/me/profile/get", auth="optional"),
        Case("me_profile_patch", "POST", "/api/v2/me/profile/update", {"display_name": "Smoke Test"}, auth="required"),
        Case("me_preferences_get", "POST", "/api/v2/me/preferences/get", auth="required"),
        Case("me_preferences_put", "POST", "/api/v2/me/preferences/update", {"preferences": {"themes": ["food"]}}, auth="required"),
        Case("me_favorites_get", "POST", "/api/v2/me/favorites/list", {"limit": 5}, auth="required"),
        Case("me_favorites_post", "POST", "/api/v2/me/favorites/add", {"guide_card_id": "guide_card_1001"}, auth="required"),
        Case("me_favorites_delete", "POST", "/api/v2/me/favorites/remove", {"favorite_id": FAVORITE_ID}, auth="required"),
        Case("me_history_get", "POST", "/api/v2/me/history/list", {"limit": 5}, auth="optional"),
        Case("me_history_event", "POST", "/api/v2/me/history/events", {"content_ref": {"guide_card_id": "guide_card_1001"}}),
        Case("me_history_delete", "POST", "/api/v2/me/history/clear", {"scope": "all"}, auth="optional"),
        Case("me_feedback", "POST", "/api/v2/me/feedback", {"target_type": "guide_card", "target_id": "guide_card_1001"}),
        Case("me_consent_get", "POST", "/api/v2/me/consent/get", auth="required"),
        Case("me_consent_put", "POST", "/api/v2/me/consent/update", {"consent": {"personalization": True}}, auth="required"),
        Case("me_summary", "POST", "/api/v2/me/summary/get", auth="optional"),
        Case("health", "POST", "/api/v2/health/check"),
        Case("pages_home_feed", "POST", "/api/v2/pages/home_feed", {"limit": 5}),
        Case("pages_guide_detail", "POST", "/api/v2/pages/guide_detail", {"guide_card_id": "guide_card_1001"}),
        Case("pages_redirect_prepare", "POST", "/api/v2/pages/redirect_prepare", {"guide_card_id": "guide_card_1001"}, auth="optional"),
        Case("pages_me_summary", "POST", "/api/v2/pages/me_summary", auth="optional"),
    ]


def main() -> int:
    print(f"Running gateway smoke tests against: {BASE_URL}")
    passed = 0
    failed = 0
    cases = build_cases()

    for c in cases:
        status, body = do_request(c)
        ok_http = status in c.allowed_http
        ok_json = is_valid_envelope(body)
        ok = ok_http and ok_json
        if ok:
            passed += 1
            print(f"[PASS] {c.name:<24} {c.method} {c.path} -> HTTP {status}")
        else:
            failed += 1
            preview = body[:220].replace("\n", " ")
            print(f"[FAIL] {c.name:<24} {c.method} {c.path} -> HTTP {status}; body={preview}")

    print(f"\nResult: passed={passed}, failed={failed}, total={len(cases)}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
