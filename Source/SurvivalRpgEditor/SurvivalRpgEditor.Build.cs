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
			"AIModule",
			"AssetRegistry",
			"AssetTools",
			"BlueprintGraph",
			"BlueprintEditorLibrary",
			"CQTest",
			"CommonGame",
			"CommonUI",
			"EngineSettings",
			"EnhancedInput",
			"GF_Harvesting_Magic",
			"GameFeatures",
			"GameplayAbilities",
			"GameplayStateTreeModule",
			"GameplayTags",
			"InputBlueprintNodes",
			"InputCore",
			"LevelEditor",
			"ModelViewViewModel",
			"ModelViewViewModelBlueprint",
			"ModelViewViewModelEditor",
			"ModularGameplay",
			"ModularGameplayActors",
			"PropertyBindingUtils",
			"Projects",
			"SlateCore",
			"StateTreeEditorModule",
			"UIExtension",
			"UMG",
			"UMGEditor",
			"UnrealEd",
		});

		SetupIrisSupport(Target);
	}
}
