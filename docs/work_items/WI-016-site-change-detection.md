# WI-016: Annotator Site-Change Detection

Status: `[DEFERRED]`

## Outcome and acceptance criteria

`auto_annotate.py` splits a clean span at auscultation-site changes before deciding S1/S2 phase, improving
phase correctness on an S2-louder straddle without regressing clean-span coverage or existing groups.

## Current conclusion

The validated design is multi-cue. A near-silence dropout longer than the local IBI is the primary
reposition cue; a sustained HR step catches editorial splices. Each resulting span can reuse the existing
phase vote. Direct S1/S2-balance flips are only long-window confirmation because respiration modulates the
same level and centroid axes at comparable scale. Detection must run on clean spans, not raw clinic audio.

Implementation is deferred because the available hand-annotated set contains no S2-louder straddle, so the
failure this feature is meant to fix cannot currently be scored.

## Scope and non-goals

Add boundaries only; do not replace phase logic. Gapless site changes shorter than the confirmation window
remain a manual review case. Do not tune against raw noisy spans.

## Dependencies

A maintainer must hand-annotate one S2-louder base/pulmonary straddle. Refs 1, 2, and 6 are candidates.

## Next action and decision points

After ground truth exists, implement local-IBI-normalized dropout and median-filtered HR-step finders,
partition spans, and score phase correctness before/after. The maintainer confirms the boundary labels and
whether the incremental coverage justifies retaining the feature.

## Observed separate work

Residual high-frequency recording noise may merit a low-pass/notch study, but it is not part of boundary
detection.
