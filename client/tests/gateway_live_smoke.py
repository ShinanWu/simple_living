#!/usr/bin/env python3
"""
Live gateway smoke test aligned with the currently deployed brpc restful routes
(services/gateway/src/gateway_edge_server_main.cpp) and C-end page flows.

Usage:
  BASE_URL=https://shaotang.top python3 client/tests/gateway_live_smoke.py
"""

from __future__ import annotations

import json
import os
import ssl
import sys
import urllib.error
import urllib.request


BASE_URL = os.getenv("BASE_URL", "https://shaotang.top").rstrip("/")
TIMEOUT = float(os.getenv("REQUEST_TIMEOUT", "15"))
SAMPLE_GUIDE = os.getenv("SAMPLE_GUIDE_CARD_ID", "guide_clothing_tmall_919142939015")


def _ssl_context() -> ssl.SSLContext:
    """Verified TLS context; prefer certifi's CA bundle when the platform store is unavailable."""
    try:
        import certifi  # type: ignore

        return ssl.create_default_context(cafile=certifi.where())
    except Exception:
        return ssl.create_default_context()


SSL_CONTEXT = _ssl_context()


def call(method: str, path: str, body: dict | None = None, headers: dict | None = None) -> tuple[int, dict]:
    url = f"{BASE_URL}{path}"
    data = json.dumps(body).encode("utf-8") if body is not None else None
    hdrs = {"Accept": "application/json", "Content-Type": "application/json"}
    if headers:
        hdrs.update(headers)
    req = urllib.request.Request(url, data=data, method=method, headers=hdrs)
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT, context=SSL_CONTEXT) as resp:
            return resp.status, json.loads(resp.read())
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8", errors="replace")
        try:
            return e.code, json.loads(raw)
        except json.JSONDecodeError:
            return e.code, {"raw": raw[:300]}
    except urllib.error.URLError as e:
        return 0, {"error": str(e)}


def ok_envelope(obj: dict) -> bool:
    return isinstance(obj, dict) and "success" in obj and "code" in obj


def main() -> int:
    print(f"Live smoke against {BASE_URL}\n")
    failed = 0

    def check(name: str, cond: bool, detail: str = "") -> None:
        nonlocal failed
        if cond:
            print(f"[PASS] {name}")
        else:
            failed += 1
            print(f"[FAIL] {name} {detail}")

    st, guest = call("POST", "/api/v2/guest/session", {
        "device_id": "live_smoke_device",
        "client_platform": "wechat_miniprogram",
        "app_version": "1.0.0",
    })
    check("guest_session", st == 200 and guest.get("success") is True, str(guest))
    session_id = (guest.get("data") or {}).get("session_id", "")
    gh = {"X-Guest-Session-Id": session_id} if session_id else {}

    st, feed = call("GET", "/api/v2/pages/home_feed?theme=clothing&limit=3")
    items = (feed.get("data") or {}).get("items") or []
    check("home_feed", st == 200 and feed.get("success") and len(items) > 0, str(feed)[:200])

    guide_id = (items[0].get("guide_card_id") if items else None) or SAMPLE_GUIDE
    st, detail = call("GET", f"/api/v2/pages/guide_detail?guide_card_id={guide_id}", headers=gh)
    check("guide_detail", st == 200 and detail.get("success"), str(detail)[:200])

    st, me = call("GET", "/api/v2/pages/me_summary", headers=gh)
    check("me_summary", st == 200 and me.get("success"), str(me)[:200])

    st, hist_evt = call("POST", "/api/v2/me/history/events", {
        "content_ref": {"type": "guide_card", "guide_card_id": guide_id},
        "source_surface": "detail",
    }, headers=gh)
    check("history_event", st == 200 and hist_evt.get("success"), str(hist_evt)[:200])

    st, hist_list = call("GET", "/api/v2/me/history?limit=5", headers=gh)
    hist_items = (hist_list.get("data") or {}).get("items") or []
    check("history_list", st == 200 and hist_list.get("success") and len(hist_items) > 0, str(hist_list)[:200])
    if hist_items:
        cref = hist_items[0].get("content_ref") or {}
        check("history_content_ref", cref.get("guide_card_id"), str(cref))

    st, health = call("GET", "/api/v2/health")
    check("health", st == 200 and health.get("success"), str(health)[:200])

    counts = (me.get("data") or {}).get("counts") or {}
    check("me_summary_counts_integer", isinstance(counts.get("favorites_count"), int), str(counts))

    rec_id = items[0].get("recommendation_id", "") if items else ""
    st, redirect = call("POST", "/api/v2/pages/redirect_prepare", {
        "guide_card_id": guide_id,
        "recommendation_id": rec_id,
        "scene": "home_feed",
        "item_rank": 1,
    }, headers=gh)
    landing = ((redirect.get("data") or {}).get("landing_url") or "").strip()
    click_id = ((redirect.get("data") or {}).get("click_id") or "").strip()
    check("redirect_prepare_reachable", st == 200 and redirect.get("success"), str(redirect)[:200])
    check("redirect_prepare_nonempty", bool(landing and click_id), str(redirect)[:200])
    if not landing:
        print("[WARN] redirect_prepare returned empty landing_url (check affiliate payload on guide card)")

    for theme in ("clothing", "food", "housing", "transport"):
        st, tfeed = call("GET", f"/api/v2/pages/home_feed?theme={theme}&limit=1")
        n = len((tfeed.get("data") or {}).get("items") or [])
        check(f"theme_{theme}_reachable", st == 200 and tfeed.get("success"), f"items={n}")
        if n == 0:
            print(f"[WARN] theme_{theme} feed is empty (publish content in backoffice)")

    print(f"\nResult: failed={failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
