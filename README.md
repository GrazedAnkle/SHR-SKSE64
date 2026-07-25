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
python tools/check_pybind_golden.py
```

The remaining `tools/check_*_golden.py` tools verify the compiled core's source
conditioning, beat rendering, rhythm, mapping, and full trajectories against the
committed manifests under `tests/golden/`. Pass `--capture` to regenerate a
manifest after an intentional core change.
