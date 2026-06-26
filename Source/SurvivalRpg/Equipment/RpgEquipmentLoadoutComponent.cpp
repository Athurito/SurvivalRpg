#include "RpgEquipmentLoadoutComponent.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "RpgEquipmentInstance.h"
#include "RpgEquipmentManagerComponent.h"
#include "RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentLoadoutComponent)

URpgEquipmentLoadoutComponent::URpgEquipmentLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgEquipmentLoadoutComponent::BeginPlay()
{
	EnsureDefaultSlots();
	Super::BeginPlay();
}

void URpgEquipmentLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Slots);
}

URpgInventoryItemInstance* URpgEquipmentLoadoutComponent::GetItemInEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	const int32 SlotIndex = FindSlotIndex(EquipmentSlot);
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Item : nullptr;
}

void URpgEquipmentLoadoutComponent::RequestAssignItemToEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	AssignItemToEquipmentSlot(EquipmentSlot, Item);
}

void URpgEquipmentLoadoutComponent::RequestClearEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot)
{
	ClearEquipmentSlot(EquipmentSlot);
}

bool URpgEquipmentLoadoutComponent::CanAssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, const URpgInventoryItemInstance* Item) const
{
	if (!IsManagedEquipmentSlot(EquipmentSlot) || !Item)
	{
		return false;
	}

	const URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	if (!OwnerInventory || !OwnerInventory->ContainsItemInstance(const_cast<URpgInventoryItemInstance*>(Item)))
	{
		return false;
	}

	const URpgInventoryFragment_EquippableItem* EquippableFragment = Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>();
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	return EquipmentCDO && EquipmentCDO->CanEquipInSlot(EquipmentSlot);
}

bool URpgEquipmentLoadoutComponent::AssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	EnsureDefaultSlots();
	const int32 SlotIndex = FindSlotIndex(EquipmentSlot);
	if (!Slots.IsValidIndex(SlotIndex) || !CanAssignItemToEquipmentSlot(EquipmentSlot, Item))
	{
		return false;
	}

	ClearItemFromAllEquipmentSlots(Item);
	UnequipRuntimeSlot(EquipmentSlot);

	Slots[SlotIndex].Item = Item;
	EquippedItemsBySlot.Remove(EquipmentSlot);

	if (HasReadyEquipmentTarget())
	{
		if (URpgEquipmentInstance* EquippedItem = EquipLoadoutItem(Item, EquipmentSlot))
		{
			EquippedItemsBySlot.Add(EquipmentSlot, EquippedItem);
		}
	}

	OnRep_Slots();
	RefreshWeaponAbilityLoadout();
	return true;
}

URpgInventoryItemInstance* URpgEquipmentLoadoutComponent::ClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	EnsureDefaultSlots();
	const int32 SlotIndex = FindSlotIndex(EquipmentSlot);
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}

	URpgInventoryItemInstance* OldItem = Slots[SlotIndex].Item;
	if (OldItem == nullptr)
	{
		return nullptr;
	}

	UnequipRuntimeSlot(EquipmentSlot);
	Slots[SlotIndex].Item = nullptr;
	OnRep_Slots();
	RefreshWeaponAbilityLoadout();
	return OldItem;
}

void URpgEquipmentLoadoutComponent::ClearItemFromAllEquipmentSlots(URpgInventoryItemInstance* Item)
{
	if (!Item)
	{
		return;
	}

	bool bChanged = false;
	for (FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (Slot.Item == Item)
		{
			UnequipRuntimeSlot(Slot.EquipmentSlot);
			Slot.Item = nullptr;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnRep_Slots();
		RefreshWeaponAbilityLoadout();
	}
}

void URpgEquipmentLoadoutComponent::UnequipLoadoutFromCurrentPawn()
{
	if (AActor* OwnerActor = GetOwner(); !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		UnequipRuntimeSlot(Slot.EquipmentSlot);
	}

	EquippedItemsBySlot.Reset();
}

bool URpgEquipmentLoadoutComponent::RefreshEquipmentLoadoutOnCurrentPawn()
{
	if (AActor* OwnerActor = GetOwner(); !OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	EnsureDefaultSlots();
	UnequipLoadoutFromCurrentPawn();

	if (!HasReadyEquipmentTarget())
	{
		return false;
	}

	for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (Slot.Item == nullptr)
		{
			continue;
		}

		if (URpgEquipmentInstance* EquippedItem = EquipLoadoutItem(Slot.Item, Slot.EquipmentSlot))
		{
			EquippedItemsBySlot.Add(Slot.EquipmentSlot, EquippedItem);
		}
	}

	RefreshWeaponAbilityLoadout();
	return true;
}

void URpgEquipmentLoadoutComponent::OnRep_Slots()
{
	BroadcastSlotsChanged();
}

void URpgEquipmentLoadoutComponent::EnsureDefaultSlots()
{
	const ERpgEquipmentSlot DefaultSlots[] =
	{
		ERpgEquipmentSlot::MainHand,
		ERpgEquipmentSlot::OffHand,
		ERpgEquipmentSlot::Head,
		ERpgEquipmentSlot::Chest,
		ERpgEquipmentSlot::Hands,
		ERpgEquipmentSlot::Legs,
		ERpgEquipmentSlot::Feet
	};

	for (ERpgEquipmentSlot DefaultSlot : DefaultSlots)
	{
		if (FindSlotIndex(DefaultSlot) == INDEX_NONE)
		{
			FRpgEquipmentLoadoutSlot& NewSlot = Slots.AddDefaulted_GetRef();
			NewSlot.EquipmentSlot = DefaultSlot;
		}
	}
}

int32 URpgEquipmentLoadoutComponent::FindSlotIndex(ERpgEquipmentSlot EquipmentSlot) const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index].EquipmentSlot == EquipmentSlot)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

URpgEquipmentInstance* URpgEquipmentLoadoutComponent::EquipLoadoutItem(URpgInventoryItemInstance* SlotItem, ERpgEquipmentSlot EquipmentSlot) const
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

void URpgEquipmentLoadoutComponent::UnequipRuntimeSlot(ERpgEquipmentSlot EquipmentSlot)
{
	if (!IsManagedEquipmentSlot(EquipmentSlot))
	{
		return;
	}

	URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	TWeakObjectPtr<URpgEquipmentInstance> ExistingItem = EquippedItemsBySlot.FindRef(EquipmentSlot);
	EquippedItemsBySlot.Remove(EquipmentSlot);

	if (EquipmentManager)
	{
		if (ExistingItem.IsValid())
		{
			EquipmentManager->UnequipItem(ExistingItem.Get());
			return;
		}

		EquipmentManager->UnequipItemInSlot(EquipmentSlot);
	}
}

URpgEquipmentManagerComponent* URpgEquipmentLoadoutComponent::FindEquipmentManager() const
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

URpgInventoryManagerComponent* URpgEquipmentLoadoutComponent::FindOwnerInventory() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const ARpgPlayerState* RpgPlayerState = OwnerController->GetPlayerState<ARpgPlayerState>())
		{
			return RpgPlayerState->GetInventoryManagerComponent();
		}
	}

	return nullptr;
}

bool URpgEquipmentLoadoutComponent::HasReadyEquipmentTarget() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (const APawn* Pawn = OwnerController->GetPawn())
		{
			return Pawn->FindComponentByClass<URpgEquipmentManagerComponent>() != nullptr &&
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) != nullptr;
		}
	}

	return false;
}

void URpgEquipmentLoadoutComponent::BroadcastSlotsChanged() const
{
	FRpgEquipmentLoadoutSlotsChangedMessage Message;
	Message.Owner = GetOwner();
	Message.Slots = Slots;

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(this);
	MessageSystem.BroadcastMessage(RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged, Message);
}

void URpgEquipmentLoadoutComponent::RefreshWeaponAbilityLoadout() const
{
	if (const AController* OwnerController = Cast<AController>(GetOwner()))
	{
		if (URpgWeaponAbilityLoadoutComponent* WeaponAbilityLoadout = OwnerController->FindComponentByClass<URpgWeaponAbilityLoadoutComponent>())
		{
			WeaponAbilityLoadout->RefreshAbilityBindings();
		}
	}
}

bool URpgEquipmentLoadoutComponent::IsManagedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	return EquipmentSlot == ERpgEquipmentSlot::MainHand ||
		EquipmentSlot == ERpgEquipmentSlot::OffHand ||
		EquipmentSlot == ERpgEquipmentSlot::Head ||
		EquipmentSlot == ERpgEquipmentSlot::Chest ||
		EquipmentSlot == ERpgEquipmentSlot::Hands ||
		EquipmentSlot == ERpgEquipmentSlot::Legs ||
		EquipmentSlot == ERpgEquipmentSlot::Feet;
}
