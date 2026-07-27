// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgInventoryDragVisualWidget.generated.h"

class SBox;
struct FStreamableHandle;
class UBorder;
class UImage;
class USizeBox;
class UTextBlock;
class UTexture2D;

/**
 * Presentation-only drag ghost shared by spatial inventory, gear, carry, and quick-access widgets.
 *
 * The widget never validates or commits inventory mutations. It mirrors an existing drag payload and constrains its
 * Slate desired size to the exact occupied grid footprint so pointer geometry, preview, and drop routing can agree.
 */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Builds the visual from the canonical inventory drag payload and the target grid's Slate-unit cell metrics. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drag Visual")
	void ConfigureFromPayload(
		const FRpgInventoryDragPayload& Payload,
		float InCellSize,
		float InCellPadding,
		ERpgInventoryInteractionPreviewState InPreviewState = ERpgInventoryInteractionPreviewState::None);

	/**
	 * Configures presentation explicitly when a caller already resolved item UI data.
	 * InUnrotatedFootprint is definition-space; bInRotated controls the currently occupied footprint.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drag Visual")
	void ConfigureVisual(
		TSoftObjectPtr<UTexture2D> InIcon,
		int32 InStackCount,
		FRpgInventoryGridSize InUnrotatedFootprint,
		float InCellSize,
		float InCellPadding,
		ERpgInventoryInteractionPreviewState InPreviewState = ERpgInventoryInteractionPreviewState::None,
		bool bInRotated = false);

	/** Replaces the definition-space footprint and rotation without changing icon, stack, or preview semantics. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drag Visual")
	void SetFootprint(FRpgInventoryGridSize InUnrotatedFootprint, bool bInRotated);

	/** Rotates or unrotates the current footprint and optionally rotates the icon art by 90 degrees. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drag Visual")
	void SetFootprintRotated(bool bInRotated);

	/** Updates target-grid dimensions in Slate units; padding applies only between occupied cells. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drag Visual")
	void SetCellMetrics(float InCellSize, float InCellPadding);

	/** Changes semantic feedback without invalidating the ghost's layout. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Drag Visual")
	void SetPreviewState(ERpgInventoryInteractionPreviewState InPreviewState);

	/** Exact desired size currently reported to Slate for the rotated occupied footprint. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Drag Visual")
	FVector2D GetExactVisualSize() const { return ExactVisualSize; }

	/** Occupied cell dimensions after applying the current rotation. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Drag Visual")
	FRpgInventoryGridSize GetOccupiedFootprint() const;

	/** Cell size currently used by this canonical decorator, in local Slate units. */
	float GetConfiguredCellSize() const { return CellSize; }

	/** Inter-cell padding currently used by this canonical decorator, in local Slate units. */
	float GetConfiguredCellPadding() const { return CellPadding; }

	/** Presentation color resolved from the current semantic preview state. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Drag Visual")
	FLinearColor GetResolvedPreviewColor() const;

	/** Computes the exact footprint size used by both the native Slate constraint and optional Blueprint SizeBox. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Drag Visual")
	static FVector2D CalculateExactVisualSize(
		FRpgInventoryGridSize InUnrotatedFootprint,
		bool bInRotated,
		float InCellSize,
		float InCellPadding);

	/**
	 * Resolves an inset icon quad before optional clockwise rotation.
	 * Rotated quads swap their pre-rotation dimensions and remain centered, preserving the source aspect ratio.
	 */
	static void CalculateIconPaintGeometry(
		FVector2D AllottedSize,
		bool bInRotated,
		float InPadding,
		FVector2D& OutPaintPosition,
		FVector2D& OutPaintSize);

	/**
	 * Render scale for a fill-aligned UImage that rotates inside an already-rotated footprint.
	 * The inverse aspect correction creates the pre-rotation HxW quad before the 90-degree render transform.
	 */
	static FVector2D CalculateIconRenderScale(FVector2D OccupiedVisualSize, bool bInRotated);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativePreConstruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	/**
	 * Optional designer root. Name a SizeBox "RootSizeBox" to mirror the native exact-size constraint in UMG.
	 * The surrounding native SBox still guarantees correct desired size when this binding is omitted.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drag Visual|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RootSizeBox = nullptr;

	/** Optional image that receives the lazy-loaded UIData icon and the configured rotation. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drag Visual|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIcon = nullptr;

	/** Optional text that displays positive stack counts using the compact "Nx" inventory convention. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drag Visual|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StackCountText = nullptr;

	/** Optional border whose brush color reflects the semantic preview state. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Drag Visual|Bindings", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> StateBorder = nullptr;

	/** Rotates ItemIcon clockwise with the occupied footprint. Disable when the icon material handles rotation itself. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	bool bRotateIconWithFootprint = true;

	/** Thickness in Slate units of the native semantic frame used when StateBorder is not bound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Native Fallback", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float NativeBorderThickness = 2.0f;

	/** Alpha multiplier for the native semantic fill used when StateBorder is not bound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Native Fallback", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float NativeFillOpacity = 0.22f;

	/** Inset in Slate units applied to the native icon used when ItemIcon is not bound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Native Fallback", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float NativeIconPadding = 4.0f;

	/** Font size used by the native stack label when StackCountText is not bound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Native Fallback", meta = (ClampMin = "1", UIMin = "1"))
	int32 NativeStackFontSize = 12;

	/** Color used while no semantic target is active. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor NeutralColor = FLinearColor(0.35f, 0.35f, 0.35f, 0.65f);

	/** Color for a valid move into empty space. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor MoveColor = FLinearColor(0.08f, 0.82f, 0.18f, 0.82f);

	/** Color for merging into a compatible stack. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor MergeColor = FLinearColor(0.0f, 0.72f, 0.62f, 0.85f);

	/** Color for swapping two items. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor SwapColor = FLinearColor(0.95f, 0.72f, 0.08f, 0.88f);

	/** Color for a compatible equipment target. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor EquipColor = FLinearColor(0.08f, 0.72f, 0.95f, 0.88f);

	/** Color for an actionbar binding target. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor BindColor = FLinearColor(0.36f, 0.48f, 1.0f, 0.88f);

	/** Color for a valid clear/unassign target. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor ClearColor = FLinearColor(0.72f, 0.72f, 0.76f, 0.82f);

	/** Color for a locally incompatible or blocked target. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor BlockedColor = FLinearColor(0.82f, 0.06f, 0.05f, 0.88f);

	/** Color when the occupied footprint extends beyond the target grid. UI-only and safe to tune in child widgets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor OutOfBoundsColor = FLinearColor(0.95f, 0.12f, 0.04f, 0.9f);

	/** Color while a server-authoritative inventory request is awaiting acknowledgement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor PendingColor = FLinearColor(1.0f, 0.48f, 0.04f, 0.9f);

	/** Color after the server rejects the most recent request. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Drag Visual|Style")
	FLinearColor RejectedColor = FLinearColor(1.0f, 0.02f, 0.02f, 1.0f);

	/** Current lazy icon reference. Presentation-only and never authoritative inventory state. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Drag Visual|State")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Current visible stack count. Values above zero use the project's compact "Nx" inventory convention. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Drag Visual|State")
	int32 StackCount = 0;

	/** Definition-space item footprint before rotation. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Drag Visual|State")
	FRpgInventoryGridSize UnrotatedFootprint;

	/** Whether the current occupied footprint swaps width and height. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Drag Visual|State")
	bool bFootprintRotated = false;

	/** Width and height of one target-grid cell in Slate units. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Drag Visual|State")
	float CellSize = 70.0f;

	/** Space between target-grid cells in Slate units. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Drag Visual|State")
	float CellPadding = 2.0f;

	/** Current presentation-only target result; the server still performs final validation. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Drag Visual|State")
	ERpgInventoryInteractionPreviewState PreviewState = ERpgInventoryInteractionPreviewState::None;

	/** Called after native bindings have been synchronized, allowing optional Blueprint animation or materials. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Drag Visual", meta = (DisplayName = "On Drag Visual Updated"))
	void BP_OnDragVisualUpdated(
		FRpgInventoryGridSize OccupiedFootprint,
		FVector2D VisualSize,
		ERpgInventoryInteractionPreviewState NewPreviewState,
		FLinearColor PreviewColor);

private:
	void RefreshAllVisuals();
	void RefreshLayout();
	void RefreshIconAndStack();
	void RefreshIconRotation();
	void RefreshPreviewStyle();
	void NotifyBlueprintVisualUpdated();
	void InvalidateNativeFallbackPaint() const;
	void RequestIconResource();
	void HandleIconResourceLoaded(FSoftObjectPath LoadedPath);
	void CancelIconResourceRequest();

	UPROPERTY(Transient)
	FVector2D ExactVisualSize = FVector2D(70.0f, 70.0f);

	/** Strong presentation-only reference retained after the soft icon finishes loading. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedIcon = nullptr;

	FSoftObjectPath RequestedIconPath;
	TSharedPtr<FStreamableHandle> IconLoadHandle;
	TSharedPtr<SBox> NativeSizeConstraint;
};
