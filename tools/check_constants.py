#!/usr/bin/env python3
"""Verify provenance tags on constants in src/Constants.hpp.

Each `constexpr` inherits the nearest preceding tag in its blank-line-separated
block unless its own line provides an override. A blank line or `=====` banner
ends the block.

The vocabulary is defined in docs/DOCUMENTATION_CONVENTIONS.md. Exit code is the
number of untagged constants.

Stdlib-only so it can run in CI.

Usage:  python tools/check_constants.py [--root .]
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

TAGS = ("physio", "ref", "dsp", "game", "asset", "util")
TAG_RE = re.compile(r"\[(" + "|".join(TAGS) + r")\]")
CONSTEXPR_RE = re.compile(r"^\s*constexpr\b.*?\b([A-Za-z_]\w*)\s*=")
COMMENT_RE = re.compile(r"^\s*//")
BANNER_RE = re.compile(r"=====")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".", help="repo root (default: cwd)")
    args = ap.parse_args()

    path = Path(args.root).resolve() / "src" / "Constants.hpp"
    if not path.exists():
        print(f"no src/Constants.hpp under {args.root}", file=sys.stderr)
        return 1

    current: str | None = None
    problems: list[str] = []
    counts: dict[str, int] = {t: 0 for t in TAGS}

    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or BANNER_RE.search(line):
            current = None
        is_comment = COMMENT_RE.match(line) is not None
        tag_here = TAG_RE.search(line)

        decl = CONSTEXPR_RE.match(line)
        if decl:
            tag = tag_here.group(1) if tag_here else current
            if tag is None:
                problems.append(f"Constants.hpp:{lineno}: '{decl.group(1)}' has no provenance tag")
            else:
                counts[tag] += 1
            continue

        # A tagged comment applies to the rest of its block.
        if is_comment and tag_here:
            current = tag_here.group(1)

    total = sum(counts.values())
    if problems:
        print(f"{len(problems)} untagged constant(s):\n")
        for p in problems:
            print(f"  {p}")
        print("\nTag each with one of: " + ", ".join(f"[{t}]" for t in TAGS))
    else:
        breakdown = ", ".join(f"{counts[t]} {t}" for t in TAGS if counts[t])
        print(f"constants: all {total} tagged ({breakdown})")
    return len(problems)


if __name__ == "__main__":
    sys.exit(main())
