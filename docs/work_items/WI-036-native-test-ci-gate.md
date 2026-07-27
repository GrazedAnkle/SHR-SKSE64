# WI-036: Native Test CI Gate

Status: `[NEXT]`

## Outcome and acceptance criteria

The C++ unit tests run automatically on push and pull request. A change that breaks them fails CI rather
than waiting for a maintainer to remember to build and run them locally.

The implementation must:

- configure and run the Catch2 suite in CI without building the plugin or CommonLibSSE, since the test
  executable does not link them;
- add the missing configure preset for that combination, as every tests-enabled preset currently also
  builds the plugin;
- decouple the vcpkg `tests` feature from the `plugin` feature so Catch2 can be installed without the full
  plugin dependency set; and
- run the unit-test filter, leaving the `[integration]` and `[e2e]` labels - which need a Skyrim module or
  a running engine - excluded and unrun in CI.

## Current conclusion

Fifteen test files and roughly three thousand lines of Catch2 coverage are gated only on a maintainer
remembering to run them. The docs workflow runs three documentation checkers; the offline-goldens workflow
builds the binding and runs pytest. Neither compiles the tests.

The suite is already structurally ready. `SHRTests` links `shr_core`, Catch2, spdlog, and toml11, with no
CommonLibSSE dependency, and the `Core-Release-Clang` preset already proves the core configures without
the plugin. Two specific things block a job today:

- no preset sets `BUILD_PLUGIN=OFF` with `BUILD_TESTS=ON`; the four tests-enabled presets all build the
  plugin, which requires the CommonLibSSE submodule and its full dependency closure; and
- the vcpkg `tests` feature declares a dependency on `shr[plugin]`, so requesting Catch2 through the
  manifest drags in the entire plugin dependency set regardless of what CMake is asked to build.

The second is the more interesting one: the manifest encodes a coupling that the CMake targets do not
have. Testing the portable core is independent of the plugin everywhere except the dependency manifest.

## Scope and non-goals

Establish the CI gate and remove the manifest-level coupling that prevents it. Confirm by observation
rather than by inference that a plugin-free tests configure actually succeeds - the reasoning above is
sound but unverified, and a preset that does not configure is worse than none.

Do not attempt to run `[integration]` or `[e2e]` tests in CI; they need a Skyrim module and a running
engine respectively, and the existing test presets already separate them for this reason. Do not build the
plugin in CI as part of this item. A plugin compile gate is defensible work, but it needs the CommonLibSSE
submodule and a much heavier dependency install, and it should be justified on its own rather than
smuggled in behind the unit tests.

## Dependencies

None blocking. [WI-037](WI-037-core-plugin-separation.md) touches the same CMake option and target
structure, so if both are active, sequence them rather than interleaving - this item is smaller and
should land first, so that WI-037 has a working native gate to change under.

## Next action and decision points

Verify the configure first: `BUILD_PLUGIN=OFF` with `BUILD_TESTS=ON`, against a manifest feature set that
supplies Catch2, spdlog, toml11, and pocketfft without the plugin closure. Everything else follows from
whether that succeeds, and it may surface an ordering problem in the `find_package` blocks that the option
combination has never exercised.

Then decide whether the new preset is CI-only or the recommended local preset for core work, and whether
this job belongs in the existing offline-goldens workflow - it already provisions a Windows runner, a
pinned toolchain, and vcpkg - or in a separate workflow with its own trigger paths. Sharing the runner is
cheaper; a separate workflow makes the failure signal clearer.

## Newly observed work to split out

None.
