#include "RpgEquipmentLoadoutComponent.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "RpgEquipmentInstance.h"
#include "RpgEquipmentManagerComponent.h"
#include "RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SlotContainerProvider.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgEquipmentLoadoutComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Equipment_Load_Light, "Equipment.Load.Light");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Equipment_Load_Medium, "Equipment.Load.Medium");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Equipment_Load_Heavy, "Equipment.Load.Heavy");

URpgEquipmentLoadoutComponent::URpgEquipmentLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgEquipmentLoadoutComponent::BeginPlay()
{
	EnsureDefaultSlots();
	Super::BeginPlay();
	RefreshEquipmentLoadState();
}

void URpgEquipmentLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Slots);
	DOREPLIFETIME(ThisClass, RememberedOffhands);
	DOREPLIFETIME_CONDITION(ThisClass, CurrentEquipmentLoadWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, CurrentEquipmentLoadTier, COND_OwnerOnly);
}

URpgInventoryItemInstance* URpgEquipmentLoadoutComponent::GetItemInEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	const int32 SlotIndex = FindSlotIndex(EquipmentSlot);
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Item : nullptr;
}

void URpgEquipmentLoadoutComponent::RequestAssignItemToEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	if (URpgInventoryUiActionComponent* UiActions = GetOwner()
		? GetOwner()->FindComponentByClass<URpgInventoryUiActionComponent>()
		: nullptr)
	{
		UiActions->RequestAssignItemToEquipmentSlot(EquipmentSlot, Item);
	}
}

void URpgEquipmentLoadoutComponent::RequestClearEquipmentSlot_Implementation(ERpgEquipmentSlot EquipmentSlot)
{
	if (URpgInventoryUiActionComponent* UiActions = GetOwner()
		? GetOwner()->FindComponentByClass<URpgInventoryUiActionComponent>()
		: nullptr)
	{
		UiActions->RequestClearEquipmentSlot(EquipmentSlot);
	}
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
		return Item->FindFragmentByClass<URpgInventoryFragment_ItemContainer>() != nullptr;
	}

	const URpgInventoryFragment_EquippableItem* EquippableFragment = Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>();
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	return EquipmentCDO && EquipmentCDO->CanEquipInSlot(EquipmentSlot);
}

bool URpgEquipmentLoadoutComponent::AssignItemToEquipmentSlot(ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

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

	if (EquipmentSlot == ERpgEquipmentSlot::OffHand && IsTwoHandItem(GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand)))
	{
		return false;
	}

	if (!ClearItemFromAllEquipmentSlots(Item))
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

	if (IsRuntimeEquipmentSlot(EquipmentSlot))
	{
		if (!AssignRuntimeEquipmentSlot(EquipmentSlot, Item))
		{
			return false;
		}
	}
	else
	{
		Slots[SlotIndex].Item = Item;
		EquippedItemsBySlot.Remove(EquipmentSlot);
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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return nullptr;
	}

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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	EnsureDefaultSlots();
	if (!CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::MainHand, Item) ||
		!IsItemInCarryActivationRole(Item, RpgGameplayTags::Equipment_Slot_MainHand))
	{
		return false;
	}

	if (GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand) == Item)
	{
		return ClearActiveHands();
	}

	URpgInventoryItemInstance* PreviousMainHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
	URpgInventoryItemInstance* PreviousOffHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
	RememberCurrentOffhandForActiveMainhand();
	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, nullptr);
	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, nullptr);

	if (!AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, Item))
	{
		AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, PreviousMainHand);
		if (!IsTwoHandItem(PreviousMainHand))
		{
			AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, PreviousOffHand);
		}
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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	EnsureDefaultSlots();
	if (!CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::OffHand, Item) ||
		!IsItemInCarryActivationRole(Item, RpgGameplayTags::Equipment_Slot_OffHand))
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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

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
			IsItemInCarryActivationRole(Entry.OffHandItem, RpgGameplayTags::Equipment_Slot_OffHand) &&
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

	bool bClearedInvalidHandSelection = false;
	for (FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (Slot.Item == nullptr)
		{
			continue;
		}

		if (!IsRuntimeEquipmentSlot(Slot.EquipmentSlot))
		{
			continue;
		}

		if ((Slot.EquipmentSlot == ERpgEquipmentSlot::MainHand &&
				!IsItemInCarryActivationRole(Slot.Item, RpgGameplayTags::Equipment_Slot_MainHand)) ||
			(Slot.EquipmentSlot == ERpgEquipmentSlot::OffHand &&
				!IsItemInCarryActivationRole(Slot.Item, RpgGameplayTags::Equipment_Slot_OffHand)))
		{
			Slot.Item = nullptr;
			bClearedInvalidHandSelection = true;
			continue;
		}

		if (URpgEquipmentInstance* EquippedItem = EquipLoadoutItem(Slot.Item, Slot.EquipmentSlot))
		{
			EquippedItemsBySlot.Add(Slot.EquipmentSlot, EquippedItem);
		}
	}

	if (bClearedInvalidHandSelection)
	{
		OnRep_Slots();
	}
	RefreshWeaponAbilityLoadout();
	RefreshEquipmentLoadState();
	return true;
}

void URpgEquipmentLoadoutComponent::OnRep_Slots()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RefreshEquipmentLoadState();
	}
	BroadcastSlotsChanged();
}

void URpgEquipmentLoadoutComponent::OnRep_EquipmentLoadState()
{
	BroadcastSlotsChanged();
}

FGameplayTag URpgEquipmentLoadoutComponent::GetEquipmentLoadTierTag() const
{
	return GetTagForEquipmentLoadTier(CurrentEquipmentLoadTier);
}

FRpgEquipmentDodgeProfile URpgEquipmentLoadoutComponent::GetDodgeProfileForCurrentLoad() const
{
	switch (CurrentEquipmentLoadTier)
	{
	case ERpgEquipmentLoadTier::Medium:
		return MediumDodgeProfile;
	case ERpgEquipmentLoadTier::Heavy:
		return HeavyDodgeProfile;
	case ERpgEquipmentLoadTier::Light:
	default:
		return LightDodgeProfile;
	}
}

void URpgEquipmentLoadoutComponent::RefreshEquipmentLoadState()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const float NewLoadWeight = CalculateEquipmentLoadWeight();
	const ERpgEquipmentLoadTier NewLoadTier = ResolveEquipmentLoadTier(NewLoadWeight);
	const bool bChanged = !FMath::IsNearlyEqual(CurrentEquipmentLoadWeight, NewLoadWeight) || CurrentEquipmentLoadTier != NewLoadTier;

	CurrentEquipmentLoadWeight = NewLoadWeight;
	CurrentEquipmentLoadTier = NewLoadTier;
	ApplyEquipmentLoadTierTag();

	if (bChanged)
	{
		BroadcastSlotsChanged();
	}
}

bool URpgEquipmentLoadoutComponent::ReconcilePhysicalEquipmentFromInventory()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!OwnerInventory || !InventoryLayout)
	{
		return false;
	}

	const FRpgEquipmentSelectionSaveData PreviousSelection = ExportEquipmentSelection();
	EnsureDefaultSlots();
	const ERpgEquipmentSlot PhysicalSlots[] =
	{
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

	bool bAllReconciled = true;
	for (const ERpgEquipmentSlot EquipmentSlot : PhysicalSlots)
	{
		FRpgInventorySlotAddress Address;
		URpgInventoryItemInstance* PhysicalItem = nullptr;
		if (URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(EquipmentSlot, Address))
		{
			PhysicalItem = InventoryLayout->GetItemInSlotAddress(Address);
		}

		const int32 SlotIndex = FindSlotIndex(EquipmentSlot);
		if (!Slots.IsValidIndex(SlotIndex))
		{
			bAllReconciled = false;
			continue;
		}
		if (PhysicalItem && !CanAssignItemToEquipmentSlot(EquipmentSlot, PhysicalItem))
		{
			PhysicalItem = nullptr;
			bAllReconciled = false;
		}

		// Runtime equipment is rebuilt once below, after every slot points at the reconstructed inventory instances.
		Slots[SlotIndex].Item = PhysicalItem;
	}

	RestoreEquipmentSelection(PreviousSelection);
	InventoryLayout->ApplyLayoutCapacityToInventory();
	RefreshEquipmentLoadoutOnCurrentPawn();
	RefreshEquipmentLoadState();
	return bAllReconciled;
}

FRpgEquipmentSelectionSaveData URpgEquipmentLoadoutComponent::ExportEquipmentSelection() const
{
	FRpgEquipmentSelectionSaveData Result;
	if (const URpgInventoryItemInstance* MainHandItem = GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
		IsItemInCarryActivationRole(MainHandItem, RpgGameplayTags::Equipment_Slot_MainHand))
	{
		Result.ActiveMainHandItemId = MainHandItem->GetItemId();
	}
	if (const URpgInventoryItemInstance* OffHandItem = GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand);
		IsItemInCarryActivationRole(OffHandItem, RpgGameplayTags::Equipment_Slot_OffHand))
	{
		Result.ActiveOffHandItemId = OffHandItem->GetItemId();
	}

	Result.RememberedOffhands.Reserve(RememberedOffhands.Num());
	for (const FRpgRememberedOffhandForMainHand& Pairing : RememberedOffhands)
	{
		if (!Pairing.MainHandItem || !Pairing.OffHandItem ||
			!IsItemInCarryActivationRole(Pairing.MainHandItem, RpgGameplayTags::Equipment_Slot_MainHand) ||
			!IsItemInCarryActivationRole(Pairing.OffHandItem, RpgGameplayTags::Equipment_Slot_OffHand))
		{
			continue;
		}

		const FRpgInventoryItemId MainHandItemId = Pairing.MainHandItem->GetItemId();
		const FRpgInventoryItemId OffHandItemId = Pairing.OffHandItem->GetItemId();
		if (!MainHandItemId.IsValid() || !OffHandItemId.IsValid())
		{
			continue;
		}

		FRpgRememberedOffhandItemIds& SavedPairing = Result.RememberedOffhands.AddDefaulted_GetRef();
		SavedPairing.MainHandItemId = MainHandItemId;
		SavedPairing.OffHandItemId = OffHandItemId;
	}
	return Result;
}

void URpgEquipmentLoadoutComponent::RestoreEquipmentSelection(const FRpgEquipmentSelectionSaveData& SaveData)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	if (!OwnerInventory)
	{
		return;
	}

	EnsureDefaultSlots();
	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, nullptr);
	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, nullptr);
	RememberedOffhands.Reset();

	URpgInventoryItemInstance* MainHandItem = OwnerInventory->FindItemById(SaveData.ActiveMainHandItemId);
	if (!IsItemInCarryActivationRole(MainHandItem, RpgGameplayTags::Equipment_Slot_MainHand) ||
		!CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::MainHand, MainHandItem))
	{
		MainHandItem = nullptr;
	}

	URpgInventoryItemInstance* OffHandItem = OwnerInventory->FindItemById(SaveData.ActiveOffHandItemId);
	if (!IsItemInCarryActivationRole(OffHandItem, RpgGameplayTags::Equipment_Slot_OffHand) ||
		!CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::OffHand, OffHandItem))
	{
		OffHandItem = nullptr;
	}

	if (MainHandItem)
	{
		if (!AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, MainHandItem))
		{
			MainHandItem = nullptr;
		}
	}
	if (OffHandItem && !IsTwoHandItem(MainHandItem))
	{
		if (!AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, OffHandItem))
		{
			OffHandItem = nullptr;
		}
	}

	for (const FRpgRememberedOffhandItemIds& SavedPairing : SaveData.RememberedOffhands)
	{
		URpgInventoryItemInstance* SavedMainHand = OwnerInventory->FindItemById(SavedPairing.MainHandItemId);
		URpgInventoryItemInstance* SavedOffHand = OwnerInventory->FindItemById(SavedPairing.OffHandItemId);
		if (SavedMainHand && SavedOffHand &&
			!IsTwoHandItem(SavedMainHand) &&
			IsItemInCarryActivationRole(SavedMainHand, RpgGameplayTags::Equipment_Slot_MainHand) &&
			IsItemInCarryActivationRole(SavedOffHand, RpgGameplayTags::Equipment_Slot_OffHand) &&
			CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::MainHand, SavedMainHand) &&
			CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::OffHand, SavedOffHand))
		{
			SetRememberedOffhandForMainHand(SavedMainHand, SavedOffHand);
		}
	}

	if (MainHandItem && OffHandItem && !IsTwoHandItem(MainHandItem))
	{
		SetRememberedOffhandForMainHand(MainHandItem, OffHandItem);
	}

	OnRep_Slots();
	RefreshWeaponAbilityLoadout();
}

float URpgEquipmentLoadoutComponent::CalculateEquipmentLoadWeight() const
{
	TSet<const URpgInventoryItemInstance*> LoadBearingItems;
	bool bResolvedSpatialLayout = false;
	if (const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout())
	{
		bResolvedSpatialLayout = true;
		for (const FRpgInventorySlotGroupView& Group : InventoryLayout->GetSlotGroups())
		{
			if (Group.GroupKind != ERpgInventorySlotGroupKind::Gear && Group.GroupKind != ERpgInventorySlotGroupKind::Carry)
			{
				continue;
			}

			for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
			{
				for (int32 X = 0; X < Group.GridSize.Width; ++X)
				{
					if (const URpgInventoryItemInstance* Item = InventoryLayout->GetItemInSlotAddress(Group.MakeAddress(X, Y)))
					{
						LoadBearingItems.Add(Item);
					}
				}
			}
		}
	}

	// Startup fallback before the spatial layout is ready. Once available, physical Gear/Carry locations are canonical.
	if (!bResolvedSpatialLayout)
	{
		for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
		{
			if (Slot.Item)
			{
				LoadBearingItems.Add(Slot.Item);
			}
		}
	}

	float Result = 0.0f;
	for (const URpgInventoryItemInstance* Item : LoadBearingItems)
	{
		if (const URpgEquipmentDefinition* EquipmentDefinition = FindEquipmentDefinition(Item))
		{
			Result += FMath::Max(0.0f, EquipmentDefinition->EquipLoadWeight);
		}
	}

	return Result;
}

ERpgEquipmentLoadTier URpgEquipmentLoadoutComponent::ResolveEquipmentLoadTier(float LoadWeight) const
{
	return ResolveLoadTierForWeight(LoadWeight, MediumLoadThreshold, HeavyLoadThreshold);
}

ERpgEquipmentLoadTier URpgEquipmentLoadoutComponent::ResolveLoadTierForWeight(
	float LoadWeight,
	float MediumThreshold,
	float HeavyThreshold)
{
	const float SafeMediumThreshold = FMath::Max(0.0f, MediumThreshold);
	const float SafeHeavyThreshold = FMath::Max(SafeMediumThreshold, HeavyThreshold);
	if (LoadWeight < SafeMediumThreshold)
	{
		return ERpgEquipmentLoadTier::Light;
	}

	return LoadWeight < SafeHeavyThreshold ? ERpgEquipmentLoadTier::Medium : ERpgEquipmentLoadTier::Heavy;
}

void URpgEquipmentLoadoutComponent::ApplyEquipmentLoadTierTag() const
{
	const AController* OwnerController = Cast<AController>(GetOwner());
	const APawn* Pawn = OwnerController ? OwnerController->GetPawn() : nullptr;
	URpgAbilitySystemComponent* AbilitySystemComponent = Pawn
		? Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
		: nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag ActiveTierTag = GetTagForEquipmentLoadTier(CurrentEquipmentLoadTier);
	const FGameplayTag TierTags[] =
	{
		TAG_Equipment_Load_Light,
		TAG_Equipment_Load_Medium,
		TAG_Equipment_Load_Heavy
	};

	for (const FGameplayTag& TierTag : TierTags)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(
			TierTag,
			TierTag == ActiveTierTag ? 1 : 0,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
}

const URpgEquipmentDefinition* URpgEquipmentLoadoutComponent::FindEquipmentDefinition(const URpgInventoryItemInstance* Item)
{
	const URpgInventoryFragment_EquippableItem* EquippableFragment = Item
		? Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>()
		: nullptr;
	const TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment
		? EquippableFragment->GetEquipmentDefinition()
		: nullptr;
	return EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
}

FGameplayTag URpgEquipmentLoadoutComponent::GetTagForEquipmentLoadTier(ERpgEquipmentLoadTier Tier)
{
	switch (Tier)
	{
	case ERpgEquipmentLoadTier::Medium:
		return TAG_Equipment_Load_Medium;
	case ERpgEquipmentLoadTier::Heavy:
		return TAG_Equipment_Load_Heavy;
	case ERpgEquipmentLoadTier::Light:
	default:
		return TAG_Equipment_Load_Light;
	}
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
		return EquipmentManager->EquipItemInSlotWithInstigator(EquipmentDefinition, EquipmentSlot, SlotItem);
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
	Message.EquipmentLoadWeight = CurrentEquipmentLoadWeight;
	Message.EquipmentLoadTier = CurrentEquipmentLoadTier;

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

		if (URpgActionBarComponent* ActionBar = OwnerController->FindComponentByClass<URpgActionBarComponent>())
		{
			ActionBar->RefreshBindings();
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

bool URpgEquipmentLoadoutComponent::IsItemInCarryActivationRole(
	const URpgInventoryItemInstance* Item,
	FGameplayTag ActivationRole) const
{
	const URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = FindPlayerInventoryLayout();
	if (!OwnerInventory || !InventoryLayout || !Item || !ActivationRole.IsValid())
	{
		return false;
	}

	FRpgInventoryGridPlacement Placement;
	FRpgInventorySlotAddress Address;
	return OwnerInventory->GetItemPlacement(const_cast<URpgInventoryItemInstance*>(Item), Placement) &&
		InventoryLayout->TryMakeSlotAddressFromPlacement(Placement, Address) &&
		InventoryLayout->GetCarryActivationRole(Address) == ActivationRole;
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
		CurrentPlacement.GetContainerHandle() == TargetPlacement.GetContainerHandle() &&
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

	if (URpgInventoryItemInstance* TargetItem = OwnerInventory->GetItemAtContainerCell(
		TargetPlacement.GetContainerHandle(), TargetPlacement.X, TargetPlacement.Y))
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

	URpgInventoryItemInstance* PreviousItem = Slots[SlotIndex].Item;
	UnequipRuntimeSlot(EquipmentSlot);
	Slots[SlotIndex].Item = Item;
	EquippedItemsBySlot.Remove(EquipmentSlot);

	if (Item && HasReadyEquipmentTarget())
	{
		if (URpgEquipmentInstance* EquippedItem = EquipLoadoutItem(Item, EquipmentSlot))
		{
			EquippedItemsBySlot.Add(EquipmentSlot, EquippedItem);
			return true;
		}

		// Runtime creation/grants failed: restore the previous active selection instead of reporting a false success.
		Slots[SlotIndex].Item = PreviousItem;
		if (PreviousItem)
		{
			if (URpgEquipmentInstance* RestoredItem = EquipLoadoutItem(PreviousItem, EquipmentSlot))
			{
				EquippedItemsBySlot.Add(EquipmentSlot, RestoredItem);
			}
		}
		return false;
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
