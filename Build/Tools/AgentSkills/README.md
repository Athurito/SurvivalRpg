# Shared agent skills

The canonical skills live in `.agents/skills/<name>/`, where Codex discovers them.
Claude Code only discovers `.claude/skills/<name>/`, so `sync.py` writes a
generated full copy of every skill there, including its references. Copies
instead of pointer stubs let Claude load the real skill body on invocation and
keep references one level deep. Codex-only `agents/` metadata is not copied, and
each copied Markdown file carries a generated marker. Symlinks are not used
because Windows clones check them out as plain text unless `core.symlinks` is
enabled.

Edit only the canonical files, then run from the repository root:

```powershell
python Build/Tools/AgentSkills/sync.py
python Build/Tools/AgentSkills/sync.py --check
python -m unittest discover -b -s Build/Tools/AgentSkills -p "test_*.py"
```

CI runs the check and the tests on every pull request. `--check` changes
nothing and exits with 1 when:

- a copy is missing, stale, or has extra files, or a generated copy has no
  canonical skill left
- a skill's frontmatter is invalid: the name must match the folder and use
  lowercase words joined by hyphens; the description is at most 1024 characters,
  the stricter of the two tools' limits
- a canonical skill, `AGENTS.md`, or `CLAUDE.md` names a repository path, a
  `/Game/...` asset path, or a `URpg`/`ARpg`/`FRpg`/`ERpg`/`IRpg` identifier that
  does not exist, or contains a broken relative link
- that Markdown refers to a skill as `$name` instead of its plain name
  (`agents/openai.yaml` may keep the Codex syntax)
- an `AGENTS.md` has no sibling `CLAUDE.md` importing it with `@AGENTS.md`; once
  a `CLAUDE.md` exists, Claude Code reads only `CLAUDE.md` files

Only code spans and relative links are checked; globs and placeholders such as
`<name>` or `*` are skipped. Folders under `.claude/skills/` without the
generated marker are left alone, so Claude-only skills can live there under a
different name.
