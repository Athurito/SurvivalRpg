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
			"AnimGraph",
			"AnimGraphRuntime",
			"AnimationWarpingEditor",
			"AnimationWarpingRuntime",
			"AssetRegistry",
			"AssetTools",
			"BlendStack",
			"BlendStackEditor",
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
			"PoseSearch",
			"PoseSearchEditor",
			"Projects",
			"SlateCore",
			"StateTreeEditorModule",
			"SurvivalRpgAnimGraph",
			"UIExtension",
			"UMG",
			"UMGEditor",
			"UnrealEd",
		});

		SetupIrisSupport(Target);
	}
}
