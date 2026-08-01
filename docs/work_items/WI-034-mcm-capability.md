# WI-034: MCM Capability

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Players can inspect and edit a deliberately selected set of subject/runtime and adapter settings through
Skyrim's Mod Configuration Menu (MCM), with defined persistence and application semantics. The MCM is a
thin Skyrim adapter over typed native settings; it neither owns model behavior nor exposes offline
calibration controls.

The demand is not speculative. In-game editing existed before the SKSE rewrite, so its absence is a
regression rather than a missing enhancement, and because subject settings differ per character, the
current path asks a player to maintain one hand-edited TOML profile per character.

The implementation must:

- keep `ModelCoefficients` outside the MCM, Papyrus state, configuration files, and co-save records;
- persist every exposed setting at the scope the type seam assigns it, with no per-setting exception;
- persist per-character settings as tri-state overrides, so an absent record and an explicitly unset
  override are the same state;
- validate a complete typed settings update before applying any part of it, reusing the existing native
  settings-versus-coefficients validation rather than duplicating domain rules in the adapter;
- route Papyrus/UI changes through the execution-thread mechanism settled by WI-026 rather than mutating
  `Config`, `Runtime`, simulation, rhythm, or audio state directly from a callback;
- apply each setting according to the live-application matrix below, including the state transform that
  makes the resting heart-rate control mean something on an existing character;
- expose a per-character fitness ceiling, since a fitness *initial value* is erased by ordinary play;
- keep `SimulationState` persistence independent from settings persistence and make load, revert,
  new-game, reset-to-default, and upgrade ordering deterministic;
- add whatever bridge, host form, compiled scripts, localization, build inputs, and packaging the
  feasibility spike shows to be actually required, and no more;
- state whether SkyUI and any helper framework are hard dependencies or optional integration components,
  and preserve the declared non-MCM installation path;
- keep slider ranges, increments, labels, and help text as presentation metadata over native validation;
  and
- cover the native settings bridge and persistence/reconfiguration policy with automated tests, plus
  explicit in-game checks for registration, input, save/load, reset, and missing optional dependencies.

## Current conclusion

MCM belongs on the Skyrim side of the boundary in
[ARCHITECTURE.md](../ARCHITECTURE.md#value-and-responsibility-split). It may produce
`RuntimeSettings` replacements and adapter-owned settings changes, but it must not turn the immutable
offline `ModelCoefficients` value into player configuration.

The current implementation is startup-oriented rather than reconfigurable:

- TOML is loaded once on the `kDataLoaded` message into one process-global `Config`;
- `Runtime` copies `RuntimeSettings` into const members at construction;
- audio volume is sent to the source voice only during `HeartbeatVoice::Init`;
- notification and input paths consult the global configuration at use sites without a settings-update
  transaction or synchronization contract; and
- the distribution contains only the native plugin and heartbeat asset, with no Papyrus script, host
  plugin form, translation, or MCM layout.

Adding a menu without first defining authority and application semantics would create two competing
configuration systems and several apparent controls that do not update the live subsystem. The
decisions below settle authority; what remains open is listed under next action.

### Persistence scope follows the existing type seam

The split is a rule about which type a setting already lives in, not a judgement made per setting:

- a setting in `RuntimeSettings` - that is, `SimulationSettings` plus `ArrhythmiaSusceptibility` - is a
  property of the subject and is **per-character**, persisted in SHR's co-save; and
- a setting in the adapter `Config` - debug, input, audio, notification - is a property of the
  installation and is **profile-wide**, persisted outside the save.

The seam already exists in the source, so the rule needs no new taxonomy and gives a defined answer for
settings added later. Note that `Config` currently also carries the TOML *source* of the per-character
values: `Config::HeartRate` and `Config::Arrhythmia` are read once at startup to construct
`RuntimeSettings`. Under this rule those TOML entries stay, but demote in meaning from "the value" to
"the default a new character starts from".

### Per-character settings persist as tri-state overrides

Each per-character setting is written to the co-save only when the player has moved it. Unset means
follow the profile/TOML default. The payoff is structural rather than cosmetic:

- a save written before this feature existed has no override records, which is exactly the state that
  means "unset", so **pre-MCM saves load correctly by construction** - no record-version bump and no
  migration table;
- "reset to default" becomes a representable state rather than a re-entry of the current default; and
- a setting added later needs no migration for the same reason, which is what keeps unrelated model work
  from blocking this item.

Precedence is therefore: co-save override, else TOML/profile value, else the compiled default.

### The live-application matrix is derived, not chosen

How a setting applies follows from how the model already consumes it:

- `MaximumHeartRate` and `ArrhythmiaSusceptibility` are pure parameters read on every step, so a
  validated update applies immediately;
- adapter settings - audio volume, notification enablement, listen key - apply immediately once WI-022
  gives the voice an owner and WI-026 settles delivery; and
- `RestingHeartRate` is read **only** in `CreateInitialState`, where it seeds `Fitness`. It is a seed,
  not a parameter. A naive slider bound to it would therefore do nothing, forever, on an existing
  character.

Because the seeding is linear, the correct treatment is exact and unique. Applied to the current drifted
`Fitness` rather than to a fresh seed:

```
Fitness += (oldResting - newResting) / RestingHRSlope
```

This is the only transform that commutes with drift: re-seeding deletes the character's progression, and
doing nothing lies about what the control does. The general rule it stands for - a setting consumed only
at seed time needs a state transform, not a binding - applies to any future seed-shaped setting.

### The fitness ceiling is a subject property, not a coefficient exposure

Fitness is a transient, not a subject property: it climbs toward one global target whenever exertion
exceeds idle and decays toward another when it does not, so every adventuring character converges to the
same value and a per-character fitness *value* is erased by ordinary play. The setting that survives play
is the ceiling, so `FitnessMaxMets` moves from
`ModelCoefficients` into `SimulationSettings`, beside `MaximumHeartRate`. This is a boundary
reclassification, not an exception to the coefficient rule: maximal aerobic capacity varies roughly
two-and-a-half-fold across real subjects, which makes it subject identity in the same sense as maximum
heart rate, whereas the coefficients it currently sits beside are calibration.

The move has three couplings the design must answer rather than discover:

- `AcuteFatigueMax` and `LongTermFatigueMax` are defined in `Constants.hpp` as fractions of the
  calibrated ceiling. They remain fixed coefficients unless deliberately re-derived per character;
- normalized fitness divides by the span between the ceiling and `FitnessBaseMets`, and that value lerps
  the recovery time constants, so this control moves recovery kinetics directly; and
- `ModelCoefficients.cpp` validates the ceiling against `FitnessBaseMets` and `IdleMets`. Once the
  ceiling is a setting, that becomes a settings-versus-coefficients check of the kind
  `Runtime.cpp`: `ValidateRuntimeCoefficientContext` already performs, and belongs there.

### The configuration format does not change

MCM dissolves most of the "TOML is unfamiliar" objection by removing the need to edit a file at all, and
a helper framework introduces its own JSON layout file regardless, so converting TOML to JSON would
unify nothing while churning a working toml11 path.

## Scope and non-goals

Design and implement the MCM adapter, native settings bridge, persistence policy, reconfiguration
mechanisms, packaging, localization, automated adapter tests, and in-game acceptance checks. Select a
small initial surface from the existing heart-rate, arrhythmia-susceptibility, fitness-ceiling,
audio-volume, listen-key, and notification settings. Debug logging and editable notification prose remain
TOML-only unless their in-game use earns the additional UI and localization surface.

Do not expose or persist `ModelCoefficients`, put MCM concerns into `shr_core`, make Papyrus the owner of
physiological state, silently reset a running simulation, change the configuration file format, or
introduce a helper dependency without a declared installation and compatibility policy.

This item ships the control surface. It does not fix the post-combat recovery behavior that motivated a
fitness control in the first place: with the current adrenaline and recovery-tau coupling, a fitness
slider moves resting heart rate and little else. That work is tracked separately and may deserve to land
first, but it is not a blocker here, because tri-state overrides let settings be added without migration.

## Dependencies

- [WI-022](WI-022-audio-resource-ownership.md) gives the audio voice an explicit owner, which is what
  makes a live volume update a small change rather than a lifetime question.
- [WI-026](WI-026-runtime-thread-contract.md) must settle the execution context or mailbox through which
  settings changes reach the update and audio owners, and must consolidate the adapter's file-scope
  state into the single owner that settings updates are delivered into.
- [WI-023](WI-023-cosave-record-validation.md) is a **hard blocker**, not a conditional one: per-character
  settings persist in SHR's co-save, so the loader's malformed and unknown-version policy must be defined
  before override records are written.

The immutable coefficient boundary is owned by
[ARCHITECTURE.md](../ARCHITECTURE.md#value-and-responsibility-split), not an implementation dependency.

## Next action and decision points

The feasibility spike runs first, before any UI design, because its result determines how much of this
item exists. If MCM Helper can drive the menu from a configuration JSON alone, with no ESP, then the
Creation Kit form, the Papyrus sources, a resurrected Papyrus compile workflow, the vcpkg script-source
features, and the conditional FOMOD branch all disappear from scope. The spike must also observe which
thread a setting change is delivered on, which is a direct input to WI-026.

That branch is worth spiking before committing to the other one because the project carries no Papyrus
build configuration at all. Git history holds a deleted set - three Pyro project files and vcpkg manifest
features installing Bethesda and SKSE Papyrus sources - but it is a reference shape at best rather than a
starting point: it imported from distribution directories that do not exist and pointed at a build tree
that is not a configured preset. Authoring that workflow is real scope, incurred only if the spike fails.

Settle after the spike:

- standard SkyUI MCM versus MCM Helper, and hard dependency versus conditional FOMOD component;
- whether the native boundary accepts complete immutable `RuntimeSettings` snapshots or a typed patch
  resolved into a snapshot before enqueueing;
- the co-save record layout for tri-state overrides, agreed with WI-023;
- the lifecycle ordering among `kDataLoaded`, MCM registration, runtime construction, new game, co-save
  load/revert, and a late MCM update;
- whether `Runtime` holds `const RuntimeSettings m_Settings` still - a settings replacement path has to
  revisit that constness even for updates that are otherwise trivially safe, and the choice between
  replacing the member and rebuilding the runtime is the same choice as preserving versus resetting
  simulation state; and
- the minimal first menu, user-facing ranges and help text, key-conflict behavior, localization keys, and
  accessible reset/default controls.

## Newly observed work to split out

None.
