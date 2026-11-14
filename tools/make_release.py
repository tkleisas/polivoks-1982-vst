#!/usr/bin/env python3
"""Stage and zip the MPC and desktop release assets with license notices."""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLUGIN_NAME = "Polivoks 1982"
MPC_FOLDER = "bokontep - VST - Polivoks 1982"
DESKTOP_FOLDER = "Polivoks 1982"


def git_commit(path: Path, fallback: str) -> str:
    try:
        return subprocess.check_output(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except Exception:
        return fallback


def copy_common(dest: Path, repo: str, version: str, sdk: Path, plugin_readme: Path) -> None:
    shutil.copy2(ROOT / "README.md", dest / "README.md")
    shutil.copy2(ROOT / "LICENSE", dest / "LICENSE")
    shutil.copy2(ROOT / "NOTICE.md", dest / "NOTICE.md")
    shutil.copy2(plugin_readme, dest / "PLUGIN-README.md")
    shutil.copytree(ROOT / "LICENSES", dest / "LICENSES")

    source = f"""Polivoks 1982 {version}

Source repository:
{repo}

Source commit:
{git_commit(ROOT, "working tree")}

MPC Plugin SDK (MIT):
https://github.com/inmusic-software/mpc-plugin-builder-sdk
SDK commit: {git_commit(sdk, "pinned")}

License:
  Plugin: GPL-3.0-or-later (see LICENSE)
  MPC Plugin SDK: MIT (see LICENSES/MPC-Plugin-SDK-MIT.txt)

The complete corresponding source for the plugin is the source repository
above at the listed commit. The MPC Plugin SDK is fetched by build.sh /
build.ps1 at the pinned commit and is MIT-licensed.
"""
    (dest / "SOURCE.txt").write_text(source, encoding="utf-8")


def install_mpc(dest: Path, version: str) -> None:
    (dest / "INSTALL.txt").write_text(f"""Polivoks 1982 {version} - Akai MPC standalone

1. Copy the whole "{MPC_FOLDER}" folder into a folder named exactly
   "VST2 Plugins" at the root of a USB drive, SD card, or the MPC internal
   drive. The resulting layout is:

     <drive>/VST2 Plugins/{MPC_FOLDER}/

2. Put the drive in the MPC. The MPC must run MPC OS 3.10.1 or later.

3. On the MPC: Preferences -> Enhancements (Beta). Turn on
   "Enable Enhancements" and "External Plugin Support".

4. Restart the MPC application (or power-cycle the MPC) so it rescans VST2
   plugins. Save your project first: restarting the app discards unsaved work.

5. Add the instrument from the plugin browser under "External Plugins".

This is an independent GPL-3.0-or-later emulation. See LICENSE and NOTICE.md.
The MPC Plugin SDK is MIT-licensed; see LICENSES/MPC-Plugin-SDK-MIT.txt.
""", encoding="utf-8")


def install_desktop(dest: Path, version: str, desktop_name: str) -> None:
    if desktop_name.startswith("windows"):
        platform_hint = "Copy the folder into the VST2 folder used by your host (e.g. C:\\\\Program Files\\\\Common Files\\\\VST2)."
    elif desktop_name.startswith("mac"):
        platform_hint = "Copy the .vst bundle into ~/Library/Audio/Plug-Ins/VST/ (or the host's VST2 folder)."
    else:
        platform_hint = "Copy the \"Polivoks 1982\" folder into ~/.vst (or the host's VST2 folder)."
    (dest / "INSTALL.txt").write_text(f"""Polivoks 1982 {version} - desktop VST2

{platform_hint}

This is a VST2 plugin. There is no custom graphical editor; the host shows
its generic parameter panel. Many current DAWs have dropped VST2 support;
check that your host still loads VST2 plugins.

The plugin is GPL-3.0-or-later. See LICENSE and NOTICE.md.
The MPC Plugin SDK is MIT-licensed; see LICENSES/MPC-Plugin-SDK-MIT.txt.
""", encoding="utf-8")


def zip_dir(src: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        target.unlink()
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(src.rglob("*")):
            if p.is_file():
                z.write(p, p.relative_to(src).as_posix())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sdk", required=True, help="MPC Plugin SDK checkout used for the build")
    ap.add_argument("--out", required=True, help="output directory for the zips")
    ap.add_argument("--repo", default="https://github.com/tkleisas/polivoks-1982-vst")
    ap.add_argument("--desktop-name", default="linux-vst2",
                    help="suffix for the desktop zip, e.g. linux-vst2 or windows-vst2")
    args = ap.parse_args()

    sdk = Path(args.sdk).resolve()
    out = Path(args.out).resolve()
    build = sdk / "plugins" / PLUGIN_NAME / "build"
    mpc_src = build / MPC_FOLDER
    desktop_src = build / "desktop" / DESKTOP_FOLDER

    if not (mpc_src / "plugin.json").is_file():
        raise SystemExit(f"MPC build not found: {mpc_src}")
    if not (desktop_src / "plugin.json").is_file():
        raise SystemExit(f"desktop build not found: {desktop_src}")

    manifest = json.loads((mpc_src / "plugin.json").read_text(encoding="utf-8"))
    version = str(manifest.get("version", "1.0.0"))
    if not str(manifest.get("license", "")).startswith("GPL-3.0"):
        raise SystemExit("refusing to package: plugin.json license is not GPL-3.0")

    plugin_readme = ROOT / "plugin" / PLUGIN_NAME / "README.md"
    if not plugin_readme.is_file():
        raise SystemExit(f"missing {plugin_readme}")

    with tempfile.TemporaryDirectory(prefix="polivoks-release-") as td:
        stage = Path(td)

        mpc_root = stage / "mpc"
        mpc_root.mkdir()
        shutil.copytree(mpc_src, mpc_root / MPC_FOLDER)
        shutil.copy2(plugin_readme, mpc_root / MPC_FOLDER / "README.md")
        copy_common(mpc_root, args.repo, version, sdk, plugin_readme)
        install_mpc(mpc_root, version)
        mpc_zip = out / f"polivoks-1982-{version}-mpc.zip"
        zip_dir(mpc_root, mpc_zip)

        desktop_root = stage / "desktop"
        desktop_root.mkdir()
        shutil.copytree(desktop_src, desktop_root / DESKTOP_FOLDER)
        copy_common(desktop_root, args.repo, version, sdk, plugin_readme)
        install_desktop(desktop_root, version, args.desktop_name)
        desktop_zip = out / f"polivoks-1982-{version}-{args.desktop_name}.zip"
        zip_dir(desktop_root, desktop_zip)

    print(f"MPC asset:     {mpc_zip}")
    print(f"Desktop asset: {desktop_zip}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
