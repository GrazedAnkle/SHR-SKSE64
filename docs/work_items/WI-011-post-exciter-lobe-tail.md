# WI-011: Post-Exciter Lobe and Tail Follow-Ups

Status: `[NEXT]`

## Outcome and acceptance criteria

The lobe tamer and S1 tail each still earn their place on the current low-cut, exciter-free source. Any
taming remains compatible with physiologically multi-component S1 rather than enforcing a single-lobe
ideal. The tail mode is appropriate to the post-cut body, the splice ramp introduces no meaningful HF
transient, and the combined envelope is preferred by ear with vigor jitter active. Rest, peak, and recovery
headroom remain clean.

## Current conclusion

The broad mush defect and loudness operating point are closed. The lobe/tail constants were tuned against
an earlier spectrum. The tail's 46 Hz center may no longer match the post-cut body, and its splice produces
a tiny broadband transient at `TailSpliceMs`. Static maximum-drive renders are insufficient because the
envelope stages ride per-beat vigor. The project's reference set now shows that distinct secondary S1
lobes are common, invalidating the earlier physiological premise that a forceful S1 must decay as one
monotonic lobe. The current tamer also detects its envelope with a 2 ms box-smoothed sample magnitude,
whereas the repaired onset compressor uses the analytic envelope; carrier phase and frequency invariance
have not been demonstrated for the tamer.

Manual review of the post-cut source places its later envelope maximum roughly 18 ms after the main
maximum. That is compatible with reported normal M1-T1 separation, but it does not identify the two
lobes as M1 and T1: this is one monaural, single-site recording without ECG or simultaneous valve-site
channels, and a roughly 50-60 Hz ringing/interference pattern can produce similar spacing. Multiplicative
suppression at that time would also remove any overlapping M1/tissue ring, not selectively remove T1.

A fixed-HR offline stage isolation shows why HF temporal skew cannot arbitrate this design: onset
compression moves HF earlier relative to the whole S1, while the lobe tamer alone moves it later; their
current combination moves the lead/lag result modestly toward zero with drive. Disabling the output
limiter and contractility gain does not change that direction. Skew is therefore useful here only for
showing which stage introduced a late-HF wash over the complete rendered S1, never as a physiological or
perceptual target.

## Scope and non-goals

Re-evaluate `LobeTameDecayMs`, `LobeTameEnvMs`, `TailResonatorHz`, `TailRingLevel`, `TailSpliceMs`, and
`TailRampMs` as one interacting envelope outcome. Quantify where `S1SystoleFraction` truncates the tail,
but do not retune that timing cap inside this item. Do not revisit the accepted breath amplitude, default
limiter character, or retired exciter. Treat the source as a fixed mixed-component baseline; do not name
or replace one source lobe as T1 inside this item.

## Dependencies

- Current model in [SYNTHESIS_MODEL.md](../SYNTHESIS_MODEL.md#second-order-couplings).
- [WI-006](WI-006-s1-envelope-sharpness.md) so the same envelope is not tuned twice to different goals.

## Next action and decision points

Plot stage-by-stage jitter-live beats and sweep tail center/level/ramp including "tail off." Compare the
current tamer detector with the analytic envelope on synthetic multi-lobe signals with known timing, and
assert invariance to carrier phase/frequency before trusting reference lobe summaries. Measure the splice
residual, 80-200 Hz balance, and tail energy surviving the high-HR timing cap. The maintainer decides
whether the lobe correction and subtle tail enrichment independently outweigh their costs.
Include three ear-test families: the current source/tamer, tamer-off, and a deliberately neutralized
baseline with a separately added late component.
Report full-S1 HF lead/lag stage deltas beside those families as a diagnostic, with no reference setpoint;
the ear decision and the envelope/spectral/headroom gates remain authoritative.

## Observed separate work

Characterize ref8's systolic rumble during this pass; if it is steady recording noise rather than decaying
ring-down, close the observation without adding a synthesis feature. The source audio after the current S1
slice contains the documented editing bump, so restoring it is not a safe "natural tail" experiment. If
the capped high-HR S1 sounds audibly truncated, open the dedicated `S1SystoleFraction` item already
specified by [WI-006](WI-006-s1-envelope-sharpness.md); overlap also requires an additive render path,
because the current C++ lobe copies overwrite rather than mix.

If the neutral-baseline family is preferred and state variation cannot be expressed as a small delta over
the unmodified source, open a separate controllable S1-component item. Its first acceptance gate is an
identity test: at the source recording's nominal state, the neutralized baseline plus added component must
reconstruct the original envelope, spectrum, and percept by ear. Delay and level need independent controls;
respiration/loading are candidate level drivers, while PR/conduction/activation and rhythm are candidate
timing drivers. Ordinary HR or contractility must not move the split by default without evidence. This is
a synthesis hypothesis, not permission to label the source's later lobe T1.
