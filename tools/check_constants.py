#!/usr/bin/env python3
"""Verify constant provenance and exact model-registry coverage.

Every ``constexpr`` in ``src/core/Constants.hpp`` must have a provenance tag and appear exactly once in
``ModelCoefficientRegistry.hpp``: either as a typed live field in one owning group or as an explicitly
classified non-live constant. A small deliberately broken fixture proves that missing, duplicate, and
mistyped entries are detected by this gate.

The regexes below match a narrow, stable subset of the two headers' grammar. If a richer parse is ever
needed - nested conditionals, macro-built entries, anything wanting real preprocessing - generate one
header from the other instead of extending the parser.

Stdlib-only so it can run in CI.

Usage:  python tools/check_constants.py [--root .]
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

TAGS = ("physio", "ref", "dsp", "game", "asset", "util")
NONLIVE_CLASSIFICATIONS = {
    "AssetFixed",
    "Derived",
    "Dormant",
    "GameIntegration",
    "Utility",
}

TAG_RE = re.compile(r"\[(" + "|".join(TAGS) + r")\]")
CONSTEXPR_RE = re.compile(r"^\s*constexpr\s+(?P<type>[A-Za-z_:][\w:]*)\s+(?P<name>[A-Za-z_]\w*)\s*=")
COMMENT_RE = re.compile(r"^\s*//")
BANNER_RE = re.compile(r"=====")
MACRO_START_RE = re.compile(r"^\s*#define\s+(SHR_[A-Z_]+)\(X\)\s*\\?\s*$")
LIVE_ENTRY_RE = re.compile(r"^\s*X\(\s*(?P<type>[^,]+),\s*(?P<name>[A-Za-z_]\w*)\s*\)\s*\\?\s*$")
NONLIVE_ENTRY_RE = re.compile(
    r"^\s*X\(\s*(?P<classification>[A-Za-z_]\w*),\s*(?P<type>[^,]+),"
    r"\s*(?P<name>[A-Za-z_]\w*)\s*,"
)
GROUP_ENTRY_RE = re.compile(
    r"^\s*X\(\s*(?P<public>[A-Za-z_]\w*),\s*(?P<type>[A-Za-z_]\w*),"
    r"\s*(?P<fields>SHR_[A-Z_]+)\s*\)\s*\\?\s*$"
)


@dataclass(frozen=True)
class Constant:
    Type: str
    Line: int


@dataclass(frozen=True)
class RegistryEntry:
    Type: str
    Owner: str
    Line: int


def _normalized_type(value: str) -> str:
    return "".join(value.split())


def _constants(path: Path) -> tuple[dict[str, Constant], list[str]]:
    values: dict[str, Constant] = {}
    problems: list[str] = []
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        match = CONSTEXPR_RE.match(line)
        if not match:
            continue
        name = match.group("name")
        if name in values:
            problems.append(
                f"{path.name}:{lineno}: duplicate constant '{name}' "
                f"(first declared on line {values[name].Line})"
            )
            continue
        values[name] = Constant(_normalized_type(match.group("type")), lineno)
    return values, problems


def _registry(path: Path) -> tuple[dict[str, RegistryEntry], list[str], dict[str, tuple[str, str]]]:
    entries: dict[str, RegistryEntry] = {}
    problems: list[str] = []
    groups: dict[str, tuple[str, str]] = {}
    current_macro: str | None = None

    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        start = MACRO_START_RE.match(line)
        if start:
            current_macro = start.group(1)
            continue
        if current_macro is None:
            continue

        if current_macro == "SHR_MODEL_COEFFICIENT_GROUPS":
            match = GROUP_ENTRY_RE.match(line)
            if match:
                fields = match.group("fields")
                if fields in groups:
                    problems.append(f"{path.name}:{lineno}: duplicate group field list '{fields}'")
                else:
                    groups[fields] = (match.group("public"), match.group("type"))
        elif current_macro == "SHR_NONLIVE_MODEL_CONSTANTS":
            match = NONLIVE_ENTRY_RE.match(line)
            if match:
                classification = match.group("classification")
                if classification not in NONLIVE_CLASSIFICATIONS:
                    problems.append(
                        f"{path.name}:{lineno}: '{match.group('name')}' has unknown "
                        f"non-live classification '{classification}'"
                    )
                _add_registry_entry(
                    entries,
                    problems,
                    path,
                    lineno,
                    match.group("name"),
                    match.group("type"),
                    classification,
                )
        elif current_macro.endswith("_COEFFICIENTS"):
            match = LIVE_ENTRY_RE.match(line)
            if match:
                _add_registry_entry(
                    entries,
                    problems,
                    path,
                    lineno,
                    match.group("name"),
                    match.group("type"),
                    current_macro,
                )

        if line.rstrip().endswith("\\"):
            continue
        current_macro = None

    live_owners = {entry.Owner for entry in entries.values() if entry.Owner.startswith("SHR_")}
    for owner in sorted(live_owners - set(groups)):
        problems.append(f"{path.name}: live field list '{owner}' is absent from SHR_MODEL_COEFFICIENT_GROUPS")
    for fields in sorted(set(groups) - live_owners):
        problems.append(f"{path.name}: group field list '{fields}' has no live entries")
    return entries, problems, groups


def _add_registry_entry(
    entries: dict[str, RegistryEntry],
    problems: list[str],
    path: Path,
    lineno: int,
    name: str,
    scalar_type: str,
    owner: str,
) -> None:
    entry = RegistryEntry(_normalized_type(scalar_type), owner, lineno)
    previous = entries.get(name)
    if previous is not None:
        problems.append(
            f"{path.name}:{lineno}: duplicate registry entry '{name}' "
            f"(first registered on line {previous.Line})"
        )
        return
    entries[name] = entry


def _registry_problems(constants_path: Path, registry_path: Path) -> tuple[list[str], int, int]:
    constants, problems = _constants(constants_path)
    registry, registry_parse_problems, _groups = _registry(registry_path)
    problems.extend(registry_parse_problems)

    for name in sorted(set(constants) - set(registry)):
        problems.append(
            f"{registry_path.name}: constant '{name}' is neither a live field nor explicitly non-live"
        )
    for name in sorted(set(registry) - set(constants)):
        problems.append(
            f"{registry_path.name}:{registry[name].Line}: registry entry '{name}' "
            f"has no declaration in {constants_path.name}"
        )
    for name in sorted(set(constants) & set(registry)):
        expected = constants[name].Type
        actual = registry[name].Type
        if expected != actual:
            problems.append(
                f"{registry_path.name}:{registry[name].Line}: '{name}' type {actual} "
                f"does not match {constants_path.name} type {expected}"
            )

    live_count = sum(entry.Owner.startswith("SHR_") for entry in registry.values())
    return problems, live_count, len(registry) - live_count


def _provenance_problems(path: Path) -> tuple[list[str], dict[str, int]]:
    current: str | None = None
    problems: list[str] = []
    counts: dict[str, int] = {tag: 0 for tag in TAGS}

    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or BANNER_RE.search(line):
            current = None
        is_comment = COMMENT_RE.match(line) is not None
        tag_here = TAG_RE.search(line)

        declaration = CONSTEXPR_RE.match(line)
        if declaration:
            tag = tag_here.group(1) if tag_here else current
            if tag is None:
                problems.append(
                    f"Constants.hpp:{lineno}: '{declaration.group('name')}' has no provenance tag"
                )
            else:
                counts[tag] += 1
            continue

        if is_comment and tag_here:
            current = tag_here.group(1)
    return problems, counts


def _check_negative_fixture(root: Path) -> list[str]:
    fixture = root / "tests" / "fixtures" / "model_coefficient_registry"
    problems, _live, _nonlive = _registry_problems(
        fixture / "Constants.hpp",
        fixture / "ModelCoefficientRegistry.hpp",
    )
    expected = ("duplicate registry entry 'Alpha'", "'Alpha' type int", "constant 'Count'")
    missing = [text for text in expected if not any(text in problem for problem in problems)]
    if missing:
        return ["negative model-registry fixture no longer proves: " + ", ".join(missing)]
    return []


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".", help="repo root (default: cwd)")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    constants_path = root / "src" / "core" / "Constants.hpp"
    registry_path = root / "src" / "core" / "ModelCoefficientRegistry.hpp"
    if not constants_path.exists() or not registry_path.exists():
        print("missing src/core/Constants.hpp or src/core/ModelCoefficientRegistry.hpp", file=sys.stderr)
        return 1

    provenance, counts = _provenance_problems(constants_path)
    registry, live_count, nonlive_count = _registry_problems(constants_path, registry_path)
    fixture = _check_negative_fixture(root)
    problems = [*provenance, *registry, *fixture]

    if problems:
        print(f"{len(problems)} constant/registry problem(s):\n")
        for problem in problems:
            print(f"  {problem}")
        return len(problems)

    total = sum(counts.values())
    breakdown = ", ".join(f"{counts[tag]} {tag}" for tag in TAGS if counts[tag])
    print(f"constants: all {total} tagged ({breakdown})")
    print(
        f"model coefficient registry: {live_count} live, {nonlive_count} explicitly non-live; "
        "exact names and scalar types"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
