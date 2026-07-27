# WI-035: Golden Check Consolidation

Status: `[NEXT]`

## Outcome and acceptance criteria

One way to verify offline goldens and one way to author them. Verification is pytest; authoring is a
single explicit capture CLI. No golden domain carries its own argument parsing, binding import, manifest
read/write, or failure formatting.

The implementation must:

- give every golden domain module the same public surface, currently reached through private names of
  inconsistent shape (`_render` in two modules, `_build` in the other two);
- make `tests/golden/test_goldens.py` depend only on that public surface;
- reduce capture to one CLI covering all four domains, replacing four `main()` functions and the
  `--capture` fan-out in `tools/check_goldens.py`;
- route every binding import through `core_offline.load_binding`, which already exists for this and which
  `tests/golden/conftest.py` already uses;
- fold `tools/check_coefficient_overrides.py` into the pytest suite, since its `verify` function is
  already a sequence of assertions over the binding and it currently needs a bespoke CI step; and
- leave the golden manifests byte-identical, proving the consolidation changed no captured value.

## Current conclusion

The offline path shares an implementation but not its plumbing. Three entry points cover one job: four
`tools/check_*_golden.py` CLIs, the pytest suite, and `tools/check_goldens.py` wrapping both. Each
checker's `main()` is roughly forty lines of the same sequence, and the copies differ in ways that carry
no meaning: the trajectory checker truncates its failure list at forty problems, the beat-renderer checker
prints all of them.

Six tools hand-roll `sys.path.insert` plus `import shr_pybind` plus the same error string, while the
shared loader written for exactly this is used only by the conftest.

The cost is visible in the documentation. `README.md` needs a full paragraph to explain which of the three
entry points to use when, and that paragraph is the readable symptom rather than the problem itself.

## Scope and non-goals

Consolidate the plumbing around the golden checks: entry points, binding loading, manifest handling, and
failure reporting. Keep the explicit separation between verification and capture, and keep capture
something a maintainer performs deliberately after an intended retune - the safety property here is that
normal verification never rewrites a manifest, and that must survive.

Do not change what any golden covers, what any manifest contains, or the fixtures that drive them.
Retuning captured values is out of scope by construction: the acceptance criterion is that the manifests
do not change. Cross-language fixture duplication is [WI-038](WI-038-fixture-single-sourcing.md).

## Dependencies

None. The offline execution boundary in [ARCHITECTURE.md](../ARCHITECTURE.md#offline-execution) already
establishes the compiled core as the single implementation and pytest as the verification path; this work
finishes conforming the tools to it.

## Next action and decision points

Settle the shared surface before moving code: whether each domain exposes plain module-level functions or
a small registered descriptor value, and whether capture becomes a flag on one tool or its own tool. The
descriptor form is the better fit if the coefficient-override contract joins the same registry, since it
is an assertion suite rather than a manifest comparison and does not fit a `build`/`diff` pair.

Then decide whether `tools/check_goldens.py` survives at all. Its `--build` convenience is real, but if
verification is plain pytest and capture is one CLI, the wrapper may have nothing left to wrap.

Update the `README.md` build section and the offline-goldens workflow in the same change, and confirm the
manifests are unchanged with a byte comparison rather than a passing verify - a passing verify after an
accidental recapture proves nothing.

## Newly observed work to split out

None.
