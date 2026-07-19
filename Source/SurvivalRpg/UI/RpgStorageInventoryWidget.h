#pragma once

#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include "RpgStorageInventoryWidget.generated.h"

class URpgInventoryManagerComponent;
class URpgInventoryPanelViewModel;
class URpgInventorySlotGroupPanelWidget;
class URpgInventorySpatialGridWidget;
class URpgPlayerInventoryViewModel;

/**
 * CommonUI storage/loot screen presenter shared by chests, corpses, and dropped-loot actors.
 *
 * The widget keeps gameplay truth in the two inventory manager components from the screen payload. It projects the
 * owning player's carry/content groups and one exact secondary root grid through one screen-owned drag session and
 * controller panel navigator. It deliberately does not inherit the Player screen's Gear, Carry, or Actionbar hosts.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgStorageInventoryWidget : public URpgInventoryInteractionScreenWidget, public IRpgUIScreenPayloadReceiver
{
	GENERATED_BODY()

public:
	/** Current validated inventory screen payload, or null after an invalid/cleared payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	URpgInventoryScreenPayload* GetInventoryScreenPayload() const { return InventoryScreenPayload.Get(); }

	/** Exact root handle rendered by SecondaryInventoryGrid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	FRpgInventoryContainerHandle GetSecondaryInventoryRootHandle() const { return SecondaryRootHandle; }

	/**
	 * Local presentation generation incremented once whenever a staged dual-inventory context is actually bound.
	 * Diagnostic-only: it is never authoritative, replicated, or saved.
	 */
	uint32 GetStoragePresentationBindGeneration() const { return StoragePresentationBindGeneration; }

	/**
	 * Storage-owned aggregate projection for the player side.
	 * The VM is presentation-only, has this screen as its Outer, and remains stable across CommonUI pooling.
	 */
	URpgPlayerInventoryViewModel* GetStoragePlayerInventoryViewModel() const
	{
		return StoragePlayerInventoryViewModel;
	}

protected:
	//~IRpgUIScreenPayloadReceiver interface
	virtual void ReceiveScreenPayload_Implementation(UObject* Payload) override;
	//~End of IRpgUIScreenPayloadReceiver interface

	virtual void BindInventoryScreenPresentation() override;
	virtual void UnbindInventoryScreenPresentation() override;
	virtual void ForwardInventoryInteractionContextToChildren() override;
	virtual void RegisterInventoryScreenNavigationPanels(
		URpgInventoryPanelNavigationCoordinator* Navigator) override;
	virtual void AppendInventoryScreenSpatialGrids(
		TArray<URpgInventorySpatialGridWidget*>& OutGrids) const override;

	/**
	 * Canonical combined player-side panel authored in CUI_StorageSpatial.
	 * Carry roles are listed first, followed by Pockets and equipped item-owned grids.
	 * Optional metadata keeps the retired CUI_StorageContainer asset loadable as a rollback artifact; the canonical
	 * asset contract is enforced by SurvivalRpg.Inventory.UI.StorageSpatialComposition.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupPanelWidget> PlayerGroupsPanel = nullptr;

	/**
	 * Canonical authored spatial root grid for the payload's secondary inventory (10x6 by default).
	 * Optional only for loading the retired rollback asset; CUI_StorageSpatial is tested to provide this binding.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialGridWidget> SecondaryInventoryGrid = nullptr;

private:
	/** Validates and stages Payload; inactive CommonUI screens defer all coordinator and view-model binding. */
	void ApplyInventoryScreenPayload(UObject* Payload);

	/**
	 * Binds the currently staged context exactly once for this activation or active-context transition.
	 * Returns true only when a complete context was bound during this call.
	 */
	bool BindStorageScreenContext();
	void EnsureStoragePlayerViewModel();
	void EnsureSecondaryPanelViewModel();
	void RefreshCombinedPlayerGroups();
	void BindSecondarySpatialGrid();
	void ResetStorageScreenContext();

	UFUNCTION()
	void HandleStoragePlayerSlotGroupsChanged();

	/** Validated payload staged during async screen initialization and retained only until deactivation or replacement. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryScreenPayload> InventoryScreenPayload = nullptr;

	/** Validated player inventory belonging to the staged or currently bound local presentation context. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PrimaryInventory = nullptr;

	/** View model filtered to SecondaryRootHandle; it owns presentation state only. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelViewModel> SecondaryPanelViewModel = nullptr;

	/** Storage-owned read-only projection of the owning player's carry and content groups. */
	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerInventoryViewModel> StoragePlayerInventoryViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> SecondaryInventory = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryContainerHandle SecondaryRootHandle;

	/** True after the staged context has passed through BindStorageScreenContext for the current activation. */
	bool bStorageContextBound = false;

	/** Monotonic local diagnostic used to verify that lifecycle transitions do not bind the same context twice. */
	uint32 StoragePresentationBindGeneration = 0;
};
