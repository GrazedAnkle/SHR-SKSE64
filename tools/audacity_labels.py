#!/usr/bin/env python3
"""Convert between S1/S2 annotations and Audacity label tracks.

Exports contain an S1 and S2 region for each beat, per-beat notes on S1, and
zero-length `# <group>` labels. Times are absolute recording seconds.

Examples:
  python tools/audacity_labels.py export docs/references/candidates/11.txt
  # ...review in Audacity, Export > Labels to 11.labels.txt...
  python tools/audacity_labels.py import docs/references/candidates/11.labels.txt -o reviewed_11.txt
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import shrlib

# Optional trailing note on a beat line.
_NOTE_RE = re.compile(r"\(([^)]*)\)\s*$")


def parse_annot_lines(path: str | Path) -> list[tuple[str, list]]:
    """Parse an annotation file into ordered groups while retaining per-beat notes.

    Returns [(group_label, [(s1a, s1b, s2a, s2b, note), ...]), ...].
    """
    groups: list[tuple[str, list]] = []
    label = "default"
    cur: list | None = None
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("S1="):
            m1, m2 = shrlib._S1_RE.search(line), shrlib._S2_RE.search(line)
            if not (m1 and m2):
                continue
            note = _NOTE_RE.search(line)
            beat = (shrlib.mmss(m1.group(1)), shrlib.mmss(m1.group(2)),
                    shrlib.mmss(m2.group(1)), shrlib.mmss(m2.group(2)),
                    note.group(1) if note else "")
            if cur is None:
                cur = []
                groups.append((label, cur))
            cur.append(beat)
        else:
            label = line.rstrip(":").strip()
            cur = None
    return [(lab, beats) for lab, beats in groups if beats]


def _lbl(a: float, b: float, text: str) -> str:
    return f"{a:.6f}\t{b:.6f}\t{text}"


def export_labels(annot: str | Path, out: Path | None = None) -> int:
    """Write an Audacity label track from an annotation/candidate file."""
    out = Path(out) if out else Path(annot).with_suffix(".labels.txt")
    lines: list[str] = []
    for label, beats in parse_annot_lines(annot):
        lines.append(_lbl(beats[0][0], beats[0][0], f"# {label}"))
        for s1a, s1b, s2a, s2b, note in beats:
            lines.append(_lbl(s1a, s1b, f"S1 ({note})" if note else "S1"))
            lines.append(_lbl(s2a, s2b, "S2"))
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {out}  ({sum(len(b) for _, b in parse_annot_lines(annot))} beats). "
          f"In Audacity: File > Import > Labels.")
    return 0


def import_labels(labels: str | Path, out: Path | None = None) -> int:
    """Write an annotation file from an Audacity label track.

    Zero-length `#` labels start groups. S1 and S2 regions pair into beats.
    """
    out = Path(out) if out else Path(labels).with_suffix(".annot.txt")
    rows = []
    for raw in Path(labels).read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        parts = raw.split("\t")
        if len(parts) < 3:
            continue
        rows.append((float(parts[0]), float(parts[1]), parts[2].strip()))
    rows.sort(key=lambda r: r[0])

    groups: list[tuple[str, list]] = [("default", [])]
    pending = None
    for a, b, text in rows:
        if a == b and text.startswith("#"):
            groups.append((text.lstrip("# ").strip(), []))
        elif text.startswith("S1"):
            pending = (a, b)
        elif text.startswith("S2"):
            if pending is None:
                print(f"  warn: S2 at {a:.3f}s with no preceding S1, skipping")
                continue
            groups[-1][1].append((pending[0], pending[1], a, b))
            pending = None

    f = shrlib.fmt_mmss
    lines: list[str] = []
    for label, beats in groups:
        if not beats:
            continue
        lines.append(f"{label}:")
        for s1a, s1b, s2a, s2b in beats:
            lines.append(f"S1={f(s1a)}-{f(s1b)} S2={f(s2a)}-{f(s2b)}")
        lines.append("")
    out.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
    n = sum(len(b) for _, b in groups)
    print(f"wrote {out}  ({n} beats). Review, then promote to docs/references/timestamps/.")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["export", "import"], help="direction of conversion")
    ap.add_argument("file", help="annotation file (export) or Audacity labels file (import)")
    ap.add_argument("-o", "--out", type=Path, help="output path (default: sibling .labels.txt / .annot.txt)")
    a = ap.parse_args()
    return export_labels(a.file, a.out) if a.mode == "export" else import_labels(a.file, a.out)


if __name__ == "__main__":
    sys.exit(main())
