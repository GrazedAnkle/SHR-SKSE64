"""Shared pytest plumbing for the Python suite.

Must stay at ``tests/`` rather than in a subdirectory: ``pytest_addoption`` is honored only from initial
conftests - those in the ancestor chain of the invocation's arguments - so ``--module-dir`` registered
further down is an unrecognized argument to any invocation that does not name that directory.
"""
from __future__ import annotations

from pathlib import Path

import pytest

import core_offline


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--module-dir",
        type=Path,
        # core_offline owns the default so the suite and the tools cannot disagree.
        default=core_offline.DEFAULT_MODULE_DIR,
        help=core_offline.MODULE_DIR_HELP,
    )


@pytest.fixture(scope="session")
def shr_pybind(request: pytest.FixtureRequest):
    """The compiled offline binding, loaded once per session.

    Lazy so tests needing no binding still run when one is not built. Fails rather than skips: a skipped
    golden is indistinguishable from a passing one in CI.
    """
    module_dir = request.config.getoption("--module-dir")
    try:
        return core_offline.load_binding(module_dir)
    except SystemExit as error:
        pytest.fail(str(error), pytrace=False)


@pytest.fixture(scope="session")
def binding_summary(request: pytest.FixtureRequest) -> str:
    """Which binding this session loaded, for failures whose likeliest cause is the wrong one."""
    return core_offline.provenance_summary(request.config.getoption("--module-dir"))
