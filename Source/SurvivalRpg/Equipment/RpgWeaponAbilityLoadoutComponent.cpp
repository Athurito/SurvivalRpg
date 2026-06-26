#include "RpgWeaponAbilityLoadoutComponent.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWeaponAbilityLoadoutComponent)

URpgWeaponAbilityLoadoutComponent::URpgWeaponAbilityLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void URpgWeaponAbilityLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, Slots, COND_OwnerOnly);
}

void URpgWeaponAbilityLoadoutComponent::BeginPlay()
{
	EnsureSlotCount();
	Super::BeginPlay();
}

FRpgWeaponAbilityLoadoutSlot URpgWeaponAbilityLoadoutComponent::GetSlot(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : FRpgWeaponAbilityLoadoutSlot();
}

void URpgWeaponAbilityLoadoutComponent::RequestAssignAbilityToSlot_Implementation(int32 SlotIndex, FGameplayTag AbilityIdTag)
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

	Slots[SlotIndex].AbilityIdTag = AbilityIdTag;
	RefreshAbilityBindings();
}

void URpgWeaponAbilityLoadoutComponent::RequestClearSlot_Implementation(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	Slots[SlotIndex] = FRpgWeaponAbilityLoadoutSlot();
	if (URpgAbilitySystemComponent* RpgASC = GetRpgPlayerController() ? GetRpgPlayerController()->GetRpgAbilitySystemComponent() : nullptr)
	{
		RpgASC->ClearRuntimeAbilityInputTag(GetInputTagForSlotIndex(SlotIndex));
	}

	OnRep_Slots();
}

void URpgWeaponAbilityLoadoutComponent::RefreshAbilityBindings()
{
	EnsureSlotCount();

	URpgAbilitySystemComponent* RpgASC = GetRpgPlayerController() ? GetRpgPlayerController()->GetRpgAbilitySystemComponent() : nullptr;
	if (!RpgASC || !RpgASC->HasGrantAuthority())
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		FRpgWeaponAbilityLoadoutSlot& Slot = Slots[SlotIndex];
		const FGameplayTag RuntimeInputTag = GetInputTagForSlotIndex(SlotIndex);
		RpgASC->ClearRuntimeAbilityInputTag(RuntimeInputTag);

		const bool bCanBind = Slot.AbilityIdTag.IsValid() && RpgASC->HasAbilityWithAbilityId(Slot.AbilityIdTag);
		Slot.bAvailable = bCanBind && RpgASC->BindInputTagToAbilityId(Slot.AbilityIdTag, RuntimeInputTag);
	}

	OnRep_Slots();
}

void URpgWeaponAbilityLoadoutComponent::HandleInputPressed(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !Slots[SlotIndex].bAvailable)
	{
		return;
	}

	if (ARpgPlayerController* RpgPC = GetRpgPlayerController())
	{
		if (URpgAbilitySystemComponent* RpgASC = RpgPC->GetRpgAbilitySystemComponent())
		{
			RpgASC->AbilityInputTagPressed(GetInputTagForSlotIndex(SlotIndex));
		}
	}
}

void URpgWeaponAbilityLoadoutComponent::HandleInputReleased(int32 SlotIndex)
{
	EnsureSlotCount();
	if (!IsValidSlotIndex(SlotIndex) || !Slots[SlotIndex].bAvailable)
	{
		return;
	}

	if (ARpgPlayerController* RpgPC = GetRpgPlayerController())
	{
		if (URpgAbilitySystemComponent* RpgASC = RpgPC->GetRpgAbilitySystemComponent())
		{
			RpgASC->AbilityInputTagReleased(GetInputTagForSlotIndex(SlotIndex));
		}
	}
}

FGameplayTag URpgWeaponAbilityLoadoutComponent::GetInputTagForSlotIndex(int32 SlotIndex)
{
	switch (SlotIndex)
	{
	case 0:
		return RpgGameplayTags::InputTag_Weapon_Ability_1;
	case 1:
		return RpgGameplayTags::InputTag_Weapon_Ability_2;
	case 2:
		return RpgGameplayTags::InputTag_Weapon_Ability_3;
	default:
		return FGameplayTag();
	}
}

void URpgWeaponAbilityLoadoutComponent::OnRep_Slots()
{
	EnsureSlotCount();
	BroadcastSlotsChanged();
}

void URpgWeaponAbilityLoadoutComponent::EnsureSlotCount()
{
	const int32 ClampedSlotCount = FMath::Clamp(SlotCount, 1, 3);
	if (Slots.Num() != ClampedSlotCount)
	{
		Slots.SetNum(ClampedSlotCount);
	}
}

void URpgWeaponAbilityLoadoutComponent::BroadcastSlotsChanged() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FRpgWeaponAbilityLoadoutChangedMessage Message;
	Message.Owner = GetTypedOuter<APlayerController>();
	Message.LoadoutComponent = const_cast<URpgWeaponAbilityLoadoutComponent*>(this);

	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(World);
	MessageSystem.BroadcastMessage(RpgGameplayTags::Rpg_WeaponAbilityLoadout_Message_SlotsChanged, Message);
}

bool URpgWeaponAbilityLoadoutComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex);
}

ARpgPlayerController* URpgWeaponAbilityLoadoutComponent::GetRpgPlayerController() const
{
	return Cast<ARpgPlayerController>(GetOwner());
}
