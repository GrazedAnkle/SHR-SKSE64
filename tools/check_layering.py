#!/usr/bin/env python3
"""Validate the core/adapter/plugin dependency boundary at the include site.

src/ holds three layers with different dependency rules, and src/ is the single include root, so
every local include names the layer it reaches into ("core/RenderSpec.hpp"). That makes each
boundary crossing visible where it happens and mechanically checkable here.

Checks:
  1. Layers include downward only: core -> {core}, adapter -> {core, adapter}, plugin -> all.
     The Core-Release-Clang preset already fails to configure on a crossing that pulls a library
     core cannot see; this additionally catches header-only crossings, which compile either way.
  2. Every include of a project header is layer-qualified, in src/, tests/, and bindings/ alike.
  3. Every qualified include resolves to a file that exists.
  4. Each layer directory and its CMake source list hold exactly the same files, so a new file
     cannot be silently omitted from the build the way ModelCoefficientRegistry.hpp was.

Exit code is the number of problems found.

Stdlib-only so it can run in CI.

Usage:  python tools/check_layering.py [--root .]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# What each layer is allowed to include. Dependencies point inward, so this is a chain, not a graph;
# docs/ARCHITECTURE.md owns why the boundary sits where it does.
ALLOWED: dict[str, set[str]] = {
    "core": {"core"},
    "adapter": {"core", "adapter"},
    "plugin": {"core", "adapter", "plugin"},
}

# Consumers outside src/ sit above every layer and may reach any of them.
CONSUMER_DIRS = ("tests", "bindings")

SOURCE_SUFFIXES = (".cpp", ".hpp")

INCLUDE_RE = re.compile(r'^\s*#include\s*(["<])([^">]+)([">])')

# set(core_sources ... ) blocks in CMakeLists.txt, one per layer.
LIST_RE_TEMPLATE = r"set\(\s*{name}\s*(.*?)\n\)"


def layer_of(path: Path, src_dir: Path) -> str | None:
    """The layer directory a src/ file belongs to, or None if it is not under one."""
    try:
        rel = path.relative_to(src_dir)
    except ValueError:
        return None
    return rel.parts[0] if len(rel.parts) > 1 and rel.parts[0] in ALLOWED else None


def cmake_list(text: str, name: str) -> set[str] | None:
    """File paths named in a set(<name> ...) block, as posix strings relative to the repo root."""
    m = re.search(LIST_RE_TEMPLATE.format(name=re.escape(name)), text, re.DOTALL)
    if not m:
        return None
    entries = set()
    for line in m.group(1).split("\n"):
        line = line.split("#", 1)[0].strip()
        if line:
            entries.add(line)
    return entries


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".", help="repo root (default: cwd)")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    src_dir = root / "src"
    if not src_dir.is_dir():
        print(f"no src/ under {root}", file=sys.stderr)
        return 1

    problems: list[str] = []

    # Project headers by bare basename, used to spot an include that should have been qualified.
    header_names = {p.name for p in src_dir.rglob("*.hpp")}

    scan: list[Path] = [p for p in src_dir.rglob("*") if p.suffix in SOURCE_SUFFIXES]
    for name in CONSUMER_DIRS:
        d = root / name
        if d.is_dir():
            scan += [p for p in d.rglob("*") if p.suffix in SOURCE_SUFFIXES]

    for path in sorted(scan):
        rel = path.relative_to(root).as_posix()
        own_layer = layer_of(path, src_dir)
        allowed = ALLOWED[own_layer] if own_layer else set(ALLOWED)

        for lineno, line in enumerate(path.read_text(encoding="utf-8").split("\n"), 1):
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            target = m.group(2)
            head, _, tail = target.partition("/")

            if head in ALLOWED and tail:
                if not (src_dir / target).exists():
                    problems.append(f"{rel}:{lineno}: includes '{target}', which does not exist")
                elif head not in allowed:
                    problems.append(
                        f"{rel}:{lineno}: {own_layer} includes '{target}'; "
                        f"{own_layer} may only include from {sorted(allowed)}"
                    )
            elif "/" not in target and target in header_names:
                problems.append(
                    f"{rel}:{lineno}: includes '{target}' unqualified; name the layer, as in 'core/{target}'"
                )

    # Directory contents and CMake source lists must agree in both directions.
    cmake_text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    for layer in sorted(ALLOWED):
        listed = cmake_list(cmake_text, f"{layer}_sources")
        if listed is None:
            problems.append(f"CMakeLists.txt: no set({layer}_sources ...) block")
            continue
        on_disk = {
            p.relative_to(root).as_posix()
            for p in (src_dir / layer).rglob("*")
            if p.suffix in SOURCE_SUFFIXES
        }
        for missing in sorted(on_disk - listed):
            problems.append(f"CMakeLists.txt: {layer}_sources does not list {missing}")
        for stale in sorted(listed - on_disk):
            problems.append(f"CMakeLists.txt: {layer}_sources lists {stale}, which is not on disk")

    for p in problems:
        print(p)
    if not problems:
        n = len(scan)
        print(f"layering: {n} files; includes qualified, downward only, and matching the CMake lists")
    return len(problems)


if __name__ == "__main__":
    sys.exit(main())
