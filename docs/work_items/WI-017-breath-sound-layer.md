# WI-017: Breath-Sound Layer

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

The player character's breathing supplies a coherent exertion/recovery cue in first and third person,
driven by the existing respiratory state. A reference-versus-offline feature list, a minimal synthesis
design, and auditions define acceptance before in-game integration.

## Current conclusion

Breath is shaped airflow noise rather than a discrete transient, so synthesis is a better starting point
than samples. The existing rate, phase, depth, and planned asymmetry already provide its envelope. Initial
features to decompose are noise band, airflow-rate envelope, inspiration/expiration spectral difference,
exertion-dependent intensity/turbulence, possible high-effort voiced huffing, and listener perspective.

## Scope and non-goals

Design and prototype the audio layer. Do not use breath audio to infer reference depth for calibrating the
heartbeat muffle. Do not add incidental diastolic noise as a separate goal; the breath layer will naturally
occupy much of that silence.

## Dependencies

- [WI-009](WI-009-breath-curve-asymmetry.md) for the final airflow envelope.
- The [reference-state ledger](../MEASUREMENT_METHODS.md#reference-state-ledger) for state-compatible
  reference-intensity calibration.
- [WI-018](WI-018-breath-variability.md) should share the same per-cycle state rather than add another
  random envelope.

## Next action and decision points

Choose representative reference windows, write the feature inventory, and prototype shaped noise in an
offline harness. The maintainer decides whether separate first-/third-person perspectives are necessary in
the first implementation and auditions the airflow envelope before integration.

## Observed separate work

A voiced grunt on a heavy attack may be event-specific rather than part of continuous breathing; split it
if the prototype supports that distinction.
