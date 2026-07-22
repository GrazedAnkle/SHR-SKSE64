# WI-018: Breath-to-Breath Variability

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Respiratory depth and period vary naturally from cycle to cycle without making total beat loudness
unstable. Draws are bounded, state-scaled, reproducible in tests, and balanced jointly with the fast
contractility jitter and breath-sound envelope.

## Current conclusion

The current respiratory oscillator is deterministic, so repeated cycles sound metronomic even when the
mean swing matches a reference. The existing bounded per-beat vigor jitter supplies the right structural
pattern at a slower event scale: draw once per breath, hold/interpolate through the cycle, and clamp rare
outliers. Occasional sighs may require a separate event model.

## Scope and non-goals

Add variability to cycle depth and period. Do not widen `BreathAmpDepth` to imitate variability; that
constant owns the mean inspiratory attenuation. Do not tune the slow and fast loudness components
independently.

## Dependencies

- [WI-009](WI-009-breath-curve-asymmetry.md) for the per-cycle curve.
- [WI-017](WI-017-breath-sound-layer.md) for shared audible consequences.
- The [reference-state ledger](../MEASUREMENT_METHODS.md#reference-state-ledger) for state-matched
  variability evidence.

## Next action and decision points

Measure per-cycle period/depth proxies on a stable, sufficiently long reference, propose bounded
distributions, and simulate their contribution to slow loudness variance. The maintainer decides whether
ordinary variability is enough or sighs deserve a separate event.

## Observed separate work

If per-cycle randomness breaks the engine's rational phase locks, record that as a useful consequence, not
as the calibration target for the variability.
