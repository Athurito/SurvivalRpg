using UnrealBuildTool;

/**
 * Uncooked-only representations of project animation graph nodes.
 *
 * Runtime node structs remain in SurvivalRpg. Keeping their editor graph wrappers in an
 * UncookedOnly module lets separate PIE/game processes load uncooked AnimBlueprint assets
 * without pulling the full SurvivalRpgEditor module into gameplay.
 */
public class SurvivalRpgAnimGraph : ModuleRules
{
	public SurvivalRpgAnimGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"AnimGraph",
			"AnimGraphRuntime",
			"Core",
			"CoreUObject",
			"Engine",
			"SurvivalRpg",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"BlueprintGraph",
			"UnrealEd",
		});
	}
}
