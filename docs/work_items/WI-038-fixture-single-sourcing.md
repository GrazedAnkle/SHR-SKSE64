# WI-038: Test Fixture Single Sourcing

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Test fixture values exist once. The C++ suite and the offline golden checks drive the same fixtures from
the same definition, so the two cannot disagree.

The implementation must:

- remove the hand transcription between `tests/BeatRenderFixtures.hpp` and `tools/beat_render_fixtures.py`;
- establish which artifact is authoritative and make the other derive from it mechanically;
- resolve the same question for `tools/rhythm_mapping_fixtures.py` and `tools/trajectory_fixtures.py`,
  either by single-sourcing them the same way or by recording why they do not need it; and
- keep the golden manifests byte-identical, proving the two definitions were in fact already in agreement
  at the time of the change.

## Current conclusion

The beat-render fixtures are transcribed by hand into two languages: seven fixtures of eight fields each,
in `tests/BeatRenderFixtures.hpp` and `tools/beat_render_fixtures.py`. The guard against drift is
one-directional:

- editing a value in the Python file fails `check_beat_renderer_golden.py`, because the committed manifest
  was captured from the previous specs; but
- editing a value in the C++ header fails nothing, because the manifest is generated from the Python specs
  and the C++ array feeds only the Catch2 assertions.

So the header is nominally the source of record while the Python file is what actually determines the
golden, and a change to the header alone silently desynchronizes them. Nothing is currently out of
agreement; the guard simply would not report it.

This is the last hand-mirrored artifact in the offline path. The render implementation, the rhythm and
mapping path, and the source conditioning are all single-sourced through the compiled core - the fixtures
that drive them are not.

## Scope and non-goals

Single-source the fixture values. Keep the C++ suite's fixtures usable as `constexpr` data, since the
Catch2 tests consume them at compile time and losing that would be a real regression in exchange for a
maintenance improvement.

Do not change any fixture value, add fixtures, or retune the goldens. The acceptance criterion is that the
manifests do not change, which is also the proof that the two definitions currently agree.

Do not extend this to the reference measurement data. The state ledger and `measurements.json` have their
own generation path and their own checker, and they are not part of this duplication.

## Dependencies

None blocking. This shares files with [WI-035](WI-035-golden-check-consolidation.md), so sequence them
rather than running both at once; WI-035 should land first, since it establishes the module surface these
fixtures would be loaded through.

## Next action and decision points

Choose the direction of derivation. The options are not equivalent:

- exposing the compiled fixtures through the binding makes C++ authoritative and matches the principle the
  offline unification already established, that Python reimplements nothing the core owns. It also puts
  test-only data into the binding surface, which so far exposes production API;
- generating both files from a neutral data file removes the language asymmetry entirely, but introduces a
  build-time generation step and a third artifact to keep honest; and
- generating the Python file from the C++ header by parsing it reuses the technique
  `tools/check_constants.py` relies on, which is the technique that file's own note warns against
  extending further.

Decide first whether the rhythm-mapping and trajectory fixtures share this shape. If they do, that argues
for the neutral data file; if the beat-render fixtures are the only genuine duplication, the binding route
is proportionate.

## Newly observed work to split out

None.
