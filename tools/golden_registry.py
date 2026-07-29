"""The set of offline golden domains, and the surface each one exposes.

Adding a domain here adds it to both the pytest cases and the capture tool. Each domain module exposes
exactly this surface, and nothing else is required of it:

    ID: str                                  # the domain's stable name, used as the pytest case id
    MANIFEST: Path                           # the committed manifest this domain owns
    def build(module) -> dict                # drive the binding, return a fresh manifest
    def diff(expected, actual) -> list[str]  # one human-readable line per divergence, empty if equal
    def summary(manifest) -> list[str]       # what was captured, reported at capture time

``build`` receives the loaded binding rather than importing one, so no domain module resolves a build
tree or names ``shr_pybind``: the caller decides which binding is under test.

ARCHITECTURE.md (offline execution) owns the verify/author split these domains are consumed through.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import golden_beat_render  # noqa: E402
import golden_rhythm_mapping  # noqa: E402
import golden_source_conditioning  # noqa: E402
import golden_trajectory  # noqa: E402

# Ordered along the offline pipeline: source conditioning -> beat rendering -> rhythm/mapping -> trajectory.
DOMAINS = (
    golden_source_conditioning,
    golden_beat_render,
    golden_rhythm_mapping,
    golden_trajectory,
)

# A shifted trajectory diverges on every sampled field of every frame, so divergence lists are unbounded
# in principle. The first failures identify the change; the rest repeat it.
MAX_REPORTED_PROBLEMS = 40


def by_id() -> dict[str, object]:
    """The registry keyed by domain ID, for selecting domains by name on a command line."""
    return {domain.ID: domain for domain in DOMAINS}


def format_problems(problems: list[str]) -> str:
    """Render a divergence list for a failure message, capped at MAX_REPORTED_PROBLEMS."""
    shown = [f"  {problem}" for problem in problems[:MAX_REPORTED_PROBLEMS]]
    if len(problems) > MAX_REPORTED_PROBLEMS:
        shown.append(f"  ... and {len(problems) - MAX_REPORTED_PROBLEMS} more")
    return "\n".join(shown)
