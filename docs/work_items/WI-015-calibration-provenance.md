# WI-015: Calibration Provenance Audit

Status: `[NEXT]`

## Outcome and acceptance criteria

Every `[ref]` and `[dsp]` constant has an audited `[[calibrate]]` binding or a specific, durable
`[[uncited]]` reason. Distinctive load-bearing prose figures have committed leaves or are marked
prose-only by design. Where feasible, the rationale includes an observable admissible bound and the
constant-to-observable mapping; otherwise it explains why no useful independent bound exists. No
"not yet audited; provenance unknown" placeholder remains.

## Current conclusion

The checker now prevents new unclassified constants, but many existing `[dsp]` constants are covered only
by placeholder reasons. The `[ref]` systole slope/intercept are bound; `SystolePEPShortening` and the PVC
coefficients remain blocked on state/evidence. Derived values and some historical breath windows still lack
a citable home.

A value need not be directly measurable to be constrained. Useful bounds fall into five classes:
physiology/literature envelopes, within-recording ratios or timing, source-asset measurements, exact
DSP/geometry/headroom constraints, and ear-set operating points with regression guardrails. Prefer a bound
on the realized response or trajectory; a filter cutoff, normalized state scale, or interacting gain is
usually not independently meaningful.

## Scope and non-goals

Classify provenance and build only the cheap checker extensions that make a real invariant enforceable.
Do not retune a constant merely because its origin is weak; open or link the focused tuning item. Do not
force a literature interval onto an asset-specific or normalized gameplay/DSP mapping when a structural
or perceptual bound is the honest constraint.

## Dependencies

- Structured state-sensitive bindings described by the
  [reference-state ledger](../MEASUREMENT_METHODS.md#reference-state-ledger).
- Calibration rules in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#calibration-and-provenance).
- [WI-010](WI-010-pvc-tuning.md) for PVC coefficients.

## Next action and decision points

Audit placeholder `[[uncited]]` entries feature-by-feature, beginning with the current synthesis chain.
For each, either write the structural/ear/reference rationale, create a compatible leaf and binding, or
link the work item that must resolve it. First state the observable being protected, then calculate or
measure the broadest defensible interval and map that interval back to the constant family. Treat coupled
families together: breath filter cutoff/poles/order, amplitude gain and limiter headroom, and PVC
timing/amplitude/morphology. The maintainer decides whether an ear-set value's rationale is faithful before
the placeholder is removed.

## Observed separate work

Replacing prose citations with derived-expression support is worthwhile only where a distinctive,
load-bearing value cannot be stored directly as a leaf.
