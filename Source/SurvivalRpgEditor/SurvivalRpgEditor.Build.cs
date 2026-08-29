using UnrealBuildTool;

public class SurvivalRpgEditor : ModuleRules
{
	public SurvivalRpgEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// UE 5.8 installed builds omit this editor plugin's public headers when it is only a private dependency.
		PrivateIncludePathModuleNames.Add("DataValidation");

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
			"DataValidation",
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
