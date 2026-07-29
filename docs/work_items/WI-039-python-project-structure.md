# WI-039: Python Project Structure

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

The Python side has a declared project configuration. Modules import each other because the project says
how they resolve, not because each file repairs `sys.path` on the way in.

The implementation must:

- add a project configuration file declaring test paths, import roots, and the supported interpreter;
- remove the import-path manipulation it makes unnecessary, currently nineteen `sys.path.insert` calls
  across `tools/` and `tests/`;
- keep a bare `pytest` invocation at the repository root working, including the golden suite's
  `--module-dir` option, which is currently registered from a non-root conftest;
- keep every tool runnable directly as a script, since that is how they are invoked and documented; and
- give `tools/` a structure or index that distinguishes load-bearing modules from exploratory ones.

## Current conclusion

There is no `pyproject.toml`, `pytest.ini`, or `setup.cfg` for roughly seven thousand lines of Python.
Each module repairs its own import path, so the same three-line prelude appears nineteen times, and
`tools/` is not importable as a package by any means other than that prelude.

The whole suite runs from the repository root - seventy-five tests, including the golden cases - so this
is a maintenance and legibility problem rather than a broken state. Shared plumbing sits in
`tests/conftest.py`, which must stay at that level because pytest honors argument registration only from
initial conftests. That constraint is currently satisfied by knowing it, which is the general shape of the
problem: import resolution is established by convention at nineteen call sites rather than declared once.

`tools/` also conflates four populations with nothing to distinguish them: a shared analysis library
(`shrlib.py`), thin offline clients, the checkers, and one-off measurement scripts such as
`measure_clap.py`, `measure_source_candidates.py`, and `plot_metric_anchors.py`. A reader cannot tell
which are load-bearing. `shrlib.py` has the same problem internally at roughly seven hundred lines: the
annotation and timestamp parsing it contains is a different concern from the acoustic metrics, and the two
halves share nothing but the file.

## Scope and non-goals

Declare the project configuration, remove the import workarounds it obviates, and make the tool
populations legible. Prefer configuration that removes code over configuration that adds structure - the
`sys.path` prelude count is the honest measure of success here.

Do not package or publish the tools as an installable distribution. Nothing consumes them from outside the
repository, and an installable package would add a build and versioning surface for no consumer.

Do not restructure `shrlib.py` as part of the mechanical work. Splitting its parsing half from its metrics
half is defensible, but it is imported widely and the churn would obscure the rest of this change. Record
it as a candidate for whenever that file is next opened substantially.

Formatting and linting are [WI-040](WI-040-formatting-standards.md), not this item, even though both would
be configured in the same file.

## Dependencies

None. The redundant golden-check entry points whose imports would otherwise have been migrated here and
then deleted are already gone. The remaining `sys.path` repair in the offline path is the `tools/`
insertion each golden domain module, `tools/golden_registry.py`, and `tools/capture_goldens.py` perform to
reach their fixtures and each other, which this item owns.

## Next action and decision points

Settle how the compiled binding is resolved, which is the one genuinely load-bearing decision. The golden
suite takes an explicit `--module-dir`, `core_offline` defaults to `build/pybind`, and `.vscode/settings.json`
adds the same path for the editor's benefit. A declared import root could unify these or could add a fourth
mechanism alongside the existing three; the goal is fewer, so decide which ones survive before adding
anything.

Then decide whether `tools/` gains subdirectories or only an index document. Subdirectories express the
distinction structurally but break every documented invocation path and every reference in the docs, which
`tools/check_docs.py` validates. An index costs nothing and expresses the same thing less durably.

## Newly observed work to split out

Splitting the annotation and timestamp parsing out of `shrlib.py` is deferred rather than dropped, on the
grounds of import churn rather than merit.
