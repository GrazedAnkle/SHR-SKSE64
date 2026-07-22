# WI-008: Spectral Breath-Muffle Evidence

Status: `[NEXT]`

## Outcome and acceptance criteria

The spectral breath effect is supported by a reproducible, source-compatible reference ruler. The
well-grounded low-pass muffle is separated from any real pitch motion, and `BreathPitchDipDepth` is
recalibrated or removed if the ref11 F0 change does not survive. S1 and S2 transmission effects are
validated separately. The accepted muffle is stated as a bounded, realized inspiratory-versus-expiratory
transfer response over the source's useful bands, not justified by an uninterpreted cutoff number.

## Current conclusion

Breath amplitude swing is accepted and is outside this item. Inspiratory HF attenuation is well supported
in direction, but the current cutoff is mostly ear/self-anchored because absolute cross-recording centroid
does not transfer. Ref11's reported F0 decrease is a fragile, physiologically unsupported cross-gap result.
S2 currently inherits the S1-tuned muffle without a clean validation reference.

The engine filters in source time and then resamples. Filtering is linear and commutes with amplitude, but
it does not commute with unchanged-coefficient resampling: the approximate output-domain corner is the
requested corner times the resample ratio. At maximum muffle the named 112 Hz corner is therefore heard
near 96 Hz (`0.86 * 112`), and PVC S1 near 87 Hz (`0.90 * 0.86 * 112`). That extra correlated dulling may
be useful, but it is currently implicit and changes what `BreathLowPassMinHz` means.

For the current two cascaded one-poles, maximum muffle adds approximately 1.5, 3.3, 4.7, 8.0, and
10.9 dB of attenuation relative to the open setting at 50, 80, 100, 150, and 200 Hz respectively. The
more interpretable current target is therefore about 9.4 dB of added 200-versus-50 Hz tilt before the
pitch resample. `BreathLowPassOpenHz` itself attenuates 200 Hz by about 1.6 dB, so “effectively open” also
needs a declared passband and tolerance.

A reduced physical model can bound the sign, scale, and transition width of this response, but cannot
derive a universal anatomical cutoff. Inflation-dependent lung measurements use different sources and
paths, while external human heart sounds combine transmission changes with respiration-driven changes at
the cardiac source. Use those models as a plausibility prior on an effective transfer ratio, not as a
direct calibration target or permission to import absolute animal attenuation.

## Scope and non-goals

Validate the estimator and evidence before another cutoff retune. Do not chase ref20's absolute centroid
ratio or use ref8/ref21 as breath-kinetics gates. Do not re-open `BreathAmpDepth` for sub-dB movements
inside the reference uncertainty. Audit filter/resample order and decide whether the cutoff constants name
source-domain or audible output-domain corners before calibrating them. Co-audit
`BreathLowPassOpenHz`, `BreathLowPassMinHz`, and `BreathLowPassPoles`; cutoff alone does not define the
response. Compare a bounded high-shelf if the physical/reference envelope does not support an indefinitely
increasing low-pass attenuation.

## Dependencies

- [WI-005](WI-005-cross-gap-audit.md) for ref11 window consistency.
- Measurement rules in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md).
- The [reference-state ledger](../MEASUREMENT_METHODS.md#reference-state-ledger) must independently
  identify depth before any reference is treated as maximum depth.

## Next action and decision points

Build a synthetic F0 validation set spanning muffle, noise, and low-frequency multimodality; compare the
current peak estimator with autocorrelation or cepstral alternatives. Then remeasure ref11 on the shared
window rule and seek a longer stable-state breath reference. The maintainer judges cutoff variants by ear
only after the evidence pass identifies a valid direction.
Use synthetic impulse/noise fixtures to compare the realized transfer functions for filter-before-resample
and output-domain filter placement, including deep-inspiration sinus S1/S2 and PVC S1.

Build a back-of-the-envelope transmission prior from published heart/thorax and inflation-dependent lung
measurements. Express it as broad allowable inspiratory-minus-expiratory attenuation or tilt at fixed
frequencies/bands. Then combine that prior with a within-recording fixed-band ratio and the ear test. Record
which candidate responses the prior rules out; do not manufacture a precise cutoff from uncertain path
length, site, source-component, or transducer assumptions.

## Observed separate work

A clean-S2 breathing reference would support an S2-specific transmission item. Reference acquisition is a
manual task and may remain the blocker.
