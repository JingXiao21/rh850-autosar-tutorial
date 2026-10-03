"""Download the open-source toolchain used by examples/mini_autosar_ecu into tools/toolchains/.

  * xPack GNU Arm Embedded GCC (arm-none-eabi-gcc / gdb / binutils)  - GPL
  * Renode portable (full-system simulator incl. STM32L552 / RAMN)  - MIT

Nothing is installed system-wide; tools/toolchains/ is git-ignored.
Requires the GitHub CLI (`gh`) for downloads (falls back to urllib on direct URLs).

Usage:  python tools/setup_toolchains.py [--force]
"""
import argparse
import hashlib
import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent / "toolchains"
DL = ROOT / "_dl"

GCC_VER = "15.2.1-1.1"
GCC_ZIP = f"xpack-arm-none-eabi-gcc-{GCC_VER}-win32-x64.zip"
GCC_URL = f"https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v{GCC_VER}/{GCC_ZIP}"
GCC_SHA256 = "bae6a3d1667697ce750c3b13d6d26d80973ecedc2cc87bf04869e83447fd93ea"
GCC_DIR = ROOT / f"xpack-arm-none-eabi-gcc-{GCC_VER}"

RENODE_VER = "1.17.0"
RENODE_ZIP = f"renode-{RENODE_VER}.windows-portable.zip"
RENODE_URL = f"https://github.com/renode/renode/releases/download/v{RENODE_VER}/{RENODE_ZIP}"
RENODE_DIR = ROOT / f"renode_{RENODE_VER}-portable"


def download(url: str, dest: Path) -> None:
    if dest.exists():
        return
    print(f"downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "setup_toolchains"})
    with urllib.request.urlopen(req, timeout=600) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--force", action="store_true", help="re-extract even if already present")
    args = ap.parse_args()
    if sys.platform != "win32":
        print("This script fetches the Windows packages; on Linux/macOS install arm-none-eabi-gcc "
              "and Renode from your package manager or the same GitHub releases.")
        return 1
    DL.mkdir(parents=True, exist_ok=True)

    download(GCC_URL, DL / GCC_ZIP)
    if sha256(DL / GCC_ZIP) != GCC_SHA256:
        print("ERROR: arm-none-eabi-gcc archive checksum mismatch")
        return 1
    if args.force or not GCC_DIR.exists():
        zipfile.ZipFile(DL / GCC_ZIP).extractall(ROOT)

    download(RENODE_URL, DL / RENODE_ZIP)
    if args.force or not RENODE_DIR.exists():
        zipfile.ZipFile(DL / RENODE_ZIP).extractall(ROOT)

    print(f"gcc    : {GCC_DIR / 'bin' / 'arm-none-eabi-gcc.exe'}")
    print(f"renode : {RENODE_DIR / 'renode.exe'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
