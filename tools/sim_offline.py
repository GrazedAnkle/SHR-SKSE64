"""Offline mirror of HeartRateSimulation::Step.

The dynamics counterpart to engine_offline.py: a one-to-one port of
src/Simulation.cpp for observing HR and contractility trends, including
contractility-driven post-exercise systole shortening.

Constants are parsed directly from src/Constants.hpp. Logic and structure are
reimplemented here and must be kept in sync with C++.

Only continuous exertion profiles are modelled; time-warp paths are not.

Usage:
  python sim_offline.py                          # default rest->sprint->rest profile, prints summary
  python sim_offline.py --csv out.csv            # also dump the full per-step trajectory
  python sim_offline.py --resting 50 --max 195
  python sim_offline.py --set SlowRecoveryTau=90 # try a value without rebuilding the DLL
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path

import shrlib

_C = shrlib.parse_constants(Path(__file__).resolve().parent.parent / "src" / "Constants.hpp")


def _clamp(x: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, x))


class Sim:
    def __init__(self, resting_hr: float, max_hr: float):
        self.C = _C
        self.resting_hr = resting_hr
        self.max_hr = max_hr
        c = self.C
        self.fitness = c["FitnessBaseMets"] + (c["BaseRestingHR"] - resting_hr) / c["RestingHRSlope"]
        self.exertion = c["IdleMets"]
        self.acute_fatigue = 0.0
        self.long_term_fatigue = 0.0
        self.target_hr = self.compute_target_hr(c["IdleMets"])
        self.fast_hr = c["HRFastFraction"] * self.target_hr
        self.slow_hr = (1.0 - c["HRFastFraction"]) * self.target_hr
        self.adrenaline = 0.0
        self.contractility = 0.0
        self.resp_rate = c["RestingRespRate"]
        self.resp_depth = 0.0
        self.resp_phase = 0.0

    @property
    def heart_rate(self) -> float:
        return self.fast_hr + self.slow_hr

    def effective_fitness(self) -> float:
        c = self.C
        return max(self.fitness - self.acute_fatigue - self.long_term_fatigue, c["FitnessAbsoluteMin"])

    def effective_resting_hr(self) -> float:
        c = self.C
        return c["BaseRestingHR"] - (self.effective_fitness() - c["FitnessBaseMets"]) * c["RestingHRSlope"]

    def contractility_excess(self) -> float:
        eff_resting = self.effective_resting_hr()
        hr_implied = _clamp((self.heart_rate - eff_resting) / (self.max_hr - eff_resting), 0.0, 1.0)
        return max(0.0, self.contractility - hr_implied)

    def compute_target_hr(self, exertion: float) -> float:
        c = self.C
        effective = self.effective_fitness()
        eff_resting = c["BaseRestingHR"] - (effective - c["FitnessBaseMets"]) * c["RestingHRSlope"]
        fraction = max((exertion - c["IdleMets"]) / (effective - c["IdleMets"]), 0.0)
        target = eff_resting + fraction * (self.max_hr - eff_resting)
        return min(target, c["HRFormulaCeiling"])

    def contractility_target(self) -> float:
        c = self.C
        norm_exertion = _clamp(
            (self.exertion - c["IdleMets"]) / (self.effective_fitness() - c["IdleMets"]), 0.0, 1.0)
        adrenergic = _clamp(self.adrenaline / c["AdrenalineContractilityScale"], 0.0, 1.0)
        return min(norm_exertion + adrenergic, 1.0)

    def _update_exertion(self, state: dict, delta: float):
        c = self.C
        self.adrenaline *= math.exp(-math.log(2.0) * delta / c["AdrenalineHalfLife"])

        if state.get("sprint"):
            movement = c["SprintingMets"]
        elif state.get("run"):
            movement = c["RunningMets"]
        elif state.get("walk"):
            movement = c["WalkingMets"]
        else:
            movement = c["IdleMets"]

        is_moving = movement > c["IdleMets"]
        if is_moving and state.get("swim"):
            movement = c["SwimmingMets"]
        if state.get("sneak"):
            movement = c["IdleMets"] + (movement - c["IdleMets"]) * c["CrouchMovementMultiplier"]
        if state.get("mount"):
            movement = c["IdleMets"] + (movement - c["IdleMets"]) * c["MountedMultiplier"]

        target_mets = min(movement + self.adrenaline, self.effective_fitness())
        diff = target_mets - self.exertion
        rate = c["ExertionAccumulationRate"] if diff > 0.0 else c["ExertionRecoveryRate"]
        self.exertion += math.copysign(min(abs(diff), rate * delta), diff)

    def _update_acute_fatigue(self, delta: float):
        c = self.C
        norm = _clamp((self.exertion - c["IdleMets"]) / (self.fitness - c["IdleMets"]), 0.0, 1.0)
        target = c["AcuteFatigueMax"] * norm
        diff = target - self.acute_fatigue
        tau = c["AcuteFatigueGainTau"] if diff > 0.0 else c["AcuteFatigueDecayTau"]
        self.acute_fatigue += (1.0 - math.exp(-delta / tau)) * diff

    def _update_long_term_fatigue(self, game_hours: float):
        c = self.C
        norm = self.acute_fatigue / c["AcuteFatigueMax"]
        target = c["LongTermFatigueMax"] * norm
        diff = target - self.long_term_fatigue
        tau = c["LongTermFatigueGainTau"] if diff > 0.0 else c["LongTermFatigueDecayTau"]
        self.long_term_fatigue += (1.0 - math.exp(-game_hours / tau)) * diff

    def _update_fitness(self, game_hours: float):
        c = self.C
        norm_fatigue = (self.acute_fatigue / c["AcuteFatigueMax"]
                        + self.long_term_fatigue / c["LongTermFatigueMax"]) * 0.5
        efficacy = max(0.0, 1.0 - norm_fatigue)
        is_training = self.exertion > c["IdleMets"]
        target = c["FitnessMaxMets"] if is_training else c["FitnessBaseMets"]
        tau = c["FitnessGainTau"] if is_training else c["FitnessDecayTau"]
        step = (1.0 - math.exp(-game_hours / tau)) * (target - self.fitness)
        self.fitness += step * efficacy if is_training else step

    def _update_respiration(self, delta: float):
        c = self.C
        self.resp_phase = math.fmod(self.resp_phase + self.resp_rate / 60.0 * delta, 1.0)
        fraction = _clamp((self.exertion - c["IdleMets"]) /
                          (self.effective_fitness() - c["IdleMets"]), 0.0, 1.0)
        target_rate, target_depth = shrlib.ventilation_targets(c, fraction)
        diff = target_rate - self.resp_rate
        tau = c["RespOnsetTau"] if diff > 0.0 else c["RespRecoveryTau"]
        self.resp_rate += (1.0 - math.exp(-delta / tau)) * diff
        depth_diff = target_depth - self.resp_depth
        depth_tau = (c["BreathDepthOnsetTau"] if depth_diff > 0.0
                     else c["BreathDepthRecoveryTau"])
        self.resp_depth += (1.0 - math.exp(-delta / depth_tau)) * depth_diff

    def _update_contractility(self, delta: float):
        c = self.C
        target = self.contractility_target()
        tau = c["ContractilityOnsetTau"] if target > self.contractility else c["ContractilityDecayTau"]
        self.contractility += (1.0 - math.exp(-delta / tau)) * (target - self.contractility)

    def _update_current_hr(self, delta: float):
        c = self.C
        norm_fitness = _clamp(
            (self.effective_fitness() - c["FitnessBaseMets"]) / (c["FitnessMaxMets"] - c["FitnessBaseMets"]),
            0.0, 1.0)
        fast_onset = _lerp(c["FastOnsetTauSedentary"], c["FastOnsetTauElite"], norm_fitness)
        fast_recovery = _lerp(c["FastRecoveryTauSedentary"], c["FastRecoveryTauElite"], norm_fitness)

        def exp_step(current, target, onset_tau, recovery_tau):
            diff = target - current
            tau = onset_tau if diff > 0.0 else recovery_tau
            return (1.0 - math.exp(-delta / tau)) * diff

        self.fast_hr += exp_step(self.fast_hr, c["HRFastFraction"] * self.target_hr, fast_onset, fast_recovery)
        self.slow_hr += exp_step(self.slow_hr, (1.0 - c["HRFastFraction"]) * self.target_hr,
                                 c["SlowOnsetTau"], c["SlowRecoveryTau"])

    def step(self, state: dict, delta: float, game_hours: float = 0.0):
        self._update_exertion(state, delta)
        self._update_acute_fatigue(delta)
        self._update_long_term_fatigue(game_hours)
        self._update_fitness(game_hours)
        self.target_hr = self.compute_target_hr(self.exertion)
        self._update_respiration(delta)
        self._update_contractility(delta)
        self._update_current_hr(delta)

    def nominal_systole(self) -> float:
        c = self.C
        return _clamp(c["SystoleIntercept"] - self.heart_rate * c["SystoleSlope"], c["SystoleMin"], c["SystoleMax"])

    def sinus_systole(self) -> float:
        c = self.C
        return max(self.nominal_systole() - c["SystolePEPShortening"] * self.contractility_excess(), c["SystoleMin"])


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def run(resting_hr: float, max_hr: float, profile, step_size: float):
    sim = Sim(resting_hr, max_hr)
    t = 0.0
    rows = []
    for duration, state in profile:
        elapsed = 0.0
        while elapsed < duration:
            sim.step(state, step_size)
            t += step_size
            elapsed += step_size
            excess = sim.contractility_excess()
            nominal = sim.nominal_systole()
            sinus = sim.sinus_systole()
            resp_fraction = _clamp(
                (sim.exertion - sim.C["IdleMets"]) /
                (sim.effective_fitness() - sim.C["IdleMets"]), 0.0, 1.0)
            target_resp_rate, target_resp_depth = shrlib.ventilation_targets(sim.C, resp_fraction)
            rows.append({
                "t": t, "exertion": sim.exertion, "adrenaline": sim.adrenaline,
                "targetHR": sim.target_hr, "fastHR": sim.fast_hr, "slowHR": sim.slow_hr,
                "HR": sim.heart_rate, "contractility": sim.contractility,
                "hrImplied": sim.contractility - excess if excess > 0 else None,
                "contractilityExcess": excess, "nominalSystole_ms": nominal * 1000.0,
                "sinusSystole_ms": sinus * 1000.0, "systoleDev_ms": (sinus - nominal) * 1000.0,
                "respRate": sim.resp_rate, "respDepth": sim.resp_depth,
                "targetRespRate": target_resp_rate, "targetRespDepth": target_resp_depth,
            })
    return rows


def summarize(rows, recovery_start_t: float):
    """Print peak contractility values and the recovery systole-deviation locus."""
    rec = [r for r in rows if r["t"] >= recovery_start_t]
    peak = max(rows, key=lambda r: r["contractilityExcess"])
    print(f"peak contractility   = {max(r['contractility'] for r in rows):.3f}")
    print(f"peak excess          = {peak['contractilityExcess']:.3f} "
          f"at t={peak['t']:.1f}s, HR={peak['HR']:.0f}, systoleDev={peak['systoleDev_ms']:.1f}ms")
    print(f"max systole shortening = {min(r['systoleDev_ms'] for r in rows):.1f}ms")
    print("\nRecovery decoupling locus (systole deviation as HR falls):")
    # TODO: Pull reference values from measurements.json
    print(f"  {'HR':>6}  {'excess':>7}  {'dev_ms':>7}    (refs: ref8 142->-12, ref12 86->-58)")
    # Sample the first recovery row at or below each HR target.
    for hr_target in (180, 160, 142, 120, 100, 86, 70):
        below = [r for r in rec if r["HR"] <= hr_target]
        if below:
            r = below[0]
            print(f"  {r['HR']:6.0f}  {r['contractilityExcess']:7.3f}  {r['systoleDev_ms']:7.1f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--resting", type=float, default=55.0)
    ap.add_argument("--max", type=float, default=200.0)
    ap.add_argument("--step", type=float, default=0.0333, help="step size (s); ~30 FPS")
    ap.add_argument("--sprint", type=float, default=120.0, help="sprint duration (s)")
    ap.add_argument("--recover", type=float, default=420.0, help="recovery duration (s)")
    ap.add_argument("--csv", type=str, default=None, help="dump full trajectory to CSV")
    ap.add_argument("--set", action="append", default=[], metavar="Name=Value",
                    help="override a Constants.hpp value for this run")
    args = ap.parse_args()

    for override in args.set:
        name, value = override.split("=", 1)
        _C[name] = float(value)

    warmup = 30.0
    profile = [
        (warmup, {}),
        (args.sprint, {"sprint": True}),
        (args.recover, {}),
    ]
    rows = run(args.resting, args.max, profile, args.step)
    recovery_start = warmup + args.sprint
    summarize(rows, recovery_start)

    if args.csv:
        import csv
        with open(args.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print(f"\nwrote {len(rows)} rows -> {args.csv}")


if __name__ == "__main__":
    main()
