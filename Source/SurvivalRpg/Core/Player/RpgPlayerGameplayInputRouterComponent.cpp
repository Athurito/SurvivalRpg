#include "RpgPlayerGameplayInputRouterComponent.h"

#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgUIScreenBlueprintLibrary.h"
#include "SurvivalRpg/UI/RpgUIScreenSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerGameplayInputRouterComponent)

URpgPlayerGameplayInputRouterComponent::URpgPlayerGameplayInputRouterComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgPlayerGameplayInputRouterComponent::HandleGameplayInputPressed(FGameplayTag InputTag)
{
	ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner());
	if (!RpgPC || !InputTag.IsValid())
	{
		return;
	}

	if (InputTag == RpgGameplayTags::InputTag_UI_Inventory)
	{
		if (!RpgPC->IsLocalController())
		{
			return;
		}

		CancelQuickAccessRadial();
		URpgUIScreenBlueprintLibrary::OpenUIScreen(
			RpgPC,
			RpgGameplayTags::UI_Screen_Inventory,
			nullptr);
		return;
	}

	const int32 ActionBarSlotIndex = GetActionBarSlotIndexFromInputTag(InputTag);
	if (ActionBarSlotIndex != INDEX_NONE)
	{
		if (URpgActionBarComponent* ActionBar = RpgPC->GetActionBarComponent())
		{
			ActionBar->ActivateSlot(ActionBarSlotIndex);
		}
		return;
	}

	const int32 WeaponAbilitySlotIndex = GetWeaponAbilitySlotIndexFromInputTag(InputTag);
	if (WeaponAbilitySlotIndex != INDEX_NONE)
	{
		if (URpgWeaponAbilityLoadoutComponent* WeaponAbilityLoadout = RpgPC->GetWeaponAbilityLoadoutComponent())
		{
			WeaponAbilityLoadout->HandleInputPressed(WeaponAbilitySlotIndex);
		}
	}
}

void URpgPlayerGameplayInputRouterComponent::HandleGameplayInputReleased(FGameplayTag InputTag)
{
	ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner());
	if (!RpgPC || !InputTag.IsValid())
	{
		return;
	}

	const int32 ActionBarSlotIndex = GetActionBarSlotIndexFromInputTag(InputTag);
	if (ActionBarSlotIndex != INDEX_NONE)
	{
		if (URpgActionBarComponent* ActionBar = RpgPC->GetActionBarComponent())
		{
			ActionBar->ReleaseSlot(ActionBarSlotIndex);
		}
		return;
	}

	const int32 WeaponAbilitySlotIndex = GetWeaponAbilitySlotIndexFromInputTag(InputTag);
	if (WeaponAbilitySlotIndex != INDEX_NONE)
	{
		if (URpgWeaponAbilityLoadoutComponent* WeaponAbilityLoadout = RpgPC->GetWeaponAbilityLoadoutComponent())
		{
			WeaponAbilityLoadout->HandleInputReleased(WeaponAbilitySlotIndex);
		}
	}
}

void URpgPlayerGameplayInputRouterComponent::BeginQuickAccessRadial()
{
	const ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner());
	if (!RpgPC || !RpgPC->IsLocalController() || !RpgPC->GetPawn() || RpgPC->ShouldShowMouseCursor())
	{
		return;
	}
	if (const ULocalPlayer* LocalPlayer = RpgPC->GetLocalPlayer())
	{
		const URpgUIScreenSubsystem* Screens = LocalPlayer->GetSubsystem<URpgUIScreenSubsystem>();
		if (Screens &&
			(Screens->IsScreenActiveOrPending(RpgGameplayTags::UI_Screen_Inventory) ||
			 Screens->IsScreenActiveOrPending(RpgGameplayTags::UI_Screen_Storage) ||
			 Screens->IsScreenActiveOrPending(RpgGameplayTags::UI_Screen_Loot)))
		{
			return;
		}
	}

	SetQuickAccessRadialState(true, INDEX_NONE);
}

void URpgPlayerGameplayInputRouterComponent::UpdateQuickAccessRadial(FVector2D StickInput)
{
	if (!bQuickAccessRadialOpen)
	{
		return;
	}

	int32 NewSelection = INDEX_NONE;
	if (StickInput.SizeSquared() >= FMath::Square(QuickAccessRadialDeadZone))
	{
		// atan2(X, Y) makes segment zero point up and advances clockwise, matching the HUD layout.
		constexpr float SegmentAngle = 2.0f * PI / 8.0f;
		float ClockwiseAngle = FMath::Atan2(StickInput.X, StickInput.Y);
		if (ClockwiseAngle < 0.0f)
		{
			ClockwiseAngle += 2.0f * PI;
		}
		NewSelection = FMath::FloorToInt((ClockwiseAngle + SegmentAngle * 0.5f) / SegmentAngle) % 8;
	}

	SetQuickAccessRadialState(true, NewSelection);
}

void URpgPlayerGameplayInputRouterComponent::CommitQuickAccessRadial()
{
	if (!bQuickAccessRadialOpen)
	{
		return;
	}

	const int32 SlotToTrigger = QuickAccessRadialSelection;
	SetQuickAccessRadialState(false, INDEX_NONE);
	if (SlotToTrigger != INDEX_NONE)
	{
		if (ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner()))
		{
			if (URpgActionBarComponent* ActionBar = RpgPC->GetActionBarComponent())
			{
				ActionBar->TriggerSlot(SlotToTrigger);
			}
		}
	}
}

void URpgPlayerGameplayInputRouterComponent::CancelQuickAccessRadial()
{
	if (bQuickAccessRadialOpen)
	{
		SetQuickAccessRadialState(false, INDEX_NONE);
	}
}

void URpgPlayerGameplayInputRouterComponent::SetQuickAccessRadialState(bool bIsOpen, int32 SelectedSlotIndex)
{
	SelectedSlotIndex = bIsOpen && SelectedSlotIndex >= 0 && SelectedSlotIndex < 8
		? SelectedSlotIndex
		: INDEX_NONE;
	if (bQuickAccessRadialOpen == bIsOpen && QuickAccessRadialSelection == SelectedSlotIndex)
	{
		return;
	}

	bQuickAccessRadialOpen = bIsOpen;
	QuickAccessRadialSelection = SelectedSlotIndex;
	OnQuickAccessRadialChanged.Broadcast(bQuickAccessRadialOpen, QuickAccessRadialSelection);
}

int32 URpgPlayerGameplayInputRouterComponent::GetActionBarSlotIndexFromInputTag(FGameplayTag InputTag)
{
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_1)
	{
		return 0;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_2)
	{
		return 1;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_3)
	{
		return 2;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_4)
	{
		return 3;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_5)
	{
		return 4;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_6)
	{
		return 5;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_7)
	{
		return 6;
	}
	if (InputTag == RpgGameplayTags::InputTag_ActionBar_Slot_8)
	{
		return 7;
	}

	return INDEX_NONE;
}

int32 URpgPlayerGameplayInputRouterComponent::GetWeaponAbilitySlotIndexFromInputTag(FGameplayTag InputTag)
{
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_1)
	{
		return 0;
	}
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_2)
	{
		return 1;
	}
	if (InputTag == RpgGameplayTags::InputTag_Weapon_Ability_3)
	{
		return 2;
	}

	return INDEX_NONE;
}
