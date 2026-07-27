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

The offline analysis binding (`shr_pybind`) and the golden regression checks run
from a Python virtual environment:

```
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python tools/build_pybind.py
python -m pytest tests --module-dir build/pybind
```

That runs the whole Python suite: one case for each compiled-core golden (source
conditioning, beat rendering, rhythm/mapping, and full trajectories) plus the
analysis and offline-client tests. Narrow it to `tests/golden` for the goldens
alone. Point `--module-dir` at another binding tree when needed. `tools/check_goldens.py`
remains a convenience wrapper (and supports `--build`); its verify mode launches
that same pytest suite. The individual `tools/check_*_golden.py` tools remain
runnable for a focused check or `--capture`; use `tools/check_goldens.py
--capture` to regenerate every manifest after an intentional core change.
The waveform manifests hash raw float bytes, so
`.github/workflows/offline-goldens.yml` pins the compiler used for capture and
verification rather than relying on the hosted runner's floating LLVM version.

`build_pybind.py` also writes a type stub (`build/pybind/shr_pybind.pyi`) so an
editor resolves the binding's API. For Pylance, add `build/pybind` to
`python.analysis.extraPaths`. `.vscode/settings.json` handles this already.

#### One preset for editing across all paths

`Release-Clang` (plugin + tests) and the binding build are separate configures,
so C++ IntelliSense only resolves includes for whichever one was configured last.
To edit the plugin, tests, and binding together with one compile database,
configure the `Dev-Clang` preset (it turns `BUILD_PYBIND` on and uses the `.venv`
interpreter, so it also configures straight from an IDE with no activated venv):

```
cmake --preset Dev-Clang
```

Point the IDE at `build/dev-clang/compile_commands.json`. This preset is for
development and IntelliSense; the plugin release still ships from `Release-Clang`
and the golden binding from `build_pybind.py`. Create `.venv` first (see above);
for a venv elsewhere, override `-DPython_EXECUTABLE`.
