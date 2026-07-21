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
#include "SurvivalRpg/Inventory/RpgInventoryEquipmentPlacementPolicy.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

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
	ReconcileEquipmentLoadFromInventory();
}

void URpgEquipmentLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, Slots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, RememberedOffhands, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, CurrentEquipmentLoadWeight, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, CurrentEquipmentLoadTier, COND_OwnerOnly);
}

URpgInventoryItemInstance* URpgEquipmentLoadoutComponent::GetItemInEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	const int32 SlotIndex = FindSlotIndex(EquipmentSlot);
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Item : nullptr;
}

bool URpgEquipmentLoadoutComponent::CanUseEquipmentSlotForOwnedItem(
	ERpgEquipmentSlot EquipmentSlot,
	const URpgInventoryItemInstance* Item) const
{
	if (!FRpgInventoryEquipmentPlacementPolicy::IsManagedEquipmentSlot(EquipmentSlot) || !Item)
	{
		return false;
	}

	const URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	if (!OwnerInventory || !OwnerInventory->ContainsItemInstance(const_cast<URpgInventoryItemInstance*>(Item)))
	{
		return false;
	}

	return FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(Item, EquipmentSlot);
}

bool URpgEquipmentLoadoutComponent::CanActivateItemInEquipmentSlot(
	ERpgEquipmentSlot EquipmentSlot,
	const URpgInventoryItemInstance* Item) const
{
	if (!FRpgInventoryEquipmentPlacementPolicy::IsHandEquipmentSlot(
			EquipmentSlot) ||
		!CanUseEquipmentSlotForOwnedItem(EquipmentSlot, Item))
	{
		return false;
	}

	return EquipmentSlot != ERpgEquipmentSlot::OffHand ||
		!IsTwoHandItem(GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand));
}

bool URpgEquipmentLoadoutComponent::ActivateMainHandItem(URpgInventoryItemInstance* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	EnsureDefaultSlots();
	if (!CanActivateItemInEquipmentSlot(ERpgEquipmentSlot::MainHand, Item) ||
		!IsItemInCarryActivationRole(Item, RpgGameplayTags::Equipment_Slot_MainHand))
	{
		return false;
	}

	if (GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand) == Item)
	{
		return ClearActiveHands();
	}

	return SetMainHandItemActive(Item);
}

bool URpgEquipmentLoadoutComponent::SetMainHandItemActive(
	URpgInventoryItemInstance* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	EnsureDefaultSlots();
	if (!CanActivateItemInEquipmentSlot(ERpgEquipmentSlot::MainHand, Item) ||
		!IsItemInCarryActivationRole(
			Item,
			RpgGameplayTags::Equipment_Slot_MainHand))
	{
		return false;
	}
	if (GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand) == Item)
	{
		return true;
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
	if (!CanActivateItemInEquipmentSlot(ERpgEquipmentSlot::OffHand, Item) ||
		!IsItemInCarryActivationRole(Item, RpgGameplayTags::Equipment_Slot_OffHand))
	{
		return false;
	}

	if (GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand) == Item)
	{
		return ClearActiveOffHand(true);
	}

	return SetOffHandItemActive(Item);
}

bool URpgEquipmentLoadoutComponent::SetOffHandItemActive(
	URpgInventoryItemInstance* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	EnsureDefaultSlots();
	if (!CanActivateItemInEquipmentSlot(ERpgEquipmentSlot::OffHand, Item) ||
		!IsItemInCarryActivationRole(
			Item,
			RpgGameplayTags::Equipment_Slot_OffHand))
	{
		return false;
	}
	if (GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand) == Item)
	{
		return true;
	}

	URpgInventoryItemInstance* ActiveMainHand = GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
	const bool bMovesActiveMainHand = ActiveMainHand == Item;
	if (bMovesActiveMainHand)
	{
		// One concrete item may never own both active-hand slots. Delay notification until the target assignment
		// succeeds so observers see only the final role switch.
		AssignRuntimeEquipmentSlot(
			ERpgEquipmentSlot::MainHand,
			nullptr);
	}

	if (!AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::OffHand, Item))
	{
		if (bMovesActiveMainHand)
		{
			AssignRuntimeEquipmentSlot(
				ERpgEquipmentSlot::MainHand,
				Item);
		}
		return false;
	}

	if (bMovesActiveMainHand)
	{
		ClearRememberedOffhandForMainHand(Item);
	}
	else if (ActiveMainHand)
	{
		SetRememberedOffhandForMainHand(ActiveMainHand, Item);
	}

	OnRep_Slots();
	RefreshWeaponAbilityLoadout();
	return true;
}

bool URpgEquipmentLoadoutComponent::ClearActiveMainHand()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	URpgInventoryItemInstance* ActiveMainHand =
		GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand);
	if (!ActiveMainHand)
	{
		return false;
	}

	RememberCurrentOffhandForActiveMainhand();
	AssignRuntimeEquipmentSlot(ERpgEquipmentSlot::MainHand, nullptr);
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
			CanUseEquipmentSlotForOwnedItem(ERpgEquipmentSlot::OffHand, Entry.OffHandItem))
		{
			return Entry.OffHandItem;
		}
	}

	return nullptr;
}

void URpgEquipmentLoadoutComponent::DetachRuntimeEquipmentFromCurrentPawn()
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

bool URpgEquipmentLoadoutComponent::ReconcileRuntimeEquipmentOnCurrentPawn()
{
	if (AActor* OwnerActor = GetOwner(); !OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	EnsureDefaultSlots();
	if (!HasReadyEquipmentTarget())
	{
		DetachRuntimeEquipmentFromCurrentPawn();
		return false;
	}
	URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (!EquipmentManager)
	{
		return false;
	}

	bool bClearedInvalidHandSelection = false;
	bool bAllRuntimeEquipmentApplied = true;
	for (FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (!IsRuntimeEquipmentSlot(Slot.EquipmentSlot))
		{
			continue;
		}

		if (Slot.Item &&
			((Slot.EquipmentSlot == ERpgEquipmentSlot::MainHand &&
				!IsItemInCarryActivationRole(Slot.Item, RpgGameplayTags::Equipment_Slot_MainHand)) ||
			(Slot.EquipmentSlot == ERpgEquipmentSlot::OffHand &&
				!IsItemInCarryActivationRole(Slot.Item, RpgGameplayTags::Equipment_Slot_OffHand))))
		{
			Slot.Item = nullptr;
			bClearedInvalidHandSelection = true;
		}
	}

	const int32 MainHandIndex = FindSlotIndex(ERpgEquipmentSlot::MainHand);
	const int32 OffHandIndex = FindSlotIndex(ERpgEquipmentSlot::OffHand);
	if (Slots.IsValidIndex(MainHandIndex) &&
		Slots.IsValidIndex(OffHandIndex) &&
		Slots[OffHandIndex].Item &&
		(Slots[OffHandIndex].Item == Slots[MainHandIndex].Item ||
			IsTwoHandItem(Slots[MainHandIndex].Item)))
	{
		Slots[OffHandIndex].Item = nullptr;
		bClearedInvalidHandSelection = true;
	}

	// Tear down every mismatching runtime entry before creating replacements. Equipment-manager conflict
	// resolution may remove another slot, so a one-pass remove/add loop can otherwise double-unequip a
	// stale tracked instance or briefly grant the same source item twice during a role switch.
	TArray<URpgEquipmentInstance*> RuntimeInstancesToRemove;
	for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (!IsRuntimeEquipmentSlot(Slot.EquipmentSlot))
		{
			continue;
		}

		URpgEquipmentInstance* ManagerItem =
			EquipmentManager->GetEquipmentInstanceInSlot(
				Slot.EquipmentSlot);
		if (ManagerItem && ManagerItem->GetInstigator() != Slot.Item)
		{
			RuntimeInstancesToRemove.AddUnique(ManagerItem);
		}
	}
	for (URpgEquipmentInstance* RuntimeInstance : RuntimeInstancesToRemove)
	{
		EquipmentManager->UnequipItem(RuntimeInstance);
	}

	EquippedItemsBySlot.Reset();
	for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (!IsRuntimeEquipmentSlot(Slot.EquipmentSlot))
		{
			continue;
		}

		URpgEquipmentInstance* ManagerItem =
			EquipmentManager->GetEquipmentInstanceInSlot(
				Slot.EquipmentSlot);
		if (ManagerItem && ManagerItem->GetInstigator() == Slot.Item)
		{
			EquippedItemsBySlot.Add(Slot.EquipmentSlot, ManagerItem);
			continue;
		}
		if (!Slot.Item)
		{
			continue;
		}

		if (URpgEquipmentInstance* EquippedItem =
			EquipLoadoutItem(Slot.Item, Slot.EquipmentSlot))
		{
			EquippedItemsBySlot.Add(Slot.EquipmentSlot, EquippedItem);
		}
		else
		{
			bAllRuntimeEquipmentApplied = false;
		}
	}

	// Rebuild tracking from manager truth after conflict resolution and prove that every desired slot survived.
	EquippedItemsBySlot.Reset();
	for (const FRpgEquipmentLoadoutSlot& Slot : Slots)
	{
		if (!IsRuntimeEquipmentSlot(Slot.EquipmentSlot))
		{
			continue;
		}

		URpgEquipmentInstance* ManagerItem =
			EquipmentManager->GetEquipmentInstanceInSlot(
				Slot.EquipmentSlot);
		if (ManagerItem && ManagerItem->GetInstigator() == Slot.Item)
		{
			EquippedItemsBySlot.Add(Slot.EquipmentSlot, ManagerItem);
		}
		else if (Slot.Item || ManagerItem)
		{
			bAllRuntimeEquipmentApplied = false;
		}
	}

	RefreshWeaponAbilityLoadout();
	ReconcileEquipmentLoadFromInventory();
	if (bClearedInvalidHandSelection)
	{
		BroadcastSlotsChanged();
	}
	return bAllRuntimeEquipmentApplied;
}

void URpgEquipmentLoadoutComponent::OnRep_Slots()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ReconcileEquipmentLoadFromInventory();
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

void URpgEquipmentLoadoutComponent::ReconcileEquipmentLoadFromInventory()
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
		if (PhysicalItem && !CanUseEquipmentSlotForOwnedItem(EquipmentSlot, PhysicalItem))
		{
			PhysicalItem = nullptr;
			bAllReconciled = false;
		}

		// Runtime equipment is rebuilt once below, after every slot points at the reconstructed inventory instances.
		Slots[SlotIndex].Item = PhysicalItem;
	}

	ApplyEquipmentSelectionPointers(PreviousSelection);
	InventoryLayout->ApplyLayoutCapacityToInventory();
	const bool bHadReadyEquipmentTarget = HasReadyEquipmentTarget();
	if (!ReconcileRuntimeEquipmentOnCurrentPawn() &&
		bHadReadyEquipmentTarget)
	{
		bAllReconciled = false;
	}
	BroadcastSlotsChanged();
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

	ApplyEquipmentSelectionPointers(SaveData);
	if (!ReconcileRuntimeEquipmentOnCurrentPawn())
	{
		ReconcileEquipmentLoadFromInventory();
	}
	BroadcastSlotsChanged();
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
	return FRpgInventoryEquipmentPlacementPolicy::FindEquipmentDefinition(Item);
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

	EquippedItemsBySlot.Remove(EquipmentSlot);

	if (URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		if (URpgEquipmentInstance* ManagerItem =
			EquipmentManager->GetEquipmentInstanceInSlot(
				EquipmentSlot))
		{
			EquipmentManager->UnequipItem(ManagerItem);
		}
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

bool URpgEquipmentLoadoutComponent::IsTwoHandItem(const URpgInventoryItemInstance* Item) const
{
	const URpgEquipmentDefinition* EquipmentCDO = FindEquipmentDefinition(Item);
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

void URpgEquipmentLoadoutComponent::ApplyEquipmentSelectionPointers(
	const FRpgEquipmentSelectionSaveData& SaveData)
{
	EnsureDefaultSlots();
	const int32 MainHandIndex =
		FindSlotIndex(ERpgEquipmentSlot::MainHand);
	const int32 OffHandIndex =
		FindSlotIndex(ERpgEquipmentSlot::OffHand);
	if (!Slots.IsValidIndex(MainHandIndex) ||
		!Slots.IsValidIndex(OffHandIndex))
	{
		return;
	}

	Slots[MainHandIndex].Item = nullptr;
	Slots[OffHandIndex].Item = nullptr;
	RememberedOffhands.Reset();

	URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	if (!OwnerInventory)
	{
		return;
	}

	URpgInventoryItemInstance* MainHandItem =
		OwnerInventory->FindItemById(
			SaveData.ActiveMainHandItemId);
	if (!IsItemInCarryActivationRole(
			MainHandItem,
			RpgGameplayTags::Equipment_Slot_MainHand) ||
		!CanUseEquipmentSlotForOwnedItem(
			ERpgEquipmentSlot::MainHand,
			MainHandItem))
	{
		MainHandItem = nullptr;
	}

	URpgInventoryItemInstance* OffHandItem =
		OwnerInventory->FindItemById(
			SaveData.ActiveOffHandItemId);
	if (!IsItemInCarryActivationRole(
			OffHandItem,
			RpgGameplayTags::Equipment_Slot_OffHand) ||
		!CanUseEquipmentSlotForOwnedItem(
			ERpgEquipmentSlot::OffHand,
			OffHandItem) ||
		OffHandItem == MainHandItem ||
		IsTwoHandItem(MainHandItem))
	{
		OffHandItem = nullptr;
	}

	Slots[MainHandIndex].Item = MainHandItem;
	Slots[OffHandIndex].Item = OffHandItem;

	for (const FRpgRememberedOffhandItemIds& SavedPairing :
		SaveData.RememberedOffhands)
	{
		URpgInventoryItemInstance* SavedMainHand =
			OwnerInventory->FindItemById(
				SavedPairing.MainHandItemId);
		URpgInventoryItemInstance* SavedOffHand =
			OwnerInventory->FindItemById(
				SavedPairing.OffHandItemId);
		if (SavedMainHand && SavedOffHand &&
			SavedMainHand != SavedOffHand &&
			!IsTwoHandItem(SavedMainHand) &&
			IsItemInCarryActivationRole(
				SavedMainHand,
				RpgGameplayTags::Equipment_Slot_MainHand) &&
			IsItemInCarryActivationRole(
				SavedOffHand,
				RpgGameplayTags::Equipment_Slot_OffHand) &&
			CanUseEquipmentSlotForOwnedItem(
				ERpgEquipmentSlot::MainHand,
				SavedMainHand) &&
			CanUseEquipmentSlotForOwnedItem(
				ERpgEquipmentSlot::OffHand,
				SavedOffHand))
		{
			SetRememberedOffhandForMainHand(
				SavedMainHand,
				SavedOffHand);
		}
	}

	if (MainHandItem && OffHandItem)
	{
		SetRememberedOffhandForMainHand(
			MainHandItem,
			OffHandItem);
	}
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

	if (Item && !CanUseEquipmentSlotForOwnedItem(EquipmentSlot, Item))
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
