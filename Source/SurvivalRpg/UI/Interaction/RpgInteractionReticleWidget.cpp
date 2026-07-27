// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInteractionReticleWidget.h"

#include "CommonInputModeTypes.h"
#include "Engine/LocalPlayer.h"
#include "Input/CommonUIActionRouterBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInteractionReticleWidget)

URpgInteractionReticleWidget::URpgInteractionReticleWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::Collapsed);
}

void URpgInteractionReticleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindCommonUiInputRouter();
}

void URpgInteractionReticleWidget::NativeDestruct()
{
	UnbindCommonUiInputRouter();
	SetVisibility(ESlateVisibility::Collapsed);
	Super::NativeDestruct();
}

void URpgInteractionReticleWidget::BindCommonUiInputRouter()
{
	if (ObservedCommonUiInputRouter.IsValid())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UCommonUIActionRouterBase* CommonUiInputRouter =
		LocalPlayer ? LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	if (!CommonUiInputRouter)
	{
		SetVisibility(ESlateVisibility::Collapsed);
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

void URpgInteractionReticleWidget::UnbindCommonUiInputRouter()
{
	if (UCommonUIActionRouterBase* CommonUiInputRouter = ObservedCommonUiInputRouter.Get())
	{
		CommonUiInputRouter->OnActiveInputModeChanged().Remove(ActiveInputModeChangedHandle);
	}

	ActiveInputModeChangedHandle.Reset();
	ObservedCommonUiInputRouter.Reset();
}

void URpgInteractionReticleWidget::HandleActiveInputModeChanged(ECommonInputMode ActiveInputMode)
{
	SetVisibility(
		ShouldShowForInputMode(ActiveInputMode)
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
}

bool URpgInteractionReticleWidget::ShouldShowForInputMode(ECommonInputMode ActiveInputMode)
{
	return ActiveInputMode == ECommonInputMode::Game;
}
