# Unreal MCP Asset Authoring

Use this workflow whenever work creates or changes Blueprint, Widget Blueprint, Gameplay Ability, or DataAsset assets.

## Required workflow

1. Check that Unreal Editor is running and that the project MCP endpoint is reachable at `http://127.0.0.1:8000/mcp`.
2. Search the available MCP tools before deciding that an asset operation is unsupported. Tool availability can vary with the active editor toolsets.
3. Inspect the existing parent classes, related assets, Blueprint graphs, widget trees, object properties, references, and project naming/placement conventions before authoring.
4. Author through the narrowest applicable MCP surface:
   - Blueprint tools for Blueprint assets, graphs, functions, events, nodes, variables, and compilation;
   - UMG tools for Widget Blueprints, widget trees, named slots, bindings, and widget variables;
   - Object tools for reflected property inspection and assignment;
   - DataAsset/asset tools for definitions, fragment-instance configuration, references, loading, and saving;
   - GAS tools for Gameplay Ability, Gameplay Effect, Gameplay Cue, AbilitySet, and tag-related asset inspection or authoring where available.
5. Compile affected Blueprints and Widget Blueprints, save all changed assets, and validate parentage, compile status, required properties, graphs/bindings, references, and asset contracts.

Do not replace an asset-authoring step with a runtime C++ content class merely because editing `.uasset` files is less convenient than editing source.

If the required operation still cannot be performed after tool search and inspection, record the exact missing capability. Only then introduce the smallest reusable editor-only MCP/tooling seam that closes that confirmed gap. Keep it out of runtime modules and do not create one-off tooling for a single content leaf.
