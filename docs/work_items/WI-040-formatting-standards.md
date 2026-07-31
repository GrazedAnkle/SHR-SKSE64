# WI-040: Automated Formatting Standards

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Code style is enforced by a tool rather than by discipline, on both the C++ and Python sides, without
losing the readability properties the current hand-maintained style provides.

The implementation must:

- adopt a formatter configuration for each language that reproduces the existing style closely enough that
  the reformatting diff is mechanical rather than a restyling;
- preserve the aligned declaration columns used throughout the C++ sources, which are load-bearing for
  reading the constant and settings tables;
- apply the reformatting as its own isolated commit, so that history remains bisectable and the change is
  reviewable as "formatting only"; and
- run the formatter in CI as a check rather than as a mutation.

## Current conclusion

There is no `.clang-format` for roughly eight thousand lines of C++ and no formatter or linter
configuration for roughly seven thousand lines of Python. Both are currently consistent, which is the
argument for capturing that consistency now while the tool has an unambiguous target to reproduce, rather
than after it has decayed and the configuration becomes a series of judgment calls.

The C++ style is the constraint that makes this non-trivial. The sources use consecutive-assignment and
consecutive-declaration alignment extensively - in the constant tables, the settings structures, and the
member declarations - and that alignment is genuinely useful for reading tabular data. A default
configuration would strip it, producing an enormous diff that also makes those files harder to read. The
configuration has to be fitted to the existing style rather than chosen from a preset.

## Scope and non-goals

Fit and adopt the configurations, reformat once, and add the CI check.

Do not treat this as an opportunity to change the style. If the fitted configuration and the existing code
disagree somewhere, the default resolution is to configure the tool to match the code. Reserve genuine
style changes for cases where the existing style cannot be expressed by the tool at all, and record those
explicitly rather than absorbing them into the reformatting commit.

Do not adopt a Python linter's full default rule set at the same time as its formatter. Formatting is
mechanical and safe; lint rules encode opinions about correctness and will produce findings that need
individual judgment. If a linter is adopted, start from a deliberately small rule set.

Do not add pre-commit hooks or editor-level enforcement as part of this. A CI check is sufficient to hold
the property, and local enforcement is a workflow preference that should be chosen separately.

## Dependencies

None blocking. The core/adapter/plugin relocation that would have collided with a reformatting commit has
landed, so this no longer needs sequencing behind it.

`pyproject.toml` already exists and carries `[tool.*]` tables only, so the Python half of this item adds a
formatter and linter table there rather than introducing the file. It is also where the supported
interpreter would move: `[tool.shr] requires-python` holds it now because no linting table existed to
declare a target version.

The repository currently mixes CRLF and LF line endings between files, which is why nearly any edit draws a
git conversion warning. Deciding whether a formatter or `.gitattributes` owns that - and whether
normalizing is one commit or none - belongs to this item.

## Next action and decision points

Fit the C++ configuration against the existing sources before committing to the item at all, and measure
the resulting diff. If the alignment properties can be reproduced, the diff should be close to empty and
this is nearly free; if they cannot, the honest choice is between a large restyling and not adopting a C++
formatter, and that is a decision to make with the measurement in hand rather than in advance.

Then decide whether the CI check gates or merely reports. Gating is the point of the item, but it is worth
confirming the formatter version is pinned first - an unpinned formatter that changes its output across a
release turns a green branch red without a code change, which is the failure mode that makes teams
disable these checks.

## Newly observed work to split out

None.
