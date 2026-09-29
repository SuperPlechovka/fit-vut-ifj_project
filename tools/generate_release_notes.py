#!/usr/bin/env python3
# AI prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
"""Generate GitHub notes against the published version recorded in VERSION."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SEMVER = r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"


def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True)


def generate(version, repository, head):
    current = (ROOT / "VERSION").read_text().strip()
    if not all(re.fullmatch(SEMVER, v) for v in (current, version)):
        raise ValueError("VERSION and requested version must be stable MAJOR.MINOR.PATCH")
    if tuple(map(int, version.split('.'))) <= tuple(map(int, current.split('.'))):
        raise ValueError("New version must be greater than VERSION")

    pages = json.loads(run("gh", "api", "--paginate", "--slurp",
                           f"repos/{repository}/releases?per_page=100"))
    releases = [release for page in pages for release in page]
    previous = f"v{current}"
    published = any(r["tag_name"] == previous and not r["draft"]
                    and not r["prerelease"] for r in releases)
    args = ["gh", "api", "--method", "POST",
            f"repos/{repository}/releases/generate-notes",
            "-f", f"tag_name=v{version}", "-f", f"target_commitish={head}"]
    if published:
        # A tag on another branch is not a valid baseline for this main release.
        tag = f"refs/tags/{previous}"
        run("git", "rev-parse", "--verify", f"{tag}^{{commit}}")
        run("git", "merge-base", "--is-ancestor", tag, head)
        args.extend(["-f", f"previous_tag_name={previous}"])
        print(f"Release notes range: {previous} -> {head}", file=sys.stderr)
    elif releases or (ROOT / ".github/release-notes" / f"{previous}.md").exists():
        raise ValueError(
            f"Previous stable release {previous} is not published. Finish/recover "
            "that release first; refusing to regenerate notes from older history.")
    else:
        # Bootstrap of a fresh template: its inherited VERSION has never shipped.
        print("First release: no prior published release or prepared notes.", file=sys.stderr)

    notes = json.loads(run(*args))
    if not isinstance(notes.get("body"), str):
        raise ValueError("GitHub did not return a release notes body")
    return notes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        notes = generate(args.version, os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_SHA"])
        args.output.write_text(json.dumps(notes, ensure_ascii=False) + "\n", encoding="utf-8")
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
