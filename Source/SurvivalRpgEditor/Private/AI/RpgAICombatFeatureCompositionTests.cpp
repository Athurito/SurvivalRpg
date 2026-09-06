#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameFeatureData.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/Core/AI/RpgAIPawnData.h"
#include "SurvivalRpg/Core/AI/RpgAIPlayerState.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddAbilities.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

namespace RpgAICombatFeatureCompositionTests
{
	constexpr TCHAR CombatFeatureDataObject[] =
		TEXT("/GF_Combat_Core/GF_Combat_Core.GF_Combat_Core");
	constexpr TCHAR CombatCoreAbilitySetObject[] =
		TEXT("/GF_Combat_Core/GAS/AbilitySets/AS_Combat_Core_Default.AS_Combat_Core_Default");
	constexpr TCHAR CombatCoreAbilitySetPackage[] =
		TEXT("/GF_Combat_Core/GAS/AbilitySets/AS_Combat_Core_Default");
	constexpr TCHAR CombatAIDefenseAbilitySetObject[] =
		TEXT("/GF_Combat_Core/GAS/AbilitySets/AS_Combat_AI_Defense.AS_Combat_AI_Defense");
	constexpr TCHAR StaminaAttributeSetClass[] =
		TEXT("/Script/SurvivalRpg.RpgStaminaSet");
	constexpr TCHAR CombatAttributeSetClass[] =
		TEXT("/Script/SurvivalRpg.RpgCombatSet");
	constexpr TCHAR DefenseAttributeSetClass[] =
		TEXT("/Script/SurvivalRpg.RpgDefenseSet");

	struct FAIPawnDataContract
	{
		const TCHAR* ObjectPath;
		const TCHAR* PackagePath;
	};

	constexpr FAIPawnDataContract AIPawnDataContracts[] = {
		{
			TEXT("/Game/SurvivalRpg/Enemies/DA_AIPawnData.DA_AIPawnData"),
			TEXT("/Game/SurvivalRpg/Enemies/DA_AIPawnData"),
		},
		{
			TEXT("/GF_AI_RiftMonsters/AI/PawnData/DA_RiftGrunt_Sword_PawnData.DA_RiftGrunt_Sword_PawnData"),
			TEXT("/GF_AI_RiftMonsters/AI/PawnData/DA_RiftGrunt_Sword_PawnData"),
		},
		{
			TEXT("/GF_AI_Wildlife/AI/PawnData/DA_PlaceholderBeast_PawnData.DA_PlaceholderBeast_PawnData"),
			TEXT("/GF_AI_Wildlife/AI/PawnData/DA_PlaceholderBeast_PawnData"),
		},
	};

	constexpr const TCHAR* ExperienceClassPaths[] = {
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience.RpgPrototypeExperience_C"),
	};

	int32 CountAbilitySetPath(
		const FRpgGameFeatureAbilitiesEntry& Entry,
		const FSoftObjectPath& ExpectedPath)
	{
		int32 Count = 0;
		for (const TSoftObjectPtr<const URpgAbilitySet>& AbilitySet : Entry.GrantedAbilitySets)
		{
			Count += AbilitySet.ToSoftObjectPath() == ExpectedPath ? 1 : 0;
		}
		return Count;
	}

	int32 CountPawnDataAbilitySetPath(
		const URpgAIPawnData& PawnData,
		const FSoftObjectPath& ExpectedPath)
	{
		int32 Count = 0;
		for (const TObjectPtr<const URpgAbilitySet>& AbilitySet : PawnData.AbilitySets)
		{
			Count += FSoftObjectPath(AbilitySet.Get()) == ExpectedPath ? 1 : 0;
		}
		return Count;
	}

	int32 CountAttributeSetPath(
		const FRpgGameFeatureAbilitiesEntry& Entry,
		const FSoftObjectPath& ExpectedPath)
	{
		int32 Count = 0;
		for (const FRpgGameFeatureAttributeSetGrant& AttributeSet : Entry.GrantedAttributes)
		{
			Count += AttributeSet.AttributeSetType.ToSoftObjectPath() == ExpectedPath ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgAICombatFeatureCompositionTest,
	"SurvivalRpg.AI.CombatFeatureComposition",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgAICombatFeatureCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgAICombatFeatureCompositionTests;

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();
	const FSoftObjectPath CoreAbilitySetPath(CombatCoreAbilitySetObject);

	for (const FAIPawnDataContract& Contract : AIPawnDataContracts)
	{
		const URpgAIPawnData* PawnData = LoadObject<URpgAIPawnData>(nullptr, Contract.ObjectPath);
		if (!TestNotNull(
			*FString::Printf(TEXT("AI PawnData loads: %s"), Contract.ObjectPath),
			PawnData))
		{
			continue;
		}

		TestEqual(
			*FString::Printf(
				TEXT("The shared combat core set stays out of AI PawnData: %s"),
				Contract.ObjectPath),
			CountPawnDataAbilitySetPath(*PawnData, CoreAbilitySetPath),
			0);
		TestFalse(
			*FString::Printf(
				TEXT("AI PawnData has no package dependency on GF_Combat_Core: %s"),
				Contract.PackagePath),
			AssetRegistry.ContainsDependency(
				FName(Contract.PackagePath),
				FName(CombatCoreAbilitySetPackage),
				UE::AssetRegistry::EDependencyCategory::Package));
	}

	const UGameFeatureData* CombatFeatureData =
		LoadObject<UGameFeatureData>(nullptr, CombatFeatureDataObject);
	if (!TestNotNull(TEXT("GF_Combat_Core GameFeatureData loads"), CombatFeatureData))
	{
		return false;
	}

	const FSoftObjectPath AIDefenseAbilitySetPath(CombatAIDefenseAbilitySetObject);
	const FSoftObjectPath StaminaAttributeSetPath(StaminaAttributeSetClass);
	const FSoftObjectPath CombatAttributeSetPath(CombatAttributeSetClass);
	const FSoftObjectPath DefenseAttributeSetPath(DefenseAttributeSetClass);
	int32 AIEntryCount = 0;
	int32 CoreAbilitySetCount = 0;
	int32 AIDefenseAbilitySetCount = 0;
	int32 StaminaAttributeSetCount = 0;
	int32 CombatAttributeSetCount = 0;
	int32 DefenseAttributeSetCount = 0;
	for (const UGameFeatureAction* Action : CombatFeatureData->GetActions())
	{
		const URpgGameFeatureAction_AddAbilities* AddAbilitiesAction =
			Cast<URpgGameFeatureAction_AddAbilities>(Action);
		if (!AddAbilitiesAction)
		{
			continue;
		}

		for (const FRpgGameFeatureAbilitiesEntry& Entry : AddAbilitiesAction->AbilitiesList)
		{
			if (Entry.ActorClass.LoadSynchronous() != ARpgAIPlayerState::StaticClass())
			{
				continue;
			}

			++AIEntryCount;
			CoreAbilitySetCount += CountAbilitySetPath(Entry, CoreAbilitySetPath);
			AIDefenseAbilitySetCount += CountAbilitySetPath(Entry, AIDefenseAbilitySetPath);
			StaminaAttributeSetCount += CountAttributeSetPath(Entry, StaminaAttributeSetPath);
			CombatAttributeSetCount += CountAttributeSetPath(Entry, CombatAttributeSetPath);
			DefenseAttributeSetCount += CountAttributeSetPath(Entry, DefenseAttributeSetPath);
		}
	}

	TestTrue(
		TEXT("GF_Combat_Core has at least one AI PlayerState grant entry"),
		AIEntryCount > 0);
	TestEqual(
		TEXT("GF_Combat_Core grants the shared core set to AI exactly once"),
		CoreAbilitySetCount,
		1);
	TestEqual(
		TEXT("GF_Combat_Core preserves the AI defense set exactly once"),
		AIDefenseAbilitySetCount,
		1);
	TestEqual(
		TEXT("GF_Combat_Core preserves Stamina for AI exactly once"),
		StaminaAttributeSetCount,
		1);
	TestEqual(
		TEXT("GF_Combat_Core preserves Combat for AI exactly once"),
		CombatAttributeSetCount,
		1);
	TestEqual(
		TEXT("GF_Combat_Core preserves Defense for AI exactly once"),
		DefenseAttributeSetCount,
		1);

	for (const TCHAR* ExperienceClassPath : ExperienceClassPaths)
	{
		const UClass* ExperienceClass = LoadClass<URpgExperienceDefinition>(
			nullptr,
			ExperienceClassPath);
		const URpgExperienceDefinition* Experience = ExperienceClass
			? Cast<URpgExperienceDefinition>(ExperienceClass->GetDefaultObject())
			: nullptr;
		if (!TestNotNull(
			*FString::Printf(TEXT("Experience loads: %s"), ExperienceClassPath),
			Experience))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(
				TEXT("Experience activates GF_Combat_Core: %s"),
				ExperienceClassPath),
			Experience->GameFeaturesToEnable.Contains(TEXT("GF_Combat_Core")));
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
