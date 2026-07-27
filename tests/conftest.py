"""Shared pytest plumbing for the Python suite.

Must stay at ``tests/`` rather than in a subdirectory: ``pytest_addoption`` is honored only from initial
conftests - those in the ancestor chain of the invocation's arguments - so ``--module-dir`` registered
further down is an unrecognized argument to any invocation that does not name that directory.
"""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
DEFAULT_MODULE_DIR = ROOT / "build" / "pybind"

sys.path.insert(0, str(TOOLS))
from core_offline import load_binding  # noqa: E402


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--module-dir",
        type=Path,
        default=DEFAULT_MODULE_DIR,
        help="directory containing the built shr_pybind module (default: build/pybind)",
    )


@pytest.fixture(scope="session")
def shr_pybind(request: pytest.FixtureRequest):
    """The compiled offline binding, loaded once per session.

    Lazy so tests needing no binding still run when one is not built. Fails rather than skips: a skipped
    golden is indistinguishable from a passing one in CI.
    """
    module_dir = request.config.getoption("--module-dir")
    try:
        return load_binding(module_dir)
    except SystemExit as error:
        pytest.fail(str(error), pytrace=False)
