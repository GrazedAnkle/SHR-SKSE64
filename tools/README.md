# Tools index

Every module in `tools/` is listed here, grouped by what breaking it costs. `check_docs.py` fails if a
`tools/*.py` file is missing from this index, so the grouping cannot silently drift as tools are added.

## How the Python side is organized

`pyproject.toml` declares `tools/` as a flat import root - not a package, and not a distribution. Modules
import each other by plain name (`import shrlib`) and every tool also runs directly as a script
(`python tools/<name>.py`, which is how they are documented and invoked). Three consequences are worth
knowing before changing that file:

- adding subdirectories under `tools/` would break direct script invocation, the flat import root, and the
  script references this file's checker resolves;
- the repository root is deliberately not an import root. With both roots declared, `shrlib` and
  `tools.shrlib` are each importable and can be live at once, giving one file two module objects; and
- the configuration carries `[tool.*]` tables only. Nothing consumes these tools from outside the
  repository, so a `[project]` table would add a build and versioning surface for no consumer.

Test paths and the import root are declared once there, so `pytest` at the repository root needs no
arguments. `[tool.ruff]` sits beside them; run `ruff format` before pushing, or let CI name what differs.
[DOCUMENTATION_CONVENTIONS.md](../docs/DOCUMENTATION_CONVENTIONS.md#source-formatting) owns formatting, and
[README.md](../README.md) owns setup and the compiled binding's build and publish steps.

## Load-bearing

Breaking one of these breaks a gate, a published artifact, or a documented number.

### Compiled-core binding and its offline clients

Thin clients over the compiled `shr_core` binding; [ARCHITECTURE.md](../docs/ARCHITECTURE.md#offline-execution)
owns the boundary they observe.

| Module | Purpose | Covered by |
|---|---|---|
| `core_offline.py` | Shared plumbing: binding resolution, provenance, coefficient overrides, render entry points | `tests/test_offline_clients.py` |
| `rhythm_offline.py` | Steady-state rhythm client for beat-to-beat analysis | `tests/test_offline_clients.py`, `tests/test_estimators.py` |
| `sim_offline.py` | Observes a physiology trajectory | `tests/test_offline_clients.py` |
| `audition_core.py` | Builds a stereo audition reel through the core renderer | `tests/test_audition_core.py` |
| `legacy_s1.py` | Retired late-S1 counterfactual transforms, kept so removed effects stay auditable | `tests/test_legacy_s1.py` |
| `build_pybind.py` | Configures and builds the binding in a plugin-free tree (CI, or no CommonLibSSE submodule) | - |

### Golden domains and their fixtures

The primary acceptance gate for physiology, rhythm, and DSP changes. Verification is `pytest`; authoring
is `capture_goldens.py`, and the split is deliberate.

| Module | Purpose | Covered by |
|---|---|---|
| `golden_registry.py` | The set of domains and the surface each exposes | `tests/golden/test_goldens.py` |
| `golden_source_conditioning.py` | Source-conditioning domain | `tests/golden/test_goldens.py` |
| `golden_beat_render.py` | Beat-renderer domain | `tests/golden/test_goldens.py` |
| `golden_rhythm_mapping.py` | Rhythm and acoustic-mapping domain | `tests/golden/test_goldens.py` |
| `golden_trajectory.py` | Full-trajectory Runtime domain | `tests/golden/test_goldens.py` |
| `beat_render_fixtures.py` | Shared beat-render fixture specs and stage names | via the domains |
| `rhythm_mapping_fixtures.py` | Shared rhythm scenarios and mapping cases | via the domains |
| `trajectory_fixtures.py` | Shared scripted full-trajectory scenarios | via the domains |
| `capture_goldens.py` | Authors the manifests; only ever writes | - |

### Checkers

Run by `.github/workflows/docs.yml`. Stdlib-only, so they need no virtual environment.

| Module | Purpose |
|---|---|
| `check_docs.py` | Documentation cross-references, anchors, symbols, script references, and this index |
| `check_constants.py` | Constant provenance tags and exact model-registry coverage |
| `check_data_citations.py` | Documentation figures and synthesis calibrations against `measurements.json` |
| `check_layering.py` | Core/adapter/plugin include direction, include qualification, and CMake list coverage |
| `check_menu.py` | Mod Configuration Menu layout and defaults against the settings registry |

### Reference measurement and annotation

Produces `docs/references/measurements.json`, which the documentation cites and `check_data_citations.py`
guards, so a change here can move a published number.

| Module | Purpose | Covered by |
|---|---|---|
| `shrlib.py` | Shared analysis library: annotation and timestamp parsing plus the acoustic metrics | `tests/test_metrics.py` |
| `ref_analyze.py` | Measures the annotated reference recordings | `tests/test_reference_lineage.py` |
| `reference_states.py` | Loads and validates the hand-authored state ledger | `tests/test_reference_states.py` |
| `auto_annotate.py` | Detects candidate S1/S2 landmarks and clean spans for review | `tests/test_auto_annotate.py` |
| `audacity_labels.py` | Converts between annotations and Audacity label tracks | - |

## Exploratory

One-off measurement and review scripts, kept so a past finding stays reproducible. Nothing imports them
and no gate runs them; expect to read them before trusting them, and expect the question they answered to
be settled already.

| Module | Answered |
|---|---|
| `measure_clap.py` | S2 valve-clap brightness and S1 envelope timing from references |
| `measure_source_candidates.py` | Which recording to use as the synthesis source sample |
| `plot_metric_anchors.py` | A reproducible temporal-anchor review view for one annotated lobe |
| `audit_s1_tail.py` | Late-S1 morphology, with a paired blind audition package |
