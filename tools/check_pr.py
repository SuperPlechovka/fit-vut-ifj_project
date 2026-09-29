#!/usr/bin/env python3
"""IFJ26: PR description validator adapted from the AI-assisted generic template.
Prompt: tady jsou materiály k projektu z IFJ. chci aby jsi použil šablonu a upravil ji tak, aby byla ready na tento projekt. nechci v ní nic zbytečně navíc
"""

import json
import os
from pathlib import Path
import re
import sys

SECTIONS = ("Proč", "Změny", "Ověření", "Dokumentace", "Související issue")


def validate(body):
    clean = re.sub(r"<!--.*?-->", "", body, flags=re.S)
    errors = []
    for title in SECTIONS:
        match = re.search(rf"^## {re.escape(title)}\s*\n(.*?)(?=^## |\Z)", clean, re.M | re.S)
        section = re.sub(r"^- \[[ xX]\].*$", "", match.group(1), flags=re.M) if match else ''
        if len(section.strip().strip('-').strip()) < 5:
            errors.append(f"Vyplň sekci: {title}")
    if not re.search(r"^- \[[xX]\]", clean, re.M):
        errors.append("PR musí obsahovat vyplněný checklist.")
    if re.search(r"^- \[ \]", clean, re.M):
        errors.append("Dokonči checklist nebo vysvětli neaplikovatelné položky a označ je hotové.")
    return errors


if __name__ == "__main__":
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    if "pull_request" not in event:
        print("Not a PR event; description check not applicable")
        sys.exit(0)
    pr = event["pull_request"]
    if pr.get("user", {}).get("login") == "dependabot[bot]":
        print("Dependabot uses its generated description; application tests still apply")
        sys.exit(0)
    if pr.get("draft"):
        print("Draft PR; description will be required when ready for review")
        sys.exit(0)
    errors = validate(pr.get("body") or "")
    for error in errors:
        print(error)
    sys.exit(bool(errors))
