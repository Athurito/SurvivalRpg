#include "RpgPlayerGameplayInputRouterComponent.h"

#include "CommonInputModeTypes.h"
#include "Engine/LocalPlayer.h"
#include "Input/CommonUIActionRouterBase.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/UI/RpgUIScreenBlueprintLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerGameplayInputRouterComponent)

URpgPlayerGameplayInputRouterComponent::URpgPlayerGameplayInputRouterComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgPlayerGameplayInputRouterComponent::BeginPlay()
{
	Super::BeginPlay();
	BindCommonUiInputRouter();
}

void URpgPlayerGameplayInputRouterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelQuickAccessRadial();
	SetQuickAccessLookSuppressed(false);
	UnbindCommonUiInputRouter();
	Super::EndPlay(EndPlayReason);
}

void URpgPlayerGameplayInputRouterComponent::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	BindCommonUiInputRouter();
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
	if (bQuickAccessRadialOpen || !CanBeginQuickAccessRadial())
	{
		return;
	}

	SetQuickAccessRadialState(true, INDEX_NONE);
}

void URpgPlayerGameplayInputRouterComponent::UpdateQuickAccessRadial(FVector2D StickInput)
{
	if (!bQuickAccessRadialOpen)
	{
		return;
	}

	SetQuickAccessRadialState(
		true,
		ResolveQuickAccessRadialSelection(StickInput, QuickAccessRadialDeadZone));
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
	SetQuickAccessRadialState(false, INDEX_NONE);
}

int32 URpgPlayerGameplayInputRouterComponent::ResolveQuickAccessRadialSelection(
	FVector2D StickInput,
	float DeadZone)
{
	if (StickInput.ContainsNaN())
	{
		return INDEX_NONE;
	}

	const float ClampedDeadZone = FMath::Clamp(DeadZone, 0.0f, 1.0f);
	const float StickMagnitudeSquared = StickInput.SizeSquared();
	if (StickMagnitudeSquared <= UE_SMALL_NUMBER ||
		StickMagnitudeSquared < FMath::Square(ClampedDeadZone))
	{
		return INDEX_NONE;
	}

	// atan2(X, Y) makes segment zero point up and advances clockwise, matching the authored HUD layout.
	constexpr float SegmentAngle = 2.0f * PI / static_cast<float>(QuickAccessRadialSlotCount);
	float ClockwiseAngle = FMath::Atan2(StickInput.X, StickInput.Y);
	if (ClockwiseAngle < 0.0f)
	{
		ClockwiseAngle += 2.0f * PI;
	}

	return FMath::FloorToInt((ClockwiseAngle + SegmentAngle * 0.5f) / SegmentAngle)
		% QuickAccessRadialSlotCount;
}

bool URpgPlayerGameplayInputRouterComponent::CanBeginQuickAccessRadial()
{
	const ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner());
	if (!RpgPC || !RpgPC->IsLocalController() || !RpgPC->GetPawn())
	{
		return false;
	}

	BindCommonUiInputRouter();
	return !ObservedCommonUiInputRouter.IsValid() ||
		ObservedCommonUiInputRouter->GetActiveInputMode(ECommonInputMode::Game) !=
			ECommonInputMode::Menu;
}

void URpgPlayerGameplayInputRouterComponent::BindCommonUiInputRouter()
{
	if (ObservedCommonUiInputRouter.IsValid())
	{
		return;
	}

	const ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner());
	ULocalPlayer* LocalPlayer = RpgPC ? RpgPC->GetLocalPlayer() : nullptr;
	UCommonUIActionRouterBase* CommonUiInputRouter =
		LocalPlayer ? LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	if (!CommonUiInputRouter)
	{
		return;
	}

	ObservedCommonUiInputRouter = CommonUiInputRouter;
	ActiveInputModeChangedHandle =
		CommonUiInputRouter->OnActiveInputModeChanged().AddUObject(
			this,
			&ThisClass::HandleActiveInputModeChanged);
	HandleActiveInputModeChanged(
		CommonUiInputRouter->GetActiveInputMode(ECommonInputMode::Game));
}

void URpgPlayerGameplayInputRouterComponent::UnbindCommonUiInputRouter()
{
	if (UCommonUIActionRouterBase* CommonUiInputRouter = ObservedCommonUiInputRouter.Get())
	{
		CommonUiInputRouter->OnActiveInputModeChanged().Remove(ActiveInputModeChangedHandle);
	}

	ActiveInputModeChangedHandle.Reset();
	ObservedCommonUiInputRouter.Reset();
}

void URpgPlayerGameplayInputRouterComponent::HandleActiveInputModeChanged(
	ECommonInputMode ActiveInputMode)
{
	if (ActiveInputMode == ECommonInputMode::Menu)
	{
		CancelQuickAccessRadial();
	}
}

void URpgPlayerGameplayInputRouterComponent::SetQuickAccessRadialState(bool bIsOpen, int32 SelectedSlotIndex)
{
	SelectedSlotIndex =
		bIsOpen &&
		SelectedSlotIndex >= 0 &&
		SelectedSlotIndex < QuickAccessRadialSlotCount
		? SelectedSlotIndex
		: INDEX_NONE;
	if (bQuickAccessRadialOpen == bIsOpen && QuickAccessRadialSelection == SelectedSlotIndex)
	{
		SetQuickAccessLookSuppressed(bIsOpen);
		return;
	}

	const bool bWasOpen = bQuickAccessRadialOpen;
	bQuickAccessRadialOpen = bIsOpen;
	QuickAccessRadialSelection = SelectedSlotIndex;
	if (bWasOpen != bQuickAccessRadialOpen)
	{
		SetQuickAccessLookSuppressed(bQuickAccessRadialOpen);
	}
	OnQuickAccessRadialChanged.Broadcast(bQuickAccessRadialOpen, QuickAccessRadialSelection);
}

void URpgPlayerGameplayInputRouterComponent::SetQuickAccessLookSuppressed(bool bShouldSuppress)
{
	ARpgPlayerController* RpgPC = Cast<ARpgPlayerController>(GetOwner());
	if (bOwnsQuickAccessLookSuppression == bShouldSuppress)
	{
		return;
	}

	if (bShouldSuppress)
	{
		if (!RpgPC || !RpgPC->IsLocalController())
		{
			return;
		}

		RpgPC->SetIgnoreLookInput(true);
		bOwnsQuickAccessLookSuppression = true;
		return;
	}

	if (RpgPC)
	{
		RpgPC->SetIgnoreLookInput(false);
	}
	bOwnsQuickAccessLookSuppression = false;
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
