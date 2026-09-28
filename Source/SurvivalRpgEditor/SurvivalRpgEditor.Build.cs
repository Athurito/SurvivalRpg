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
			"AnimGraph",
			"AssetRegistry",
			"AssetTools",
			"BlueprintGraph",
			"BlueprintEditorLibrary",
			"Chooser",
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
			"IKRig",
			"InputBlueprintNodes",
			"InputCore",
			"Json",
			"JsonUtilities",
			"LevelEditor",
			"ModelViewViewModel",
			"ModelViewViewModelBlueprint",
			"ModelViewViewModelEditor",
			"ModularGameplay",
			"ModularGameplayActors",
			"MotionWarping",
			"Mover",
			"NetworkPrediction",
			"PhysicsControl",
			"PhysicsCore",
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
