// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInteractionPromptWidget.h"

#include "CommonActionWidget.h"
#include "CommonLazyImage.h"
#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Input/RpgInputConfig.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionPromptData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInteractionPromptWidget)

void URpgInteractionPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildFallbackWidgetTree();
}

void URpgInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshPawnDataBinding();
	RefreshPromptPresentation();
}

void URpgInteractionPromptWidget::NativeDestruct()
{
	SetPromptData(nullptr);
	BoundIndicator = nullptr;
	ReleasePawnDataBinding();
	Super::NativeDestruct();
}

void URpgInteractionPromptWidget::SetPromptData(URpgInteractionPromptData* InPromptData)
{
	if (PromptData == InPromptData)
	{
		return;
	}

	if (PromptData)
	{
		PromptData->OnPromptChangedNative().RemoveAll(this);
	}

	PromptData = InPromptData;
	if (PromptData)
	{
		PromptData->OnPromptChangedNative().AddUObject(this, &ThisClass::HandlePromptDataChanged);
	}

	RefreshPromptPresentation();
}

void URpgInteractionPromptWidget::BindIndicator_Implementation(UIndicatorDescriptor* Indicator)
{
	BoundIndicator = Indicator;
	SetPromptData(Indicator ? Cast<URpgInteractionPromptData>(Indicator->GetDataObject()) : nullptr);
	RefreshPawnDataBinding();
}

void URpgInteractionPromptWidget::UnbindIndicator_Implementation(const UIndicatorDescriptor* Indicator)
{
	if (Indicator && BoundIndicator != Indicator)
	{
		return;
	}

	SetPromptData(nullptr);
	BoundIndicator = nullptr;
	ReleasePawnDataBinding();
}

void URpgInteractionPromptWidget::HandlePromptDataChanged(URpgInteractionPromptData* ChangedPromptData)
{
	if (ChangedPromptData == PromptData)
	{
		RefreshPromptPresentation();
	}
}

void URpgInteractionPromptWidget::RefreshPromptPresentation()
{
	const ERpgInteractionPromptState PromptState = PromptData
		? PromptData->State
		: ERpgInteractionPromptState::Hidden;

	if (ActionTextBlock)
	{
		ActionTextBlock->SetText(PromptData ? PromptData->ActionText : FText::GetEmpty());
	}
	if (TargetTextBlock)
	{
		TargetTextBlock->SetText(PromptData ? PromptData->TargetText : FText::GetEmpty());
		TargetTextBlock->SetVisibility(
			PromptState == ERpgInteractionPromptState::Nearby
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible);
	}
	if (BlockedReasonTextBlock)
	{
		const bool bOutOfRange = PromptState == ERpgInteractionPromptState::FocusedOutOfRange;
		const bool bShowBlockedReason = PromptState == ERpgInteractionPromptState::Blocked &&
			PromptData && !PromptData->BlockedReason.IsEmpty();
		BlockedReasonTextBlock->SetText(
			bOutOfRange
				? NSLOCTEXT("RpgInteraction", "PromptTooFarAway", "Too far away")
				: (bShowBlockedReason ? PromptData->BlockedReason : FText::GetEmpty()));
		BlockedReasonTextBlock->SetVisibility(
			(bOutOfRange || bShowBlockedReason)
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	if (InputActionWidget)
	{
		const bool bShowInputAction =
			PromptState == ERpgInteractionPromptState::Ready ||
			PromptState == ERpgInteractionPromptState::FocusedOutOfRange;
		InputActionWidget->SetVisibility(
			bShowInputAction ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		InputActionWidget->SetIsEnabled(PromptState == ERpgInteractionPromptState::Ready);
	}

	const TSoftObjectPtr<UTexture2D> NewIcon = PromptData
		? PromptData->Icon
		: TSoftObjectPtr<UTexture2D>();
	if (PromptIcon && AppliedPromptIcon != NewIcon)
	{
		AppliedPromptIcon = NewIcon;
		if (AppliedPromptIcon.IsNull())
		{
			PromptIcon->SetBrush(FSlateBrush());
		}
		else
		{
			PromptIcon->SetBrushFromLazyTexture(AppliedPromptIcon);
		}
	}
	if (PromptIcon)
	{
		PromptIcon->SetVisibility(
			AppliedPromptIcon.IsNull() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	SetVisibility(
		PromptState == ERpgInteractionPromptState::Hidden
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	BP_OnPromptPresentationChanged(PromptState);
}

void URpgInteractionPromptWidget::BuildFallbackWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("InteractionPromptRoot"));
	UHorizontalBox* PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("InteractionPromptRow"));
	InputActionWidget = WidgetTree->ConstructWidget<UCommonActionWidget>(
		UCommonActionWidget::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ThisClass, InputActionWidget));
	PromptIcon = WidgetTree->ConstructWidget<UCommonLazyImage>(
		UCommonLazyImage::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ThisClass, PromptIcon));
	ActionTextBlock = WidgetTree->ConstructWidget<UCommonTextBlock>(
		UCommonTextBlock::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ThisClass, ActionTextBlock));
	TargetTextBlock = WidgetTree->ConstructWidget<UCommonTextBlock>(
		UCommonTextBlock::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ThisClass, TargetTextBlock));
	BlockedReasonTextBlock = WidgetTree->ConstructWidget<UCommonTextBlock>(
		UCommonTextBlock::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ThisClass, BlockedReasonTextBlock));

	if (!Root || !PromptRow || !InputActionWidget || !PromptIcon || !ActionTextBlock ||
		!TargetTextBlock || !BlockedReasonTextBlock)
	{
		return;
	}

	WidgetTree->RootWidget = Root;
	if (UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(PromptRow))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
	}
	if (UHorizontalBoxSlot* InputSlot = PromptRow->AddChildToHorizontalBox(InputActionWidget))
	{
		InputSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		InputSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* IconSlot = PromptRow->AddChildToHorizontalBox(PromptIcon))
	{
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* ActionSlot = PromptRow->AddChildToHorizontalBox(ActionTextBlock))
	{
		ActionSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* TargetSlot = Root->AddChildToVerticalBox(TargetTextBlock))
	{
		TargetSlot->SetHorizontalAlignment(HAlign_Center);
	}
	if (UVerticalBoxSlot* ReasonSlot = Root->AddChildToVerticalBox(BlockedReasonTextBlock))
	{
		ReasonSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void URpgInteractionPromptWidget::RefreshPawnDataBinding()
{
	ARpgBasePlayerState* NewPlayerState = nullptr;
	if (const APlayerController* PlayerController = GetOwningPlayer())
	{
		NewPlayerState = PlayerController->GetPlayerState<ARpgBasePlayerState>();
	}

	if (BoundPlayerState.Get() != NewPlayerState)
	{
		ReleasePawnDataBinding();
		BoundPlayerState = NewPlayerState;
		if (NewPlayerState)
		{
			NewPlayerState->OnPawnDataChanged().AddUObject(this, &ThisClass::HandlePawnDataChanged);
		}
	}

	const URpgPawnData* PawnData = NewPlayerState
		? NewPlayerState->GetPawnData<URpgPawnData>()
		: nullptr;
	if (!PawnData)
	{
		if (const APlayerController* PlayerController = GetOwningPlayer())
		{
			if (const APawn* Pawn = PlayerController->GetPawn())
			{
				if (const URpgPawnExtensionComponent* PawnExtension =
					URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
				{
					PawnData = PawnExtension->GetPawnData<URpgPawnData>();
				}
			}
		}
	}

	ApplyInputActionFromPawnData(PawnData);
}

void URpgInteractionPromptWidget::ReleasePawnDataBinding()
{
	if (ARpgBasePlayerState* PlayerState = BoundPlayerState.Get())
	{
		PlayerState->OnPawnDataChanged().RemoveAll(this);
	}
	BoundPlayerState.Reset();

	if (InputActionWidget)
	{
		InputActionWidget->SetEnhancedInputAction(nullptr);
	}
}

void URpgInteractionPromptWidget::HandlePawnDataChanged(const URpgPawnData* NewPawnData)
{
	ApplyInputActionFromPawnData(NewPawnData);
}

void URpgInteractionPromptWidget::ApplyInputActionFromPawnData(const URpgPawnData* PawnData)
{
	if (!InputActionWidget)
	{
		return;
	}

	const UInputAction* InteractAction = ResolveInteractionInputAction(PawnData);
	if (InputActionWidget->GetEnhancedInputAction() != InteractAction)
	{
		InputActionWidget->SetEnhancedInputAction(const_cast<UInputAction*>(InteractAction));
	}
}

const UInputAction* URpgInteractionPromptWidget::ResolveInteractionInputAction(const URpgPawnData* PawnData)
{
	return PawnData && PawnData->InputConfig
		? PawnData->InputConfig->FindAbilityInputActionForTag(
			RpgGameplayTags::InputTag_Ability_Interact,
			/*bLogNotFound=*/ false)
		: nullptr;
}
