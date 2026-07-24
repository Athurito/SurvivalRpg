#include "RpgInventoryUiActionDomainHandlers.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

AActor* FRpgInventoryUiActionDomainHandler::GetOwner() const
{
	return ActionComponent.GetOwner();
}

UWorld* FRpgInventoryUiActionDomainHandler::GetWorld() const
{
	return ActionComponent.GetWorld();
}

AActor* FRpgInventoryUiActionDomainHandler::GetRequestingActor() const
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	return OwnerController ? OwnerController->GetPawn() : GetOwner();
}

bool FRpgInventoryUiActionDomainHandler::CanAccessInventory(
	URpgInventoryManagerComponent* Inventory) const
{
	return ActionComponent.CanAccessInventory(Inventory);
}

bool FRpgInventoryUiActionDomainHandler::CanAccessBaseStorageStation(
	const URpgBaseStorageStationComponent* Station) const
{
	return ActionComponent.CanAccessBaseStorageStation(Station);
}

URpgInventoryManagerComponent*
FRpgInventoryUiActionDomainHandler::FindPlayerInventory() const
{
	return ActionComponent.FindPlayerInventory();
}

void FRpgInventoryUiActionDomainHandler::RequestQuickTransferItem(
	URpgInventoryManagerComponent* SourceInventory,
	URpgInventoryManagerComponent* TargetInventory,
	FRpgInventoryQuickTransferRequest Request)
{
	ActionComponent.RequestQuickTransferItem_Implementation(
		SourceInventory,
		TargetInventory,
		MoveTemp(Request));
}
