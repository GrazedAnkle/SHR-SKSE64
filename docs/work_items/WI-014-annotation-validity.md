# WI-014: Annotation Domain and Regime Validity

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Each estimator declares the annotation domains and temporal regimes it consumes, and ignores beats or
landmarks invalid for that estimate without discarding their valid uses elsewhere. Bracket-only breath
beats, low-SNR S2 labels, transition beats, and scoped exclusions no longer leak into unrelated medians.

## Current conclusion

Reference groups are semantic, not statistical. Current parsing discards some inline confidence flags,
and a beat added to bracket a breath landmark can enter a systole/timbre median. The correct abstraction is
domain-specific validity plus a steady/transition regime, not whole-beat deletion.

Ref11's breath-hold beat at 05:24.013 is the concrete automatic-onset migration case. LF contact noise
immediately before S1 moves the automatic `s1a` to 05:23.951, while the automatic S1 end and both S2
landmarks agree with the hand annotation. Machine-readable validity should therefore exclude automatic
S1-onset scoring and dependent auto measurements for that beat, pair it through its valid landmarks, and
retain its S2 evidence. Once that path is covered by a regression test, remove the `KNOWN_RESIDUAL`
group special-case from `auto_annotate.py`; do not resolve it by widening the global match tolerance or
discarding the whole beat.

## Scope and non-goals

Design the smallest annotation metadata and parser change that covers domain, regime, and purpose. Do not
reannotate valid landmarks or turn every incidental note into a schema field.

## Dependencies

- Group/aggregation rules in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#periodic-sampling-and-aggregation).
- Existing `EX=` scope and detector confidence flags as migration seeds.

## Next action and decision points

Quantify the current leaf changes caused by flagged and bracket-only beats, include the ref11 05:24.013
case in the schema and regression-test proposal, and have the maintainer approve the hand-authoring burden
before changing parsers.

## Observed separate work

Site-change detection still needs its own S2-louder ground truth and should remain deferred.

After the annotation domain/regime schema settles, revisit `plot_metric_anchors.py` arguments so the tool
can select a group, beat, sound, and validity domain directly from an annotation file. Retain numeric
`--start` / `--end` as the schema-independent path for engine renders and synthetic fixtures.
