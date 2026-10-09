#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "RpgCraftingSaveTypes.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgCraftingStationComponent.generated.h"

class UTexture2D;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;
class ARpgBaseCampActor;
class URpgCraftingRecipeDefinition;
class URpgCraftingRecipeSet;
struct FRpgInventoryBatchOperation;

/** Replicated state of a station's order. Pausing is a separate flag and freezes any of these states. */
UENUM(BlueprintType)
enum class ERpgCraftingOrderState : uint8
{
	/** A paid unit is being produced. */
	Running,

	/** The connected chests lack the next unit's materials; the station retries until they arrive. */
	WaitingForMaterials,

	/** The target chest has no room for the next unit's outputs; the station retries without consuming materials. */
	WaitingForSpace,

	/** The target chest is gone or no longer connected; the station retries until it returns or the target changes. */
	WaitingForTarget
};

/** One resource requirement consumed by a crafting station. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingResourceCost
{
	GENERATED_BODY()

	/** Item definition required by the recipe. Static recipe data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of items one unit consumes from the station's connected chests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/** One item stack created by a crafting station. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingOutputItem
{
	GENERATED_BODY()

	/** Item definition produced by the recipe. Instance data is created through the inventory manager. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of items one unit produces. Stackable definitions merge in the target chest; itemized ones are single pieces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/** Server-only credit for materials paid into the unit in progress, refunded when the order stops. */
USTRUCT()
struct SURVIVALRPG_API FRpgCraftingRefundEntry
{
	GENERATED_BODY()

	UPROPERTY(NotReplicated)
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	UPROPERTY(NotReplicated)
	int32 Count = 0;

	UPROPERTY(NotReplicated)
	TObjectPtr<URpgInventoryManagerComponent> Inventory = nullptr;

	/** Stable original chest, persisted independently of actor lifetime. */
	UPROPERTY(NotReplicated)
	FName InventoryId;
};

/**
 * Replicated read model of a station's single order: one recipe produced unit by unit until QuantityTotal. Every unit
 * takes its materials from the connected chests when it starts and delivers its outputs into the target chest.
 * Server-authoritative; clients only display it.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingOrder
{
	GENERATED_BODY()

	/** Stable id used by stop and target commands; invalid while the station is idle. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	FGuid OrderId;

	/** Static recipe data produced by this order. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	TObjectPtr<URpgCraftingRecipeDefinition> Recipe = nullptr;

	/**
	 * Persistent container id of the connected chest receiving the outputs. None stores automatically: each unit goes
	 * into the first chest of GetOutputTargets that has room.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	FName TargetContainerId;

	/** Units requested by the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	int32 QuantityTotal = 0;

	/** Units whose outputs were committed to the target chest. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	int32 QuantityCompleted = 0;

	/** What the order is doing or waiting for. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	ERpgCraftingOrderState State = ERpgCraftingOrderState::Running;

	/** Player pause; freezes the unit timer and every retry. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	bool bPaused = false;

	/** True while the unit in progress has consumed its materials and holds their credits. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	bool bUnitPaid = false;

	/** Server world time when the current unit started. UI uses it for progress display. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	float UnitStartServerTime = 0.0f;

	/** Server world time when the current unit finishes. UI uses it for progress display. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	float UnitFinishServerTime = 0.0f;

	/** Seconds left on the paid unit while paused or not yet resumed after a restore. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Order")
	float PausedRemainingTime = 0.0f;

	/** Server-only credits of the paid unit. */
	UPROPERTY(NotReplicated)
	TArray<FRpgCraftingRefundEntry> UnitCredits;

	/** True while the station has an order. */
	bool IsActive() const { return OrderId.IsValid(); }
};

/** Designer texts that name a station's units and actions in the crafting screen. Static, UI read-only. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingStationPresentation
{
	GENERATED_BODY()

	/** One unit of work, such as "piece" or "run". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Display")
	FText UnitSingular;

	/** Several units of work, such as "pieces" or "runs". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Display")
	FText UnitPlural;

	/** Main action label, such as "Start crafting" or "Start smelting". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Display")
	FText StartActionText;

	/** Label of the order strip, such as "Crafting" or "Smelting". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Display")
	FText OrderLabel;
};

/** GameplayMessage payload for order, pause and progress changes. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingStationChangeMessage
{
	GENERATED_BODY()

	/** Crafting station component whose replicated order changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UActorComponent> Station = nullptr;

	/** Order that changed or finished, or invalid when the whole station state refreshed. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	FGuid OrderId;

	/** Current state of the order, if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	ERpgCraftingOrderState OrderState = ERpgCraftingOrderState::Running;

	/** True when the order completed or stopped with this change. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	bool bOrderFinished = false;

	/** True when the pause flag changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	bool bPauseStateChanged = false;
};

/**
 * Crafting station: offers recipes and runs one order at a time in the background. Each unit takes its materials from
 * the connected chests (the station's base, or chests within StorageSearchRadius outside a base) when it starts and
 * delivers its outputs into the order's target chest. Missing materials, room or target make the order wait and retry.
 */
UCLASS(Blueprintable, ClassGroup = (Crafting), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgCraftingStationComponent : public UActorComponent, public IInteractableTarget
{
	GENERATED_BODY()

public:
	explicit URpgCraftingStationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;

	/** Stable authored identity, with the persistent container ID or level actor name as fallback. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Persistence")
	FName GetPersistentStationId() const;

	/** Captures the order with relative time only; must share the world inventory snapshot. */
	FRpgCraftingStationSaveData ExportCraftingState() const;

	/** Restores a validated order without advancing production. */
	bool RestoreCraftingState(const FRpgCraftingStationSaveData& SaveData);

	/** Starts restored timers after all world inventories have been restored. */
	void ResumeRestoredCrafting();

	/** Server lifecycle gate: the station cannot act until its saved order is restored. */
	bool IsPersistenceRestorePending() const { return bPersistenceRestorePending; }

	/** Authority-only persistence seam; GameMode releases this after the world restore. */
	void SetPersistenceRestorePending(bool bPending);

	/** Resolves only the requesting player's inventory; never searches other players. */
	static URpgInventoryManagerComponent* FindRequestingPlayerInventory(const AActor* RequestingActor);

	/** Prepares exact ordinary-material debits, the requester's own inventory first; does not mutate inventory. Used by construction. */
	static bool BuildResourceConsumptionPlan(AActor* RequestingActor,
		const TArray<URpgInventoryManagerComponent*>& StorageInventories,
		const TArray<FRpgCraftingResourceCost>& RequiredItems, int32 Quantity,
		TArray<FRpgInventoryBatchOperation>& OutOperations,
		TArray<FRpgCraftingRefundEntry>& OutRefundEntries);

	/** Prepares exact debits from shared chests only, in the given order; player inventories are never sources. */
	static bool BuildStorageConsumptionPlan(const TArray<URpgInventoryManagerComponent*>& StorageInventories,
		const TArray<FRpgCraftingResourceCost>& RequiredItems, int32 Quantity,
		TArray<FRpgInventoryBatchOperation>& OutOperations,
		TArray<FRpgCraftingRefundEntry>& OutRefundEntries);

	/** Chests this station takes materials from and may deliver into, sorted by persistent id. Works on clients for display. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Storage")
	TArray<URpgInventoryManagerComponent*> GetConnectedStorageInventories() const;

	/** Connected chest inventory with this persistent container id, or null. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Storage")
	URpgInventoryManagerComponent* FindConnectedStorageInventory(FName ContainerId) const;

	/** Persistent container id of a chest inventory, or None for any other inventory. */
	static FName GetStorageContainerId(const URpgInventoryManagerComponent* Inventory);

	/**
	 * Chests that may receive the recipe's outputs, in delivery order. A named target yields just that connected chest.
	 * None yields the automatic order of RankAutomaticOutputTargets. Works on clients for display.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Storage")
	TArray<URpgInventoryManagerComponent*> GetOutputTargets(const URpgCraftingRecipeDefinition* RecipeDefinition, FName TargetContainerId) const;

	/**
	 * Automatic delivery order for an output among the given chests. Chests assigned to that exact item come first,
	 * then chests assigned to its category, then unassigned chests already holding it, then other unassigned chests.
	 * Chests whose assignments do not name the output are left out, even when they already hold some of it, so the
	 * station never fills a chest meant for something else.
	 */
	static TArray<URpgInventoryManagerComponent*> RankAutomaticOutputTargets(const TArray<URpgInventoryManagerComponent*>& StorageInventories,
		TSubclassOf<URpgInventoryItemDefinition> OutputDefinition);

	/** Count of one definition in the given chests that orders may consume. */
	static int32 CountStorageResource(const TArray<URpgInventoryManagerComponent*>& StorageInventories,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);

	/** Units of the recipe the given chests can pay for; MaxUnitsWithoutCosts for a recipe without costs. */
	static int32 CountAffordableUnits(const TArray<URpgInventoryManagerComponent*>& StorageInventories,
		const URpgCraftingRecipeDefinition* RecipeDefinition, int32 MaxUnitsWithoutCosts);

	/** Runtime-links this placed or spawned station to a base camp. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Base Storage")
	void SetLinkedBaseCamp(ARpgBaseCampActor* NewBaseCamp);

	/** Returns the base camp whose chests this station uses, if it stands inside one. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Base Storage")
	ARpgBaseCampActor* GetLinkedBaseCamp() const { return ResolveSpatialBaseCamp(); }

	/** Semantic station tags used by recipe filters, such as Crafting.Station.Smelter. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Recipes", meta = (Categories = "Crafting.Station"))
	FGameplayTagContainer GetStationTags() const { return StationTags; }

	/** Player-facing station name for crafting screens; empty when the station class did not author one. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Display")
	FText GetStationDisplayName() const { return StationDisplayName; }

	/** Optional station icon for crafting screen headers. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Display")
	TSoftObjectPtr<UTexture2D> GetStationIcon() const { return StationIcon; }

	/** Designer texts naming this station's units and actions. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Display")
	FRpgCraftingStationPresentation GetStationPresentation() const { return Presentation; }

	/** Outside-base chest radius in centimeters; a containing base overrides it. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Storage")
	float GetStorageSearchRadius() const { return StorageSearchRadius; }

	/** Largest quantity one order may request. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Order")
	int32 GetMaxOrderQuantity() const { return FMath::Max(1, MaxOrderQuantity); }

	/** Returns recipes from the configured set that match this station's tags and base unlock state. Globally locked recipes may still be returned for UI display. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Recipes")
	TArray<URpgCraftingRecipeDefinition*> GetAvailableRecipes() const;

	/** Returns true when the recipe is globally unlocked or marked unlocked by default. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Recipes")
	bool IsRecipeUnlocked(const URpgCraftingRecipeDefinition* RecipeDefinition) const;

	/** Count of one definition in the connected chests that orders may consume. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Storage")
	int32 GetAvailableResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Units the connected chests can pay for right now; MaxOrderQuantity for a recipe without costs. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Recipes")
	int32 GetAffordableUnitCount(const URpgCraftingRecipeDefinition* RecipeDefinition) const;

	/**
	 * Pre-check shared by UI and server: access, offered and unlocked recipe, idle station, quantity range, at least one
	 * output target and the first unit's materials. TargetContainerId None stores automatically. Room in the targets is
	 * checked by the server's dry run on start.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Order")
	bool CanStartCraftingOrder(AActor* RequestingActor, const URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity, FName TargetContainerId) const;

	/**
	 * Starts an order. The first unit must be payable and deliverable now: it consumes its materials at once, later units
	 * when they start. Server-authoritative.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Order")
	bool StartCraftingOrder(AActor* RequestingActor, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity, FName TargetContainerId);

	/**
	 * Stops the order's remaining units. The paid unit's materials go back to the chests; when they fit nowhere, the
	 * order instead ends after that unit. Server-authoritative.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Order")
	bool StopCraftingOrder(AActor* RequestingActor, FGuid OrderId);

	/**
	 * Changes the chest receiving the order's outputs; None switches to automatic storing. A waiting order retries at
	 * once. Server-authoritative.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Order")
	bool SetCraftingOrderTarget(AActor* RequestingActor, FGuid OrderId, FName TargetContainerId);

	/** Pauses the order, freezing the unit timer and every retry. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Order")
	bool PauseCraftingStation(AActor* RequestingActor);

	/** Resumes a paused order. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Order")
	bool ResumeCraftingStation(AActor* RequestingActor);

	/** True while the station has an order. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Order")
	bool HasCraftingOrder() const { return CurrentOrder.IsActive(); }

	/** The station's replicated order; OrderId is invalid while idle. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Order")
	FRpgCraftingOrder GetCraftingOrder() const { return CurrentOrder; }

	/** Native read access to the replicated order without copying. */
	const FRpgCraftingOrder& GetCurrentOrder() const { return CurrentOrder; }

	/** True while the order is paused by a server-authoritative action. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Order")
	bool IsCraftingPaused() const { return CurrentOrder.IsActive() && CurrentOrder.bPaused; }

	/** Returns true when the requesting actor stands close enough to use this station. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	bool CanActorAccess(const AActor* RequestingActor) const;

protected:
	/** Stable unique identity authored on placed stations; built stations inherit their container identity. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Persistence")
	FName PersistentStationId;

	/** Interaction option shown by the Lyra-style interaction scan when this station can open its crafting UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Interaction")
	FInteractionOption OpenCraftingOption;

	/** Station identity tags used by recipe definitions to decide where they can be crafted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Recipes", meta = (Categories = "Crafting.Station"))
	FGameplayTagContainer StationTags;

	/**
	 * Player-facing name shown in the crafting screen header, such as "Workbench" or "Kiln".
	 * Designer-authored static data on the station class or placed instance; not replicated, UI read-only.
	 * The crafting view model falls back to a generic title when this is empty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Display")
	FText StationDisplayName;

	/** Optional icon shown next to StationDisplayName in the crafting screen header. Designer-authored, UI read-only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Display", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> StationIcon;

	/** Names of this station's units and actions, such as "run" and "Start smelting" for a kiln. Empty texts fall back to generic ones. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Display")
	FRpgCraftingStationPresentation Presentation;

	/** Data-driven recipe list offered by this station. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Recipes")
	TObjectPtr<URpgCraftingRecipeSet> AvailableRecipeSet;

	/** Outside-base radius in centimeters for connected chests. A containing base overrides this radius; zero connects no outside chests. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1", Units = "cm"))
	float StorageSearchRadius = 1200.0f;

	/** Maximum direct distance in centimeters for using this station. Zero or below allows access at any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/** Server-resolved spatial base reference for presentation and existing Blueprint callers; not a material-count source. */
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category = "Crafting|Base Storage")
	TObjectPtr<ARpgBaseCampActor> LinkedBaseCamp;

	/** Largest quantity one order may request. Designer tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Order", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxOrderQuantity = 99;

	/** Seconds between retries while the order waits for materials, room or its target. Server tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Order", meta = (ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float WaitingRetryInterval = 1.0f;

	/** Replicated order; its unit credits stay on the server. */
	UPROPERTY(ReplicatedUsing = OnRep_CraftingState, BlueprintReadOnly, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	FRpgCraftingOrder CurrentOrder;

	/** Lightweight replicated pulse that wakes clients even when the order ends. */
	UPROPERTY(ReplicatedUsing = OnRep_CraftingState, BlueprintReadOnly, Category = "Crafting|Order", meta = (AllowPrivateAccess = "true"))
	int32 CraftingStateRevision = 0;

private:
	UFUNCTION()
	void OnRep_CraftingState();

	bool IsRecipeOfferedByStation(const URpgCraftingRecipeDefinition* RecipeDefinition) const;
	ARpgBaseCampActor* ResolveSpatialBaseCamp() const;
	TFunction<bool()> MakeContextRevalidator(AActor* RequestingActor, FGuid ExpectedOrderId = FGuid()) const;
	bool BuildUnitOutputOperations(const URpgCraftingRecipeDefinition* RecipeDefinition, URpgInventoryManagerComponent* Target,
		TArray<FRpgInventoryBatchOperation>& InOutOperations) const;
	bool PlanUnitStart(const URpgCraftingRecipeDefinition* RecipeDefinition, const TArray<URpgInventoryManagerComponent*>& Targets,
		TArray<FRpgInventoryBatchOperation>& OutConsumption, TArray<FRpgCraftingRefundEntry>& OutCredits, ERpgCraftingOrderState& OutBlockedState) const;
	bool ApplyStationBatch(const TArray<FRpgInventoryBatchOperation>& Operations, TFunction<void()> CommitSideEffects, TFunction<bool()> Revalidate);
	bool RefundUnitCredits(TFunction<void()> CommitSideEffects);
	void TryStartNextUnit();
	void StartUnitTimer(float RemainingDuration);
	void CompleteActiveUnit();
	void CompleteActiveUnitInternal();
	void RetryWaitingOrder();
	void ContinueOrderInternal();
	void EnterWaitingState(ERpgCraftingOrderState WaitingState);
	void ClearOrderTimers();
	float GetServerWorldTimeSeconds() const;
	float GetRecipeCraftTime(const URpgCraftingRecipeDefinition* RecipeDefinition) const;

	void MarkCraftingStateDirty(bool bPauseStateChanged = false, FGuid FinishedOrderId = FGuid());

private:
	// Covers preparation, publication and synchronous observers; nested public commands cannot invalidate paid units.
	bool bMutationInProgress = false;
	bool bPersistenceRestorePending = false;
	FTimerHandle CraftingTimerHandle;
	FTimerHandle RetryTimerHandle;
};
