#pragma once

#include "CommonTileView.h"
#include "CoreMinimal.h"

#include "RpgInventoryTileView.generated.h"

class URpgInventoryDragDropCoordinator;

/**
 * CommonTileView specialization for inventory panels.
 *
 * This class centralizes ListView/TileView mouse drag/drop and controller item-accept routing so
 * entry widgets stay presentation-focused and gameplay mutations still flow through the
 * server-validated inventory UI action component.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgInventoryTileView : public UCommonTileView
{
	GENERATED_BODY()

public:
	explicit URpgInventoryTileView(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local coordinator used by all generated entry widgets and drop commits. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Current coordinator used by this inventory TileView. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	URpgInventoryDragDropCoordinator* GetDragDropCoordinator() const { return DragDropCoordinator.Get(); }

protected:
	virtual void NativeOnEntryGenerated(UUserWidget* EntryWidget) override;
	virtual UDragDropOperation* HandleListEntryDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, UUserWidget& EntryWidget) override;
	virtual TOptional<EItemDropZone> HandleListEntryCanAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual FReply HandleListEntryAcceptDrop(const FDragDropEvent& DropEvent, EItemDropZone DropZone, UUserWidget& EntryWidget) override;
	virtual void OnItemClickedInternal(UObject* Item) override;

private:
	void ApplyCoordinatorToEntry(UUserWidget* EntryWidget) const;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;

	/** If true, TileView item click/confirm uses pick/place. Leave false when entries inherit URpgInventorySlotEntryWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	bool bUseItemClickAsAccept = false;
};
