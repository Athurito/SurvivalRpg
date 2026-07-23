#include "RpgUIScreenRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgActivatableWidget.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CommonActivatableWidget.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(
	TAG_RpgUIScreenRegistry_UnregisteredLayer,
	"UI.Layer.Automation.UnregisteredScreenLayer");

namespace
{
	FRpgUIScreenRegistryEntry MakeValidScreenEntry(
		const FGameplayTag ScreenTag)
	{
		FRpgUIScreenRegistryEntry Entry;
		Entry.ScreenTag = ScreenTag;
		Entry.LayerTag =
			RpgGameplayTags::UI_Layer_GameMenu;
		Entry.WidgetClass =
			UCommonActivatableWidget::StaticClass();
		return Entry;
	}

	bool HasIssueContaining(
		const FDataValidationContext& Context,
		const TCHAR* ExpectedText)
	{
		for (const FDataValidationContext::FIssue& Issue :
			Context.GetIssues())
		{
			if (Issue.Message.ToString().Contains(
				ExpectedText))
			{
				return true;
			}
		}

		return false;
	}

	TArray<FName> GetProjectContentRoots()
	{
		TArray<FName> ContentRoots = { FName(TEXT("/Game")) };
		for (const TSharedRef<IPlugin>& Plugin :
			IPluginManager::Get().GetEnabledPluginsWithContent())
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
	FRpgUIScreenRegistryAuthoredAssetValidationTest,
	"SurvivalRpg.UI.ScreenRegistry.Validation.AuthoredAsset",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryAuthoredAssetValidationTest::RunTest(
	const FString& Parameters)
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<
			FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	FARFilter RegistryFilter;
	RegistryFilter.ClassPaths.Add(
		URpgUIScreenRegistry::StaticClass()->
			GetClassPathName());
	RegistryFilter.bRecursiveClasses = true;
	RegistryFilter.bRecursivePaths = true;
	for (const FName ContentRoot :
		GetProjectContentRoots())
	{
		RegistryFilter.PackagePaths.Add(ContentRoot);
	}

	TArray<FAssetData> RegistryAssets;
	AssetRegistry.GetAssets(
		RegistryFilter,
		RegistryAssets);
	RegistryAssets.Sort(
		[](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(
				Right.PackageName);
		});

	int32 LoadedRegistryCount = 0;
	for (const FAssetData& RegistryAsset : RegistryAssets)
	{
		const FString AssetPath =
			RegistryAsset.GetObjectPathString();
		const URpgUIScreenRegistry* Registry =
			Cast<URpgUIScreenRegistry>(
				RegistryAsset.GetAsset());
		if (!Registry)
		{
			AddError(
				FString::Printf(
					TEXT(
						"%s is registered as a screen registry "
						"but did not load as URpgUIScreenRegistry"),
					*AssetPath));
			continue;
		}

		++LoadedRegistryCount;

		FDataValidationContext Context;
		const EDataValidationResult Result =
			Registry->IsDataValid(Context);
		for (const FDataValidationContext::FIssue& Issue :
			Context.GetIssues())
		{
			const FString PathAwareIssue =
				FString::Printf(
					TEXT("%s: %s"),
					*AssetPath,
					*Issue.Message.ToString());
			if (Issue.Severity == EMessageSeverity::Error)
			{
				AddError(PathAwareIssue);
			}
			else if (
				Issue.Severity ==
					EMessageSeverity::Warning)
			{
				AddWarning(PathAwareIssue);
			}
		}

		if (Result != EDataValidationResult::Valid &&
			Context.GetNumErrors() == 0)
		{
			AddError(
				FString::Printf(
					TEXT(
						"%s returned a non-valid result without "
						"a validation error"),
					*AssetPath));
		}

		for (int32 EntryIndex = 0;
			EntryIndex < Registry->Screens.Num();
			++EntryIndex)
		{
			const UClass* WidgetClass =
				Registry->Screens[EntryIndex].
					WidgetClass.LoadSynchronous();
			if (!TestNotNull(
				*FString::Printf(
					TEXT(
						"%s Screens[%d].WidgetClass loads"),
					*AssetPath,
					EntryIndex),
				WidgetClass))
			{
				continue;
			}

			TestTrue(
				*FString::Printf(
					TEXT(
						"%s Screens[%d].WidgetClass derives "
						"from CommonActivatableWidget"),
					*AssetPath,
					EntryIndex),
				WidgetClass->IsChildOf(
					UCommonActivatableWidget::StaticClass()));
			TestFalse(
				*FString::Printf(
					TEXT(
						"%s Screens[%d].WidgetClass is "
						"concrete"),
					*AssetPath,
					EntryIndex),
				WidgetClass->HasAnyClassFlags(
					CLASS_Abstract));
		}
	}

	TestTrue(
		TEXT(
			"AssetRegistry found and loaded at least one "
			"authored screen registry in project content"),
		LoadedRegistryCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenRegistryValidationDiagnosticsTest,
	"SurvivalRpg.UI.ScreenRegistry.Validation.Diagnostics",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryValidationDiagnosticsTest::RunTest(
	const FString& Parameters)
{
	URpgUIScreenRegistry* Registry =
		NewObject<URpgUIScreenRegistry>(
			GetTransientPackage());
	if (!TestNotNull(
		TEXT("Transient screen registry exists"),
		Registry))
	{
		return false;
	}

	FRpgUIScreenRegistryEntry LegacyValidEntry =
		MakeValidScreenEntry(
			RpgGameplayTags::UI_Screen_Inventory);
	LegacyValidEntry.bSingleInstance = false;
	Registry->Screens.Add(LegacyValidEntry);

	Registry->Screens.AddDefaulted();

	FRpgUIScreenRegistryEntry WrongNamespacesEntry;
	WrongNamespacesEntry.ScreenTag =
		RpgGameplayTags::UI_Layer_Game;
	WrongNamespacesEntry.LayerTag =
		RpgGameplayTags::UI_Screen_Storage;
	Registry->Screens.Add(WrongNamespacesEntry);

	Registry->Screens.Add(
		MakeValidScreenEntry(
			RpgGameplayTags::UI_Screen_Inventory));

	FRpgUIScreenRegistryEntry AbstractClassEntry =
		MakeValidScreenEntry(
			RpgGameplayTags::UI_Screen_Storage);
	AbstractClassEntry.WidgetClass =
		URpgActivatableWidget::StaticClass();
	Registry->Screens.Add(AbstractClassEntry);

	FRpgUIScreenRegistryEntry StaleObjectEntry =
		MakeValidScreenEntry(
			RpgGameplayTags::UI_Screen_Crafting);
	StaleObjectEntry.WidgetClass =
		TSoftClassPtr<UCommonActivatableWidget>(
			FSoftObjectPath(
				TEXT(
					"/Game/SurvivalRpg/UI/CUI_StorageSpatial."
					"StaleScreenClass_C")));
	Registry->Screens.Add(StaleObjectEntry);

	FDataValidationContext Context;
	const EDataValidationResult Result =
		Registry->IsDataValid(Context);
	TestTrue(
		TEXT("Malformed screen mappings invalidate the registry"),
		Result == EDataValidationResult::Invalid);
	TestEqual(
		TEXT("Every malformed field receives one focused error"),
		Context.GetNumErrors(),
		9u);
	TestEqual(
		TEXT(
			"Legacy multi-instance data receives one migration "
			"warning"),
		Context.GetNumWarnings(),
		1u);

	for (const TCHAR* ExpectedDiagnostic :
		{
			TEXT("below UI.Screen"),
			TEXT("must be a descendant of UI.Screen"),
			TEXT("duplicates Screens[0]"),
			TEXT("CommonUI layer owned by the root layout"),
			TEXT("must be a descendant of UI.Layer"),
			TEXT("WidgetClass is unset"),
			TEXT("is abstract"),
			TEXT("StaleScreenClass_C' could not be loaded"),
			TEXT("always single-instance")
		})
	{
		TestTrue(
			*FString::Printf(
				TEXT(
					"Validation provides an actionable "
					"diagnostic containing '%s'"),
				ExpectedDiagnostic),
			HasIssueContaining(
				Context,
				ExpectedDiagnostic));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenRegistryRegisteredLayerContractTest,
	"SurvivalRpg.UI.ScreenRegistry.Validation.RegisteredLayerContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryRegisteredLayerContractTest::RunTest(
	const FString& Parameters)
{
	URpgUIScreenRegistry* Registry =
		NewObject<URpgUIScreenRegistry>(
			GetTransientPackage());
	if (!TestNotNull(
		TEXT("Transient screen registry exists"),
		Registry))
	{
		return false;
	}

	const TPair<FGameplayTag, FGameplayTag> RegisteredMappings[] = {
		{
			RpgGameplayTags::UI_Screen_Inventory,
			RpgGameplayTags::UI_Layer_Game
		},
		{
			RpgGameplayTags::UI_Screen_Storage,
			RpgGameplayTags::UI_Layer_GameMenu
		},
		{
			RpgGameplayTags::UI_Screen_MainMenu,
			RpgGameplayTags::UI_Layer_Menu
		},
		{
			RpgGameplayTags::UI_Screen_Respawn,
			RpgGameplayTags::UI_Layer_Modal
		}
	};

	for (const TPair<FGameplayTag, FGameplayTag>& Mapping :
		RegisteredMappings)
	{
		FRpgUIScreenRegistryEntry Entry =
			MakeValidScreenEntry(Mapping.Key);
		Entry.LayerTag = Mapping.Value;
		Registry->Screens.Add(MoveTemp(Entry));
	}

	FDataValidationContext ValidContext;
	TestEqual(
		TEXT("All four registered root layers validate"),
		Registry->IsDataValid(ValidContext),
		EDataValidationResult::Valid);
	TestEqual(
		TEXT("Registered root layers emit no validation errors"),
		ValidContext.GetNumErrors(),
		0u);

	Registry->Screens[0].LayerTag =
		TAG_RpgUIScreenRegistry_UnregisteredLayer;
	FDataValidationContext UnsupportedContext;
	TestEqual(
		TEXT("A valid UI.Layer descendant not owned by the root layout is rejected"),
		Registry->IsDataValid(UnsupportedContext),
		EDataValidationResult::Invalid);
	TestEqual(
		TEXT("The unsupported layer receives one focused error"),
		UnsupportedContext.GetNumErrors(),
		1u);
	TestTrue(
		TEXT("The unsupported-layer diagnostic names root-layout registration"),
		HasIssueContaining(
			UnsupportedContext,
			TEXT("not registered by URpgPrimaryGameLayout")));

	return true;
}

#endif
