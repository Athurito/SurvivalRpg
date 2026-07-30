#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"

#include "RpgInventoryPointerAutomationTestScreen.generated.h"

class UWidget;
class URpgInventoryDragDropCoordinator;

/**
 * Transient screen fixture that drives the real screen-level pointer router through one deterministic addressed target.
 * Gameplay commits still pass through the production drag/drop coordinator and inventory action gateway.
 */
UCLASS(NotBlueprintable, Transient)
class SURVIVALRPG_API URpgInventoryPointerAutomationTestScreen final
	: public URpgInventoryInteractionScreenWidget
{
	GENERATED_BODY()

public:
	/** Initializes the same screen-owned coordinator graph used by an activated production screen. */
	void EnsureTestInteractionObjects()
	{
		EnsureInventoryInteractionObjects();
	}

	URpgInventoryDragDropCoordinator* GetTestDragDropCoordinator() const
	{
		return GetScreenDragDropCoordinator();
	}

	/** Configures the single non-spatial target addressed by this test fixture. */
	void ConfigureAddressedTarget(
		UWidget* InTargetOwner,
		const FRpgInventoryDropTarget& InDropTarget);

	/** Enables or disables test-target addressing without bypassing the real screen router. */
	void SetTargetAddressed(bool bInTargetAddressed)
	{
		bTargetAddressed = bInTargetAddressed;
	}

	/** Invokes the production screen's NativeOnDragOver entry point. */
	bool InvokeNativePointerDragOver(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation);

	/** Invokes the production screen's NativeOnDrop entry point. */
	bool InvokeNativePointerDrop(
		const FGeometry& InGeometry,
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation);

	/** Invokes the production screen's NativeOnDragLeave entry point. */
	void InvokeNativePointerDragLeave(
		const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation);

	int32 GetPreviewRouteCount() const { return PreviewRouteCount; }
	int32 GetCommitRouteCount() const { return CommitRouteCount; }
	int32 GetExternalPreviewClearCount() const
	{
		return ExternalPreviewClearCount;
	}

	virtual bool IsEditorOnly() const override { return true; }

protected:
	virtual bool RouteInventoryPayloadToScreenSpecificTarget(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed) override;
	virtual void ClearInventoryScreenSpecificDragPreviews() override;

private:
	/** Strong test-only owner for the addressed target identity tracked weakly by the production screen. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> TargetOwner = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryDropTarget DropTarget;

	bool bTargetAddressed = true;
	int32 PreviewRouteCount = 0;
	int32 CommitRouteCount = 0;
	int32 ExternalPreviewClearCount = 0;
};
