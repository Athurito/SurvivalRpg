#pragma once

#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include "RpgStorageInventoryWidget.generated.h"

class URpgInventoryManagerComponent;
class URpgInventoryPanelViewModel;
class URpgInventorySlotGroupPanelWidget;
class URpgInventorySpatialGridWidget;
class UWidget;

/**
 * CommonUI storage/loot screen presenter shared by chests, corpses, and dropped-loot actors.
 *
 * The widget keeps gameplay truth in the two inventory manager components from the screen payload. It projects the
 * owning player's carry/content groups and one exact secondary root grid through the same drag session and controller
 * panel navigator used by the player inventory screen.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgStorageInventoryWidget : public URpgPlayerInventoryWidget, public IRpgUIScreenPayloadReceiver
{
	GENERATED_BODY()

public:
	explicit URpgStorageInventoryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Receives and applies a storage/loot payload after Blueprint compatibility logic has run.
	 * This idempotent entry point lets the screen router finalize native spatial binding during asset migration.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Storage")
	void ApplyInventoryScreenPayload(UObject* Payload);

	/** Current validated inventory screen payload, or null after an invalid/cleared payload. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	URpgInventoryScreenPayload* GetInventoryScreenPayload() const { return InventoryScreenPayload.Get(); }

	/** Exact root handle rendered by SecondaryInventoryGrid. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
	FRpgInventoryContainerHandle GetSecondaryInventoryRootHandle() const { return SecondaryRootHandle; }

protected:
	//~IRpgUIScreenPayloadReceiver interface
	virtual void ReceiveScreenPayload_Implementation(UObject* Payload) override;
	//~End of IRpgUIScreenPayloadReceiver interface

	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual void RegisterAdditionalInventoryNavigationPanels(URpgInventoryPanelNavigationCoordinator* Navigator) override;
	virtual void AppendAdditionalSpatialGrids(TArray<URpgInventorySpatialGridWidget*>& OutGrids) const override;

	/**
	 * Optional combined player-side panel. The migration asset uses the existing styled spatial group panel class.
	 * Carry roles are listed first, followed by Pockets and equipped item-owned grids.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySlotGroupPanelWidget> PlayerGroupsPanel = nullptr;

	/** Optional exact spatial root grid for the payload's secondary inventory (10x6 by default). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialGridWidget> SecondaryInventoryGrid = nullptr;

	/** Styled spatial group panel used to replace the legacy player TileView wrapper at screen initialization. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Migration")
	TSoftClassPtr<URpgInventorySlotGroupPanelWidget> PlayerGroupsPanelClass;

	/** Styled spatial grid used to replace the legacy secondary TileView wrapper at screen initialization. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Migration")
	TSoftClassPtr<URpgInventorySpatialGridWidget> SecondaryInventoryGridClass;

	/** Presentation notification after both inventories and their exact spatial addresses have been resolved. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Storage", meta = (DisplayName = "On Storage Spatial Presenter Bound"))
	void BP_OnStorageSpatialPresenterBound(
		URpgInventoryScreenPayload* Payload,
		FRpgInventoryContainerHandle SecondaryContainer,
		bool bPlayerSpatialPanelReady,
		bool bSecondarySpatialGridReady);

private:
	UFUNCTION()
	void HandlePlayerSlotGroupsChanged();

	void EnsureSecondaryPanelViewModel();
	void EnsureSpatialReplacementWidgets();
	void RefreshCombinedPlayerGroups();
	void BindSecondarySpatialGrid();
	void ApplyLegacyWidgetVisibility();

	/** Local payload retained only for the lifetime of this active screen. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryScreenPayload> InventoryScreenPayload = nullptr;

	/** View model filtered to SecondaryRootHandle; it owns presentation state only. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryPanelViewModel> SecondaryPanelViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> SecondaryInventory = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryContainerHandle SecondaryRootHandle;

	/** Runtime-discovered legacy children. Names intentionally differ from Blueprint variables to preserve graph types. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> LegacyPlayerInventoryWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> LegacyStorageInventoryWidget = nullptr;
};
