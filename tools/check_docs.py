#!/usr/bin/env python3
"""Validate project documentation cross-references.

Checks docs/*.md for local links and anchors, source symbols, numbered coupling
references, and Python script references. Python script references in tools/*.py
are also checked, and tools/README.md must index every module in tools/.

Exit code is the number of problems found.

Stdlib-only so it can run in CI.

Usage:  python tools/check_docs.py [--root .]
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^#{1,6}\s+(.*?)\s*#*\s*$")
FILECITE_RE = re.compile(r"(\w+\.(?:hpp|cpp))\s*:\s*`?([A-Za-z_]\w*)`?")
QUALIFIED_RE = re.compile(r"`([A-Za-z_]\w*(?:::[A-Za-z_]\w*)+)`")
BARE_SYM_RE = re.compile(r"`([A-Za-z_]\w*)`")
# Treat camelCase and PascalCase words as symbols, excluding m_ members and
# digit-prefixed names.
CAMEL_RE = re.compile(r"^[A-Za-z]+[a-z][A-Z]\w*$")
PYREF_RE = re.compile(r"\b([\w-]+\.py)\b")
COUPLING_REF_RE = re.compile(r"coupling\s+(\d+)", re.IGNORECASE)
NUMBERED_RE = re.compile(r"^(\d+)\.\s")
FENCE_RE = re.compile(r"^\s*```")


def slug(heading: str) -> str:
    """GitHub-style heading anchor slug."""
    s = heading.strip().lower()
    s = re.sub(r"[^\w\s-]", "", s)
    s = re.sub(r"\s+", "-", s)
    return s


def headings_of(path: Path) -> set[str]:
    slugs: set[str] = set()
    in_fence = False
    for line in path.read_text(encoding="utf-8").splitlines():
        if FENCE_RE.match(line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        m = HEADING_RE.match(line)
        if m:
            slugs.add(slug(m.group(1)))
    return slugs


def coupling_count(docs_dir: Path) -> int:
    path = docs_dir / "SYNTHESIS_MODEL.md"
    if not path.exists():
        return 0
    lines = path.read_text(encoding="utf-8").splitlines()
    in_section = False
    count = 0
    for line in lines:
        if line.startswith("## "):
            in_section = "Second-order couplings" in line
            continue
        if in_section:
            m = NUMBERED_RE.match(line)
            if m:
                count = max(count, int(m.group(1)))
    return count


def lines_of(text: str) -> list[tuple[int, str]]:
    return list(enumerate(text.splitlines(), start=1))


def tools_index_problems(tools_dir: Path, resolvable: set[str]) -> list[str]:
    """Every tools/*.py must appear in tools/README.md, and the index must not cite a script that is gone.

    The index cites the tests covering each module, so a cited name resolves against tests/ as well as
    tools/.
    """
    index = tools_dir / "README.md"
    if not index.is_file():
        return [f"{tools_dir.name}/README.md: missing tools index"]

    listed = set(PYREF_RE.findall(index.read_text(encoding="utf-8")))
    present = {path.name for path in tools_dir.glob("*.py")}
    problems = [f"tools/README.md: does not list {name}" for name in sorted(present - listed)]
    problems += [
        f"tools/README.md: references Python script '{name}' not in tools/ or tests/"
        for name in sorted(listed - resolvable)
    ]
    return problems


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".", help="repo root (default: cwd)")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    docs_dir = root / "docs"
    src_dir = root / "src"
    tools_dir = root / "tools"

    if not docs_dir.is_dir():
        print(f"no docs/ under {root}", file=sys.stderr)
        return 1

    # Python scripts in tools/ and tests/ both count as resolvable. tests/ is searched recursively so
    # suite subdirectories (tests/golden/) resolve like any other test module.
    tests_dir = root / "tests"
    tool_scripts = {p.name for p in tools_dir.glob("*.py")}
    tool_scripts |= {p.name for p in tests_dir.rglob("*.py")}

    src_text = "\n".join(
        p.read_text(encoding="utf-8", errors="ignore")
        for p in src_dir.rglob("*")
        if p.suffix in (".cpp", ".hpp", ".h")
    )
    src_words = set(re.findall(r"[A-Za-z_]\w*", src_text))

    heading_cache: dict[Path, set[str]] = {}

    def headings_cached(path: Path) -> set[str]:
        if path not in heading_cache:
            heading_cache[path] = headings_of(path)
        return heading_cache[path]

    n_couplings = coupling_count(docs_dir)
    problems: list[str] = []

    for doc in sorted(docs_dir.glob("*.md")):
        text = doc.read_text(encoding="utf-8")
        in_fence = False
        for lineno, line in lines_of(text):
            if FENCE_RE.match(line):
                in_fence = not in_fence

            # Links outside fenced blocks.
            if not in_fence:
                for target in LINK_RE.findall(line):
                    target = target.strip()
                    if target.startswith(("http://", "https://", "mailto:")):
                        continue
                    path_part, _, anchor = target.partition("#")
                    if path_part:
                        dest = (doc.parent / path_part).resolve()
                        if not dest.exists():
                            problems.append(f"{doc.name}:{lineno}: link to missing file '{path_part}'")
                            continue
                        if anchor and dest.suffix == ".md":
                            if slug(anchor) not in headings_cached(dest):
                                problems.append(
                                    f"{doc.name}:{lineno}: link to missing anchor '#{anchor}' in {path_part}"
                                )
                    elif anchor:
                        if slug(anchor) not in headings_cached(doc):
                            problems.append(f"{doc.name}:{lineno}: in-page anchor '#{anchor}' not found")

            # Symbol citations, including fenced blocks.
            for fname, sym in FILECITE_RE.findall(line):
                if not (src_dir / fname).exists():
                    problems.append(f"{doc.name}:{lineno}: cites source file '{fname}' not in src/")
                if sym not in src_words:
                    problems.append(f"{doc.name}:{lineno}: symbol '{sym}' (cited as {fname}: {sym}) not found in src/")
            for qualified in QUALIFIED_RE.findall(line):
                leaf = qualified.split("::")[-1]
                if leaf not in src_words:
                    problems.append(f"{doc.name}:{lineno}: symbol '{qualified}' not found in src/")
            if not in_fence:
                for sym in BARE_SYM_RE.findall(line):
                    if CAMEL_RE.match(sym) and sym not in src_words:
                        problems.append(f"{doc.name}:{lineno}: symbol '{sym}' (backticked) not found in src/")

            # Coupling references outside fenced blocks.
            if not in_fence and n_couplings:
                for num in COUPLING_REF_RE.findall(line):
                    if int(num) > n_couplings:
                        problems.append(
                            f"{doc.name}:{lineno}: 'coupling {num}' exceeds {n_couplings} couplings in SYNTHESIS_MODEL"
                        )

            # Script references, including fenced command examples.
            for ref in PYREF_RE.findall(line):
                if ref not in tool_scripts:
                    problems.append(f"{doc.name}:{lineno}: references Python script '{ref}' not in tools/ or tests/")

    # Also check references in tool docstrings and comments.
    for tool in sorted(tools_dir.glob("*.py")):
        for lineno, line in lines_of(tool.read_text(encoding="utf-8")):
            for ref in PYREF_RE.findall(line):
                if ref not in tool_scripts:
                    problems.append(f"{tool.name}:{lineno}: references Python script '{ref}' not in tools/ or tests/")

    problems.extend(tools_index_problems(tools_dir, tool_scripts))

    if problems:
        print(f"{len(problems)} doc cross-reference problem(s):\n")
        for p in problems:
            print(f"  {p}")
    else:
        print("docs: all cross-references resolve")
    return len(problems)


if __name__ == "__main__":
    sys.exit(main())
