#include "RpgRespawnScreenWidget.h"

#include "Components/Button.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "TimerManager.h"

URpgRespawnScreenWidget::URpgRespawnScreenWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InputConfig = ERpgWidgetInputMode::Menu;
	bIsBackHandler = true;
	bIsModal = true;
}

void URpgRespawnScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RespawnButton)
	{
		RespawnButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleRespawnButtonClicked);
	}
}

void URpgRespawnScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	BindToOwningPlayerState();
	RefreshRespawnAvailability();
}

void URpgRespawnScreenWidget::NativeOnDeactivated()
{
	UnbindFromPlayerState();
	Super::NativeOnDeactivated();
}

UWidget* URpgRespawnScreenWidget::NativeGetDesiredFocusTarget() const
{
	return RespawnButton;
}

bool URpgRespawnScreenWidget::NativeOnHandleBackAction()
{
	// Death is gameplay state, not a dismissible dialog. The controller closes
	// this screen only after the replicated waiting state clears.
	return true;
}

void URpgRespawnScreenWidget::HandleRespawnButtonClicked()
{
	if (!BoundPlayerState || !BoundPlayerState->CanRespawnNow())
	{
		RefreshRespawnAvailability();
		return;
	}

	if (RespawnButton)
	{
		RespawnButton->SetIsEnabled(false);
	}

	if (ARpgPlayerController* PlayerController =
		Cast<ARpgPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestRespawn();
	}

	// A rejected or delayed server request must not leave the local action
	// permanently disabled. Replicated state still decides whether it may run.
	ScheduleAvailabilityRefresh(0.25f);
}

void URpgRespawnScreenWidget::HandleRespawnStateChanged(
	bool bIsWaitingForRespawn,
	float RespawnAvailableServerTime)
{
	(void)bIsWaitingForRespawn;
	(void)RespawnAvailableServerTime;
	RefreshRespawnAvailability();
}

void URpgRespawnScreenWidget::BindToOwningPlayerState()
{
	ARpgPlayerState* CurrentPlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<ARpgPlayerState>()
		: nullptr;
	if (BoundPlayerState == CurrentPlayerState)
	{
		return;
	}

	UnbindFromPlayerState();
	BoundPlayerState = CurrentPlayerState;
	if (BoundPlayerState)
	{
		BoundPlayerState->OnRespawnStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleRespawnStateChanged);
	}
}

void URpgRespawnScreenWidget::UnbindFromPlayerState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AvailabilityRefreshTimer);
	}

	if (BoundPlayerState)
	{
		BoundPlayerState->OnRespawnStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRespawnStateChanged);
		BoundPlayerState = nullptr;
	}
}

void URpgRespawnScreenWidget::RefreshRespawnAvailability()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AvailabilityRefreshTimer);
	}

	const bool bCanRespawn =
		BoundPlayerState && BoundPlayerState->CanRespawnNow();
	const bool bWasEnabled = RespawnButton && RespawnButton->GetIsEnabled();
	if (RespawnButton)
	{
		RespawnButton->SetIsEnabled(bCanRespawn);
		if (bCanRespawn && !bWasEnabled && IsActivated())
		{
			RespawnButton->SetFocus();
		}
	}

	if (!BoundPlayerState || !BoundPlayerState->IsWaitingForRespawn() ||
		bCanRespawn)
	{
		return;
	}

	const float RemainingTime = BoundPlayerState->GetRemainingRespawnTime();
	if (RemainingTime > 0.0f)
	{
		ScheduleAvailabilityRefresh(RemainingTime + KINDA_SMALL_NUMBER);
	}
}

void URpgRespawnScreenWidget::ScheduleAvailabilityRefresh(float DelaySeconds)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AvailabilityRefreshTimer,
			this,
			&ThisClass::RefreshRespawnAvailability,
			FMath::Max(DelaySeconds, KINDA_SMALL_NUMBER),
			false);
	}
}
