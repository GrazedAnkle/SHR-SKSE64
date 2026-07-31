"""Load and validate the hand-authored reference-state ledger.

The ledger lives separately from generated measurements so contextual judgments
survive ``ref_analyze.py --all``.  This module normalizes those judgments into
the measurement node named by each record's ``scope``. The exported description
maps below are the schema vocabulary for authors and validators.
"""

from __future__ import annotations

import tomllib
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1

# Mandatory dimensions. These describe the reference, not an engine fixture:
# normalized engine coordinates belong in a calibration's state_compatibility.
MANDATORY_STATE_DIMENSION_DESCRIPTIONS = {
    "heart_rate": ("Heart rate over this measurement scope, normally in beats/min from an adjacent leaf."),
    "effort_phase": (
        "Protocol phase: rest, rising/steady exercise, recovery, breath hold, or another named maneuver."
    ),
    "demand": (
        "Metabolic workload relative to individual capacity; HR or arousal alone cannot establish it."
    ),
    "respiratory_rate": "Breathing-cycle frequency over the scope, normally in breaths/min.",
    "respiratory_depth": (
        "Per-cycle tidal excursion (breath depth), not instantaneous phase or lung inflation."
    ),
    "contractility": (
        "Intrinsic cardiac inotropy; qualitative context inference is allowed, acoustic self-inference is not."
    ),
    "fatigue": "Acute or accumulated capacity-reducing fatigue relevant to the protocol.",
    "posture": "Body orientation or dynamic posture relevant to physiology and sensor coupling.",
    "auscultation_site": ("Anatomical sensor position or the smallest defensible set of possible positions."),
    "rhythm": (
        "Rhythm class or burden relevant to timing and morphology; 'regular' does not assert "
        "ECG-confirmed sinus rhythm."
    ),
}
MANDATORY_STATE_DIMENSIONS = tuple(MANDATORY_STATE_DIMENSION_DESCRIPTIONS)

# Optional dimensions are useful only for particular maneuvers. They use the
# same observation/status schema when present but need not be padded into every
# unrelated state record.
OPTIONAL_STATE_DIMENSION_DESCRIPTIONS = {
    "lung_inflation": (
        "Instantaneous lung-inflation or respiratory-cycle position during the scope; distinct "
        "from per-cycle tidal excursion and not a claim about maximal lung capacity."
    ),
}
OPTIONAL_STATE_DIMENSIONS = tuple(OPTIONAL_STATE_DIMENSION_DESCRIPTIONS)
STATE_DIMENSION_DESCRIPTIONS = {
    **MANDATORY_STATE_DIMENSION_DESCRIPTIONS,
    **OPTIONAL_STATE_DIMENSION_DESCRIPTIONS,
}
STATE_DIMENSIONS = tuple(STATE_DIMENSION_DESCRIPTIONS)

# A non-unknown observation says how its value was obtained. The acoustic feature
# being calibrated is never an independent source for its own physiological driver.
KNOWN_STATUS_DESCRIPTIONS = {
    "measured": (
        "Derived from an independent numerical/landmark measurement; prefer value_from for committed leaves."
    ),
    "reported": (
        "Explicitly stated or visibly established by the source protocol, narration, or accompanying video."
    ),
    "estimated": (
        "An approximate value, range, or ordinal class from independent evidence with limitations documented."
    ),
    "context_inferred": (
        "A qualitative conclusion from independent protocol context; requires an explicit rationale."
    ),
}
KNOWN_STATUSES = frozenset(KNOWN_STATUS_DESCRIPTIONS)

# Unknown reasons encode the state of the evidence review, not confidence levels.
# In particular, not_yet_sourced is deliberately temporary; an attempted review
# must replace it with a value, not_recorded, or not_identifiable.
UNKNOWN_REASON_DESCRIPTIONS = {
    "not_yet_sourced": (
        "A plausible source or measurement path exists but has not yet been reviewed or attempted."
    ),
    "not_recorded": (
        "Review confirms the required observation was absent from the available source material."
    ),
    "not_identifiable": (
        "Available evidence was reviewed but cannot isolate the value because of confounds, "
        "circularity, or resolution."
    ),
    "not_applicable": (
        "The dimension has no meaningful value for this scope, such as breathing rate during a hold."
    ),
}
UNKNOWN_REASONS = frozenset(UNKNOWN_REASON_DESCRIPTIONS)

OBSERVATION_FIELD_DESCRIPTIONS = {
    "status": "One known-status enum or 'unknown'.",
    "value": ("Literal scalar, range, list, or qualitative class; mutually exclusive with value_from."),
    "value_from": ("Dotted path to a generated measurement leaf; resolved into value during regeneration."),
    "source": "Committed evidence location or stable source description for a literal value.",
    "rationale": (
        "Required explanation for context_inferred; it must identify the independent context used."
    ),
    "reason": "Required unknown-reason enum when status is unknown.",
    "note": "Optional limitation or interpretation that does not change status/value semantics.",
}
OBSERVATION_KEYS = frozenset(OBSERVATION_FIELD_DESCRIPTIONS)


def split_key(key: str) -> list[str]:
    """Split a dotted path while allowing dots inside double-quoted components."""
    parts: list[str] = []
    current: list[str] = []
    quoted = False
    escaped = False
    for char in key:
        if escaped:
            current.append(char)
            escaped = False
        elif quoted and char == "\\":
            escaped = True
        elif char == '"':
            quoted = not quoted
        elif char == "." and not quoted:
            part = "".join(current)
            if not part:
                raise ValueError("empty path component")
            parts.append(part)
            current = []
        else:
            current.append(char)
    if quoted or escaped:
        raise ValueError("unterminated quoted component")
    part = "".join(current)
    if not part:
        raise ValueError("empty path component")
    parts.append(part)
    return parts


def resolve(data: Any, key: str) -> Any:
    value = data
    for component in split_key(key):
        if not isinstance(value, dict) or component not in value:
            raise KeyError(component)
        value = value[component]
    return value


def quoted_component(component: str) -> str:
    """Return a component safe for this module's dotted-path grammar."""
    escaped = component.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def group_state_path(reference: str, group: str) -> str:
    return f"{reference}.groups.{quoted_component(group)}.state"


def _normalize_observation(name: str, raw: Any, measurements: dict) -> dict:
    if not isinstance(raw, dict):
        raise ValueError(f"{name}: expected an observation table")
    extra = set(raw) - OBSERVATION_KEYS
    if extra:
        raise ValueError(f"{name}: unknown field(s): {', '.join(sorted(extra))}")

    status = raw.get("status")
    if status in KNOWN_STATUSES:
        has_value = "value" in raw
        has_value_from = "value_from" in raw
        if has_value == has_value_from:
            raise ValueError(f"{name}: {status} requires exactly one of 'value' or 'value_from'")
        source = raw.get("source")
        if has_value and (not isinstance(source, str) or not source):
            raise ValueError(f"{name}: {status} with a literal value requires a non-empty 'source'")
        if "reason" in raw:
            raise ValueError(f"{name}: known observations cannot have an unknown 'reason'")
        if status == "context_inferred" and not raw.get("rationale"):
            raise ValueError(f"{name}: context_inferred requires a 'rationale'")

        out = {"status": status}
        if has_value_from:
            value_from = raw["value_from"]
            if not isinstance(value_from, str) or not value_from:
                raise ValueError(f"{name}: 'value_from' must be a non-empty path")
            try:
                out["value"] = resolve(measurements, value_from)
            except (KeyError, ValueError) as exc:
                raise ValueError(f"{name}: value_from '{value_from}' does not resolve ({exc})") from exc
            out["source"] = value_from
        else:
            out["value"] = raw["value"]
            out["source"] = source
        for optional in ("rationale", "note"):
            if optional in raw:
                out[optional] = raw[optional]
        return out

    if status == "unknown":
        reason = raw.get("reason")
        if reason not in UNKNOWN_REASONS:
            allowed = ", ".join(sorted(UNKNOWN_REASONS))
            raise ValueError(f"{name}: unknown requires reason in {{{allowed}}}")
        forbidden = set(raw) & {"value", "value_from", "rationale"}
        if forbidden:
            raise ValueError(f"{name}: unknown cannot have {', '.join(sorted(forbidden))}")
        out = {"status": "unknown", "reason": reason}
        if "note" in raw:
            out["note"] = raw["note"]
        return out

    allowed = ", ".join(sorted(KNOWN_STATUSES | {"unknown"}))
    raise ValueError(f"{name}: status must be one of {allowed}")


def normalize_state(raw: Any, measurements: dict, *, expected_scope: str | None = None) -> dict:
    """Validate one source record and return its generated-JSON representation."""
    if not isinstance(raw, dict):
        raise ValueError("state record must be a table")
    scope = raw.get("scope")
    if not isinstance(scope, str) or not scope:
        raise ValueError("state record needs a non-empty 'scope'")
    if expected_scope is not None and scope != expected_scope:
        raise ValueError(f"state scope is {scope!r}, expected {expected_scope!r}")
    missing = [field for field in MANDATORY_STATE_DIMENSIONS if field not in raw]
    if missing:
        raise ValueError(f"{scope}: missing mandatory dimension(s): {', '.join(missing)}")
    allowed = {"scope", "note", *STATE_DIMENSIONS}
    extra = set(raw) - allowed
    if extra:
        raise ValueError(f"{scope}: unknown record field(s): {', '.join(sorted(extra))}")

    out = {"kind": "reference_state", "scope": scope}
    for field in STATE_DIMENSIONS:
        if field in raw:
            out[field] = _normalize_observation(f"{scope}.{field}", raw[field], measurements)
    if "note" in raw:
        out["note"] = raw["note"]
    return out


def attach_reference_states(measurements: dict, ledger_path: Path) -> None:
    """Load the TOML ledger and attach each normalized state in-place."""
    parsed = tomllib.loads(ledger_path.read_text(encoding="utf-8"))
    if parsed.get("schema_version") != SCHEMA_VERSION:
        raise ValueError(
            f"{ledger_path}: schema_version must be {SCHEMA_VERSION}, got {parsed.get('schema_version')!r}"
        )
    records = parsed.get("state")
    if not isinstance(records, list) or not records:
        raise ValueError(f"{ledger_path}: expected one or more [[state]] records")

    seen: set[str] = set()
    for raw in records:
        scope = raw.get("scope") if isinstance(raw, dict) else None
        if not isinstance(scope, str):
            raise ValueError(f"{ledger_path}: every [[state]] needs a string scope")
        if scope in seen:
            raise ValueError(f"{ledger_path}: duplicate state scope {scope!r}")
        seen.add(scope)
        try:
            node = resolve(measurements, scope)
        except (KeyError, ValueError) as exc:
            raise ValueError(f"{ledger_path}: scope '{scope}' does not resolve ({exc})") from exc
        if not isinstance(node, dict):
            raise ValueError(f"{ledger_path}: scope '{scope}' does not name an object")
        if "state" in node:
            raise ValueError(f"{ledger_path}: scope '{scope}' already has a state")
        node["state"] = normalize_state(raw, measurements)

    # Group measurements are the reusable calibration units. Requiring a state
    # on every one prevents a new group from silently entering the mixed-state
    # systole fit without ledger coverage.
    for reference, entry in measurements.items():
        if not reference.startswith("ref") or not isinstance(entry, dict):
            continue
        for group, values in entry.get("groups", {}).items():
            if "state" not in values:
                raise ValueError(f"{ledger_path}: no state for {reference} group {group!r}")


def validate_embedded_state(state: Any, *, expected_scope: str | None = None) -> list[str]:
    """Return structural problems for a generated state record."""
    problems: list[str] = []
    if not isinstance(state, dict):
        return ["state is not an object"]
    if state.get("kind") == "mixed_reference_states":
        members = state.get("members")
        if not isinstance(members, list) or not members or not all(isinstance(v, str) for v in members):
            problems.append("mixed state needs a non-empty string 'members' array")
        return problems
    if state.get("kind") != "reference_state":
        problems.append("state kind must be 'reference_state' or 'mixed_reference_states'")
        return problems
    scope = state.get("scope")
    if not isinstance(scope, str) or not scope:
        problems.append("reference state needs a non-empty scope")
    elif expected_scope is not None and scope != expected_scope:
        problems.append(f"state scope {scope!r} does not match {expected_scope!r}")
    for field in MANDATORY_STATE_DIMENSIONS:
        obs = state.get(field)
        if not isinstance(obs, dict):
            problems.append(f"missing observation '{field}'")
            continue
        status = obs.get("status")
        if status in KNOWN_STATUSES:
            if "value" not in obs or not isinstance(obs.get("source"), str) or not obs.get("source"):
                problems.append(f"{field}: {status} needs value and source")
            if status == "context_inferred" and not obs.get("rationale"):
                problems.append(f"{field}: context_inferred needs rationale")
        elif status == "unknown":
            if obs.get("reason") not in UNKNOWN_REASONS:
                problems.append(f"{field}: invalid unknown reason")
        else:
            problems.append(f"{field}: invalid status {status!r}")
    for field in OPTIONAL_STATE_DIMENSIONS:
        if field in state:
            observation_problems = validate_embedded_observation(field, state[field])
            problems.extend(observation_problems)
    return problems


def validate_embedded_observation(field: str, obs: Any) -> list[str]:
    """Return structural problems for one optional embedded observation."""
    if not isinstance(obs, dict):
        return [f"{field}: observation is not an object"]
    status = obs.get("status")
    if status in KNOWN_STATUSES:
        problems = []
        if "value" not in obs or not isinstance(obs.get("source"), str) or not obs.get("source"):
            problems.append(f"{field}: {status} needs value and source")
        if status == "context_inferred" and not obs.get("rationale"):
            problems.append(f"{field}: context_inferred needs rationale")
        return problems
    if status == "unknown":
        return [] if obs.get("reason") in UNKNOWN_REASONS else [f"{field}: invalid unknown reason"]
    return [f"{field}: invalid status {status!r}"]
