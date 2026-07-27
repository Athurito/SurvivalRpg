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
	const FPromptPresentationRules PresentationRules = ResolvePresentationRules(PromptState);

	if (ActionTextBlock)
	{
		ActionTextBlock->SetText(
			PresentationRules.bShowActionText && PromptData
				? PromptData->ActionText
				: FText::GetEmpty());
		ActionTextBlock->SetVisibility(
			PresentationRules.bShowActionText
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (TargetTextBlock)
	{
		// The default prompt is intentionally one line. Authored widgets may still read TargetText
		// from PromptData in their presentation hook when a specialized layout needs it.
		TargetTextBlock->SetText(FText::GetEmpty());
		TargetTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BlockedReasonTextBlock)
	{
		BlockedReasonTextBlock->SetText(ResolveBlockedReasonText(PromptData, PromptState));
		BlockedReasonTextBlock->SetVisibility(
			PresentationRules.bShowBlockedReason
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (BlockedIcon)
	{
		BlockedIcon->SetVisibility(
			PresentationRules.bShowBlockedReason
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (NearbyMarker)
	{
		NearbyMarker->SetVisibility(
			PresentationRules.bShowNearbyMarker
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	if (InputActionWidget)
	{
		InputActionWidget->SetVisibility(
			PresentationRules.bShowInputAction
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
		InputActionWidget->SetIsEnabled(PresentationRules.bShowInputAction);
	}
	if (InputActionContainer)
	{
		InputActionContainer->SetVisibility(
			PresentationRules.bShowInputAction
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
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
			PresentationRules.bShowPromptIcon && !AppliedPromptIcon.IsNull()
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	SetVisibility(
		PresentationRules.bShowWidget
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	BP_OnPromptPresentationChanged(PromptState);
}

URpgInteractionPromptWidget::FPromptPresentationRules
URpgInteractionPromptWidget::ResolvePresentationRules(ERpgInteractionPromptState PromptState)
{
	FPromptPresentationRules Rules;
	Rules.bShowWidget = PromptState != ERpgInteractionPromptState::Hidden;
	Rules.bShowActionText = PromptState == ERpgInteractionPromptState::Ready;
	Rules.bShowInputAction = PromptState == ERpgInteractionPromptState::Ready;
	Rules.bShowPromptIcon = PromptState == ERpgInteractionPromptState::Ready;
	Rules.bShowBlockedReason = PromptState == ERpgInteractionPromptState::Blocked;
	Rules.bShowNearbyMarker =
		PromptState == ERpgInteractionPromptState::Nearby ||
		PromptState == ERpgInteractionPromptState::FocusedOutOfRange;
	return Rules;
}

FText URpgInteractionPromptWidget::ResolveBlockedReasonText(
	const URpgInteractionPromptData* InPromptData,
	ERpgInteractionPromptState PromptState)
{
	if (PromptState != ERpgInteractionPromptState::Blocked)
	{
		return FText::GetEmpty();
	}

	return InPromptData && !InPromptData->BlockedReason.IsEmpty()
		? InPromptData->BlockedReason
		: NSLOCTEXT("RpgInteraction", "PromptNotAvailable", "Not available");
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
	UCommonTextBlock* FallbackBlockedIcon = WidgetTree->ConstructWidget<UCommonTextBlock>(
		UCommonTextBlock::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ThisClass, BlockedIcon));
	BlockedIcon = FallbackBlockedIcon;
	UCommonTextBlock* FallbackNearbyMarker = WidgetTree->ConstructWidget<UCommonTextBlock>(
		UCommonTextBlock::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ThisClass, NearbyMarker));
	NearbyMarker = FallbackNearbyMarker;

	if (!Root || !PromptRow || !InputActionWidget || !PromptIcon || !ActionTextBlock ||
		!TargetTextBlock || !BlockedReasonTextBlock || !FallbackBlockedIcon ||
		!FallbackNearbyMarker)
	{
		return;
	}
	FallbackBlockedIcon->SetText(FText::FromString(TEXT("!")));
	FallbackNearbyMarker->SetText(FText::FromString(TEXT("\u25CB")));

	WidgetTree->RootWidget = Root;
	if (UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(PromptRow))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
	}
	if (UHorizontalBoxSlot* NearbyMarkerSlot = PromptRow->AddChildToHorizontalBox(NearbyMarker))
	{
		NearbyMarkerSlot->SetVerticalAlignment(VAlign_Center);
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
	if (UHorizontalBoxSlot* BlockedIconSlot = PromptRow->AddChildToHorizontalBox(BlockedIcon))
	{
		BlockedIconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		BlockedIconSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* ActionSlot = PromptRow->AddChildToHorizontalBox(ActionTextBlock))
	{
		ActionSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* ReasonSlot = PromptRow->AddChildToHorizontalBox(BlockedReasonTextBlock))
	{
		ReasonSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* TargetSlot = Root->AddChildToVerticalBox(TargetTextBlock))
	{
		TargetSlot->SetHorizontalAlignment(HAlign_Center);
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
