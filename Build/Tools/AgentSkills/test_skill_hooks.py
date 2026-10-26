"""Offline tests for skill_hooks.py against a synthetic repository layout."""

from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

import skill_hooks
import sync


SKILL = """---
name: demo-skill
description: Guides demo work. Use when testing the hooks.
---

# Demo Skill
"""


class SkillHookTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.write(".agents/skills/demo-skill/SKILL.md", SKILL)
        self.write(".agents/skills/demo-skill/references/notes.md", "# Notes\n")
        sync.write(self.root, sync.discover(self.root))

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write(self, relative: str, text: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")

    def call(self, hook, relative: str, cwd: Path | None = None) -> tuple[int, str, str]:
        file_path = relative if cwd else str(self.root / relative)
        event = {"tool_name": "Edit", "tool_input": {"file_path": file_path}, "cwd": str(cwd or self.root)}
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = hook(event)
        return code, stdout.getvalue(), stderr.getvalue()

    def test_pre_blocks_generated_copy_and_names_canonical_file(self) -> None:
        code, _, stderr = self.call(skill_hooks.pre, ".claude/skills/demo-skill/references/notes.md")
        self.assertEqual(code, skill_hooks.BLOCK)
        self.assertIn(".agents/skills/demo-skill/references/notes.md", stderr)

    def test_pre_resolves_relative_paths_against_cwd(self) -> None:
        code, _, _ = self.call(skill_hooks.pre, "skills/demo-skill/SKILL.md", cwd=self.root / ".claude")
        self.assertEqual(code, skill_hooks.BLOCK)

    def test_pre_allows_canonical_claude_only_and_unrelated_files(self) -> None:
        self.write(".claude/skills/local-only/SKILL.md", "---\nname: local-only\ndescription: local\n---\n")
        for relative in (".agents/skills/demo-skill/SKILL.md", ".claude/skills/local-only/SKILL.md",
                         ".claude/settings.json", "docs/guide.md"):
            with self.subTest(relative=relative):
                self.assertEqual(self.call(skill_hooks.pre, relative)[0], 0)

    def test_post_mirrors_canonical_edit(self) -> None:
        self.write(".agents/skills/demo-skill/references/notes.md", "# Notes\n\nNew line.\n")
        code, stdout, _ = self.call(skill_hooks.post, ".agents/skills/demo-skill/references/notes.md")
        self.assertEqual(code, 0)
        self.assertIn("New line.", (self.root / ".claude/skills/demo-skill/references/notes.md").read_text(encoding="utf-8"))
        context = json.loads(stdout)["hookSpecificOutput"]["additionalContext"]
        self.assertIn(".claude/skills/demo-skill/references/notes.md", context)

    def test_post_is_silent_without_changes_and_ignores_other_files(self) -> None:
        self.assertEqual(self.call(skill_hooks.post, ".agents/skills/demo-skill/SKILL.md"), (0, "", ""))
        self.assertEqual(self.call(skill_hooks.post, "docs/guide.md"), (0, "", ""))

    def test_post_reports_invalid_frontmatter(self) -> None:
        self.write(".agents/skills/demo-skill/SKILL.md", SKILL.replace("name: demo-skill", "name: Demo_Skill"))
        code, _, stderr = self.call(skill_hooks.post, ".agents/skills/demo-skill/SKILL.md")
        self.assertEqual(code, skill_hooks.BLOCK)
        self.assertIn("could not mirror", stderr)


if __name__ == "__main__":
    unittest.main()
