#include "RpgInventoryActionWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/UI/RpgInventoryFeedbackToastWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"

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
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewClass.h"

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

#endif
