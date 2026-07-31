"""Observe a compiled-core physiology trajectory.

The default rest -> sprint -> recovery profile is driven through bound ``Runtime.step``. Python retains
the scenario, CSV, and summary surfaces; all physiology and acoustic-mapping values come from ``shr_core``.

Usage:
  python tools/sim_offline.py
  python tools/sim_offline.py --csv out.csv
  python tools/sim_offline.py --resting 50 --max 195
  python tools/sim_offline.py --set SlowRecoveryTau=90
"""

from __future__ import annotations

import argparse
from pathlib import Path

import core_offline


_PLAYER_KEYS = {
    "dead": "is_dead",
    "sprint": "is_sprinting",
    "run": "is_running",
    "walk": "is_walking",
    "swim": "is_swimming",
    "sneak": "is_sneaking",
    "mount": "is_on_mount",
}


def _player_state(module, state: dict):
    unknown = set(state) - set(_PLAYER_KEYS)
    if unknown:
        raise ValueError(f"unknown player-state fields: {sorted(unknown)}")
    return module.PlayerState(
        **{target: bool(state.get(source, False)) for source, target in _PLAYER_KEYS.items()}
    )


def _systoles(module, snapshot, coefficients) -> tuple[float, float]:
    nominal_ibi = 60.0 / snapshot.heart_rate
    event = module.BeatEvent(
        ibi=nominal_ibi,
        filling_interval=nominal_ibi,
        coupling_fraction=0.0,
        vigor=snapshot.contractility,
        kind="sinus",
    )
    nominal_snapshot = module.PhysiologySnapshot(
        heart_rate=snapshot.heart_rate,
        contractility_excess=0.0,
    )
    nominal = module.create_render_spec(
        event=event,
        physiology=nominal_snapshot,
        coefficients=coefficients,
    ).systole_duration
    sinus = module.create_render_spec(
        event=event,
        physiology=snapshot,
        coefficients=coefficients,
    ).systole_duration
    return float(nominal), float(sinus)


def run(
    resting_hr: float,
    max_hr: float,
    profile,
    step_size: float,
    *,
    module=None,
    coefficients=None,
    module_dir: str | Path = core_offline.DEFAULT_MODULE_DIR,
) -> list[dict]:
    """Run a profile through the bound Runtime and return the preserved observation rows."""
    if step_size <= 0.0:
        raise ValueError("step_size must be positive")
    module = module or core_offline.load_binding(module_dir)
    coefficients = coefficients or module.default_model_coefficients
    runtime = module.Runtime(
        resting_heart_rate=resting_hr,
        maximum_heart_rate=max_hr,
        arrhythmia_susceptibility=0.0,
        seed=0,
        coefficients=coefficients,
    )
    runtime.init()

    time = 0.0
    rows: list[dict] = []
    for duration, state in profile:
        elapsed = 0.0
        player = _player_state(module, state)
        while elapsed < duration:
            result = runtime.step(
                player=player,
                delta_seconds=step_size,
                game_hours_delta=0.0,
                output_enabled=False,
            )
            snapshot = result.physiology
            time += step_size
            elapsed += step_size
            nominal, sinus = _systoles(module, snapshot, coefficients)
            excess = float(snapshot.contractility_excess)
            rows.append(
                {
                    "t": time,
                    "exertion": float(snapshot.exertion),
                    "adrenaline": float(snapshot.adrenaline),
                    "targetHR": float(runtime.target_heart_rate),
                    "fastHR": float(snapshot.fast_heart_rate),
                    "slowHR": float(snapshot.slow_heart_rate),
                    "HR": float(snapshot.heart_rate),
                    "contractility": float(snapshot.contractility),
                    "hrImplied": (float(snapshot.contractility) - excess if excess > 0.0 else None),
                    "contractilityExcess": excess,
                    "nominalSystole_ms": nominal * 1000.0,
                    "sinusSystole_ms": sinus * 1000.0,
                    "systoleDev_ms": (sinus - nominal) * 1000.0,
                    "respRate": float(snapshot.respiration_rate),
                    "respDepth": float(snapshot.respiration_depth),
                    "targetRespRate": float(runtime.target_respiration_rate),
                    "targetRespDepth": float(runtime.target_respiration_depth),
                }
            )
    return rows


def summarize(rows: list[dict], recovery_start_t: float) -> None:
    """Print peak contractility values and the recovery systole-deviation locus."""
    recovery = [row for row in rows if row["t"] >= recovery_start_t]
    peak = max(rows, key=lambda row: row["contractilityExcess"])
    print(f"peak contractility   = {max(row['contractility'] for row in rows):.3f}")
    print(
        f"peak excess          = {peak['contractilityExcess']:.3f} "
        f"at t={peak['t']:.1f}s, HR={peak['HR']:.0f}, "
        f"systoleDev={peak['systoleDev_ms']:.1f}ms"
    )
    print(f"max systole shortening = {min(row['systoleDev_ms'] for row in rows):.1f}ms")
    print("\nRecovery decoupling locus (systole deviation as HR falls):")
    print(f"  {'HR':>6}  {'excess':>7}  {'dev_ms':>7}    (refs: ref8 142->-12, ref12 86->-58)")
    for hr_target in (180, 160, 142, 120, 100, 86, 70):
        below = [row for row in recovery if row["HR"] <= hr_target]
        if below:
            row = below[0]
            print(f"  {row['HR']:6.0f}  {row['contractilityExcess']:7.3f}  {row['systoleDev_ms']:7.1f}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--module-dir",
        type=Path,
        default=core_offline.DEFAULT_MODULE_DIR,
        help=core_offline.MODULE_DIR_HELP,
    )
    parser.add_argument("--resting", type=float, default=55.0)
    parser.add_argument("--max", type=float, default=200.0)
    parser.add_argument("--step", type=float, default=0.0333, help="step size (s); ~30 FPS")
    parser.add_argument("--sprint", type=float, default=120.0)
    parser.add_argument("--recover", type=float, default=420.0)
    parser.add_argument("--csv", type=str, default=None)
    parser.add_argument(
        "--set",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="apply a named immutable model-coefficient override; repeatable",
    )
    args = parser.parse_args()

    module = core_offline.load_binding(args.module_dir)
    coefficients, overrides = core_offline.coefficients_from_args(module, args.set)
    core_offline.print_overrides(overrides)

    warmup = 30.0
    profile = [
        (warmup, {}),
        (args.sprint, {"sprint": True}),
        (args.recover, {}),
    ]
    rows = run(
        args.resting,
        args.max,
        profile,
        args.step,
        module=module,
        coefficients=coefficients,
    )
    summarize(rows, warmup + args.sprint)

    if args.csv:
        import csv

        with open(args.csv, "w", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
            writer.writeheader()
            writer.writerows(rows)
        print(f"\nwrote {len(rows)} rows -> {args.csv}")


if __name__ == "__main__":
    main()
