# Skyrim Heart Rate for SKSE64

SHR is an SKSE mod that simulates the player character's cardiovascular response
to activity, fitness, fatigue, and stress. Dynamically synthesized heartbeat
audio is its primary feedback channel.

## Upgrading from v0.4.0.0

v0.5.0.0 is compatible with existing saves. You do not need to start a new game
or use save cleaning tools.

A full replacement is recommended instead of merging v0.5.0.0 with an existing
v0.4.0.0 installation:

1. Back up `Data\SKSE\Plugins\SHR.toml` if it contains custom settings, then
   remove the v0.4.0.0 installation.
2. Install v0.5.0.0 and launch the game once. A new commented
   `Data\SKSE\Plugins\SHR.toml` will be generated automatically.
3. Reapply any desired settings manually. Do not restore the old configuration
   file wholesale, as its format is incompatible with v0.5.0.0.

The following v0.4.0.0 files are obsolete and may be deleted if they remain:

- `Data\SkyrimHeartRate.esp`
- `Data\Sound\fx\SHR_HeartBeat\HeartBeat_Shortened-30.wav`
- `Data\Sound\fx\SHR_HeartBeat\HeartBeat_Shortened-60.wav`
- `Data\Sound\fx\SHR_HeartBeat\HeartBeat_Shortened-90.wav`

Do not delete `HeartBeat_Shortened.wav`; v0.5.0.0 still uses it.

An existing save may display Skyrim's missing-content warning after the ESP is
removed. This warning is expected and safe to dismiss because the ESP contained
only the obsolete sound definitions.

## Mod Installation

### Requirements

- Skyrim SE version 1.6.353 or earlier[^skyrimversion]
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [SKSE](https://skse.silverlock.org/) for Skyrim SE 1.6.353

[^skyrimversion]: This mod *may* work on newer versions of Skyrim SE, but this
    has not been tested.

If your version of Skyrim is newer than 1.6.353 and you would like to downgrade,
you will have to either [replace your SkyrimSE.exe with the older version](https://www.reddit.com/r/skyrim/comments/xew6ew/how_to_downgrade_skyrim_16629_to_skyrim_16353/),
or run the [Unofficial Downgrade Patcher](https://www.nexusmods.com/skyrimspecialedition/mods/57618)
(see "Old Files", FullPatcher_1.6.XYZ-1.6.353, replacing XYZ with your current version).

You would also have to install a corresponding version of SKSE. For Skyrim
1.6.353, use v2.1.5 (2_01_05). You can find older versions of SKSE in the
[archive](https://skse.silverlock.org/download/archive/).

### Installation

After meeting the requirements, download the mod from the
[releases](https://github.com/GrazedAnkle/SHR-SKSE64/releases) page and install
it as you would any other mod. [Mod Organizer 2](https://github.com/ModOrganizer2/modorganizer/releases)
is highly recommended. Step Modifications provides a detailed
[beginner guide to Mod Organizer](https://stepmodifications.org/wiki/Guide:Mod_Organizer).

If you would like to install the mod manually, you will need to set up the
directory structure as follows:

```
Skyrim Special Edition
|--Data
|  |--SKSE
|  |  `--Plugins          <-- (Create these folders if they don't exist)
|  |     `--SHR.dll       <-- (From the PluginRelease folder)
|  `--Sound               <-- (The whole folder as-is)
`--SkyrimSE.exe
```

## Usage

Heartbeat audio starts disabled. Press `H` by default to toggle it and any
enabled notifications.

The toggle key, audio volume, resting and maximum heart rates, arrhythmia
susceptibility, notifications, and logging can be configured in
`Data\SKSE\Plugins\SHR.toml`. Changes take effect the next time the game starts.

## Building from Source

### Requirements

| Dependency                                  | Minimum Required |
|---------------------------------------------|------------------|
| [vcpkg](https://github.com/microsoft/vcpkg) | ---              |
| CMake                                       | `3.21`           |
| C++ compiler                                | C++23 support    |
| Python 3                                    | `3.11`           |

### Libraries

This mod uses the following libraries:

- [CommonLibSSE NG (alandtse fork)](https://github.com/alandtse/CommonLibVR)
- [fmt](https://github.com/fmtlib/fmt)
- [pocketfft](https://github.com/mreineck/pocketfft)
- [spdlog](https://github.com/gabime/spdlog)
- [toml11](https://github.com/ToruNiina/toml11)

For testing:

- [Catch2](https://github.com/catchorg/Catch2)

Python package dependencies are listed in [requirements.txt](requirements.txt).

### Building

Configure and build the SKSE plugin with a CMake preset (requires `VCPKG_ROOT` set
and clang-cl on `PATH`):

```
cmake --preset Release-Clang
cmake --build build/release-clang
```

Every preset that builds the plugin also builds the Catch2 suite. Run it against
whichever tree you configured:

```
ctest --test-dir build/release-clang --output-on-failure
```

CI runs the same suite without building the plugin at all, through the
`Core-Tests-Release-MSVC` configure preset and its `Core-Unit-Tests` test preset.
That pair exists for `.github/workflows/native-tests.yml` rather than for local
work - it selects MSVC and resolves the test dependencies without the plugin
closure, so it needs no CommonLibSSE submodule.
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#dependency-boundary) owns why that
gate is possible.

The offline analysis binding (`shr_pybind`) and the golden regression checks run
from a Python virtual environment:

```
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
cmake --preset Dev-Clang
cmake --build build/dev-clang
python -m pytest
```

Any build that enables `SHR_BUILD_PYBIND` publishes the binding, its type stub, and a
provenance stamp naming the tree it came from into `build/module`, which is where
the Python side imports from. `pyproject.toml` declares the test paths and the
`tools/` import root, so `pytest` needs no arguments;
[tools/README.md](tools/README.md) indexes the tools and covers how the Python
side is organized.

That runs the whole Python suite: one case for each compiled-core golden (source
conditioning, beat rendering, rhythm/mapping, and full trajectories), the
immutable coefficient-override contract, and the analysis and offline-client
tests. Narrow it to `tests/golden` for the compiled-core gates alone, or select
one with `-k trajectory`.

Build trees keep their own copy of the binding, so several can coexist; point
`--module-dir` at one to test it specifically rather than whichever was built
most recently. To build the binding without the plugin - CI, or a checkout
without the CommonLibSSE submodule - use the portable tree instead, which
publishes identically:

```
python tools/build_pybind.py
```

Verifying goldens is the pytest run above; authoring them is a separate tool,
for the reason
[ARCHITECTURE.md](docs/ARCHITECTURE.md#offline-execution) records:

```
python tools/capture_goldens.py                 # recapture every manifest
python tools/capture_goldens.py trajectory      # recapture one domain
```

Review the resulting manifest diff before committing it. The waveform manifests
hash raw float bytes, so `.github/workflows/offline-goldens.yml` pins the
compiler used for capture and verification rather than relying on the hosted
runner's floating LLVM version; capturing with a different toolchain produces a
manifest that will not verify there.

The published stub (`build/module/shr_pybind.pyi`) is what lets an editor resolve
the binding's API. For Pylance, `build/module` and `tools` both need to be on
`python.analysis.extraPaths`, since it does not read pytest's import root;
`.vscode/settings.json` handles both already.

#### One preset for editing across all paths

`Release-Clang` (plugin + tests) and the portable binding tree are separate
configures, so C++ IntelliSense only resolves includes for whichever one was
configured last. The `Dev-Clang` preset configures the plugin, tests, and binding
together, so one compile database serves every path (it turns `SHR_BUILD_PYBIND` on
and uses the `.venv` interpreter, so it also configures straight from an IDE with
no activated venv):

```
cmake --preset Dev-Clang
```

Point the IDE at `build/dev-clang/compile_commands.json`. Create `.venv` first
(see above); for a venv elsewhere, override `-DPython_EXECUTABLE`. This is the
recommended local preset and it also publishes the binding the Python suite uses;
the plugin release still ships from `Release-Clang`.
