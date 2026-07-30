"""Configure and build the offline core Python binding (shr_pybind) in a plugin-free tree.

For when the plugin cannot be built - CI, or a checkout without the CommonLibSSE submodule. README.md
(Building) covers the ordinary local path, which needs no second command.

    python tools/build_pybind.py            # configure + build into build/pybind
    python tools/build_pybind.py --clean    # delete the build tree first

Requires the same environment as any core build (VCPKG_ROOT, clang-cl on PATH). Verify with: python -m
pytest

The extension must be built against the interpreter that will import it, so this passes
``-DPython_EXECUTABLE`` for the running interpreter rather than the ``.venv`` one the presets pin: that
builds the binding from an interpreter in any location, activated or not.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tomllib
from pathlib import Path

import core_offline

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build" / "pybind"
PRESET = "Core-Release-Clang"


def declared_minimum_python() -> tuple[int, ...]:
    """The interpreter floor declared in ``pyproject.toml``'s ``[tool.shr] requires-python``."""
    config = tomllib.loads((ROOT / "pyproject.toml").read_text(encoding="utf-8"))
    specifier = config["tool"]["shr"]["requires-python"]
    return tuple(int(part) for part in specifier.removeprefix(">=").split("."))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true", help="Remove the build tree before configuring.")
    args = parser.parse_args()

    if args.clean and BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)

    running = sys.version_info[:2]
    minimum = declared_minimum_python()
    if running < minimum:
        # Non-fatal: the ABI tag of the produced .pyd follows this interpreter regardless.
        print(
            f"note: building against Python {'.'.join(map(str, running))}; pyproject declares "
            f">={'.'.join(map(str, minimum))}",
            file=sys.stderr,
        )

    configure = [
        "cmake",
        "--preset", PRESET,
        "-B", str(BUILD_DIR),
        "-DBUILD_PYBIND=ON",
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

    # A build that succeeded but published nothing would otherwise surface later as an import failure with
    # no obvious cause.
    published = sorted(core_offline.DEFAULT_MODULE_DIR.glob("shr_pybind*.pyd"))
    if not published:
        sys.exit(
            f"built, but nothing was published to {core_offline.DEFAULT_MODULE_DIR.relative_to(ROOT)}; "
            "check the POST_BUILD step in cmake/publish_pybind_module.cmake"
        )
    print(f"Published {published[-1].relative_to(ROOT)} ({core_offline.provenance_summary()})")
    print("Verify with: python -m pytest")


if __name__ == "__main__":
    main()
