#include "RpgInventoryPointerAutomationTestScreen.h"

#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryPointerAutomationTestScreen)

void URpgInventoryPointerAutomationTestScreen::ConfigureAddressedTarget(
	UWidget* InTargetOwner,
	const FRpgInventoryDropTarget& InDropTarget)
{
	TargetOwner = InTargetOwner;
	DropTarget = InDropTarget;
}

bool URpgInventoryPointerAutomationTestScreen::InvokeNativePointerDragOver(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	return NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

bool URpgInventoryPointerAutomationTestScreen::InvokeNativePointerDrop(
	const FGeometry& InGeometry,
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	return NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void URpgInventoryPointerAutomationTestScreen::InvokeNativePointerDragLeave(
	const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	NativeOnDragLeave(InDragDropEvent, InOperation);
}

bool URpgInventoryPointerAutomationTestScreen::
	RouteInventoryPayloadToScreenSpecificTarget(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed)
{
	(void)GhostCenterScreenPosition;
	bOutTargetAddressed =
		bTargetAddressed &&
		IsValid(TargetOwner) &&
		URpgInventoryDragDropCoordinator::IsTargetValid(DropTarget);
	if (!bOutTargetAddressed)
	{
		return false;
	}

	SwitchActivePointerDropTarget(TargetOwner);
	URpgInventoryDragDropCoordinator* Coordinator =
		GetScreenDragDropCoordinator();
	if (!Coordinator)
	{
		return false;
	}

	if (bCommit)
	{
		++CommitRouteCount;
		return Coordinator->CommitPayloadToTarget(Payload, DropTarget);
	}

	++PreviewRouteCount;
	return Coordinator->UpdateInteractionPreview(Payload, DropTarget);
}

void URpgInventoryPointerAutomationTestScreen::
	ClearInventoryScreenSpecificDragPreviews()
{
	++ExternalPreviewClearCount;
}
