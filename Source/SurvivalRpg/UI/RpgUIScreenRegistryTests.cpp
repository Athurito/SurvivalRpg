#include "RpgUIScreenRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgBaseTerminalWidget.h"
#include "SurvivalRpg/UI/RpgCraftingStationWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryWidget.h"
#include "SurvivalRpg/UI/RpgStorageInventoryWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenSubsystem.h"
#include "SurvivalRpg/UI/RpgUISettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "CommonLocalPlayer.h"
#include "Components/PanelWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PrimaryGameLayout.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr TCHAR GameMenuClassPath[] =
		TEXT("/Game/SurvivalRpg/UI/Menus/GameMenu/GameMenu/CUI_GameMenu.CUI_GameMenu_C");
	constexpr TCHAR GameMenuPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/Menus/GameMenu/GameMenu/CUI_GameMenu");
	constexpr TCHAR PlayerInventoryPackageName[] =
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_PlayerInventory");
	constexpr TCHAR BaseTerminalSpatialPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseTerminalSpatial");
	constexpr TCHAR LegacyBaseTerminalPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseTerminal");
	constexpr TCHAR ScreenRegistryPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry");

	class FScopedUISettingsOverride
	{
	public:
		FScopedUISettingsOverride()
			: Settings(GetMutableDefault<URpgUISettings>())
		{
			if (Settings)
			{
				OriginalRegistry = Settings->ScreenRegistry;
				OriginalMappings = Settings->DefaultScreenMappings;
			}
		}

		~FScopedUISettingsOverride()
		{
			if (Settings)
			{
				Settings->ScreenRegistry = OriginalRegistry;
				Settings->DefaultScreenMappings = OriginalMappings;
			}
		}

		URpgUISettings* Get() const { return Settings; }

	private:
		URpgUISettings* Settings = nullptr;
		TSoftObjectPtr<URpgUIScreenRegistry> OriginalRegistry;
		TArray<FRpgUIScreenRegistryEntry> OriginalMappings;
	};

	TArray<FString> GetProjectContentRootPrefixes()
	{
		TArray<FString> ContentRoots = { TEXT("/Game/") };
		for (const TSharedRef<IPlugin>& Plugin :
			IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			if (!Plugin->IsMounted() ||
				Plugin->GetType() != EPluginType::Project)
			{
				continue;
			}

			FString MountedAssetPath = Plugin->GetMountedAssetPath();
			MountedAssetPath.RemoveFromEnd(TEXT("/"));
			if (!MountedAssetPath.IsEmpty())
			{
				ContentRoots.AddUnique(MountedAssetPath + TEXT("/"));
			}
		}
		return ContentRoots;
	}

	bool IsProjectContentPackage(
		const FString& PackageName,
		const TArray<FString>& ContentRootPrefixes)
	{
		for (const FString& ContentRootPrefix : ContentRootPrefixes)
		{
			if (PackageName.StartsWith(ContentRootPrefix))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryScreenFamilyClosureTest,
	"SurvivalRpg.UI.ScreenRegistry.InventoryScreenFamilyClosure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryScreenFamilyClosureTest::RunTest(
	const FString& Parameters)
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry"))
			.Get();
	AssetRegistry.WaitForCompletion();
	TSet<FTopLevelAssetPath> DerivedScreenClassPaths;
	AssetRegistry.GetDerivedClassNames(
		{
			URpgInventoryInteractionScreenWidget::StaticClass()
				->GetClassPathName()
		},
		{},
		DerivedScreenClassPaths);

	TSet<FString> ProjectScreenClassPaths;
	const TArray<FString> ProjectContentRootPrefixes =
		GetProjectContentRootPrefixes();
	for (const FTopLevelAssetPath& ClassPath : DerivedScreenClassPaths)
	{
		const FString PackageName =
			ClassPath.GetPackageName().ToString();
		const FString ClassPathString = ClassPath.ToString();
		const FString AuthoredGeneratedClassName =
			FPackageName::GetShortName(PackageName) + TEXT("_C");
		if (IsProjectContentPackage(
				PackageName,
				ProjectContentRootPrefixes) &&
			ClassPath.GetAssetName().ToString() ==
				AuthoredGeneratedClassName)
		{
			ProjectScreenClassPaths.Add(ClassPathString);
		}
	}

	const TSet<FString> ExpectedScreenClassPaths = {
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_PlayerInventory.CUI_PlayerInventory_C"),
		TEXT(
			"/Game/SurvivalRpg/UI/"
			"CUI_StorageSpatial.CUI_StorageSpatial_C"),
		TEXT(
			"/Game/SurvivalRpg/UI/"
			"CUI_BaseTerminalSpatial.CUI_BaseTerminalSpatial_C"),
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingStationSpatial.CUI_CraftingStationSpatial_C")
	};
	TestEqual(
		TEXT("Exactly four authored InventoryInteraction screens exist"),
		ProjectScreenClassPaths.Num(),
		ExpectedScreenClassPaths.Num());
	for (const FString& ExpectedClassPath : ExpectedScreenClassPaths)
	{
		TestTrue(
			*FString::Printf(
				TEXT("Expected inventory screen exists: %s"),
				*ExpectedClassPath),
			ProjectScreenClassPaths.Contains(ExpectedClassPath));
	}
	for (const FString& ActualClassPath : ProjectScreenClassPaths)
	{
		TestTrue(
			*FString::Printf(
				TEXT("Inventory screen belongs to the closed authored family: %s"),
				*ActualClassPath),
			ExpectedScreenClassPaths.Contains(ActualClassPath));
	}

	const URpgUIScreenRegistry* Registry =
		LoadObject<URpgUIScreenRegistry>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"DA_RpgUIScreenRegistry.DA_RpgUIScreenRegistry"));
	if (!TestNotNull(TEXT("UI screen registry loads"), Registry))
	{
		return false;
	}

	struct FExpectedSemanticRoute
	{
		FGameplayTag ScreenTag;
		const TCHAR* WidgetClassPath = nullptr;
	};
	const FExpectedSemanticRoute ExpectedRoutes[] = {
		{
			RpgGameplayTags::UI_Screen_Inventory,
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_PlayerInventory.CUI_PlayerInventory_C")
		},
		{
			RpgGameplayTags::UI_Screen_Storage,
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_StorageSpatial.CUI_StorageSpatial_C")
		},
		{
			RpgGameplayTags::UI_Screen_Loot,
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_StorageSpatial.CUI_StorageSpatial_C")
		},
		{
			RpgGameplayTags::UI_Screen_BaseTerminal,
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_BaseTerminalSpatial.CUI_BaseTerminalSpatial_C")
		},
		{
			RpgGameplayTags::UI_Screen_Crafting,
			TEXT(
				"/Game/SurvivalRpg/Crafting/UI/"
				"CUI_CraftingStationSpatial.CUI_CraftingStationSpatial_C")
		}
	};
	for (const FExpectedSemanticRoute& ExpectedRoute : ExpectedRoutes)
	{
		FRpgUIScreenRegistryEntry Entry;
		const bool bFound =
			Registry->FindScreen(ExpectedRoute.ScreenTag, Entry);
		TestTrue(
			*FString::Printf(
				TEXT("Semantic inventory route exists: %s"),
				*ExpectedRoute.ScreenTag.ToString()),
			bFound);
		if (!bFound)
		{
			continue;
		}

		TestEqual(
			*FString::Printf(
				TEXT("Semantic route keeps its canonical screen: %s"),
				*ExpectedRoute.ScreenTag.ToString()),
			Entry.WidgetClass.ToSoftObjectPath().ToString(),
			FString(ExpectedRoute.WidgetClassPath));
		TestTrue(
			*FString::Printf(
				TEXT("Semantic route uses the GameMenu layer: %s"),
				*ExpectedRoute.ScreenTag.ToString()),
			Entry.LayerTag == RpgGameplayTags::UI_Layer_GameMenu);
		TestTrue(
			*FString::Printf(
				TEXT("Semantic route suspends input while loading: %s"),
				*ExpectedRoute.ScreenTag.ToString()),
			Entry.bSuspendInputUntilLoaded);
		TestTrue(
			*FString::Printf(
				TEXT("Semantic route is single-instance: %s"),
				*ExpectedRoute.ScreenTag.ToString()),
			Entry.bSingleInstance);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenRegistryStorageSpatialMappingTest,
	"SurvivalRpg.UI.ScreenRegistry.StorageSpatialMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryStorageSpatialMappingTest::RunTest(const FString& Parameters)
{
	const URpgUIScreenRegistry* Registry = LoadObject<URpgUIScreenRegistry>(
		nullptr,
		TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry.DA_RpgUIScreenRegistry"));
	if (!TestNotNull(TEXT("UI screen registry loads"), Registry))
	{
		return false;
	}

	TSet<FGameplayTag> UniqueScreenTags;
	for (const FRpgUIScreenRegistryEntry& Candidate : Registry->Screens)
	{
		TestFalse(
			*FString::Printf(TEXT("Screen tag %s is not duplicated"), *Candidate.ScreenTag.ToString()),
			UniqueScreenTags.Contains(Candidate.ScreenTag));
		UniqueScreenTags.Add(Candidate.ScreenTag);
	}

	FRpgUIScreenRegistryEntry StorageEntry;
	if (!TestTrue(
		TEXT("Storage registry entry exists"),
		Registry->FindScreen(RpgGameplayTags::UI_Screen_Storage, StorageEntry)))
	{
		return false;
	}

	TestTrue(
		TEXT("Storage opens on the CommonUI game-menu layer"),
		StorageEntry.LayerTag == RpgGameplayTags::UI_Layer_GameMenu);
	TestEqual(
		TEXT("Storage registry points at the authored spatial screen"),
		StorageEntry.WidgetClass.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/SurvivalRpg/UI/CUI_StorageSpatial.CUI_StorageSpatial_C")));

	UClass* StorageClass = StorageEntry.WidgetClass.LoadSynchronous();
	TestTrue(
		TEXT("Mapped Storage class derives from the native Storage presenter"),
		StorageClass && StorageClass->IsChildOf(URpgStorageInventoryWidget::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenRegistryBaseTerminalSpatialMappingTest,
	"SurvivalRpg.UI.ScreenRegistry.BaseTerminalSpatialMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryBaseTerminalSpatialMappingTest::RunTest(
	const FString& Parameters)
{
	const URpgUIScreenRegistry* Registry = LoadObject<URpgUIScreenRegistry>(
		nullptr,
		TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry.DA_RpgUIScreenRegistry"));
	if (!TestNotNull(TEXT("UI screen registry loads"), Registry))
	{
		return false;
	}

	int32 BaseTerminalEntryCount = 0;
	for (const FRpgUIScreenRegistryEntry& Candidate : Registry->Screens)
	{
		BaseTerminalEntryCount +=
			Candidate.ScreenTag == RpgGameplayTags::UI_Screen_BaseTerminal ? 1 : 0;
	}
	TestEqual(
		TEXT("Base Terminal is authored exactly once in the screen registry"),
		BaseTerminalEntryCount,
		1);

	FRpgUIScreenRegistryEntry BaseTerminalEntry;
	if (!TestTrue(
		TEXT("Base Terminal registry entry exists"),
		Registry->FindScreen(
			RpgGameplayTags::UI_Screen_BaseTerminal,
			BaseTerminalEntry)))
	{
		return false;
	}

	TestTrue(
		TEXT("Base Terminal opens on the CommonUI game-menu layer"),
		BaseTerminalEntry.LayerTag == RpgGameplayTags::UI_Layer_GameMenu);
	TestTrue(
		TEXT("Base Terminal suspends owning-player input while its screen class streams"),
		BaseTerminalEntry.bSuspendInputUntilLoaded);
	TestTrue(
		TEXT("Base Terminal reuses its active CommonUI screen instance"),
		BaseTerminalEntry.bSingleInstance);
	TestEqual(
		TEXT("Base Terminal registry points at the authored spatial screen"),
		BaseTerminalEntry.WidgetClass.ToSoftObjectPath().ToString(),
		FString(TEXT(
			"/Game/SurvivalRpg/UI/CUI_BaseTerminalSpatial.CUI_BaseTerminalSpatial_C")));

	UClass* BaseTerminalClass = BaseTerminalEntry.WidgetClass.LoadSynchronous();
	TestTrue(
		TEXT("Mapped Base Terminal class derives from the native Base Terminal presenter"),
		BaseTerminalClass &&
			BaseTerminalClass->IsChildOf(URpgBaseTerminalWidget::StaticClass()));

	const IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FName> RegistryDependencies;
	TestTrue(
		TEXT("Asset Registry resolves UI screen registry dependencies"),
		AssetRegistry.GetDependencies(
			FName(ScreenRegistryPackageName),
			RegistryDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));
	TestTrue(
		TEXT("UI screen registry depends on the authored spatial Base Terminal"),
		RegistryDependencies.Contains(FName(BaseTerminalSpatialPackageName)));
	TestFalse(
		TEXT("UI screen registry no longer depends on the legacy Base Terminal"),
		RegistryDependencies.Contains(FName(LegacyBaseTerminalPackageName)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenRegistryCraftingSpatialMappingTest,
	"SurvivalRpg.UI.ScreenRegistry.CraftingSpatialMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryCraftingSpatialMappingTest::RunTest(
	const FString& Parameters)
{
	const URpgUIScreenRegistry* Registry = LoadObject<URpgUIScreenRegistry>(
		nullptr,
		TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry.DA_RpgUIScreenRegistry"));
	if (!TestNotNull(TEXT("UI screen registry loads"), Registry))
	{
		return false;
	}

	int32 CraftingEntryCount = 0;
	for (const FRpgUIScreenRegistryEntry& Candidate : Registry->Screens)
	{
		CraftingEntryCount +=
			Candidate.ScreenTag == RpgGameplayTags::UI_Screen_Crafting ? 1 : 0;
	}
	TestEqual(
		TEXT("Crafting is authored exactly once in the screen registry"),
		CraftingEntryCount,
		1);

	FRpgUIScreenRegistryEntry CraftingEntry;
	if (!TestTrue(
		TEXT("Crafting registry entry exists"),
		Registry->FindScreen(
			RpgGameplayTags::UI_Screen_Crafting,
			CraftingEntry)))
	{
		return false;
	}

	TestTrue(
		TEXT("Crafting opens on the CommonUI game-menu layer"),
		CraftingEntry.LayerTag == RpgGameplayTags::UI_Layer_GameMenu);
	TestTrue(
		TEXT("Crafting suspends owning-player input while its screen class streams"),
		CraftingEntry.bSuspendInputUntilLoaded);
	TestTrue(
		TEXT("Crafting reuses its active CommonUI screen instance"),
		CraftingEntry.bSingleInstance);
	TestEqual(
		TEXT("Crafting registry points at the authored spatial screen"),
		CraftingEntry.WidgetClass.ToSoftObjectPath().ToString(),
		FString(TEXT(
			"/Game/SurvivalRpg/Crafting/UI/CUI_CraftingStationSpatial.CUI_CraftingStationSpatial_C")));

	UClass* CraftingClass = CraftingEntry.WidgetClass.LoadSynchronous();
	TestTrue(
		TEXT("Mapped Crafting class derives from the native Crafting presenter"),
		CraftingClass &&
			CraftingClass->IsChildOf(URpgCraftingStationWidget::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenRegistryLootSpatialMappingTest,
	"SurvivalRpg.UI.ScreenRegistry.LootSpatialMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryLootSpatialMappingTest::RunTest(const FString& Parameters)
{
	const URpgUIScreenRegistry* Registry = LoadObject<URpgUIScreenRegistry>(
		nullptr,
		TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry.DA_RpgUIScreenRegistry"));
	if (!TestNotNull(TEXT("UI screen registry loads"), Registry))
	{
		return false;
	}

	int32 LootEntryCount = 0;
	for (const FRpgUIScreenRegistryEntry& Candidate : Registry->Screens)
	{
		LootEntryCount += Candidate.ScreenTag == RpgGameplayTags::UI_Screen_Loot ? 1 : 0;
	}
	TestEqual(
		TEXT("Loot is authored exactly once instead of relying on a runtime alias"),
		LootEntryCount,
		1);

	FRpgUIScreenRegistryEntry LootEntry;
	if (!TestTrue(
		TEXT("Explicit Loot registry entry exists"),
		Registry->FindScreen(RpgGameplayTags::UI_Screen_Loot, LootEntry)))
	{
		return false;
	}

	TestTrue(
		TEXT("Loot opens on the CommonUI game-menu layer"),
		LootEntry.LayerTag == RpgGameplayTags::UI_Layer_GameMenu);
	TestTrue(
		TEXT("Loot mapping preserves its own semantic screen identity"),
		LootEntry.ScreenTag == RpgGameplayTags::UI_Screen_Loot);
	TestTrue(
		TEXT("Loot suspends owning-player input while its screen class streams"),
		LootEntry.bSuspendInputUntilLoaded);
	TestTrue(
		TEXT("Loot reuses its active CommonUI screen instance"),
		LootEntry.bSingleInstance);
	TestEqual(
		TEXT("Loot explicitly selects the authored dual-inventory spatial screen"),
		LootEntry.WidgetClass.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/SurvivalRpg/UI/CUI_StorageSpatial.CUI_StorageSpatial_C")));

	UClass* LootClass = LootEntry.WidgetClass.LoadSynchronous();
	TestTrue(
		TEXT("Mapped Loot class derives from the native Storage/Loot presenter"),
		LootClass && LootClass->IsChildOf(URpgStorageInventoryWidget::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenRegistryExactResolutionTest,
	"SurvivalRpg.UI.ScreenRegistry.ExactResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryExactResolutionTest::RunTest(const FString& Parameters)
{
	FScopedUISettingsOverride SettingsOverride;
	URpgUISettings* Settings = SettingsOverride.Get();
	if (!TestNotNull(TEXT("Engine exists for the LocalPlayer-owned test subsystem"), GEngine))
	{
		return false;
	}
	UCommonLocalPlayer* LocalPlayer =
		NewObject<UCommonLocalPlayer>(GEngine);
	URpgUIScreenSubsystem* ScreenSubsystem =
		NewObject<URpgUIScreenSubsystem>(LocalPlayer);
	if (!TestNotNull(TEXT("Mutable UI settings exist"), Settings) ||
		!TestNotNull(TEXT("Transient CommonLocalPlayer exists"), LocalPlayer) ||
		!TestNotNull(TEXT("Transient screen subsystem exists"), ScreenSubsystem))
	{
		return false;
	}

	FRpgUIScreenRegistryEntry StorageEntry;
	StorageEntry.ScreenTag = RpgGameplayTags::UI_Screen_Storage;
	StorageEntry.LayerTag = RpgGameplayTags::UI_Layer_GameMenu;

	URpgUIScreenRegistry* StorageOnlyRegistry =
		NewObject<URpgUIScreenRegistry>(GetTransientPackage());
	StorageOnlyRegistry->Screens.Add(StorageEntry);
	Settings->ScreenRegistry = StorageOnlyRegistry;
	Settings->DefaultScreenMappings.Reset();

	FRpgUIScreenRegistryEntry ResolvedEntry;
	TestFalse(
		TEXT("A Storage-only registry cannot resolve Loot through a hidden alias"),
		ScreenSubsystem->ResolveScreenEntry(
			RpgGameplayTags::UI_Screen_Loot,
			ResolvedEntry));

	Settings->ScreenRegistry.Reset();
	Settings->DefaultScreenMappings.Add(StorageEntry);
	TestFalse(
		TEXT("A Storage-only config fallback cannot resolve Loot through a hidden alias"),
		ScreenSubsystem->ResolveScreenEntry(
			RpgGameplayTags::UI_Screen_Loot,
			ResolvedEntry));

	FRpgUIScreenRegistryEntry LootEntry = StorageEntry;
	LootEntry.ScreenTag = RpgGameplayTags::UI_Screen_Loot;
	Settings->DefaultScreenMappings.Add(LootEntry);
	TestTrue(
		TEXT("An exact Loot fallback mapping resolves normally"),
		ScreenSubsystem->ResolveScreenEntry(
			RpgGameplayTags::UI_Screen_Loot,
			ResolvedEntry));
	TestTrue(
		TEXT("Exact fallback resolution preserves the requested Loot identity"),
		ResolvedEntry.ScreenTag == RpgGameplayTags::UI_Screen_Loot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenAsyncCloseLifecycleTest,
	"SurvivalRpg.UI.ScreenRouter.AsyncCloseLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenAsyncCloseLifecycleTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine exists for the LocalPlayer-owned test subsystem"), GEngine))
	{
		return false;
	}

	UCommonLocalPlayer* LocalPlayer = NewObject<UCommonLocalPlayer>(GEngine);
	URpgUIScreenSubsystem* ScreenSubsystem = NewObject<URpgUIScreenSubsystem>(LocalPlayer);
	if (!TestNotNull(TEXT("Transient CommonLocalPlayer exists"), LocalPlayer) ||
		!TestNotNull(TEXT("Transient screen subsystem exists"), ScreenSubsystem))
	{
		return false;
	}

	const FGameplayTag ScreenTag = RpgGameplayTags::UI_Screen_MainMenu;
	UCommonActivatableWidget* WidgetBeforeInitialize =
		NewObject<UCommonActivatableWidget>(LocalPlayer);
	FStreamableManager& StreamableManager =
		UAssetManager::Get().GetStreamableManager();
	TSharedPtr<FStreamableHandle> CompletedHandle =
		StreamableManager.RequestSyncLoad(
			FSoftObjectPath(
				TEXT(
					"/Game/SurvivalRpg/UI/Menus/BootMenu/"
					"CUI_BootMenu.CUI_BootMenu_C")));
	if (!TestTrue(
			TEXT("A completed streamable handle exists for the close-race test"),
			CompletedHandle.IsValid() &&
				CompletedHandle->HasLoadCompleted()))
	{
		return false;
	}

	ScreenSubsystem->PendingScreenTags.Add(ScreenTag);
	ScreenSubsystem->PendingScreenLoads.Add(
		ScreenTag,
		CompletedHandle);
	ScreenSubsystem->CloseScreen(ScreenTag);
	TestFalse(
		TEXT("Close preserves the queued completion callback of an already-loaded screen"),
		CompletedHandle->WasCanceled());
	TestTrue(
		TEXT("Closing a streaming screen keeps the tag pending until a terminal callback"),
		ScreenSubsystem->PendingScreenTags.Contains(ScreenTag));
	TestTrue(
		TEXT("Closing a streaming screen records cancellation"),
		ScreenSubsystem->CanceledPendingScreenTags.Contains(ScreenTag));

	ScreenSubsystem->HandleScreenPushState(
		ScreenTag,
		EAsyncWidgetLayerState::Initialize,
		WidgetBeforeInitialize);
	TestFalse(
		TEXT("A canceled screen is never tracked during CommonGame initialization"),
		ScreenSubsystem->ActiveScreens.Contains(ScreenTag));
	TestTrue(
		TEXT("Initialize does not release the canceled request for a same-tag reopen"),
		ScreenSubsystem->PendingScreenTags.Contains(ScreenTag));

	ScreenSubsystem->HandleScreenPushState(
		ScreenTag,
		EAsyncWidgetLayerState::AfterPush,
		WidgetBeforeInitialize);
	TestFalse(
		TEXT("AfterPush clears the canceled request"),
		ScreenSubsystem->PendingScreenTags.Contains(ScreenTag));
	TestFalse(
		TEXT("AfterPush clears the cancellation marker"),
		ScreenSubsystem->CanceledPendingScreenTags.Contains(ScreenTag));
	TestFalse(
		TEXT("AfterPush leaves no canceled widget tracked"),
		ScreenSubsystem->ActiveScreens.Contains(ScreenTag));
	CompletedHandle->ReleaseHandle();

	UCommonActivatableWidget* WidgetDuringInitialize =
		NewObject<UCommonActivatableWidget>(LocalPlayer);
	ScreenSubsystem->PendingScreenTags.Add(ScreenTag);
	ScreenSubsystem->HandleScreenPushState(
		ScreenTag,
		EAsyncWidgetLayerState::Initialize,
		WidgetDuringInitialize);
	TestTrue(
		TEXT("A live request is tracked during CommonGame initialization"),
		ScreenSubsystem->ActiveScreens.FindRef(ScreenTag) == WidgetDuringInitialize);

	// The CommonGame container has not activated the widget yet, so CloseScreen
	// must treat this as pending rather than as an active instance.
	ScreenSubsystem->CloseScreen(ScreenTag);
	TestTrue(
		TEXT("Close during initialization retains request identity"),
		ScreenSubsystem->PendingScreenTags.Contains(ScreenTag));
	TestTrue(
		TEXT("Close during initialization records cancellation"),
		ScreenSubsystem->CanceledPendingScreenTags.Contains(ScreenTag));

	ScreenSubsystem->HandleScreenPushState(
		ScreenTag,
		EAsyncWidgetLayerState::AfterPush,
		WidgetDuringInitialize);
	TestFalse(
		TEXT("Terminal completion clears the initializing request"),
		ScreenSubsystem->PendingScreenTags.Contains(ScreenTag));
	TestFalse(
		TEXT("Terminal completion removes its initializing widget"),
		ScreenSubsystem->ActiveScreens.Contains(ScreenTag));
	TestFalse(
		TEXT("A canceled never-activated widget releases its router callback"),
		WidgetDuringInitialize->OnDeactivated().IsBoundToObject(
			ScreenSubsystem));

	const FGameplayTag StalledScreenTag =
		RpgGameplayTags::UI_Screen_Boot;
	TSharedPtr<FStreamableHandle> StalledHandle =
		StreamableManager.RequestAsyncLoad(
			FSoftObjectPath(
				TEXT(
					"/Game/SurvivalRpg/UI/Menus/BootMenu/"
					"CUI_BootMenu.CUI_BootMenu_C")),
			FStreamableDelegate(),
			FStreamableManager::DefaultAsyncLoadPriority,
			/*bManageActiveHandle=*/ false,
			/*bStartStalled=*/ true);
	if (!TestTrue(
			TEXT("A stalled streamable handle exists for the cancel test"),
			StalledHandle.IsValid() &&
				!StalledHandle->HasLoadCompleted()))
	{
		return false;
	}

	ScreenSubsystem->PendingScreenTags.Add(StalledScreenTag);
	ScreenSubsystem->PendingScreenLoads.Add(
		StalledScreenTag,
		StalledHandle);
	ScreenSubsystem->CloseScreen(StalledScreenTag);
	TestTrue(
		TEXT("Close cancels an incomplete streamable request"),
		StalledHandle->WasCanceled());
	TestTrue(
		TEXT("A canceled load stays pending until its delayed terminal callback"),
		ScreenSubsystem->PendingScreenTags.Contains(
			StalledScreenTag));

	// CommonGame binds this terminal state to the handle's cancel delegate.
	// Streamable delegates are frame-delayed by default, so exercise the state
	// transition explicitly after proving that the real handle was canceled.
	ScreenSubsystem->HandleScreenPushState(
		StalledScreenTag,
		EAsyncWidgetLayerState::Canceled,
		nullptr);
	TestFalse(
		TEXT("The incomplete request no longer blocks a same-tag reopen"),
		ScreenSubsystem->PendingScreenTags.Contains(
			StalledScreenTag));
	TestFalse(
		TEXT("The incomplete request releases its streamable handle"),
		ScreenSubsystem->PendingScreenLoads.Contains(
			StalledScreenTag));

	const FGameplayTag FirstPooledTag =
		RpgGameplayTags::UI_Screen_Inventory;
	const FGameplayTag SecondPooledTag =
		RpgGameplayTags::UI_Screen_Storage;
	UCommonActivatableWidget* PooledWidget =
		NewObject<UCommonActivatableWidget>(LocalPlayer);
	ScreenSubsystem->HandleScreenPushState(
		FirstPooledTag,
		EAsyncWidgetLayerState::Initialize,
		PooledWidget);
	TestTrue(
		TEXT("A pooled screen checkout owns one router deactivation callback"),
		PooledWidget->OnDeactivated().IsBoundToObject(ScreenSubsystem));

	bool bReusedDuringFirstDeactivation = false;
	bool bObservedOverlappingCheckoutBindings = false;
	const FDelegateHandle ReuseDuringDeactivationHandle =
		PooledWidget->OnDeactivated().AddLambda(
			[&]()
			{
				if (bReusedDuringFirstDeactivation)
				{
					return;
				}

				bReusedDuringFirstDeactivation = true;
				ScreenSubsystem->HandleScreenPushState(
					SecondPooledTag,
					EAsyncWidgetLayerState::Initialize,
					PooledWidget);
				bObservedOverlappingCheckoutBindings =
					ScreenSubsystem->ScreenDeactivationBindings.Num() == 2;
			});
	PooledWidget->ActivateWidget();
	PooledWidget->DeactivateWidget();
	TestTrue(
		TEXT("The deactivation callback synchronously checks out the same pooled widget"),
		bReusedDuringFirstDeactivation);
	TestTrue(
		TEXT("Reentrant reuse preserves separate old and new checkout callbacks until each completes"),
		bObservedOverlappingCheckoutBindings);
	TestFalse(
		TEXT("The first pooled screen tag is no longer tracked"),
		ScreenSubsystem->ActiveScreens.Contains(FirstPooledTag));
	TestTrue(
		TEXT("The reentrant second checkout remains tracked after the first callback completes"),
		ScreenSubsystem->ActiveScreens.FindRef(SecondPooledTag) ==
			PooledWidget);
	TestTrue(
		TEXT("The reentrant second checkout retains its own router callback"),
		PooledWidget->OnDeactivated().IsBoundToObject(ScreenSubsystem));

	PooledWidget->ActivateWidget();
	PooledWidget->DeactivateWidget();
	TestFalse(
		TEXT("The second pooled screen tag is released normally"),
		ScreenSubsystem->ActiveScreens.Contains(SecondPooledTag));
	TestFalse(
		TEXT("The second pooled checkout also releases its router callback"),
		PooledWidget->OnDeactivated().IsBoundToObject(ScreenSubsystem));
	PooledWidget->OnDeactivated().Remove(
		ReuseDuringDeactivationHandle);

	const FGameplayTag ReentrantSameTag =
		RpgGameplayTags::UI_Screen_BaseTerminal;
	UCommonActivatableWidget* SameTagPooledWidget =
		NewObject<UCommonActivatableWidget>(LocalPlayer);
	ScreenSubsystem->HandleScreenPushState(
		ReentrantSameTag,
		EAsyncWidgetLayerState::Initialize,
		SameTagPooledWidget);
	bool bReusedForSameTagDuringDeactivation = false;
	bool bObservedSameTagCheckoutOverlap = false;
	const FDelegateHandle SameTagReuseHandle =
		SameTagPooledWidget->OnDeactivated().AddLambda(
			[&]()
			{
				if (bReusedForSameTagDuringDeactivation)
				{
					return;
				}

				bReusedForSameTagDuringDeactivation = true;
				ScreenSubsystem->HandleScreenPushState(
					ReentrantSameTag,
					EAsyncWidgetLayerState::Initialize,
					SameTagPooledWidget);
				bObservedSameTagCheckoutOverlap =
					ScreenSubsystem->ScreenDeactivationBindings.Num() == 2;
			});
	SameTagPooledWidget->ActivateWidget();
	SameTagPooledWidget->DeactivateWidget();
	TestTrue(
		TEXT("Same-tag reentrant reuse overlaps two checkout callbacks"),
		bObservedSameTagCheckoutOverlap);
	TestTrue(
		TEXT("An old same-tag callback cannot clear the newer checkout"),
		ScreenSubsystem->ActiveScreens.FindRef(ReentrantSameTag) ==
			SameTagPooledWidget);
	TestTrue(
		TEXT("The newer same-tag checkout retains its router callback"),
		SameTagPooledWidget->OnDeactivated().IsBoundToObject(
			ScreenSubsystem));
	SameTagPooledWidget->ActivateWidget();
	SameTagPooledWidget->DeactivateWidget();
	TestFalse(
		TEXT("The newer same-tag checkout releases normally"),
		ScreenSubsystem->ActiveScreens.Contains(ReentrantSameTag));
	TestFalse(
		TEXT("Same-tag reuse leaves no router callback behind"),
		SameTagPooledWidget->OnDeactivated().IsBoundToObject(
			ScreenSubsystem));
	SameTagPooledWidget->OnDeactivated().Remove(
		SameTagReuseHandle);

	UCommonActivatableWidget* TeardownWidget =
		NewObject<UCommonActivatableWidget>(LocalPlayer);
	ScreenSubsystem->HandleScreenPushState(
		RpgGameplayTags::UI_Screen_Loot,
		EAsyncWidgetLayerState::Initialize,
		TeardownWidget);
	TeardownWidget->ActivateWidget();
	TestTrue(
		TEXT("The teardown fixture starts activated"),
		TeardownWidget->IsActivated());
	TestTrue(
		TEXT("The teardown fixture owns a router callback"),
		TeardownWidget->OnDeactivated().IsBoundToObject(ScreenSubsystem));

	ScreenSubsystem->Deinitialize();
	TestFalse(
		TEXT("Subsystem teardown deactivates its active screen"),
		TeardownWidget->IsActivated());
	TestFalse(
		TEXT("Subsystem teardown removes its callback from a pool-retained widget"),
		TeardownWidget->OnDeactivated().IsBoundToObject(ScreenSubsystem));

	UCommonActivatableWidget* LateInitializeWidget =
		NewObject<UCommonActivatableWidget>(LocalPlayer);
	ScreenSubsystem->HandleScreenPushState(
		RpgGameplayTags::UI_Screen_Crafting,
		EAsyncWidgetLayerState::Initialize,
		LateInitializeWidget);
	TestFalse(
		TEXT("A push initialized during teardown is never tracked"),
		ScreenSubsystem->ActiveScreens.Contains(
			RpgGameplayTags::UI_Screen_Crafting));
	TestFalse(
		TEXT("A push initialized during teardown never binds a router callback"),
		LateInitializeWidget->OnDeactivated().IsBoundToObject(
			ScreenSubsystem));

	LateInitializeWidget->ActivateWidget();
	ScreenSubsystem->HandleScreenPushState(
		RpgGameplayTags::UI_Screen_Crafting,
		EAsyncWidgetLayerState::AfterPush,
		LateInitializeWidget);
	TestFalse(
		TEXT("A late terminal push is deactivated during subsystem teardown"),
		LateInitializeWidget->IsActivated());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIScreenRegistryPlayerInventoryMappingTest,
	"SurvivalRpg.UI.ScreenRegistry.PlayerInventoryMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUIScreenRegistryPlayerInventoryMappingTest::RunTest(const FString& Parameters)
{
	const URpgUIScreenRegistry* Registry = LoadObject<URpgUIScreenRegistry>(
		nullptr,
		TEXT("/Game/SurvivalRpg/UI/DA_RpgUIScreenRegistry.DA_RpgUIScreenRegistry"));
	if (!TestNotNull(TEXT("UI screen registry loads"), Registry))
	{
		return false;
	}

	FRpgUIScreenRegistryEntry PlayerInventoryEntry;
	if (!TestTrue(
		TEXT("Player inventory registry entry exists"),
		Registry->FindScreen(RpgGameplayTags::UI_Screen_Inventory, PlayerInventoryEntry)))
	{
		return false;
	}

	TestTrue(
		TEXT("Player inventory opens on the CommonUI game-menu layer"),
		PlayerInventoryEntry.LayerTag == RpgGameplayTags::UI_Layer_GameMenu);
	TestTrue(
		TEXT("Player inventory reuses its active CommonUI screen instance"),
		PlayerInventoryEntry.bSingleInstance);
	TestEqual(
		TEXT("Player inventory registry points at the authored inventory screen"),
		PlayerInventoryEntry.WidgetClass.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_PlayerInventory.CUI_PlayerInventory_C")));

	UClass* PlayerInventoryClass = PlayerInventoryEntry.WidgetClass.LoadSynchronous();
	TestTrue(
		TEXT("Mapped Player Inventory class derives from the native Player Inventory presenter"),
		PlayerInventoryClass && PlayerInventoryClass->IsChildOf(URpgPlayerInventoryWidget::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgUIPlayerInventoryCompositionAuthorityTest,
	"SurvivalRpg.UI.CompositionAuthority.PlayerInventoryRegistryOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgUIPlayerInventoryCompositionAuthorityTest::RunTest(const FString& Parameters)
{
	UClass* GameMenuClass = LoadClass<UUserWidget>(nullptr, GameMenuClassPath);
	if (!TestNotNull(TEXT("Legacy Game Menu class loads"), GameMenuClass))
	{
		return false;
	}

	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(GameMenuClass);
	if (!TestNotNull(TEXT("Legacy Game Menu is an authored Widget Blueprint"), GeneratedClass))
	{
		return false;
	}

	const UWidgetTree* WidgetTree = GeneratedClass->GetWidgetTreeArchetype();
	if (!TestNotNull(TEXT("Legacy Game Menu has a compiled widget tree"), WidgetTree))
	{
		return false;
	}

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for (const UWidget* Widget : Widgets)
	{
		TestFalse(
			*FString::Printf(
				TEXT("Legacy Game Menu widget '%s' is not a Player Inventory presenter"),
				*GetNameSafe(Widget)),
			Widget && Widget->IsA<URpgPlayerInventoryWidget>());
	}

	TestNull(
		TEXT("Legacy Game Menu no longer generates a CUI_PlayerInventory member"),
		FindFProperty<FObjectPropertyBase>(GameMenuClass, TEXT("CUI_PlayerInventory")));

	const UPanelWidget* Switcher = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("Switcher")));
	if (!TestNotNull(TEXT("Legacy Game Menu retains its authored Switcher"), Switcher))
	{
		return false;
	}

	const TArray<FName> ExpectedPages = {
		TEXT("CUI_Map"),
		TEXT("CUI_Journal"),
		TEXT("CUI_Character"),
		TEXT("CUI_Skills")
	};
	TestEqual(
		TEXT("Legacy Game Menu retains exactly the four non-inventory pages"),
		Switcher->GetChildrenCount(),
		ExpectedPages.Num());
	for (int32 PageIndex = 0; PageIndex < ExpectedPages.Num() && PageIndex < Switcher->GetChildrenCount(); ++PageIndex)
	{
		TestEqual(
			*FString::Printf(TEXT("Legacy Game Menu page %d keeps its semantic order"), PageIndex),
			Switcher->GetChildAt(PageIndex)->GetFName(),
			ExpectedPages[PageIndex]);
	}

	const FArrayProperty* TabNamesProperty =
		FindFProperty<FArrayProperty>(GameMenuClass, TEXT("TabButtonNames"));
	const FNameProperty* TabNameProperty =
		TabNamesProperty ? CastField<FNameProperty>(TabNamesProperty->Inner) : nullptr;
	if (!TestNotNull(TEXT("Legacy Game Menu retains its TabButtonNames array"), TabNamesProperty) ||
		!TestNotNull(TEXT("TabButtonNames remains an array of names"), TabNameProperty))
	{
		return false;
	}

	UObject* GameMenuDefaultObject = GameMenuClass->GetDefaultObject();
	FScriptArrayHelper TabNames(
		TabNamesProperty,
		TabNamesProperty->ContainerPtrToValuePtr<void>(GameMenuDefaultObject));
	TestEqual(
		TEXT("Tab names and authored Switcher pages have matching counts"),
		TabNames.Num(),
		ExpectedPages.Num());

	const TArray<FName> ExpectedTabNames = {
		TEXT("Map"),
		TEXT("Journal"),
		TEXT("Character"),
		TEXT("Skills")
	};
	for (int32 TabIndex = 0; TabIndex < ExpectedTabNames.Num() && TabIndex < TabNames.Num(); ++TabIndex)
	{
		TestEqual(
			*FString::Printf(TEXT("Legacy Game Menu tab %d matches its page"), TabIndex),
			TabNameProperty->GetPropertyValue(TabNames.GetRawPtr(TabIndex)),
			ExpectedTabNames[TabIndex]);
	}

	const IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FName> GameMenuDependencies;
	TestTrue(
		TEXT("Asset Registry resolves Legacy Game Menu dependencies"),
		AssetRegistry.GetDependencies(
			FName(GameMenuPackageName),
			GameMenuDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));
	TestFalse(
		TEXT("Legacy Game Menu no longer depends on CUI_PlayerInventory"),
		GameMenuDependencies.Contains(FName(PlayerInventoryPackageName)));

	TArray<FName> RegistryDependencies;
	TestTrue(
		TEXT("Asset Registry resolves UI screen registry dependencies"),
		AssetRegistry.GetDependencies(
			FName(ScreenRegistryPackageName),
			RegistryDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));
	TestTrue(
		TEXT("UI screen registry remains the Player Inventory composition authority"),
		RegistryDependencies.Contains(FName(PlayerInventoryPackageName)));

	return true;
}

#endif
