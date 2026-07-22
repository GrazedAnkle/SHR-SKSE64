# WI-006: S1 Envelope Sharpness

Status: `[NEXT]`

## Outcome and acceptance criteria

Decide whether the modest high-drive S1 rise-time difference is audible and, only if it is, adjust the
smallest physiologically coherent envelope lever. Validate with per-beat vigor active, the null-immune
rise ruler, matched-window spectral measures, a rest-to-exercise-to-recovery audition, and headroom checks.

## Current conclusion

The broad high-HR mush defect is closed. Engine and ref8 rise-time distributions overlap, though the
engine median remains modestly slower. Ref8's conspicuously forceful beats are loud rather than unusually
fast, so raising the onset-compression ceiling to chase them is not justified.

## Scope and non-goals

This item owns the remaining S1 sharpness decision. It does not resurrect the exciter, target ref8's
absolute HF as physiology, or use `attack_ms` across signals. Lobe/tail cleanup is
[WI-011](WI-011-post-exciter-lobe-tail.md); blunt S2 is [WI-007](WI-007-blunt-s2-attack.md).

## Dependencies

- Current envelope model in [SYNTHESIS_MODEL.md](../SYNTHESIS_MODEL.md#second-order-couplings).
- Preferred rise ruler in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#preferred-rulers).
- [WI-004](WI-004-hf-temporal-skew.md) before using skew as corroboration.

## Next action and decision points

Render jitter-live variants around the current onset/lobe parameters, plot the S1 envelopes, and stage a
blind or level-matched ear comparison. The maintainer decides whether the residual is audible enough to
earn a DSP change.

## Observed separate work

If high-HR S1 sounds truncated rather than soft, split a dedicated `S1SystoleFraction` task instead of
folding a timing-cap change into this one.
