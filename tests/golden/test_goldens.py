"""Golden verification cases for every compiled-core offline domain."""
from __future__ import annotations

import json
from collections.abc import Callable
from types import ModuleType

import pytest

import check_beat_renderer_golden
import check_rhythm_mapping_golden
import check_source_conditioning_golden
import check_trajectory_golden

GoldenBuilder = Callable[[ModuleType], dict[str, object]]
GoldenDiff = Callable[[dict, dict], list[str]]

CHECKS = (
    pytest.param(
        check_source_conditioning_golden.GOLDEN,
        check_source_conditioning_golden._render,
        check_source_conditioning_golden._diff,
        id="source-conditioning",
    ),
    pytest.param(
        check_beat_renderer_golden.GOLDEN,
        check_beat_renderer_golden._render,
        check_beat_renderer_golden._diff,
        id="beat-renderer",
    ),
    pytest.param(
        check_rhythm_mapping_golden.GOLDEN,
        check_rhythm_mapping_golden._build,
        check_rhythm_mapping_golden._diff,
        id="rhythm-mapping",
    ),
    pytest.param(
        check_trajectory_golden.GOLDEN,
        check_trajectory_golden._build,
        check_trajectory_golden._diff,
        id="trajectory",
    ),
)


@pytest.mark.parametrize(("manifest_path", "build_manifest", "diff"), CHECKS)
def test_compiled_core_golden(
    shr_pybind: ModuleType,
    manifest_path,
    build_manifest: GoldenBuilder,
    diff: GoldenDiff,
) -> None:
    if not manifest_path.exists():
        pytest.fail(
            f"golden manifest missing: {manifest_path.relative_to(manifest_path.parents[2])} "
            "(run its checker CLI with --capture)",
            pytrace=False,
        )

    expected = json.loads(manifest_path.read_text(encoding="utf-8"))
    problems = diff(expected, build_manifest(shr_pybind))
    if problems:
        pytest.fail("\n".join(problems), pytrace=False)
