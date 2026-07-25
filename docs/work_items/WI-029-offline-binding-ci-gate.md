# WI-029: Offline Binding CI Gate

Status: `[DEFERRED]`

## Outcome and acceptance criteria

Continuous integration builds the offline binding and runs the golden checks, so an unintended change to
compiled-core physiology, rhythm, mapping, conditioning, or rendering fails automatically rather than
relying on a maintainer running the checks locally.

## Current conclusion

The binding and the committed golden manifests under `tests/golden/` are validated only locally today. CI
currently runs the documentation validators on Linux, which cannot build the Windows clang extension or run
the goldens. The offline path is therefore a real regression oracle but an un-enforced one.

## Scope and non-goals

Add a CI job on a Windows runner that provisions the analysis virtual environment, builds `shr_pybind`
(the `Core-Release-Clang` preset), and runs `tools/check_goldens.py` — every golden check over the binding
in one command, exiting nonzero on any regression. Keep it independent of the CommonLib/plugin build; the
gate needs no fixture executables.

Do not fold in the plugin or Skyrim-dependent integration tests; those retain their own gates. Do not
change golden content or tolerances as part of wiring CI.

## Dependencies

- The established offline binding and goldens (see
  [ARCHITECTURE.md](../ARCHITECTURE.md#offline-execution)). No blocker.

## Next action and decision points

- Choose the runner and how it obtains clang-cl, vcpkg, and the pinned Python; decide whether the venv is
  built in CI or cached.
- Decide whether the gate also runs the Catch unit suite (which requires the CommonLib/plugin build) or
  only the Python golden tools. The Catch renderer test no longer carries PCM16 sink fingerprints — the
  goldens own rendered output — so the golden tools alone cover core render regressions.
