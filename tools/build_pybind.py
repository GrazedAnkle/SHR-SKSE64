"""Configure and build the offline core Python binding (shr_pybind).

The binding must be built against the interpreter that will import it (this venv), so this script
derives ``Python_EXECUTABLE`` and the pybind11 CMake package from the running interpreter and passes
them to the portable ``Core-Release-Clang`` preset with ``-DBUILD_PYBIND=ON``. That preset builds only
shr_core and its portable dependencies -- no CommonLib, XAudio, or Skyrim process.

    python tools/build_pybind.py            # configure + build into build/pybind
    python tools/build_pybind.py --clean    # delete the build tree first

Requires the same environment as any core build (VCPKG_ROOT, clang-cl on PATH). After a successful
build, verify it with tools/check_pybind_golden.py.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import sysconfig
from pathlib import Path

import pybind11

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build" / "pybind"
PRESET = "Core-Release-Clang"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true", help="Remove the build tree before configuring.")
    args = parser.parse_args()

    if args.clean and BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)

    if sysconfig.get_python_version() < "3.13" and not sys.warnoptions:
        # Non-fatal: the ABI tag of the produced .pyd follows this interpreter regardless.
        print(f"note: building against Python {sysconfig.get_python_version()}", file=sys.stderr)

    configure = [
        "cmake",
        "--preset", PRESET,
        "-B", str(BUILD_DIR),
        "-DBUILD_PYBIND=ON",
        f"-Dpybind11_DIR={pybind11.get_cmake_dir()}",
        f"-DPython_EXECUTABLE={sys.executable}",
    ]
    build = ["cmake", "--build", str(BUILD_DIR), "--target", "shr_pybind"]

    print("$", " ".join(configure))
    subprocess.run(configure, check=True)
    print("$", " ".join(build))
    subprocess.run(build, check=True)

    modules = sorted(BUILD_DIR.glob("shr_pybind*.pyd"))
    if not modules:
        sys.exit("build reported success but no shr_pybind*.pyd was produced")
    print(f"\nBuilt {modules[-1].relative_to(ROOT)}")
    print("Verify with: python tools/check_pybind_golden.py")


if __name__ == "__main__":
    main()
