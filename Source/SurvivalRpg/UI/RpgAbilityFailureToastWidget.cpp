#include "RpgAbilityFailureToastWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CommonLazyImage.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
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

void URpgAbilityFailureToastWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ShownResource.IsValid())
	{
		RefreshResourceBar();
	}
}

void URpgAbilityFailureToastWidget::HideAbilityFailure()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}
	ShownResource = FGameplayAttribute();
	ShownResourceMax = FGameplayAttribute();
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
	ApplyPresentation(Message);
	BP_OnAbilityFailureShown(Message);
}

void URpgAbilityFailureToastWidget::ApplyPresentation(const FRpgAbilitySimpleFailureMessage& Message)
{
	const FRpgAbilityFailurePresentation* Match = Presentations.FindByPredicate(
		[&Message](const FRpgAbilityFailurePresentation& Row)
		{
			return Row.FailureTag.IsValid() &&
				Message.FailureTags.HasTag(Row.FailureTag) &&
				(!Row.CostAttribute.IsValid() || Row.CostAttribute == Message.CostAttribute);
		});

	if (FailureIcon)
	{
		if (Match && !Match->Icon.IsNull())
		{
			FailureIcon->SetBrushFromLazyTexture(Match->Icon);
			FailureIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			FailureIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	const bool bShowResource = Match && Match->MaxAttribute.IsValid() && Message.CostAttribute.IsValid();
	ShownResource = bShowResource ? Message.CostAttribute : FGameplayAttribute();
	ShownResourceMax = bShowResource ? Match->MaxAttribute : FGameplayAttribute();
	if (ResourceBar)
	{
		ResourceBar->SetVisibility(bShowResource ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	RefreshResourceBar();
}

void URpgAbilityFailureToastWidget::RefreshResourceBar()
{
	if (!ResourceBar || !ShownResource.IsValid())
	{
		return;
	}

	const APlayerController* OwningPlayer = GetOwningPlayer();
	const UAbilitySystemComponent* AbilitySystem = OwningPlayer
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningPlayer->GetPawn())
		: nullptr;
	if (!AbilitySystem)
	{
		return;
	}

	bool bFoundValue = false;
	bool bFoundMax = false;
	const float Value = AbilitySystem->GetGameplayAttributeValue(ShownResource, bFoundValue);
	const float MaxValue = AbilitySystem->GetGameplayAttributeValue(ShownResourceMax, bFoundMax);
	ResourceBar->SetPercent(bFoundValue && bFoundMax && MaxValue > 0.0f
		? FMath::Clamp(Value / MaxValue, 0.0f, 1.0f)
		: 0.0f);
}
