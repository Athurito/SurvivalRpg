"""Claude Code hooks that keep the generated skill copies from drifting.

`pre` runs before Edit and Write calls and blocks changes to a generated copy under
`.claude/skills/`, which the next `sync.py` run would overwrite, naming the canonical file instead.
`post` runs after them and mirrors the skills again when the edited file lies under `.agents/skills/`.

Both read the hook event as JSON on stdin and locate the repository from the edited file, so they also
work in worktrees. Exit code 2 blocks the call (`pre`) or shows stderr to Claude after it (`post`);
every other outcome lets the call proceed. `sync.py --check` remains the full verification.
"""

from __future__ import annotations

import argparse
import contextlib
import io
import json
from pathlib import Path
import sys

import sync


BLOCK = 2


def edited_path(event: dict) -> Path | None:
    value = (event.get("tool_input") or {}).get("file_path")
    if not value:
        return None
    path = Path(value)
    if not path.is_absolute():
        path = Path(event.get("cwd") or ".") / path
    return path.resolve()


def locate(path: Path, folder: str) -> tuple[Path, str] | None:
    """Return the repository root and skill name when `path` lies in `<root>/<folder>/skills/<name>/`."""
    for skill_dir in path.parents:
        skills = skill_dir.parent
        if skills.name.lower() == "skills" and skills.parent.name.lower() == folder:
            return skills.parent.parent, skill_dir.name
    return None


def pre(event: dict) -> int:
    path = edited_path(event)
    found = locate(path, ".claude") if path else None
    if not found:
        return 0
    root, name = found
    copy = root / ".claude" / "skills" / name
    if not sync.is_generated(copy):
        return 0
    canonical = root / ".agents" / "skills" / name / path.relative_to(copy)
    print(f"{sync.rel(root, path)} is a generated copy that sync.py overwrites. Edit "
          f"{sync.rel(root, canonical)} instead, then run python Build/Tools/AgentSkills/sync.py.", file=sys.stderr)
    return BLOCK


def post(event: dict) -> int:
    path = edited_path(event)
    found = locate(path, ".agents") if path else None
    if not found:
        return 0
    root = found[0]
    output = io.StringIO()
    try:
        with contextlib.redirect_stdout(output):
            sync.write(root, sync.discover(root))
    except sync.SkillError as error:
        print(f"sync.py could not mirror the skills: {error}", file=sys.stderr)
        return BLOCK
    written = output.getvalue().split("\n")
    changes = [line for line in written if line]
    if changes:
        context = "sync.py mirrored the change: " + "; ".join(changes)
        json.dump({"hookSpecificOutput": {"hookEventName": "PostToolUse", "additionalContext": context}}, sys.stdout)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("event", choices=("pre", "post"))
    args = parser.parse_args(argv)
    try:
        event = json.loads(sys.stdin.buffer.read().decode("utf-8") or "{}")
    except ValueError:
        return 0
    return pre(event) if args.event == "pre" else post(event)


if __name__ == "__main__":
    sys.exit(main())
