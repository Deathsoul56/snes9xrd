#!/usr/bin/env python3
"""Package a built snes9x-qt.exe into a release-ready zip for snes9xrd.

Usage (from repo root, after building with CMake+Ninja into ./build):
    python qt/scripts/package_release.py
    python qt/scripts/package_release.py --version 0.1 --build-dir build

The resulting archive contains everything needed to run the emulator on a
clean machine, since the Qt build used by this fork is fully static
(no Qt6*.dll / libstdc++ / libwinpthread dependencies to deploy).
"""

import argparse
import re
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


def detect_version() -> str:
    main_window = REPO_ROOT / "qt" / "src" / "EmuMainWindow.cpp"
    text = main_window.read_text(encoding="utf-8")
    match = re.search(r'kSnes9xrdVersion\s*=\s*"([^"]+)"', text)
    if not match:
        raise SystemExit(f"Could not find kSnes9xrdVersion in {main_window}")
    return match.group(1)


def find_exe(build_dir: Path) -> Path:
    for name in ("snes9x-qt.exe", "snes9x-qt"):
        candidate = build_dir / name
        if candidate.is_file():
            return candidate
    raise SystemExit(
        f"Could not find snes9x-qt(.exe) in {build_dir}. Build it first with:\n"
        f"  cmake -G Ninja -B {build_dir} -S qt -DCMAKE_BUILD_TYPE=Release\n"
        f"  ninja -C {build_dir}"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default=None, help="Release version (default: read from EmuMainWindow.cpp)")
    parser.add_argument("--build-dir", default="build", help="CMake build directory (default: build)")
    parser.add_argument("--arch", default="win64", help="Archive name suffix (default: win64)")
    args = parser.parse_args()

    version = args.version or detect_version()
    build_dir = (REPO_ROOT / args.build_dir).resolve()
    exe_path = find_exe(build_dir)

    staging_name = f"snes9xrd-v{version}-{args.arch}"
    staging_dir = REPO_ROOT / staging_name
    if staging_dir.exists():
        try:
            shutil.rmtree(staging_dir)
        except PermissionError as exc:
            raise SystemExit(
                f"Could not remove {staging_dir} ({exc}).\n"
                "Close snes9x-qt.exe and any Explorer/zip window with that "
                "folder open, then try again."
            ) from exc
    staging_dir.mkdir(parents=True)

    shutil.copy2(exe_path, staging_dir / exe_path.name)
    shutil.copy2(REPO_ROOT / "data" / "cheats.bml", staging_dir / "cheats.bml")
    shutil.copy2(REPO_ROOT / "LICENSE", staging_dir / "LICENSE")

    fork_changes = (REPO_ROOT / "docs" / "changes-snes9xrd.txt").read_text(encoding="utf-8")
    upstream_changes = (REPO_ROOT / "docs" / "changes.txt").read_text(encoding="utf-8")
    combined_changes = fork_changes.rstrip() + "\n\n\n" + upstream_changes
    (staging_dir / "changes.txt").write_text(combined_changes, encoding="utf-8")

    archive_path = shutil.make_archive(str(staging_dir), "zip", root_dir=REPO_ROOT, base_dir=staging_name)

    print(f"Packaged: {archive_path}")


if __name__ == "__main__":
    main()
