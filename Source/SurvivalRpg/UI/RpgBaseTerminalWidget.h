#pragma once

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include "RpgBaseTerminalWidget.generated.h"

class UButton;
class URpgBaseResourceListWidget;
class URpgBaseStorageComponent;
class URpgBaseStorageStationComponent;
class URpgBaseStorageUpgradeDefinition;
class URpgBaseStorageViewModel;
class URpgInventoryManagerComponent;
class URpgInventorySpatialPaneWidget;
class URpgInventorySpatialGridWidget;
class URpgInventoryUiActionComponent;

/**
 * CommonUI presenter for one base terminal or filtered base-storage station.
 *
 * The screen validates and stages the local payload, owns one pooled interaction context and stable read-only base
 * storage VM, and connects authored leaves on activation. Authoritative deposit and upgrade mutations continue
 * through URpgInventoryUiActionComponent on the owning player controller.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgBaseTerminalWidget
	: public URpgInventoryInteractionScreenWidget
	, public IRpgUIScreenPayloadReceiver
{
	GENERATED_BODY()

public:
	/** Current validated payload, or null after an invalid payload or deactivation. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal")
	URpgBaseStorageScreenPayload* GetBaseStorageScreenPayload() const
	{
		return BaseStorageScreenPayload.Get();
	}

	/** Stable screen-owned read-only resource projection retained across CommonUI pooling. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal")
	URpgBaseStorageViewModel* GetBaseStorageViewModel() const
	{
		return BaseStorageViewModel.Get();
	}

	/** Authored single-container player pane used by this first terminal migration slice. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal")
	URpgInventorySpatialPaneWidget* GetPlayerInventoryPane() const
	{
		return PlayerInventoryPane.Get();
	}

	/** Exact canonical player-layout container shown by PlayerInventoryPane. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal")
	FRpgInventoryContainerHandle GetPlayerPaneContainerHandle() const
	{
		return PlayerPaneContainerHandle;
	}

	/** Local diagnostic generation incremented once for every complete presentation bind. */
	uint32 GetBaseTerminalPresentationBindGeneration() const
	{
		return BaseTerminalPresentationBindGeneration;
	}

	/** Requests an authoritative deposit of all eligible player material stacks. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal")
	void RequestDepositAllMaterials();

	/** Requests installation of the designer-selected featured terminal upgrade. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal")
	void RequestInstallFeaturedUpgrade();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
#if WITH_EDITOR
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

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
	 * Required authored Pockets pane. Its container binding is runtime state; the pane itself is static composition.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgInventorySpatialPaneWidget> PlayerInventoryPane = nullptr;

	/** Required typed resource list fed by the stable screen-owned base-storage VM. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgBaseResourceListWidget> BaseResourceList = nullptr;

	/** Optional authored pointer button for the server-validated deposit-all convenience action. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> DepositAllButton = nullptr;

	/** Optional authored pointer button for FeaturedUpgrade. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> InstallUpgradeButton = nullptr;

	/**
	 * Upgrade offered by the authored terminal. Static designer data; installation remains server-authoritative.
	 * Leave unset to hide/disable the install action without changing terminal inventory presentation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Terminal")
	TSoftObjectPtr<URpgBaseStorageUpgradeDefinition> FeaturedUpgrade;

	/** CommonUI action row for the terminal-wide deposit-all request. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Base Terminal", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle DepositAllInputAction;

	/** CommonUI action row for installing FeaturedUpgrade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Base Terminal", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle InstallUpgradeInputAction;

private:
	void ApplyBaseStorageScreenPayload(UObject* Payload);
	bool IsPayloadCoherent(const URpgBaseStorageScreenPayload* Payload) const;
	bool BindBaseTerminalContext();
	void EnsureBaseStorageViewModel();
	void RegisterBaseTerminalActionBindings();
	void UnregisterBaseTerminalActionBindings();
	void ResetBaseTerminalContext();
	void UpdateTerminalActionAvailability();
	URpgInventoryUiActionComponent* ResolveInventoryUiActionComponent() const;

	UFUNCTION()
	void HandleDepositAllClicked();

	UFUNCTION()
	void HandleInstallUpgradeClicked();

	UFUNCTION()
	void HandleInstalledUpgradesChanged(URpgBaseStorageStationComponent* ChangedStation);

	/** Validated payload staged before activation and retained only for the active presentation context. */
	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageScreenPayload> BaseStorageScreenPayload = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PlayerInventory = nullptr;

	/** Validated for payload coherence; this first single-pane slice does not render the armory yet. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> ArmoryInventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageComponent> BaseStorage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageStationComponent> StationComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageViewModel> BaseStorageViewModel = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryContainerHandle PlayerPaneContainerHandle;

	bool bBaseTerminalContextBound = false;
	uint32 BaseTerminalPresentationBindGeneration = 0;
	FUIActionBindingHandle DepositAllActionBinding;
	FUIActionBindingHandle InstallUpgradeActionBinding;
};
