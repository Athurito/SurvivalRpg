#include "RpgInventoryContextActionEntryWidget.h"
#include "RpgInventoryContextMenuWidget.h"
#include "RpgInventoryDropConfirmationDialogWidget.h"
#include "RpgInventorySplitDialogWidget.h"
#include "RpgQuickAccessSlotPickerEntryWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/UI/RpgInventoryAddressSlotWidget.h"
#include "SurvivalRpg/UI/RpgInventoryContextActionSource.h"
#include "SurvivalRpg/UI/RpgInventoryFeedbackToastWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialPaneWidget.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/PanelWidget.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ICommonInputModule.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewClass.h"
#include "Widgets/SWidget.h"

namespace RpgInventoryActionWidgetsTests
{
	constexpr TCHAR ActionEntryPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventoryContextActionEntrySpatial");
	constexpr TCHAR ActionEntryClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventoryContextActionEntrySpatial."
			"CUI_InventoryContextActionEntrySpatial_C");
	constexpr TCHAR QuickAccessEntryPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_QuickAccessSlotPickerEntrySpatial");
	constexpr TCHAR QuickAccessEntryClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_QuickAccessSlotPickerEntrySpatial."
			"CUI_QuickAccessSlotPickerEntrySpatial_C");
	constexpr TCHAR ContextMenuPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventoryContextMenuSpatial");
	constexpr TCHAR ContextMenuClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventoryContextMenuSpatial."
			"CUI_InventoryContextMenuSpatial_C");
	constexpr TCHAR SplitDialogPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventorySplitDialogSpatial");
	constexpr TCHAR SplitDialogClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventorySplitDialogSpatial."
			"CUI_InventorySplitDialogSpatial_C");
	constexpr TCHAR DropConfirmationPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventoryDropConfirmationSpatial");
	constexpr TCHAR DropConfirmationClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventoryDropConfirmationSpatial."
			"CUI_InventoryDropConfirmationSpatial_C");
	constexpr TCHAR FeedbackToastPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventoryFeedbackToastSpatial");
	constexpr TCHAR FeedbackToastClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/Presentation/"
			"CUI_InventoryFeedbackToastSpatial."
			"CUI_InventoryFeedbackToastSpatial_C");
	constexpr TCHAR SpatialPaneClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryPane.CUI_SpatialInventoryPane_C");
	constexpr TCHAR PlayerInventoryClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_PlayerInventory.CUI_PlayerInventory_C");

	class FScopedWidgetWorld
	{
	public:
		FScopedWidgetWorld()
		{
			GameInstance =
				NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
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

		URpgInventoryManagerComponent* CreateInventory(
			const TCHAR* DebugName)
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				AActor::StaticClass(),
				FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			AActor* OwnerActor =
				World->SpawnActor<AActor>(SpawnParameters);
			if (!OwnerActor)
			{
				return nullptr;
			}

			URpgInventoryManagerComponent* Inventory =
				NewObject<URpgInventoryManagerComponent>(
					OwnerActor,
					MakeUniqueObjectName(
						OwnerActor,
						URpgInventoryManagerComponent::StaticClass(),
						TEXT("Inventory")),
					RF_Transient);
			OwnerActor->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	template <typename WidgetType>
	WidgetType* CreateWorldlessAuthoredWidget(UClass* WidgetClass)
	{
		if (!WidgetClass ||
			WidgetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return nullptr;
		}

		WidgetType* Widget = NewObject<WidgetType>(
			GetTransientPackage(),
			WidgetClass,
			NAME_None,
			RF_Transient);
		return Widget && Widget->Initialize()
			? Widget
			: nullptr;
	}

	int32 CountFunctionsDeclaredByClass(const UClass* Class)
	{
		int32 Count = 0;
		for (TFieldIterator<UFunction> FunctionIt(
				Class,
				EFieldIteratorFlags::ExcludeSuper);
			FunctionIt;
			++FunctionIt)
		{
			++Count;
		}
		return Count;
	}

	UClass* ReadClassDefault(
		const UObject* Defaults,
		FName PropertyName)
	{
		const FClassProperty* Property = Defaults
			? FindFProperty<FClassProperty>(
				Defaults->GetClass(),
				PropertyName)
			: nullptr;
		return Property
			? Cast<UClass>(
				Property->GetObjectPropertyValue_InContainer(Defaults))
			: nullptr;
	}

	template <typename WidgetType>
	bool HasExactWidget(
		FAutomationTestBase& Test,
		const UWidgetTree* Tree,
		FName WidgetName,
		const TCHAR* ContractLabel)
	{
		const UWidget* Widget = Tree
			? Tree->FindWidget(WidgetName)
			: nullptr;
		return Test.TestNotNull(ContractLabel, Widget) &&
			Test.TestTrue(
				*FString::Printf(
					TEXT("%s has exact type %s"),
					ContractLabel,
					*WidgetType::StaticClass()->GetName()),
				Widget && Widget->GetClass() == WidgetType::StaticClass());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgQuickAccessDisplayIndexContractTest,
	"SurvivalRpg.Inventory.QuickAccess.ContextMenuDisplayIndices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgQuickAccessDisplayIndexContractTest::RunTest(const FString& Parameters)
{
	for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
	{
		const int32 DisplayNumber = URpgInventoryContextMenuWidget::ToQuickAccessDisplayNumber(SlotIndex);
		TestEqual(
			*FString::Printf(TEXT("Internal slot %d displays as %d"), SlotIndex, SlotIndex + 1),
			DisplayNumber,
			SlotIndex + 1);
		TestEqual(
			*FString::Printf(TEXT("Display slot %d round-trips to internal slot %d"), DisplayNumber, SlotIndex),
			URpgInventoryContextMenuWidget::ToQuickAccessSlotIndex(DisplayNumber),
			SlotIndex);
	}

	TestEqual(TEXT("Negative internal indices are rejected"),
		URpgInventoryContextMenuWidget::ToQuickAccessDisplayNumber(INDEX_NONE), INDEX_NONE);
	TestEqual(TEXT("Internal index eight is rejected"),
		URpgInventoryContextMenuWidget::ToQuickAccessDisplayNumber(8), INDEX_NONE);
	TestEqual(TEXT("Display number zero is rejected"),
		URpgInventoryContextMenuWidget::ToQuickAccessSlotIndex(0), INDEX_NONE);
	TestEqual(TEXT("Display number nine is rejected"),
		URpgInventoryContextMenuWidget::ToQuickAccessSlotIndex(9), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryAuthoredPresentationContractTest,
	"SurvivalRpg.Inventory.UI.AuthoredActionPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryAuthoredPresentationContractTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryActionWidgetsTests;

	UClass* ActionEntryClass =
		LoadClass<URpgInventoryContextActionEntryWidget>(
			nullptr,
			ActionEntryClassPath);
	UClass* QuickAccessEntryClass =
		LoadClass<URpgQuickAccessSlotPickerEntryWidget>(
			nullptr,
			QuickAccessEntryClassPath);
	UClass* ContextMenuClass =
		LoadClass<URpgInventoryContextMenuWidget>(
			nullptr,
			ContextMenuClassPath);
	UClass* SplitDialogClass =
		LoadClass<URpgInventorySplitDialogWidget>(
			nullptr,
			SplitDialogClassPath);
	UClass* DropConfirmationClass =
		LoadClass<URpgInventoryDropConfirmationDialogWidget>(
			nullptr,
			DropConfirmationClassPath);
	UClass* FeedbackToastClass =
		LoadClass<URpgInventoryFeedbackToastWidget>(
			nullptr,
			FeedbackToastClassPath);
	if (!TestNotNull(TEXT("Authored context-action row loads"), ActionEntryClass) ||
		!TestNotNull(TEXT("Authored Quick Access row loads"), QuickAccessEntryClass) ||
		!TestNotNull(TEXT("Authored context menu loads"), ContextMenuClass) ||
		!TestNotNull(TEXT("Authored split dialog loads"), SplitDialogClass) ||
		!TestNotNull(TEXT("Authored drop confirmation loads"), DropConfirmationClass) ||
		!TestNotNull(TEXT("Authored feedback toast loads"), FeedbackToastClass))
	{
		return false;
	}

	struct FClassContract
	{
		UClass* Class = nullptr;
		UClass* ExpectedParent = nullptr;
		const TCHAR* Label = nullptr;
	};
	const FClassContract ClassContracts[] = {
		{ActionEntryClass, URpgInventoryContextActionEntryWidget::StaticClass(), TEXT("Context action row")},
		{QuickAccessEntryClass, URpgQuickAccessSlotPickerEntryWidget::StaticClass(), TEXT("Quick Access row")},
		{ContextMenuClass, URpgInventoryContextMenuWidget::StaticClass(), TEXT("Context menu")},
		{SplitDialogClass, URpgInventorySplitDialogWidget::StaticClass(), TEXT("Split dialog")},
		{
			DropConfirmationClass,
			URpgInventoryDropConfirmationDialogWidget::StaticClass(),
			TEXT("Drop confirmation")
		},
		{FeedbackToastClass, URpgInventoryFeedbackToastWidget::StaticClass(), TEXT("Feedback toast")}
	};
	for (const FClassContract& Contract : ClassContracts)
	{
		TestTrue(
			*FString::Printf(
				TEXT("%s derives from its native presentation contract"),
				Contract.Label),
			Contract.Class->IsChildOf(Contract.ExpectedParent));
		UWidgetBlueprintGeneratedClass* GeneratedClass =
			Cast<UWidgetBlueprintGeneratedClass>(Contract.Class);
		if (!TestNotNull(
			*FString::Printf(
				TEXT("%s is an authored Widget Blueprint"),
				Contract.Label),
			GeneratedClass))
		{
			return false;
		}
		TestEqual(
			*FString::Printf(
				TEXT("%s declares no Blueprint graph functions"),
				Contract.Label),
			CountFunctionsDeclaredByClass(GeneratedClass),
			0);
		TestEqual(
			*FString::Printf(
				TEXT("%s owns no MVVM extension"),
				Contract.Label),
			GeneratedClass
				->GetExtensions(UMVVMViewClass::StaticClass(), false)
				.Num(),
			0);
	}

	const UWidgetTree* ActionTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(ActionEntryClass)
			->GetWidgetTreeArchetype();
	const UWidgetTree* QuickAccessTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(QuickAccessEntryClass)
			->GetWidgetTreeArchetype();
	const UWidgetTree* ContextTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(ContextMenuClass)
			->GetWidgetTreeArchetype();
	const UWidgetTree* SplitTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(SplitDialogClass)
			->GetWidgetTreeArchetype();
	const UWidgetTree* DropConfirmationTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(DropConfirmationClass)
			->GetWidgetTreeArchetype();
	const UWidgetTree* FeedbackTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(FeedbackToastClass)
			->GetWidgetTreeArchetype();
	HasExactWidget<UTextBlock>(
		*this,
		ActionTree,
		TEXT("Text_ActionLabel"),
		TEXT("Context action label"));
	HasExactWidget<UTextBlock>(
		*this,
		QuickAccessTree,
		TEXT("Text_SlotLabel"),
		TEXT("Quick Access label"));
	HasExactWidget<UButton>(
		*this,
		ContextTree,
		TEXT("Button_Dismiss"),
		TEXT("Context dismiss target"));
	HasExactWidget<UCanvasPanel>(
		*this,
		ContextTree,
		TEXT("ContextMenuCanvas"),
		TEXT("Context position canvas"));
	HasExactWidget<UBorder>(
		*this,
		ContextTree,
		TEXT("ContextMenuBorder"),
		TEXT("Context visual border"));
	HasExactWidget<UVerticalBox>(
		*this,
		ContextTree,
		TEXT("ActionsBox"),
		TEXT("Context action host"));
	HasExactWidget<UVerticalBox>(
		*this,
		ContextTree,
		TEXT("QuickAccessSlotsBox"),
		TEXT("Quick Access picker host"));
	HasExactWidget<UButton>(
		*this,
		ContextTree,
		TEXT("Button_QuickAccessBack"),
		TEXT("Quick Access back button"));
	HasExactWidget<UButton>(
		*this,
		SplitTree,
		TEXT("Button_Backdrop"),
		TEXT("Split backdrop"));
	HasExactWidget<USlider>(
		*this,
		SplitTree,
		TEXT("Slider_Amount"),
		TEXT("Split amount slider"));
	HasExactWidget<USpinBox>(
		*this,
		SplitTree,
		TEXT("SpinBox_Amount"),
		TEXT("Split amount input"));
	HasExactWidget<UButton>(
		*this,
		SplitTree,
		TEXT("Button_Confirm"),
		TEXT("Split confirm button"));
	HasExactWidget<UButton>(
		*this,
		SplitTree,
		TEXT("Button_Cancel"),
		TEXT("Split cancel button"));
	HasExactWidget<UButton>(
		*this,
		DropConfirmationTree,
		TEXT("Button_Backdrop"),
		TEXT("Drop confirmation backdrop"));
	HasExactWidget<UTextBlock>(
		*this,
		DropConfirmationTree,
		TEXT("Text_Message"),
		TEXT("Drop confirmation message"));
	HasExactWidget<UButton>(
		*this,
		DropConfirmationTree,
		TEXT("Button_Confirm"),
		TEXT("Drop confirmation confirm button"));
	HasExactWidget<UButton>(
		*this,
		DropConfirmationTree,
		TEXT("Button_Cancel"),
		TEXT("Drop confirmation cancel button"));
	HasExactWidget<UBorder>(
		*this,
		FeedbackTree,
		TEXT("FeedbackBorder"),
		TEXT("Feedback border"));
	HasExactWidget<UTextBlock>(
		*this,
		FeedbackTree,
		TEXT("FeedbackText"),
		TEXT("Feedback label"));

	const UObject* ContextDefaults = ContextMenuClass->GetDefaultObject();
	TestEqual(
		TEXT("Context menu uses the exact authored action-row class"),
		ReadClassDefault(ContextDefaults, TEXT("ActionEntryWidgetClass")),
		ActionEntryClass);
	TestEqual(
		TEXT("Context menu uses the exact authored Quick Access row class"),
		ReadClassDefault(ContextDefaults, TEXT("QuickAccessSlotEntryWidgetClass")),
		QuickAccessEntryClass);

	const IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry"))
			.Get();
	TArray<FName> ContextMenuDependencies;
	TestTrue(
		TEXT("Asset Registry resolves authored context-menu dependencies"),
		AssetRegistry.GetDependencies(
			FName(ContextMenuPackageName),
			ContextMenuDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));
	TestTrue(
		TEXT("Context menu owns a cook-visible dependency on its action rows"),
		ContextMenuDependencies.Contains(FName(ActionEntryPackageName)));
	TestTrue(
		TEXT("Context menu owns a cook-visible dependency on its Quick Access rows"),
		ContextMenuDependencies.Contains(FName(QuickAccessEntryPackageName)));

	struct FScreenContract
	{
		const TCHAR* ClassPath = nullptr;
		const TCHAR* PackageName = nullptr;
		const TCHAR* Label = nullptr;
	};
	const FScreenContract ScreenContracts[] = {
		{
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_PlayerInventory.CUI_PlayerInventory_C"),
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_PlayerInventory"),
			TEXT("Player Inventory")
		},
		{
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_StorageSpatial.CUI_StorageSpatial_C"),
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_StorageSpatial"),
			TEXT("Storage/Loot")
		},
		{
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_BaseTerminalSpatial.CUI_BaseTerminalSpatial_C"),
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_BaseTerminalSpatial"),
			TEXT("Base Terminal")
		},
		{
			TEXT(
				"/Game/SurvivalRpg/Crafting/UI/"
				"CUI_CraftingStationSpatial.CUI_CraftingStationSpatial_C"),
			TEXT(
				"/Game/SurvivalRpg/Crafting/UI/"
				"CUI_CraftingStationSpatial"),
			TEXT("Crafting")
		}
	};
	for (const FScreenContract& Contract : ScreenContracts)
	{
		UClass* ScreenClass =
			LoadClass<URpgInventoryInteractionScreenWidget>(
				nullptr,
				Contract.ClassPath);
		if (!TestNotNull(
			*FString::Printf(TEXT("%s screen loads"), Contract.Label),
			ScreenClass))
		{
			continue;
		}

		const UObject* ScreenDefaults = ScreenClass->GetDefaultObject();
		TestEqual(
			*FString::Printf(
				TEXT("%s uses the canonical context menu"),
				Contract.Label),
			ReadClassDefault(
				ScreenDefaults,
				TEXT("ContextMenuWidgetClass")),
			ContextMenuClass);
		TestEqual(
			*FString::Printf(
				TEXT("%s uses the canonical split dialog"),
				Contract.Label),
			ReadClassDefault(
				ScreenDefaults,
				TEXT("SplitDialogWidgetClass")),
			SplitDialogClass);
		TestEqual(
			*FString::Printf(
				TEXT("%s uses the canonical drop confirmation"),
				Contract.Label),
			ReadClassDefault(
				ScreenDefaults,
				TEXT("DropConfirmationDialogWidgetClass")),
			DropConfirmationClass);

		const UWidgetBlueprintGeneratedClass* GeneratedScreen =
			Cast<UWidgetBlueprintGeneratedClass>(ScreenClass);
		const UWidgetTree* ScreenTree = GeneratedScreen
			? GeneratedScreen->GetWidgetTreeArchetype()
			: nullptr;
		const UWidget* Toast = ScreenTree
			? ScreenTree->FindWidget(TEXT("InventoryFeedbackToast"))
			: nullptr;
		const UPanelWidget* RootPanel = ScreenTree
			? Cast<UPanelWidget>(ScreenTree->RootWidget)
			: nullptr;
		const int32 ToastRootIndex = RootPanel && Toast
			? RootPanel->GetChildIndex(const_cast<UWidget*>(Toast))
			: INDEX_NONE;
		TestTrue(
			*FString::Printf(
				TEXT("%s authors the canonical feedback toast directly"),
				Contract.Label),
			Toast && Toast->GetClass() == FeedbackToastClass &&
				ToastRootIndex != INDEX_NONE);

		const UWidget* DragVisualCanvas = ScreenTree
			? ScreenTree->FindWidget(TEXT("DragVisualCanvas"))
			: nullptr;
		const int32 DragVisualRootIndex = RootPanel && DragVisualCanvas
			? RootPanel->GetChildIndex(
				const_cast<UWidget*>(DragVisualCanvas))
			: INDEX_NONE;
		TestTrue(
			*FString::Printf(
				TEXT("%s keeps the toast before the final drag canvas"),
				Contract.Label),
			RootPanel && ToastRootIndex != INDEX_NONE &&
				DragVisualRootIndex != INDEX_NONE &&
				DragVisualRootIndex == RootPanel->GetChildrenCount() - 1 &&
				ToastRootIndex < DragVisualRootIndex);

		TArray<FName> ScreenDependencies;
		TestTrue(
			*FString::Printf(
				TEXT("Asset Registry resolves %s presentation dependencies"),
				Contract.Label),
			AssetRegistry.GetDependencies(
				FName(Contract.PackageName),
				ScreenDependencies,
				UE::AssetRegistry::EDependencyCategory::Package));
		TestTrue(
			*FString::Printf(
				TEXT("%s owns a cook-visible context-menu dependency"),
				Contract.Label),
			ScreenDependencies.Contains(FName(ContextMenuPackageName)));
		TestTrue(
			*FString::Printf(
				TEXT("%s owns a cook-visible split-dialog dependency"),
				Contract.Label),
			ScreenDependencies.Contains(FName(SplitDialogPackageName)));
		TestTrue(
			*FString::Printf(
				TEXT("%s owns a cook-visible drop-confirmation dependency"),
				Contract.Label),
			ScreenDependencies.Contains(
				FName(DropConfirmationPackageName)));
		TestTrue(
			*FString::Printf(
				TEXT("%s owns a cook-visible feedback-toast dependency"),
				Contract.Label),
			ScreenDependencies.Contains(FName(FeedbackToastPackageName)));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryActionModalPoolingLifecycleTest,
	"SurvivalRpg.Inventory.UI.ActionModalPoolingLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryActionModalPoolingLifecycleTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryActionWidgetsTests;

	// Commandlet automation does not run CommonInput's normal startup path,
	// while CommonActivatableWidget::NativeConstruct resolves its Back action.
	ICommonInputModule::GetSettings().LoadData();

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(
		TEXT("Standalone modal test world is valid"),
		TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("ActionModalPoolingInventory"));
	if (!TestNotNull(TEXT("Modal test inventory exists"), Inventory))
	{
		return false;
	}

	URpgInventoryItemInstance* ItemA =
		Inventory->GrantItemDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4);
	URpgInventoryItemInstance* ItemB =
		Inventory->GrantItemDefinition(
			URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
			5);
	FRpgInventoryGridPlacement PlacementA;
	FRpgInventoryGridPlacement PlacementB;
	if (!TestNotNull(TEXT("Modal source item A exists"), ItemA) ||
		!TestNotNull(TEXT("Modal source item B exists"), ItemB) ||
		!TestTrue(
			TEXT("Modal source item A has a placement"),
			ItemA && Inventory->GetItemPlacement(ItemA, PlacementA)) ||
		!TestTrue(
			TEXT("Modal source item B has a placement"),
			ItemB && Inventory->GetItemPlacement(ItemB, PlacementB)))
	{
		return false;
	}

	UClass* PaneClass =
		LoadClass<URpgInventorySpatialPaneWidget>(
			nullptr,
			SpatialPaneClassPath);
	URpgInventorySpatialPaneWidget* Pane = PaneClass
		? CreateWidget<URpgInventorySpatialPaneWidget>(
			TestWorld.GetTestWorld(),
			PaneClass)
		: nullptr;
	if (!TestNotNull(TEXT("Canonical spatial pane loads"), PaneClass) ||
		!TestNotNull(TEXT("Modal source pane initializes"), Pane))
	{
		return false;
	}

	TSharedPtr<SWidget> PaneSlate = Pane->TakeWidget();
	if (!TestTrue(
		TEXT("Modal source pane constructs its Slate representation"),
		PaneSlate.IsValid()))
	{
		return false;
	}

	Pane->BindInventoryContainer(
		Inventory,
		FRpgInventoryContainerHandle::MakeRoot(
			Inventory->GetDefaultContainerId()));
	URpgInventoryDragDropCoordinator* ContextCoordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(
				Pane,
				nullptr);
	Pane->SetInteractionContext(
		ContextCoordinator,
		nullptr,
		NAME_None,
		nullptr);
	URpgInventorySpatialGridWidget* Grid = Pane->GetSpatialGrid();
	if (!TestNotNull(
			TEXT("Modal source pane exposes its grid"),
			Grid) ||
		!TestNotNull(
			TEXT("Modal source owns one query coordinator"),
			ContextCoordinator))
	{
		return false;
	}
	TestTrue(
		TEXT("Spatial Content implements the shared context-source contract"),
		Grid->GetClass()->ImplementsInterface(
			URpgInventoryContextActionSource::StaticClass()));
	TestTrue(
		TEXT("Address and Carry inherit the shared context-source contract"),
		URpgInventoryAddressSlotWidget::StaticClass()->
			ImplementsInterface(
				URpgInventoryContextActionSource::StaticClass()));
	TestTrue(
		TEXT("Gear implements the shared context-source contract"),
		URpgEquipmentSlotWidget::StaticClass()->
			ImplementsInterface(
				URpgInventoryContextActionSource::StaticClass()));

	UClass* ContextClass =
		LoadClass<URpgInventoryContextMenuWidget>(
			nullptr,
			ContextMenuClassPath);
	URpgInventoryContextMenuWidget* Context =
		CreateWorldlessAuthoredWidget<
			URpgInventoryContextMenuWidget>(ContextClass);
	if (!TestNotNull(TEXT("Canonical context modal loads"), ContextClass) ||
		!TestNotNull(TEXT("Worldless context modal initializes"), Context))
	{
		return false;
	}

	TSharedPtr<SWidget> ContextSlateA = Context->TakeWidget();
	if (!TestTrue(
		TEXT("Context modal constructs its first Slate representation"),
		ContextSlateA.IsValid()) ||
		!TestTrue(
			TEXT("Grid selects item A for the context modal"),
			Grid->SelectCell(PlacementA.X, PlacementA.Y)))
	{
		return false;
	}

	const FGuid ContextEntryA = Grid->GetSelectedEntryId();
	const FRpgInventoryItemId ContextItemA =
		Grid->GetSelectedItemId();
	TestTrue(
		TEXT("Context A initializes before activation"),
		Context->InitializeContextMenu(
			Grid,
			FVector2D(120.0f, 80.0f)));
	TestEqual(
		TEXT("Context A captures entry A"),
		Context->GetContextEntryId(),
		ContextEntryA);
	TestEqual(
		TEXT("Context A captures item A"),
		Context->GetContextItemId(),
		ContextItemA);
	const UPanelWidget* ContextActionHostA =
		Cast<UPanelWidget>(
			Context->GetWidgetFromName(TEXT("ActionsBox")));
	TestEqual(
		TEXT("Context A owns exactly the queried Inspect row"),
		ContextActionHostA
			? ContextActionHostA->GetChildrenCount()
			: INDEX_NONE,
		1);

	// A is deliberately never activated. With no GameInstance on the modal,
	// only this class's NativeDestruct fallback can clear checkout state.
	ContextSlateA.Reset();
	TestFalse(
		TEXT("Context A releases its Slate representation"),
		Context->GetCachedWidget().IsValid());
	TestFalse(
		TEXT("Context destruct clears entry A"),
		Context->GetContextEntryId().IsValid());
	TestFalse(
		TEXT("Context destruct clears item A"),
		Context->GetContextItemId().IsValid());
	TestEqual(
		TEXT("Context destruct removes A's action rows"),
		ContextActionHostA
			? ContextActionHostA->GetChildrenCount()
			: INDEX_NONE,
		0);

	TSharedPtr<SWidget> ContextSlateB = Context->TakeWidget();
	if (!TestTrue(
		TEXT("Pooled context reconstructs its Slate representation"),
		ContextSlateB.IsValid()) ||
		!TestTrue(
			TEXT("Grid selects item B for the pooled context modal"),
			Grid->SelectCell(PlacementB.X, PlacementB.Y)))
	{
		return false;
	}

	const FGuid ContextEntryB = Grid->GetSelectedEntryId();
	const FRpgInventoryItemId ContextItemB =
		Grid->GetSelectedItemId();
	FGuid ContextEntrySeenOnActivation;
	const FDelegateHandle ContextActivationHandle =
		Context->OnActivated().AddLambda(
			[&ContextEntrySeenOnActivation, Context]()
			{
				ContextEntrySeenOnActivation =
					Context->GetContextEntryId();
			});
	TestTrue(
		TEXT("Pooled context B initializes before activation"),
		Context->InitializeContextMenu(
			Grid,
			FVector2D(240.0f, 160.0f)));
	Context->ActivateWidget();
	TestEqual(
		TEXT("Context activation already observes entry B"),
		ContextEntrySeenOnActivation,
		ContextEntryB);
	TestEqual(
		TEXT("Pooled context contains item B rather than A"),
		Context->GetContextItemId(),
		ContextItemB);
	TestNotEqual(
		TEXT("Pooled context B has a different item identity"),
		Context->GetContextItemId(),
		ContextItemA);
	Context->OnActivated().Remove(ContextActivationHandle);
	Context->DeactivateWidget();
	TestFalse(
		TEXT("Context deactivation clears entry B"),
		Context->GetContextEntryId().IsValid());
	TestFalse(
		TEXT("Context deactivation clears item B"),
		Context->GetContextItemId().IsValid());

	UClass* SplitClass =
		LoadClass<URpgInventorySplitDialogWidget>(
			nullptr,
			SplitDialogClassPath);
	URpgInventorySplitDialogWidget* Split =
		CreateWorldlessAuthoredWidget<
			URpgInventorySplitDialogWidget>(SplitClass);
	if (!TestNotNull(TEXT("Canonical split modal loads"), SplitClass) ||
		!TestNotNull(TEXT("Worldless split modal initializes"), Split))
	{
		return false;
	}

	TSharedPtr<SWidget> SplitSlateA = Split->TakeWidget();
	if (!TestTrue(
		TEXT("Split modal constructs its first Slate representation"),
		SplitSlateA.IsValid()) ||
		!TestTrue(
			TEXT("Grid reselects item A for the split modal"),
			Grid->SelectCell(PlacementA.X, PlacementA.Y)))
	{
		return false;
	}

	const FGuid SplitEntryA = Grid->GetSelectedEntryId();
	TestTrue(
		TEXT("Split A initializes before activation"),
		Split->InitializeSplitDialog(
			Grid,
			SplitEntryA,
			1,
			3,
			2));
	TestEqual(
		TEXT("Split A captures entry A"),
		Split->GetSplitEntryId(),
		SplitEntryA);
	TestEqual(
		TEXT("Split A exposes its exact default count"),
		Split->GetSelectedSplitCount(),
		2);

	SplitSlateA.Reset();
	TestFalse(
		TEXT("Split A releases its Slate representation"),
		Split->GetCachedWidget().IsValid());
	TestFalse(
		TEXT("Split destruct clears entry A"),
		Split->GetSplitEntryId().IsValid());
	TestEqual(
		TEXT("Split destruct restores the neutral count"),
		Split->GetSelectedSplitCount(),
		1);

	TSharedPtr<SWidget> SplitSlateB = Split->TakeWidget();
	if (!TestTrue(
		TEXT("Pooled split reconstructs its Slate representation"),
		SplitSlateB.IsValid()) ||
		!TestTrue(
			TEXT("Grid reselects item B for the pooled split modal"),
			Grid->SelectCell(PlacementB.X, PlacementB.Y)))
	{
		return false;
	}

	const FGuid SplitEntryB = Grid->GetSelectedEntryId();
	FGuid SplitEntrySeenOnActivation;
	const FDelegateHandle SplitActivationHandle =
		Split->OnActivated().AddLambda(
			[&SplitEntrySeenOnActivation, Split]()
			{
				SplitEntrySeenOnActivation =
					Split->GetSplitEntryId();
			});
	TestTrue(
		TEXT("Pooled split B initializes before activation"),
		Split->InitializeSplitDialog(
			Grid,
			SplitEntryB,
			1,
			4,
			3));
	Split->ActivateWidget();
	TestEqual(
		TEXT("Split activation already observes entry B"),
		SplitEntrySeenOnActivation,
		SplitEntryB);
	TestEqual(
		TEXT("Pooled split contains only B's amount"),
		Split->GetSelectedSplitCount(),
		3);
	Split->OnActivated().Remove(SplitActivationHandle);
	Split->DeactivateWidget();
	TestFalse(
		TEXT("Split deactivation clears entry B"),
		Split->GetSplitEntryId().IsValid());
	TestEqual(
		TEXT("Split deactivation restores the neutral count"),
		Split->GetSelectedSplitCount(),
		1);

	UClass* PlayerScreenClass =
		LoadClass<URpgInventoryInteractionScreenWidget>(
			nullptr,
			PlayerInventoryClassPath);
	URpgInventoryInteractionScreenWidget* DropHost =
		PlayerScreenClass
			? CreateWidget<URpgInventoryInteractionScreenWidget>(
				TestWorld.GetTestWorld(),
				PlayerScreenClass)
			: nullptr;
	UClass* DropClass =
		LoadClass<URpgInventoryDropConfirmationDialogWidget>(
			nullptr,
			DropConfirmationClassPath);
	URpgInventoryDropConfirmationDialogWidget* Drop =
		CreateWorldlessAuthoredWidget<
			URpgInventoryDropConfirmationDialogWidget>(DropClass);
	if (!TestNotNull(
			TEXT("Canonical inventory-screen host loads"),
			PlayerScreenClass) ||
		!TestNotNull(
			TEXT("Drop-confirmation host initializes"),
			DropHost) ||
		!TestNotNull(
			TEXT("Canonical drop-confirmation modal loads"),
			DropClass) ||
		!TestNotNull(
			TEXT("Worldless drop-confirmation modal initializes"),
			Drop))
	{
		return false;
	}

	const FGuid DropRequestA = FGuid::NewGuid();
	const FGuid DropRequestB = FGuid::NewGuid();
	TSharedPtr<SWidget> DropSlateA = Drop->TakeWidget();
	if (!TestTrue(
		TEXT("Drop modal constructs its first Slate representation"),
		DropSlateA.IsValid()))
	{
		return false;
	}

	TestTrue(
		TEXT("Drop A initializes before activation"),
		Drop->InitializeDropConfirmation(
			DropHost,
			DropRequestA,
			FText::FromString(TEXT("Item A")),
			2));
	TestEqual(
		TEXT("Drop A captures request A"),
		Drop->GetInitialRequestId(),
		DropRequestA);

	DropSlateA.Reset();
	TestFalse(
		TEXT("Drop A releases its Slate representation"),
		Drop->GetCachedWidget().IsValid());
	TestFalse(
		TEXT("Drop destruct clears request A"),
		Drop->GetInitialRequestId().IsValid());

	TSharedPtr<SWidget> DropSlateB = Drop->TakeWidget();
	if (!TestTrue(
		TEXT("Pooled drop modal reconstructs its Slate representation"),
		DropSlateB.IsValid()))
	{
		return false;
	}

	FGuid DropRequestSeenOnActivation;
	const FDelegateHandle DropActivationHandle =
		Drop->OnActivated().AddLambda(
			[&DropRequestSeenOnActivation, Drop]()
			{
				DropRequestSeenOnActivation =
					Drop->GetInitialRequestId();
			});
	TestTrue(
		TEXT("Pooled drop B initializes before activation"),
		Drop->InitializeDropConfirmation(
			DropHost,
			DropRequestB,
			FText::FromString(TEXT("Item B")),
			1));
	Drop->ActivateWidget();
	TestEqual(
		TEXT("Drop activation already observes request B"),
		DropRequestSeenOnActivation,
		DropRequestB);
	Drop->OnActivated().Remove(DropActivationHandle);
	Drop->DeactivateWidget();
	TestFalse(
		TEXT("Drop deactivation clears request B"),
		Drop->GetInitialRequestId().IsValid());

	return true;
}

#endif
