"""Author the offline golden manifests from the compiled core.

This tool only ever writes; verification is ``pytest``. ARCHITECTURE.md (offline execution) owns why the
two are separate commands.

    python tools/capture_goldens.py                          # recapture every domain
    python tools/capture_goldens.py trajectory               # recapture one
    python tools/capture_goldens.py rhythm-mapping trajectory
    python tools/capture_goldens.py --module-dir build/dev-clang

Build the binding first (any preset enabling SHR_BUILD_PYBIND publishes it). The waveform manifests hash raw
float bytes, so capture with the compiler .github/workflows/offline-goldens.yml pins - a manifest captured
with a different toolchain will not verify there.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import core_offline
import golden_registry

ROOT = Path(__file__).resolve().parent.parent


def main() -> None:
    domains = golden_registry.by_id()
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "domains",
        nargs="*",
        metavar="DOMAIN",
        help=f"Domains to recapture (default: all). One or more of: {', '.join(domains)}.",
    )
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=core_offline.DEFAULT_MODULE_DIR,
        help=core_offline.MODULE_DIR_HELP,
    )
    args = parser.parse_args()

    unknown = [name for name in args.domains if name not in domains]
    if unknown:
        parser.error(
            f"unknown golden domain(s): {', '.join(unknown)}; expected one or more of: {', '.join(domains)}"
        )

    selected = [domains[name] for name in args.domains] if args.domains else golden_registry.DOMAINS
    module = core_offline.load_binding(args.module_dir)

    for domain in selected:
        manifest = domain.build(module)
        domain.MANIFEST.parent.mkdir(parents=True, exist_ok=True)
        domain.MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(f"Wrote {domain.MANIFEST.relative_to(ROOT)}")
        for line in domain.summary(manifest):
            print(f"  {line}")

    print(f"\nCaptured {len(selected)} golden domain(s). Review the manifest diff before committing.")


if __name__ == "__main__":
    main()
