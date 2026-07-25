# WI-030: Developer Ergonomics for the Offline/Plugin Split

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Working across the plugin and offline paths is low-friction and CMake-driven:

- Golden manifests are produced by a **single** generation path, and the redundant path (and its build
  targets) is retired or a decision to keep it is recorded.
- One command runs every golden check, platform-independently, without a maintainer needing to know each
  tool and its build-order prerequisites.
- A developer can configure **one** preset whose compile database surfaces plugin, test, and binding
  include paths together, so IDE IntelliSense resolves includes regardless of which path they are editing.
- The binding build is reachable through CMake presets with the manual argument injection minimized; the
  venv/interpreter coupling is either eliminated or explicitly documented as irreducible.
- Committed golden manifests are byte-stable across contributors (line-ending normalization).
- The README documents an end-to-end build-and-verify quickstart.

## Current conclusion

The offline cutover made the compiled core the sole implementation reachable through `shr_pybind` (see
[ARCHITECTURE.md](../ARCHITECTURE.md#offline-execution)). That path works, but the surrounding developer
workflow has accumulated avoidable friction, surfaced by a survey of the build and golden tooling:

- **Two golden generation paths.** `check_source_conditioning_golden.py` and
  `check_beat_renderer_golden.py` generate their manifests from `shr_core`-linked fixture executables
  (`SHRSourceConditioningFixture`, `SHRBeatRendererFixture`), while `check_rhythm_mapping_golden.py`,
  `check_trajectory_golden.py`, and `check_pybind_golden.py` generate from the binding. The
  `beat_render.json` manifest is already produced by a fixture exe **and** re-verified through the binding,
  and the binding already exposes `prepare_source` + `trace_beat_render`, so it can generate both
  fixture-backed manifests directly. Both paths link the same `shr_core`, so the numbers are identical; the
  choice is only which entry point survives.
- **Heavy regeneration.** Regenerating the two fixture-backed goldens today requires a full `Release-Clang`
  build with `BUILD_PLUGIN` on (CommonLib + plugin + Catch), just to obtain two small fixture exes. The
  binding builds from `Core-Release-Clang` with `BUILD_PLUGIN` off, which is far lighter and is what a CI
  gate (WI-029) wants to build anyway.
- **Per-preset compile database.** IntelliSense reads the compile commands of whichever preset was last
  configured. Configuring the plugin preset omits the binding's translation unit; configuring the offline
  preset omits the plugin and tests. A developer moving between both paths loses include resolution for the
  path they are not currently configured for.
- **The binding stands outside pure-CMake builds.** `tools/build_pybind.py` exists because the extension
  must be built against the importing interpreter, so it derives `Python_EXECUTABLE` and `pybind11_DIR`
  from the running venv and injects them. This is the reason "all builds run through `cmake --preset`" does
  not hold today. It is partially reducible: `pybind11_DIR` can be self-derived inside CMake from the found
  interpreter (`<python> -m pybind11 --cmakedir`), and the interpreter can be read from
  `$env{VIRTUAL_ENV}` in a preset. What remains irreducible is that the venv must be **activated** (or its
  path otherwise supplied) for the preset to target the correct interpreter.
- **No single golden runner** and **no line-ending pin** on the committed manifests (the repo uses
  `core.autocrlf=true`).

## Scope and non-goals

In scope: consolidate golden generation onto the binding and retire the two fixture executables and their
CMake targets; add a single golden-runner entry point; add a unified developer preset (or a shared compile
database) that covers plugin, tests, and binding; scope `BUILD_PYBIND` to the offline/dev presets rather
than defaulting it on for plugin builders; self-derive `pybind11_DIR` from the interpreter inside CMake so
the wrapper shrinks toward interpreter selection only; pin golden-manifest line endings; document the
quickstart.

Non-goals: retuning physiology, rhythm, or DSP; changing golden **content** or tolerances (WI-029 forbids
this while wiring the gate); the plugin/Skyrim integration and e2e gates; retiring the `rhythm_offline.py`
and `sim_offline.py` behavioral mirrors (WI-028 owns that).

## Dependencies and coordination

- **WI-029 (offline binding CI gate)** already lists "golden-generation consolidation" as split-out work
  and asks that it be evaluated before or alongside the CI job so the gate covers one generation path. The
  consolidation here is effectively that prerequisite; sequence the two together.
- **WI-028 (retire offline mirrors)** is independent but shares the "binding is the sole path" direction;
  no ordering constraint.
- Unifying the plugin and binding into one configured build must reconcile the vcpkg manifest features: the
  offline preset sets `VCPKG_MANIFEST_NO_DEFAULT_FEATURES` with the `core-dsp` feature, while the plugin
  build uses the default features. A unified preset needs their union.

## Next action and decision points

- **Golden generation path.** Binding-only (retire both fixture exes) versus keeping the fixture exes.
  Recommendation: binding-only. This also invites deduplicating the beat-render golden against the PCM16
  sink fingerprints carried in the Catch renderer test (a third representation of the same oracle).
- **Unified IDE build.** A single "everything on" developer preset (`BUILD_PLUGIN` + `BUILD_TESTS` +
  `BUILD_PYBIND`) that emits one compile database, versus keeping per-purpose presets and pointing the IDE
  at a merged/aggregate compile database. The preset approach is simpler but forces every configure to find
  Python and pybind11.
- **CMake-driven binding.** Self-derive `pybind11_DIR` inside CMake and read the interpreter from
  `$env{VIRTUAL_ENV}`, letting `cmake --preset` configure the binding when the venv is active — versus
  keeping `tools/build_pybind.py` as the single documented entry point. Decide how much of the wrapper to
  dissolve and whether the activated-venv requirement is acceptable.
- **pybind default.** Keep `BUILD_PYBIND` off for the plugin-release preset (so plugin builders are not
  forced to provision Python/pybind11 or risk an interpreter-mismatched `.pyd`); turn it on only in the
  offline and unified developer presets.

## Done sub-steps

- `[DONE]` README "Building" quickstart: plugin preset build plus the venv → `build_pybind.py` → golden
  checks sequence.
- `[DONE]` Line-ending pin for `tests/golden/*.json` via `.gitattributes` (`text eol=lf`).

## Newly observed work to split out

- **pytest migration of the golden checkers.** Each checker hand-rolls argparse, a problems list, and diff
  reporting. Wrapping the verify paths as pytest cases gives one invocation and standard CI reporting, but
  each script also doubles as its `--capture` generator, so that duality must be preserved (capture stays a
  CLI; verify becomes the test). Worth its own item.
- **Offline coefficient-override API.** Already recorded under WI-028; not duplicated here.
