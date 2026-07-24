#include "RpgPlayerInventoryWidget.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Blueprint/IUserListEntry.h"
#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgLoadoutViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgActionBarSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryCarrySlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ICommonInputModule.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"

namespace RpgPlayerInventoryWidgetTests
{
	class FScopedWidgetWorld
	{
	public:
		FScopedWidgetWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedWidgetWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}

			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}

			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		bool IsValid() const
		{
			return World != nullptr;
		}

		UWorld* GetTestWorld() const
		{
			return World;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	int32 CountDirectPlayerInventoryViewModels(const UObject* Outer)
	{
		TArray<UObject*> DirectChildren;
		GetObjectsWithOuter(Outer, DirectChildren, EGetObjectsFlags::None);

		int32 Count = 0;
		for (const UObject* Candidate : DirectChildren)
		{
			if (Candidate && Candidate->IsA<URpgPlayerInventoryViewModel>())
			{
				++Count;
			}
		}
		return Count;
	}

	template <typename DelegateType>
	int32 CountDelegateBindingsTo(const DelegateType& Delegate, const UObject* Target)
	{
		int32 Count = 0;
		for (const UObject* BoundObject : Delegate.GetAllObjects())
		{
			if (BoundObject == Target)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 FindNamedViewModelSource(
		const UMVVMViewClass& ViewClass,
		FName SourceName,
		int32& OutMatchingSourceCount)
	{
		OutMatchingSourceCount = 0;
		int32 MatchingSourceIndex = INDEX_NONE;
		const TArrayView<const FMVVMViewClass_Source> Sources = ViewClass.GetSources();
		for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
		{
			const FMVVMViewClass_Source& Source = Sources[SourceIndex];
			if (Source.IsViewModel() && Source.GetName() == SourceName)
			{
				MatchingSourceIndex = SourceIndex;
				++OutMatchingSourceCount;
			}
		}
		return MatchingSourceIndex;
	}

	void PopulateExactEquipmentSourceSnapshot(
		FRpgInventoryDragPayload& Payload,
		UObject* Outer,
		FName ContainerId)
	{
		Payload.SourceInventory = NewObject<URpgInventoryManagerComponent>(Outer);
		Payload.EntryId = FGuid::NewGuid();
		Payload.StackCount = 1;
		Payload.SourcePlacement.SetContainerHandle(
			FRpgInventoryContainerHandle::MakeRoot(ContainerId));
		Payload.SourcePlacement.X = 0;
		Payload.SourcePlacement.Y = 0;
		Payload.SourcePlacement.Width = 1;
		Payload.SourcePlacement.Height = 1;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCanonicalDragVisualContractTest,
	"SurvivalRpg.Inventory.UI.CanonicalDragVisualContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCanonicalDragVisualContractTest::RunTest(const FString& Parameters)
{
	UClass* CanonicalVisualClass = LoadClass<URpgInventoryDragVisualWidget>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_InventoryDragVisual.CUI_InventoryDragVisual_C"));
	if (!TestNotNull(TEXT("Canonical authored drag-visual class loads"), CanonicalVisualClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Canonical authored class derives from the presentation-only native drag visual"),
		CanonicalVisualClass->IsChildOf(URpgInventoryDragVisualWidget::StaticClass()));

	const UWidgetBlueprintGeneratedClass* CanonicalGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(CanonicalVisualClass);
	const UWidgetTree* CanonicalWidgetTree =
		CanonicalGeneratedClass
			? CanonicalGeneratedClass->GetWidgetTreeArchetype()
			: nullptr;
	if (!TestNotNull(
		TEXT("Canonical drag visual is an authored Widget Blueprint"),
		CanonicalGeneratedClass) ||
		!TestNotNull(
			TEXT("Canonical drag visual owns an authored widget tree"),
			CanonicalWidgetTree))
	{
		return false;
	}
	TestTrue(
		TEXT("Canonical drag visual authors RootSizeBox"),
		Cast<USizeBox>(CanonicalWidgetTree->FindWidget(TEXT("RootSizeBox"))) != nullptr);
	TestTrue(
		TEXT("Canonical drag visual authors ItemIcon"),
		Cast<UImage>(CanonicalWidgetTree->FindWidget(TEXT("ItemIcon"))) != nullptr);
	TestTrue(
		TEXT("Canonical drag visual authors StackCountText"),
		Cast<UTextBlock>(CanonicalWidgetTree->FindWidget(TEXT("StackCountText"))) != nullptr);

	struct FNativePropertyExpectation
	{
		const TCHAR* Label;
		UClass* OwnerClass;
		const TCHAR* PropertyName;
	};
	const FNativePropertyExpectation NativeProperties[] = {
		{
			TEXT("Equipment drag visual"),
			URpgEquipmentSlotWidget::StaticClass(),
			TEXT("DragVisualClass")
		},
		{
			TEXT("Address/carry drag visual"),
			URpgInventoryAddressSlotWidget::StaticClass(),
			TEXT("DragVisualClass")
		},
		{
			TEXT("Spatial-item drag visual"),
			URpgInventorySpatialItemWidget::StaticClass(),
			TEXT("DragVisualClass")
		},
		{
			TEXT("Spatial controller preview"),
			URpgInventorySpatialGridWidget::StaticClass(),
			TEXT("SpatialPreviewWidgetClass")
		},
		{
			TEXT("Screen-owned free drag visual"),
			URpgInventoryInteractionScreenWidget::StaticClass(),
			TEXT("FreeDragVisualWidgetClass")
		}
	};

	for (const FNativePropertyExpectation& Expectation : NativeProperties)
	{
		const FClassProperty* ClassProperty =
			FindFProperty<FClassProperty>(
				Expectation.OwnerClass,
				Expectation.PropertyName);
		const FString PropertyExistsLabel =
			FString::Printf(TEXT("%s property exists"), Expectation.Label);
		if (!TestNotNull(*PropertyExistsLabel, ClassProperty))
		{
			continue;
		}

		const FString PropertyTypeLabel =
			FString::Printf(
				TEXT("%s only accepts canonical drag-visual presenters"),
				Expectation.Label);
		TestEqual(
			*PropertyTypeLabel,
			ClassProperty->MetaClass.Get(),
			URpgInventoryDragVisualWidget::StaticClass());
	}

	struct FAuthoredDefaultExpectation
	{
		const TCHAR* Label;
		const TCHAR* ClassPath;
		const TCHAR* PropertyName;
	};
	const FAuthoredDefaultExpectation AuthoredDefaults[] = {
		{
			TEXT("Gear"),
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_GearSlot.CUI_GearSlot_C"),
			TEXT("DragVisualClass")
		},
		{
			TEXT("Spatial item"),
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
				"CUI_SpatialInventoryItem.CUI_SpatialInventoryItem_C"),
			TEXT("DragVisualClass")
		},
		{
			TEXT("Carry"),
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
				"CUI_CarrySlot.CUI_CarrySlot_C"),
			TEXT("DragVisualClass")
		},
		{
			TEXT("Spatial controller preview"),
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
				"CUI_SpatialInventoryGrid.CUI_SpatialInventoryGrid_C"),
			TEXT("SpatialPreviewWidgetClass")
		},
		{
			TEXT("Player inventory free ghost"),
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_PlayerInventory.CUI_PlayerInventory_C"),
			TEXT("FreeDragVisualWidgetClass")
		},
		{
			TEXT("Storage free ghost"),
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_StorageSpatial.CUI_StorageSpatial_C"),
			TEXT("FreeDragVisualWidgetClass")
		},
		{
			TEXT("Base terminal free ghost"),
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_BaseTerminalSpatial.CUI_BaseTerminalSpatial_C"),
			TEXT("FreeDragVisualWidgetClass")
		},
		{
			TEXT("Crafting free ghost"),
			TEXT(
				"/Game/SurvivalRpg/Crafting/UI/"
				"CUI_CraftingStationSpatial.CUI_CraftingStationSpatial_C"),
			TEXT("FreeDragVisualWidgetClass")
		}
	};

	for (const FAuthoredDefaultExpectation& Expectation : AuthoredDefaults)
	{
		UClass* WidgetClass =
			LoadClass<UUserWidget>(nullptr, Expectation.ClassPath);
		const FString ClassLoadsLabel =
			FString::Printf(TEXT("%s authored class loads"), Expectation.Label);
		if (!TestNotNull(*ClassLoadsLabel, WidgetClass))
		{
			continue;
		}

		const FClassProperty* ClassProperty =
			FindFProperty<FClassProperty>(
				WidgetClass,
				Expectation.PropertyName);
		const FString PropertyExistsLabel =
			FString::Printf(
				TEXT("%s authored class exposes %s"),
				Expectation.Label,
				Expectation.PropertyName);
		if (!TestNotNull(*PropertyExistsLabel, ClassProperty))
		{
			continue;
		}

		UClass* ConfiguredClass =
			Cast<UClass>(
				ClassProperty->GetPropertyValue_InContainer(
					WidgetClass->GetDefaultObject()).Get());
		const FString ExactClassLabel =
			FString::Printf(
				TEXT("%s uses the exact canonical authored drag visual"),
				Expectation.Label);
		TestEqual(*ExactClassLabel, ConfiguredClass, CanonicalVisualClass);
	}

	UClass* SpatialGridClass = LoadClass<URpgInventorySpatialGridWidget>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryGrid.CUI_SpatialInventoryGrid_C"));
	const UWidgetBlueprintGeneratedClass* SpatialGridGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(SpatialGridClass);
	const UWidgetTree* SpatialGridTree =
		SpatialGridGeneratedClass
			? SpatialGridGeneratedClass->GetWidgetTreeArchetype()
			: nullptr;
	if (TestNotNull(
			TEXT("Authored spatial-grid class loads"),
			SpatialGridClass) &&
		TestNotNull(
			TEXT("Authored spatial grid owns a widget tree"),
			SpatialGridTree))
	{
		TestTrue(
			TEXT("Spatial grid authors the required dedicated PreviewCanvas"),
			Cast<UCanvasPanel>(
				SpatialGridTree->FindWidget(TEXT("PreviewCanvas"))) != nullptr);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryViewModelCompositionTest,
	"SurvivalRpg.Inventory.UI.PlayerViewModelComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryViewModelCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* PlayerWidgetClass = LoadClass<URpgPlayerInventoryWidget>(
		nullptr,
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_PlayerInventory.CUI_PlayerInventory_C"));
	if (!TestNotNull(TEXT("Authored Player Inventory class loads"), PlayerWidgetClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Authored class derives from the native Player presenter"),
		PlayerWidgetClass->IsChildOf(URpgPlayerInventoryWidget::StaticClass()));
	const UWidgetBlueprintGeneratedClass* PlayerWidgetGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(PlayerWidgetClass);
	if (!TestNotNull(
		TEXT("Authored Player Inventory uses a widget Blueprint generated class"),
		PlayerWidgetGeneratedClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Screen-owned VM initializes before an owning player context is assigned"),
		PlayerWidgetGeneratedClass->bCanCallInitializedWithoutPlayerContext);
	TestNotNull(
		TEXT("Native presenter exposes the canonical PropertyPath getter"),
		PlayerWidgetClass->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				URpgPlayerInventoryWidget,
				GetPlayerInventoryViewModel)));
	TestNull(
		TEXT("Compiled widget exposes no generated Player VM setter"),
		PlayerWidgetClass->FindFunctionByName(
			TEXT("SetRpgPlayerInventoryViewModel")));

	URpgPlayerInventoryWidget* Widget =
		CreateWidget<URpgPlayerInventoryWidget>(
			TestWorld.GetTestWorld(),
			PlayerWidgetClass);
	if (!TestNotNull(TEXT("Authored Player Inventory widget initializes"), Widget))
	{
		return false;
	}

	URpgPlayerInventoryViewModel* ViewModel = Widget->GetPlayerInventoryViewModel();
	if (!TestNotNull(TEXT("Native presenter creates its aggregate view model"), ViewModel))
	{
		return false;
	}
	TestEqual(
		TEXT("Aggregate view model is owned by the Player screen"),
		ViewModel->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("Player screen owns exactly one direct aggregate view model"),
		CountDirectPlayerInventoryViewModels(Widget),
		1);
	TestEqual(
		TEXT("Aggregate VM only permits native PropertyPath composition"),
		URpgPlayerInventoryViewModel::StaticClass()->GetMetaData(
			TEXT("MVVMAllowedContextCreationType")),
		FString(TEXT("PropertyPath")));

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
	if (!TestNotNull(TEXT("Authored Player Inventory has a compiled MVVM view"), View) ||
		!TestNotNull(TEXT("Authored MVVM view has a compiled view class"), View ? View->GetViewClass() : nullptr))
	{
		return false;
	}

	// PropertyPath sources follow the normal MVVM lifecycle and resolve when the authored Slate view constructs.
	ICommonInputModule::GetSettings().LoadData();
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestTrue(
			TEXT("Player Inventory constructs its authored Slate representation"),
			SlateWidget.IsValid()))
	{
		return false;
	}
	TestTrue(
		TEXT("Player Inventory PropertyPath source initializes during construction"),
		View->AreSourcesInitialized());

	const TArrayView<const FMVVMViewClass_Source> CompiledSources =
		View->GetViewClass()->GetSources();
	int32 MatchingSourceIndex = INDEX_NONE;
	int32 MatchingSourceCount = 0;
	for (int32 SourceIndex = 0; SourceIndex < CompiledSources.Num(); ++SourceIndex)
	{
		const FMVVMViewClass_Source& Source = CompiledSources[SourceIndex];
		if (Source.IsViewModel() &&
			Source.GetName() == URpgPlayerInventoryWidget::PlayerInventoryViewModelSourceName)
		{
			MatchingSourceIndex = SourceIndex;
			++MatchingSourceCount;
		}
	}

	TestEqual(
		TEXT("Authored MVVM view has exactly one canonical Player Inventory source"),
		MatchingSourceCount,
		1);
	if (!CompiledSources.IsValidIndex(MatchingSourceIndex) ||
		!View->GetSources().IsValidIndex(MatchingSourceIndex))
	{
		return false;
	}

	const FMVVMViewClass_Source& CompiledSource = CompiledSources[MatchingSourceIndex];
	const FMVVMView_Source& RuntimeSource = View->GetSources()[MatchingSourceIndex];
	TestEqual(
		TEXT("Canonical source expects the aggregate Player Inventory VM class"),
		CompiledSource.GetSourceClass(),
		URpgPlayerInventoryViewModel::StaticClass());
	TestFalse(
		TEXT("Canonical PropertyPath source cannot be replaced"),
		CompiledSource.CanBeSet());
	TestFalse(
		TEXT("Canonical source is non-optional because native ownership precedes MVVM initialization"),
		CompiledSource.IsOptional());
	TestFalse(
		TEXT("Runtime source was resolved from the native getter instead of injected manually"),
		RuntimeSource.bSetManually);
	TestEqual(
		TEXT("Runtime MVVM source is exactly the native-owned aggregate VM"),
		RuntimeSource.Source.Get(),
		static_cast<UObject*>(ViewModel));
	TestEqual(
		TEXT("Named MVVM lookup returns exactly the native-owned aggregate VM"),
		View->GetViewModel(
			URpgPlayerInventoryWidget::PlayerInventoryViewModelSourceName).GetObject(),
		static_cast<UObject*>(ViewModel));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryAuthoredContentHostsTest,
	"SurvivalRpg.Inventory.UI.PlayerAuthoredContentHosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryAuthoredContentHostsTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	struct FExpectedContentHost
	{
		FName Name;
		FVector2D Position;
	};

	const FExpectedContentHost ExpectedHosts[] = {
		{TEXT("Content_Pockets"), FVector2D(896.0, 116.0)},
		{TEXT("Content_Backpack"), FVector2D(900.0, 308.0)},
		{TEXT("Content_Belt"), FVector2D(1296.0, 116.0)},
		{TEXT("Content_Pouch"), FVector2D(1296.0, 308.0)},
		{TEXT("Content_ResourceBag"), FVector2D(1296.0, 500.0)},
	};
	const FName ProviderHostNames[] = {
		TEXT("Content_Backpack"),
		TEXT("Content_Belt"),
		TEXT("Content_Pouch"),
		TEXT("Content_ResourceBag"),
	};

	UClass* PlayerWidgetClass = LoadClass<URpgPlayerInventoryWidget>(
		nullptr,
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_PlayerInventory.CUI_PlayerInventory_C"));
	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(PlayerWidgetClass);
	const UWidgetTree* AuthoredTree =
		GeneratedClass ? GeneratedClass->GetWidgetTreeArchetype() : nullptr;
	if (!TestNotNull(TEXT("Authored Player Inventory class loads"), PlayerWidgetClass) ||
		!TestNotNull(TEXT("Player Inventory has a generated Widget Blueprint class"), GeneratedClass) ||
		!TestNotNull(TEXT("Player Inventory has an authored compiled WidgetTree"), AuthoredTree))
	{
		return false;
	}

	UCanvasPanel* InventoryCanvas =
		Cast<UCanvasPanel>(AuthoredTree->FindWidget(TEXT("InventoryCanvas")));
	UOverlay* RootOverlay =
		Cast<UOverlay>(AuthoredTree->FindWidget(TEXT("RootOverlay")));
	UWidget* DragVisualCanvas =
		AuthoredTree->FindWidget(TEXT("DragVisualCanvas"));
	if (!TestNotNull(TEXT("InventoryCanvas is authored"), InventoryCanvas) ||
		!TestNotNull(TEXT("RootOverlay is authored"), RootOverlay) ||
		!TestNotNull(TEXT("DragVisualCanvas is authored"), DragVisualCanvas))
	{
		return false;
	}

	TSet<int32> AuthoredChildIndices;
	UClass* CanonicalContentHostClass = nullptr;
	for (const FExpectedContentHost& Expected : ExpectedHosts)
	{
		UWidget* Host = AuthoredTree->FindWidget(Expected.Name);
		if (!TestNotNull(
			*FString::Printf(TEXT("%s is authored"), *Expected.Name.ToString()),
			Host))
		{
			return false;
		}

		TestTrue(
			*FString::Printf(TEXT("%s uses the native SlotGroup host contract"), *Expected.Name.ToString()),
			Host->IsA<URpgInventorySlotGroupWidget>());
		TestEqual(
			*FString::Printf(TEXT("%s is a direct InventoryCanvas child"), *Expected.Name.ToString()),
			Host->GetParent(),
			static_cast<UPanelWidget*>(InventoryCanvas));

		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Host->Slot);
		if (!TestNotNull(
			*FString::Printf(TEXT("%s uses a CanvasPanelSlot"), *Expected.Name.ToString()),
			CanvasSlot))
		{
			return false;
		}
		TestTrue(
			*FString::Printf(TEXT("%s is authored Size To Content"), *Expected.Name.ToString()),
			CanvasSlot->GetAutoSize());
		TestTrue(
			*FString::Printf(TEXT("%s keeps its canonical authored position"), *Expected.Name.ToString()),
			CanvasSlot->GetPosition().Equals(Expected.Position));

		const int32 ChildIndex = InventoryCanvas->GetChildIndex(Host);
		TestTrue(
			*FString::Printf(TEXT("%s has a unique Canvas child index"), *Expected.Name.ToString()),
			ChildIndex != INDEX_NONE && !AuthoredChildIndices.Contains(ChildIndex));
		AuthoredChildIndices.Add(ChildIndex);

		if (!CanonicalContentHostClass)
		{
			CanonicalContentHostClass = Host->GetClass();
		}
		TestEqual(
			*FString::Printf(TEXT("%s uses the canonical authored host class"), *Expected.Name.ToString()),
			Host->GetClass(),
			CanonicalContentHostClass);
	}

	TestEqual(
		TEXT("Canonical content hosts use CUI_InventorySlotGroupEntry"),
		CanonicalContentHostClass ? CanonicalContentHostClass->GetPathName() : FString(),
		FString(TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_InventorySlotGroupEntry.CUI_InventorySlotGroupEntry_C")));

	int32 PreviousProviderIndex = INDEX_NONE;
	for (const FName ProviderHostName : ProviderHostNames)
	{
		const UWidget* ProviderHost = AuthoredTree->FindWidget(ProviderHostName);
		const int32 ProviderIndex =
			ProviderHost ? InventoryCanvas->GetChildIndex(ProviderHost) : INDEX_NONE;
		if (PreviousProviderIndex != INDEX_NONE)
		{
			TestEqual(
				*FString::Printf(
					TEXT("%s immediately follows the previous provider host"),
					*ProviderHostName.ToString()),
				ProviderIndex,
				PreviousProviderIndex + 1);
		}
		PreviousProviderIndex = ProviderIndex;
	}

	TestEqual(
		TEXT("DragVisualCanvas remains the final root child"),
		RootOverlay->GetChildIndex(DragVisualCanvas),
		RootOverlay->GetChildrenCount() - 1);

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}
	URpgPlayerInventoryWidget* RuntimeWidget =
		CreateWidget<URpgPlayerInventoryWidget>(
			TestWorld.GetTestWorld(),
			PlayerWidgetClass);
	if (!TestNotNull(TEXT("Authored Player Inventory widget initializes"), RuntimeWidget))
	{
		return false;
	}

	for (const FExpectedContentHost& Expected : ExpectedHosts)
	{
		UWidget* RuntimeHost = RuntimeWidget->GetWidgetFromName(Expected.Name);
		const FObjectPropertyBase* BindWidgetProperty =
			FindFProperty<FObjectPropertyBase>(
				URpgPlayerInventoryWidget::StaticClass(),
				Expected.Name);
		TestTrue(
			*FString::Printf(
				TEXT("%s binds into the exact native presenter property"),
				*Expected.Name.ToString()),
			RuntimeHost &&
				BindWidgetProperty &&
				BindWidgetProperty->GetObjectPropertyValue_InContainer(RuntimeWidget) ==
					RuntimeHost);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSlotGroupLeafMvvmBindingTest,
	"SurvivalRpg.Inventory.UI.SlotGroupLeafMvvmBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSlotGroupLeafMvvmBindingTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* SlotGroupWidgetClass = LoadClass<URpgInventorySlotGroupWidget>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_InventorySlotGroupEntry.CUI_InventorySlotGroupEntry_C"));
	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(SlotGroupWidgetClass);
	if (!TestNotNull(TEXT("Canonical slot-group widget class loads"), SlotGroupWidgetClass) ||
		!TestNotNull(TEXT("Canonical slot-group widget is a Widget Blueprint"), GeneratedClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Data-only slot-group leaf initializes without a player context for runtime composition and UMG preview"),
		GeneratedClass->bCanCallInitializedWithoutPlayerContext);

	const UFunction* PresentationHook =
		SlotGroupWidgetClass->FindFunctionByName(
			TEXT("BP_OnSlotGroupViewModelSet"));
	TestNull(
		TEXT("Legacy slot-group Blueprint presentation hook is removed"),
		PresentationHook);

	const TArray<UWidgetBlueprintGeneratedClassExtension*> CompiledViewExtensions =
		GeneratedClass->GetExtensions(UMVVMViewClass::StaticClass(), false);
	TestEqual(
		TEXT("Canonical slot-group Blueprint owns exactly one compiled MVVM view extension"),
		CompiledViewExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledViewClass =
		CompiledViewExtensions.Num() == 1
			? Cast<UMVVMViewClass>(CompiledViewExtensions[0])
			: nullptr;
	if (!TestNotNull(
		TEXT("Canonical slot-group Blueprint registers its compiled MVVM view extension"),
		CompiledViewClass))
	{
		return false;
	}

	URpgInventorySlotGroupWidget* Widget =
		CreateWidget<URpgInventorySlotGroupWidget>(
			TestWorld.GetTestWorld(),
			SlotGroupWidgetClass);
	if (!TestNotNull(TEXT("Canonical slot-group widget initializes"), Widget))
	{
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
	if (!TestNotNull(TEXT("Canonical slot-group widget has a compiled MVVM view"), View) ||
		!TestNotNull(TEXT("Slot-group MVVM view has a compiled view class"), View ? View->GetViewClass() : nullptr))
	{
		return false;
	}
	TestEqual(
		TEXT("Runtime slot-group view uses the Blueprint-owned compiled extension"),
		View->GetViewClass(),
		CompiledViewClass);
	TestFalse(
		TEXT("CreateWidget initializes the MVVM view before Slate construction"),
		View->IsConstructed());

	const TArrayView<const FMVVMViewClass_Source> CompiledSources =
		CompiledViewClass->GetSources();
	int32 MatchingSourceIndex = INDEX_NONE;
	int32 MatchingSourceCount = 0;
	for (int32 SourceIndex = 0; SourceIndex < CompiledSources.Num(); ++SourceIndex)
	{
		const FMVVMViewClass_Source& Source = CompiledSources[SourceIndex];
		if (Source.IsViewModel() &&
			Source.GetName() ==
				URpgInventorySlotGroupWidget::SlotGroupViewModelSourceName)
		{
			MatchingSourceIndex = SourceIndex;
			++MatchingSourceCount;
		}
	}

	TestEqual(
		TEXT("Canonical slot-group widget has exactly one native-owned MVVM source"),
		MatchingSourceCount,
		1);
	TestEqual(
		TEXT("Canonical slot-group widget has exactly one compiled leaf binding"),
		CompiledViewClass->GetBindings().Num(),
		1);
	if (!CompiledSources.IsValidIndex(MatchingSourceIndex) ||
		!View->GetSources().IsValidIndex(MatchingSourceIndex))
	{
		return false;
	}

	const FMVVMViewClass_Source& CompiledSource =
		CompiledSources[MatchingSourceIndex];
	TestEqual(
		TEXT("Slot-group source expects the exact group VM class"),
		CompiledSource.GetSourceClass(),
		URpgInventorySlotGroupViewModel::StaticClass());
	TestTrue(
		TEXT("Slot-group source is settable by the native presenter"),
		CompiledSource.CanBeSet());
	TestTrue(
		TEXT("Slot-group source is optional while no runtime group is visible"),
		CompiledSource.IsOptional());
	TestEqual(
		TEXT("Slot-group source owns exactly one compiled binding"),
		CompiledSource.GetBindings().Num(),
		1);
	TestEqual(
		TEXT("Slot-group VM only permits presenter-supplied manual composition"),
		URpgInventorySlotGroupViewModel::StaticClass()->GetMetaData(
			TEXT("MVVMAllowedContextCreationType")),
		FString(TEXT("Manual")));
	TestNull(
		TEXT("Slot-group source starts empty"),
		View->GetViewModel(
			URpgInventorySlotGroupWidget::SlotGroupViewModelSourceName).GetObject());

	UTextBlock* GroupNameText =
		Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_GroupName")));
	if (!TestNotNull(TEXT("Canonical group-title leaf is authored"), GroupNameText))
	{
		return false;
	}

	URpgInventorySlotGroupViewModel* GroupViewModel =
		NewObject<URpgInventorySlotGroupViewModel>(Widget);
	FRpgInventorySlotGroupView GroupView;
	GroupView.ContainerId = TEXT("InternalBackpackId");
	GroupView.DisplayName = FText::FromString(TEXT("Backpack Display Name"));
	TArray<URpgInventoryAddressSlotViewModel*> EmptySlots;
	GroupViewModel->InitializeGroup(GroupView, EmptySlots);

	Widget->SetSlotGroupViewModel(GroupViewModel);
	const FMVVMView_Source& RuntimeSource =
		View->GetSources()[MatchingSourceIndex];
	TestTrue(
		TEXT("Runtime slot-group source records native manual injection"),
		RuntimeSource.bSetManually);
	TestEqual(
		TEXT("Runtime slot-group source is exactly the presenter-supplied VM"),
		RuntimeSource.Source.Get(),
		static_cast<UObject*>(GroupViewModel));

	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestTrue(TEXT("Slot-group widget constructs its authored Slate representation"), SlateWidget.IsValid()))
	{
		return false;
	}
	TestTrue(
		TEXT("Slot-group MVVM view constructs with the authored widget"),
		View->IsConstructed());
	TestTrue(
		TEXT("Slot-group MVVM sources initialize during construction"),
		View->AreSourcesInitialized());
	TestTrue(
		TEXT("Slot-group MVVM bindings initialize during construction"),
		View->AreBindingsInitialized());
	TestTrue(
		TEXT("Authored group title renders DisplayName instead of the internal container id"),
		GroupNameText->GetText().EqualTo(GroupView.DisplayName) &&
			!GroupNameText->GetText().EqualTo(FText::FromName(GroupView.ContainerId)));

	GroupView.DisplayName = FText::FromString(TEXT("Updated Backpack Name"));
	GroupViewModel->InitializeGroup(GroupView, EmptySlots);
	TestTrue(
		TEXT("FieldNotify refresh updates the authored title without a Blueprint event"),
		GroupNameText->GetText().EqualTo(GroupView.DisplayName));

	Widget->SetSlotGroupViewModel(nullptr);
	TestNull(
		TEXT("Removing a runtime group clears the optional MVVM source"),
		View->GetViewModel(
			URpgInventorySlotGroupWidget::SlotGroupViewModelSourceName).GetObject());
	SlateWidget.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryViewModelPoolingTest,
	"SurvivalRpg.Inventory.UI.PlayerViewModelPooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryViewModelPoolingTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* PlayerWidgetClass = LoadClass<URpgPlayerInventoryWidget>(
		nullptr,
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_PlayerInventory.CUI_PlayerInventory_C"));
	URpgPlayerInventoryWidget* Widget = PlayerWidgetClass
		? CreateWidget<URpgPlayerInventoryWidget>(
			TestWorld.GetTestWorld(),
			PlayerWidgetClass)
		: nullptr;
	if (!TestNotNull(TEXT("Authored Player Inventory widget initializes"), Widget))
	{
		return false;
	}

	URpgPlayerInventoryViewModel* ViewModel = Widget->GetPlayerInventoryViewModel();
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
	if (!TestNotNull(TEXT("Native aggregate VM exists"), ViewModel) ||
		!TestNotNull(TEXT("Authored MVVM view exists"), View))
	{
		return false;
	}

	// Commandlet automation does not run the normal CommonInput startup path, while CommonActivatableWidget::NativeConstruct
	// requires its default back-action data. Load the project settings before exercising the real Slate lifecycle.
	ICommonInputModule::GetSettings().LoadData();
	TestFalse(
		TEXT("CreateWidget alone does not initialize the PropertyPath source"),
		View->AreSourcesInitialized());
	Widget->ActivateWidget();
	TestTrue(
		TEXT("Player screen may activate before its first Slate construction"),
		Widget->IsActivated());
	TestEqual(
		TEXT("Activation before construction retains the native-owned VM"),
		Widget->GetPlayerInventoryViewModel(),
		ViewModel);
	TestFalse(
		TEXT("Activation before construction does not fabricate a runtime MVVM source"),
		View->AreSourcesInitialized());
	Widget->DeactivateWidget();
	TestFalse(
		TEXT("Pre-construction Player screen deactivates cleanly"),
		Widget->IsActivated());

	TSharedPtr<SWidget> FirstSlateWidget = Widget->TakeWidget();
	if (!TestTrue(
			TEXT("Player screen constructs its first Slate representation"),
			FirstSlateWidget.IsValid()))
	{
		return false;
	}
	TestTrue(
		TEXT("Player screen initializes its native PropertyPath source during construction"),
		View->AreSourcesInitialized());

	auto VerifyStableComposition = [this, Widget, ViewModel, View](
		const TCHAR* Phase,
		const bool bExpectSourceInitialized) -> bool
	{
		const FString Prefix(Phase);
		const bool bVmStable = TestEqual(
			*(Prefix + TEXT(": native VM pointer remains stable")),
			Widget->GetPlayerInventoryViewModel(),
			ViewModel);
		const bool bSourceInitializationMatches = TestEqual(
			*(Prefix + TEXT(": MVVM source initialization follows the Slate lifecycle")),
			View->AreSourcesInitialized(),
			bExpectSourceInitialized);
		const UObject* RuntimeSource = View->GetViewModel(
			URpgPlayerInventoryWidget::PlayerInventoryViewModelSourceName).GetObject();
		const bool bSourceMatchesLifecycle = bExpectSourceInitialized
			? TestEqual(
				*(Prefix + TEXT(": initialized PropertyPath resolves the native VM")),
				RuntimeSource,
				static_cast<const UObject*>(ViewModel))
			: TestNull(
				*(Prefix + TEXT(": released Slate deinitializes the runtime MVVM source")),
				RuntimeSource);
		const bool bOuterStable = TestEqual(
			*(Prefix + TEXT(": VM remains screen-owned")),
			ViewModel->GetOuter(),
			static_cast<UObject*>(Widget));
		const bool bSingleVm = TestEqual(
			*(Prefix + TEXT(": screen still owns exactly one aggregate VM")),
			CountDirectPlayerInventoryViewModels(Widget),
			1);
		const bool bGearDelegateUnique = TestEqual(
			*(Prefix + TEXT(": gear refresh delegate is bound exactly once")),
			CountDelegateBindingsTo(ViewModel->OnGearSlotsChanged, Widget),
			1);
		const bool bGroupsDelegateUnique = TestEqual(
			*(Prefix + TEXT(": group refresh delegate is bound exactly once")),
			CountDelegateBindingsTo(ViewModel->OnSlotGroupsChanged, Widget),
			1);
		const bool bActionBarDelegateUnique = TestEqual(
			*(Prefix + TEXT(": actionbar refresh delegate is bound exactly once")),
			CountDelegateBindingsTo(ViewModel->OnActionBarSlotsChanged, Widget),
			1);
		return bVmStable &&
			bSourceInitializationMatches &&
			bSourceMatchesLifecycle &&
			bOuterStable &&
			bSingleVm &&
			bGearDelegateUnique &&
			bGroupsDelegateUnique &&
			bActionBarDelegateUnique;
	};

	if (!VerifyStableComposition(TEXT("Initialized"), true))
	{
		return false;
	}

	// The PropertyPath source must reject the same replacement that UE 5.8's generated Manual setter previously
	// allowed. This proves the native getter is the only source authority, including while the screen is active.
	URpgPlayerInventoryViewModel* ForeignViewModel =
		NewObject<URpgPlayerInventoryViewModel>(TestWorld.GetTestWorld());
	TScriptInterface<INotifyFieldValueChanged> ForeignViewModelInterface;
	ForeignViewModelInterface.SetObject(ForeignViewModel);
	ForeignViewModelInterface.SetInterface(ForeignViewModel);
	AddExpectedError(
		TEXT("cannot be set"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(
		TEXT("Runtime MVVM API rejects replacement of the native PropertyPath source"),
		View->SetViewModel(
			URpgPlayerInventoryWidget::PlayerInventoryViewModelSourceName,
			ForeignViewModelInterface));
	TestEqual(
		TEXT("Rejected replacement leaves the native-owned VM in the MVVM source"),
		View->GetViewModel(
			URpgPlayerInventoryWidget::PlayerInventoryViewModelSourceName).GetObject(),
		static_cast<UObject*>(ViewModel));

	Widget->ActivateWidget();
	TestTrue(TEXT("Player screen activates through CommonUI"), Widget->IsActivated());
	if (!VerifyStableComposition(TEXT("Activated"), true))
	{
		return false;
	}

	Widget->DeactivateWidget();
	TestFalse(TEXT("Player screen deactivates through CommonUI"), Widget->IsActivated());
	if (!VerifyStableComposition(TEXT("Deactivated"), true))
	{
		return false;
	}

	FirstSlateWidget.Reset();
	TestFalse(
		TEXT("Releasing the final Slate reference runs the screen Destruct/release path"),
		Widget->GetCachedWidget().IsValid());
	if (!VerifyStableComposition(TEXT("Slate released"), false))
	{
		return false;
	}

	TSharedPtr<SWidget> ReconstructedSlateWidget = Widget->TakeWidget();
	if (!TestTrue(
		TEXT("Pooled Player screen reconstructs its Slate representation"),
		ReconstructedSlateWidget.IsValid()) ||
		!VerifyStableComposition(TEXT("Slate reconstructed"), true))
	{
		return false;
	}

	Widget->ActivateWidget();
	TestTrue(TEXT("Pooled Player screen reactivates through CommonUI"), Widget->IsActivated());
	const bool bStableAfterReactivation =
		VerifyStableComposition(TEXT("Reactivated"), true);
	Widget->DeactivateWidget();
	ReconstructedSlateWidget.Reset();
	return bStableAfterReactivation;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActionBarSlotEntryPoolingTest,
	"SurvivalRpg.Inventory.UI.ActionBarSlotEntryPooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActionBarSlotEntryPoolingTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* EntryWidgetClass = LoadClass<URpgActionBarSlotWidget>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/UI/Hud/ActionBar/"
			"CUI_ActionBarSlotEntry.CUI_ActionBarSlotEntry_C"));
	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(EntryWidgetClass);
	if (!TestNotNull(TEXT("Canonical actionbar entry class loads"), EntryWidgetClass) ||
		!TestNotNull(TEXT("Canonical actionbar entry is a Widget Blueprint"), GeneratedClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Data-only actionbar entry initializes without a player context"),
		GeneratedClass->bCanCallInitializedWithoutPlayerContext);

	const TArray<UWidgetBlueprintGeneratedClassExtension*> CompiledViewExtensions =
		GeneratedClass->GetExtensions(UMVVMViewClass::StaticClass(), false);
	TestEqual(
		TEXT("Canonical actionbar entry owns exactly one compiled MVVM view"),
		CompiledViewExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledViewClass =
		CompiledViewExtensions.Num() == 1
			? Cast<UMVVMViewClass>(CompiledViewExtensions[0])
			: nullptr;
	if (!TestNotNull(TEXT("Actionbar entry MVVM view class exists"), CompiledViewClass))
	{
		return false;
	}

	int32 MatchingSourceCount = 0;
	const int32 MatchingSourceIndex = FindNamedViewModelSource(
		*CompiledViewClass,
		URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName,
		MatchingSourceCount);
	TestEqual(
		TEXT("Actionbar entry has exactly one canonical VM source"),
		MatchingSourceCount,
		1);
	TestEqual(
		TEXT("Actionbar entry has exactly three declarative leaf bindings"),
		CompiledViewClass->GetBindings().Num(),
		3);
	const TArrayView<const FMVVMViewClass_Source> CompiledSources =
		CompiledViewClass->GetSources();
	if (!CompiledSources.IsValidIndex(MatchingSourceIndex))
	{
		return false;
	}

	const FMVVMViewClass_Source& CompiledSource =
		CompiledSources[MatchingSourceIndex];
	TestEqual(
		TEXT("Actionbar entry source expects the exact VM class"),
		CompiledSource.GetSourceClass(),
		URpgActionBarSlotViewModel::StaticClass());
	TestTrue(
		TEXT("Actionbar entry source is settable by its native presenter"),
		CompiledSource.CanBeSet());
	TestTrue(
		TEXT("Actionbar entry source is optional while the entry is pooled"),
		CompiledSource.IsOptional());
	TestEqual(
		TEXT("Actionbar entry source owns all three authored leaf bindings"),
		CompiledSource.GetBindings().Num(),
		3);
	TestEqual(
		TEXT("Actionbar slot VM only permits presenter-supplied manual composition"),
		URpgActionBarSlotViewModel::StaticClass()->GetMetaData(
			TEXT("MVVMAllowedContextCreationType")),
		FString(TEXT("Manual")));

	URpgActionBarSlotWidget* Widget =
		CreateWidget<URpgActionBarSlotWidget>(
			TestWorld.GetTestWorld(),
			EntryWidgetClass);
	if (!TestNotNull(TEXT("Canonical actionbar entry initializes"), Widget))
	{
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
	if (!TestNotNull(TEXT("Actionbar entry runtime MVVM view exists"), View) ||
		!TestNotNull(TEXT("Actionbar entry runtime view class exists"), View ? View->GetViewClass() : nullptr))
	{
		return false;
	}
	TestNull(
		TEXT("Fresh actionbar entry source starts empty"),
		View->GetViewModel(
			URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName).GetObject());
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestTrue(
		TEXT("Actionbar entry constructs its authored Slate representation"),
		SlateWidget.IsValid()))
	{
		return false;
	}
	TestTrue(
		TEXT("Actionbar entry MVVM view constructs before the pooling cycle"),
		View->IsConstructed());
	TestTrue(
		TEXT("Actionbar entry MVVM sources initialize before the pooling cycle"),
		View->AreSourcesInitialized());
	TestTrue(
		TEXT("Actionbar entry MVVM bindings initialize before the pooling cycle"),
		View->AreBindingsInitialized());

	URpgActionBarSlotViewModel* FirstViewModel =
		NewObject<URpgActionBarSlotViewModel>(Widget);
	URpgActionBarSlotViewModel* SecondViewModel =
		NewObject<URpgActionBarSlotViewModel>(Widget);
	const FRpgActionBarSlot EmptyActionBarSlot;
	FirstViewModel->InitializeSlot(2, EmptyActionBarSlot, nullptr, 0);
	SecondViewModel->InitializeSlot(3, EmptyActionBarSlot, nullptr, 0);
	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(
			Widget,
			nullptr);
	if (!TestNotNull(TEXT("Actionbar test coordinator initializes"), Coordinator) ||
		!TestNotNull(
			TEXT("Actionbar test interaction session initializes"),
			Coordinator ? Coordinator->GetInteractionSession() : nullptr))
	{
		return false;
	}

	Widget->SetActionBarSlotViewModel(FirstViewModel);
	Widget->SetDragDropCoordinator(Coordinator);
	IUserListEntry::UpdateItemSelection(*Widget, true);
	TestEqual(
		TEXT("Actionbar entry stores VM A"),
		Widget->GetActionBarSlotViewModel(),
		FirstViewModel);
	TestEqual(
		TEXT("Actionbar entry injects VM A into the exact source"),
		View->GetViewModel(
			URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstViewModel));
	TestEqual(
		TEXT("Actionbar VM A owns exactly one native refresh delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("Coordinator owns exactly one actionbar-entry delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		1);
	TestEqual(
		TEXT("Selected actionbar entry reports focused presentation"),
		Widget->GetCurrentDragDropVisualState(),
		ERpgInventorySlotDragVisualState::Focused);

	Widget->SetActionBarSlotViewModel(SecondViewModel);
	TestEqual(
		TEXT("Actionbar reassignment removes VM A's native delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		0);
	TestEqual(
		TEXT("Actionbar reassignment binds VM B exactly once"),
		CountDelegateBindingsTo(SecondViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("Actionbar reassignment replaces the exact MVVM source with VM B"),
		View->GetViewModel(
			URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(SecondViewModel));

	FRpgInventoryDragPayload PreviewPayload;
	PreviewPayload.SourceType = ERpgInventoryDragSourceType::EquipmentSlot;
	PreviewPayload.EquipmentSlot = ERpgEquipmentSlot::Head;
	if (!TestTrue(
			TEXT("Actionbar pooling preview owns canonical item metadata"),
			RpgInventoryAutomationTestTypes::PopulateCanonicalSpatialItem(
				PreviewPayload,
				Widget,
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass())))
	{
		return false;
	}
	PopulateExactEquipmentSourceSnapshot(
		PreviewPayload,
		Widget,
		TEXT("ActionBarPoolingEquipmentSource"));
	URpgInventoryInteractionSession* InteractionSession =
		Coordinator->GetInteractionSession();
	Widget->PreviewPayloadDrop(PreviewPayload);
	TestTrue(
		TEXT("Actionbar presenter records ownership after publishing a real preview"),
		Widget->bHasExternalPreviewState);
	TestEqual(
		TEXT("Actionbar preview publishes its exact logical target"),
		InteractionSession->GetTarget().TargetType,
		ERpgInventoryDropTargetType::ActionBarSlot);
	TestEqual(
		TEXT("Actionbar preview publishes VM B's slot index"),
		InteractionSession->GetTarget().ActionBarSlotIndex,
		SecondViewModel->GetSlotIndex());
	TestEqual(
		TEXT("Actionbar preview without a player-layout context is visibly blocked"),
		Widget->GetCurrentDragDropVisualState(),
		ERpgInventorySlotDragVisualState::InvalidTarget);

	URpgActionBarSlotWidget* PeerWidget =
		CreateWidget<URpgActionBarSlotWidget>(
			TestWorld.GetTestWorld(),
			EntryWidgetClass);
	if (!TestNotNull(TEXT("Peer actionbar presenter initializes"), PeerWidget))
	{
		return false;
	}
	PeerWidget->SetActionBarSlotViewModel(FirstViewModel);
	PeerWidget->SetDragDropCoordinator(Coordinator);
	PeerWidget->PreviewPayloadDrop(PreviewPayload);
	TestEqual(
		TEXT("Peer actionbar presenter replaces the shared preview target"),
		InteractionSession->GetTarget().ActionBarSlotIndex,
		FirstViewModel->GetSlotIndex());

	IUserListEntry::ReleaseEntry(*Widget);
	TestEqual(
		TEXT("Releasing an actionbar presenter preserves another slot's preview"),
		InteractionSession->GetTarget().TargetType,
		ERpgInventoryDropTargetType::ActionBarSlot);
	TestEqual(
		TEXT("Releasing an actionbar presenter preserves the peer slot index"),
		InteractionSession->GetTarget().ActionBarSlotIndex,
		FirstViewModel->GetSlotIndex());
	IUserListEntry::ReleaseEntry(*PeerWidget);
	TestEqual(
		TEXT("Releasing the actionbar presenter that owns the preview clears it"),
		InteractionSession->GetTarget().TargetType,
		ERpgInventoryDropTargetType::None);
	TestNull(
		TEXT("Released actionbar entry no longer represents a VM"),
		Widget->GetActionBarSlotViewModel());
	TestNull(
		TEXT("Released actionbar entry clears its optional MVVM source"),
		View->GetViewModel(
			URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName).GetObject());
	TestNull(
		TEXT("Released actionbar entry drops its screen coordinator"),
		Widget->DragDropCoordinator.Get());
	TestEqual(
		TEXT("Released actionbar VM B no longer targets the pooled widget"),
		CountDelegateBindingsTo(SecondViewModel->OnSlotChanged, Widget),
		0);
	TestEqual(
		TEXT("Released actionbar coordinator no longer targets the pooled widget"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		0);
	TestFalse(
		TEXT("Released actionbar entry clears external preview ownership"),
		Widget->bHasExternalPreviewState);
	TestEqual(
		TEXT("Released actionbar entry clears the external preview value"),
		Widget->ExternalPreviewState,
		ERpgInventorySlotDragVisualState::Normal);
	TestEqual(
		TEXT("Released actionbar entry forgets its published preview slot"),
		Widget->ExternalPreviewActionBarSlotIndex,
		INDEX_NONE);
	TestEqual(
		TEXT("Released actionbar entry returns to normal drag presentation"),
		Widget->GetCurrentDragDropVisualState(),
		ERpgInventorySlotDragVisualState::Normal);

	SecondViewModel->OnSlotChanged.Broadcast(SecondViewModel);
	TestNull(
		TEXT("A released actionbar VM cannot repopulate the cleared MVVM source"),
		View->GetViewModel(
			URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName).GetObject());

	Widget->SetActionBarSlotViewModel(FirstViewModel);
	Widget->SetDragDropCoordinator(Coordinator);
	TestEqual(
		TEXT("The pooled actionbar entry cleanly rebinds VM A"),
		View->GetViewModel(
			URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstViewModel));
	TestEqual(
		TEXT("The pooled actionbar entry restores exactly one VM delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("The pooled actionbar entry restores exactly one coordinator delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		1);

	IUserListEntry::ReleaseEntry(*Widget);
	SlateWidget.Reset();
	TestNull(
		TEXT("Destruct after ListView release keeps the actionbar source cleared"),
		View->GetViewModel(
			URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName).GetObject());
	TestEqual(
		TEXT("Destruct after ListView release does not restore the VM delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		0);
	TestEqual(
		TEXT("Destruct after ListView release does not restore the coordinator delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		0);

	URpgActionBarSlotWidget* ManuallyPlacedWidget =
		CreateWidget<URpgActionBarSlotWidget>(
			TestWorld.GetTestWorld(),
			EntryWidgetClass);
	if (!TestNotNull(TEXT("Manually placed actionbar presenter initializes"), ManuallyPlacedWidget))
	{
		return false;
	}
	TSharedPtr<SWidget> ManuallyPlacedSlateWidget =
		ManuallyPlacedWidget->TakeWidget();
	UMVVMView* ManuallyPlacedView =
		UMVVMSubsystem::GetViewFromUserWidget(ManuallyPlacedWidget);
	if (!TestTrue(
			TEXT("Manually placed actionbar presenter constructs its Slate representation"),
			ManuallyPlacedSlateWidget.IsValid()) ||
		!TestNotNull(
			TEXT("Manually placed actionbar presenter owns its runtime MVVM view"),
			ManuallyPlacedView))
	{
		return false;
	}
	URpgActionBarSlotViewModel* ManuallyPlacedViewModel =
		NewObject<URpgActionBarSlotViewModel>(ManuallyPlacedWidget);
	ManuallyPlacedViewModel->InitializeSlot(4, EmptyActionBarSlot, nullptr, 0);
	ManuallyPlacedWidget->SetActionBarSlotViewModel(ManuallyPlacedViewModel);
	ManuallyPlacedWidget->SetDragDropCoordinator(Coordinator);
	ManuallyPlacedWidget->PreviewPayloadDrop(PreviewPayload);
	TestEqual(
		TEXT("Manually placed actionbar presenter publishes its exact preview target"),
		InteractionSession->GetTarget().ActionBarSlotIndex,
		ManuallyPlacedViewModel->GetSlotIndex());
	TestEqual(
		TEXT("Manual actionbar VM owns exactly one presenter delegate before destruct"),
		CountDelegateBindingsTo(ManuallyPlacedViewModel->OnSlotChanged, ManuallyPlacedWidget),
		1);
	TestEqual(
		TEXT("Coordinator owns exactly one manual actionbar delegate before destruct"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, ManuallyPlacedWidget),
		1);

	ManuallyPlacedSlateWidget.Reset();
	TestEqual(
		TEXT("Destruct clears a manually placed actionbar preview"),
		InteractionSession->GetTarget().TargetType,
		ERpgInventoryDropTargetType::None);
	TestNull(
		TEXT("Destruct clears a manually placed actionbar VM"),
		ManuallyPlacedWidget->GetActionBarSlotViewModel());
	TestNull(
		TEXT("Destruct clears a manually placed actionbar MVVM source"),
		ManuallyPlacedView->GetViewModel(
			URpgActionBarSlotWidget::ActionBarSlotViewModelSourceName).GetObject());
	TestNull(
		TEXT("Destruct drops a manually placed actionbar coordinator"),
		ManuallyPlacedWidget->DragDropCoordinator.Get());
	TestEqual(
		TEXT("Destruct removes the manual actionbar VM delegate"),
		CountDelegateBindingsTo(ManuallyPlacedViewModel->OnSlotChanged, ManuallyPlacedWidget),
		0);
	TestEqual(
		TEXT("Destruct removes the manual actionbar coordinator delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, ManuallyPlacedWidget),
		0);
	TestFalse(
		TEXT("Destruct clears manual external-preview ownership"),
		ManuallyPlacedWidget->bHasExternalPreviewState);
	TestEqual(
		TEXT("Destruct forgets the manual presenter preview slot"),
		ManuallyPlacedWidget->ExternalPreviewActionBarSlotIndex,
		INDEX_NONE);
	TestEqual(
		TEXT("Destruct restores normal manual actionbar presentation"),
		ManuallyPlacedWidget->GetCurrentDragDropVisualState(),
		ERpgInventorySlotDragVisualState::Normal);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryAddressSlotEntryPoolingTest,
	"SurvivalRpg.Inventory.UI.InventoryAddressSlotEntryPooling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryAddressSlotEntryPoolingTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* EntryWidgetClass = LoadClass<URpgInventoryAddressSlotWidget>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_AddressSlotEntry.CUI_AddressSlotEntry_C"));
	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(EntryWidgetClass);
	if (!TestNotNull(TEXT("Canonical address-slot entry class loads"), EntryWidgetClass) ||
		!TestNotNull(TEXT("Canonical address-slot entry is a Widget Blueprint"), GeneratedClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Data-only address-slot entry initializes without a player context"),
		GeneratedClass->bCanCallInitializedWithoutPlayerContext);

	const TArray<UWidgetBlueprintGeneratedClassExtension*> CompiledViewExtensions =
		GeneratedClass->GetExtensions(UMVVMViewClass::StaticClass(), false);
	TestEqual(
		TEXT("Canonical address-slot entry owns exactly one compiled MVVM view"),
		CompiledViewExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledViewClass =
		CompiledViewExtensions.Num() == 1
			? Cast<UMVVMViewClass>(CompiledViewExtensions[0])
			: nullptr;
	if (!TestNotNull(TEXT("Address-slot entry MVVM view class exists"), CompiledViewClass))
	{
		return false;
	}

	int32 MatchingSourceCount = 0;
	const int32 MatchingSourceIndex = FindNamedViewModelSource(
		*CompiledViewClass,
		URpgInventoryAddressSlotWidget::AddressSlotViewModelSourceName,
		MatchingSourceCount);
	TestEqual(
		TEXT("Address-slot entry has exactly one canonical VM source"),
		MatchingSourceCount,
		1);
	TestEqual(
		TEXT("Address-slot entry has exactly two declarative leaf bindings"),
		CompiledViewClass->GetBindings().Num(),
		2);
	const TArrayView<const FMVVMViewClass_Source> CompiledSources =
		CompiledViewClass->GetSources();
	if (!CompiledSources.IsValidIndex(MatchingSourceIndex))
	{
		return false;
	}

	const FMVVMViewClass_Source& CompiledSource =
		CompiledSources[MatchingSourceIndex];
	TestEqual(
		TEXT("Address-slot source expects the exact VM class"),
		CompiledSource.GetSourceClass(),
		URpgInventoryAddressSlotViewModel::StaticClass());
	TestTrue(
		TEXT("Address-slot source is settable by its native presenter"),
		CompiledSource.CanBeSet());
	TestTrue(
		TEXT("Address-slot source is optional while the entry is pooled"),
		CompiledSource.IsOptional());
	TestEqual(
		TEXT("Address-slot source owns both authored leaf bindings"),
		CompiledSource.GetBindings().Num(),
		2);
	TestEqual(
		TEXT("Address-slot VM only permits presenter-supplied manual composition"),
		URpgInventoryAddressSlotViewModel::StaticClass()->GetMetaData(
			TEXT("MVVMAllowedContextCreationType")),
		FString(TEXT("Manual")));

	URpgInventoryAddressSlotWidget* Widget =
		CreateWidget<URpgInventoryAddressSlotWidget>(
			TestWorld.GetTestWorld(),
			EntryWidgetClass);
	if (!TestNotNull(TEXT("Canonical address-slot entry initializes"), Widget))
	{
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
	if (!TestNotNull(TEXT("Address-slot entry runtime MVVM view exists"), View) ||
		!TestNotNull(TEXT("Address-slot entry runtime view class exists"), View ? View->GetViewClass() : nullptr))
	{
		return false;
	}
	TestNull(
		TEXT("Fresh address-slot source starts empty"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::AddressSlotViewModelSourceName).GetObject());
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestTrue(
		TEXT("Address-slot entry constructs its authored Slate representation"),
		SlateWidget.IsValid()))
	{
		return false;
	}

	URpgInventoryAddressSlotViewModel* FirstViewModel =
		NewObject<URpgInventoryAddressSlotViewModel>(Widget);
	URpgInventoryAddressSlotViewModel* SecondViewModel =
		NewObject<URpgInventoryAddressSlotViewModel>(Widget);
	FRpgInventorySlotGroupView TestGroup;
	TestGroup.ContainerHandle =
		FRpgInventoryContainerHandle::MakeRoot(TEXT("PoolingTest"));
	TestGroup.ContainerId = TestGroup.ContainerHandle.ContainerId;
	TestGroup.GridSize.Width = 2;
	TestGroup.GridSize.Height = 1;
	FirstViewModel->InitializeSlot(nullptr, nullptr, TestGroup, 0, 0);
	SecondViewModel->InitializeSlot(nullptr, nullptr, TestGroup, 1, 0);
	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(
			Widget,
			nullptr);
	if (!TestNotNull(TEXT("Address-slot test coordinator initializes"), Coordinator) ||
		!TestNotNull(
			TEXT("Address-slot test interaction session initializes"),
			Coordinator ? Coordinator->GetInteractionSession() : nullptr))
	{
		return false;
	}

	Widget->SetAddressSlotViewModel(FirstViewModel);
	Widget->SetDragDropCoordinator(Coordinator);
	IUserListEntry::UpdateItemSelection(*Widget, true);
	TestEqual(
		TEXT("Address-slot entry stores VM A"),
		Widget->GetAddressSlotViewModel(),
		FirstViewModel);
	TestEqual(
		TEXT("Address-slot entry injects VM A into the exact source"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::AddressSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstViewModel));
	TestEqual(
		TEXT("Address-slot VM A owns exactly one native refresh delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("Coordinator owns exactly one address-slot delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		1);
	TestEqual(
		TEXT("Selected address-slot entry reports focused presentation"),
		Widget->GetCurrentDragDropVisualState(),
		ERpgInventorySlotDragVisualState::Focused);

	Widget->SetAddressSlotViewModel(SecondViewModel);
	TestEqual(
		TEXT("Address-slot reassignment removes VM A's native delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		0);
	TestEqual(
		TEXT("Address-slot reassignment binds VM B exactly once"),
		CountDelegateBindingsTo(SecondViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("Address-slot reassignment replaces the exact MVVM source with VM B"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::AddressSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(SecondViewModel));

	FRpgInventoryDragPayload PreviewPayload;
	PreviewPayload.SourceType = ERpgInventoryDragSourceType::EquipmentSlot;
	PreviewPayload.EquipmentSlot = ERpgEquipmentSlot::Head;
	if (!TestTrue(
			TEXT("Address pooling preview owns canonical item metadata"),
			RpgInventoryAutomationTestTypes::PopulateCanonicalSpatialItem(
				PreviewPayload,
				Widget,
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass())))
	{
		return false;
	}
	PopulateExactEquipmentSourceSnapshot(
		PreviewPayload,
		Widget,
		TEXT("AddressPoolingEquipmentSource"));
	URpgInventoryInteractionSession* InteractionSession =
		Coordinator->GetInteractionSession();
	TestTrue(
		TEXT("Address-slot test starts a valid shared interaction"),
		InteractionSession->BeginInteraction(
			PreviewPayload,
			ERpgInventoryInteractionInputMode::Mouse));
	const FRpgInventoryDropTarget SharedAddressTarget =
		URpgInventoryDragDropCoordinator::MakePlayerInventorySlotAddressTarget(
			SecondViewModel);
	InteractionSession->SetPreviewTarget(
		SharedAddressTarget,
		ERpgInventoryInteractionPreviewState::Move);

	URpgInventoryAddressSlotWidget* NonOwningPeer =
		CreateWidget<URpgInventoryAddressSlotWidget>(
			TestWorld.GetTestWorld(),
			EntryWidgetClass);
	if (!TestNotNull(
			TEXT("A second address presenter for the same target initializes"),
			NonOwningPeer))
	{
		return false;
	}
	NonOwningPeer->SetAddressSlotViewModel(SecondViewModel);
	NonOwningPeer->SetDragDropCoordinator(Coordinator);
	IUserListEntry::ReleaseEntry(*NonOwningPeer);
	TestEqual(
		TEXT("Releasing a non-owning address presenter preserves the peer preview"),
		InteractionSession->GetTarget().TargetType,
		ERpgInventoryDropTargetType::PlayerInventorySlotAddress);

	Widget->bHasExternalPreviewState = true;
	Widget->ExternalPreviewState =
		ERpgInventorySlotDragVisualState::InvalidTarget;
	Widget->RefreshDragDropVisualState();
	IUserListEntry::ReleaseEntry(*Widget);
	TestEqual(
		TEXT("Releasing the address presenter that owns the preview clears it"),
		InteractionSession->GetTarget().TargetType,
		ERpgInventoryDropTargetType::None);
	TestNull(
		TEXT("Released address-slot entry no longer represents a VM"),
		Widget->GetAddressSlotViewModel());
	TestNull(
		TEXT("Released address-slot entry clears its optional MVVM source"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::AddressSlotViewModelSourceName).GetObject());
	TestNull(
		TEXT("Released address-slot entry drops its screen coordinator"),
		Widget->GetDragDropCoordinator());
	TestEqual(
		TEXT("Released address-slot VM B no longer targets the pooled widget"),
		CountDelegateBindingsTo(SecondViewModel->OnSlotChanged, Widget),
		0);
	TestEqual(
		TEXT("Released address-slot coordinator no longer targets the pooled widget"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		0);
	TestFalse(
		TEXT("Released address-slot entry clears external preview ownership"),
		Widget->bHasExternalPreviewState);
	TestEqual(
		TEXT("Released address-slot entry returns to normal drag presentation"),
		Widget->GetCurrentDragDropVisualState(),
		ERpgInventorySlotDragVisualState::Normal);

	SecondViewModel->OnSlotChanged.Broadcast(SecondViewModel);
	TestNull(
		TEXT("A released address-slot VM cannot repopulate the cleared MVVM source"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::AddressSlotViewModelSourceName).GetObject());

	Widget->SetAddressSlotViewModel(FirstViewModel);
	Widget->SetDragDropCoordinator(Coordinator);
	TestEqual(
		TEXT("The pooled address-slot entry cleanly rebinds VM A"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::AddressSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstViewModel));
	TestEqual(
		TEXT("The pooled address-slot entry restores exactly one VM delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("The pooled address-slot entry restores exactly one coordinator delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		1);

	IUserListEntry::ReleaseEntry(*Widget);
	SlateWidget.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCarrySlotPresentationLifecycleTest,
	"SurvivalRpg.Inventory.UI.CarrySlotPresentationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCarrySlotPresentationLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* CarryWidgetClass =
		LoadClass<URpgInventoryCarrySlotWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
				"CUI_CarrySlot.CUI_CarrySlot_C"));
	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(CarryWidgetClass);
	if (!TestNotNull(
			TEXT("Canonical Carry slot class loads"),
			CarryWidgetClass) ||
		!TestNotNull(
			TEXT("Canonical Carry slot is a Widget Blueprint"),
			GeneratedClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Carry slot initializes without a player context"),
		GeneratedClass->
			bCanCallInitializedWithoutPlayerContext);

	const TArray<UWidgetBlueprintGeneratedClassExtension*>
		CompiledViewExtensions =
			GeneratedClass->GetExtensions(
				UMVVMViewClass::StaticClass(),
				false);
	TestEqual(
		TEXT("Carry slot owns exactly one compiled MVVM view"),
		CompiledViewExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledViewClass =
		CompiledViewExtensions.Num() == 1
			? Cast<UMVVMViewClass>(
				CompiledViewExtensions[0])
			: nullptr;
	if (!TestNotNull(
			TEXT("Carry slot compiled MVVM view exists"),
			CompiledViewClass))
	{
		return false;
	}

	int32 MatchingSourceCount = 0;
	const int32 MatchingSourceIndex =
		FindNamedViewModelSource(
			*CompiledViewClass,
			URpgInventoryAddressSlotWidget::
				AddressSlotViewModelSourceName,
			MatchingSourceCount);
	TestEqual(
		TEXT("Carry slot has exactly one canonical Address VM source"),
		MatchingSourceCount,
		1);
	TestEqual(
		TEXT("Carry slot owns exactly two declarative item bindings"),
		CompiledViewClass->GetBindings().Num(),
		2);
	const TArrayView<const FMVVMViewClass_Source>
		CompiledSources =
			CompiledViewClass->GetSources();
	if (!CompiledSources.IsValidIndex(
			MatchingSourceIndex))
	{
		return false;
	}

	const FMVVMViewClass_Source& CompiledSource =
		CompiledSources[MatchingSourceIndex];
	TestEqual(
		TEXT("Carry source expects the exact Address VM class"),
		CompiledSource.GetSourceClass(),
		URpgInventoryAddressSlotViewModel::StaticClass());
	TestTrue(
		TEXT("Carry source is manually settable"),
		CompiledSource.CanBeSet());
	TestTrue(
		TEXT("Carry source is optional while the screen is pooled"),
		CompiledSource.IsOptional());
	TestEqual(
		TEXT("Carry source owns both declarative item bindings"),
		CompiledSource.GetBindings().Num(),
		2);

	URpgInventoryCarrySlotWidget* Widget =
		CreateWidget<URpgInventoryCarrySlotWidget>(
			TestWorld.GetTestWorld(),
			CarryWidgetClass);
	if (!TestNotNull(
			TEXT("Canonical Carry slot initializes"),
			Widget))
	{
		return false;
	}
	UMVVMView* View =
		UMVVMSubsystem::GetViewFromUserWidget(Widget);
	if (!TestNotNull(
			TEXT("Carry slot runtime MVVM view exists"),
			View))
	{
		return false;
	}
	TestNull(
		TEXT("Fresh Carry source starts empty"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::
				AddressSlotViewModelSourceName).GetObject());
	TSharedPtr<SWidget> SlateWidget =
		Widget->TakeWidget();
	if (!TestTrue(
			TEXT("Carry slot constructs its authored Slate representation"),
			SlateWidget.IsValid()))
	{
		return false;
	}

	auto MakeCarryGroupView =
		[](
			FName ContainerId,
			ERpgEquipmentSlot EquipmentSlot)
		{
			FRpgInventorySlotGroupView GroupView;
			GroupView.ContainerHandle =
				FRpgInventoryContainerHandle::MakeRoot(
					ContainerId);
			GroupView.ContainerId = ContainerId;
			GroupView.DisplayName =
				FText::FromName(ContainerId);
			GroupView.GroupKind =
				ERpgInventorySlotGroupKind::Carry;
			GroupView.EquipmentSlotRole =
				EquipmentSlot;
			GroupView.GridSize.Width = 1;
			GroupView.GridSize.Height = 1;
			return GroupView;
		};
	auto MakeCarryGroup =
		[Widget, &MakeCarryGroupView](
			FName ContainerId,
			ERpgEquipmentSlot EquipmentSlot,
			URpgInventorySlotGroupViewModel*& OutGroup,
			URpgInventoryAddressSlotViewModel*& OutAddress)
		{
			const FRpgInventorySlotGroupView GroupView =
				MakeCarryGroupView(
					ContainerId,
					EquipmentSlot);

			OutAddress =
				NewObject<URpgInventoryAddressSlotViewModel>(
					Widget);
			OutAddress->InitializeSlot(
				nullptr,
				nullptr,
				GroupView,
				0,
				0);
			OutGroup =
				NewObject<URpgInventorySlotGroupViewModel>(
					Widget);
			OutGroup->InitializeGroup(
				GroupView,
				{ OutAddress });
		};

	URpgInventorySlotGroupViewModel* FirstGroup =
		nullptr;
	URpgInventoryAddressSlotViewModel* FirstAddress =
		nullptr;
	URpgInventorySlotGroupViewModel* SecondGroup =
		nullptr;
	URpgInventoryAddressSlotViewModel* SecondAddress =
		nullptr;
	MakeCarryGroup(
		TEXT("CarryLifecycleA"),
		ERpgEquipmentSlot::MainHand,
		FirstGroup,
		FirstAddress);
	MakeCarryGroup(
		TEXT("CarryLifecycleB"),
		ERpgEquipmentSlot::OffHand,
		SecondGroup,
		SecondAddress);

	Widget->SetCarrySlotGroupViewModel(FirstGroup);
	TestEqual(
		TEXT("Carry slot stores group A"),
		Widget->GetCarrySlotGroupViewModel(),
		FirstGroup);
	TestEqual(
		TEXT("Carry slot resolves address A"),
		Widget->GetAddressSlotViewModel(),
		FirstAddress);
	TestEqual(
		TEXT("Carry slot injects address A into its exact source"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::
				AddressSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstAddress));
	TestEqual(
		TEXT("Carry address A owns exactly one native observer"),
		CountDelegateBindingsTo(
			FirstAddress->OnSlotChanged,
			Widget),
		1);

	Widget->SetCarrySlotGroupViewModel(SecondGroup);
	TestEqual(
		TEXT("Carry rebind removes address A's observer"),
		CountDelegateBindingsTo(
			FirstAddress->OnSlotChanged,
			Widget),
		0);
	TestEqual(
		TEXT("Carry rebind adds exactly one observer to address B"),
		CountDelegateBindingsTo(
			SecondAddress->OnSlotChanged,
			Widget),
		1);
	TestEqual(
		TEXT("Carry rebind replaces the exact source with address B"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::
				AddressSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(SecondAddress));

	Widget->ReleaseInventoryPresentation();
	TestNull(
		TEXT("Released Carry slot clears its group"),
		Widget->GetCarrySlotGroupViewModel());
	TestNull(
		TEXT("Released Carry slot clears its address"),
		Widget->GetAddressSlotViewModel());
	TestNull(
		TEXT("Released Carry slot clears its optional MVVM source"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::
				AddressSlotViewModelSourceName).GetObject());
	TestNull(
		TEXT("Released Carry slot clears its coordinator"),
		Widget->GetDragDropCoordinator());
	TestEqual(
		TEXT("Released Carry address B owns no observer"),
		CountDelegateBindingsTo(
			SecondAddress->OnSlotChanged,
			Widget),
		0);
	TestEqual(
		TEXT("Released Carry slot returns to Empty presentation"),
		Widget->GetCarryPresentationState(),
		ERpgInventoryCarryPresentationState::Empty);
	TestEqual(
		TEXT("Released Carry slot clears its interaction preview"),
		Widget->GetCarryInteractionPreviewState(),
		ERpgInventoryInteractionPreviewState::None);

	Widget->SetCarrySlotGroupViewModel(FirstGroup);
	TestEqual(
		TEXT("Pooled Carry slot restores address A"),
		View->GetViewModel(
			URpgInventoryAddressSlotWidget::
				AddressSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstAddress));
	TestEqual(
		TEXT("Pooled Carry slot restores exactly one address observer"),
		CountDelegateBindingsTo(
			FirstAddress->OnSlotChanged,
			Widget),
		1);

	auto VerifyRejectedGroup =
		[this, Widget, View](
			const TCHAR* Label,
			URpgInventorySlotGroupViewModel* RejectedGroup)
		{
			Widget->SetCarrySlotGroupViewModel(RejectedGroup);
			const FString Prefix(Label);
			bool bValid = true;
			bValid &= TestNull(
				*(Prefix + TEXT(" clears the malformed group")),
				Widget->GetCarrySlotGroupViewModel());
			bValid &= TestNull(
				*(Prefix + TEXT(" clears the malformed address")),
				Widget->GetAddressSlotViewModel());
			bValid &= TestNull(
				*(Prefix + TEXT(" clears the exact optional source")),
				View->GetViewModel(
					URpgInventoryAddressSlotWidget::
						AddressSlotViewModelSourceName).GetObject());
			for (URpgInventoryAddressSlotViewModel* Address :
				RejectedGroup
					? RejectedGroup->GetSlots()
					: TArray<URpgInventoryAddressSlotViewModel*>())
			{
				if (Address)
				{
					bValid &= TestEqual(
						*(Prefix + TEXT(" installs no malformed observer")),
						CountDelegateBindingsTo(
							Address->OnSlotChanged,
							Widget),
						0);
				}
			}
			return bValid;
		};

	const FRpgInventorySlotGroupView NoSlotsView =
		MakeCarryGroupView(
			TEXT("CarryMalformedEmpty"),
			ERpgEquipmentSlot::MainHand);
	URpgInventorySlotGroupViewModel* NoSlotsGroup =
		NewObject<URpgInventorySlotGroupViewModel>(Widget);
	NoSlotsGroup->InitializeGroup(NoSlotsView, {});
	VerifyRejectedGroup(
		TEXT("A 1x1 Carry group without an address"),
		NoSlotsGroup);

	const FRpgInventorySlotGroupView TwoSlotsView =
		MakeCarryGroupView(
			TEXT("CarryMalformedTwoSlots"),
			ERpgEquipmentSlot::MainHand);
	URpgInventoryAddressSlotViewModel* TwoSlotsAddressA =
		NewObject<URpgInventoryAddressSlotViewModel>(Widget);
	URpgInventoryAddressSlotViewModel* TwoSlotsAddressB =
		NewObject<URpgInventoryAddressSlotViewModel>(Widget);
	TwoSlotsAddressA->InitializeSlot(
		nullptr,
		nullptr,
		TwoSlotsView,
		0,
		0);
	TwoSlotsAddressB->InitializeSlot(
		nullptr,
		nullptr,
		TwoSlotsView,
		0,
		0);
	URpgInventorySlotGroupViewModel* TwoSlotsGroup =
		NewObject<URpgInventorySlotGroupViewModel>(Widget);
	TwoSlotsGroup->InitializeGroup(
		TwoSlotsView,
		{ TwoSlotsAddressA, TwoSlotsAddressB });
	VerifyRejectedGroup(
		TEXT("A 1x1 Carry group with two addresses"),
		TwoSlotsGroup);

	const FRpgInventorySlotGroupView MainHandView =
		MakeCarryGroupView(
			TEXT("CarryMalformedRole"),
			ERpgEquipmentSlot::MainHand);
	FRpgInventorySlotGroupView OffHandAddressView =
		MainHandView;
	OffHandAddressView.EquipmentSlotRole =
		ERpgEquipmentSlot::OffHand;
	URpgInventoryAddressSlotViewModel* WrongRoleAddress =
		NewObject<URpgInventoryAddressSlotViewModel>(Widget);
	WrongRoleAddress->InitializeSlot(
		nullptr,
		nullptr,
		OffHandAddressView,
		0,
		0);
	URpgInventorySlotGroupViewModel* WrongRoleGroup =
		NewObject<URpgInventorySlotGroupViewModel>(Widget);
	WrongRoleGroup->InitializeGroup(
		MainHandView,
		{ WrongRoleAddress });
	VerifyRejectedGroup(
		TEXT("A Carry group with a cross-role address"),
		WrongRoleGroup);

	const FRpgInventorySlotGroupView GroupHandleView =
		MakeCarryGroupView(
			TEXT("CarryMalformedGroupHandle"),
			ERpgEquipmentSlot::MainHand);
	const FRpgInventorySlotGroupView ForeignAddressView =
		MakeCarryGroupView(
			TEXT("CarryMalformedForeignHandle"),
			ERpgEquipmentSlot::MainHand);
	URpgInventoryAddressSlotViewModel* ForeignHandleAddress =
		NewObject<URpgInventoryAddressSlotViewModel>(Widget);
	ForeignHandleAddress->InitializeSlot(
		nullptr,
		nullptr,
		ForeignAddressView,
		0,
		0);
	URpgInventorySlotGroupViewModel* ForeignHandleGroup =
		NewObject<URpgInventorySlotGroupViewModel>(Widget);
	ForeignHandleGroup->InitializeGroup(
		GroupHandleView,
		{ ForeignHandleAddress });
	VerifyRejectedGroup(
		TEXT("A Carry group with a foreign address handle"),
		ForeignHandleGroup);

	Widget->ReleaseInventoryPresentation();
	SlateWidget.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentSlotLifecycleTest,
	"SurvivalRpg.Inventory.UI.EquipmentSlotLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentSlotLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* GearWidgetClass = LoadClass<URpgEquipmentSlotWidget>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_GearSlot.CUI_GearSlot_C"));
	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(GearWidgetClass);
	if (!TestNotNull(TEXT("Canonical equipment-slot class loads"), GearWidgetClass) ||
		!TestNotNull(TEXT("Canonical equipment-slot is a Widget Blueprint"), GeneratedClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Data-only equipment slot initializes without a player context"),
		GeneratedClass->bCanCallInitializedWithoutPlayerContext);

	const TArray<UWidgetBlueprintGeneratedClassExtension*> CompiledViewExtensions =
		GeneratedClass->GetExtensions(UMVVMViewClass::StaticClass(), false);
	TestEqual(
		TEXT("Canonical equipment slot owns exactly one compiled MVVM view"),
		CompiledViewExtensions.Num(),
		1);
	const UMVVMViewClass* CompiledViewClass =
		CompiledViewExtensions.Num() == 1
			? Cast<UMVVMViewClass>(CompiledViewExtensions[0])
			: nullptr;
	if (!TestNotNull(TEXT("Equipment-slot MVVM view class exists"), CompiledViewClass))
	{
		return false;
	}

	int32 MatchingSourceCount = 0;
	const int32 MatchingSourceIndex = FindNamedViewModelSource(
		*CompiledViewClass,
		URpgEquipmentSlotWidget::EquipmentSlotViewModelSourceName,
		MatchingSourceCount);
	TestEqual(
		TEXT("Equipment slot has exactly one canonical VM source"),
		MatchingSourceCount,
		1);
	TestEqual(
		TEXT("Equipment slot has exactly one declarative leaf binding"),
		CompiledViewClass->GetBindings().Num(),
		1);
	const TArrayView<const FMVVMViewClass_Source> CompiledSources =
		CompiledViewClass->GetSources();
	if (!CompiledSources.IsValidIndex(MatchingSourceIndex))
	{
		return false;
	}

	const FMVVMViewClass_Source& CompiledSource =
		CompiledSources[MatchingSourceIndex];
	TestEqual(
		TEXT("Equipment-slot source expects the exact VM class"),
		CompiledSource.GetSourceClass(),
		URpgEquipmentSlotViewModel::StaticClass());
	TestTrue(
		TEXT("Equipment-slot source is settable by its native presenter"),
		CompiledSource.CanBeSet());
	TestTrue(
		TEXT("Equipment-slot source is optional while the screen is pooled"),
		CompiledSource.IsOptional());
	TestEqual(
		TEXT("Equipment-slot source owns the authored icon binding"),
		CompiledSource.GetBindings().Num(),
		1);
	TestEqual(
		TEXT("Equipment-slot VM only permits presenter-supplied manual composition"),
		URpgEquipmentSlotViewModel::StaticClass()->GetMetaData(
			TEXT("MVVMAllowedContextCreationType")),
		FString(TEXT("Manual")));

	URpgEquipmentSlotWidget* Widget =
		CreateWidget<URpgEquipmentSlotWidget>(
			TestWorld.GetTestWorld(),
			GearWidgetClass);
	if (!TestNotNull(TEXT("Canonical equipment slot initializes"), Widget))
	{
		return false;
	}

	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(Widget);
	if (!TestNotNull(TEXT("Equipment-slot runtime MVVM view exists"), View) ||
		!TestNotNull(TEXT("Equipment-slot runtime view class exists"), View ? View->GetViewClass() : nullptr))
	{
		return false;
	}
	TestNull(
		TEXT("Fresh equipment-slot source starts empty"),
		View->GetViewModel(
			URpgEquipmentSlotWidget::EquipmentSlotViewModelSourceName).GetObject());
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestTrue(
		TEXT("Equipment slot constructs its authored Slate representation"),
		SlateWidget.IsValid()))
	{
		return false;
	}

	URpgEquipmentSlotViewModel* FirstViewModel =
		NewObject<URpgEquipmentSlotViewModel>(Widget);
	URpgEquipmentSlotViewModel* SecondViewModel =
		NewObject<URpgEquipmentSlotViewModel>(Widget);
	FirstViewModel->InitializeSlot(ERpgEquipmentSlot::Head, nullptr);
	SecondViewModel->InitializeSlot(ERpgEquipmentSlot::Chest, nullptr);
	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(
			Widget,
			nullptr);
	if (!TestNotNull(TEXT("Equipment-slot test coordinator initializes"), Coordinator) ||
		!TestNotNull(
			TEXT("Equipment-slot test interaction session initializes"),
			Coordinator ? Coordinator->GetInteractionSession() : nullptr))
	{
		return false;
	}

	Widget->SetEquipmentSlotViewModel(FirstViewModel);
	Widget->SetDragDropCoordinator(Coordinator);
	TestEqual(
		TEXT("Equipment slot stores VM A"),
		Widget->GetEquipmentSlotViewModel(),
		FirstViewModel);
	TestEqual(
		TEXT("Equipment slot injects VM A into the exact source"),
		View->GetViewModel(
			URpgEquipmentSlotWidget::EquipmentSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstViewModel));
	TestEqual(
		TEXT("Equipment VM A owns exactly one native refresh delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("Coordinator owns exactly one equipment-slot delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		1);

	Widget->SetEquipmentSlotViewModel(SecondViewModel);
	TestEqual(
		TEXT("Equipment reassignment removes VM A's native delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		0);
	TestEqual(
		TEXT("Equipment reassignment binds VM B exactly once"),
		CountDelegateBindingsTo(SecondViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("Equipment reassignment replaces the exact MVVM source with VM B"),
		View->GetViewModel(
			URpgEquipmentSlotWidget::EquipmentSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(SecondViewModel));

	FRpgInventoryDragPayload PreviewPayload;
	PreviewPayload.SourceType = ERpgInventoryDragSourceType::EquipmentSlot;
	PreviewPayload.EquipmentSlot = ERpgEquipmentSlot::Head;
	if (!TestTrue(
			TEXT("Equipment pooling preview owns canonical item metadata"),
			RpgInventoryAutomationTestTypes::PopulateCanonicalSpatialItem(
				PreviewPayload,
				Widget,
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass())))
	{
		return false;
	}
	PopulateExactEquipmentSourceSnapshot(
		PreviewPayload,
		Widget,
		TEXT("EquipmentPoolingSource"));
	URpgInventoryInteractionSession* InteractionSession =
		Coordinator->GetInteractionSession();
	TestTrue(
		TEXT("Equipment-slot test starts a valid shared interaction"),
		InteractionSession->BeginInteraction(
			PreviewPayload,
			ERpgInventoryInteractionInputMode::Mouse));
	InteractionSession->SetPreviewTarget(
		URpgInventoryDragDropCoordinator::MakeEquipmentTarget(
			ERpgEquipmentSlot::Chest),
		ERpgInventoryInteractionPreviewState::Equip);

	URpgEquipmentSlotWidget* NonOwningPeer =
		CreateWidget<URpgEquipmentSlotWidget>(
			TestWorld.GetTestWorld(),
			GearWidgetClass);
	if (!TestNotNull(
			TEXT("A second equipment presenter for the same target initializes"),
			NonOwningPeer))
	{
		return false;
	}
	NonOwningPeer->SetEquipmentSlotViewModel(SecondViewModel);
	NonOwningPeer->SetDragDropCoordinator(Coordinator);
	NonOwningPeer->ReleaseEquipmentSlotState();
	TestEqual(
		TEXT("Releasing a non-owning equipment presenter preserves the peer preview"),
		InteractionSession->GetTarget().TargetType,
		ERpgInventoryDropTargetType::EquipmentSlot);

	Widget->bHasExternalPreviewState = true;
	Widget->ExternalPreviewState =
		ERpgInventorySlotDragVisualState::InvalidTarget;
	Widget->RefreshDragDropVisualState();
	Widget->ReleaseEquipmentSlotState();
	TestEqual(
		TEXT("Releasing the equipment presenter that owns the preview clears it"),
		InteractionSession->GetTarget().TargetType,
		ERpgInventoryDropTargetType::None);
	TestNull(
		TEXT("Released equipment slot no longer represents a VM"),
		Widget->GetEquipmentSlotViewModel());
	TestNull(
		TEXT("Released equipment slot clears its optional MVVM source"),
		View->GetViewModel(
			URpgEquipmentSlotWidget::EquipmentSlotViewModelSourceName).GetObject());
	TestNull(
		TEXT("Released equipment slot drops its screen coordinator"),
		Widget->GetDragDropCoordinator());
	TestEqual(
		TEXT("Released equipment VM B no longer targets the widget"),
		CountDelegateBindingsTo(SecondViewModel->OnSlotChanged, Widget),
		0);
	TestEqual(
		TEXT("Released equipment coordinator no longer targets the widget"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		0);
	TestFalse(
		TEXT("Released equipment slot clears external preview ownership"),
		Widget->bHasExternalPreviewState);
	TestEqual(
		TEXT("Released equipment slot returns to normal drag presentation"),
		Widget->GetCurrentDragDropVisualState(),
		ERpgInventorySlotDragVisualState::Normal);

	Widget->SetEquipmentSlotViewModel(FirstViewModel);
	Widget->SetDragDropCoordinator(Coordinator);
	TestEqual(
		TEXT("The pooled equipment slot cleanly rebinds VM A"),
		View->GetViewModel(
			URpgEquipmentSlotWidget::EquipmentSlotViewModelSourceName).GetObject(),
		static_cast<UObject*>(FirstViewModel));
	TestEqual(
		TEXT("The pooled equipment slot restores exactly one VM delegate"),
		CountDelegateBindingsTo(FirstViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("The pooled equipment slot restores exactly one coordinator delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		1);

	Widget->ReleaseEquipmentSlotState();
	SlateWidget.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgSpatialItemPresentationLifecycleTest,
	"SurvivalRpg.Inventory.UI.SpatialItemPresentationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgSpatialItemPresentationLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgPlayerInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* SpatialItemWidgetClass = LoadClass<URpgInventorySpatialItemWidget>(
		nullptr,
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryItem.CUI_SpatialInventoryItem_C"));
	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(SpatialItemWidgetClass);
	if (!TestNotNull(TEXT("Canonical spatial-item class loads"), SpatialItemWidgetClass) ||
		!TestNotNull(TEXT("Canonical spatial item is a Widget Blueprint"), GeneratedClass))
	{
		return false;
	}
	TestEqual(
		TEXT("Canonical spatial item owns no ambiguous compiled MVVM extension"),
		GeneratedClass->GetExtensions(UMVVMViewClass::StaticClass(), false).Num(),
		0);

	URpgInventorySpatialItemWidget* Widget =
		CreateWidget<URpgInventorySpatialItemWidget>(
			TestWorld.GetTestWorld(),
			SpatialItemWidgetClass);
	if (!TestNotNull(TEXT("Canonical spatial-item presenter initializes"), Widget))
	{
		return false;
	}
	TestNull(
		TEXT("Spatial-item presenter deliberately owns no ambiguous MVVM view"),
		UMVVMSubsystem::GetViewFromUserWidget(Widget));
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestTrue(
		TEXT("Native spatial-item presenter constructs its Slate representation"),
		SlateWidget.IsValid()))
	{
		return false;
	}

	URpgInventoryAddressSlotViewModel* AddressViewModel =
		NewObject<URpgInventoryAddressSlotViewModel>(Widget);
	URpgInventoryEntryViewModel* EntryViewModel =
		NewObject<URpgInventoryEntryViewModel>(Widget);
	URpgInventoryDragDropCoordinator* Coordinator =
		NewObject<URpgInventoryDragDropCoordinator>(Widget);

	Widget->SetAddressSlotViewModel(AddressViewModel);
	Widget->SetDragDropCoordinator(Coordinator);
	TestEqual(
		TEXT("Spatial item stores its address VM"),
		Widget->GetAddressSlotViewModel(),
		AddressViewModel);
	TestEqual(
		TEXT("Address VM owns exactly one spatial-item refresh delegate"),
		CountDelegateBindingsTo(AddressViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("Coordinator owns exactly one spatial-item delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		1);

	Widget->SetEntryViewModel(EntryViewModel);
	TestNull(
		TEXT("Storage-entry mode clears the mutually exclusive address VM"),
		Widget->GetAddressSlotViewModel());
	TestEqual(
		TEXT("Storage-entry mode stores the entry VM"),
		Widget->GetEntryViewModel(),
		EntryViewModel);
	TestEqual(
		TEXT("Switching modes removes the address VM delegate"),
		CountDelegateBindingsTo(AddressViewModel->OnSlotChanged, Widget),
		0);
	TestEqual(
		TEXT("Storage entry owns exactly one spatial-item refresh delegate"),
		CountDelegateBindingsTo(EntryViewModel->OnEntryChanged, Widget),
		1);

	Widget->bPendingLeftClickAccept = true;
	Widget->bHasPendingPointerDragAnchor = true;
	Widget->ReleaseSpatialItemState();
	TestNull(
		TEXT("Released spatial item clears its entry VM"),
		Widget->GetEntryViewModel());
	TestNull(
		TEXT("Released spatial item clears its coordinator"),
		Widget->DragDropCoordinator.Get());
	TestNull(
		TEXT("Released spatial item clears its owning grid"),
		Widget->OwningGrid.Get());
	TestEqual(
		TEXT("Released entry VM no longer targets the spatial item"),
		CountDelegateBindingsTo(EntryViewModel->OnEntryChanged, Widget),
		0);
	TestEqual(
		TEXT("Released coordinator no longer targets the spatial item"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		0);
	TestFalse(
		TEXT("Released spatial item clears pending click ownership"),
		Widget->bPendingLeftClickAccept);
	TestFalse(
		TEXT("Released spatial item clears its captured drag anchor"),
		Widget->bHasPendingPointerDragAnchor);
	TestEqual(
		TEXT("Released spatial item returns to normal drag presentation"),
		Widget->GetCurrentDragDropVisualState(),
		ERpgInventorySlotDragVisualState::Normal);

	Widget->SetAddressSlotViewModel(AddressViewModel);
	Widget->SetDragDropCoordinator(Coordinator);
	TestEqual(
		TEXT("A reused spatial item restores exactly one address VM delegate"),
		CountDelegateBindingsTo(AddressViewModel->OnSlotChanged, Widget),
		1);
	TestEqual(
		TEXT("A reused spatial item restores exactly one coordinator delegate"),
		CountDelegateBindingsTo(Coordinator->OnHeldPayloadChanged, Widget),
		1);

	Widget->ReleaseSpatialItemState();
	SlateWidget.Reset();
	return true;
}

#endif
