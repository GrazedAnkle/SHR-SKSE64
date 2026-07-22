# WI-009: Breath-Curve Asymmetry

Status: `[BLOCKED]`

## Outcome and acceptance criteria

The engine uses a smooth, state-dependent inspiration/expiration curve rather than the current symmetric
sine. Rest approaches the literature-supported longer expiration, while exertion/recovery approaches the
near-even ratios measured in refs 12 and 20. Transitions use explicit kinetics, and the reference and
engine curve families remain correctly identified by measurement guards.

## Current conclusion

`InspirationFraction` is dormant. The existing engine sine has a non-physical airflow corner at the
cycle boundary. The raised-cosine curve used to reconstruct `BR=` landmarks is a smooth minimum-assumption
interpolant: it passes through each hand-labelled trough and peak with zero slope, but it was not fitted as
the uniquely correct shape of real tidal breathing. Refs 12 and 20 support an approximately even cycle
under exertion/recovery, while resting physiology points to a shorter inspiration. A single scalar cannot
represent both states.

## Scope and non-goals

Implement the curve and its state dependence together. Do not merely replace the dormant scalar with an
exercise value that is wrong at rest. Breath-to-breath variability is a later design outcome.

## Dependencies

- Blocked on a defensible resting inspiration-fraction anchor; the completed state ledger does not
  manufacture one from audio.
- Curve/coverage rules in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#periodic-sampling-and-aggregation).

## Next action and decision points

Once the ledger schema exists, choose the state driver and transition kinetics, add a rest anchor, then
compare raised-cosine half-cycles with any equally smooth candidate justified by the resting anchor, then
implement the accepted curve in the simulation and offline harness. The maintainer auditions rest,
exercise, and recovery breathing cadence before acceptance. Reference coverage against a raised-cosine
reconstruction must not be treated as evidence that the engine itself should use that same curve.

## Observed separate work

Per-breath depth/period jitter and a breath-sound layer are later milestones; neither should expand this
curve item. The phase-structured *response* of respiratory rate and depth to a demand change remains
[WI-012](WI-012-simulation-dynamics.md), not a correction to the within-cycle inflation curve here.
