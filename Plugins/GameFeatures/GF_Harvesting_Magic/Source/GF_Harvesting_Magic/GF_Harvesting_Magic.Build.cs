using UnrealBuildTool;

public class GF_Harvesting_Magic : ModuleRules
{
	public GF_Harvesting_Magic(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"NetCore",
			"SurvivalRpg",
		});

		PrivateDependencyModuleNames.Add("PhysicsCore");
	}
}
