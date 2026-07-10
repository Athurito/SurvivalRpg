#pragma once

#include "Blueprint/UserWidget.h"

#include "RpgQuickAccessRadialWidget.generated.h"

class ARpgPlayerController;
class URpgPlayerGameplayInputRouterComponent;

/**
 * Lightweight gameplay overlay for the shared eight-slot quick-access radial.
 *
 * The widget owns no bindings and never activates gameplay. It reads the controller's owner-only actionbar and
 * mirrors the input router's local open/selection state. The native paint path is a usable fallback that designers
 * may replace with a Blueprint subclass without creating a second radial truth.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgQuickAccessRadialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgQuickAccessRadialWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** True while D-Pad-Up is held and the radial is visible. Local presentation state only. */
	UFUNCTION(BlueprintPure, Category = "Quick Access|Radial")
	bool IsRadialOpen() const { return bRadialOpen; }

	/** Highlighted zero-based segment or INDEX_NONE while the stick is inside the dead zone. */
	UFUNCTION(BlueprintPure, Category = "Quick Access|Radial")
	int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/** Designer presentation hook. Slots are always read from the same owner-only actionbar used by keys 1..8. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Quick Access|Radial", meta = (DisplayName = "On Quick Access Radial Changed"))
	void BP_OnQuickAccessRadialChanged(bool bIsOpen, int32 InSelectedSlotIndex);

	/** Radius from screen center to segment center in Slate units. Cosmetic only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style", meta = (ClampMin = "32", UIMin = "32"))
	float SegmentRadius = 190.0f;

	/** Native fallback segment size. Cosmetic only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style")
	FVector2D SegmentSize = FVector2D(132.0f, 58.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style")
	FLinearColor SegmentColor = FLinearColor(0.035f, 0.03f, 0.028f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style")
	FLinearColor SelectedSegmentColor = FLinearColor(0.58f, 0.43f, 0.16f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style")
	FLinearColor BlockedSegmentColor = FLinearColor(0.18f, 0.055f, 0.045f, 0.92f);

private:
	UFUNCTION()
	void HandleRadialChanged(bool bIsOpen, int32 InSelectedSlotIndex);

	FString BuildSlotLabel(int32 SlotIndex) const;
	ARpgPlayerController* GetRpgOwningPlayer() const;

	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerGameplayInputRouterComponent> ObservedInputRouter = nullptr;

	bool bRadialOpen = false;
	int32 SelectedSlotIndex = INDEX_NONE;
};
