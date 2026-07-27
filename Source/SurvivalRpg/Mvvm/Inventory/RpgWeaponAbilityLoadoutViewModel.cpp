#include "RpgWeaponAbilityLoadoutViewModel.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWeaponAbilityLoadoutViewModel)

namespace
{
	template <typename ViewModelType>
	bool AreWeaponAbilitySlotViewModelArraysEqual(
		const TArray<TObjectPtr<ViewModelType>>& A,
		const TArray<TObjectPtr<ViewModelType>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Get() != B[Index].Get())
			{
				return false;
			}
		}

		return true;
	}
}

void URpgWeaponAbilityLoadoutViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(InPlayerController);
	BindWeaponAbilityLoadoutWithAbilitySystem(
		RpgPlayerController ? RpgPlayerController->GetWeaponAbilityLoadoutComponent() : nullptr,
		RpgPlayerController ? RpgPlayerController->GetRpgAbilitySystemComponent() : nullptr);
}

void URpgWeaponAbilityLoadoutViewModel::BindWeaponAbilityLoadout(URpgWeaponAbilityLoadoutComponent* InLoadout)
{
	BindWeaponAbilityLoadoutWithAbilitySystem(InLoadout, nullptr);
}

void URpgWeaponAbilityLoadoutViewModel::BindWeaponAbilityLoadoutWithAbilitySystem(
	URpgWeaponAbilityLoadoutComponent* InLoadout,
	URpgAbilitySystemComponent* InAbilitySystem)
{
	if (ObservedLoadout.Get() == InLoadout && ObservedAbilitySystem.Get() == InAbilitySystem)
	{
		RefreshSlots();
		return;
	}

	StopCooldownRefreshTimer();
	UnregisterMessageListener();
	ObservedLoadout = InLoadout;
	ObservedAbilitySystem = InAbilitySystem;
	RegisterMessageListener();
	RefreshSlots();
	StartCooldownRefreshTimer();
}

void URpgWeaponAbilityLoadoutViewModel::UnbindWeaponAbilityLoadout()
{
	StopCooldownRefreshTimer();
	UnregisterMessageListener();
	ObservedLoadout.Reset();
	ObservedAbilitySystem.Reset();
	RefreshSlots();
}

void URpgWeaponAbilityLoadoutViewModel::RefreshSlots()
{
	const URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get();
	const TArray<FRpgWeaponAbilityLoadoutSlot> SourceSlots = Loadout ? Loadout->GetSlots() : TArray<FRpgWeaponAbilityLoadoutSlot>();
	const int32 SlotCount = Loadout ? FMath::Max(Loadout->GetNumSlots(), SourceSlots.Num()) : 3;

	TArray<TObjectPtr<URpgWeaponAbilitySlotViewModel>> PreviousSlots = MoveTemp(Slots);
	Slots.Reset();
	Slots.Reserve(SlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		URpgWeaponAbilitySlotViewModel* SlotViewModel = PreviousSlots.IsValidIndex(SlotIndex) ? PreviousSlots[SlotIndex].Get() : nullptr;
		if (!SlotViewModel)
		{
			SlotViewModel = NewObject<URpgWeaponAbilitySlotViewModel>(this);
		}

		const FRpgWeaponAbilityLoadoutSlot EmptySlot;
		const FRpgWeaponAbilityLoadoutSlot& SourceSlot = SourceSlots.IsValidIndex(SlotIndex) ? SourceSlots[SlotIndex] : EmptySlot;
		SlotViewModel->InitializeSlotWithAbilitySystem(SlotIndex, SourceSlot, ObservedAbilitySystem.Get());
		Slots.Add(SlotViewModel);
	}

	if (!AreWeaponAbilitySlotViewModelArraysEqual(PreviousSlots, Slots))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	}
	OnSlotsChanged.Broadcast();
}

void URpgWeaponAbilityLoadoutViewModel::RefreshCooldowns()
{
	const URpgAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get();
	for (URpgWeaponAbilitySlotViewModel* Slot : Slots)
	{
		if (Slot)
		{
			Slot->RefreshCooldown(AbilitySystem);
		}
	}
}

TArray<URpgWeaponAbilitySlotViewModel*> URpgWeaponAbilityLoadoutViewModel::GetSlots() const
{
	TArray<URpgWeaponAbilitySlotViewModel*> Result;
	Result.Reserve(Slots.Num());
	for (URpgWeaponAbilitySlotViewModel* Slot : Slots)
	{
		Result.Add(Slot);
	}
	return Result;
}

URpgWeaponAbilitySlotViewModel* URpgWeaponAbilityLoadoutViewModel::GetSlotAtIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

void URpgWeaponAbilityLoadoutViewModel::BeginDestroy()
{
	StopCooldownRefreshTimer();
	UnregisterMessageListener();
	Super::BeginDestroy();
}

void URpgWeaponAbilityLoadoutViewModel::RegisterMessageListener()
{
	UnregisterMessageListener();

	URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get();
	UWorld* World = Loadout ? Loadout->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	SlotsChangedHandle = MessageSubsystem.RegisterListener<FRpgWeaponAbilityLoadoutChangedMessage>(
		RpgGameplayTags::Rpg_WeaponAbilityLoadout_Message_SlotsChanged,
		this,
		&ThisClass::HandleWeaponAbilityLoadoutChanged);
}

void URpgWeaponAbilityLoadoutViewModel::UnregisterMessageListener()
{
	if (SlotsChangedHandle.IsValid())
	{
		SlotsChangedHandle.Unregister();
	}
}

void URpgWeaponAbilityLoadoutViewModel::StartCooldownRefreshTimer()
{
	StopCooldownRefreshTimer();

	UWorld* World = nullptr;
	if (URpgAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get())
	{
		World = AbilitySystem->GetWorld();
	}
	else if (URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get())
	{
		World = Loadout->GetWorld();
	}

	if (!World || CooldownRefreshInterval <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CooldownRefreshTimerHandle,
		this,
		&ThisClass::RefreshCooldowns,
		CooldownRefreshInterval,
		true);
}

void URpgWeaponAbilityLoadoutViewModel::StopCooldownRefreshTimer()
{
	UWorld* World = nullptr;
	if (URpgAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get())
	{
		World = AbilitySystem->GetWorld();
	}
	else if (URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get())
	{
		World = Loadout->GetWorld();
	}

	if (World)
	{
		World->GetTimerManager().ClearTimer(CooldownRefreshTimerHandle);
	}
	CooldownRefreshTimerHandle.Invalidate();
}

void URpgWeaponAbilityLoadoutViewModel::HandleWeaponAbilityLoadoutChanged(FGameplayTag Channel, const FRpgWeaponAbilityLoadoutChangedMessage& Message)
{
	const URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get();
	if (Loadout && Message.LoadoutComponent == Loadout)
	{
		RefreshSlots();
	}
}
