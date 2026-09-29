#!/usr/bin/env python3
# AI prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
"""Prepare VERSION, CHANGELOG and release notes; never commits or pushes."""

import argparse
from datetime import date
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SEMVER = r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"


def prepare(version, generated_notes):
    if not re.fullmatch(SEMVER, version):
        raise ValueError("Use stable MAJOR.MINOR.PATCH without v, e.g. 1.0.0")
    current = (ROOT / "VERSION").read_text().strip()
    if tuple(map(int, version.split('.'))) <= tuple(map(int, current.split('.'))):
        raise ValueError("New version must be greater than VERSION")
    path = ROOT / "CHANGELOG.md"
    original = path.read_text(encoding="utf-8")
    if re.search(rf"^## \[{re.escape(version)}\]", original, re.M):
        raise ValueError("This version already exists in CHANGELOG")
    match = re.search(r"^## \[Unreleased\]\s*\n(.*?)(?=^## \[|\Z)", original, re.M | re.S)
    if not match:
        raise ValueError("CHANGELOG must contain ## [Unreleased]")
    manual = re.sub(r"<!--.*?-->", "", match.group(1), flags=re.S).strip()
    # Empty category headings are not release notes.
    meaningful = re.sub(r"^### .*?$", "", manual, flags=re.M).strip()
    generated = generated_notes.strip()
    # GitHub returns H2/H3 headings; nest them beneath the H3 PR overview.
    generated = re.sub(r"^(#{1,6})(?=\s)",
                       lambda m: '#' * min(len(m[1]) + 2, 6), generated, flags=re.M)
    if not meaningful and not generated:
        raise ValueError("Add Unreleased changes or provide generated release notes")
    sections = []
    if meaningful:
        sections.append(manual)
    if generated:
        sections.append("### Přehled pull requestů\n\n" + generated)
    body = "\n\n".join(sections)
    blank = "## [Unreleased]\n\n<!-- Změny určené pro příští vydání. -->\n\n"
    released = f"## [{version}] - {date.today().isoformat()}\n\n{body}\n\n"
    path.write_text(original[:match.start()] + blank + released + original[match.end():], encoding="utf-8")
    (ROOT / "VERSION").write_text(version + "\n", encoding="utf-8")
    notes = ROOT / ".github/release-notes" / f"v{version}.md"
    notes.parent.mkdir(exist_ok=True)
    notes.write_text(body + "\n", encoding="utf-8")
    print(f"Prepared v{version}; previous changelog sections preserved")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--generated-json", type=Path)
    args = parser.parse_args()
    try:
        notes = json.loads(args.generated_json.read_text())["body"] if args.generated_json else ""
        prepare(args.version, notes)
    except (ValueError, OSError, KeyError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
