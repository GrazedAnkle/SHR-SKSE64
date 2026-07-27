# WI-007: Blunt S2 Attack

Status: `[NEXT]`

## Outcome and acceptance criteria

The engine S2 reaches its envelope peak on the reference timescale without shifting its established
fundamental or adding a site-specific P2 clap by default. The compiled source/render paths remain in
parity, the systole-law comparison does not absorb the S2 shape defect, and an ear test prefers the
result.

## Current conclusion

The source S2 is a genuine single broad lobe whose rise is roughly twice the current reference median.
This is an envelope defect, not evidence that S2 pitch or broadband drive response is wrong. A small
S2-specific envelope-shaping experiment is more appropriate than replacing the sample. The render also
applies the shared 5 ms `CrossfadeMs` fade-in to S2 at the copy boundary, so the source rise and boundary
ramp have not yet been separated as causes.

## Scope and non-goals

Shape the S2 buffer while preserving its fundamental and static-with-drive broadband character. The
site-dependent valve clap remains deferred. Do not standardize systole on the S2 body or peak; that would
make the timing law compensate for the wrong shape. Sweep or bypass the S2 fade-in independently before
adding a new envelope stage; do not change the S1 boundary taper as collateral work.

## Dependencies

- S2 annotation convention in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#preferred-rulers).
- S2 model and clap distinction in [SYNTHESIS_MODEL.md](../SYNTHESIS_MODEL.md#second-order-couplings).

## Next action and decision points

Regenerate reference S2 rise distributions and use the compiled trace/source-stage re-entry path to
prototype an S2-specific decaying-envelope ceiling. Compare source-only, current 5 ms fade-in,
shorter/no fade-in, and shaped variants. Ask the maintainer to compare the unshaped and shaped "dub"
before adding a production C++ stage.

## Observed separate work

The same pass may reveal that S2 needs a separate high-pass corner; record that as a separate conditioning
item rather than changing the corner opportunistically.
