// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class SurvivalRpg : ModuleRules
{
	public SurvivalRpg(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"AIModule",
			"AnimationWarpingRuntime",
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"Engine",
			"GameFeatures",
			"GameplayMessageRuntime",
			"GameplayStateTreeModule",
			"CommonLoadingScreen",
			"InputCore",
			"ModularGameplay",
			"ModularGameplayActors",
			"NetCore",
			"PoseSearch",
			"StateTreeModule",
			"UMG",
			"CommonGame",
			"CommonUI",
			"UIExtension",
			"GameplayAbilities", "GameplayTags", "GameplayTasks",
		});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlendStack",
				"CommonInput",
				"DeveloperSettings",
				"EnhancedInput",
				"CoreOnline",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"ModelViewViewModel",
				"AdvancedSessions",
				"OnlineSubsystem",
				"Projects",
			});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
		
		// Generate compile errors if using DrawDebug functions in test/shipping builds.
		PublicDefinitions.Add("SHIPPING_DRAW_DEBUG_ERROR=1");
		
		SetupGameplayDebuggerSupport(Target);
		SetupIrisSupport(Target);
	}
}
