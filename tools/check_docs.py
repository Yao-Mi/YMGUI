#!/usr/bin/env python3
"""Check that local links in repository Markdown files resolve."""

from pathlib import Path
import re
import sys
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parent.parent
LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
SKIP_DIRS = {".git", "build"}


def markdown_files():
    for path in ROOT.rglob("*.md"):
        if not any(part in SKIP_DIRS for part in path.relative_to(ROOT).parts):
            yield path


def local_target(raw):
    target = raw.strip()
    if target.startswith("<") and ">" in target:
        target = target[1:target.index(">")]
    else:
        target = target.split(maxsplit=1)[0]
    target = unquote(target.split("#", 1)[0])
    if not target or re.match(r"^(?:https?|mailto|data):", target):
        return None
    return target


def main():
    errors = []
    checked = 0
    for doc in markdown_files():
        text = doc.read_text(encoding="utf-8")
        for match in LINK_RE.finditer(text):
            target = local_target(match.group(1))
            if target is None:
                continue
            checked += 1
            resolved = (doc.parent / target).resolve()
            if not resolved.exists():
                line = text.count("\n", 0, match.start()) + 1
                errors.append(f"{doc.relative_to(ROOT)}:{line}: missing {target}")

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"Markdown links: OK ({checked} local links)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
