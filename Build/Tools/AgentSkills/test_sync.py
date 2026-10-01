"""Offline tests for sync.py against a synthetic repository layout."""

from __future__ import annotations

from pathlib import Path
import tempfile
import unittest

import sync


SKILL = """---
name: demo-skill
description: Guides demo work. Use when testing the mirror.
---

# Demo Skill

Read [the reference](references/notes.md) and `docs/guide.md`. Uses `URpgThing` and `/Game/Demo/Map`.
"""


class SyncTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.write(".agents/skills/demo-skill/SKILL.md", SKILL)
        self.write(".agents/skills/demo-skill/references/notes.md", "# Notes\n")
        self.write(".agents/skills/demo-skill/agents/openai.yaml", "interface: {}\n")
        self.write("Source/Demo/Thing.h", "class URpgThing {};\n")
        self.write("Content/Demo/Map.umap", "")
        self.write("docs/guide.md", "# Guide\n")
        self.write("AGENTS.md", "# Agents\n")
        self.write("CLAUDE.md", "@AGENTS.md\n")
        self.mirror = self.root / ".claude/skills/demo-skill"

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write(self, relative: str, text: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")

    def test_write_mirrors_skill_without_codex_metadata(self) -> None:
        self.assertEqual(sync.run(self.root, check=False), [])
        mirrored = (self.mirror / "SKILL.md").read_text(encoding="utf-8")
        self.assertTrue(mirrored.startswith("---\nname: demo-skill\n"))
        self.assertIn(sync.GENERATED_MARKER, mirrored)
        self.assertIn("# Demo Skill", mirrored)
        self.assertTrue((self.mirror / "references/notes.md").is_file())
        self.assertFalse((self.mirror / "agents").exists())
        self.assertEqual(sync.run(self.root, check=True), [])

    def test_check_reports_missing_stale_and_extra_files(self) -> None:
        self.assertIn("missing mirror file: .claude/skills/demo-skill/SKILL.md", sync.run(self.root, check=True))
        sync.run(self.root, check=False)
        self.write(".claude/skills/demo-skill/references/notes.md", "edited copy\n")
        self.write(".claude/skills/demo-skill/stray.md", "stray\n")
        problems = sync.run(self.root, check=True)
        self.assertIn("stale mirror file: .claude/skills/demo-skill/references/notes.md", problems)
        self.assertIn("extra mirror file: .claude/skills/demo-skill/stray.md", problems)
        self.assertEqual(sync.run(self.root, check=False), [])
        self.assertFalse((self.mirror / "stray.md").exists())

    def test_orphaned_mirror_is_reported_and_removed(self) -> None:
        sync.run(self.root, check=False)
        self.write(".claude/skills/old-skill/SKILL.md", f"---\nname: old-skill\n---\n<!-- {sync.GENERATED_MARKER} -->\n")
        self.assertIn("orphaned mirror: .claude/skills/old-skill", sync.run(self.root, check=True))
        sync.run(self.root, check=False)
        self.assertFalse((self.root / ".claude/skills/old-skill").exists())

    def test_hand_written_skill_with_same_name_is_rejected(self) -> None:
        self.write(".claude/skills/demo-skill/SKILL.md", "---\nname: demo-skill\ndescription: local\n---\n")
        with self.assertRaises(sync.SkillError):
            sync.run(self.root, check=False)

    def test_hand_written_claude_only_skill_is_kept(self) -> None:
        self.write(".claude/skills/local-only/SKILL.md", "---\nname: local-only\ndescription: local\n---\n")
        self.assertEqual(sync.run(self.root, check=False), [])
        self.assertTrue((self.root / ".claude/skills/local-only/SKILL.md").is_file())

    def test_dead_references_are_reported(self) -> None:
        self.write(".agents/skills/demo-skill/SKILL.md", SKILL.replace("URpgThing", "URpgGone")
                   .replace("/Game/Demo/Map", "/Game/Demo/Gone").replace("docs/guide.md", "docs/gone.md")
                   .replace("references/notes.md", "references/gone.md"))
        problems = "\n".join(sync.run(self.root, check=False))
        self.assertIn("identifier URpgGone not found", problems)
        self.assertIn("missing asset path /Game/Demo/Gone", problems)
        self.assertIn("missing path docs/gone.md", problems)
        self.assertIn("broken link references/gone.md", problems)

    def test_placeholders_and_globs_are_not_checked(self) -> None:
        self.write("AGENTS.md", "Edit `.agents/skills/<name>/SKILL.md` and `URpgGameplayAbility_*`.\n")
        self.assertEqual(sync.run(self.root, check=False), [])

    def test_codex_skill_syntax_in_markdown_is_reported(self) -> None:
        self.write("AGENTS.md", "Use $demo-skill here.\n")
        problems = sync.run(self.root, check=False)
        self.assertEqual(problems, ["AGENTS.md:1: use `demo-skill` instead of $demo-skill"])

    def test_agents_md_needs_importing_claude_md(self) -> None:
        self.write("Source/AGENTS.md", "# Source rules\n")
        self.assertIn("Source/AGENTS.md: add a sibling CLAUDE.md starting with @AGENTS.md", sync.run(self.root, check=False))
        self.write("Source/CLAUDE.md", "@AGENTS.md\n")
        self.assertEqual(sync.run(self.root, check=False), [])

    def test_invalid_frontmatter_is_rejected(self) -> None:
        self.write(".agents/skills/demo-skill/SKILL.md", SKILL.replace("name: demo-skill", "name: Demo_Skill"))
        with self.assertRaises(sync.SkillError):
            sync.run(self.root, check=True)
        self.write(".agents/skills/demo-skill/SKILL.md", SKILL.replace("Guides demo work.", "Guides demo: work."))
        with self.assertRaises(sync.SkillError):
            sync.run(self.root, check=True)


if __name__ == "__main__":
    unittest.main()
