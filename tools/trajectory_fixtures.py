"""Shared layer-3 fixtures: scripted full-trajectory Runtime scenarios.

Each scenario drives the compiled Runtime (shr_pybind) over a PlayerState profile with optional notify
events, at a fixed frame cadence and rhythm seed. Simulation is deterministic and only rhythm draws from
the seeded RNG, so a scenario is fully reproducible. These are test inputs, not physiological claims.
"""
from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class Segment:
    """PlayerState (as kwargs) active from ``start`` seconds until the next segment."""

    start: float
    player: dict


@dataclass(frozen=True)
class NotifyEvent:
    time: float
    method: str  # combat_entry | hit | jump | sleep | fast_travel
    duration: float = 0.0  # real seconds; used by sleep / fast_travel


@dataclass(frozen=True)
class TrajectoryScenario:
    seed: int
    resting_hr: float
    max_hr: float
    seconds: float
    fps: float
    snapshot_interval_s: float
    segments: tuple[Segment, ...]
    arrhythmia_susceptibility: float = 1.0
    events: tuple[NotifyEvent, ...] = field(default_factory=tuple)


TRAJECTORY_SCENARIOS: dict[str, TrajectoryScenario] = {
    # Classic exertion arc: rest -> sprint -> rest, exercising HR rise and recovery plus RSA.
    "rest_sprint_recovery": TrajectoryScenario(
        seed=1, resting_hr=55.0, max_hr=195.0, seconds=60.0, fps=30.0, snapshot_interval_s=2.0,
        segments=(
            Segment(start=0.0, player={}),
            Segment(start=15.0, player={"is_sprinting": True, "is_running": True}),
            Segment(start=40.0, player={}),
        ),
    ),
    # Adrenaline path: resting body, combat entry then hits drive the adrenaline-mediated HR bump.
    "combat_adrenaline": TrajectoryScenario(
        seed=2, resting_hr=58.0, max_hr=190.0, seconds=40.0, fps=30.0, snapshot_interval_s=2.0,
        segments=(Segment(start=0.0, player={}),),
        events=(
            NotifyEvent(time=5.0, method="combat_entry"),
            NotifyEvent(time=12.0, method="hit"),
            NotifyEvent(time=20.0, method="hit"),
        ),
    ),
}


SNAPSHOT_FIELDS = (
    "heart_rate",
    "fast_heart_rate",
    "slow_heart_rate",
    "exertion",
    "adrenaline",
    "contractility",
    "contractility_excess",
    "fitness",
    "effective_fitness",
    "acute_fatigue",
    "long_term_fatigue",
    "respiration_rate",
    "respiration_depth",
    "respiration_phase",
    "death_seconds",
)

BEAT_FIELDS = ("ibi", "filling_interval", "coupling_fraction", "vigor", "kind")
