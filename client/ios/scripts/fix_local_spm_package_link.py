#!/usr/bin/env python3
"""
XcodeGen (through XcodeProj) omits `package = <XCLocalSwiftPackageReference>` on
`XCSwiftPackageProductDependency` for path-based packages, which makes Xcode show
"Missing package product".

Run after `xcodegen generate` (idempotent).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    pbx = root / "SimpleLiving.xcodeproj" / "project.pbxproj"
    if not pbx.is_file():
        print(f"error: missing {pbx}", file=sys.stderr)
        return 1

    text = pbx.read_text(encoding="utf-8")

    m_local = re.search(
        r"^\t\t([0-9A-F]{24}) /\* XCLocalSwiftPackageReference ",
        text,
        re.MULTILINE,
    )
    if not m_local:
        print("error: no XCLocalSwiftPackageReference in project.pbxproj", file=sys.stderr)
        return 1
    local_id = m_local.group(1)

    # Must match product dependency only (not PBXFileReference "SimpleLivingCore" folder).
    block = re.search(
        r"\t\t[0-9A-F]{24} /\* SimpleLivingCore \*/ = \{\n"
        r"\t\t\tisa = XCSwiftPackageProductDependency;\n"
        r"(?:\t\t\tpackage = [0-9A-F]{24} /\* XCLocalSwiftPackageReference [^*]+ \*/;\n)?"
        r"\t\t\tproductName = SimpleLivingCore;\n"
        r"\t\t\};",
        text,
    )
    if not block:
        print("error: SimpleLivingCore XCSwiftPackageProductDependency block not found", file=sys.stderr)
        return 1

    block_s = block.group(0)
    if "package = " in block_s:
        return 0

    comment_m = re.search(r"/\* (XCLocalSwiftPackageReference [^*]+) \*/", text)
    pkg_comment = comment_m.group(1) if comment_m else "XCLocalSwiftPackageReference"
    insert = f"\t\t\tpackage = {local_id} /* {pkg_comment} */;\n"
    new_block = block_s.replace(
        "\t\t\tisa = XCSwiftPackageProductDependency;\n",
        "\t\t\tisa = XCSwiftPackageProductDependency;\n" + insert,
        1,
    )
    new_text = text[: block.start()] + new_block + text[block.end() :]
    pbx.write_text(new_text, encoding="utf-8")
    print(f"patched {pbx}: linked SimpleLivingCore to local package {local_id}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
