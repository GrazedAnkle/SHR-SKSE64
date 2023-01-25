# Skyrim Heart Rate for SKSE64

A mod that simulates heart rate and its effects on the player.

## Mod Installation

### Requirements
- Skyrim SE version 1.6.353 or earlier (later versions are currently unsupported due to breaking changes)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [SKSE](https://skse.silverlock.org/) for Skyrim SE 1.6.353

## Important
Unfortunately, later versions of Skyrim SE and CommonlibSSE have made breaking
changes that make some of the movement detection code stop working. This means
only Skyrim SE version 1.6.353 or earlier is currently supported.

If your version of Skyrim is newer than this, you will have to either [replace
your SkyrimSE.exe with the older version](https://www.reddit.com/r/skyrim/comments/xew6ew/how_to_downgrade_skyrim_16629_to_skyrim_16353/),
or run the [Unofficial Downgrade Patcher](https://www.nexusmods.com/skyrimspecialedition/mods/57618) (see "Old Files", FullPatcher_1.6.XYZ-1.6.353, replacing XYZ with your current version).

You will also have to install a corresponding version of SKSE. I believe v2.1.5
(2_01_05) is the latest version that supports 1.6.353. You can find older
versions of SKSE in the [archive](https://skse.silverlock.org/download/archive/).

Once you have completed the above steps, you can download the mod from the
[releases](https://github.com/GrazedAnkle/SHR-SKSE64/releases) page and install
just like any other mod. If unsure how to do this, I recommend using [Mod
Organizer 2](https://github.com/ModOrganizer2/modorganizer/releases) to manage
the installation. You can find a fairly detailed guide on how to use it on
the Step Modifications [Mod Organizer page](https://stepmodifications.org/wiki/Guide:Mod_Organizer).

## Build Dependencies

| Dependency                                  | Minimum Required |
|---------------------------------------------|------------------|
| [vcpkg](https://github.com/microsoft/vcpkg) | -                |
| CMake                                       | `3.21`           |
| C++ compiler                                | C++23 compliance |
