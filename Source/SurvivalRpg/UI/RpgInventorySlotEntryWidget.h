#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "CoreMinimal.h"

#include "RpgInventorySlotEntryWidget.generated.h"

class URpgInventoryDragDropCoordinator;
class URpgInventoryEntryViewModel;

/**
 * Native base for TileView inventory slot entries.
 *
 * The widget stores the current entry view model assigned by UTileView and forwards controller
 * Accept input to the screen-local drag/drop coordinator. Blueprint children should keep only
 * presentation logic here: icon, stack text, hover/drop highlights, and tooltips.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgInventorySlotEntryWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	explicit URpgInventorySlotEntryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Assigns the screen-local drag/drop coordinator that owns controller held-item state. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	void SetDragDropCoordinator(URpgInventoryDragDropCoordinator* InCoordinator);

	/** Current entry view model assigned by the owning TileView. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Entry")
	URpgInventoryEntryViewModel* GetEntryViewModel() const { return EntryViewModel.Get(); }

	/** Current drag/drop coordinator assigned by the owning inventory TileView or screen widget. */
	UFUNCTION(BlueprintPure, Category = "Inventory|DragDrop")
	URpgInventoryDragDropCoordinator* GetDragDropCoordinator() const { return DragDropCoordinator.Get(); }

	/** Controller/CommonUI Accept helper: pick the item up or place the currently held item on this slot. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|DragDrop")
	bool HandleEntryAccept();

protected:
	//~IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End of IUserObjectListEntry interface

	//~IUserListEntry interface
	virtual void NativeOnEntryReleased() override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnClicked() override;
	//~End of IUserListEntry interface

	/** Blueprint presentation hook called whenever this recycled entry receives a new view model. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Entry", meta = (DisplayName = "On Inventory Entry ViewModel Set"))
	void BP_OnInventoryEntryViewModelSet(URpgInventoryEntryViewModel* NewEntryViewModel);

	/** Blueprint presentation hook called when the TileView releases this entry for reuse. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Entry", meta = (DisplayName = "On Inventory Entry Released"))
	void BP_OnInventoryEntryReleased();

	/** Blueprint presentation hook for selected/focused list state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Entry", meta = (DisplayName = "On Inventory Entry Selection Changed"))
	void BP_OnInventoryEntrySelectionChanged(bool bIsSelected);

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|Entry", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryEntryViewModel> EntryViewModel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Inventory|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryDragDropCoordinator> DragDropCoordinator = nullptr;
};
