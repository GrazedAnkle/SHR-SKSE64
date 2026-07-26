# WI-028: Retire Remaining Offline Python Mirrors

Status: `[NEXT]`

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

The binding already exposes the behavioral replacements: `RhythmEngine` for rhythm, `Runtime.step` for the
full trajectory, and the complete source/mapping/render path for audio. The audition channel policy is
native stereo for written audition files and explicit channel zero for numerical rulers, as owned by
[ARCHITECTURE.md](../ARCHITECTURE.md#offline-execution). Its immutable `ModelCoefficients` value now
provides the bound replacement for `--set Constant=value` without expanding `RuntimeSettings`.

## Scope and non-goals

Replace `rhythm_offline.py`'s rhythm reconstruction with the bound `RhythmEngine`, and `sim_offline.py`'s
`Step` port with the bound `Runtime.step`, keeping their measurement, CLI, and reporting surfaces. Delete
the duplicated behavioral code once each tool reads the compiled core.

Move active audition and measurement rendering to the compiled native-layout path. Retired tail/tamer
counterfactual transforms may remain Python analysis operations, but they must consume core-owned active
stages rather than make the legacy renderer an engine oracle.

Do not port measurement/plotting/annotation to C++ or retune physiology, rhythm, or DSP.

## Dependencies

None.

## Next action and decision points

Migrate both clients without changing their analysis rulers: write native-layout stereo audition files,
select channel zero explicitly for mono metrics, and route `--set` through the immutable binding value.

## Newly observed work to split out

None.
