// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgInventoryDragVisualWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "Widgets/Layout/SBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryDragVisualWidget)

namespace
{
	FRpgInventoryGridSize SanitizeFootprint(FRpgInventoryGridSize Footprint)
	{
		Footprint.Width = FMath::Max(1, Footprint.Width);
		Footprint.Height = FMath::Max(1, Footprint.Height);
		return Footprint;
	}

	TSoftObjectPtr<UTexture2D> ResolvePayloadIcon(const FRpgInventoryDragPayload& Payload)
	{
		const URpgInventoryFragment_UIData* UIData = Payload.ItemInstance
			? Payload.ItemInstance->FindFragmentByClass<URpgInventoryFragment_UIData>()
			: nullptr;
		return UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
	}
}

void URpgInventoryDragVisualWidget::ConfigureFromPayload(
	const FRpgInventoryDragPayload& Payload,
	float InCellSize,
	float InCellPadding,
	ERpgInventoryInteractionPreviewState InPreviewState)
{
	const bool bUseSourceRotation = Payload.SourcePlacement.IsValid() && Payload.SourcePlacement.bRotated;
	ConfigureVisual(
		ResolvePayloadIcon(Payload),
		Payload.StackCount,
		Payload.ItemFootprint,
		InCellSize,
		InCellPadding,
		InPreviewState,
		bUseSourceRotation);
}

void URpgInventoryDragVisualWidget::ConfigureVisual(
	TSoftObjectPtr<UTexture2D> InIcon,
	int32 InStackCount,
	FRpgInventoryGridSize InUnrotatedFootprint,
	float InCellSize,
	float InCellPadding,
	ERpgInventoryInteractionPreviewState InPreviewState,
	bool bInRotated)
{
	Icon = MoveTemp(InIcon);
	StackCount = FMath::Max(0, InStackCount);
	UnrotatedFootprint = SanitizeFootprint(InUnrotatedFootprint);
	bFootprintRotated = bInRotated;
	CellSize = FMath::Max(1.0f, InCellSize);
	CellPadding = FMath::Max(0.0f, InCellPadding);
	PreviewState = InPreviewState;
	RefreshAllVisuals();
}

void URpgInventoryDragVisualWidget::SetFootprint(FRpgInventoryGridSize InUnrotatedFootprint, bool bInRotated)
{
	const FRpgInventoryGridSize SanitizedFootprint = SanitizeFootprint(InUnrotatedFootprint);
	if (UnrotatedFootprint == SanitizedFootprint && bFootprintRotated == bInRotated)
	{
		return;
	}

	UnrotatedFootprint = SanitizedFootprint;
	bFootprintRotated = bInRotated;
	RefreshLayout();
	RefreshIconRotation();
	NotifyBlueprintVisualUpdated();
}

void URpgInventoryDragVisualWidget::SetFootprintRotated(bool bInRotated)
{
	if (bFootprintRotated == bInRotated)
	{
		return;
	}

	bFootprintRotated = bInRotated;
	RefreshLayout();
	RefreshIconRotation();
	NotifyBlueprintVisualUpdated();
}

void URpgInventoryDragVisualWidget::SetCellMetrics(float InCellSize, float InCellPadding)
{
	const float SanitizedCellSize = FMath::Max(1.0f, InCellSize);
	const float SanitizedCellPadding = FMath::Max(0.0f, InCellPadding);
	if (FMath::IsNearlyEqual(CellSize, SanitizedCellSize) && FMath::IsNearlyEqual(CellPadding, SanitizedCellPadding))
	{
		return;
	}

	CellSize = SanitizedCellSize;
	CellPadding = SanitizedCellPadding;
	RefreshLayout();
	NotifyBlueprintVisualUpdated();
}

void URpgInventoryDragVisualWidget::SetPreviewState(ERpgInventoryInteractionPreviewState InPreviewState)
{
	if (PreviewState == InPreviewState)
	{
		return;
	}

	PreviewState = InPreviewState;
	RefreshPreviewStyle();
	NotifyBlueprintVisualUpdated();
}

FRpgInventoryGridSize URpgInventoryDragVisualWidget::GetOccupiedFootprint() const
{
	return SanitizeFootprint(UnrotatedFootprint).GetRotated(bFootprintRotated);
}

FLinearColor URpgInventoryDragVisualWidget::GetResolvedPreviewColor() const
{
	switch (PreviewState)
	{
	case ERpgInventoryInteractionPreviewState::Move:
		return MoveColor;
	case ERpgInventoryInteractionPreviewState::Merge:
		return MergeColor;
	case ERpgInventoryInteractionPreviewState::Swap:
		return SwapColor;
	case ERpgInventoryInteractionPreviewState::Equip:
		return EquipColor;
	case ERpgInventoryInteractionPreviewState::Bind:
		return BindColor;
	case ERpgInventoryInteractionPreviewState::Clear:
		return ClearColor;
	case ERpgInventoryInteractionPreviewState::Blocked:
		return BlockedColor;
	case ERpgInventoryInteractionPreviewState::OutOfBounds:
		return OutOfBoundsColor;
	case ERpgInventoryInteractionPreviewState::Pending:
		return PendingColor;
	case ERpgInventoryInteractionPreviewState::Rejected:
		return RejectedColor;
	case ERpgInventoryInteractionPreviewState::None:
	default:
		return NeutralColor;
	}
}

FVector2D URpgInventoryDragVisualWidget::CalculateExactVisualSize(
	FRpgInventoryGridSize InUnrotatedFootprint,
	bool bInRotated,
	float InCellSize,
	float InCellPadding)
{
	const FRpgInventoryGridSize OccupiedFootprint = SanitizeFootprint(InUnrotatedFootprint).GetRotated(bInRotated);
	const float SanitizedCellSize = FMath::Max(1.0f, InCellSize);
	const float SanitizedCellPadding = FMath::Max(0.0f, InCellPadding);
	return FVector2D(
		OccupiedFootprint.Width * SanitizedCellSize + FMath::Max(0, OccupiedFootprint.Width - 1) * SanitizedCellPadding,
		OccupiedFootprint.Height * SanitizedCellSize + FMath::Max(0, OccupiedFootprint.Height - 1) * SanitizedCellPadding);
}

TSharedRef<SWidget> URpgInventoryDragVisualWidget::RebuildWidget()
{
	const TSharedRef<SWidget> DesignerContent = Super::RebuildWidget();
	ExactVisualSize = CalculateExactVisualSize(UnrotatedFootprint, bFootprintRotated, CellSize, CellPadding);

	return SAssignNew(NativeSizeConstraint, SBox)
		.WidthOverride(ExactVisualSize.X)
		.HeightOverride(ExactVisualSize.Y)
		[
			DesignerContent
		];
}

void URpgInventoryDragVisualWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	NativeSizeConstraint.Reset();
}

void URpgInventoryDragVisualWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Drag decorators must never intercept routing intended for the inventory screen below them.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshAllVisuals();
}

int32 URpgInventoryDragVisualWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	int32 ContentLayer = LayerId;
	int32 HighestLayer = LayerId;

	if (!StateBorder && WhiteBrush)
	{
		const FLinearColor WidgetTint = InWidgetStyle.GetColorAndOpacityTint();
		const FLinearColor BorderColor = GetResolvedPreviewColor() * WidgetTint;
		FLinearColor FillColor = BorderColor;
		FillColor.A *= FMath::Clamp(NativeFillOpacity, 0.0f, 1.0f);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			ContentLayer++,
			AllottedGeometry.ToPaintGeometry(),
			WhiteBrush,
			ESlateDrawEffect::None,
			FillColor);

		const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		const float BorderThickness = FMath::Clamp(NativeBorderThickness, 0.0f, FMath::Min(LocalSize.X, LocalSize.Y) * 0.5f);
		if (BorderThickness > KINDA_SMALL_NUMBER)
		{
			const auto DrawBorderStrip = [&](const FVector2D& Position, const FVector2D& Size)
			{
				if (Size.X > KINDA_SMALL_NUMBER && Size.Y > KINDA_SMALL_NUMBER)
				{
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						ContentLayer,
						AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
						WhiteBrush,
						ESlateDrawEffect::None,
						BorderColor);
				}
			};

			DrawBorderStrip(FVector2D::ZeroVector, FVector2D(LocalSize.X, BorderThickness));
			DrawBorderStrip(FVector2D(0.0f, LocalSize.Y - BorderThickness), FVector2D(LocalSize.X, BorderThickness));
			DrawBorderStrip(
				FVector2D(0.0f, BorderThickness),
				FVector2D(BorderThickness, FMath::Max(0.0f, LocalSize.Y - BorderThickness * 2.0f)));
			DrawBorderStrip(
				FVector2D(LocalSize.X - BorderThickness, BorderThickness),
				FVector2D(BorderThickness, FMath::Max(0.0f, LocalSize.Y - BorderThickness * 2.0f)));
			++ContentLayer;
		}
		HighestLayer = FMath::Max(HighestLayer, ContentLayer - 1);
	}

	const int32 PaintedLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		ContentLayer,
		InWidgetStyle,
		bParentEnabled);
	HighestLayer = FMath::Max(HighestLayer, PaintedLayer);
	int32 NextLayer = HighestLayer + 1;

	if (!ItemIcon && Icon.Get() && WhiteBrush)
	{
		const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		const float IconPadding = FMath::Max(0.0f, NativeIconPadding);
		const FVector2D IconSize(
			FMath::Max(0.0f, LocalSize.X - IconPadding * 2.0f),
			FMath::Max(0.0f, LocalSize.Y - IconPadding * 2.0f));
		if (IconSize.X > KINDA_SMALL_NUMBER && IconSize.Y > KINDA_SMALL_NUMBER)
		{
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(Icon.Get());
			IconBrush.ImageSize = IconSize;
			const FLinearColor IconTint = InWidgetStyle.GetColorAndOpacityTint();

			if (bRotateIconWithFootprint && bFootprintRotated)
			{
				const FVector2D UnrotatedIconSize(IconSize.Y, IconSize.X);
				const FVector2D UnrotatedIconOffset(
					IconPadding + (IconSize.X - UnrotatedIconSize.X) * 0.5f,
					IconPadding + (IconSize.Y - UnrotatedIconSize.Y) * 0.5f);
				FSlateDrawElement::MakeRotatedBox(
					OutDrawElements,
					NextLayer++,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(UnrotatedIconSize),
						FSlateLayoutTransform(FVector2f(UnrotatedIconOffset))),
					&IconBrush,
					ESlateDrawEffect::None,
					UE_HALF_PI,
					FVector2f(UnrotatedIconSize * 0.5f),
					FSlateDrawElement::RelativeToElement,
					IconTint);
			}
			else
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					NextLayer++,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(IconSize),
						FSlateLayoutTransform(FVector2f(IconPadding, IconPadding))),
					&IconBrush,
					ESlateDrawEffect::None,
					IconTint);
			}
		}
	}

	if (!StackCountText && StackCount > 0)
	{
		const FString StackText = FString::Printf(TEXT("%dx"), StackCount);
		const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FMath::Max(1, NativeStackFontSize));
		const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		const FVector2D TextSize(34.0f, FMath::Max(18.0f, static_cast<float>(NativeStackFontSize) + 4.0f));
		const FVector2D TextPosition(
			FMath::Max(2.0f, LocalSize.X - TextSize.X - 2.0f),
			FMath::Max(2.0f, LocalSize.Y - TextSize.Y - 2.0f));
		FSlateDrawElement::MakeText(
			OutDrawElements,
			NextLayer++,
			AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(TextPosition))),
			StackText,
			FontInfo,
			ESlateDrawEffect::None,
			FLinearColor::White * InWidgetStyle.GetColorAndOpacityTint());
	}

	return FMath::Max(PaintedLayer, NextLayer - 1);
}

void URpgInventoryDragVisualWidget::RefreshAllVisuals()
{
	RefreshLayout();
	RefreshIconAndStack();
	RefreshPreviewStyle();
	NotifyBlueprintVisualUpdated();
}

void URpgInventoryDragVisualWidget::RefreshLayout()
{
	const FVector2D NewExactVisualSize = CalculateExactVisualSize(UnrotatedFootprint, bFootprintRotated, CellSize, CellPadding);
	const bool bSizeChanged = !ExactVisualSize.Equals(NewExactVisualSize);
	ExactVisualSize = NewExactVisualSize;

	if (NativeSizeConstraint.IsValid())
	{
		NativeSizeConstraint->SetWidthOverride(FOptionalSize(ExactVisualSize.X));
		NativeSizeConstraint->SetHeightOverride(FOptionalSize(ExactVisualSize.Y));
	}

	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(ExactVisualSize.X);
		RootSizeBox->SetHeightOverride(ExactVisualSize.Y);
	}

	if (bSizeChanged)
	{
		InvalidateLayoutAndVolatility();
	}
}

void URpgInventoryDragVisualWidget::RefreshIconAndStack()
{
	if (ItemIcon)
	{
		if (Icon.IsNull())
		{
			// SetBrushFromTexture also cancels any stale async request from a previously reused drag visual.
			ItemIcon->SetBrushFromTexture(nullptr, false);
		}
		else
		{
			ItemIcon->SetBrushFromSoftTexture(Icon, false);
		}
		RefreshIconRotation();
	}

	if (StackCountText)
	{
		StackCountText->SetText(StackCount > 0
			? FText::FromString(FString::Printf(TEXT("%dx"), StackCount))
			: FText::GetEmpty());
		StackCountText->SetVisibility(StackCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	InvalidateNativeFallbackPaint();
}

void URpgInventoryDragVisualWidget::RefreshIconRotation()
{
	if (ItemIcon)
	{
		ItemIcon->SetRenderTransformAngle(bRotateIconWithFootprint && bFootprintRotated ? 90.0f : 0.0f);
	}
	InvalidateNativeFallbackPaint();
}

void URpgInventoryDragVisualWidget::RefreshPreviewStyle()
{
	if (StateBorder)
	{
		StateBorder->SetBrushColor(GetResolvedPreviewColor());
	}
	InvalidateNativeFallbackPaint();
}

void URpgInventoryDragVisualWidget::NotifyBlueprintVisualUpdated()
{
	BP_OnDragVisualUpdated(GetOccupiedFootprint(), ExactVisualSize, PreviewState, GetResolvedPreviewColor());
}

void URpgInventoryDragVisualWidget::InvalidateNativeFallbackPaint() const
{
	if (const TSharedPtr<SWidget> CachedWidget = GetCachedWidget())
	{
		CachedWidget->Invalidate(EInvalidateWidgetReason::Paint);
	}
}
