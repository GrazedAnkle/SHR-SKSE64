# Documentation Conventions

These conventions keep the authoritative documentation current without turning it into a development
log. The reader-facing map lives in [README.md](README.md); task status lives in
[ROADMAP.md](ROADMAP.md).

## Document ownership

Each topic has one authoritative owner. Other documents link to it instead of restating it:

- [ARCHITECTURE.md](ARCHITECTURE.md) owns the core/plugin dependency and runtime boundary.
- [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md) owns physiological state-to-sound mechanisms.
- [SIMULATION_MODEL.md](SIMULATION_MODEL.md) owns physiological state evolution.
- [CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md) owns the current contractility signal in depth.
- [REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md) owns findings from reference recordings.
- [MEASUREMENT_METHODS.md](MEASUREMENT_METHODS.md) owns cross-cutting measurement methodology.
- [ROADMAP.md](ROADMAP.md) owns priority, dependencies, and links to focused work items.

Every authoritative document describes what is true now. There is no frozen documentation tier.

## Status tags

- `[ACTIVE]` - being worked on now.
- `[NEXT]` - specified and next in line.
- `[NEEDS DESIGN]` - the outcome is known but its implementation is not yet settled.
- `[BLOCKED]` - cannot progress until a named dependency or external input changes.
- `[DEFERRED]` - deliberately postponed, with the reason recorded.
- `[DONE]` - allowed only for a completed sub-step inside a still-open focused work item. Standalone
  completed work moves to its authoritative owner and is removed from task tracking.

## Completed work is atemporal

When work completes, remove it from the roadmap or focused work item and fold the durable result into the
owning document. State the problem, cause, and current solution so the "why" survives without retaining a
chronological investigation log. Git history owns ordinary development history.

## Focused work items

One work item owns one independently closable outcome. It contains:

- outcome and acceptance criteria;
- current conclusion;
- scope and non-goals;
- dependencies;
- links to authoritative evidence instead of copied model or measurement prose;
- next action and decision points; and
- newly observed work that should be split out rather than silently added to scope.

The filename begins with a stable `WI-###` ID. The roadmap points to that ID and never relies on line or
nested-item numbers.

## Tie prose to code and data

When a document states tuned behavior, name the code symbol rather than repeating its literal value. The
code owns as-built values; documentation explains the reason. Quote a number when the number carries the
argument, such as a fitted law or calibration target, and keep the owning symbol or data leaf beside it.

Reference findings are owned by [REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md). Measurement-scope state is
authored in `references/state_ledger.toml`; `tools/ref_analyze.py` embeds it with computed values in the
generated `references/measurements.json`. [MEASUREMENT_METHODS.md](MEASUREMENT_METHODS.md#reference-state-ledger)
owns the schema semantics. `tools/check_docs.py` validates links, anchors, symbols, coupling numbers, and
tool references, and enforces that `tools/README.md` indexes every module in `tools/` so the
load-bearing/exploratory distinction there cannot silently decay. `tools/check_data_citations.py` validates supported prose citations, calibration bindings,
and their structured state references.

## Keep in-source comments lean

To ensure code remains readable, and to minimize surface area for comment drift, source comments should only document
implementation details or quirks necessary for local understanding. Detailed explanations go in appropriate owning docs,
and should not be duplicated in comments.

## Constant provenance tags

Every constant in `src/core/Constants.hpp` has one provenance tag answering what process is required to change
it:

- `[physio]` - physiology or sport-science literature.
- `[ref]` - re-measure the reference recordings.
- `[dsp]` - synthesis tuning by ear, including a value whose historical origin was a reference.
- `[game]` - gameplay or engine-integration feel.
- `[asset]` - fixed by the source asset.
- `[util]` - dimensionless conversion.

The file remains grouped by feature; a feature header may provide the tag when every constant in the group
shares it. `tools/check_constants.py` enforces coverage.

Treat `Constants.hpp` as a compact as-built parameter index. Keep provenance tags, units, valid domains,
disable sentinels, non-obvious implementation coupling, and a one-line semantic description. Physiology
and acoustic theory, literature ranges, reference findings, calibration rationale, and architectural
purpose belong in the authoritative owning document; delete header prose that the docs already cover
rather than maintaining a second copy.

## Medical terminology

Use the correct term, then add a short plain-language gloss on its first use in a document. Common terms
such as S1 and S2 need no gloss. The fuller definition belongs in [GLOSSARY.md](GLOSSARY.md); do not gloss
every repetition or hyperlink every term.
