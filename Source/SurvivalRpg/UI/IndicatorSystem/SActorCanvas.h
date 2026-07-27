// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidgetPool.h"
#include "Engine/LocalPlayer.h"
#include "Widgets/SPanel.h"

class FActiveTimerHandle;
class FArrangedChildren;
class URpgIndicatorManagerComponent;
class UIndicatorDescriptor;

/**
 * Slate panel that owns pooled indicator widgets and positions them over world actors.
 */
class SActorCanvas : public SPanel, public FGCObject
{
public:
	class FSlot : public TSlotBase<FSlot>
	{
	public:
		explicit FSlot(UIndicatorDescriptor* InIndicator)
			: TSlotBase<FSlot>()
			, Indicator(InIndicator)
		{
		}

		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
		SLATE_SLOT_END_ARGS()
		using TSlotBase<FSlot>::Construct;

		UIndicatorDescriptor* Indicator = nullptr;
		FVector2D ScreenPosition = FVector2D::ZeroVector;
		float Depth = 0.0f;
		bool bHasValidPosition = false;
		uint32 ProjectedRevision = 0;
	};

	SLATE_BEGIN_ARGS(SActorCanvas)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
	SLATE_END_ARGS()

	SActorCanvas()
		: CanvasChildren(this)
	{
	}

	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InLocalPlayerContext);

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual FChildren* GetChildren() override { return &CanvasChildren; }
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	void OnIndicatorAdded(UIndicatorDescriptor* Indicator);
	void OnIndicatorRemoved(UIndicatorDescriptor* Indicator);
	void AddIndicatorWidget(UIndicatorDescriptor* Indicator);
	void RemoveIndicatorWidget(UIndicatorDescriptor* Indicator);
	EActiveTimerReturnType UpdateCanvas(double CurrentTime, float DeltaTime);
	void UpdateActiveTimer();
	void GetOffsetAndSize(const UIndicatorDescriptor* Indicator, FVector2D& OutSize, FVector2D& OutOffset) const;

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	FScopedWidgetSlotArguments AddActorSlot(UIndicatorDescriptor* Indicator);

	TArray<TObjectPtr<UIndicatorDescriptor>> ActiveIndicators;
	FLocalPlayerContext LocalPlayerContext;
	TWeakObjectPtr<URpgIndicatorManagerComponent> IndicatorManager;
	TPanelChildren<FSlot> CanvasChildren;
	FUserWidgetPool IndicatorPool;
	mutable TOptional<FGeometry> PaintGeometry;
	TSharedPtr<FActiveTimerHandle> TickHandle;
};
