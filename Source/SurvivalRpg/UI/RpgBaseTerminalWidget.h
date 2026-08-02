#pragma once

#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "SurvivalRpg/Base/RpgBaseStorageTransactionTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"
#include "TimerManager.h"
#include "Types/SlateEnums.h"

#include "RpgBaseTerminalWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class URpgBaseResourceListWidget;
class URpgBaseStorageComponent;
class URpgBaseStorageStationComponent;
class URpgBaseStorageUpgradeDefinition;
class URpgBaseStorageViewModel;
class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgInventorySpatialPaneWidget;
class URpgInventorySpatialGridWidget;
class URpgInventoryUiActionComponent;
class URpgPlayerInventoryPaneWidget;
class UWidget;

/** Stable terminal domains. Domain state is presentation-only; inventories remain authoritative components. */
UENUM(BlueprintType)
enum class ERpgBaseTerminalDomain : uint8
{
	/** Shared definition-count base resource pool. */
	Materials,

	/** Spatial base armory inventory supplied by the validated terminal payload. */
	Armory,

	/** Owner-relevant 4x5 personal storage for the current base and profile. */
	Personal,

	/** Shared Rift containment, shown as a locked progression card before physical installation. */
	Rift
};

/** Convenience quantity choices resolved locally against the latest replicated snapshot. */
UENUM(BlueprintType)
enum class ERpgBaseTerminalQuantityPreset : uint8
{
	One,
	Ten,
	Max,
	Custom
};

/** Direction of a prepared base-resource quantity request. */
UENUM(BlueprintType)
enum class ERpgBaseTerminalQuantityDirection : uint8
{
	Deposit,
	Withdraw
};

/** Presentation severity for terminal-local feedback. */
UENUM(BlueprintType)
enum class ERpgBaseTerminalFeedbackSeverity : uint8
{
	Info,
	Success,
	Warning,
	Error
};

/** Stable reason code for terminal-local feedback; it never implies authoritative completion. */
UENUM(BlueprintType)
enum class ERpgBaseTerminalFeedbackCode : uint8
{
	None,
	RequestSubmitted,
	InvalidContext,
	InvalidQuantity,
	NoAvailableQuantity,
	DomainUnavailable,
	AccessLost,
	/** Server committed the complete requested mutation. */
	AuthoritativeSuccess,
	/** Server committed only the reported safe subset, usually due to capacity. */
	AuthoritativePartial,
	/** Range, session membership, privacy, or owner permission failed. */
	NoAccess,
	/** The stable item id no longer resolves in the expected inventory. */
	MissingItem,
	/** Network or inventory revision no longer matches the displayed snapshot. */
	Stale,
	/** The selected logical domain has no remaining capacity. */
	CapacityFull,
	/** The target spatial inventory cannot place the requested result. */
	NoPlacement,
	/** Item storage mode or runtime-state contract rejects this route. */
	UnsupportedMode,
	/** Required physical base capability is not installed. */
	CapabilityLocked,
	/** Required shared world knowledge has not been discovered. */
	KnowledgeMissing,
	/** Authoritative resource costs are unavailable. */
	MissingCosts,
	/** Rift extraction would exceed the inclusive 100-point ceiling. */
	StrainBlocked,
	/** Concurrent or current gameplay state conflicts with the request. */
	Conflict,
	/** A destructive Rift action still requires explicit user confirmation. */
	ConfirmationRequired,
	/** Invalid request or unconfirmed rollback failure reported by the server. */
	ServerRejected
};

/** UI-local record emitted when a quantity request has passed terminal preflight and is dispatched. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseTerminalQuantityRequest
{
	GENERATED_BODY()

	/** Deposit or withdraw path selected by the UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Quantity")
	ERpgBaseTerminalQuantityDirection Direction = ERpgBaseTerminalQuantityDirection::Withdraw;

	/** Preset selected by the player before local clamping. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Quantity")
	ERpgBaseTerminalQuantityPreset Preset = ERpgBaseTerminalQuantityPreset::One;

	/** Material definition affected by the request. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Quantity")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Exact player-owned item instance for deposit requests; null for withdrawals. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Quantity")
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Locally resolved positive quantity sent for server validation. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Quantity")
	int32 Quantity = 0;
};

/** Current terminal-local feedback. Gameplay success still comes from authoritative replicated state/messages. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseTerminalLocalFeedback
{
	GENERATED_BODY()

	/** Stable feedback reason suitable for Blueprint styling. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Feedback")
	ERpgBaseTerminalFeedbackCode Code = ERpgBaseTerminalFeedbackCode::None;

	/** Cosmetic severity suitable for color/icon selection. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Feedback")
	ERpgBaseTerminalFeedbackSeverity Severity = ERpgBaseTerminalFeedbackSeverity::Info;

	/** Localized player-facing explanation. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Feedback")
	FText Message;

	/** Domain active when the feedback was produced. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Feedback")
	ERpgBaseTerminalDomain Domain = ERpgBaseTerminalDomain::Materials;

	/** Quantity associated with a prepared request, or zero when not applicable. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Feedback")
	int32 RequestedQuantity = 0;

	/** True only for a dispatched request whose authoritative result has not been inferred locally. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage|Terminal|Feedback")
	bool bAwaitingAuthoritativeUpdate = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgBaseTerminalDomainChanged,
	ERpgBaseTerminalDomain,
	NewDomain);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgBaseTerminalLocalFeedbackChanged,
	FRpgBaseTerminalLocalFeedback,
	Feedback);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgBaseTerminalQuantityRequestPrepared,
	FRpgBaseTerminalQuantityRequest,
	Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgBaseTerminalCommandResultReceived,
	FRpgBaseStorageCommandResult,
	Result);

/**
 * CommonUI presenter for one base terminal or filtered base-storage station.
 *
 * The screen owns only pooled interaction and read-only presentation state. Authoritative inventory/resource
 * mutations continue through URpgInventoryUiActionComponent and the replicated storage/inventory components.
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

	/** Legacy single-container pane currently authored by the shipping terminal Blueprint, if present. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal")
	URpgInventorySpatialPaneWidget* GetPlayerInventoryPane() const;

	/** Complete reusable player-inventory pane when the terminal Blueprint has migrated to it. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal")
	URpgPlayerInventoryPaneWidget* GetReusablePlayerInventoryPane() const;

	/** Exact widget bound under PlayerInventoryPane during the compatibility migration. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal")
	UWidget* GetPlayerInventoryPaneWidget() const { return PlayerInventoryPane.Get(); }

	/** Exact canonical player-layout container shown by the legacy PlayerInventoryPane. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal")
	FRpgInventoryContainerHandle GetPlayerPaneContainerHandle() const
	{
		return PlayerPaneContainerHandle;
	}

	/** Current stable secondary domain selected by local UI state. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Domains")
	ERpgBaseTerminalDomain GetActiveDomain() const { return ActiveDomain; }

	/** Returns the authoritative inventory component associated with one spatial domain, if supplied. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Domains")
	URpgInventoryManagerComponent* GetDomainInventory(ERpgBaseTerminalDomain Domain) const;

	/** Returns domains whose gameplay source exists, independent of whether the current Blueprint authors a pane. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Domains")
	TArray<ERpgBaseTerminalDomain> GetAvailableDomains() const;

	/** True when the domain has a valid gameplay source in the active terminal context. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Domains")
	bool IsDomainAvailable(ERpgBaseTerminalDomain Domain) const;

	/** True when both the domain source and an authored presenter exist. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Domains")
	bool CanPresentDomain(ERpgBaseTerminalDomain Domain) const;

	/** Switches local presentation to one authored, available domain without mutating gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Domains")
	bool SetActiveDomain(ERpgBaseTerminalDomain NewDomain);

	/**
	 * Supplies optional future Personal/Rift inventory payload data without creating a manager.
	 * Call after the base-terminal payload is staged; deactivation clears these UI-only references.
	 */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Domains")
	bool SetOptionalDomainInventory(
		ERpgBaseTerminalDomain Domain,
		URpgInventoryManagerComponent* Inventory);

	/** Current local feedback model; it does not claim authoritative gameplay completion. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Feedback")
	FRpgBaseTerminalLocalFeedback GetLocalFeedback() const { return LocalFeedback; }

	/** Last authoritative result, including Smart Deposit per-stack outcomes for result-list presentation. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Feedback")
	FRpgBaseStorageCommandResult GetLastCommandResult() const
	{
		return LastCommandResult;
	}

	/** Clears the terminal-local feedback model. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Feedback")
	void ClearLocalFeedback();

	/** Resolves a withdraw preset against the latest replicated resource count. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Quantity")
	int32 ResolveWithdrawQuantity(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		ERpgBaseTerminalQuantityPreset Preset,
		int32 CustomQuantity = 0) const;

	/** Dispatches one server-validated withdraw request after local quantity preflight. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Quantity")
	bool RequestWithdrawResource(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		ERpgBaseTerminalQuantityPreset Preset,
		int32 CustomQuantity = 0);

	/** Resolves a deposit preset against the player's current stack and the base's free capacity. */
	UFUNCTION(BlueprintPure, Category = "Base Storage|Terminal|Quantity")
	int32 ResolveDepositQuantity(
		URpgInventoryItemInstance* Item,
		ERpgBaseTerminalQuantityPreset Preset,
		int32 CustomQuantity = 0) const;

	/** Dispatches one server-validated material-stack deposit after local quantity preflight. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Quantity")
	bool RequestDepositMaterialStack(
		URpgInventoryItemInstance* Item,
		ERpgBaseTerminalQuantityPreset Preset,
		int32 CustomQuantity = 0);

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

	/** Requests safe owner-only removal of an installed upgrade and grants only its authored refunds. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Upgrades")
	bool RequestDecommissionUpgrade(URpgBaseStorageUpgradeDefinition* Upgrade);

	/** Requests member-authorized stabilization of one exact contained Rift item. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Rift")
	bool RequestStabilizeRiftItem(URpgInventoryItemInstance* Item);

	/** Requests owner-only destructive extraction; bConfirmed must come from an explicit preview confirmation. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Rift")
	bool RequestExtractRiftItem(URpgInventoryItemInstance* Item, bool bConfirmed);

	/** Requests one authored material-cost cleanse of the base's current Rift strain. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Terminal|Rift")
	bool RequestCleanseRiftStrain();

	/** Fired after the active presentation domain changes. */
	UPROPERTY(BlueprintAssignable, Category = "Base Storage|Terminal|Domains")
	FRpgBaseTerminalDomainChanged OnActiveDomainChanged;

	/** Fired whenever local terminal feedback changes. */
	UPROPERTY(BlueprintAssignable, Category = "Base Storage|Terminal|Feedback")
	FRpgBaseTerminalLocalFeedbackChanged OnLocalFeedbackChanged;

	/** Fired when a positive quantity has been prepared and dispatched for server validation. */
	UPROPERTY(BlueprintAssignable, Category = "Base Storage|Terminal|Quantity")
	FRpgBaseTerminalQuantityRequestPrepared OnQuantityRequestPrepared;

	/** Fired for the owning client's correlated authoritative result, including detailed Smart Deposit rows. */
	UPROPERTY(BlueprintAssignable, Category = "Base Storage|Terminal|Feedback")
	FRpgBaseTerminalCommandResultReceived OnAuthoritativeCommandResult;

	/** Lets the authored terminal swap between its Rift upgrade card and concrete sealed-slot presentation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Base Storage|Terminal|Rift", meta = (DisplayName = "On Rift Domain Lock State Changed"))
	void BP_OnRiftDomainLockStateChanged(bool bKnowledgeDiscovered, bool bContainmentInstalled);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeHandlePreviousPanelAction() override;
	virtual bool NativeHandleNextPanelAction() override;
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
	virtual FName GetInitialInventoryNavigationPanelId() const override;
	virtual void AppendInventoryScreenSpatialGrids(
		TArray<URpgInventorySpatialGridWidget*>& OutGrids) const override;
	virtual bool RouteInventoryPayloadToScreenSpecificTarget(
		const FRpgInventoryDragPayload& Payload,
		FVector2D GhostCenterScreenPosition,
		bool bCommit,
		bool& bOutTargetAddressed) override;
	virtual void ClearInventoryScreenSpecificDragPreviews() override;
	virtual bool UpdateInventoryScreenSpecificControllerDragVisual(
		const FRpgInventoryDragPayload& Payload) override;
	virtual void RefreshInventoryScreenSpecificInteractionPresentation(
		ERpgInventoryInteractionPreviewState PreviewState,
		bool bHasPayload,
		bool bPendingRequest) override;

	/**
	 * Compatibility binding for either the legacy spatial pane or the complete reusable player pane.
	 * Optional metadata lets the current Blueprint migrate without a native-class load failure.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PlayerInventoryPane = nullptr;

	/** Required typed resource list fed by the stable screen-owned base-storage VM. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgBaseResourceListWidget> BaseResourceList = nullptr;

	/** Optional authored root pane for the payload's base armory inventory. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialPaneWidget> ArmoryInventoryPane = nullptr;

	/** Optional future pane for a host-supplied personal storage inventory. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialPaneWidget> PersonalInventoryPane = nullptr;

	/** Optional future pane for a host-supplied rift storage inventory. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgInventorySpatialPaneWidget> RiftInventoryPane = nullptr;

	/** Optional stable Materials-domain tab button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> MaterialsDomainButton = nullptr;

	/** Optional stable Armory-domain tab button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ArmoryDomainButton = nullptr;

	/** Optional stable Personal-domain tab button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> PersonalDomainButton = nullptr;

	/** Optional stable Rift-domain tab button. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> RiftDomainButton = nullptr;

	/** Optional authored pointer button for the server-validated deposit-all convenience action. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> DepositAllButton = nullptr;

	/** Optional authored pointer button for FeaturedUpgrade. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> InstallUpgradeButton = nullptr;

	/** Optional local-only material-name search field. Text never leaves the owning client. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> StorageSearchBox = nullptr;

	/** Optional button that cycles broad local material categories without changing replicated order. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StorageCategoryFilterButton = nullptr;

	/** Optional label reflecting the active local material category filter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageCategoryFilterText = nullptr;

	/** Optional button that cycles useful local list-order presets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StorageSortButton = nullptr;

	/** Optional label reflecting the active local list-order preset. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageSortText = nullptr;

	/** Optional Materials-domain used/max capacity readout, including the local 80 percent warning state. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageCapacityText = nullptr;

	/** Optional effective Rift strain readout in the inclusive 0..100 range. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageStrainText = nullptr;

	/** Optional one-unit withdrawal control for the selected material row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StorageWithdrawOneButton = nullptr;

	/** Optional ten-unit withdrawal control for the selected material row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StorageWithdrawTenButton = nullptr;

	/** Optional maximum withdrawal control for the selected material row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StorageWithdrawMaxButton = nullptr;

	/** Optional positive custom withdrawal quantity; committing the text submits the selected material request. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> StorageWithdrawCustomInput = nullptr;

	/** Optional per-resource Smart Deposit result list populated only from owning-client authoritative feedback. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageSmartDepositResultText = nullptr;

	/** Optional localized terminal feedback text for submitted, partial, rejected, and successful commands. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StorageActionResultText = nullptr;

	/** Optional Rift knowledge/install/slot-state presentation. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RiftLockStateText = nullptr;

	/** Optional deterministic stabilization-cost presentation for the selected contained instance. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RiftActionCostText = nullptr;

	/** Optional deterministic extraction output and strain preview for the selected contained instance. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RiftActionPreviewText = nullptr;

	/** Optional authored container for selected Rift cost/output preview widgets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StorageRiftPreviewRow = nullptr;

	/** Optional authored container for Rift transaction controls. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StorageRiftActionRow = nullptr;

	/** Optional member-authorized stabilization control for the selected exact Rift item. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> StabilizeRiftButton = nullptr;

	/** Optional owner-only preview/confirm extraction control for the selected exact Rift item. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExtractRiftButton = nullptr;

	/** Optional member-authorized authored-cost strain cleanse control. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CleanseRiftButton = nullptr;

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

	/** Seconds between local access/range checks while a terminal context is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Base Storage|Terminal|Access", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float AccessValidationInterval = 0.2f;

private:
	void ApplyBaseStorageScreenPayload(UObject* Payload);
	bool IsPayloadCoherent(const URpgBaseStorageScreenPayload* Payload) const;
	bool BindBaseTerminalContext();
	bool HasAutoDepositableLoot() const;
	bool IsRiftKnowledgeDiscovered() const;
	bool IsRiftContainmentInstalled() const;
	void SelectInitialDomain();
	void EnsureBaseStorageViewModel();
	void RegisterBaseTerminalActionBindings();
	void UnregisterBaseTerminalActionBindings();
	void RegisterBaseStorageFeedbackListener();
	void UnregisterBaseStorageFeedbackListener();
	void ResetBaseTerminalContext();
	void UpdateTerminalActionAvailability();
	void UpdateDomainPresentation();
	void UpdateDomainActionAvailability();
	void RefreshTerminalControlPresentation();
	void RefreshMaterialControlPresentation();
	void RefreshRiftControlPresentation();
	void RefreshAuthoritativeResultPresentation();
	bool CycleActiveDomain(int32 Direction);
	URpgInventoryItemInstance* ResolveSelectedRiftItem() const;
	int32 ReadCustomWithdrawQuantity() const;
	void BindAvailableDomainPanes();
	void ReleaseDomainPanes();
	void ConfigureQuickTransferRoutes();
	URpgInventorySpatialPaneWidget* GetDomainPane(ERpgBaseTerminalDomain Domain) const;
	FName GetDomainPanelId(ERpgBaseTerminalDomain Domain) const;
	int32 ResolvePresetQuantity(
		ERpgBaseTerminalQuantityPreset Preset,
		int32 CustomQuantity,
		int32 AvailableQuantity) const;
	void SetLocalFeedback(
		ERpgBaseTerminalFeedbackCode Code,
		ERpgBaseTerminalFeedbackSeverity Severity,
		FText Message,
		int32 RequestedQuantity = 0,
		bool bAwaitingAuthoritativeUpdate = false);
	void StartAccessValidation();
	void StopAccessValidation();
	void ValidateTerminalAccess();
	void HandlePlayerInventoryPaneNavigationPanelsChanged();
	void HandleBaseStorageCommandFeedback(
		FGameplayTag Channel,
		const FRpgBaseStorageCommandFeedbackMessage& Message);
	bool BuildStorageRequestContext(
		FRpgBaseStorageRequestContext& OutContext) const;
	URpgInventoryUiActionComponent* ResolveInventoryUiActionComponent() const;

	UFUNCTION()
	void HandleDepositAllClicked();

	UFUNCTION()
	void HandleInstallUpgradeClicked();

	UFUNCTION()
	void HandleMaterialsDomainClicked();

	UFUNCTION()
	void HandleArmoryDomainClicked();

	UFUNCTION()
	void HandlePersonalDomainClicked();

	UFUNCTION()
	void HandleRiftDomainClicked();

	UFUNCTION()
	void HandleStorageSearchTextChanged(const FText& SearchText);

	UFUNCTION()
	void HandleStorageCategoryFilterClicked();

	UFUNCTION()
	void HandleStorageSortClicked();

	UFUNCTION()
	void HandleWithdrawOneClicked();

	UFUNCTION()
	void HandleWithdrawTenClicked();

	UFUNCTION()
	void HandleWithdrawMaxClicked();

	UFUNCTION()
	void HandleCustomWithdrawCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleStabilizeRiftClicked();

	UFUNCTION()
	void HandleExtractRiftClicked();

	UFUNCTION()
	void HandleCleanseRiftClicked();

	UFUNCTION()
	void HandleBaseStorageResourcesChanged();

	UFUNCTION()
	void HandleActiveInventorySelectionChanged();

	void HandleMaterialSelectionChanged(UObject* SelectedItem);

	UFUNCTION()
	void HandleInstalledUpgradesChanged(URpgBaseStorageStationComponent* ChangedStation);

	/** Validated payload staged before activation and retained only for the active presentation context. */
	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageScreenPayload> BaseStorageScreenPayload = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PlayerInventory = nullptr;

	/** Payload-owned base armory; UI never creates or authoritatively mutates it. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> ArmoryInventory = nullptr;

	/** Optional host-supplied personal inventory retained only for the active terminal presentation. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> PersonalInventory = nullptr;

	/** Optional host-supplied rift inventory retained only for the active terminal presentation. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryManagerComponent> RiftInventory = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageComponent> BaseStorage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageStationComponent> StationComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgBaseStorageViewModel> BaseStorageViewModel = nullptr;

	UPROPERTY(Transient)
	FRpgInventoryContainerHandle PlayerPaneContainerHandle;

	/** Material definition selected in the local resource list; never replicated or used without server validation. */
	UPROPERTY(Transient)
	TSubclassOf<URpgInventoryItemDefinition> SelectedMaterialDefinition;

	/** Exact item whose server extraction preview may be confirmed by the next owner action. */
	UPROPERTY(Transient)
	FRpgInventoryItemId PendingExtractionConfirmationItemId;

	/** Exact item associated with the most recently submitted extraction-preview request. */
	UPROPERTY(Transient)
	FRpgInventoryItemId SubmittedExtractionPreviewItemId;

	/** Request correlation for SubmittedExtractionPreviewItemId so unrelated command feedback cannot arm extraction. */
	UPROPERTY(Transient)
	FGuid SubmittedExtractionPreviewRequestId;

	/** Current presentation-only domain; default Materials preserves the existing Blueprint behavior. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Base Storage|Terminal|Domains", meta = (AllowPrivateAccess = "true"))
	ERpgBaseTerminalDomain ActiveDomain = ERpgBaseTerminalDomain::Materials;

	/** Last successful local tab selection retained across pooled terminal opens. */
	UPROPERTY(Transient)
	ERpgBaseTerminalDomain LastUsedDomain = ERpgBaseTerminalDomain::Materials;

	/** Last local feedback retained until cleared or replaced; it is never replicated or saved. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Base Storage|Terminal|Feedback", meta = (AllowPrivateAccess = "true"))
	FRpgBaseTerminalLocalFeedback LocalFeedback;

	/** Latest owning-client command result; transient UI read model and never gameplay authority. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Base Storage|Terminal|Feedback", meta = (AllowPrivateAccess = "true"))
	FRpgBaseStorageCommandResult LastCommandResult;

	/** Latest server-confirmed storage revision used to chain local commands before property replication catches up. */
	UPROPERTY(Transient)
	int64 KnownNetworkRevision = INDEX_NONE;

	bool bBaseTerminalContextBound = false;
	uint32 BaseTerminalPresentationBindGeneration = 0;
	FUIActionBindingHandle DepositAllActionBinding;
	FUIActionBindingHandle InstallUpgradeActionBinding;
	FGameplayMessageListenerHandle BaseStorageCommandFeedbackHandle;
	FTimerHandle AccessValidationTimer;
};
