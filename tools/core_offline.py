"""Shared plumbing for thin offline clients of the compiled ``shr_core`` binding."""

from __future__ import annotations

import importlib
import json
import sys
from pathlib import Path

import numpy as np
import soundfile as sf


ROOT = Path(__file__).resolve().parent.parent

# Written by cmake/publish_pybind_module.cmake; README.md (Building) owns what it means.
DEFAULT_MODULE_DIR = ROOT / "build" / "module"

# Shared so no tool's --help can drift from the default it applies.
MODULE_DIR_HELP = (
    "directory containing the built shr_pybind module "
    f"(default: {DEFAULT_MODULE_DIR.relative_to(ROOT).as_posix()})"
)


def load_binding(module_dir: str | Path = DEFAULT_MODULE_DIR):
    """Import ``shr_pybind`` from a selected build directory."""
    resolved = str(Path(module_dir).resolve())
    if resolved not in sys.path:
        sys.path.insert(0, resolved)
    try:
        return importlib.import_module("shr_pybind")
    except ImportError as error:
        raise SystemExit(
            f"cannot import shr_pybind from {module_dir} ({error}); "
            "build it with any preset that enables BUILD_PYBIND (cmake --preset Dev-Clang), "
            "or for a plugin-free tree: python tools/build_pybind.py"
        ) from error


def binding_provenance(module_dir: str | Path = DEFAULT_MODULE_DIR) -> dict[str, str] | None:
    """What the publishing build recorded about the binding in ``module_dir``, if anything did."""
    try:
        return json.loads((Path(module_dir) / "binding.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def provenance_summary(module_dir: str | Path = DEFAULT_MODULE_DIR) -> str:
    """One line naming the binding under test, for failure messages."""
    stamp = binding_provenance(module_dir)
    if stamp is None:
        return f"{module_dir} (no provenance stamp; published by an older build or copied by hand)"
    return (
        f"{stamp.get('build_type') or 'unknown'} build from {stamp.get('binary_dir')}, "
        f"{stamp.get('compiler')}, published {stamp.get('published_utc')}"
    )


def binding_available(module_dir: str | Path = DEFAULT_MODULE_DIR) -> bool:
    """Return whether the default compiled binding can be imported."""
    try:
        load_binding(module_dir)
    except SystemExit:
        return False
    return True


def parse_override_args(module, settings: list[str] | None) -> dict[str, float | int]:
    """Parse ``NAME=VALUE`` strings while preserving the binding registry's scalar type."""
    defaults = dict(module.default_model_coefficients.values)
    overrides: dict[str, float | int] = {}
    for item in settings or []:
        name, separator, raw_value = item.partition("=")
        name = name.strip()
        raw_value = raw_value.strip()
        if not separator or not name or not raw_value:
            raise SystemExit(f"--set: expected NAME=VALUE, got {item!r}")

        prototype = defaults.get(name)
        try:
            if isinstance(prototype, int) and not isinstance(prototype, bool):
                value: float | int = int(raw_value, 10)
            else:
                value = float(raw_value)
        except ValueError as error:
            kind = "an integer" if isinstance(prototype, int) else "numeric"
            raise SystemExit(f"--set: {name} value {raw_value!r} is not {kind}") from error
        overrides[name] = value
    return overrides


def coefficients_from_args(
    module,
    settings: list[str] | None,
    extra_overrides: dict[str, float | int] | None = None,
):
    """Return one validated immutable coefficient value plus the applied batch."""
    overrides = parse_override_args(module, settings)
    overrides.update(extra_overrides or {})
    try:
        coefficients = module.default_model_coefficients.with_overrides(overrides)
    except (TypeError, ValueError) as error:
        raise SystemExit(f"--set: {error}") from error
    return coefficients, overrides


def print_overrides(overrides: dict[str, float | int]) -> None:
    for name, value in overrides.items():
        print(f"  override: {name} = {value} (production default overridden)")


def prepare_source(module, path: str | Path, coefficients):
    """Decode a WAV container in Python and condition its PCM through the compiled core."""
    pcm, sample_rate = sf.read(path, dtype="int16", always_2d=True)
    source = module.prepare_source(
        np.ascontiguousarray(pcm),
        int(sample_rate),
        coefficients,
    )
    return source, int(sample_rate)


def _render_kwargs(render) -> dict:
    return {
        "ibi": render.ibi,
        "systole_duration": render.systole_duration,
        "s1_amplitude": render.s1_amplitude,
        "s2_amplitude": render.s2_amplitude,
        "s1_resample_ratio": render.s1_resample_ratio,
        "s2_resample_ratio": render.s2_resample_ratio,
        "lowpass_cutoff_hz": render.lowpass_cutoff_hz,
        "onset_compression": render.onset_compression,
        "kind": render.kind,
    }


def render_beat(module, source, render, coefficients) -> np.ndarray:
    """Render one native-layout beat through the compiled core."""
    return np.asarray(
        module.render_beat(source=source, coefficients=coefficients, **_render_kwargs(render)),
        dtype=np.float32,
    )


def trace_beat(module, source, render, coefficients) -> dict[str, np.ndarray]:
    """Return the compiled renderer's native-layout domain trace."""
    trace = module.trace_beat_render(
        source=source,
        coefficients=coefficients,
        **_render_kwargs(render),
    )
    return {name: np.asarray(value, dtype=np.float32) for name, value in trace.items()}


def render_from_source_stages(
    module,
    source_s1: np.ndarray,
    source_s2: np.ndarray,
    sample_rate: int,
    render,
    coefficients,
) -> np.ndarray:
    """Continue altered post-source stages through compiled transmission/mix/limiting."""
    return np.asarray(
        module.render_beat_from_source_stages(
            source_s1=np.ascontiguousarray(source_s1, dtype=np.float32),
            source_s2=np.ascontiguousarray(source_s2, dtype=np.float32),
            sample_rate=sample_rate,
            coefficients=coefficients,
            **_render_kwargs(render),
        ),
        dtype=np.float32,
    )
