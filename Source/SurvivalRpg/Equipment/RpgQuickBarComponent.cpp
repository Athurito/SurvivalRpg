#include "RpgQuickBarComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "RpgEquipmentDefinition.h"
#include "RpgEquipmentInstance.h"
#include "RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgQuickBarComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_QuickBar_Message_SlotsChanged, "Rpg.QuickBar.Message.SlotsChanged");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rpg_QuickBar_Message_ActiveIndexChanged, "Rpg.QuickBar.Message.ActiveIndexChanged");

URpgQuickBarComponent::URpgQuickBarComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgQuickBarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Slots);
	DOREPLIFETIME(ThisClass, ActiveSlotIndex);
}

void URpgQuickBarComponent::BeginPlay()
{
	EnsureSlotCount();
	Super::BeginPlay();
}

void URpgQuickBarComponent::CycleActiveSlotForward()
{
	EnsureSlotCount();
	if (Slots.Num() < 2)
	{
		return;
	}

	const int32 OldIndex = ActiveSlotIndex < 0 ? Slots.Num() - 1 : ActiveSlotIndex;
	int32 NewIndex = ActiveSlotIndex;
	do
	{
		NewIndex = (NewIndex + 1) % Slots.Num();
		if (Slots[NewIndex] != nullptr)
		{
			SetActiveSlotIndex(NewIndex);
			return;
		}
	} while (NewIndex != OldIndex);
}

void URpgQuickBarComponent::CycleActiveSlotBackward()
{
	EnsureSlotCount();
	if (Slots.Num() < 2)
	{
		return;
	}

	const int32 OldIndex = ActiveSlotIndex < 0 ? Slots.Num() - 1 : ActiveSlotIndex;
	int32 NewIndex = ActiveSlotIndex;
	do
	{
		NewIndex = (NewIndex - 1 + Slots.Num()) % Slots.Num();
		if (Slots[NewIndex] != nullptr)
		{
			SetActiveSlotIndex(NewIndex);
			return;
		}
	} while (NewIndex != OldIndex);
}

void URpgQuickBarComponent::SetActiveSlotIndex_Implementation(int32 NewIndex)
{
	EnsureSlotCount();
	if (!Slots.IsValidIndex(NewIndex) || ActiveSlotIndex == NewIndex)
	{
		return;
	}

	UnequipItemInSlot();
	ActiveSlotIndex = NewIndex;
	EquipItemInSlot();
	OnRep_ActiveSlotIndex();
}

URpgInventoryItemInstance* URpgQuickBarComponent::GetActiveSlotItem() const
{
	return Slots.IsValidIndex(ActiveSlotIndex) ? Slots[ActiveSlotIndex] : nullptr;
}

TArray<URpgInventoryItemInstance*> URpgQuickBarComponent::GetSlots() const
{
	TArray<URpgInventoryItemInstance*> Result;
	Result.Reserve(Slots.Num());
	for (URpgInventoryItemInstance* SlotItem : Slots)
	{
		Result.Add(SlotItem);
	}
	return Result;
}

int32 URpgQuickBarComponent::GetNextFreeItemSlot() const
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (Slots[SlotIndex] == nullptr)
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

void URpgQuickBarComponent::AddItemToSlot(int32 SlotIndex, URpgInventoryItemInstance* Item)
{
	EnsureSlotCount();
	if (!Slots.IsValidIndex(SlotIndex) || Item == nullptr || Slots[SlotIndex] != nullptr)
	{
		return;
	}

	Slots[SlotIndex] = Item;
	OnRep_Slots();
}

URpgInventoryItemInstance* URpgQuickBarComponent::RemoveItemFromSlot(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}

	if (ActiveSlotIndex == SlotIndex)
	{
		UnequipItemInSlot();
		ActiveSlotIndex = INDEX_NONE;
		OnRep_ActiveSlotIndex();
	}

	URpgInventoryItemInstance* Result = Slots[SlotIndex];
	if (Result != nullptr)
	{
		Slots[SlotIndex] = nullptr;
		OnRep_Slots();
	}

	return Result;
}

void URpgQuickBarComponent::OnRep_Slots()
{
	BroadcastSlotsChanged();
}

void URpgQuickBarComponent::OnRep_ActiveSlotIndex()
{
	BroadcastActiveIndexChanged();
}

void URpgQuickBarComponent::EnsureSlotCount()
{
	if (Slots.Num() < NumSlots)
	{
		Slots.AddDefaulted(NumSlots - Slots.Num());
	}
	else if (Slots.Num() > NumSlots)
	{
		Slots.SetNum(NumSlots);
		if (!Slots.IsValidIndex(ActiveSlotIndex))
		{
			ActiveSlotIndex = INDEX_NONE;
		}
	}
}

void URpgQuickBarComponent::UnequipItemInSlot()
{
	if (URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		if (EquippedItem != nullptr)
		{
			EquipmentManager->UnequipItem(EquippedItem);
			EquippedItem = nullptr;
		}
	}
}

void URpgQuickBarComponent::EquipItemInSlot()
{
	if (!Slots.IsValidIndex(ActiveSlotIndex) || EquippedItem != nullptr)
	{
		return;
	}

	URpgInventoryItemInstance* SlotItem = Slots[ActiveSlotIndex];
	const URpgInventoryFragment_EquippableItem* EquippableFragment = SlotItem ? SlotItem->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() : nullptr;
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
	if (EquipmentDefinition == nullptr)
	{
		return;
	}

	if (URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		EquippedItem = EquipmentManager->EquipItem(EquipmentDefinition);
		if (EquippedItem != nullptr)
		{
			EquippedItem->SetInstigator(SlotItem);
		}
	}
}

URpgEquipmentManagerComponent* URpgQuickBarComponent::FindEquipmentManager() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const APawn* Pawn = OwnerController->GetPawn())
		{
			return Pawn->FindComponentByClass<URpgEquipmentManagerComponent>();
		}
	}
	return nullptr;
}

void URpgQuickBarComponent::BroadcastSlotsChanged() const
{
	FRpgQuickBarSlotsChangedMessage Message;
	Message.Owner = GetOwner();
	Message.Slots = Slots;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(TAG_Rpg_QuickBar_Message_SlotsChanged, Message);
}

void URpgQuickBarComponent::BroadcastActiveIndexChanged() const
{
	FRpgQuickBarActiveIndexChangedMessage Message;
	Message.Owner = GetOwner();
	Message.ActiveIndex = ActiveSlotIndex;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(TAG_Rpg_QuickBar_Message_ActiveIndexChanged, Message);
}
