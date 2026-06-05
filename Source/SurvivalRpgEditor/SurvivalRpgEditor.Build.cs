using UnrealBuildTool;

public class SurvivalRpgEditor : ModuleRules
{
	public SurvivalRpgEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"StateTreeModule",
			"SurvivalRpg",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayStateTreeModule",
			"GameplayTags",
			"PropertyBindingUtils",
			"StateTreeEditorModule",
		});
	}
}
