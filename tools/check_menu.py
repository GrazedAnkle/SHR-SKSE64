#!/usr/bin/env python3
"""Verify that the Mod Configuration Menu layout and the settings registry agree.

The menu is data in three files that no compiler reads together: the layout in
``contrib/Distribution/MCM/Config/SHR/config.json``, the framework's own defaults beside it in
``settings.ini``, and the registry in ``src/adapter/Settings.cpp`` that every edit is dispatched
against. A control naming a setting the registry does not have fails silently in the player's hands -
the slider moves and nothing happens - so the agreement is checked here instead.

What this gate enforces:

* every setting control names a registry id, and passes that same id as the action's argument;
* a command control (reset, which runs on the config script instance) names no setting at all, so it
  cannot be mistaken for one that stores something;
* the action's function and the control's source type match the registry's kind, so a value cannot
  cross the Papyrus boundary as the wrong type;
* a slider's own range sits inside the registry domain, which is what makes "no legal slider position
  produces an update the runtime rejects" true rather than merely intended; and
* every registry setting is reachable from some page, so a setting cannot be added and forgotten.

The regexes match a narrow, stable subset of the registry's grammar. If a richer parse is ever needed,
generate the tables from one source instead of extending the parser.

Stdlib-only so it can run in CI.

Usage:  python tools/check_menu.py [--root .]
"""

from __future__ import annotations

import argparse
import configparser
import json
import re
import sys
from pathlib import Path

CONFIG_JSON = Path("contrib/Distribution/MCM/Config/SHR/config.json")
SETTINGS_INI = Path("contrib/Distribution/MCM/Config/SHR/settings.ini")
REGISTRY = Path("src/adapter/Settings.cpp")
CONSTANTS = Path("src/core/Constants.hpp")

# The native each kind is written and read through, and the source type MCM Helper stores it as.
KIND_FUNCTIONS = {
    "Float": ("SetFloat", "ModSettingFloat"),
    "Bool": ("SetBool", "ModSettingBool"),
    "Key": ("SetInt", "ModSettingInt"),
}

# A setting is edited through a global native and names a registry id. A command runs something on
# the config script instance - reset, which has to repaint - and names no setting at all. Telling
# them apart by action type is what lets the registry check stay strict about everything else.
SETTING_ACTION = "CallGlobalFunction"
COMMAND_ACTION = "CallFunction"

TABLE_RE = re.compile(r"constexpr std::array<\w+, \w+> (?P<name>\w+)\{ \{(?P<body>.*?)\n    \} \};", re.S)
CONSTEXPR_RE = re.compile(r"^\s*constexpr\s+float\s+(?P<name>\w+)\s*=\s*(?P<value>[^;/]+)", re.M)
ARITHMETIC_RE = re.compile(r"^[\d\s.+\-*/()]+$")
COMMENT_RE = re.compile(r"//[^\n]*")


def _constants(path: Path) -> dict[str, float]:
    values: dict[str, float] = {}
    for match in CONSTEXPR_RE.finditer(path.read_text(encoding="utf-8")):
        expression = match.group("value").replace("F", "").strip()
        if ARITHMETIC_RE.match(expression):
            values[match.group("name")] = float(eval(expression))  # noqa: S307 - digits and operators only
    return values


def _resolve(token: str, constants: dict[str, float]) -> float | None:
    token = token.strip()
    if token.startswith("C::"):
        return constants.get(token[3:])
    expression = token.replace("F", "").strip()
    return float(eval(expression)) if ARITHMETIC_RE.match(expression) else None  # noqa: S307


def _fields(row: str) -> list[str]:
    """Split one braced row on its top-level commas, so RecordType('SRHR') stays one field."""
    fields: list[str] = []
    depth = 0
    current = ""
    for character in row:
        if character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
        if character == "," and depth == 0:
            fields.append(current)
            current = ""
            continue
        current += character
    fields.append(current)
    return [field.strip() for field in fields if field.strip()]


def _registry(root: Path) -> tuple[dict[str, dict[str, object]], list[str]]:
    """Map each setting id to its kind and domain, from the two tables in the registry."""
    # Comments are dropped first: a row is recognized by starting with its id, and an explanatory
    # comment above one would otherwise hide it.
    text = COMMENT_RE.sub("", (root / REGISTRY).read_text(encoding="utf-8"))
    constants = _constants(root / CONSTANTS)
    settings: dict[str, dict[str, object]] = {}
    problems: list[str] = []

    for table in TABLE_RE.finditer(text):
        # A Subject row carries the co-save 4CC and its text; a Profile row does not.
        offset = 2 if table.group("name") == "SubjectTable" else 0
        for raw in table.group("body").split("},"):
            row = raw.strip().lstrip("{").strip()
            if not row.startswith('"'):
                continue
            fields = _fields(row)
            identifier = fields[0].strip('"')
            kind = fields[1 + offset].removeprefix("Kind::")
            low = _resolve(fields[2 + offset], constants)
            high = _resolve(fields[3 + offset], constants)
            if kind not in KIND_FUNCTIONS:
                problems.append(f"{REGISTRY}: {identifier} has unknown kind {kind}")
                continue
            if low is None or high is None:
                problems.append(f"{REGISTRY}: {identifier} has a domain this parser cannot resolve")
                continue
            settings[identifier] = {"Kind": kind, "Min": low, "Max": high}

    if not settings:
        problems.append(f"{REGISTRY}: no settings parsed; the table grammar has moved")
    return settings, problems


def _controls(root: Path) -> list[dict[str, object]]:
    layout = json.loads((root / CONFIG_JSON).read_text(encoding="utf-8"))
    return [
        control
        for page in layout.get("pages", [])
        for control in page.get("content", [])
        if "action" in control
    ]


def _check_control(control: dict[str, object], settings: dict[str, dict[str, object]]) -> list[str]:
    identifier = str(control.get("id", "<unnamed>"))
    setting = settings.get(identifier)
    if setting is None:
        return [f"{CONFIG_JSON}: control {identifier} is not in the registry"]

    problems: list[str] = []
    function, source = KIND_FUNCTIONS[str(setting["Kind"])]
    action = control.get("action", {})
    options = control.get("valueOptions", {})

    if action.get("function") != function:
        problems.append(
            f"{CONFIG_JSON}: {identifier} is a {setting['Kind']} setting but calls "
            f"{action.get('function')}, not {function}"
        )
    if options.get("sourceType") != source:
        problems.append(
            f"{CONFIG_JSON}: {identifier} stores as {options.get('sourceType')}, not {source}"
        )

    # The id is passed twice - as the control's own id and as the action's first argument - and the
    # plugin only ever sees the second, so a mismatch would write a different setting than displayed.
    params = action.get("params", [])
    if not params or params[0] != identifier:
        problems.append(f"{CONFIG_JSON}: {identifier} passes {params[:1]} as its action id")

    # A slider is the only control offering positions the player can pick; the rest carry the whole
    # type. Its range must be a sub-range of the domain, or a legal position would be rejected.
    if control.get("type") == "slider":
        for bound in ("min", "max"):
            value = options.get(bound)
            if value is None:
                problems.append(f"{CONFIG_JSON}: {identifier} has no {bound}")
            elif not setting["Min"] <= float(value) <= setting["Max"]:
                problems.append(
                    f"{CONFIG_JSON}: {identifier} {bound} {value} is outside the registry domain "
                    f"[{setting['Min']}, {setting['Max']}]"
                )
    return problems


def _check_defaults(root: Path, settings: dict[str, dict[str, object]]) -> list[str]:
    parser = configparser.ConfigParser()
    parser.read(root / SETTINGS_INI, encoding="utf-8")
    present = {f"{name}:{section}" for section in parser.sections() for name in parser[section]}
    # Case-insensitive: configparser lowercases keys, and the ids are mixed case.
    lowered = {identifier.lower() for identifier in present}
    return [
        f"{SETTINGS_INI}: no default for {identifier}"
        for identifier in settings
        if identifier.lower() not in lowered
    ]


def _check_command(control: dict[str, object]) -> list[str]:
    """A command must not look like a setting, or it would appear to store something it does not."""
    label = str(control.get("text", "<unlabelled>"))
    action = control.get("action", {})

    problems = [
        f"{CONFIG_JSON}: command {label!r} has no {field}"
        for field in ("form", "scriptName", "function")
        if not action.get(field)
    ]
    if "id" in control or "valueOptions" in control:
        problems.append(f"{CONFIG_JSON}: command {label!r} declares a setting id or source type")
    return problems


def check(root: Path) -> list[str]:
    settings, problems = _registry(root)
    if problems:
        return problems

    controls = _controls(root)
    unknown = [
        control
        for control in controls
        if control["action"].get("type") not in (SETTING_ACTION, COMMAND_ACTION)
    ]
    problems.extend(
        f"{CONFIG_JSON}: control {control.get('id', control.get('text'))!r} has an unrecognized "
        f"action type {control['action'].get('type')!r}"
        for control in unknown
    )

    settings_controls = [c for c in controls if c["action"].get("type") == SETTING_ACTION]
    for control in settings_controls:
        problems.extend(_check_control(control, settings))
    for control in (c for c in controls if c["action"].get("type") == COMMAND_ACTION):
        problems.extend(_check_command(control))

    reachable = {str(control.get("id")) for control in settings_controls}
    problems.extend(
        f"{CONFIG_JSON}: registry setting {identifier} has no control"
        for identifier in settings
        if identifier not in reachable
    )
    problems.extend(_check_defaults(root, settings))
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."))
    arguments = parser.parse_args()

    problems = check(arguments.root)
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"{len(problems)} menu problem(s).", file=sys.stderr)
        return 1
    print("Menu layout and settings registry agree.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
