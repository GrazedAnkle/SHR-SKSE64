# WI-023: Co-Save Record Validation

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Co-save loading validates each known record's version, size, and read result before applying it. Truncated,
unknown-version, duplicate, and unknown records produce defined fallback behavior and diagnostics without
partially restoring an incoherent simulation state. Compatibility fixtures cover the oldest supported
record set and the current set.

The policy must additionally distinguish two kinds of record, because the co-save is about to carry both:

- **state records**, where a missing record means "use the initial value" and a malformed one is a loss of
  progression; and
- **settings-override records**, where a missing record means "the player never moved this control" and is
  the ordinary case rather than an error.

## Current conclusion

`OnLoad` retrieves `recordVersion` and `recordSize` but dispatches only on record type, and each
`ReadRecordData` result is ignored. Missing records already have deliberate backward-compatible defaults;
malformed or future-version records do not yet have an equally explicit policy. `OnLoad` now collects
legacy records as optional adapter values and translates them into the complete `SimulationState` owned
by the core boundary, making the existing missing-field defaults and migration rules inspectable without
embedding legacy sentinels in the core state.

That optional-valued shape is the right one for what comes next. [WI-034](WI-034-mcm-capability.md)
persists per-character settings as tri-state overrides, which places a second family of records in the
same co-save whose *absence is meaningful and normal*. This makes the item a hard blocker for that one
rather than a conditional dependency, and it constrains the fallback policy: a settings override cannot
fall back to "the initial value" the way a state field does, because its unset state already resolves
through the profile default. A malformed override must therefore be droppable to unset without touching
the restored simulation, and a malformed state record must not silently be read as an override or the
reverse.

Several state fields also carry in-band sentinels - a non-positive fitness or respiratory rate, a negative
contractility or respiratory depth - that currently distinguish "absent or invalid" from "genuinely
persisted". Deciding whether those stay as sentinels or become the same optional shape belongs to this
item, since the answer sets whether validation happens at read time or at translation time.

## Scope and non-goals

Define validation and fallback policy for both record families, use the cohesive persisted-state boundary,
and retain forward skipping of unknown record types. Do not change physiological defaults, do not remove
compatibility with currently supported saves, and do not design the settings surface itself - this item
owns only what a settings record must satisfy to be applied.

## Dependencies

Use the established [`SimulationState` persistence boundary](../ARCHITECTURE.md#runtime-contract); record
validation itself does not require a broader architecture change.

## Next action and decision points

Inventory the record sets emitted by released versions and write malformed/legacy fixtures before changing
the loader. Decide:

- whether one invalid known state record rejects the complete restore or falls back field-by-field
  (maintainer's call);
- whether a malformed settings override is dropped to unset silently or with a warning, given that
  dropping it changes live behavior rather than only historical state; and
- whether the in-band sentinels above are retained or replaced by the optional shape already used at the
  adapter edge.
