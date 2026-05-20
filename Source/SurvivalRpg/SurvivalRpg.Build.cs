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
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"Engine",
			"GameFeatures",
			"GameplayStateTreeModule",
			"CommonLoadingScreen",
			"InputCore",
			"ModularGameplay",
			"ModularGameplayActors",
			"NetCore",
			"StateTreeModule",
			"UMG",
			"CommonUI",
			"GameplayAbilities", "GameplayTags", "GameplayTasks",
		});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DeveloperSettings",
				"EnhancedInput",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"ModelViewViewModel",
				"AdvancedSessions",
				"GameplayMessageRuntime",
				"Projects",
			});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
		
		// Generate compile errors if using DrawDebug functions in test/shipping builds.
		PublicDefinitions.Add("SHIPPING_DRAW_DEBUG_ERROR=1");
		
		SetupGameplayDebuggerSupport(Target);
		SetupIrisSupport(Target);
	}
}
