#pragma once

#include "Blueprint/DragDropOperation.h"
#include "CoreMinimal.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"

#include "RpgInventoryDragDropOperation.generated.h"

class URpgInventoryInteractionSession;

/** Native drag operation used by mouse drag/drop inventory widgets. */
UCLASS(BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	/** UI-only payload carried by the mouse drag operation. */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory|DragDrop", meta = (ExposeOnSpawn = "true"))
	FRpgInventoryDragPayload InventoryPayload;

	/** Connects drag cancellation to the same screen-local interaction used by controller pick/place. */
	void SetInteractionSession(URpgInventoryInteractionSession* InInteractionSession);
	URpgInventoryInteractionSession* GetInteractionSession() const { return InteractionSession.Get(); }

	/** Mirrors UE's TopLeft decorator placement so target routing uses the center of the visible free ghost. */
	FVector2D ResolveDecoratorCenterScreen(FVector2D PointerScreenPosition) const;

	/** Pulls rotation/state from the shared session immediately, including when no pointer-move event is generated. */
	void SynchronizeFromInteractionSession();

	/** Suppresses UE's interpolated decorator while a screen paints the canonical free ghost itself. */
	void SetScreenOwnedDragVisualActive(bool bInActive);

	virtual void Dragged_Implementation(const FPointerEvent& PointerEvent) override;
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;

private:
	void RefreshDecoratorPointerOffset();
	bool bScreenOwnedDragVisualActive = false;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryInteractionSession> InteractionSession = nullptr;
};
