#include "RpgInventoryActionWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

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

#endif
