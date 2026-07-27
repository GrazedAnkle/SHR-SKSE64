"""Run every offline golden check in one command.

Drives all four golden checkers over the built ``shr_pybind`` binding, so a maintainer does not need to
know each tool or its build-order prerequisites. This is the single entry point CI invokes after building
the binding.

    python tools/check_goldens.py               # verify every golden against the compiled core
    python tools/check_goldens.py --build       # build the binding first, then verify
    python tools/check_goldens.py --capture      # regenerate every manifest (only after an intended retune)

Verification is one pytest invocation with a distinct case for each golden. Each checker remains runnable
on its own for focused verification or capture; ``--capture`` forwards to all four authoring CLIs.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Ordered along the offline pipeline: source conditioning -> beat rendering -> rhythm/mapping -> trajectory.
CHECKERS = (
    "check_source_conditioning_golden.py",
    "check_beat_renderer_golden.py",
    "check_rhythm_mapping_golden.py",
    "check_trajectory_golden.py",
)


def _run(script: Path, forwarded: list[str]) -> int:
    print(f"\n=== {script.name} ===", flush=True)
    return subprocess.run([sys.executable, str(script), *forwarded]).returncode


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=ROOT / "build" / "pybind",
        help="Directory containing the built shr_pybind*.pyd (default: build/pybind).",
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="Build the binding via tools/build_pybind.py before running the checks.",
    )
    parser.add_argument(
        "--capture",
        action="store_true",
        help="Regenerate every manifest instead of verifying (only after an intended core retune).",
    )
    args = parser.parse_args()

    if args.build:
        print("=== build_pybind.py ===", flush=True)
        build = subprocess.run([sys.executable, str(ROOT / "tools" / "build_pybind.py")]).returncode
        if build != 0:
            sys.exit("binding build failed; aborting golden checks")

    if not args.capture:
        check = [
            sys.executable,
            "-m",
            "pytest",
            str(ROOT / "tests" / "golden"),
            "--module-dir",
            str(args.module_dir),
        ]
        sys.exit(subprocess.run(check).returncode)

    forwarded = ["--module-dir", str(args.module_dir)]
    forwarded.append("--capture")

    results = {name: _run(ROOT / "tools" / name, forwarded) for name in CHECKERS}

    print("\n=== summary ===", flush=True)
    for name, code in results.items():
        print(f"  {'PASS' if code == 0 else 'FAIL'}  {name}")

    failed = [name for name, code in results.items() if code != 0]
    if failed:
        sys.exit(f"\n{len(failed)} golden check(s) failed: {', '.join(failed)}")
    print(f"\nAll {len(CHECKERS)} golden checks passed.")


if __name__ == "__main__":
    main()
