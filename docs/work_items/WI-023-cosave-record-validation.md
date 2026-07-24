# WI-023: Co-Save Record Validation

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Co-save loading validates each known record's version, size, and read result before applying it. Truncated,
unknown-version, duplicate, and unknown records produce defined fallback behavior and diagnostics without
partially restoring an incoherent simulation state. Compatibility fixtures cover the oldest supported
record set and the current set.

## Current conclusion

`OnLoad` retrieves `recordVersion` and `recordSize` but dispatches only on record type, and each
`ReadRecordData` result is ignored. Missing records already have deliberate backward-compatible defaults;
malformed or future-version records do not yet have an equally explicit policy. `OnLoad` now collects
legacy records as optional adapter values and translates them into the complete `SimulationState` owned
by the core boundary, making the existing missing-field defaults and migration rules inspectable without
embedding legacy sentinels in the core state.

## Scope and non-goals

Define validation and fallback policy, use the cohesive persisted-state boundary, and retain forward
skipping of unknown record types. Do not change physiological defaults or remove compatibility with
currently supported saves.

## Dependencies

Use the established [`SimulationState` persistence boundary](../ARCHITECTURE.md#runtime-contract); record
validation itself does not require a broader architecture change.

## Next action and decision points

Inventory the record sets emitted by released versions and write malformed/legacy fixtures before changing
the loader. The maintainer decides whether one invalid known record rejects the complete restore or falls
back field-by-field.
