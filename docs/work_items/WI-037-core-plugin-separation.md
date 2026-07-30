# WI-037: Core and Plugin Source Separation

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

The core/plugin dependency boundary is a fact of the source layout rather than a convention held together
by hand-maintained CMake lists. A file's directory states which target owns it, and an include statement
states which side of the boundary it reaches across.

The implementation must:

- separate core, plugin-adapter, and shared sources into distinct directories;
- make each target list only its own directory, eliminating the current situation where core headers
  appear in two different lists and are handed to both targets;
- keep `shr_core` free of any CommonLibSSE, XAudio, TOML, spdlog, or precompiled-header dependency, which
  the `Core-Release-Clang` preset already proves and must continue to prove;
- namespace the project's CMake options, removing the save-clobber-restore workaround that currently
  protects `BUILD_TESTS` from CommonLibSSE's identically named option;
- give the plugin-adapter sources that the tests compile directly a real target to link against; and
- leave every build artifact behaviorally identical, verified through the goldens and the native suite.

## Current conclusion

`src/` is one flat directory holding three populations with genuinely different dependency rules. CMake
reconciles them through four overlapping variables (`headers`, `core_sources`, `sources`, `tests`), and
the reconciliation is already imperfect:

- core headers are listed twice, once in `headers` and again in the `shr_core` target, so adding one means
  editing two places and omitting the second fails silently;
- `add_commonlibsse_plugin` receives `${headers}`, which is every header including all of the core ones,
  so the plugin target nominally owns files belonging to `shr_core`; and
- the test executable compiles `Config.cpp`, `LoggingConfiguration.cpp`, and `NotificationPolicy.cpp`
  directly from `src/` rather than linking them, maintaining a second copy of their compilation settings.

The `BUILD_TESTS` collision is a related symptom. Because the option is not namespaced, it collides with
CommonLibSSE's option of the same name, and the build works only because the value is saved, forced off
across the `add_subdirectory` call, and restored afterward. That workaround is correct and non-obvious,
which is the combination worth removing.

None of this is currently broken. It is the maintenance cost of a boundary that
[ARCHITECTURE.md](../ARCHITECTURE.md#dependency-boundary) describes precisely while the file layout does
not express it at all.

## Scope and non-goals

Restructure directories, targets, and options. Update includes, the CMake source lists, the presets, and
the architecture documentation together, since a partial move leaves two conventions in play at once and
is worse than either.

Do not change any runtime behavior, any numerical value, or any public core signature. This is a pure
relocation, and its acceptance criterion is that nothing observable changes.

Do not fold the plugin-adapter support target into `shr_core`. `Config`, `LoggingConfiguration`, and
`NotificationPolicy` are Skyrim-free, but they are adapter policy rather than model behavior, and the
core's dependency rule is what makes it portable. Their being Skyrim-free is settled rather than assumed:
the native CI gate compiles and links all three in a tree configured with the plugin off and no
CommonLibSSE submodule resolved, reaching only spdlog and toml11. The support target is therefore viable.

## Dependencies

None blocking. The native CI gate that catches regressions from the move now exists, so the move can be
verified rather than reasoned about. That gate configures the plugin-free tests preset, which this item's
option renaming and target restructuring both touch; keep the preset and the workflow building as the
structure changes.

## Next action and decision points

Settle the directory shape before moving anything. The main decision is whether shared value headers -
`RenderSpec`, `BeatEvent`, `PhysiologySnapshot`, `SimulationState`, and similar - live inside the core
directory or in a third shared directory. They are core-owned types that the adapter consumes, which
argues for keeping them in core and letting the adapter include across the boundary in the one permitted
direction; a third directory makes the shared surface explicit but adds a category whose rules then need
their own definition.

Then decide whether includes become directory-qualified. Qualification makes each boundary crossing
visible at the include site, which is most of the value of the move, but it touches every file and
enlarges an already large mechanical diff.

## Newly observed work to split out

None.
