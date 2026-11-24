#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package a built Polivoks 1982 VST3 bundle for release.

Usage:
    python3 vst3/package.py [--version 1.2.0] [--build-dir vst3/build] [--out dist]
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def project_version() -> str:
    text = (ROOT / "vst3" / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"project\([^)]*VERSION\s+([0-9.]+)", text, re.IGNORECASE | re.DOTALL)
    return m.group(1) if m else "1.0.0"


def git_commit() -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        return "working tree"


def find_bundle(build_dir: Path) -> Path:
    candidates = [
        build_dir / "Polivoks1982_artefacts" / "Release" / "VST3" / "Polivoks 1982.vst3",
        build_dir / "Polivoks1982_artefacts" / "VST3" / "Polivoks 1982.vst3",
    ]
    for p in candidates:
        if p.is_dir():
            return p
    found = list(build_dir.rglob("Polivoks 1982.vst3"))
    if found:
        return found[0]
    raise SystemExit("VST3 bundle not found under %s" % build_dir)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default=project_version())
    ap.add_argument("--build-dir", default=str(ROOT / "vst3" / "build"))
    ap.add_argument("--out", default=str(ROOT / "dist"))
    ap.add_argument("--platform", default="windows" if os.name == "nt" else "linux")
    args = ap.parse_args()

    build_dir = Path(args.build_dir)
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    bundle = find_bundle(build_dir)

    readme = (ROOT / "vst3" / "README.md")
    license_file = ROOT / "LICENSE"
    notice = ROOT / "NOTICE.md"
    licenses = ROOT / "LICENSES"
    for p in (readme, license_file, notice):
        if not p.is_file():
            raise SystemExit("missing %s" % p)

    source = f"""Polivoks 1982 VST3 {args.version}

Source repository:
https://github.com/tkleisas/polivoks-1982-vst

Source commit:
{git_commit()}

JUCE:
https://github.com/juce-framework/JUCE tag 7.0.12, used under GPLv3.

The VST3 SDK is the copy bundled with JUCE. The plugin is
GPL-3.0-or-later; see LICENSE. The complete corresponding source is the
repository above at the listed commit.
"""
    (out_dir / "SOURCE.txt").write_text(source, encoding="utf-8")

    zip_path = out_dir / f"polivoks-1982-{args.version}-{args.platform}-vst3.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for child in sorted(bundle.rglob("*")):
            if child.is_file():
                z.write(child, Path(bundle.name) / child.relative_to(bundle))
        z.write(readme, "README-VST3.md")
        z.write(license_file, "LICENSE")
        z.write(notice, "NOTICE.md")
        z.write(out_dir / "SOURCE.txt", "SOURCE.txt")
        for child in sorted(licenses.rglob("*")):
            if child.is_file():
                z.write(child, Path("LICENSES") / child.relative_to(licenses))

    print(zip_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
