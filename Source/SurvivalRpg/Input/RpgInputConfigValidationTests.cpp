#include "RpgInputConfig.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "InputAction.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"

namespace
{
	struct FInputConfigValidationSnapshot
	{
		EDataValidationResult Result =
			EDataValidationResult::NotValidated;
		TArray<FString> Errors;
		TArray<FString> Warnings;
	};

	FRpgInputAction MakeInputMapping(
		const UInputAction* InputAction,
		const FGameplayTag InputTag)
	{
		FRpgInputAction Mapping;
		Mapping.InputAction = InputAction;
		Mapping.InputTag = InputTag;
		return Mapping;
	}

	FInputConfigValidationSnapshot ValidateInputConfig(
		const URpgInputConfig& Config)
	{
		FInputConfigValidationSnapshot Snapshot;
		FDataValidationContext Context;
		Snapshot.Result = Config.IsDataValid(Context);
		for (const FDataValidationContext::FIssue& Issue :
			Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error)
			{
				Snapshot.Errors.Add(Issue.Message.ToString());
			}
			else if (
				Issue.Severity ==
					EMessageSeverity::Warning)
			{
				Snapshot.Warnings.Add(
					Issue.Message.ToString());
			}
		}

		return Snapshot;
	}

	bool ContainsAll(
		const FString& Text,
		const TArray<FString>& ExpectedFragments)
	{
		for (const FString& Fragment : ExpectedFragments)
		{
			if (!Text.Contains(Fragment))
			{
				return false;
			}
		}

		return true;
	}

	TArray<FName> GetInputConfigContentRoots()
	{
		TArray<FName> ContentRoots = { FName(TEXT("/Game")) };
		for (const TSharedRef<IPlugin>& Plugin :
			IPluginManager::Get().
				GetEnabledPluginsWithContent())
		{
			if (!Plugin->IsMounted() ||
				Plugin->GetType() != EPluginType::Project)
			{
				continue;
			}

			FString MountedAssetPath =
				Plugin->GetMountedAssetPath();
			MountedAssetPath.RemoveFromEnd(TEXT("/"));
			if (!MountedAssetPath.IsEmpty())
			{
				ContentRoots.AddUnique(
					FName(*MountedAssetPath));
			}
		}

		return ContentRoots;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInputConfigValidationDiagnosticsTest,
	"SurvivalRpg.Input.Config.Validation.Diagnostics",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInputConfigValidationDiagnosticsTest::RunTest(
	const FString& Parameters)
{
	const auto NewConfig =
		[]()
		{
			return NewObject<URpgInputConfig>(
				GetTransientPackage());
		};
	const auto NewAction =
		[](UObject* Outer)
		{
			return NewObject<UInputAction>(Outer);
		};
	const auto ExpectInvalid =
		[this](
			const TCHAR* Scenario,
			const URpgInputConfig& Config,
			const int32 ExpectedErrorCount,
			const TArray<FString>& ExpectedFragments)
		{
			const FInputConfigValidationSnapshot Snapshot =
				ValidateInputConfig(Config);
			TestEqual(
				*FString::Printf(
					TEXT("%s is rejected"),
					Scenario),
				Snapshot.Result,
				EDataValidationResult::Invalid);
			TestEqual(
				*FString::Printf(
					TEXT("%s emits focused errors"),
					Scenario),
				Snapshot.Errors.Num(),
				ExpectedErrorCount);
			const FString Errors =
				FString::Join(
					Snapshot.Errors,
					TEXT("\n"));
			TestTrue(
				*FString::Printf(
					TEXT(
						"%s diagnostics identify the "
						"property path and conflict"),
					Scenario),
				ContainsAll(
					Errors,
					ExpectedFragments));
		};

	URpgInputConfig* ValidConfig = NewConfig();
	UInputAction* SharedNativeAction =
		NewAction(ValidConfig);
	ValidConfig->NativeInputActions.Add(
		MakeInputMapping(
			SharedNativeAction,
			RpgGameplayTags::InputTag_Jump));
	ValidConfig->NativeInputActions.Add(
		MakeInputMapping(
			SharedNativeAction,
			RpgGameplayTags::InputTag_StopJump));
	ValidConfig->AbilityInputActions.Add(
		MakeInputMapping(
			SharedNativeAction,
			RpgGameplayTags::InputTag_Weapon_Primary));
	const FInputConfigValidationSnapshot ValidSnapshot =
		ValidateInputConfig(*ValidConfig);
	TestEqual(
		TEXT(
			"Distinct tags may intentionally share one "
			"InputAction within and across both lists"),
		ValidSnapshot.Result,
		EDataValidationResult::Valid);
	TestEqual(
		TEXT("The valid shared-action config emits no errors"),
		ValidSnapshot.Errors.Num(),
		0);
	TestEqual(
		TEXT(
			"The valid shared-action config emits no "
			"cross-list warnings"),
		ValidSnapshot.Warnings.Num(),
		0);

	URpgInputConfig* NullActionConfig = NewConfig();
	NullActionConfig->NativeInputActions.Add(
		MakeInputMapping(
			nullptr,
			RpgGameplayTags::InputTag_Jump));
	ExpectInvalid(
		TEXT("A null native action"),
		*NullActionConfig,
		1,
		{
			NullActionConfig->GetPathName(),
			TEXT("NativeInputActions[0].InputAction"),
			TEXT("is null")
		});

	URpgInputConfig* MissingTagConfig = NewConfig();
	MissingTagConfig->AbilityInputActions.Add(
		MakeInputMapping(
			NewAction(MissingTagConfig),
			FGameplayTag()));
	ExpectInvalid(
		TEXT("An unset ability input tag"),
		*MissingTagConfig,
		1,
		{
			MissingTagConfig->GetPathName(),
			TEXT("AbilityInputActions[0].InputTag"),
			TEXT("is unset")
		});

	URpgInputConfig* RootTagConfig = NewConfig();
	RootTagConfig->NativeInputActions.Add(
		MakeInputMapping(
			NewAction(RootTagConfig),
			FGameplayTag::RequestGameplayTag(
				TEXT("InputTag"),
				/*ErrorIfNotFound=*/ false)));
	ExpectInvalid(
		TEXT("The InputTag root"),
		*RootTagConfig,
		1,
		{
			RootTagConfig->GetPathName(),
			TEXT("NativeInputActions[0].InputTag"),
			TEXT("strict descendant of InputTag")
		});

	URpgInputConfig* WrongNamespaceConfig = NewConfig();
	WrongNamespaceConfig->NativeInputActions.Add(
		MakeInputMapping(
			NewAction(WrongNamespaceConfig),
			RpgGameplayTags::UI_Screen_Inventory));
	ExpectInvalid(
		TEXT("A tag from another namespace"),
		*WrongNamespaceConfig,
		1,
		{
			WrongNamespaceConfig->GetPathName(),
			TEXT("NativeInputActions[0].InputTag"),
			TEXT("UI.Screen.Inventory"),
			TEXT("strict descendant of InputTag")
		});

	URpgInputConfig* DuplicateTagConfig = NewConfig();
	DuplicateTagConfig->NativeInputActions.Add(
		MakeInputMapping(
			NewAction(DuplicateTagConfig),
			RpgGameplayTags::InputTag_Jump));
	DuplicateTagConfig->NativeInputActions.Add(
		MakeInputMapping(
			NewAction(DuplicateTagConfig),
			RpgGameplayTags::InputTag_Jump));
	ExpectInvalid(
		TEXT("A duplicate tag within the native list"),
		*DuplicateTagConfig,
		1,
		{
			DuplicateTagConfig->GetPathName(),
			TEXT("NativeInputActions[1].InputTag"),
			TEXT("duplicates NativeInputActions[0].InputTag")
		});

	URpgInputConfig* ExactDuplicateConfig = NewConfig();
	UInputAction* ExactDuplicateAction =
		NewAction(ExactDuplicateConfig);
	ExactDuplicateConfig->AbilityInputActions.Add(
		MakeInputMapping(
			ExactDuplicateAction,
			RpgGameplayTags::InputTag_Weapon_Primary));
	ExactDuplicateConfig->AbilityInputActions.Add(
		MakeInputMapping(
			ExactDuplicateAction,
			RpgGameplayTags::InputTag_Weapon_Primary));
	ExpectInvalid(
		TEXT("An exact duplicate pair within the ability list"),
		*ExactDuplicateConfig,
		1,
		{
			ExactDuplicateConfig->GetPathName(),
			TEXT("AbilityInputActions[1]"),
			TEXT("exactly duplicates AbilityInputActions[0]"),
			TEXT("InputTag.Weapon.Primary")
		});

	URpgInputConfig* CrossListTagConfig = NewConfig();
	CrossListTagConfig->NativeInputActions.Add(
		MakeInputMapping(
			NewAction(CrossListTagConfig),
			RpgGameplayTags::InputTag_Jump));
	CrossListTagConfig->AbilityInputActions.Add(
		MakeInputMapping(
			NewAction(CrossListTagConfig),
			RpgGameplayTags::InputTag_Jump));
	ExpectInvalid(
		TEXT("A duplicate tag across native and ability lists"),
		*CrossListTagConfig,
		1,
		{
			CrossListTagConfig->GetPathName(),
			TEXT("AbilityInputActions[0].InputTag"),
			TEXT("duplicates NativeInputActions[0].InputTag")
		});

	URpgInputConfig* CrossListExactConfig = NewConfig();
	UInputAction* CrossListExactAction =
		NewAction(CrossListExactConfig);
	CrossListExactConfig->NativeInputActions.Add(
		MakeInputMapping(
			CrossListExactAction,
			RpgGameplayTags::InputTag_Jump));
	CrossListExactConfig->AbilityInputActions.Add(
		MakeInputMapping(
			CrossListExactAction,
			RpgGameplayTags::InputTag_Jump));
	ExpectInvalid(
		TEXT("An exact duplicate pair across both lists"),
		*CrossListExactConfig,
		1,
		{
			CrossListExactConfig->GetPathName(),
			TEXT("AbilityInputActions[0]"),
			TEXT("exactly duplicates NativeInputActions[0]"),
			TEXT("InputTag.Jump")
		});

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInputConfigAuthoredAssetValidationTest,
	"SurvivalRpg.Input.Config.Validation.AuthoredAssets",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInputConfigAuthoredAssetValidationTest::RunTest(
	const FString& Parameters)
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<
			FAssetRegistryModule>(
				TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	FARFilter InputConfigFilter;
	InputConfigFilter.ClassPaths.Add(
		URpgInputConfig::StaticClass()->
			GetClassPathName());
	InputConfigFilter.bRecursiveClasses = true;
	InputConfigFilter.bRecursivePaths = true;
	for (const FName ContentRoot :
		GetInputConfigContentRoots())
	{
		InputConfigFilter.PackagePaths.Add(ContentRoot);
	}

	TArray<FAssetData> InputConfigAssets;
	AssetRegistry.GetAssets(
		InputConfigFilter,
		InputConfigAssets);
	InputConfigAssets.Sort(
		[](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(
				Right.PackageName);
		});

	int32 LoadedConfigCount = 0;
	for (const FAssetData& InputConfigAsset :
		InputConfigAssets)
	{
		const FString AssetPath =
			InputConfigAsset.GetObjectPathString();
		const URpgInputConfig* InputConfig =
			Cast<URpgInputConfig>(
				InputConfigAsset.GetAsset());
		if (!InputConfig)
		{
			AddError(FString::Printf(
				TEXT(
					"%s is registered as an input config "
					"but did not load as URpgInputConfig"),
				*AssetPath));
			continue;
		}

		++LoadedConfigCount;
		const FInputConfigValidationSnapshot Snapshot =
			ValidateInputConfig(*InputConfig);
		for (const FString& Error : Snapshot.Errors)
		{
			AddError(FString::Printf(
				TEXT("%s: %s"),
				*AssetPath,
				*Error));
		}
		for (const FString& Warning : Snapshot.Warnings)
		{
			AddWarning(FString::Printf(
				TEXT("%s: %s"),
				*AssetPath,
				*Warning));
		}

		if (Snapshot.Result != EDataValidationResult::Valid &&
			Snapshot.Errors.IsEmpty())
		{
			AddError(FString::Printf(
				TEXT(
					"%s returned a non-valid result without "
					"a validation error"),
				*AssetPath));
		}
	}

	TestTrue(
		TEXT(
			"AssetRegistry found the project and combat "
			"input configs"),
		LoadedConfigCount >= 2);
	return true;
}

#endif
