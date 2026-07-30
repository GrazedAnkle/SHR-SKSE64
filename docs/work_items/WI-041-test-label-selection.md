# WI-041: Native Test Label Selection

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Selecting part of the native suite by label either works or is not offered. A preset that names a label
filter actually filters, and a preset that can select nothing does not exist.

The implementation must:

- decide whether Catch2 tags reach CTest as labels, and make the test presets consistent with that
  decision;
- correct or remove the exclusion value in the `Unit-Tests` preset, which CTest matches as a regular
  expression rather than as a list of literal bracketed tags;
- resolve what `Integration-Tests` and `E2E-Tests` should be while no test carries their tags, since both
  currently exit non-zero rather than report an empty selection; and
- leave the set of tests that actually run unchanged, since this item concerns selection rather than
  coverage.

## Current conclusion

Three test presets in `CMakePresets.json` offer label-based selection that nothing implements. The offer
fails independently at three layers, so repairing any one of them alone changes no observable behavior:

- no test carries `[integration]` or `[e2e]`. Every tag in the suite - `[simulation]`, `[audio]`,
  `[rhythm]`, `[acoustic]`, `[coefficients]`, `[config]`, `[logging]`, `[notification]`, and their
  qualifiers - is unit-level;
- `catch_discover_tests` is called without `ADD_TAGS_AS_LABELS`, so it registers each test with no CTest
  label at all. `ctest --print-labels` reports that no labels exist, and no label filter can match
  anything regardless of which tags the tests carry; and
- the exclusion value `[integration],[e2e]` is matched as a regular expression, in which square brackets
  denote character classes and the comma is a literal. Passed verbatim to `ctest -LE`, it excludes nothing.

The observable result is that `Unit-Tests` selects the whole suite - the right outcome today, reached by
coincidence rather than by filtering - while `Integration-Tests` and `E2E-Tests` select nothing and exit 8
under the `noTestsAction` of `error` they inherit from `All-Tests`.

Nothing is currently mis-tested. Every test in the suite is a unit test, so running all of them is
correct. The cost is that the presets describe a capability that does not exist, and the first test to
need a Skyrim module or a running engine would be silently included in the unit run rather than excluded
from it.

## Scope and non-goals

Settle the selection mechanism and make the presets honest about it. Whatever is chosen must survive
`catch_discover_tests`, which is the layer where the current design silently drops the information the
filters depend on.

Do not write integration or end-to-end tests under this item. Whether such tests are worth having is a
coverage question; this item is only about whether the machinery that would select them works.

Do not change which tests the CI gate actually runs. The native workflow runs the whole suite because the
whole suite is unit tests, and that stays true regardless of how selection is expressed.

## Dependencies

None blocking. This item changes the test invocation used by the Windows native-test workflow, so it
sequences after that gate rather than alongside it.

## Next action and decision points

The mechanism is cheaper than the current state suggests, so decide the shape first rather than costing
the repair. Catch2 3.11.0's `catch_discover_tests` accepts `ADD_TAGS_AS_LABELS`, which emits every tag as
a CTest label. Enabling it and querying the result confirms both halves of the behavior: labels appear
per tag, and they appear **without** their brackets - `fatigue`, not `[fatigue]`. Selection then works
normally, with `-L fatigue` selecting exactly the eleven fatigue cases and an exclusion of
`simulation|audio` reducing 117 tests to 30.

That has a consequence for the second acceptance bullet. The existing exclusion value is wrong twice over
and would remain wrong even after propagation is enabled, because the brackets are not part of the label
and the comma is not alternation. The corrected form is `integration|e2e`.

The remaining decision is whether to keep the three-preset split at all. Enabling the flag and correcting
the expression makes the machinery genuine but still selects nothing, because the tags it filters on are
unused; the alternative is to collapse the presets to one unit gate and reintroduce selection when a test
that genuinely needs a Skyrim module or a running engine first exists. The first route keeps the intended
eventual shape visible at the cost of machinery that stays inert; the second removes machinery rather than
adding it, at the cost of the presets no longer documenting the intent.

A narrower question sits underneath either route: whether the tag vocabulary should distinguish
environment requirements from subject-matter grouping at all. The current tags name what a test is about,
not what it needs to run, and only the latter can justify excluding a test from a gate.

## Newly observed work to split out

None.
