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

## Evaluations

`evals/<skill>.json` holds at least three scenarios per skill that test whether
Codex and Claude Code load the right skills and keep their boundary rules. Each
scenario uses the evaluation structure of Anthropic's skill-authoring guide plus
a stable `id` and optional `excluded_skills`:

| Field | Meaning |
| --- | --- |
| `id` | Stable name in reports, lowercase words joined by hyphens |
| `skills` | Skills that must load; the minimal set the routing in `AGENTS.md` requires |
| `excluded_skills` | Optional; skills whose loading means misrouting |
| `query` | The request, phrased the way users actually ask, often with a tempting wrong approach |
| `files` | Optional repository files attached to the query |
| `expected_behavior` | One observable behavior per item, graded independently |

The file's own skill must appear in `skills` or `excluded_skills`. Keep
behaviors valid when repository state moves on, for example "when no roadmap
task is ready, asks the user" instead of naming today's task status.

```powershell
python Build/Tools/AgentSkills/skill_evals.py check
python Build/Tools/AgentSkills/skill_evals.py run --tool codex
python Build/Tools/AgentSkills/skill_evals.py run --tool claude --model haiku --skill unreal-gasp-expert
python Build/Tools/AgentSkills/skill_evals.py grade Saved/AgentSkillEvals/<run>
python Build/Tools/AgentSkills/skill_evals.py report Saved/AgentSkillEvals/<run> Saved/AgentSkillEvals/<run>
```

`check` runs in CI and validates the fields, the skill names, the minimum count,
and, like `sync.py --check`, the repository paths and identifiers in code spans.

`run` starts one fresh headless session per scenario from the repository root:
Claude Code with `-p --permission-mode plan --no-session-persistence`, Codex
with `exec --sandbox read-only --ephemeral`. Each query gets a fixed note to
work read-only and answer with a plan. The tool's user and project
configuration still apply, as in real use. A skill counts as loaded when Claude
calls the Skill tool or when a tool call reads `skills/<name>/SKILL.md`, which is
how Codex loads skills. Only tool inputs are scanned, so listings and search
results never count. Other skill files read, such as references, are recorded
too. Use `--id` or `--skill` to narrow a run, `--model` to compare models,
`--jobs` for parallel sessions, and `--exe` when the CLI is not on `PATH`. The
CLI must be logged in for headless use, so `claude -p` or `codex exec` has to
work on its own.

`grade` sends each answer and the condensed tool calls to a judge session
without tools, started outside the repository: Claude by default, `--judge
codex` otherwise. The judge marks each `expected_behavior` item as met with
short evidence; a planned step counts, since sessions are read-only. Grading
uses a scenario's current behaviors while its query is unchanged, so an older
run can be graded again after a rubric fix; existing grades are reused unless
the behaviors changed or `--force` is given. Treat grades as evidence, not
proof, and check not-met items against the transcript before changing a skill.
`report` prints one row per scenario and run, labels the runs A, B, and so on
for before-and-after comparisons, and lists each run's totals.

Each run writes a new folder under `Saved/AgentSkillEvals/<time>-<tool>[-<model>]`
with `run.json` (tool version, model, commit), the raw transcript, stderr, the
result, and the grade per scenario. Like all of `Saved/`, it stays local. Exit
codes: 0 all passed, 1 a trigger, session, or behavior failed, 2 setup error.

Run the affected scenarios with at least one model per tool before and after
changing a skill description, a boundary rule, or the routing in `AGENTS.md`.
Record the `report` tables and the deviations that led to the change in the pull
request.
