#include "RpgQuickAccessRadialWidget.h"

#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerGameplayInputRouterComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgQuickAccessRadialWidget)

URpgQuickAccessRadialWidget::URpgQuickAccessRadialWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

void URpgQuickAccessRadialWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ARpgPlayerController* PlayerController = GetRpgOwningPlayer();
	ObservedInputRouter = PlayerController ? PlayerController->GetGameplayInputRouterComponent() : nullptr;
	if (ObservedInputRouter)
	{
		ObservedInputRouter->OnQuickAccessRadialChanged.AddUniqueDynamic(this, &ThisClass::HandleRadialChanged);
		bRadialOpen = ObservedInputRouter->IsQuickAccessRadialOpen();
		SelectedSlotIndex = ObservedInputRouter->GetQuickAccessRadialSelection();
	}

	SetVisibility(bRadialOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void URpgQuickAccessRadialWidget::NativeDestruct()
{
	if (ObservedInputRouter)
	{
		ObservedInputRouter->OnQuickAccessRadialChanged.RemoveDynamic(this, &ThisClass::HandleRadialChanged);
	}
	ObservedInputRouter = nullptr;
	Super::NativeDestruct();
}

int32 URpgQuickAccessRadialWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 NextLayer = LayerId;
	if (bRadialOpen)
	{
		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FSlateFontInfo SlotFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12);
		const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
		const URpgActionBarComponent* ActionBar = GetRpgOwningPlayer()
			? GetRpgOwningPlayer()->GetActionBarComponent()
			: nullptr;

		for (int32 SlotIndex = 0; SlotIndex < 8; ++SlotIndex)
		{
			const float Angle = static_cast<float>(SlotIndex) * (2.0f * PI / 8.0f);
			const FVector2D Direction(FMath::Sin(Angle), -FMath::Cos(Angle));
			const FVector2D TopLeft = Center + Direction * SegmentRadius - SegmentSize * 0.5f;
			const FRpgActionBarSlot Binding = ActionBar ? ActionBar->GetSlot(SlotIndex) : FRpgActionBarSlot();
			const FLinearColor FillColor = SlotIndex == SelectedSlotIndex
				? SelectedSegmentColor
				: (!Binding.IsEmpty() && !Binding.bAvailable ? BlockedSegmentColor : SegmentColor);

			if (WhiteBrush)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					NextLayer,
					AllottedGeometry.ToPaintGeometry(FVector2f(SegmentSize), FSlateLayoutTransform(FVector2f(TopLeft))),
					WhiteBrush,
					ESlateDrawEffect::None,
					FillColor);
			}

			FSlateDrawElement::MakeText(
				OutDrawElements,
				NextLayer + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(SegmentSize - FVector2D(12.0f, 8.0f)), FSlateLayoutTransform(FVector2f(TopLeft + FVector2D(6.0f, 4.0f)))),
				BuildSlotLabel(SlotIndex),
				SlotFont,
				ESlateDrawEffect::None,
				FLinearColor::White);
		}
		NextLayer += 2;
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NextLayer, InWidgetStyle, bParentEnabled);
}

void URpgQuickAccessRadialWidget::HandleRadialChanged(bool bIsOpen, int32 InSelectedSlotIndex)
{
	bRadialOpen = bIsOpen;
	SelectedSlotIndex = bIsOpen ? InSelectedSlotIndex : INDEX_NONE;
	SetVisibility(bRadialOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	InvalidateLayoutAndVolatility();
	BP_OnQuickAccessRadialChanged(bRadialOpen, SelectedSlotIndex);
}

FString URpgQuickAccessRadialWidget::BuildSlotLabel(int32 SlotIndex) const
{
	const ARpgPlayerController* PlayerController = GetRpgOwningPlayer();
	const URpgActionBarComponent* ActionBar = PlayerController ? PlayerController->GetActionBarComponent() : nullptr;
	const FRpgActionBarSlot Binding = ActionBar ? ActionBar->GetSlot(SlotIndex) : FRpgActionBarSlot();

	FString ActionLabel(TEXT("Empty"));
	switch (Binding.SlotType)
	{
	case ERpgActionBarSlotType::CarrySlot:
		ActionLabel = Binding.CarryRole.IsNone() ? TEXT("Carry") : Binding.CarryRole.ToString();
		break;
	case ERpgActionBarSlotType::Consumable:
		ActionLabel = Binding.ConsumableDefinition ? GetNameSafe(Binding.ConsumableDefinition.Get()) : TEXT("Consumable");
		break;
	case ERpgActionBarSlotType::Ability:
		ActionLabel = Binding.AbilityId.IsValid() ? Binding.AbilityId.ToString() : TEXT("Ability");
		break;
	default:
		break;
	}

	if (!Binding.IsEmpty() && !Binding.bAvailable)
	{
		ActionLabel += TEXT(" [Blocked]");
	}
	return FString::Printf(TEXT("%d  %s"), SlotIndex + 1, *ActionLabel);
}

ARpgPlayerController* URpgQuickAccessRadialWidget::GetRpgOwningPlayer() const
{
	return Cast<ARpgPlayerController>(GetOwningPlayer());
}
