# WI-028: Retire Remaining Offline Python Mirrors

Status: `[DEFERRED]`

## Outcome and acceptance criteria

No offline Python code reimplements core physiology, rhythm, or DSP. `tools/rhythm_offline.py` and
`tools/sim_offline.py` are reduced to thin clients over the compiled binding (or removed), while their
measurement, observation, and CLI value is preserved. Core behavior changes require no matching Python
port anywhere in `tools/`.

## Current conclusion

The offline-execution cutover made the compiled core the sole implementation of source conditioning, beat
rendering, acoustic mapping, rhythm, and full Runtime trajectories, reachable through the Python binding
(see [ARCHITECTURE.md](../ARCHITECTURE.md#offline-execution)). Two Python behavioral mirrors survived that
cutover for reasons unrelated to offline execution:

- `rhythm_offline.py` reconstructs a steady-state sinus subset (base-IBI RSA, Frank-Starling,
  per-beat vigor jitter) and feeds the **legacy mono `synth_beat` audition path**. Its reconstruction
  duplicates the compiled `RhythmEngine`, but it is wrapped in beat-to-beat measurement code that is a
  preserved analysis ruler, and its only consumer is `tools/audit_s1_tail.py`.
- `sim_offline.py` ports `HeartRateSimulation::Step` as a standalone physiology-observation CLI (exertion
  profiles, CSV dump, trajectory summary). It has no importers.

The binding already exposes the replacements: `RhythmEngine` for rhythm and `Runtime.step` for the full
trajectory. The blocking work is not exposing the core but untangling each tool from concerns the cutover
deliberately left alone.

## Scope and non-goals

Replace `rhythm_offline.py`'s rhythm reconstruction with the bound `RhythmEngine`, and `sim_offline.py`'s
`Step` port with the bound `Runtime.step`, keeping their measurement, CLI, and reporting surfaces. Delete
the duplicated behavioral code once each tool reads the compiled core.

Do not change the legacy mono audition audio path or port measurement/plotting/annotation to C++ as part of
this item. Do not retune physiology, rhythm, or DSP.

## Dependencies

- `rhythm_offline.py`'s reconstruction is coupled to the legacy mono `synth_beat` audio; moving it onto the
  compiled core's stereo float render is gated on the deferred mono-versus-stereo audition-audio decision.
- `sim_offline.py`'s `--set Constant=value` (tune a coefficient offline without rebuilding) has no
  equivalent on the binding: `Constants.hpp` coefficients are not part of `RuntimeSettings`. Preserving it
  needs an offline coefficient-override surface (see decision points).

## Next action and decision points

- Decide whether to build an offline coefficient-override API (expose selected `Constants.hpp` overrides to
  the binding, per the "future offline coefficient-override API" already noted in
  [ARCHITECTURE.md](../ARCHITECTURE.md)) or to drop `--set` when `sim_offline.py` becomes a thin client. The
  override API is independently valuable and can be split into its own work item.
- Decide whether `rhythm_offline.py`'s measurement harness moves onto binding-rendered stereo audio (which
  couples it to the audition-audio decision) or stays on the mono path until that decision lands.

## Newly observed work to split out

- **Offline coefficient-override API**: a separate immutable override value carried into the binding so
  physiology and DSP coefficients can be swept offline without rebuilding the extension.
