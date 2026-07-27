# WI-010: PVC Tuning

Status: `[DEFERRED]`

## Outcome and acceptance criteria

PVC S1 level, S2 level, systole, and tone match within-recording PVC-versus-neighbor measurements on refs 7
and 9, using rulers that survive the recording chain. The result remains coherent across heart rate,
contractility, and coupling-interval sweeps; each bypass of the sinus S1 shaping stages is tested rather
than inherited as one all-or-nothing branch. Tuned constants have reference leaves or an explicit
literature/ear/structural rationale.

## Current conclusion

PVC timing exists; [WI-019](WI-019-pvc-compensatory-pause.md) owns the known compensatory-pause scheduling
defect. Amplitude and dulling do not yet convincingly match the references. PVC S1 level currently follows
relative filling but omits the sinus contractility gain, while S2 follows a coupling-based perfusion ramp.
`ResamplePVCRatio` lacks an audited reference binding. PVCs also skip sinus onset compression; conduction
can justify different activation and onset, but the bypass still needs to be tested rather than inherited
implicitly.

The current Frank-Starling input is preceding RR (or PVC coupling interval) divided by nominal IBI. It is
a cycle-length proxy, not literal diastolic filling time: the interval includes the preceding beat's
systole, so it can materially overstate the short filling opportunity before a PVC. PVC systole is
`max(0.55 * nominalSystole, 150 ms)`; it reaches the 150 ms floor above roughly 112 bpm and becomes longer
than the sinus floor/curve above roughly 184 bpm. That inversion may be an intended ref7 compromise, but
it has not been justified across HR. The compiled binding now supplies actual PVC rhythm events,
`CreateRenderSpec` amplitudes/systole, trace stages, and native-layout output; no shaping-only Python
preview is a valid calibration fixture.

## Scope and non-goals

Tune the existing PVC morphology only. Do not simply apply the sinus contractility law to an ectopic beat:
coupling/filling, activation site, contractile state, and whether the beat ejects interact. New arrhythmia
states and coroutine architecture are later design work. Build the calibration sweep from bound
`BeatEvent` / `RenderSpec` values and compiled trace stages.

## Dependencies

- The settled sinus S1 envelope in
  [SYNTHESIS_MODEL.md](../SYNTHESIS_MODEL.md#second-order-couplings) is the comparison baseline.
- [WI-019](WI-019-pvc-compensatory-pause.md) should land before neighbor-beat and filling comparisons.
- Measurement and provenance rules in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md).

## Next action and decision points

Measure each PVC against adjacent sinus beats in refs 7 and 9, add reproducible leaves, then sweep
amplitude/resample and the onset bypass independently. Include low/high-HR and
low/high-contractility synthetic fixtures so a fixed coupling fraction is not mistaken for fixed absolute
filling time. Compare the current RR proxy with an explicit available-filling-time proxy, and sweep the
PVC/sinus systole crossover. The maintainer auditions the resulting morphology families.

## Observed separate work

If the references require sustained patterns such as bigeminy, open a separate rhythm-state item.
