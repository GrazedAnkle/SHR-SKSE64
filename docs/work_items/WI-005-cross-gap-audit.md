# WI-005: Cross-Gap Reference-Claim Audit

Status: `[NEXT]`

## Outcome and acceptance criteria

Every claim comparing distant windows inside one recording is classified by whether the recording chain
can act over the gap. Trusted findings are regenerated with one window rule and an appropriate ruler;
unsafe magnitudes are removed or explicitly directional. No DSP coefficient depends on an unaudited
cross-gap value.

## Current conclusion

The ref11 inspiratory muffle direction is physiologically plausible, but its absolute centroid baselines
are method-inconsistent and the reported F0 drop is unsupported. Absolute drive-pair peak levels can be
changed by AGC; gain-invariant rise timing survives. Ref13's loudness trajectory supports direction, not a
20 dB coefficient.

## Scope and non-goals

Audit ref11 first, then the analogous ref13/ref14/ref15 comparisons. This item classifies evidence; actual
breath-muffle or pitch changes belong to [WI-008](WI-008-spectral-breath-muffle.md).

## Dependencies

- Confound rules in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#recording-chain-confounds).
- Duration-matched skew work in [WI-004](WI-004-hf-temporal-skew.md) for the same drive pairs.

## Next action and decision points

Pin one ref11 S1 windowing rule, regenerate both the drive and breath comparisons from landmarks, and
validate a robust F0 estimator on synthetic muffled/noisy signals before applying it to ref11. Ask for a
waveform/spectrogram review if estimator families disagree.

## Observed separate work

Distinctive surviving prose values that still lack leaves feed
[WI-015](WI-015-calibration-provenance.md).
