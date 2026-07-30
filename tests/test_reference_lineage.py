"""Tests for logical-reference lineage in generated measurements."""

import unittest

import ref_analyze


class ReferenceLineageTests(unittest.TestCase):
    def test_ref12_clip_merges_into_ref13_with_scoped_breath_and_provenance(self):
        data = {
            "ref12": {
                "source": "docs/references/original/12.wav",
                "annotations": "docs/references/timestamps/12.txt",
                "groups": {"recovery": {"hr": 150.0}},
                "breath": {"resp_rate_bpm": 19.8},
            },
            "ref13": {
                "source": "docs/references/original/13.wav",
                "annotations": "docs/references/timestamps/13.txt",
                "groups": {"exercise": {"hr": 168.0}},
                "breath": {"resp_rate_bpm": 24.0},
            },
        }

        ref_analyze.merge_derived_clips(data)

        self.assertNotIn("ref12", data)
        parent = data["ref13"]
        self.assertEqual(set(parent["groups"]), {"exercise", "recovery"})
        self.assertEqual(parent["groups"]["recovery"]["provenance"]["derived_clip"], "ref12")
        self.assertEqual(parent["derived_clips"]["ref12"]["parent_start_s"], 242.912)
        self.assertEqual(parent["breath_scopes"]["exercise"]["resp_rate_bpm"], 24.0)
        self.assertEqual(parent["breath_scopes"]["recovery"]["resp_rate_bpm"], 19.8)


if __name__ == "__main__":
    unittest.main()
