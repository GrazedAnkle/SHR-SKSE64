# WI-001: Reference Breath Landmarks

Status: `[DEFERRED]`

## Outcome and acceptance criteria

Refs 11, 14, and 15 have only the breath landmarks that a reviewer can defend from both the modulation
view and the recording. Every retained `BR=` extremum is bracketed by S1/S2 annotations and passes
`auto_annotate.py --check-breath`. Unknown or unmeasurable cycles remain explicit rather than inferred.

## Current conclusion

Refs 12, 13, and 20 already have committed landmarks. Review of refs 11, 14, and 15 is postponed because
their present S1/S2 grids do not cover enough usable cycles to measure the breath landmarks confidently.
Additional beat annotation is the prerequisite, not more detector analysis.

Ref21 is not a kinetics anchor because its deliberate patterns may include holds. Ref8's breath band is
filtered away, so it is optional evidence and never a completion gate.

## Scope and non-goals

Scope is manual S1/S2 extension followed by adjudication of the existing two-axis breath candidates.
Do not infer respiratory depth from the acoustic muffle; that would use the dependent variable to create
its own predictor. Extending ref20's recovery tail is a separate focused annotation session and is not
required here.

## Dependencies

- A S1/S2 pass over enough clean audio in refs 11, 14, and 15.
- Existing `auto_annotate.py --breath <annot> --view` and breath-span validation.
- The landmark semantics in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#periodic-sampling-and-aggregation).

## Next action and decision points

When this work resumes, extend one reference's beat grid first, marking bracket-only beats so
[WI-014](WI-014-annotation-validity.md) can migrate them later. Then review candidate extrema in the
modulation view and Audacity. The reviewer decides whether each cycle is defensible and stops rather than
manufacturing a complete-looking catalog.

## Observed separate work

- A half-open `BR=` form may be useful if truncated cycles become common; current evidence does not
  justify the parser and test changes.
- A long, clean, stable-state breath recording would improve spectral-muffle calibration; that belongs to
  [WI-008](WI-008-spectral-breath-muffle.md).
