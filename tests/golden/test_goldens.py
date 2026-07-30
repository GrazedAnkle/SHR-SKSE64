"""Golden verification cases for every compiled-core offline domain.

One case per domain in ``tools/golden_registry.py``, driven through that registry's surface, so this
file names no domain individually.
"""
from __future__ import annotations

import json
from types import ModuleType

import pytest

import golden_registry

CHECKS = tuple(pytest.param(domain, id=domain.ID) for domain in golden_registry.DOMAINS)


@pytest.mark.parametrize("domain", CHECKS)
def test_compiled_core_golden(shr_pybind: ModuleType, binding_summary: str, domain: ModuleType) -> None:
    if not domain.MANIFEST.exists():
        pytest.fail(
            f"golden manifest missing: {domain.MANIFEST.relative_to(domain.MANIFEST.parents[2])} "
            f"(author it with: python tools/capture_goldens.py {domain.ID})",
            pytrace=False,
        )

    expected = json.loads(domain.MANIFEST.read_text(encoding="utf-8"))
    problems = domain.diff(expected, domain.build(shr_pybind))
    if problems:
        pytest.fail(
            f"compiled core DIVERGED from the {domain.ID} golden "
            f"({len(problems)} difference(s)):\n"
            f"{golden_registry.format_problems(problems)}\n"
            f"Binding under test: {binding_summary}\n"
            f"Recapture only if the change is intended: "
            f"python tools/capture_goldens.py {domain.ID}",
            pytrace=False,
        )
