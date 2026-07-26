# WI-034: MCM Capability

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Players can inspect and edit a deliberately selected set of subject/runtime and adapter settings through
Skyrim's Mod Configuration Menu (MCM), with defined persistence and application semantics. The MCM is a
thin Skyrim adapter over typed native settings; it neither owns model behavior nor exposes offline
calibration controls.

The implementation must:

- keep `ModelCoefficients` outside the MCM, Papyrus state, configuration files, and co-save records;
- define one authoritative persisted value and scope for every exposed setting, including explicit
  precedence or migration for the existing TOML configuration;
- validate a complete typed settings update before applying any part of it;
- route Papyrus/UI changes through the execution-thread mechanism settled by WI-026 rather than mutating
  `Config`, `Runtime`, simulation, rhythm, or audio state directly from a callback;
- define whether each setting applies immediately, on the next runtime step, after a runtime reset, or
  only to a new game/character;
- keep `SimulationState` persistence independent from settings persistence and make load, revert, new-game,
  reset-to-default, and upgrade ordering deterministic;
- add the required Papyrus/native bridge, host form, compiled scripts, localization, build inputs, and
  FOMOD packaging;
- state whether SkyUI and any helper framework are hard dependencies or optional integration components,
  and preserve the declared non-MCM installation path;
- reuse native validation for accepted domains while keeping slider ranges, increments, labels, and help
  text as presentation metadata; and
- cover the native settings bridge and persistence/reconfiguration policy with automated tests, plus
  explicit in-game checks for registration, input, save/load, reset, and missing optional dependencies.

## Current conclusion

MCM belongs on the Skyrim side of the boundary in
[ARCHITECTURE.md](../ARCHITECTURE.md#value-and-responsibility-split). It may produce
`RuntimeSettings` replacements and adapter-owned settings changes, but it must not turn the immutable
offline coefficient value from WI-033 into player configuration.

The current implementation is startup-oriented rather than reconfigurable:

- TOML is loaded once on the `kDataLoaded` message into one process-global `Config`;
- `Runtime` copies `RuntimeSettings` into const members at construction;
- audio volume is sent to the source voice only during `HeartbeatVoice::Init`;
- notification and input paths consult the global configuration at use sites without a settings-update
  transaction or synchronization contract; and
- the distribution contains only the native plugin and heartbeat asset, with no Papyrus script, host
  plugin form, translation, or MCM layout.

Adding a menu without first defining authority and application semantics would therefore create two
competing configuration systems and several apparent controls that do not update the live subsystem.

## Scope and non-goals

Design and implement the MCM adapter, native settings bridge, persistence policy, reconfiguration
mechanisms, packaging, localization, automated adapter tests, and in-game acceptance checks. Select a small
initial surface from the existing heart-rate, arrhythmia-susceptibility, audio-volume, listen-key, and
notification settings. Debug logging and editable notification prose remain TOML-only unless their
in-game use earns the additional UI and localization surface.

Do not expose or persist `ModelCoefficients`, put MCM concerns into `shr_core`, make Papyrus the owner of
physiological state, silently reset a running simulation, or introduce a helper dependency without a
declared installation and compatibility policy.

## Dependencies

- [WI-026](WI-026-runtime-thread-contract.md) must settle the execution context or mailbox through which
  settings changes reach the update and audio owners.
- [WI-023](WI-023-cosave-record-validation.md) becomes a dependency only if the selected persistence
  design adds settings to SHR's co-save rather than using an external profile store or framework-owned
  persistence.
- [WI-033](WI-033-offline-coefficient-overrides.md) is not an implementation dependency; it defines the
  calibration boundary that the MCM must not cross.

## Next action and decision points

Before implementing the UI, settle:

- standard SkyUI MCM versus MCM Helper, and hard dependency versus conditional FOMOD component;
- the persistence/scope matrix for each proposed setting: compiled/TOML default, profile-wide value,
  per-save value, migration precedence, and reset behavior;
- the live-application matrix. Audio volume, notification enablement, and listen-key changes can be
  adapter updates; maximum HR and arrhythmia susceptibility can be validated runtime updates; resting HR
  needs an explicit preserve-state, transform-state, or reset/new-character policy because it seeds
  fitness;
- whether the native boundary accepts complete immutable `RuntimeSettings` snapshots or a typed patch
  that is resolved into a snapshot before enqueueing;
- the lifecycle ordering among `kDataLoaded`, MCM registration, runtime construction, new game, co-save
  load/revert, and a late MCM update;
- the minimal first menu, user-facing ranges and help text, key-conflict behavior, localization keys, and
  accessible reset/default controls; and
- the plugin form/quest, Papyrus registration functions, build/compile workflow, packaged files, and
  manual compatibility matrix.

## Newly observed work to split out

None.
