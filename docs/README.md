# SHR Documentation

SHR is a physiological heart-rate simulation whose heartbeat audio is the primary feedback channel. These
documents describe the current model, its evidence, and the remaining work without requiring development
history.

## Reading path

1. [Glossary](GLOSSARY.md) for project terminology.
2. [Runtime Architecture](ARCHITECTURE.md) for the core/plugin boundary and data flow.
3. [Synthesis Model](SYNTHESIS_MODEL.md) and [Simulation Model](SIMULATION_MODEL.md) for the two halves of
   the system: state-to-sound and state evolution.
4. [Measurement Methods](MEASUREMENT_METHODS.md) before trusting a number, then
   [Reference Analysis](REFERENCE_ANALYSIS.md) for evidence from our own recordings and
   [Literature Analysis](LITERATURE_ANALYSIS.md) for evidence from published sources.
5. [Roadmap](ROADMAP.md) for the current milestone and priority order; individual work items are GitHub
   issues labeled `work-item`.

## Document map

- [CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md) gives the current contractility signal in depth.
- [references/timestamps.txt](references/timestamps.txt) catalogs the local reference recordings;
  committed landmark files, the authored state ledger, and generated measurements live beside it.
- [DOCUMENTATION_CONVENTIONS.md](DOCUMENTATION_CONVENTIONS.md) contains authoring, status, ownership, and
  provenance conventions.

Each topic has one authoritative owner. Other documents link to it instead of copying it, and code remains
the source of truth for as-built behavior and constant values.
