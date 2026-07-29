"""Configure and build the offline core Python binding (shr_pybind).

The binding must be built against the interpreter that will import it (this venv), so this script passes
``-DPython_EXECUTABLE`` for the running interpreter to the portable ``Core-Release-Clang`` preset with
``-DBUILD_PYBIND=ON`` (CMake self-derives the pybind11 CMake package from that interpreter). That preset
builds only shr_core and its portable dependencies -- no CommonLib, XAudio, or Skyrim process.

Unlike the ``Dev-Clang`` preset (which pins the ``.venv`` interpreter), this script targets whichever
interpreter runs it, so it builds the binding without an activated or conventionally located venv.

    python tools/build_pybind.py            # configure + build into build/pybind
    python tools/build_pybind.py --clean    # delete the build tree first

Requires the same environment as any core build (VCPKG_ROOT, clang-cl on PATH). After a successful
build, verify it with: python -m pytest tests --module-dir build/pybind
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import sysconfig
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build" / "pybind"
PRESET = "Core-Release-Clang"


def generate_stub() -> None:
    """Regenerate the binding's type stub so Pylance/Pyright resolves its API.

    Writes ``build/pybind/shr_pybind.pyi`` next to the compiled module, where Pyright prefers it over the
    ``.pyd`` (point the IDE at ``build/pybind`` via ``python.analysis.extraPaths``). Best-effort: a missing
    ``pybind11-stubgen`` or a stub error warns but does not fail the build -- the module itself is usable.
    """
    env = dict(os.environ)
    env["PYTHONPATH"] = os.pathsep.join(p for p in (str(BUILD_DIR), env.get("PYTHONPATH", "")) if p)
    stub = [
        sys.executable,
        "-m",
        "pybind11_stubgen",
        "shr_pybind",
        "-o",
        str(BUILD_DIR),
        "--exit-code",
    ]
    print("$", " ".join(stub))
    if subprocess.run(stub, env=env).returncode != 0:
        print(
            "note: stub generation failed; install pybind11-stubgen (in requirements.txt) for binding "
            "IntelliSense. The compiled module is unaffected.",
            file=sys.stderr,
        )
        return
    print(f"Wrote {(BUILD_DIR / 'shr_pybind.pyi').relative_to(ROOT)}")


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
    generate_stub()
    print(f"Verify with: python -m pytest tests --module-dir {BUILD_DIR.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
