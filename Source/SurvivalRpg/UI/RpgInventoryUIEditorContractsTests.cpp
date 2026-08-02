#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "RpgBaseResourceListWidget.h"
#include "RpgBaseTerminalWidget.h"
#include "RpgCraftingActionButtonWidget.h"
#include "RpgCraftingStationWidget.h"
#include "RpgInventoryCarrySlotWidget.h"
#include "RpgInventorySpatialPaneWidget.h"
#include "RpgPlayerInventoryPaneWidget.h"
#include "RpgPlayerInventoryWidget.h"
#include "RpgStorageInventoryWidget.h"

#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FRequiredWidgetProperty
	{
		const UClass* NativeWidgetClass = nullptr;
		const TCHAR* PropertyName = nullptr;
	};

	struct FAuthoredInventoryScreen
	{
		const TCHAR* ClassPath = nullptr;
		const UClass* ExpectedNativeClass = nullptr;
	};

	struct FExpectedActionRow
	{
		const TCHAR* PropertyName = nullptr;
		const TCHAR* TablePath = nullptr;
		const TCHAR* RowName = nullptr;
	};

	const FDataTableRowHandle* ResolveActionRow(
		FAutomationTestBase& Test,
		const UClass& WidgetClass,
		const TCHAR* PropertyName)
	{
		const FStructProperty* Property =
			FindFProperty<FStructProperty>(
				&WidgetClass,
				PropertyName);
		if (!Test.TestNotNull(
			*FString::Printf(
				TEXT("%s exposes %s"),
				*WidgetClass.GetPathName(),
				PropertyName),
			Property))
		{
			return nullptr;
		}

		if (!Test.TestTrue(
			*FString::Printf(
				TEXT("%s is a DataTableRowHandle"),
				PropertyName),
			Property->Struct.Get() ==
				FDataTableRowHandle::StaticStruct()))
		{
			return nullptr;
		}

		return Property->ContainerPtrToValuePtr<
			FDataTableRowHandle>(
				WidgetClass.GetDefaultObject());
	}

	void TestActionRow(
		FAutomationTestBase& Test,
		const UClass& WidgetClass,
		const FExpectedActionRow& Expected)
	{
		const FDataTableRowHandle* ActionRow =
			ResolveActionRow(
				Test,
				WidgetClass,
				Expected.PropertyName);
		if (!ActionRow)
		{
			return;
		}

		Test.TestNotNull(
			*FString::Printf(
				TEXT("%s.%s has an authored DataTable"),
				*WidgetClass.GetPathName(),
				Expected.PropertyName),
			ActionRow->DataTable.Get());
		if (!ActionRow->DataTable)
		{
			return;
		}

		Test.TestEqual(
			*FString::Printf(
				TEXT("%s.%s uses the canonical action table"),
				*WidgetClass.GetPathName(),
				Expected.PropertyName),
			ActionRow->DataTable->GetPathName(),
			FString(Expected.TablePath));
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s.%s uses the expected row"),
				*WidgetClass.GetPathName(),
				Expected.PropertyName),
			ActionRow->RowName,
			FName(Expected.RowName));
		Test.TestNotNull(
			*FString::Printf(
				TEXT("%s.%s resolves its authored row"),
				*WidgetClass.GetPathName(),
				Expected.PropertyName),
			ActionRow->DataTable->FindRowUnchecked(
				ActionRow->RowName));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryUIRequiredBindWidgetsTest,
	"SurvivalRpg.UI.Inventory.EditorContracts.RequiredBindWidgets",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryUIRequiredBindWidgetsTest::RunTest(
	const FString& Parameters)
{
	const FRequiredWidgetProperty RequiredProperties[] = {
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Content_Pockets") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Carry_Weapon1") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Carry_Weapon2") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Carry_Offhand") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Content_Backpack") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Content_Belt") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Content_Pouch") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Content_ResourceBag") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("ActionBarTileView") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_Head") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_Chest") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_Hands") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_Legs") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_Feet") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_Backpack") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_Belt") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_Pouch") },
		{ URpgPlayerInventoryPaneWidget::StaticClass(), TEXT("Gear_ResourceBag") },
		{ URpgPlayerInventoryWidget::StaticClass(), TEXT("PlayerInventoryPane") },
		{ URpgStorageInventoryWidget::StaticClass(), TEXT("PlayerInventoryPane") },
		{ URpgStorageInventoryWidget::StaticClass(), TEXT("SecondaryInventoryGrid") },
		{ URpgBaseTerminalWidget::StaticClass(), TEXT("BaseResourceList") },
		{ URpgInventorySpatialPaneWidget::StaticClass(), TEXT("SpatialGrid") },
		{ URpgBaseResourceListWidget::StaticClass(), TEXT("ResourceList") },
		{ URpgCraftingActionButtonWidget::StaticClass(), TEXT("Text") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("PlayerInventoryPane") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("OutputInventoryPane") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("RecipeList") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("IngredientList") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("CraftingJobsList") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("RecipeNameText") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("RecipeDescriptionText") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("CraftTimeText") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("CraftQuantityText") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("RecipeIcon") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("CraftButton") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("PauseButton") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("QuantityMinusButton") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("QuantityPlusButton") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("QuantityFiveButton") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("QuantityTenButton") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("QuantityMaxButton") },
		{ URpgCraftingStationWidget::StaticClass(), TEXT("AutoDepositCheckBox") }
	};

	for (const FRequiredWidgetProperty& Required :
		RequiredProperties)
	{
		const FProperty* Property =
			FindFProperty<FProperty>(
				Required.NativeWidgetClass,
				Required.PropertyName);
		if (!TestNotNull(
			*FString::Printf(
				TEXT("%s exposes %s"),
				*Required.NativeWidgetClass->GetName(),
				Required.PropertyName),
			Property))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s.%s is a required BindWidget"),
				*Required.NativeWidgetClass->GetName(),
				Required.PropertyName),
			Property->HasMetaData(TEXT("BindWidget")));
		TestFalse(
			*FString::Printf(
				TEXT("%s.%s is not an optional binding"),
				*Required.NativeWidgetClass->GetName(),
				Required.PropertyName),
			Property->HasMetaData(
				TEXT("BindWidgetOptional")));
	}

	for (const TCHAR* CarryPropertyName :
		{
			TEXT("Carry_Weapon1"),
			TEXT("Carry_Weapon2"),
			TEXT("Carry_Offhand")
		})
	{
		const FObjectPropertyBase* CarryProperty =
			FindFProperty<FObjectPropertyBase>(
				URpgPlayerInventoryPaneWidget::StaticClass(),
				CarryPropertyName);
		if (TestNotNull(
			*FString::Printf(
				TEXT("%s has an object property"),
				CarryPropertyName),
			CarryProperty))
		{
			TestTrue(
				*FString::Printf(
					TEXT("%s has the exact carry-slot type"),
					CarryPropertyName),
				CarryProperty->PropertyClass.Get() ==
					URpgInventoryCarrySlotWidget::
						StaticClass());
		}
	}

	const FObjectPropertyBase* TransitionalTerminalPane =
		FindFProperty<FObjectPropertyBase>(
			URpgBaseTerminalWidget::StaticClass(),
			TEXT("PlayerInventoryPane"));
	if (TestNotNull(
			TEXT("Base Terminal exposes the transitional PlayerInventoryPane binding"),
			TransitionalTerminalPane))
	{
		TestTrue(
			TEXT("Base Terminal player pane is optional during the authored migration"),
			TransitionalTerminalPane->HasMetaData(TEXT("BindWidgetOptional")));
		TestEqual(
			TEXT("Base Terminal compatibility binding accepts either supported pane class"),
			TransitionalTerminalPane->PropertyClass.Get(),
			UWidget::StaticClass());
	}

	for (const UClass* RootClass :
		{
			URpgPlayerInventoryWidget::StaticClass(),
			URpgStorageInventoryWidget::StaticClass(),
			URpgCraftingStationWidget::StaticClass()
		})
	{
		const FObjectPropertyBase* PaneProperty =
			FindFProperty<FObjectPropertyBase>(
				RootClass,
				TEXT("PlayerInventoryPane"));
		if (TestNotNull(
				*FString::Printf(
					TEXT("%s exposes the reusable PlayerInventoryPane"),
					*RootClass->GetName()),
				PaneProperty))
		{
			TestEqual(
				*FString::Printf(
					TEXT("%s.PlayerInventoryPane has the exact passive Pane type"),
					*RootClass->GetName()),
				PaneProperty->PropertyClass.Get(),
				URpgPlayerInventoryPaneWidget::StaticClass());
		}

		TestNull(
			*FString::Printf(
				TEXT("%s exposes no legacy PlayerGroupsPanel property"),
				*RootClass->GetName()),
			FindFProperty<FProperty>(RootClass, TEXT("PlayerGroupsPanel")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryUIAuthoredScreenDefaultsTest,
	"SurvivalRpg.UI.Inventory.EditorContracts.AuthoredScreenDefaults",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryUIAuthoredScreenDefaultsTest::RunTest(
	const FString& Parameters)
{
	const FAuthoredInventoryScreen Screens[] = {
		{
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_PlayerInventory.CUI_PlayerInventory_C"),
			URpgPlayerInventoryWidget::StaticClass()
		},
		{
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_StorageSpatial.CUI_StorageSpatial_C"),
			URpgStorageInventoryWidget::StaticClass()
		},
		{
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_BaseTerminalSpatial."
				"CUI_BaseTerminalSpatial_C"),
			URpgBaseTerminalWidget::StaticClass()
		},
		{
			TEXT(
				"/Game/SurvivalRpg/Crafting/UI/"
				"CUI_CraftingStationSpatial."
				"CUI_CraftingStationSpatial_C"),
			URpgCraftingStationWidget::StaticClass()
		}
	};

	constexpr TCHAR CommonActionTablePath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Input/"
			"CDT_RpgUIActions_All.CDT_RpgUIActions_All");
	const FExpectedActionRow CommonRows[] = {
		{
			TEXT("PreviousPanelInputAction"),
			CommonActionTablePath,
			TEXT("UI.Inventory.PreviousPanel")
		},
		{
			TEXT("NextPanelInputAction"),
			CommonActionTablePath,
			TEXT("UI.Inventory.NextPanel")
		},
		{
			TEXT("QuickTransferInputAction"),
			CommonActionTablePath,
			TEXT("UI.Inventory.QuickTransfer")
		},
		{
			TEXT("QuickSplitInputAction"),
			CommonActionTablePath,
			TEXT("UI.Inventory.QuickSplit")
		},
		{
			TEXT("UseOrEquipInputAction"),
			CommonActionTablePath,
			TEXT("UI.Inventory.UseOrEquip")
		},
		{
			TEXT("DropInputAction"),
			CommonActionTablePath,
			TEXT("UI.Inventory.Drop")
		}
	};

	for (const FAuthoredInventoryScreen& Screen : Screens)
	{
		UClass* WidgetClass =
			LoadClass<UUserWidget>(
				nullptr,
				Screen.ClassPath);
		if (!TestNotNull(
			*FString::Printf(
				TEXT("Authored inventory screen loads: %s"),
				Screen.ClassPath),
			WidgetClass))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s derives from its native presenter"),
				Screen.ClassPath),
			WidgetClass->IsChildOf(
				Screen.ExpectedNativeClass));

		for (const FExpectedActionRow& CommonRow :
			CommonRows)
		{
			TestActionRow(
				*this,
				*WidgetClass,
				CommonRow);
		}

		const FDataTableRowHandle* BackAction =
			ResolveActionRow(
				*this,
				*WidgetClass,
				TEXT("BackInputAction"));
		if (BackAction)
		{
			TestNull(
				*FString::Printf(
					TEXT("%s delegates Back to CommonUI"),
					Screen.ClassPath),
				BackAction->DataTable.Get());
			TestTrue(
				*FString::Printf(
					TEXT("%s has no partial Back row"),
					Screen.ClassPath),
				BackAction->RowName.IsNone());
		}

		for (const TCHAR* ClassPropertyName :
			{
				TEXT("FreeDragVisualWidgetClass"),
				TEXT("ContextMenuWidgetClass"),
				TEXT("SplitDialogWidgetClass"),
				TEXT("DropConfirmationDialogWidgetClass")
			})
		{
			const FClassProperty* ClassProperty =
				FindFProperty<FClassProperty>(
					WidgetClass,
					ClassPropertyName);
			if (!TestNotNull(
				*FString::Printf(
					TEXT("%s exposes %s"),
					Screen.ClassPath,
					ClassPropertyName),
				ClassProperty))
			{
				continue;
			}

			const UClass* PresentationClass =
				Cast<UClass>(
					ClassProperty->
						GetObjectPropertyValue_InContainer(
							WidgetClass->
								GetDefaultObject()));
			if (TestNotNull(
				*FString::Printf(
					TEXT("%s authors %s"),
					Screen.ClassPath,
					ClassPropertyName),
				PresentationClass))
			{
				TestFalse(
					*FString::Printf(
						TEXT("%s.%s is concrete"),
						Screen.ClassPath,
						ClassPropertyName),
					PresentationClass->
						HasAnyClassFlags(
							CLASS_Abstract));
			}
		}
	}

	UClass* BaseTerminalClass =
		LoadClass<UUserWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_BaseTerminalSpatial."
				"CUI_BaseTerminalSpatial_C"));
	if (BaseTerminalClass)
	{
		constexpr TCHAR BaseActionTablePath[] =
			TEXT(
				"/Game/SurvivalRpg/UI/Input/"
				"DT_RpgUIActions_BaseTerminal."
				"DT_RpgUIActions_BaseTerminal");
		TestActionRow(
			*this,
			*BaseTerminalClass,
			{
				TEXT("DepositAllInputAction"),
				BaseActionTablePath,
				TEXT("UI.BaseTerminal.DepositAll")
			});
		TestActionRow(
			*this,
			*BaseTerminalClass,
			{
				TEXT("InstallUpgradeInputAction"),
				BaseActionTablePath,
				TEXT("UI.BaseTerminal.InstallUpgrade")
			});
	}

	UClass* CraftingStationClass =
		LoadClass<UUserWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/Crafting/UI/"
				"CUI_CraftingStationSpatial."
				"CUI_CraftingStationSpatial_C"));
	if (CraftingStationClass)
	{
		constexpr TCHAR CraftingActionTablePath[] =
			TEXT(
				"/Game/SurvivalRpg/UI/Input/"
				"DT_RpgUIActions_Crafting."
				"DT_RpgUIActions_Crafting");
		TestActionRow(
			*this,
			*CraftingStationClass,
			{
				TEXT("CraftInputAction"),
				CraftingActionTablePath,
				TEXT("UI.Crafting.Craft")
			});
		TestActionRow(
			*this,
			*CraftingStationClass,
			{
				TEXT("TogglePauseInputAction"),
				CraftingActionTablePath,
				TEXT("UI.Crafting.TogglePause")
			});
	}

	return true;
}

#endif
