"""Contract tests for the compiled-core offline audition client."""

import unittest

import numpy as np

import audition_core


class AnalysisChannelContractTests(unittest.TestCase):
    def test_analysis_selects_channel_zero_without_downmixing(self):
        rendered = np.array([[1.0, 9.0], [2.0, 8.0], [3.0, 7.0]], dtype=np.float32)
        selected = audition_core.analysis_channel(rendered)
        np.testing.assert_array_equal(selected, rendered[:, 0])

    def test_analysis_rejects_an_already_flattened_buffer(self):
        with self.assertRaisesRegex(ValueError, "frames, channels"):
            audition_core.analysis_channel(np.zeros(16, dtype=np.float32))


class StereoDiagnosticTests(unittest.TestCase):
    def test_identical_channels_have_no_side_energy(self):
        rendered = np.column_stack([np.arange(1.0, 5.0), np.arange(1.0, 5.0)])
        self.assertEqual(audition_core.side_to_mid_db(rendered), float("-inf"))

    def test_side_diagnostic_is_only_defined_for_stereo(self):
        self.assertIsNone(audition_core.side_to_mid_db(np.zeros((8, 1))))


if __name__ == "__main__":
    unittest.main()
