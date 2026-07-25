# WI-031: Pytest Migration of the Golden Checkers

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

The golden verification paths run as pytest cases under a single invocation with standard reporting, while
each checker keeps its `--capture` regeneration CLI:

- One `pytest` invocation runs every golden verification, with per-golden pass/fail and diff output through
  standard test reporting rather than hand-rolled `print`/`sys.exit`.
- Each checker's `--capture` mode stays a command-line entry point (regenerating a manifest is an authoring
  action, not a test).
- The binding-not-built and manifest-missing cases report as actionable skips or failures, not tracebacks.
- CI (WI-029) invokes the pytest suite instead of, or alongside, `tools/check_goldens.py`.

## Current conclusion

The four golden checkers (`check_source_conditioning_golden.py`, `check_beat_renderer_golden.py`,
`check_rhythm_mapping_golden.py`, `check_trajectory_golden.py`) each hand-roll argparse, a problems list,
and diff reporting, and `tools/check_goldens.py` subprocesses all four. Every checker doubles as its own
`--capture` generator and its verifier. Wrapping the verify paths as pytest cases gives one invocation and
standard CI reporting, but the capture/verify duality in each script must be preserved: capture stays a CLI,
verify becomes the test.

## Scope and non-goals

Extract each checker's verify path into a pytest case (parametrized over goldens/fixtures where natural),
keeping the capture path as a CLI. Preserve the existing manifests, tolerances, and comparison logic
unchanged. Decide the fate of `tools/check_goldens.py` (retire in favor of `pytest`, or keep as a thin
convenience wrapper).

Do not change golden content, tolerances, or the binding surface. Do not port measurement or annotation
tooling to pytest.

## Dependencies

- The established binding and goldens (see [ARCHITECTURE.md](../ARCHITECTURE.md#offline-execution)). No
  blocker.
- Coordinates with **WI-029**: the CI gate should invoke whichever single entry point this settles on.

## Next action and decision points

- Decide the capture/verify split mechanism: a shared conftest fixture that imports the built `shr_pybind`
  once, versus each test module importing it. The module directory (`build/pybind` by default) needs to be
  selectable, as `--module-dir` is today.
- Decide whether `pytest` replaces `tools/check_goldens.py` outright or the runner stays as a
  non-pytest-dependent entry point.
- Decide how `pytest` addresses the module directory (marker, `--module-dir` addoption, or env var) so a
  developer can point the suite at `build/dev-clang` as well as `build/pybind`.
