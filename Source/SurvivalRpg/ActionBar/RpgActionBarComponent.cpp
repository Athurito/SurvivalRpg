#include "RpgActionBarComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarComponent)

URpgActionBarComponent::URpgActionBarComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgActionBarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, Slots, COND_OwnerOnly);
}

void URpgActionBarComponent::BeginPlay()
{
	EnsureSlotCount();
	Super::BeginPlay();
}

FRpgActionBarSlot URpgActionBarComponent::GetSlot(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : FRpgActionBarSlot();
}

void URpgActionBarComponent::RequestBindInventorySlotToSlot_Implementation(int32 SlotIndex, FRpgInventorySlotAddress SlotAddress)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !SlotAddress.IsValid())
	{
		return;
	}

	const ARpgPlayerController* RpgPC = GetRpgPlayerController();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPC ? RpgPC->GetPlayerInventoryLayoutComponent() : nullptr;
	if (!InventoryLayout ||
		!InventoryLayout->IsSlotAddressActionbarBindable(SlotAddress) ||
		InventoryLayout->IsCarrySlotAddress(SlotAddress))
	{
		return;
	}

	FRpgActionBarSlot& Slot = Slots[SlotIndex];
	Slot.SlotType = ERpgActionBarSlotType::InventorySlotBinding;
	Slot.SlotAddress = SlotAddress;
	OnRep_Slots();
}

void URpgActionBarComponent::RequestBindCarrySlotToSlot_Implementation(int32 SlotIndex, FRpgInventorySlotAddress SlotAddress)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !SlotAddress.IsValid())
	{
		return;
	}

	const ARpgPlayerController* RpgPC = GetRpgPlayerController();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPC ? RpgPC->GetPlayerInventoryLayoutComponent() : nullptr;
	if (!InventoryLayout ||
		!InventoryLayout->IsSlotAddressActionbarBindable(SlotAddress) ||
		!InventoryLayout->IsCarrySlotAddress(SlotAddress))
	{
		return;
	}

	FRpgActionBarSlot& Slot = Slots[SlotIndex];
	Slot.SlotType = ERpgActionBarSlotType::CarrySlotBinding;
	Slot.SlotAddress = SlotAddress;
	OnRep_Slots();
}

void URpgActionBarComponent::RequestClearSlot_Implementation(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	Slots[SlotIndex] = FRpgActionBarSlot();
	OnRep_Slots();
}

void URpgActionBarComponent::ActivateSlot(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	const FRpgActionBarSlot& Slot = Slots[SlotIndex];
	ARpgPlayerController* RpgPC = GetRpgPlayerController();
	if (!RpgPC)
	{
		return;
	}

	URpgInventoryUiActionComponent* UiActions = RpgPC->GetInventoryUiActionComponent();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = RpgPC->GetPlayerInventoryLayoutComponent();
	if (!UiActions || !InventoryLayout || Slot.IsEmpty())
	{
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::CarrySlotBinding)
	{
		UiActions->RequestActivateCarrySlot(Slot.SlotAddress);
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::InventorySlotBinding)
	{
		ARpgPlayerState* RpgPS = RpgPC->GetRpgPlayerState();
		URpgInventoryManagerComponent* PlayerInventory = RpgPS ? RpgPS->GetInventoryManagerComponent() : nullptr;
		URpgInventoryItemInstance* Item = InventoryLayout->GetItemInSlotAddress(Slot.SlotAddress);
		if (PlayerInventory && Item)
		{
			UiActions->RequestUseInventoryItem(PlayerInventory, Item, 1);
		}
	}
}

void URpgActionBarComponent::ReleaseSlot(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	// 1..8 actionbar V1.5 only activates slot sources. Held abilities stay on the Q/E/R weapon ability loadout.
}

FGameplayTag URpgActionBarComponent::GetInputTagForSlotIndex(int32 SlotIndex)
{
	switch (SlotIndex)
	{
	case 0:
		return RpgGameplayTags::InputTag_ActionBar_Slot_1;
	case 1:
		return RpgGameplayTags::InputTag_ActionBar_Slot_2;
	case 2:
		return RpgGameplayTags::InputTag_ActionBar_Slot_3;
	case 3:
		return RpgGameplayTags::InputTag_ActionBar_Slot_4;
	case 4:
		return RpgGameplayTags::InputTag_ActionBar_Slot_5;
	case 5:
		return RpgGameplayTags::InputTag_ActionBar_Slot_6;
	case 6:
		return RpgGameplayTags::InputTag_ActionBar_Slot_7;
	case 7:
		return RpgGameplayTags::InputTag_ActionBar_Slot_8;
	default:
		return FGameplayTag();
	}
}

void URpgActionBarComponent::OnRep_Slots()
{
	EnsureSlotCount();
	BroadcastSlotsChanged();
}

void URpgActionBarComponent::EnsureSlotCount()
{
	const int32 ClampedSlotCount = FMath::Clamp(SlotCount, 1, 8);
	if (Slots.Num() != ClampedSlotCount)
	{
		Slots.SetNum(ClampedSlotCount);
	}
}

void URpgActionBarComponent::BroadcastSlotsChanged() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FRpgActionBarSlotsChangedMessage Message;
	Message.Owner = GetTypedOuter<APlayerController>();
	Message.ActionBarComponent = const_cast<URpgActionBarComponent*>(this);

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(World);
	MessageSystem.BroadcastMessage(RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged, Message);
}

bool URpgActionBarComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex);
}

ARpgPlayerController* URpgActionBarComponent::GetRpgPlayerController() const
{
	return Cast<ARpgPlayerController>(GetOwner());
}
