#include "RpgAbilityFailureToastWidget.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAbilityFailureToastWidget)

URpgAbilityFailureToastWidget::URpgAbilityFailureToastWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

void URpgAbilityFailureToastWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	HideAbilityFailure();
}

void URpgAbilityFailureToastWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWorld* World = GetWorld(); World && !FailureListenerHandle.IsValid())
	{
		FailureListenerHandle = UGameplayMessageSubsystem::Get(World).RegisterListener(
			TAG_ABILITY_SIMPLE_FAILURE_MESSAGE,
			this,
			&ThisClass::HandleFailureMessage);
	}
}

void URpgAbilityFailureToastWidget::NativeDestruct()
{
	if (FailureListenerHandle.IsValid())
	{
		FailureListenerHandle.Unregister();
	}
	HideAbilityFailure();
	Super::NativeDestruct();
}

void URpgAbilityFailureToastWidget::ShowAbilityFailure(const FText& Reason)
{
	if (!MessageText || Reason.IsEmpty())
	{
		return;
	}

	MessageText->SetText(Reason);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
		World->GetTimerManager().SetTimer(
			HideTimerHandle,
			this,
			&ThisClass::HideAbilityFailure,
			FMath::Max(0.25f, DisplayDuration),
			false);
	}
}

void URpgAbilityFailureToastWidget::HideAbilityFailure()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void URpgAbilityFailureToastWidget::HandleFailureMessage(FGameplayTag Channel, const FRpgAbilitySimpleFailureMessage& Message)
{
	// Other local players and remote failures relayed to this client are not this HUD's concern.
	if (Message.PlayerController != GetOwningPlayer())
	{
		return;
	}

	ShowAbilityFailure(Message.UserFacingReason);
	BP_OnAbilityFailureShown(Message);
}
