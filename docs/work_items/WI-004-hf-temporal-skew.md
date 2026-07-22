# WI-004: `hf_temporal_skew` Validity Domain

Status: `[NEXT]`

## Outcome and acceptance criteria

The valid S1 window for `shrlib.hf_temporal_skew` is expressed as a tested function of S1 duration or
systole rather than as an unconditional millisecond range. Synthetic invariance tests and the real
within-recording drive pairs agree on the domain. Documentation and metric metadata describe it as a
defect detector, not a perceptual snap scale.

## Current conclusion

Skew correctly exposed the retired exciter's late-HF wash, but its magnitude does not order perceived
sharpness inside the reference family. Fixed windows can truncate high-HR S1 and flip the sign; the old
claim that the sign is stable from 100-160 ms is false at peak HR. Existing comparisons used equal-length
windows, but their physiological interpretation needs a duration-matched recheck.

## Scope and non-goals

Map window and S1-duration sensitivity, then re-run ref11/ref14/ref15 drive pairs. Do not tune DSP to
ref8's skew or turn the metric into a setpoint.

The reusable anchor visualization and foundational synthetic known-value coverage are part of the shared
[measurement infrastructure](../MEASUREMENT_METHODS.md#diagnostic-anchor-views). This item retains the
duration/window sweep, the resulting validity domain, and interpretation against the real drive pairs.

## Dependencies

- Metric policy in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#preferred-rulers).
- `shrlib.hf_temporal_skew`, `tests/test_metrics.py`, and committed S1 annotations.

## Next action and decision points

Sweep synthetic two-lobe signals over lobe duration and window fraction, choose a duration-matched
candidate, and plot each real waveform before accepting a sign change. The maintainer judges whether the
duration-matched result still tracks the audible family boundary.

## Observed separate work

If the drive-pair direction collapses after duration matching, update the reference finding under
[WI-005](WI-005-cross-gap-audit.md); do not silently retune S1.
