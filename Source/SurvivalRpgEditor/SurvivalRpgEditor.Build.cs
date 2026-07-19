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
			"AssetRegistry",
			"BlueprintGraph",
			"CommonUI",
			"EnhancedInput",
			"GameplayStateTreeModule",
			"GameplayTags",
			"InputBlueprintNodes",
			"InputCore",
			"ModelViewViewModel",
			"ModelViewViewModelBlueprint",
			"ModelViewViewModelEditor",
			"PropertyBindingUtils",
			"StateTreeEditorModule",
			"UIExtension",
			"UMG",
			"UMGEditor",
			"UnrealEd",
		});
	}
}
