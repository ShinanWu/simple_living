#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
xcodegen generate
python3 scripts/fix_local_spm_package_link.py
echo "OK: open SimpleLiving.xcodeproj (Package product link is patched for XcodeGen)."
