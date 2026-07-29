#include "RpgTradeSkillProgressionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/DataValidation.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "RpgTradeSkillGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTradeSkillDefaultCurvesTest,
	"SurvivalRpg.Progression.TradeSkills.DefaultCurves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTradeSkillDefaultCurvesTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Level 1 uses the documented 100 XP cost"),
		URpgTradeSkillProgressionComponent::CalculateDefaultXPToNextLevel(1),
		100.0f);
	TestEqual(
		TEXT("Level 1 yield starts at 1.0"),
		URpgTradeSkillProgressionComponent::CalculateDefaultYieldMultiplier(1),
		1.0f);
	TestEqual(
		TEXT("Level 100 yield ends at 1.5"),
		URpgTradeSkillProgressionComponent::CalculateDefaultYieldMultiplier(100),
		1.5f);
	TestEqual(
		TEXT("Level 1 rare find starts at 1.0"),
		URpgTradeSkillProgressionComponent::CalculateDefaultRareFindMultiplier(1),
		1.0f);
	TestEqual(
		TEXT("Level 100 rare find ends at 2.0"),
		URpgTradeSkillProgressionComponent::CalculateDefaultRareFindMultiplier(100),
		2.0f);
	TestEqual(
		TEXT("Legacy Harvesting maps to Foraging"),
		URpgTradeSkillProgressionComponent::GetSkillTagForLegacySkill(ETradeSkill::Harvesting),
		FGameplayTag(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging));
	TestEqual(
		TEXT("Legacy Logging maps to the appended Logging skill"),
		URpgTradeSkillProgressionComponent::GetSkillTagForLegacySkill(ETradeSkill::Logging),
		FGameplayTag(RpgTradeSkillGameplayTags::Skill_Gathering_Logging));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTradeSkillAuthorityAndLevelUpTest,
	"SurvivalRpg.Progression.TradeSkills.AuthorityAndLevelUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTradeSkillAuthorityAndLevelUpTest::RunTest(const FString& Parameters)
{
	AActor* Owner = NewObject<AActor>();
	URpgTradeSkillProgressionComponent* Component =
		NewObject<URpgTradeSkillProgressionComponent>(Owner);
	Component->ResetSkillStatesToDefaults();

	TestEqual(TEXT("All six core skills receive default states"), Component->SkillStates.Num(), 6);
	TestTrue(
		TEXT("A server-owned skill accepts positive XP"),
		Component->AddSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging, 100.0f));
	TestEqual(
		TEXT("The default first XP threshold advances Foraging to level 2"),
		Component->GetSkillLevelByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		2);
	TestEqual(
		TEXT("Exact threshold consumption leaves zero carried XP"),
		Component->GetSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		0.0f);
	const float LevelTwoCost = URpgTradeSkillProgressionComponent::CalculateDefaultXPToNextLevel(2);
	const float LevelThreeCost = URpgTradeSkillProgressionComponent::CalculateDefaultXPToNextLevel(3);
	TestTrue(
		TEXT("One award can cross multiple skill levels"),
		Component->AddSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging,
			LevelTwoCost + LevelThreeCost + 25.0f));
	TestEqual(
		TEXT("Repeated thresholds advance through level 4"),
		Component->GetSkillLevelByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		4);
	TestEqual(
		TEXT("XP beyond repeated thresholds is retained"),
		Component->GetSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		25.0f);
	TestFalse(
		TEXT("Non-positive XP is rejected"),
		Component->AddSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Foraging, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTradeSkillSaveStateValidationTest,
	"SurvivalRpg.Progression.TradeSkills.SaveStateValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTradeSkillSaveStateValidationTest::RunTest(const FString& Parameters)
{
	FTradeSkillState Mining;
	Mining.SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
	Mining.Level = 12;
	Mining.XP = 34.0f;

	TestTrue(
		TEXT("A unique registered Skill.* state validates"),
		URpgTradeSkillProgressionComponent::ValidateSkillStates({ Mining }));
	TestFalse(
		TEXT("Duplicate skill identities are rejected"),
		URpgTradeSkillProgressionComponent::ValidateSkillStates({ Mining, Mining }));

	FTradeSkillState InvalidLevel = Mining;
	InvalidLevel.Level = 101;
	TestFalse(
		TEXT("Levels above the supported cap are rejected"),
		URpgTradeSkillProgressionComponent::ValidateSkillStates({ InvalidLevel }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTradeSkillRestoreReconnectTest,
	"SurvivalRpg.Progression.TradeSkills.TagRestoreDefaultsAndReconnect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTradeSkillRestoreReconnectTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<FTradeSkillState> LegacyIndexStates;
	LegacyIndexStates.SetNum(static_cast<int32>(ETradeSkill::MAX));
	for (int32 Index = 0; Index < LegacyIndexStates.Num(); ++Index)
	{
		LegacyIndexStates[Index].Level = Index + 2;
		LegacyIndexStates[Index].XP = static_cast<float>(Index * 10);
	}
	AActor* LegacyOwner = NewObject<AActor>();
	URpgTradeSkillProgressionComponent* LegacyComponent =
		NewObject<URpgTradeSkillProgressionComponent>(LegacyOwner);
	TestTrue(
		TEXT("A tagless legacy enum-index snapshot migrates before strict validation"),
		LegacyComponent->RestoreSkillStates(LegacyIndexStates));
	TestEqual(
		TEXT("Legacy Mining index migrates to its stable gathering tag"),
		LegacyComponent->GetSkillLevelByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Mining),
		4);
	TestEqual(
		TEXT("Legacy Harvesting index migrates to Foraging"),
		LegacyComponent->GetSkillLevelByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Foraging),
		5);
	TestEqual(
		TEXT("Legacy Logging index migrates to its appended stable tag"),
		LegacyComponent->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Logging),
		40.0f);

	FTradeSkillState Mining;
	Mining.SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Mining;
	Mining.Level = 12;
	Mining.XP = 34.0f;

	AActor* FirstOwner = NewObject<AActor>();
	URpgTradeSkillProgressionComponent* FirstComponent =
		NewObject<URpgTradeSkillProgressionComponent>(FirstOwner);
	TestTrue(
		TEXT("A tag-keyed save snapshot restores authoritatively"),
		FirstComponent->RestoreSkillStates({Mining}));
	TestEqual(
		TEXT("The restored tag retains its saved level"),
		FirstComponent->GetSkillLevelByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Mining),
		12);
	TestEqual(
		TEXT("The restored tag retains its saved XP"),
		FirstComponent->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Mining),
		34.0f);
	TestEqual(
		TEXT("A missing older-save skill initializes at level one"),
		FirstComponent->GetSkillLevelByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Logging),
		1);
	TestEqual(
		TEXT("A missing older-save skill initializes with zero XP"),
		FirstComponent->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Logging),
		0.0f);
	TestEqual(
		TEXT("An older save gains Skinning at level one"),
		FirstComponent->GetSkillLevelByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Skinning),
		1);
	TestEqual(
		TEXT("An older save gains Skinning with zero XP"),
		FirstComponent->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Skinning),
		0.0f);
	TestEqual(
		TEXT("The deprecated enum adapter resolves the same migrated tag state"),
		FirstComponent->GetSkillLevel(ETradeSkill::Mining),
		12);

	const TArray<FTradeSkillState> ReconnectSnapshot =
		FirstComponent->ExportSkillStates();
	AActor* ReconnectedOwner = NewObject<AActor>();
	URpgTradeSkillProgressionComponent* ReconnectedComponent =
		NewObject<URpgTradeSkillProgressionComponent>(ReconnectedOwner);
	TestTrue(
		TEXT("A reconnect restores the exported tag-keyed snapshot"),
		ReconnectedComponent->RestoreSkillStates(ReconnectSnapshot));
	TestEqual(
		TEXT("Reconnect preserves the saved mining level"),
		ReconnectedComponent->GetSkillLevelByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Mining),
		12);
	TestEqual(
		TEXT("Reconnect preserves the saved mining XP"),
		ReconnectedComponent->GetSkillXPByTag(
			RpgTradeSkillGameplayTags::Skill_Gathering_Mining),
		34.0f);
	TestEqual(
		TEXT("Reconnect retains exactly the six core skill identities"),
		ReconnectSnapshot.Num(),
		6);
	return true;
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgTradeSkillConfigValidationTest,
	"SurvivalRpg.Progression.TradeSkills.ConfigValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgTradeSkillConfigValidationTest::RunTest(const FString& Parameters)
{
	URpgTradeSkillConfigData* Config = NewObject<URpgTradeSkillConfigData>();
	Config->TaggedSkillConfigs.Add(
		RpgTradeSkillGameplayTags::Skill_Gathering_Foraging,
		FTradeSkillConfig());
	FDataValidationContext ValidContext;
	TestEqual(
		TEXT("A default tag-keyed config validates"),
		Config->IsDataValid(ValidContext),
		EDataValidationResult::Valid);

	Config->TaggedSkillConfigs.FindChecked(
		RpgTradeSkillGameplayTags::Skill_Gathering_Foraging).MaxLevel = 101;
	FDataValidationContext InvalidContext;
	TestEqual(
		TEXT("A config above the supported level cap is rejected"),
		Config->IsDataValid(InvalidContext),
		EDataValidationResult::Invalid);
	return true;
}

#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
