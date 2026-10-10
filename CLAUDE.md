@AGENTS.md

## Claude Code

- `.claude/skills/*` are generated copies of the canonical skills in `.agents/skills/*`. Do not edit them directly; follow "Agent skills" in AGENTS.md.
- The Unreal MCP server `unreal-mcp` is configured in `.mcp.json`. Approve it once when Claude Code asks. It only responds while the Unreal Editor runs with the MCP plugin enabled. A session that started before the editor stays without its tools until `unreal-mcp` is reconnected with `/mcp`.
- `.claude/settings.json` enables Epic's official plugin `unreal-engine-skills-for-claude-code`; Claude Code offers to install it when you trust the folder. Its skills are generic and the routing in AGENTS.md still decides:
  - The `unreal-mcp` skill adds the generic tool workflow and safety rules. For asset authoring, also load `unreal-lyra-expert` and follow its MCP reference, which wins where they differ.
  - `create-toolset` applies to new or changed tools in `Build/Tools/Unreal/`, within the capability-gap rule of that MCP reference.
  - `unreal-skill` authors in-editor Agent Skills. Project knowledge belongs in `.agents/skills/` instead.
