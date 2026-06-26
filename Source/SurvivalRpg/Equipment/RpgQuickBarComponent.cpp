#include "RpgQuickBarComponent.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "RpgEquipmentDefinition.h"
#include "RpgEquipmentInstance.h"
#include "RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgQuickBarComponent)

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
	if (!Slots.IsValidIndex(NewIndex))
	{
		return;
	}

	if (ActiveSlotIndex == NewIndex)
	{
		RefreshActiveLoadoutOnCurrentPawn();
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

void URpgQuickBarComponent::RequestAssignItemToLoadoutSlot_Implementation(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	AssignItemToLoadoutSlot(SlotIndex, EquipmentSlot, Item);
}

void URpgQuickBarComponent::RequestSwapLoadoutSlots_Implementation(int32 SourceSlotIndex, ERpgEquipmentSlot SourceEquipmentSlot, int32 TargetSlotIndex, ERpgEquipmentSlot TargetEquipmentSlot)
{
	SwapLoadoutSlots(SourceSlotIndex, SourceEquipmentSlot, TargetSlotIndex, TargetEquipmentSlot);
}

void URpgQuickBarComponent::RequestClearLoadoutSlot_Implementation(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot)
{
	RemoveItemFromLoadoutSlot(SlotIndex, EquipmentSlot);
}

bool URpgQuickBarComponent::CanAssignItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, const URpgInventoryItemInstance* Item) const
{
	if (!Slots.IsValidIndex(SlotIndex) || !Item || !IsQuickBarEquipmentSlot(EquipmentSlot))
	{
		return false;
	}

	const URpgInventoryManagerComponent* OwnerInventory = FindOwnerInventory();
	if (!OwnerInventory || !OwnerInventory->ContainsItemInstance(const_cast<URpgInventoryItemInstance*>(Item)))
	{
		return false;
	}

	return IsItemAllowedInQuickBarSlot(Item, EquipmentSlot);
}

void URpgQuickBarComponent::AddItemToSlot(int32 SlotIndex, URpgInventoryItemInstance* Item)
{
	AddItemToLoadoutSlot(SlotIndex, ERpgEquipmentSlot::MainHand, Item);
}

void URpgQuickBarComponent::AddItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	EnsureSlotCount();
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].GetItemForSlot(EquipmentSlot) != nullptr)
	{
		return;
	}

	AssignItemToLoadoutSlot(SlotIndex, EquipmentSlot, Item);
}

bool URpgQuickBarComponent::AssignItemToLoadoutSlot(int32 SlotIndex, ERpgEquipmentSlot EquipmentSlot, URpgInventoryItemInstance* Item)
{
	EnsureSlotCount();
	if (!CanAssignItemToLoadoutSlot(SlotIndex, EquipmentSlot, Item))
	{
		return false;
	}

	const bool bWasActiveSlot = ActiveSlotIndex == SlotIndex;
	if (bWasActiveSlot)
	{
		UnequipItemInSlot();
	}

	ClearItemFromAllLoadoutSlots(Item);
	Slots[SlotIndex].SetItemForSlot(EquipmentSlot, Item);

	if (bWasActiveSlot)
	{
		EquipItemInSlot();
	}

	OnRep_Slots();
	return true;
}

bool URpgQuickBarComponent::SwapLoadoutSlots(int32 SourceSlotIndex, ERpgEquipmentSlot SourceEquipmentSlot, int32 TargetSlotIndex, ERpgEquipmentSlot TargetEquipmentSlot)
{
	EnsureSlotCount();
	if (!Slots.IsValidIndex(SourceSlotIndex) || !Slots.IsValidIndex(TargetSlotIndex) ||
		!IsQuickBarEquipmentSlot(SourceEquipmentSlot) || !IsQuickBarEquipmentSlot(TargetEquipmentSlot))
	{
		return false;
	}

	if (SourceSlotIndex == TargetSlotIndex && SourceEquipmentSlot == TargetEquipmentSlot)
	{
		return true;
	}

	URpgInventoryItemInstance* SourceItem = Slots[SourceSlotIndex].GetItemForSlot(SourceEquipmentSlot);
	URpgInventoryItemInstance* TargetItem = Slots[TargetSlotIndex].GetItemForSlot(TargetEquipmentSlot);

	if (SourceItem && !IsItemAllowedInQuickBarSlot(SourceItem, TargetEquipmentSlot))
	{
		return false;
	}

	if (TargetItem && !IsItemAllowedInQuickBarSlot(TargetItem, SourceEquipmentSlot))
	{
		return false;
	}

	const bool bTouchesActiveSlot = ActiveSlotIndex == SourceSlotIndex || ActiveSlotIndex == TargetSlotIndex;
	if (bTouchesActiveSlot)
	{
		UnequipItemInSlot();
	}

	Slots[SourceSlotIndex].SetItemForSlot(SourceEquipmentSlot, TargetItem);
	Slots[TargetSlotIndex].SetItemForSlot(TargetEquipmentSlot, SourceItem);

	if (bTouchesActiveSlot)
	{
		EquipItemInSlot();
	}

	OnRep_Slots();
	return true;
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

void URpgQuickBarComponent::ClearItemFromAllLoadoutSlots(URpgInventoryItemInstance* Item)
{
	if (Item == nullptr)
	{
		return;
	}

	bool bChanged = false;
	const bool bTouchesActiveSlot = Slots.IsValidIndex(ActiveSlotIndex) &&
		(Slots[ActiveSlotIndex].MainHandItem == Item || Slots[ActiveSlotIndex].OffHandItem == Item);
	const bool bHadEquippedReferences = MainHandEquippedItem != nullptr || OffHandEquippedItem != nullptr;

	if (bTouchesActiveSlot)
	{
		UnequipItemInSlot();
	}

	for (FRpgQuickBarLoadoutSlot& Slot : Slots)
	{
		if (Slot.MainHandItem == Item)
		{
			Slot.MainHandItem = nullptr;
			bChanged = true;
		}

		if (Slot.OffHandItem == Item)
		{
			Slot.OffHandItem = nullptr;
			bChanged = true;
		}
	}

	if (bTouchesActiveSlot && bHadEquippedReferences && Slots.IsValidIndex(ActiveSlotIndex) && Slots[ActiveSlotIndex].HasAnyItem())
	{
		EquipItemInSlot();
	}

	if (bChanged)
	{
		OnRep_Slots();
	}
}

void URpgQuickBarComponent::UnequipActiveLoadoutFromCurrentPawn()
{
	if (AActor* OwnerActor = GetOwner(); !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	UnequipItemInSlot();
	if (URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::OffHand);
		EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
	}
	ClearEquippedItemReferences();
}

bool URpgQuickBarComponent::RefreshActiveLoadoutOnCurrentPawn()
{
	if (AActor* OwnerActor = GetOwner(); !OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	EnsureSlotCount();
	if (!Slots.IsValidIndex(ActiveSlotIndex) || !Slots[ActiveSlotIndex].HasAnyItem())
	{
		ClearEquippedItemReferences();
		return true;
	}

	URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (!EquipmentManager || !HasReadyEquipmentTarget())
	{
		ClearEquippedItemReferences();
		return false;
	}

	if (IsActiveLoadoutAppliedToCurrentPawn())
	{
		return true;
	}

	UnequipItemInSlot();
	EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::OffHand);
	EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
	ClearEquippedItemReferences();

	EquipItemInSlot();
	return IsActiveLoadoutAppliedToCurrentPawn();
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
	URpgEquipmentInstance* OldOffHandEquippedItem = OffHandEquippedItem;
	URpgEquipmentInstance* OldMainHandEquippedItem = MainHandEquippedItem;
	ClearEquippedItemReferences();

	if (URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager())
	{
		if (OldOffHandEquippedItem != nullptr)
		{
			EquipmentManager->UnequipItem(OldOffHandEquippedItem);
		}

		if (OldMainHandEquippedItem != nullptr)
		{
			EquipmentManager->UnequipItem(OldMainHandEquippedItem);
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

bool URpgQuickBarComponent::IsActiveLoadoutAppliedToCurrentPawn() const
{
	if (!Slots.IsValidIndex(ActiveSlotIndex))
	{
		return MainHandEquippedItem == nullptr && OffHandEquippedItem == nullptr;
	}

	const FRpgQuickBarLoadoutSlot& LoadoutSlot = Slots[ActiveSlotIndex];
	if (!LoadoutSlot.HasAnyItem())
	{
		return MainHandEquippedItem == nullptr && OffHandEquippedItem == nullptr;
	}

	const URpgEquipmentManagerComponent* EquipmentManager = FindEquipmentManager();
	if (!EquipmentManager)
	{
		return false;
	}

	const bool bMainHandApplied = LoadoutSlot.MainHandItem == nullptr ||
		(MainHandEquippedItem != nullptr && EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand) == MainHandEquippedItem);

	const bool bOffHandBlocked = EquipmentManager->IsEquipmentSlotBlocked(ERpgEquipmentSlot::OffHand);
	const bool bOffHandApplied = LoadoutSlot.OffHandItem == nullptr ||
		bOffHandBlocked ||
		(OffHandEquippedItem != nullptr && EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::OffHand) == OffHandEquippedItem);

	return bMainHandApplied && bOffHandApplied;
}

void URpgQuickBarComponent::ClearEquippedItemReferences()
{
	MainHandEquippedItem = nullptr;
	OffHandEquippedItem = nullptr;
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

URpgInventoryManagerComponent* URpgQuickBarComponent::FindOwnerInventory() const
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

bool URpgQuickBarComponent::HasReadyEquipmentTarget() const
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

bool URpgQuickBarComponent::IsQuickBarEquipmentSlot(ERpgEquipmentSlot EquipmentSlot)
{
	return EquipmentSlot == ERpgEquipmentSlot::MainHand || EquipmentSlot == ERpgEquipmentSlot::OffHand;
}

bool URpgQuickBarComponent::IsItemAllowedInQuickBarSlot(const URpgInventoryItemInstance* Item, ERpgEquipmentSlot EquipmentSlot)
{
	if (!Item || !IsQuickBarEquipmentSlot(EquipmentSlot))
	{
		return false;
	}

	const URpgInventoryFragment_EquippableItem* EquippableFragment = Item->FindFragmentByClass<URpgInventoryFragment_EquippableItem>();
	TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = EquippableFragment ? EquippableFragment->GetEquipmentDefinition() : nullptr;
	const URpgEquipmentDefinition* EquipmentCDO = EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition) : nullptr;
	if (!EquipmentCDO || !EquipmentCDO->CanEquipInSlot(EquipmentSlot))
	{
		return false;
	}

	const URpgInventoryFragment_ItemTraits* Traits = Item->FindFragmentByClass<URpgInventoryFragment_ItemTraits>();
	if (!Traits)
	{
		return true;
	}

	if (Traits->ItemCategory == ERpgInventoryItemCategory::Armor || Traits->IsMaterial())
	{
		return false;
	}

	return Traits->bCanAssignToQuickBar;
}

void URpgQuickBarComponent::BroadcastSlotsChanged() const
{
}

void URpgQuickBarComponent::BroadcastActiveIndexChanged() const
{
}
