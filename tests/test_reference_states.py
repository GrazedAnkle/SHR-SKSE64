"""Schema tests for the hand-authored reference-state ledger."""

import tempfile
import unittest
from pathlib import Path

from tools import check_data_citations as citations
from tools import reference_states as rs


def unknown(reason="not_recorded"):
    return {"status": "unknown", "reason": reason}


def complete_record(scope="ref1.groups.baseline"):
    record = {"scope": scope}
    for field in rs.MANDATORY_STATE_DIMENSIONS:
        record[field] = unknown()
    record["heart_rate"] = {"status": "measured", "value_from": "ref1.groups.baseline.hr"}
    record["contractility"] = {
        "status": "context_inferred",
        "value": "baseline",
        "source": "fixture protocol",
        "rationale": "resting context",
    }
    return record


class ReferenceStateSchemaTests(unittest.TestCase):
    def setUp(self):
        self.measurements = {"ref1": {"groups": {"baseline": {"hr": 72.0}}}}

    def test_schema_vocabulary_has_descriptions(self):
        self.assertEqual(
            tuple(rs.MANDATORY_STATE_DIMENSION_DESCRIPTIONS), rs.MANDATORY_STATE_DIMENSIONS
        )
        self.assertEqual(tuple(rs.OPTIONAL_STATE_DIMENSION_DESCRIPTIONS),
                         rs.OPTIONAL_STATE_DIMENSIONS)
        self.assertEqual(tuple(rs.STATE_DIMENSION_DESCRIPTIONS), rs.STATE_DIMENSIONS)
        self.assertEqual(frozenset(rs.KNOWN_STATUS_DESCRIPTIONS), rs.KNOWN_STATUSES)
        self.assertEqual(frozenset(rs.UNKNOWN_REASON_DESCRIPTIONS), rs.UNKNOWN_REASONS)
        self.assertEqual(frozenset(rs.OBSERVATION_FIELD_DESCRIPTIONS), rs.OBSERVATION_KEYS)
        for descriptions in (
            rs.STATE_DIMENSION_DESCRIPTIONS,
            rs.MANDATORY_STATE_DIMENSION_DESCRIPTIONS,
            rs.OPTIONAL_STATE_DIMENSION_DESCRIPTIONS,
            rs.KNOWN_STATUS_DESCRIPTIONS,
            rs.UNKNOWN_REASON_DESCRIPTIONS,
            rs.OBSERVATION_FIELD_DESCRIPTIONS,
        ):
            self.assertTrue(all(descriptions.values()))

    def test_normalize_resolves_live_measurement_values(self):
        state = rs.normalize_state(complete_record(), self.measurements)
        self.assertEqual(state["heart_rate"]["value"], 72.0)
        self.assertEqual(state["heart_rate"]["source"], "ref1.groups.baseline.hr")
        self.assertEqual(state["contractility"]["status"], "context_inferred")

    def test_optional_dimension_is_validated_when_present(self):
        record = complete_record()
        record["lung_inflation"] = {
            "status": "reported",
            "value": "end_inspiratory_hold",
            "source": "fixture protocol",
        }
        state = rs.normalize_state(record, self.measurements)
        self.assertEqual(state["lung_inflation"]["value"], "end_inspiratory_hold")
        self.assertEqual(rs.validate_embedded_state(state), [])

    def test_every_dimension_is_mandatory_even_when_unknown(self):
        record = complete_record()
        del record["fatigue"]
        with self.assertRaisesRegex(ValueError, "fatigue"):
            rs.normalize_state(record, self.measurements)

    def test_unknown_reason_distinguishes_open_work_from_corpus_limits(self):
        for reason in rs.UNKNOWN_REASONS:
            record = complete_record()
            record["fatigue"] = unknown(reason)
            state = rs.normalize_state(record, self.measurements)
            self.assertEqual(state["fatigue"]["reason"], reason)

    def test_context_inference_requires_a_non_acoustic_rationale(self):
        record = complete_record()
        del record["contractility"]["rationale"]
        with self.assertRaisesRegex(ValueError, "rationale"):
            rs.normalize_state(record, self.measurements)

    def test_ledger_attaches_state_and_rejects_uncovered_groups(self):
        record = complete_record()
        dimensions = "\n".join(
            f'{field} = {{ status = "unknown", reason = "not_recorded" }}'
            for field in rs.MANDATORY_STATE_DIMENSIONS
        )
        # Override heart_rate after constructing the string to exercise TOML loading.
        dimensions = dimensions.replace(
            'heart_rate = { status = "unknown", reason = "not_recorded" }',
            'heart_rate = { status = "measured", value_from = "ref1.groups.baseline.hr" }',
        )
        text = f'schema_version = 1\n[[state]]\nscope = "ref1.groups.baseline"\n{dimensions}\n'
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "states.toml"
            path.write_text(text, encoding="utf-8")
            rs.attach_reference_states(self.measurements, path)
        self.assertEqual(self.measurements["ref1"]["groups"]["baseline"]["state"]["kind"],
                         "reference_state")

    def test_embedded_mixed_state_requires_members(self):
        self.assertTrue(rs.validate_embedded_state({"kind": "mixed_reference_states", "members": []}))

    def test_calibration_state_key_resolves_and_matches_its_scope(self):
        state = rs.normalize_state(complete_record(), self.measurements)
        self.measurements["ref1"]["groups"]["baseline"]["state"] = state
        self.assertEqual(
            citations.state_target_problems(
                self.measurements, "ref1.groups.baseline.state", "fixture"
            ),
            [],
        )
        state["scope"] = "ref1.groups.wrong"
        self.assertTrue(citations.state_target_problems(
            self.measurements, "ref1.groups.baseline.state", "fixture"
        ))

    def test_mixed_calibration_state_checks_every_member(self):
        self.measurements["ref1"]["groups"]["baseline"]["state"] = rs.normalize_state(
            complete_record(), self.measurements
        )
        self.measurements["fit"] = {
            "state": {
                "kind": "mixed_reference_states",
                "members": ["ref1.groups.baseline.state"],
            }
        }
        self.assertEqual(citations.state_target_problems(
            self.measurements, "fit.state", "fixture"
        ), [])
        self.measurements["fit"]["state"]["members"].append("ref1.groups.missing.state")
        self.assertTrue(citations.state_target_problems(
            self.measurements, "fit.state", "fixture"
        ))


if __name__ == "__main__":
    unittest.main()
