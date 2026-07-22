# WI-019: PVC Compensatory-Pause Scheduling

Status: `[DEFERRED]`

## Outcome and acceptance criteria

The compensatory pause ends at the intended next sinus impulse rather than starting another full sinus
interval. For nominal interval `R` and a single PVC at coupling `c`, a preceding sinus beat at `t = 0`
produces the PVC at `t = cR` and the next sinus beat at `t = 2R`; the PVC-to-sinus interval is therefore
`(2 - c)R`.

Deterministic tests cover a single PVC, couplet, and maximum-length run across frame steps that both land on
and overshoot each boundary. In-run inter-ectopic intervals are subtracted exactly once from the original
compensatory window. If the minimum-pause floor engages, the next sinus beat follows that floored remainder
without another normal IBI. The following sinus interval resumes normal cadence, timer overshoot is
preserved, and the Frank-Starling filling proxy uses the actual interval from the final PVC to the returning
sinus beat.

## Current conclusion

`RhythmEngine` computes the intended single-PVC window as
`fullPause = (2 - coupling) * m_NextIBI`. Runs subtract each additional inter-ectopic interval from that
window. After `m_InPause` waits the resulting `m_PauseDuration`, however, the pause-exit branch subtracts
the completed pause and seeds a fresh `BaseIBI`. The next sinus beat therefore fires one full normal
interval late: near `t = 3R` for a single PVC instead of `t = 2R`.

The same extra interval follows a run. `m_PrecedingRR` stores only `m_PauseDuration`, so while the extra IBI
exists it also understates the actual final-PVC-to-sinus interval used by the Frank-Starling proxy. The code
comment beside the reseed points here rather than presenting the current behavior as intentional.

## Scope and non-goals

Fix the pause-completion state transition, the first returning sinus beat, and interval bookkeeping. Keep
single-PVC and run behavior coherent, including the minimum-pause floor and frame overshoot.

Do not retune PVC probability, coupling, run-extension probability, morphology, amplitude, systole, or
tone. Those remain with [WI-010](WI-010-pvc-tuning.md). Do not use this focused correction to introduce a
new rhythm architecture or additional arrhythmias.

## Dependencies

None for the scheduling correction. It should land before WI-010 uses adjacent beats or filling intervals
for calibration.

## Next action and decision points

Add a deterministic rhythm fixture with controlled PVC/run draws, encode the timelines above, then change
the pause-exit transition without losing accumulated frame overshoot. Confirm that `Beat::IBI` continues to
mean output-buffer duration while the separate preceding-RR state records the actual filling interval.
