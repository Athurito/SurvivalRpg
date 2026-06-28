#include "RpgActionBarComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"

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

void URpgActionBarComponent::RequestAssignItemToSlot_Implementation(int32 SlotIndex, URpgInventoryItemInstance* ItemInstance)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !ItemInstance)
	{
		return;
	}

	const ARpgPlayerController* RpgPC = GetRpgPlayerController();
	const ARpgPlayerState* RpgPS = RpgPC ? RpgPC->GetRpgPlayerState() : nullptr;
	const URpgInventoryManagerComponent* PlayerInventory = RpgPS ? RpgPS->GetInventoryManagerComponent() : nullptr;
	if (!PlayerInventory || !PlayerInventory->ContainsItemInstance(ItemInstance))
	{
		return;
	}

	FRpgActionBarSlot& Slot = Slots[SlotIndex];
	Slot.SlotType = ERpgActionBarSlotType::InventoryItem;
	Slot.ItemInstance = ItemInstance;
	Slot.AbilityIdTag = FGameplayTag();
	OnRep_Slots();
}

void URpgActionBarComponent::RequestAssignAbilityToSlot_Implementation(int32 SlotIndex, FGameplayTag AbilityIdTag)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !AbilityIdTag.IsValid())
	{
		return;
	}

	URpgAbilitySystemComponent* RpgASC = GetRpgPlayerController() ? GetRpgPlayerController()->GetRpgAbilitySystemComponent() : nullptr;
	if (!RpgASC || !RpgASC->HasAbilityWithAbilityId(AbilityIdTag))
	{
		return;
	}

	const FGameplayTag RuntimeInputTag = GetInputTagForSlotIndex(SlotIndex);
	if (!RuntimeInputTag.IsValid())
	{
		return;
	}
	RpgASC->BindInputTagToAbilityId(AbilityIdTag, RuntimeInputTag);

	FRpgActionBarSlot& Slot = Slots[SlotIndex];
	Slot.SlotType = ERpgActionBarSlotType::Ability;
	Slot.ItemInstance = nullptr;
	Slot.AbilityIdTag = AbilityIdTag;
	OnRep_Slots();
}

void URpgActionBarComponent::RequestClearSlot_Implementation(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	if (URpgAbilitySystemComponent* RpgASC = GetRpgPlayerController() ? GetRpgPlayerController()->GetRpgAbilitySystemComponent() : nullptr)
	{
		RpgASC->ClearRuntimeAbilityInputTag(GetInputTagForSlotIndex(SlotIndex));
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

	if (Slot.SlotType == ERpgActionBarSlotType::InventoryItem)
	{
		ARpgPlayerState* RpgPS = RpgPC->GetRpgPlayerState();
		URpgInventoryManagerComponent* PlayerInventory = RpgPS ? RpgPS->GetInventoryManagerComponent() : nullptr;
		if (URpgInventoryUiActionComponent* UiActions = RpgPC->GetInventoryUiActionComponent())
		{
			UiActions->RequestUseInventoryItem(PlayerInventory, Slot.ItemInstance, 1);
		}
		return;
	}

	if (Slot.SlotType == ERpgActionBarSlotType::Ability)
	{
		if (URpgAbilitySystemComponent* RpgASC = RpgPC->GetRpgAbilitySystemComponent())
		{
			RpgASC->AbilityInputTagPressed(Slot.AbilityIdTag);
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

	const FRpgActionBarSlot& Slot = Slots[SlotIndex];
	if (Slot.SlotType != ERpgActionBarSlotType::Ability)
	{
		return;
	}

	if (ARpgPlayerController* RpgPC = GetRpgPlayerController())
	{
		if (URpgAbilitySystemComponent* RpgASC = RpgPC->GetRpgAbilitySystemComponent())
		{
			RpgASC->AbilityInputTagReleased(Slot.AbilityIdTag);
		}
	}
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
