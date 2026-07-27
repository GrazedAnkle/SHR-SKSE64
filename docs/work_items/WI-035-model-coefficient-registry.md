# WI-035: Model-Coefficient Registry Deduplication and Drift Guards

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

The immutable coefficient surface has one auditable enumeration of live fields, and an automated gate
fails when the typed C++ groups, Python override lookup, or Python value reporting drift apart.

The implementation must:

- make every live coefficient's stable name, scalar type, and owning group originate from one registry
  entry, or prove exact equivalence to that registry at build/test time;
- drive both binding override lookup and `ModelCoefficients.values` reporting from the same metadata;
- preserve strict integer handling for structural controls, immutable batch replacement, derived-value
  reporting, and reason-specific rejection of unsupported controls;
- retain `Constants.hpp` as the compact, human-auditable production-default and provenance index;
- keep cross-field and consuming-context validation explicit where it protects a formula rather than a
  single field; and
- leave the public coefficient names, production defaults, and compiled-core goldens unchanged.

The drift gate must detect missing, duplicate, mis-grouped, and mistyped live fields. It must also prove
that every non-live constant is deliberately classified as derived, asset-fixed, utility,
game-integration, or dormant rather than silently omitted.

## Current conclusion

The current surface repeats live field names in `Constants.hpp`, the typed groups in
`ModelCoefficients.hpp`, binding override dispatch, and binding value reporting. Those repetitions do not
all have the same role. `Constants.hpp` deliberately owns values, units, domains, semantics, and
provenance; relational validators deliberately name the fields participating in each formula. The
accidental duplication is the structural enumeration shared by the typed groups and the binding's two
registry paths.

C++23 cannot reflect aggregate field names, so this cannot be removed with standard reflection. The
smallest useful design is group-aware coefficient metadata that supplies at least name, scalar type, and
member identity. Binding descriptor tables can then use one entry for lookup and reporting. To eliminate
the typed-group repetition as well, a compact X-macro or included definition table can generate both the
member declarations and descriptors while each member still takes its default from `Constants.hpp`.

An external YAML/TOML schema and general code generator are not justified while the metadata serves only
C++ storage and one binding. Reconsider generation only if units, provenance, CLI help, experiment
manifests, or documentation become additional consumers.

## Scope and non-goals

Consolidate the live coefficient field metadata, replace the binding's independent setter/reporter
enumerations, and add an exact drift check to the normal coefficient or constant gates. Keep unsupported
control classifications explicit and actionable.

Do not retune coefficients, rename the Python API, expose additional controls, change
`RuntimeSettings`, generate model formulas or relational validation, or replace the existing
`Constants.hpp` provenance workflow. Do not add a reflection or serialization dependency solely for this
registry.

## Dependencies

None. The current immutable coefficient boundary is owned by
[ARCHITECTURE.md](../ARCHITECTURE.md#value-and-responsibility-split). The thin offline clients consume
the stable binding names as they exist now, so registry deduplication can proceed independently.

## Next action and decision points

- Prototype a binding-only descriptor table and a shared X-macro/definition-table version; compare
  auditability, compiler diagnostics, and the amount of real duplication each removes.
- Decide whether the canonical registry generates typed members or whether a lightweight source audit
  proves the handwritten groups equivalent.
- Define the unsupported-control classification beside the live registry or in the drift checker without
  weakening the current reason-specific errors.
- Add a negative fixture that deliberately omits or mistypes one entry so the drift gate itself is proven.

## Newly observed work to split out

If coefficient metadata later becomes a public manifest or documentation source, split schema/code
generation into its own work item rather than expanding this maintenance refactor silently.
