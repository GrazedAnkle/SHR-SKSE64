"""Contract tests for the thin rhythm/simulation clients and their override plumbing."""
import unittest
from pathlib import Path

import core_offline
import rhythm_offline
import sim_offline

ROOT = Path(__file__).resolve().parents[1]


SOURCE = (
    ROOT
    / "contrib"
    / "Distribution"
    / "Sound"
    / "fx"
    / "SHR_HeartBeat"
    / "HeartBeat_Shortened.wav"
)


@unittest.skipUnless(
    SOURCE.is_file() and core_offline.binding_available(),
    "compiled binding and heartbeat source are required",
)
class OfflineClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.module = core_offline.load_binding()

    def test_cli_override_parser_preserves_structural_integer_type(self):
        coefficients, overrides = core_offline.coefficients_from_args(
            self.module,
            ["BreathLowPassPoles=3"],
        )
        self.assertEqual(overrides, {"BreathLowPassPoles": 3})
        self.assertIsInstance(coefficients.values["BreathLowPassPoles"], int)
        self.assertEqual(coefficients.values["BreathLowPassPoles"], 3)

    def test_rhythm_client_passes_vigor_override_to_bound_engine(self):
        coefficients = self.module.default_model_coefficients.with_overrides(
            {"VigorJitterScale": 0.0}
        )
        sequence = rhythm_offline.beat_sequence(
            SOURCE,
            hr=176.0,
            contractility=0.75,
            exertion=0.75,
            n_beats=3,
            seed=11,
            module=self.module,
            coefficients=coefficients,
        )
        self.assertTrue(
            all(beat["event"].vigor == 0.75 for beat in sequence["beats"])
        )

    def test_simulation_client_passes_override_to_bound_runtime(self):
        profile = [(1.0, {"sprint": True})]
        default_rows = sim_offline.run(
            55.0,
            200.0,
            profile,
            1.0,
            module=self.module,
            coefficients=self.module.default_model_coefficients,
        )
        slower = self.module.default_model_coefficients.with_overrides(
            {"ExertionAccumulationRate": 0.5}
        )
        overridden_rows = sim_offline.run(
            55.0,
            200.0,
            profile,
            1.0,
            module=self.module,
            coefficients=slower,
        )
        self.assertLess(
            overridden_rows[-1]["exertion"],
            default_rows[-1]["exertion"],
        )


if __name__ == "__main__":
    unittest.main()
