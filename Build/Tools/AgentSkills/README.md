# Shared agent skills

The canonical skills live in `.agents/skills/<name>/`, where Codex discovers them.
Claude Code only discovers `.claude/skills/<name>/SKILL.md`, so `sync.py`
generates one small entry point per skill there. Each entry point repeats the
canonical `name` and `description` (the trigger metadata) and tells Claude to
read the canonical `SKILL.md`. Symlinks are not used because Windows clones
check them out as plain text unless `core.symlinks` is enabled.

Edit only the canonical files. After adding, renaming, removing, or changing the
frontmatter of a skill, run from the repository root:

```powershell
python Build/Tools/AgentSkills/sync.py
python Build/Tools/AgentSkills/sync.py --check
```

`--check` changes nothing and exits with 1 when an entry point is missing,
stale, or orphaned, when a skill's frontmatter is invalid (name must match the
folder and use lowercase words joined by hyphens; description at most 1024
characters, the stricter of the two tools' limits), or when a skill,
`AGENTS.md`, or `CLAUDE.md` names a repository path that does not exist, or
when that Markdown refers to a skill as `$name` instead of its plain name
(`agents/openai.yaml` may keep the Codex syntax). Only markdown links and
backticked paths that start with a known top-level folder are checked; globs
and placeholders are skipped.

Removing a canonical skill also removes its generated entry point. Folders under
`.claude/skills/` without the generated marker are left alone, so Claude-only
skills can live there.
