# Menu Artifacts

The Mod Configuration Menu needs three things the C++ build does not produce: a Skyrim plugin holding the
quest the menu framework registers through, compiled Papyrus scripts, and a menu layout. This document
owns how each is regenerated, because `contrib/Distribution/SHR.esp` is committed as a binary and would
otherwise have no source.

Why a plugin exists at all: MCM Helper obtains config scripts from SkyUI's registered list, resolves each
to a quest, and derives the configuration folder from *that quest's source plugin filename*. With no
quest there is no registration, and `Data/MCM/Config/SHR/config.json` is never looked for. An ESP-less
menu is therefore not possible, which the [WI-034](https://github.com/GrazedAnkle/SHR-SKSE64/issues/4)
feasibility spike established before any of this was designed.

## The plugin

`SHR.esp` is ESL-flagged and holds exactly one quest. It is authored in xEdit; the Creation Kit is not
required. The record must end up as:

- **EDID** SHR_ConfigMenuQuest, **DNAM** flags Start Game Enabled only, priority 0, type None
- **VMAD** one script, `SHR_ConfigMenu`, no properties
- **Aliases** one reference alias: **ALID** PlayerAlias, **ALFR** PlayerRef [00000014]
- the alias's script lives in the quest's `VMAD -> Aliases`, not in the alias record: one entry whose
  Object v2 names the quest itself with alias index 0, carrying SKI_PlayerLoadGameAlias

Two traps, both of which cost real debugging time:

- **HEDR version must be `1.700000`.** xEdit stamps `1.71`, the Creation Kit 1.6.1130 format, and any
  runtime older than 1.6.1130 skips such a plugin *silently* - the mod manager still assigns a load index,
  xEdit's own error check passes, and no log mentions it. The plugin simply does not exist to the game.
- **The `Script Fragments` block inside VMAD cannot be removed** and does not need to be. A freshly
  created VMAD has it too, and FragmentCount 0 binds nothing.

Verify a rebuild with xEdit's Check for Errors, then in game: the plugin should appear in the SKSE
co-save's plugin list, and the quest should report running with its alias filled once a save is loaded.

## The scripts

Sources are in `src/papyrus/`. Neither script names a setting, so **adding one is a layout edit and a
row in the registry at `src/adapter/Settings.hpp` - no Papyrus recompile.** That matters because
compiling these is a manual step outside the CMake build.

- `SHR_Native.psc` is Hidden and holds nothing but global native declarations, so it compiles against
  an empty import path. Its surface is generic: a setter and a getter per Papyrus type a control can
  carry, plus the list of ids needing each, all keyed by the control ids the layout declares.
- `SHR_ConfigMenu.psc` extends MCM_ConfigBase. It exists because a menu is resolved through a quest,
  and because OnConfigOpen and RefreshMenu are reachable only from a config instance. It asks the
  plugin for the ids it must sync, so it never learns a setting name. It needs an import path with MCM
  Helper's and SkyUI's script sources plus Bethesda's, which is the expensive one to reproduce - keep
  it setting-agnostic.

Compile `SHR_Native.psc` first: with no imports, a failure there is the invocation rather than the script
chain, which makes it a free smoke test.

The compiled output is committed under `contrib/Distribution/Scripts/`, for the same reason the plugin
is: the release archive is assembled from that directory, and a menu without its scripts is an ESL and a
layout that do nothing. Compilation is a manual step, so this is the one hazard the repository cannot
check - nothing verifies that a committed `.pex` was built from the `.psc` beside it. Recompile and
commit both together, and treat a script edit without a matching binary change as an incomplete one.

The split of duties between the two is deliberate. A control's action writes the value, and the plugin
validates and stores it; the change event that follows about 7ms later re-reads what was stored and
pushes it back into the display. That is a read-back rather than a second write, which is what lets a
clamped or rejected edit correct itself on screen instead of leaving the control showing a value nothing
honours.

## The layout

`contrib/Distribution/MCM/Config/SHR/config.json` describes pages, controls, ranges, labels, and help
text; settings.ini beside it supplies the framework's own defaults. Both deploy through the existing
`contrib/Distribution` copy step.

Ranges here are presentation over the native domains in `src/adapter/Settings.hpp`, which are the
validation authority. A control's id is also what its change event reports back, so an id that disagrees
with the registry fails the lookup rather than writing the wrong setting.

Controls come in two kinds, told apart by their action. A **setting** is edited through a global
native and names a registry id. A **command** - the two reset controls - runs a method on the config
script instance instead, because a reset has to repaint and RefreshMenu is reachable only from there;
that is what the CallFunction action type and the quest's own form reference are for. A command names
no setting and stores nothing.

Reset follows one rule: **each scope falls back to the layer beneath it.** A character's settings fall
back to the profile, which is what clearing its co-save overrides means. The profile falls back to the
values compiled in, and is written to the configuration file so the fallback survives a restart.

Nothing at compile time reads the layout and the registry together, and the failure is quiet in the
player's hands - the slider moves and nothing happens - so `tools/check_menu.py` gates the agreement:
every control names a registry id and passes that same id to its action, the action's function and the
control's source type match the registry's kind, a slider's range sits inside the registry domain, and
no setting is left without a control. Keeping slider ranges inside the domains is what makes *no legal
slider position produces an update the runtime rejects* a checked property rather than an intention.

MCM Helper 1.5.0 rejects unknown top-level keys in `config.json`, including $schema, which its 1.6 schema
permits. 1.6.0 is the SkyUI 6 rewrite; 1.5.0 is the SkyUI 5.2 build.
