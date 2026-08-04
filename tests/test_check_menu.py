"""The menu gate must fail on the disagreements it exists to catch, not just pass on the real files.

Each case starts from the repository's own layout and breaks exactly one thing, so a case that stops
failing means the check regressed rather than that the fixture drifted.
"""

from __future__ import annotations

import json
import shutil
from pathlib import Path

import pytest

from check_menu import CONFIG_JSON, check

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture
def layout(tmp_path: Path) -> Path:
    """A copy of the real menu and registry, ready to be broken one field at a time."""
    for relative in (
        CONFIG_JSON,
        Path("contrib/Distribution/MCM/Config/SHR/settings.ini"),
        Path("src/adapter/Settings.cpp"),
        Path("src/core/Constants.hpp"),
    ):
        destination = tmp_path / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / relative, destination)
    return tmp_path


def _controls(root: Path) -> tuple[dict, list[dict]]:
    layout = json.loads((root / CONFIG_JSON).read_text(encoding="utf-8"))
    return layout, [control for page in layout["pages"] for control in page["content"]]


def _write(root: Path, layout: dict) -> None:
    (root / CONFIG_JSON).write_text(json.dumps(layout, indent=2), encoding="utf-8")


def _slider(controls: list[dict]) -> dict:
    return next(control for control in controls if control.get("type") == "slider")


def test_the_repository_agrees_with_itself() -> None:
    assert check(ROOT) == []


def test_an_unknown_control_id_is_caught(layout: Path) -> None:
    document, controls = _controls(layout)
    _slider(controls)["id"] = "fNotASetting:Subject"
    _write(layout, document)

    assert any("not in the registry" in problem for problem in check(layout))


def test_a_slider_reaching_past_its_domain_is_caught(layout: Path) -> None:
    document, controls = _controls(layout)
    slider = _slider(controls)
    slider["valueOptions"]["max"] = 10_000
    _write(layout, document)

    assert any("outside the registry domain" in problem for problem in check(layout))


def test_the_wrong_native_for_the_kind_is_caught(layout: Path) -> None:
    document, controls = _controls(layout)
    _slider(controls)["action"]["function"] = "SetBool"
    _write(layout, document)

    assert any("not SetFloat" in problem for problem in check(layout))


def test_an_action_writing_a_different_setting_is_caught(layout: Path) -> None:
    document, controls = _controls(layout)
    slider = _slider(controls)
    slider["action"]["params"] = ["fArrhythmia:Subject", "{value}"]
    _write(layout, document)

    assert any("as its action id" in problem for problem in check(layout))


def test_a_setting_with_no_control_is_caught(layout: Path) -> None:
    document, controls = _controls(layout)
    orphan = _slider(controls)
    for page in document["pages"]:
        page["content"] = [control for control in page["content"] if control is not orphan]
    _write(layout, document)

    assert any("has no control" in problem for problem in check(layout))


def test_a_missing_framework_default_is_caught(layout: Path) -> None:
    path = layout / "contrib/Distribution/MCM/Config/SHR/settings.ini"
    kept = [line for line in path.read_text(encoding="utf-8").splitlines() if "fVolume" not in line]
    path.write_text("\n".join(kept), encoding="utf-8")

    assert any("no default for" in problem for problem in check(layout))


def _command(controls: list[dict]) -> dict:
    return next(c for c in controls if c.get("action", {}).get("type") == "CallFunction")


def test_a_command_masquerading_as_a_setting_is_caught(layout: Path) -> None:
    document, controls = _controls(layout)
    _command(controls)["id"] = "fVolume:Profile"
    _write(layout, document)

    assert any("declares a setting id" in problem for problem in check(layout))


def test_a_command_missing_its_form_is_caught(layout: Path) -> None:
    document, controls = _controls(layout)
    del _command(controls)["action"]["form"]
    _write(layout, document)

    assert any("has no form" in problem for problem in check(layout))


def test_an_unrecognized_action_type_is_caught(layout: Path) -> None:
    document, controls = _controls(layout)
    _slider(controls)["action"]["type"] = "CallSomethingElse"
    _write(layout, document)

    assert any("unrecognized action type" in problem for problem in check(layout))
