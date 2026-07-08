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
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SlotContainerProvider.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

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
	DOREPLIFETIME(ThisClass, RememberedOffhands);
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

	if (IsSlotContainerEquipmentSlot(EquipmentSlot))
	{
		return Item->FindFragmentByClass<URpgInventoryFragment_SlotContainerProvider>() != nullptr;
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

	if (Slots[SlotIndex].Item == Item)
	{
		return true;
	}

	if (!CanClearEquipmentSlot(EquipmentSlot))
	{
		return false;
	}

	if (!ClearItemFromAllEquipmentSlots(Item))
	{
		return false;
	}

	if (EquipmentSlot == ERpgEquipmentSlot::OffHand && IsTwoHandItem(GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand)))
	{
		return false;
	}

	if (EquipmentSlot == ERpgEquipmentSlot::MainHand)
	{
		RememberCurrentOffhandForActiveMainhand();
		if (IsTwoHandItem(Item))
		{
			AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, nullptr);
		}
	}

	if (EquipmentSlot != ERpgEquipmentSlot::MainHand &&
		EquipmentSlot != ERpgEquipmentSlot::OffHand &&
		!MoveInventoryItemToEquipmentSlotAddress(EquipmentSlot, Item))
	{
		return false;
	}

	UnequipRuntimeSlot(EquipmentSlot);

	Slots[SlotIndex].Item = Item;
	EquippedItemsBySlot.Remove(EquipmentSlot);

	if (IsRuntimeEquipmentSlot(EquipmentSlot) && HasReadyEquipmentTarget())
	{
		if (URpgEquipmentInstance* EquippedItem = EquipLoadoutItem(Item, EquipmentSlot))
		{
			EquippedItemsBySlot.Add(EquipmentSlot, EquippedItem);
		}
	}

	OnRep_Slots();
	if (URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout())
	{
		InventoryLayout->ApplyLayoutCapacityToInventory();
	}
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

	if (!CanClearEquipmentSlot(EquipmentSlot))
	{
		return nullptr;
	}

	UnequipRuntimeSlot(EquipmentSlot);
	Slots[SlotIndex].Item = nullptr;
	ClearRememberedOffhandEntriesForItem(OldItem);
	OnRep_Slots();
	if (URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout())
	{
		InventoryLayout->ApplyLayoutCapacityToInventory();
	}
	RefreshWeaponAbilityLoadout();
	return OldItem;
}

bool URpgEquipmentLoadoutComponent::ClearItemFromAllEquipmentSlots(URpgInventoryItemInstance* Item)
{
	if (!Item)
	{
		return true;
	}

	for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (Slot.Item == Item && !CanClearEquipmentSlot(Slot.EquipmentSlot))
		{
			return false;
		}
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
		ClearRememberedOffhandEntriesForItem(Item);
		OnRep_Slots();
		if (URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout())
		{
			InventoryLayout->ApplyLayoutCapacityToInventory();
		}
		RefreshWeaponAbilityLoadout();
	}

	return true;
}

bool URpgEquipmentLoadoutComponent::CanRemoveItemFromLoadout(URpgInventoryItemInstance* Item) const
{
	if (!Item)
	{
		return true;
	}

	for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (Slot.Item == Item && !CanClearEquipmentSlot(Slot.EquipmentSlot))
		{
			return false;
		}
	}

	return true;
}

bool URpgEquipmentLoadoutComponent::ActivateMainHandItem(URpgInventoryItemInstance* Item)
{
	EnsureDefaultSlots();
	if (!CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::MainHand, Item))
	{
		return false;
	}

	if (GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand) == Item)
	{
		return ClearActiveHands();
	}

	RememberCurrentOffhandForActiveMainhand();
	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, nullptr);
	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, nullptr);

	if (!AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, Item))
	{
		return false;
	}

	if (!IsTwoHandItem(Item))
	{
		if (URpgInventoryItemInstance* RememberedOffhand = GetRememberedOffhandForMainHand(Item))
		{
			AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, RememberedOffhand);
		}
	}

	OnRep_Slots();
	RefreshWeaponAbilityLoadout();
	return true;
}

bool URpgEquipmentLoadoutComponent::ActivateOffHandItem(URpgInventoryItemInstance* Item)
{
	EnsureDefaultSlots();
	if (!CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::OffHand, Item))
	{
		return false;
	}

	if (GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand) == Item)
	{
		return ClearActiveOffHand(true);
	}

	URpgInventoryItemInstance* ActiveMainHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
	if (IsTwoHandItem(ActiveMainHand))
	{
		return false;
	}

	if (!AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, Item))
	{
		return false;
	}

	if (ActiveMainHand)
	{
		SetRememberedOffhandForMainHand(ActiveMainHand, Item);
	}

	OnRep_Slots();
	RefreshWeaponAbilityLoadout();
	return true;
}

bool URpgEquipmentLoadoutComponent::ClearActiveHands()
{
	RememberCurrentOffhandForActiveMainhand();

	const bool bHadMainHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand) != nullptr;
	const bool bHadOffHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand) != nullptr;
	if (!bHadMainHand && !bHadOffHand)
	{
		return false;
	}

	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, nullptr);
	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, nullptr);
	OnRep_Slots();
	RefreshWeaponAbilityLoadout();
	return true;
}

bool URpgEquipmentLoadoutComponent::ClearActiveOffHand(bool bForgetForActiveMainHand)
{
	URpgInventoryItemInstance* ActiveOffHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
	if (!ActiveOffHand)
	{
		return false;
	}

	if (bForgetForActiveMainHand)
	{
		ClearRememberedOffhandForMainHand(GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand));
	}

	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, nullptr);
	OnRep_Slots();
	RefreshWeaponAbilityLoadout();
	return true;
}

URpgInventoryItemInstance* URpgEquipmentLoadoutComponent::GetRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem) const
{
	if (!MainHandItem)
	{
		return nullptr;
	}

	const URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	for (const FRpgRememberedOffhandForMainHand& Entry : RememberedOffhands)
	{
		if (Entry.MainHandItem == MainHandItem &&
			Entry.OffHandItem &&
			OwnerInventory &&
			OwnerInventory->ContainsItemInstance(Entry.OffHandItem) &&
			CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::OffHand, Entry.OffHandItem))
		{
			return Entry.OffHandItem;
		}
	}

	return nullptr;
}

void URpgEquipmentLoadoutComponent::UnequipLoadoutFromCurrentPawn()
{
	if (AActor* OwnerActor = GetOwner(); !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (IsRuntimeEquipmentSlot(Slot.EquipmentSlot))
		{
			UnequipRuntimeSlot(Slot.EquipmentSlot);
		}
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

		if (!IsRuntimeEquipmentSlot(Slot.EquipmentSlot))
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
		ERpgEquipmentSlot::Feet,
		ERpgEquipmentSlot::Backpack,
		ERpgEquipmentSlot::Belt,
		ERpgEquipmentSlot::Pouch,
		ERpgEquipmentSlot::ResourceBag
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
	if (!IsRuntimeEquipmentSlot(EquipmentSlot))
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

URpgPlayerInventoryLayoutComponent* URpgEquipmentLoadoutComponent::FindPlayerInventoryLayout() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgPlayerInventoryLayoutComponent>() : nullptr;
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

bool URpgEquipmentLoadoutComponent::CanClearEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	if (!IsSlotContainerEquipmentSlot(EquipmentSlot))
	{
		return true;
	}

	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	return !InventoryLayout || InventoryLayout->CanUnequipSlotContainer(EquipmentSlot);
}

bool URpgEquipmentLoadoutComponent::IsTwoHandItem(const URpgInventoryItemInstance* Item) const
{
	const URpgInventoryFragment_EquippableItem* EquippableFragment = Item ? Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>() : nullptr;
	const TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	return EquipmentCDO && EquipmentCDO->HandOccupancy == ERpgEquipmentHandOccupancy::BothHands;
}

bool URpgEquipmentLoadoutComponent::MoveInventoryItemToEquipmentSlotAddress(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item) const
{
	URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!OwnerInventory || !InventoryLayout || !Item || !OwnerInventory->ContainsItemInstance(Item))
	{
		return false;
	}

	FRpgInventorySlotAddress TargetAddress;
	FRpgInventoryGridPlacement TargetPlacement;
	if (!URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(EquipmentSlot, TargetAddress) ||
		!InventoryLayout->ResolveSlotAddress(TargetAddress, TargetPlacement) ||
		!InventoryLayout->CanItemUseSlotAddress(Item, TargetAddress))
	{
		return false;
	}

	FRpgInventoryGridPlacement CurrentPlacement;
	if (OwnerInventory->GetItemPlacement(Item, CurrentPlacement) &&
		CurrentPlacement.ContainerId == TargetPlacement.ContainerId &&
		CurrentPlacement.X == TargetPlacement.X &&
		CurrentPlacement.Y == TargetPlacement.Y)
	{
		return true;
	}

	FRpgInventorySlotAddress SourceAddress;
	if (!CurrentPlacement.IsValid() ||
		!InventoryLayout->TryMakeSlotAddressFromPlacement(CurrentPlacement, SourceAddress))
	{
		return false;
	}

	if (URpgInventoryItemInstance* TargetItem = OwnerInventory->GetItemAtCell(TargetPlacement.ContainerId, TargetPlacement.X, TargetPlacement.Y))
	{
		if (!InventoryLayout->CanItemUseSlotAddress(TargetItem, SourceAddress))
		{
			return false;
		}
	}

	const FGuid EntryId = FindInventoryEntryIdForItem(OwnerInventory, Item);
	return EntryId.IsValid() && OwnerInventory->MoveInventoryEntryToPlacement(EntryId, TargetPlacement);
}

FGuid URpgEquipmentLoadoutComponent::FindInventoryEntryIdForItem(const URpgInventoryManagerComponent* Inventory, const URpgInventoryItemInstance* Item) const
{
	if (!Inventory || !Item)
	{
		return FGuid();
	}

	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		if (Entry.Instance == Item)
		{
			return Entry.EntryId;
		}
	}

	return FGuid();
}

void URpgEquipmentLoadoutComponent::RememberCurrentOffhandForActiveMainhand()
{
	URpgInventoryItemInstance* ActiveMainHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
	URpgInventoryItemInstance* ActiveOffHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
	if (ActiveMainHand && ActiveOffHand && !IsTwoHandItem(ActiveMainHand))
	{
		SetRememberedOffhandForMainHand(ActiveMainHand, ActiveOffHand);
	}
}

void URpgEquipmentLoadoutComponent::SetRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem, URpgInventoryItemInstance* OffHandItem)
{
	if (!MainHandItem)
	{
		return;
	}

	if (!OffHandItem)
	{
		ClearRememberedOffhandForMainHand(MainHandItem);
		return;
	}

	for (FRpgRememberedOffhandForMainHand& Entry : RememberedOffhands)
	{
		if (Entry.MainHandItem == MainHandItem)
		{
			Entry.OffHandItem = OffHandItem;
			return;
		}
	}

	FRpgRememberedOffhandForMainHand& NewEntry = RememberedOffhands.AddDefaulted_GetRef();
	NewEntry.MainHandItem = MainHandItem;
	NewEntry.OffHandItem = OffHandItem;
}

void URpgEquipmentLoadoutComponent::ClearRememberedOffhandForMainHand(URpgInventoryItemInstance* MainHandItem)
{
	if (!MainHandItem)
	{
		return;
	}

	RememberedOffhands.RemoveAll([MainHandItem](const FRpgRememberedOffhandForMainHand& Entry)
	{
		return Entry.MainHandItem == MainHandItem;
	});
}

void URpgEquipmentLoadoutComponent::ClearRememberedOffhandEntriesForItem(URpgInventoryItemInstance* Item)
{
	if (!Item)
	{
		return;
	}

	RememberedOffhands.RemoveAll([Item](const FRpgRememberedOffhandForMainHand& Entry)
	{
		return Entry.MainHandItem == Item || Entry.OffHandItem == Item;
	});
}

bool URpgEquipmentLoadoutComponent::AssignRuntimeEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	if (!IsRuntimeEquipmentSlot(EquipmentSlot))
	{
		return false;
	}

	EnsureDefaultSlots();
	const int32 SlotIndex = FindSlotIndex(EquipmentSlot);
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	if (Item && !CanAssignItemToEquipmentSlot(EquipmentSlot, Item))
	{
		return false;
	}

	if (Slots[SlotIndex].Item == Item)
	{
		return true;
	}

	UnequipRuntimeSlot(EquipmentSlot);
	Slots[SlotIndex].Item = Item;
	EquippedItemsBySlot.Remove(EquipmentSlot);

	if (Item && HasReadyEquipmentTarget())
	{
		if (URpgEquipmentInstance* EquippedItem = EquipLoadoutItem(Item, EquipmentSlot))
		{
			EquippedItemsBySlot.Add(EquipmentSlot, EquippedItem);
		}
	}

	return true;
}

bool URpgEquipmentLoadoutComponent::IsManagedEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	return IsRuntimeEquipmentSlot(EquipmentSlot) || IsSlotContainerEquipmentSlot(EquipmentSlot);
}

bool URpgEquipmentLoadoutComponent::IsRuntimeEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	return EquipmentSlot == ERpgEquipmentSlot::MainHand ||
		EquipmentSlot == ERpgEquipmentSlot::OffHand ||
		EquipmentSlot == ERpgEquipmentSlot::Head ||
		EquipmentSlot == ERpgEquipmentSlot::Chest ||
		EquipmentSlot == ERpgEquipmentSlot::Hands ||
		EquipmentSlot == ERpgEquipmentSlot::Legs ||
		EquipmentSlot == ERpgEquipmentSlot::Feet;
}

bool URpgEquipmentLoadoutComponent::IsSlotContainerEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	return EquipmentSlot == ERpgEquipmentSlot::Backpack ||
		EquipmentSlot == ERpgEquipmentSlot::Belt ||
		EquipmentSlot == ERpgEquipmentSlot::Pouch ||
		EquipmentSlot == ERpgEquipmentSlot::ResourceBag;
}
