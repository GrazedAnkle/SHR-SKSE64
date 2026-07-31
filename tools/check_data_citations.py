#!/usr/bin/env python3
"""Verify documentation figures and synthesis calibrations against measurements.json.

tools/data_citations.toml maps prose citations and tuned constants to numeric
leaves in docs/references/measurements.json. Citation checks support direct
numeric leaves only; calibration entries may use explicit formulas. Exit code is
the number of problems.

Stdlib-only so it can run in CI (Python 3.11+ for tomllib).

Usage: python tools/check_data_citations.py [--root .]
"""
from __future__ import annotations

import argparse
import ast
import json
import operator
import re
import sys
import tomllib
from pathlib import Path
from typing import Any

try:  # Direct script execution puts tools/ on sys.path; package imports put the repo root there.
    from reference_states import resolve, validate_embedded_state
except ModuleNotFoundError:  # pragma: no cover - exercised only by package-style callers
    from tools.reference_states import resolve, validate_embedded_state


NUMBER_RE = re.compile(r"(?<![\w.])-?(?:\d+(?:\.\d+)?|\.\d+)(?![\w.])")


def doc_path(root: Path, name: str) -> Path:
    path = Path(name)
    if path.parts and path.parts[0] == "docs":
        return root / path
    return root / "docs" / path


def alternatives(text: str, expected: str) -> str:
    """Give a compact hint when a document has numbers but not the expected one."""
    values: list[str] = []
    for value in NUMBER_RE.findall(text):
        if value != expected and value not in values:
            values.append(value)
        if len(values) == 8:
            break
    return ", ".join(values) if values else "none"


TUNED_TAGS = ("ref", "dsp")
TAG_RE = re.compile(r"\[(physio|ref|dsp|game|asset|util)\]")
CONSTEXPR_RE = re.compile(r"^\s*constexpr\b.*?\b([A-Za-z_]\w*)\s*=\s*(-?[\d.]+)")


def tuned_constants(root: Path) -> dict[str, float]:
    """Return [ref]- and [dsp]-tagged constants using the check_constants.py block-tag rule."""
    path = root / "src" / "core" / "Constants.hpp"
    if not path.exists():
        return {}
    found: dict[str, float] = {}
    current: str | None = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or "=====" in line:
            current = None
        tag_here = TAG_RE.search(line)
        decl = CONSTEXPR_RE.match(line)
        if decl:
            tag = tag_here.group(1) if tag_here else current
            if tag in TUNED_TAGS:
                found[decl.group(1)] = float(decl.group(2))
            continue
        if line.strip().startswith("//") and tag_here:
            current = tag_here.group(1)
    return found


def float_constants(root: Path) -> dict[str, float]:
    """Return numeric-literal constants for calibration formula evaluation.

    Ignores provenance tags and skips expressions that reference other symbols.
    """
    path = root / "src" / "core" / "Constants.hpp"
    if not path.exists():
        return {}
    found: dict[str, float] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        decl = CONSTEXPR_RE.match(line)
        if decl:
            found[decl.group(1)] = float(decl.group(2))
    return found


_LEAF_RE = re.compile(r"\{([^{}]+)\}")
_BINOPS = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
           ast.Div: operator.truediv, ast.Pow: operator.pow}
_UNARYOPS = {ast.UAdd: operator.pos, ast.USub: operator.neg}


def _eval_ast(node: ast.AST, names: dict[str, float]) -> float:
    """Evaluate a restricted arithmetic AST: numbers, named constants, + - * / ** and unary sign."""
    if isinstance(node, ast.Expression):
        return _eval_ast(node.body, names)
    if isinstance(node, ast.Constant):
        if isinstance(node.value, (int, float)) and not isinstance(node.value, bool):
            return float(node.value)
        raise ValueError(f"non-numeric literal {node.value!r}")
    if isinstance(node, ast.Name):
        if node.id not in names:
            raise ValueError(f"unknown constant '{node.id}'")
        return names[node.id]
    if isinstance(node, ast.BinOp) and type(node.op) in _BINOPS:
        return _BINOPS[type(node.op)](_eval_ast(node.left, names), _eval_ast(node.right, names))
    if isinstance(node, ast.UnaryOp) and type(node.op) in _UNARYOPS:
        return _UNARYOPS[type(node.op)](_eval_ast(node.operand, names))
    raise ValueError("only + - * / ** and named constants are allowed")


def eval_spec(spec: Any, constants: dict[str, float], measurements: Any) -> tuple[float | None, str | None]:
    """Evaluate a calibration value or formula.

    Formulas may use Constants.hpp names and `{dotted.key}` measurement leaves.
    Returns `(value, error)` with exactly one member set.
    """
    if isinstance(spec, bool) or not isinstance(spec, (int, float, str)):
        return None, f"must be a number or formula string, got {type(spec).__name__}"
    if isinstance(spec, (int, float)):
        return float(spec), None

    missing: list[str] = []

    def sub(match: re.Match) -> str:
        try:
            value = resolve(measurements, match.group(1))
        except (KeyError, ValueError):
            missing.append(match.group(1))
            return "0"
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            missing.append(match.group(1))
            return "0"
        return repr(float(value))

    expr = _LEAF_RE.sub(sub, spec)
    if missing:
        return None, f"leaf(s) do not resolve in measurements.json: {', '.join(missing)}"
    try:
        return float(_eval_ast(ast.parse(expr, mode="eval"), constants)), None
    except (SyntaxError, ValueError, ZeroDivisionError) as exc:
        return None, f"formula error in {spec!r}: {exc}"


def state_target_problems(measurements: Any, key: Any, prefix: str) -> list[str]:
    """Validate a calibration's state and every member of a mixed-state target."""
    if not isinstance(key, str) or not key:
        return [f"{prefix}: 'state_key' must be a non-empty string"]
    try:
        state = resolve(measurements, key)
    except (KeyError, ValueError) as exc:
        return [f"{prefix}: state_key '{key}' does not resolve in measurements.json ({exc})"]

    if isinstance(state, dict) and state.get("kind") == "mixed_reference_states":
        problems = [f"{prefix} / {key}: {problem}" for problem in validate_embedded_state(state)]
        for member in state.get("members", []):
            try:
                member_state = resolve(measurements, member)
            except (KeyError, ValueError) as exc:
                problems.append(f"{prefix} / {key}: member '{member}' does not resolve ({exc})")
                continue
            expected_scope = member[:-len(".state")] if member.endswith(".state") else None
            problems.extend(
                f"{prefix} / {key} / {member}: {problem}"
                for problem in validate_embedded_state(member_state, expected_scope=expected_scope)
            )
    else:
        expected_scope = key[:-len(".state")] if key.endswith(".state") else None
        problems = [
            f"{prefix} / {key}: {problem}"
            for problem in validate_embedded_state(state, expected_scope=expected_scope)
        ]
    return problems


def check_calibrations(root: Path, manifest: dict, measurements: Any) -> list[str]:
    """Require every tuned constant to be calibrated or explicitly uncited."""
    problems: list[str] = []
    constants = tuned_constants(root)
    if not constants:
        return ["src/core/Constants.hpp: no [ref]/[dsp] constants found (parser broken?)"]
    fconstants = float_constants(root)

    declared: set[str] = set()
    for index, entry in enumerate(manifest.get("calibrate", []), start=1):
        prefix = f"manifest calibrate[{index}]"
        name = entry.get("constant")
        key = entry.get("key")
        if not isinstance(name, str) or not isinstance(key, str):
            problems.append(f"{prefix}: 'constant' and 'key' must be strings")
            continue
        declared.add(name)
        for field in ("engine", "tolerance", "ruler", "state_key", "state_compatibility"):
            if field not in entry:
                problems.append(f"{prefix} / {name}: missing '{field}'")
        if name not in constants:
            problems.append(f"{prefix} / {name}: not a [ref]/[dsp] constant in Constants.hpp")
        problems += state_target_problems(measurements, entry.get("state_key"), f"{prefix} / {name}")
        if not isinstance(entry.get("state_compatibility"), str) or not entry.get("state_compatibility"):
            problems.append(f"{prefix} / {name}: 'state_compatibility' must be a non-empty string")
        try:
            leaf = resolve(measurements, key)
        except (KeyError, ValueError) as exc:
            problems.append(f"{prefix} / {name} / calibration target '{key}' does not resolve "
                            f"in measurements.json ({exc}) - a target with no leaf behind it is exactly "
                            f"the self-referential bug this relation exists to catch")
            continue
        engine, engine_err = eval_spec(entry.get("engine"), fconstants, measurements)
        tolerance, tol_err = eval_spec(entry.get("tolerance"), fconstants, measurements)
        if engine_err:
            problems.append(f"{prefix} / {name}: engine {engine_err}")
        if tol_err:
            problems.append(f"{prefix} / {name}: tolerance {tol_err}")
        if isinstance(leaf, (int, float)) and engine is not None and tolerance is not None:
            if abs(engine - leaf) > tolerance:
                problems.append(f"{prefix} / {name}: engine reads {engine:.3f} against reference {leaf} "
                                f"({key}), a gap of {abs(engine - leaf):.3f} > tolerance {tolerance:.3f}")

    for index, entry in enumerate(manifest.get("uncited", []), start=1):
        name = entry.get("constant")
        if not isinstance(name, str):
            problems.append(f"manifest uncited[{index}]: 'constant' must be a string")
            continue
        if not entry.get("reason"):
            problems.append(f"manifest uncited[{index}] / {name}: needs a 'reason' - "
                            f"declaring a constant uncited is a decision, not an omission")
        if name in declared:
            problems.append(f"{name}: declared both calibrated and uncited")
        state_keys = entry.get("state_keys", [])
        if not isinstance(state_keys, list) or not all(isinstance(v, str) for v in state_keys):
            problems.append(f"manifest uncited[{index}] / {name}: 'state_keys' must be a string array")
        else:
            for state_key in state_keys:
                problems += state_target_problems(
                    measurements, state_key, f"manifest uncited[{index}] / {name}"
                )
        declared.add(name)

    for name in sorted(set(constants) - declared):
        problems.append(f"Constants.hpp / {name}: tuned ([ref]/[dsp]) but neither bound to a reference "
                        f"leaf ([[calibrate]]) nor declared uncited ([[uncited]]) in data_citations.toml")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".", help="repo root (default: cwd)")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    manifest_path = root / "tools" / "data_citations.toml"
    measurements_path = root / "docs" / "references" / "measurements.json"
    problems: list[str] = []

    if not manifest_path.exists():
        print(f"missing manifest: {manifest_path}", file=sys.stderr)
        return 1
    if not measurements_path.exists():
        print(f"missing measurements: {measurements_path}", file=sys.stderr)
        return 1

    try:
        manifest = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as exc:
        print(f"invalid manifest {manifest_path}: {exc}", file=sys.stderr)
        return 1
    try:
        measurements = json.loads(measurements_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"invalid measurements {measurements_path}: {exc}", file=sys.stderr)
        return 1

    entries = manifest.get("cite")
    if not isinstance(entries, list):
        problems.append("manifest: expected one or more [[cite]] entries")
        entries = []

    for index, entry in enumerate(entries, start=1):
        prefix = f"manifest cite[{index}]"
        if not isinstance(entry, dict):
            problems.append(f"{prefix}: entry must be a table")
            continue
        key = entry.get("key")
        precision = entry.get("precision")
        docs = entry.get("docs")
        if not isinstance(key, str) or not key:
            problems.append(f"{prefix}: key must be a non-empty string")
            continue
        if not isinstance(precision, int) or isinstance(precision, bool) or precision < 0:
            problems.append(f"{prefix} / {key}: precision must be a non-negative integer")
            continue
        if not isinstance(docs, list) or not docs or not all(isinstance(doc, str) for doc in docs):
            problems.append(f"{prefix} / {key}: docs must be a non-empty string array")
            continue

        try:
            value = resolve(measurements, key)
        except (KeyError, ValueError) as exc:
            problems.append(f"manifest / {key} / expected source value / found-instead: unresolved ({exc})")
            continue
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            problems.append(f"manifest / {key} / expected numeric source value / found-instead: {type(value).__name__}")
            continue
        expected = f"{value:.{precision}f}"
        pattern = re.compile(rf"(?<![\w.]){re.escape(expected)}(?![\w.])")

        for doc_name in docs:
            path = doc_path(root, doc_name)
            if not path.is_file():
                problems.append(f"{doc_name} / {key} / expected {expected} / found-instead: missing document")
                continue
            text = path.read_text(encoding="utf-8")
            if not pattern.search(text):
                problems.append(
                    f"{doc_name} / {key} / expected {expected} / "
                    f"found-instead: {alternatives(text, expected)}"
                )

    problems += check_calibrations(root, manifest, measurements)

    if problems:
        print(f"{len(problems)} data citation problem(s):\n")
        for problem in problems:
            print(f"  {problem}")
    else:
        cited = len(manifest.get("calibrate", []))
        uncited = len(manifest.get("uncited", []))
        print(f"data citations: all guarded figures match measurements.json; "
              f"{cited} constant(s) bound to a reference leaf, {uncited} declared uncited")
    return len(problems)


if __name__ == "__main__":
    sys.exit(main())
