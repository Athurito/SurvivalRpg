#include "RpgPhysicalStorageViewModel.h"

#include "Engine/World.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPhysicalStorageViewModel)

void URpgPhysicalStorageViewModel::BindContainer(URpgInventoryContainerComponent* Container)
{
	if (ObservedContainer.Get() == Container)
	{
		Refresh();
		return;
	}
	UnbindContainer();
	ObservedContainer = Container;
	if (Container)
	{
		SettingsChangedHandle = Container->OnPhysicalStorageSettingsChanged.AddUObject(this, &ThisClass::HandleSettingsChanged);
		if (UWorld* World = Container->GetWorld())
		{
			InventoryChangedHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRpgInventoryChangeMessage>(
				FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
				this, &ThisClass::HandleInventoryChanged);
		}
	}
	Refresh();
}

void URpgPhysicalStorageViewModel::UnbindContainer()
{
	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}
	if (ObservedContainer.IsValid())
	{
		ObservedContainer->OnPhysicalStorageSettingsChanged.Remove(SettingsChangedHandle);
	}
	SettingsChangedHandle.Reset();
	ObservedContainer.Reset();
	SetCommandPending(false);
	SetCommandResult(false, FText::GetEmpty());
	Refresh();
}

void URpgPhysicalStorageViewModel::Refresh()
{
	const URpgInventoryContainerComponent* Container = ObservedContainer.Get();
	const bool bPhysical = Container && Container->AllowsCraftingAccess() && Container->IsContainerAccessible() &&
		Container->GetTransferPolicy() == ERpgInventoryContainerTransferPolicy::Bidirectional && !Container->GetPersistentContainerId().IsNone();
	const FRpgPhysicalStorageMetadata Metadata = bPhysical ? Container->ExportPhysicalStorageMetadata() : FRpgPhysicalStorageMetadata();
	UE_MVVM_SET_PROPERTY_VALUE(bIsPhysicalStorage, bPhysical);
	UE_MVVM_SET_PROPERTY_VALUE(ContainerId, bPhysical ? Container->GetPersistentContainerId() : NAME_None);
	UE_MVVM_SET_PROPERTY_VALUE(BaseId, Metadata.BaseId);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentTier, Metadata.UpgradeTier);
	UE_MVVM_SET_PROPERTY_VALUE(SettingsRevision, bPhysical ? Metadata.SettingsRevision : INDEX_NONE);
	UE_MVVM_SET_PROPERTY_VALUE(GridWidth, bPhysical ? Metadata.GridSize.Width : 0);
	UE_MVVM_SET_PROPERTY_VALUE(GridHeight, bPhysical ? Metadata.GridSize.Height : 0);
	UE_MVVM_SET_PROPERTY_VALUE(bStationChest, bPhysical && !Metadata.LinkedStationId.IsNone());
	const FText NewDisplayName = bPhysical ? Container->GetStorageDisplayName() : FText::GetEmpty();
	if (!StorageDisplayName.EqualTo(NewDisplayName))
	{
		StorageDisplayName = NewDisplayName;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StorageDisplayName);
	}
	bool bChanged = Assignments.Num() != Metadata.Assignments.Num();
	for (int32 Index = 0; !bChanged && Index < Assignments.Num(); ++Index)
	{
		bChanged = Assignments[Index].ItemDefinition != Metadata.Assignments[Index].ItemDefinition ||
			Assignments[Index].Category != Metadata.Assignments[Index].Category ||
			Assignments[Index].AssignmentOrder != Metadata.Assignments[Index].AssignmentOrder;
	}
	if (bChanged)
	{
		Assignments = Metadata.Assignments;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Assignments);
	}
}

void URpgPhysicalStorageViewModel::SetCommandPending(bool bPending)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCommandPending, bPending);
}

void URpgPhysicalStorageViewModel::SetCommandResult(bool bSucceeded, const FText& Message)
{
	UE_MVVM_SET_PROPERTY_VALUE(bLastCommandSucceeded, bSucceeded);
	if (!LastCommandMessage.EqualTo(Message))
	{
		LastCommandMessage = Message;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(LastCommandMessage);
	}
}

void URpgPhysicalStorageViewModel::HandleSettingsChanged(URpgInventoryContainerComponent* Container)
{
	if (Container == ObservedContainer.Get())
	{
		Refresh();
	}
}

void URpgPhysicalStorageViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const URpgInventoryContainerComponent* Container = ObservedContainer.Get();
	if (Container && Message.bCapacityChanged && Container->GetInventoryManager() == Message.InventoryOwner)
	{
		// Capacity can replicate after chest settings; project the actual inventory grid once its OnRep arrives.
		Refresh();
	}
}

void URpgPhysicalStorageViewModel::BeginDestroy()
{
	UnbindContainer();
	Super::BeginDestroy();
}
