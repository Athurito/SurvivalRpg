// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "Engine/Blueprint.h"
#include "GameFeatureData.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#include "SurvivalRpg/Animation/RpgCombatAnimationProfile.h"
#include "SurvivalRpg/Animation/RpgCombatAnimationProfileProviderComponent.h"
#include "SurvivalRpg/GameFeatures/RpgGameFeatureAction_AddComponents.h"

namespace RpgCombatAnimationFeatureCompositionTests
{
	constexpr TCHAR CombatFeatureDataObject[] =
		TEXT("/GF_Combat_Core/GF_Combat_Core.GF_Combat_Core");
	constexpr TCHAR ProviderBlueprintObject[] =
		TEXT("/GF_Combat_Core/Animations/Profiles/BP_RpgCombatAnimationProfileProvider.BP_RpgCombatAnimationProfileProvider");
	constexpr TCHAR CombatProfileObject[] =
		TEXT("/GF_Combat_Core/Animations/Profiles/DA_RpgCombatAnimationProfile.DA_RpgCombatAnimationProfile");
	constexpr TCHAR GaspCharacterClass[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/BP_Rpg_Character_GASP.BP_Rpg_Character_GASP_C");
	constexpr TCHAR ProviderComponentClass[] =
		TEXT("/GF_Combat_Core/Animations/Profiles/BP_RpgCombatAnimationProfileProvider.BP_RpgCombatAnimationProfileProvider_C");

	constexpr TCHAR CombatFeatureDataPackage[] = TEXT("/GF_Combat_Core/GF_Combat_Core");
	constexpr TCHAR ProviderBlueprintPackage[] =
		TEXT("/GF_Combat_Core/Animations/Profiles/BP_RpgCombatAnimationProfileProvider");
	constexpr TCHAR CombatProfilePackage[] =
		TEXT("/GF_Combat_Core/Animations/Profiles/DA_RpgCombatAnimationProfile");
	constexpr TCHAR GaspAnimBlueprintPackage[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP/ABP_RpgCharacter_GASP");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCombatAnimationFeatureCompositionTest,
	"SurvivalRpg.Animation.Combat.FeatureComposition",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCombatAnimationFeatureCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgCombatAnimationFeatureCompositionTests;

	const UGameFeatureData* CombatFeatureData =
		LoadObject<UGameFeatureData>(nullptr, CombatFeatureDataObject);
	UBlueprint* ProviderBlueprint =
		LoadObject<UBlueprint>(nullptr, ProviderBlueprintObject);
	URpgCombatAnimationProfile* CombatProfile =
		LoadObject<URpgCombatAnimationProfile>(nullptr, CombatProfileObject);
	if (!TestNotNull(TEXT("GF_Combat_Core GameFeatureData loads"), CombatFeatureData) ||
		!TestNotNull(TEXT("The feature-owned combat profile provider Blueprint loads"), ProviderBlueprint) ||
		!TestNotNull(TEXT("The feature-owned combat animation profile loads"), CombatProfile))
	{
		return false;
	}

	TestEqual(
		TEXT("The provider Blueprint derives directly from the native lifecycle seam"),
		ProviderBlueprint->ParentClass.Get(),
		URpgCombatAnimationProfileProviderComponent::StaticClass());
	const URpgCombatAnimationProfileProviderComponent* ProviderDefaults =
		ProviderBlueprint->GeneratedClass
			? Cast<URpgCombatAnimationProfileProviderComponent>(
				ProviderBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (TestNotNull(TEXT("The provider generated-class defaults load"), ProviderDefaults))
	{
		TestEqual(
			TEXT("The provider binds the exact feature-owned combat profile"),
			ProviderDefaults->GetCombatAnimationProfile(),
			CombatProfile);
		TestFalse(
			TEXT("The cosmetic profile provider never ticks"),
			ProviderDefaults->PrimaryComponentTick.bCanEverTick);
		TestFalse(
			TEXT("The cosmetic profile provider never replicates"),
			ProviderDefaults->GetIsReplicated());
		FDataValidationContext ProviderValidationContext;
		TestEqual(
			TEXT("The configured provider passes Unreal data validation"),
			ProviderDefaults->IsDataValid(ProviderValidationContext),
			EDataValidationResult::Valid);
	}

	const FObjectPropertyBase* ProfileProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgCombatAnimationProfileProviderComponent::StaticClass(),
			TEXT("CombatAnimationProfile"));
	TestTrue(
		TEXT("The provider exposes one designer-authored hard profile property"),
		ProfileProperty &&
			ProfileProperty->PropertyClass == URpgCombatAnimationProfile::StaticClass() &&
			ProfileProperty->HasAllPropertyFlags(
				CPF_Edit | CPF_DisableEditOnInstance |
				CPF_BlueprintVisible | CPF_BlueprintReadOnly) &&
			!ProfileProperty->HasAnyPropertyFlags(CPF_Transient));

	const FSoftObjectPath ExpectedActorClass(GaspCharacterClass);
	const FSoftObjectPath ExpectedProviderClass(ProviderComponentClass);
	int32 ProviderEntryCount = 0;
	for (const UGameFeatureAction* Action : CombatFeatureData->GetActions())
	{
		const URpgGameFeatureAction_AddComponents* AddComponentsAction =
			Cast<URpgGameFeatureAction_AddComponents>(Action);
		if (!AddComponentsAction)
		{
			continue;
		}

		for (const FRpgGameFeatureComponentEntry& Entry :
			AddComponentsAction->ComponentList)
		{
			if (Entry.ComponentClass.ToSoftObjectPath() != ExpectedProviderClass)
			{
				continue;
			}

			++ProviderEntryCount;
			TestEqual(
				TEXT("The profile provider targets only the isolated GASP character"),
				Entry.ActorClass.ToSoftObjectPath(),
				ExpectedActorClass);
			TestTrue(
				TEXT("The profile provider is present in graphical client worlds"),
				Entry.bClientComponent);
			TestFalse(
				TEXT("The profile provider is omitted from dedicated servers"),
				Entry.bServerComponent);
		}
	}
	TestEqual(
		TEXT("GF_Combat_Core grants exactly one combat-animation profile provider"),
		ProviderEntryCount,
		1);

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous(
		{
			FString(TEXT("/GF_Combat_Core")),
			FString(TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/GASP")),
		},
		true,
		false);
	AssetRegistry.WaitForCompletion();

	TestTrue(
		TEXT("GF_Combat_Core records the provider Blueprint as feature composition"),
		AssetRegistry.ContainsDependency(
			FName(CombatFeatureDataPackage),
			FName(ProviderBlueprintPackage),
			UE::AssetRegistry::EDependencyCategory::Package));
	UE::AssetRegistry::FDependencyQuery HardGamePackageQuery;
	HardGamePackageQuery.Required =
		UE::AssetRegistry::EDependencyProperty::Hard |
		UE::AssetRegistry::EDependencyProperty::Game;
	TestTrue(
		TEXT("The feature-owned provider hard-references its complete profile for cook and GC"),
		AssetRegistry.ContainsDependency(
			FName(ProviderBlueprintPackage),
			FName(CombatProfilePackage),
			UE::AssetRegistry::EDependencyCategory::Package,
			HardGamePackageQuery));
	TestFalse(
		TEXT("The core GASP AnimBlueprint has no direct dependency on the Combat GameFeature profile"),
		AssetRegistry.ContainsDependency(
			FName(GaspAnimBlueprintPackage),
			FName(CombatProfilePackage),
			UE::AssetRegistry::EDependencyCategory::Package));

	TArray<FName> CombatProfileReferencers;
	AssetRegistry.GetReferencers(
		FName(CombatProfilePackage),
		CombatProfileReferencers,
		UE::AssetRegistry::EDependencyCategory::Package);
	TestTrue(
		TEXT("The feature-owned provider is a recorded profile referencer"),
		CombatProfileReferencers.Contains(FName(ProviderBlueprintPackage)));
	TestFalse(
		TEXT("The core GASP AnimBlueprint is no longer a profile referencer"),
		CombatProfileReferencers.Contains(FName(GaspAnimBlueprintPackage)));

	TArray<FAssetData> AssetsToValidate;
	for (const FName PackageName : {
			 FName(GaspAnimBlueprintPackage),
			 FName(CombatFeatureDataPackage),
			 FName(ProviderBlueprintPackage),
		 })
	{
		TArray<FAssetData> PackageAssets;
		AssetRegistry.GetAssetsByPackageName(PackageName, PackageAssets);
		AssetsToValidate.Append(PackageAssets);
	}
	TestEqual(
		TEXT("The focused validation set contains the AnimBP, GameFeatureData, and provider Blueprint"),
		AssetsToValidate.Num(),
		3);

	UEditorValidatorSubsystem* ValidatorSubsystem = GEditor
		? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>()
		: nullptr;
	if (TestNotNull(
			TEXT("The Unreal Editor validation subsystem is available"),
			ValidatorSubsystem))
	{
		FValidateAssetsSettings ValidationSettings;
		ValidationSettings.bCollectPerAssetDetails = true;
		ValidationSettings.bCaptureAssetLoadLogs = false;
		ValidationSettings.bCaptureLogsDuringValidation = false;
		ValidationSettings.bShowIfNoFailures = false;
		ValidationSettings.bSilent = true;
		ValidationSettings.ShowMessageLogSeverity.Reset();
		ValidationSettings.ValidationUsecase = EDataValidationUsecase::Manual;

		FValidateAssetsResults ValidationResults;
		ValidatorSubsystem->ValidateAssetsWithSettings(
			AssetsToValidate,
			ValidationSettings,
			ValidationResults);
		for (const TPair<FString, FValidateAssetsDetails>& AssetDetails :
			ValidationResults.AssetsDetails)
		{
			if (AssetDetails.Value.Result != EDataValidationResult::Invalid)
			{
				continue;
			}

			for (const FText& Error : AssetDetails.Value.ValidationErrors)
			{
				AddError(FString::Printf(
					TEXT("Data Validation failed for %s: %s"),
					*AssetDetails.Key,
					*Error.ToString()));
			}
		}
		TestEqual(
			TEXT("Focused Unreal Data Validation reports no invalid assets"),
			ValidationResults.NumInvalid,
			0);
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
