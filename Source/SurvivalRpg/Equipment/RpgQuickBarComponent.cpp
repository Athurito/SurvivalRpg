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

URpgInventoryItemInstance* FRpgQuickBarLoadoutSlot::GetItemForSlot(ERpgEquipmentSlot Slot) const
{
	switch (Slot)
	{
	case ERpgEquipmentSlot::MainHand:
		return MainHandItem;
	case ERpgEquipmentSlot::OffHand:
		return OffHandItem;
	default:
		return nullptr;
	}
}

void FRpgQuickBarLoadoutSlot::SetItemForSlot(ERpgEquipmentSlot Slot, URpgInventoryItemInstance* Item)
{
	switch (Slot)
	{
	case ERpgEquipmentSlot::MainHand:
		MainHandItem = Item;
		break;
	case ERpgEquipmentSlot::OffHand:
		OffHandItem = Item;
		break;
	default:
		break;
	}
}

bool FRpgQuickBarLoadoutSlot::HasAnyItem() const
{
	return MainHandItem != nullptr || OffHandItem != nullptr;
}

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
		if (Slots[NewIndex].HasAnyItem())
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
		if (Slots[NewIndex].HasAnyItem())
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
	return Slots.IsValidIndex(ActiveSlotIndex) ? Slots[ActiveSlotIndex].MainHandItem : nullptr;
}

TArray<URpgInventoryItemInstance*> URpgQuickBarComponent::GetSlots() const
{
	TArray<URpgInventoryItemInstance*> Result;
	Result.Reserve(Slots.Num());
	for (const FRpgQuickBarLoadoutSlot& Slot : Slots)
	{
		Result.Add(Slot.MainHandItem);
	}
	return Result;
}

URpgInventoryItemInstance* URpgQuickBarComponent::GetItemInLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].GetItemForSlot(EquipmentSlot) : nullptr;
}

int32 URpgQuickBarComponent::GetNextFreeItemSlot() const
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (!Slots[SlotIndex].HasAnyItem())
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

void URpgQuickBarComponent::AddItemToSlot(int32 SlotIndex, URpgInventoryItemInstance* Item)
{
	AddItemToLoadoutSlot(SlotIndex, ERpgEquipmentSlot::MainHand, Item);
}

void URpgQuickBarComponent::AddItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	EnsureSlotCount();
	if (!Slots.IsValidIndex(SlotIndex) || Item == nullptr || Slots[SlotIndex].GetItemForSlot(EquipmentSlot) != nullptr)
	{
		return;
	}

	const bool bWasActiveSlot = ActiveSlotIndex == SlotIndex;
	if (bWasActiveSlot)
	{
		UnequipItemInSlot();
	}

	Slots[SlotIndex].SetItemForSlot(EquipmentSlot, Item);

	if (bWasActiveSlot)
	{
		EquipItemInSlot();
	}

	OnRep_Slots();
}

URpgInventoryItemInstance* URpgQuickBarComponent::RemoveItemFromSlot(int32 SlotIndex)
{
	return RemoveItemFromLoadoutSlot(SlotIndex, ERpgEquipmentSlot::MainHand);
}

URpgInventoryItemInstance* URpgQuickBarComponent::RemoveItemFromLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot)
{
	EnsureSlotCount();
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}

	URpgInventoryItemInstance* Result = Slots[SlotIndex].GetItemForSlot(EquipmentSlot);
	if (Result == nullptr)
	{
		return nullptr;
	}

	if (ActiveSlotIndex == SlotIndex)
	{
		UnequipItemInSlot();
	}

	Slots[SlotIndex].SetItemForSlot(EquipmentSlot, nullptr);

	if (ActiveSlotIndex == SlotIndex)
	{
		if (Slots[SlotIndex].HasAnyItem())
		{
			EquipItemInSlot();
		}
		else
		{
			ActiveSlotIndex = INDEX_NONE;
			OnRep_ActiveSlotIndex();
		}
	}

	OnRep_Slots();
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
		if (OffHandEquippedItem != nullptr)
		{
			EquipmentManager->UnequipItem(OffHandEquippedItem);
			OffHandEquippedItem = nullptr;
		}

		if (MainHandEquippedItem != nullptr)
		{
			EquipmentManager->UnequipItem(MainHandEquippedItem);
			MainHandEquippedItem = nullptr;
		}
	}
}

void URpgQuickBarComponent::EquipItemInSlot()
{
	if (!Slots.IsValidIndex(ActiveSlotIndex) || MainHandEquippedItem != nullptr || OffHandEquippedItem != nullptr)
	{
		return;
	}

	const FRpgQuickBarLoadoutSlot& LoadoutSlot = Slots[ActiveSlotIndex];
	MainHandEquippedItem = EquipLoadoutItem(LoadoutSlot.MainHandItem, ERpgEquipmentSlot::MainHand);

	URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (!EquipmentManager || !EquipmentManager->IsEquipmentSlotBlocked(ERpgEquipmentSlot::OffHand))
	{
		OffHandEquippedItem = EquipLoadoutItem(LoadoutSlot.OffHandItem, ERpgEquipmentSlot::OffHand);
	}
}

URpgEquipmentInstance* URpgQuickBarComponent::EquipLoadoutItem(URpgInventoryItemInstance* SlotItem, ERpgEquipmentSlot EquipmentSlot) const
{
	const URpgInventoryFragment_EquippableItem* EquippableFragment = SlotItem ? SlotItem->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() : nullptr;
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
	if (EquipmentDefinition == nullptr)
	{
		return nullptr;
	}

	if (URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		URpgEquipmentInstance* EquippedItem = EquipmentManager->EquipItemInSlot(EquipmentDefinition, EquipmentSlot);
		if (EquippedItem != nullptr)
		{
			EquippedItem->SetInstigator(SlotItem);
		}
		return EquippedItem;
	}

	return nullptr;
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
	Message.LoadoutSlots = Slots;
	Message.Slots = GetSlots();

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
