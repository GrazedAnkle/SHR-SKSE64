"""Shared pytest plumbing for the compiled-core golden checks."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
DEFAULT_MODULE_DIR = ROOT / "build" / "pybind"

sys.path.insert(0, str(TOOLS))
from core_offline import load_binding  # noqa: E402

_BINDING = None


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--module-dir",
        type=Path,
        default=DEFAULT_MODULE_DIR,
        help="directory containing the built shr_pybind module (default: build/pybind)",
    )


def pytest_sessionstart(session: pytest.Session) -> None:
    global _BINDING
    module_dir = session.config.getoption("--module-dir")
    try:
        _BINDING = load_binding(module_dir)
    except SystemExit as error:
        pytest.exit(str(error), returncode=1)


@pytest.fixture(scope="session")
def shr_pybind():
    return _BINDING
