@AGENTS.md

## Claude Code

- `.claude/skills/*` are generated copies of the canonical skills in `.agents/skills/*`. Do not edit them directly; follow "Agent skills" in AGENTS.md. Hooks in `.claude/settings.json` (`Build/Tools/AgentSkills/skill_hooks.py`) block edits of the copies and mirror every edit under `.agents/skills/`; still run `sync.py --check` before committing.
- Claude's auto-memory is private to Claude. Put reusable project knowledge in the matching skill reference instead, so Codex gets it too.
- The Unreal MCP server `unreal-mcp` is configured in `.mcp.json`. Approve it once when Claude Code asks. It only responds while the Unreal Editor runs with the MCP plugin enabled. A session that started before the editor stays without its tools until `unreal-mcp` is reconnected with `/mcp`.
- The JetBrains Rider MCP server `rider` is also configured in `.mcp.json`. It only responds while Rider runs with its MCP server enabled, on the port Rider shows there. Approve it once like `unreal-mcp`; a session started before Rider stays without its tools. When to use it instead of `unreal-mcp` or `ue.py` is in "Rider MCP" of `.agents/skills/unreal-lyra-expert/references/editor-session-and-pie.md`.
- `.claude/settings.json` enables Epic's official plugin `unreal-engine-skills-for-claude-code`; Claude Code offers to install it when you trust the folder. Its skills are generic and the routing in AGENTS.md still decides:
  - The `unreal-mcp` skill adds the generic tool workflow and safety rules. For asset authoring, also load `unreal-lyra-expert` and follow its MCP reference, which wins where they differ.
  - `create-toolset` applies to new or changed tools in `Build/Tools/Unreal/`, within the capability-gap rule of that MCP reference.
  - `unreal-skill` authors in-editor Agent Skills. Project knowledge belongs in `.agents/skills/` instead.
