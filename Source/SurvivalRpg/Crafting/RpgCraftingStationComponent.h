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
class ARpgDroppedInventoryActor;
class URpgBaseStorageStationComponent;
class URpgBaseStorageComponent;
class URpgCraftingRecipeDefinition;
class URpgCraftingRecipeSet;
struct FRpgInventoryBatchOperation;

/** Resource source order used by a crafting station when a recipe consumes materials. */
UENUM(BlueprintType)
enum class ERpgCraftingResourceConsumeOrder : uint8
{
	/** Consume from linked base storage first, then player/allowed inventory sources. */
	BaseThenPlayer,

	/** Consume from player/allowed inventory sources first, then linked base storage. */
	PlayerThenBase,

	/** Only consume from linked base storage. */
	BaseOnly,

	/** Only consume from player/allowed inventory sources. */
	PlayerOnly
};

/** Replicated lifecycle state for one crafting station job. */
UENUM(BlueprintType)
enum class ERpgCraftingJobState : uint8
{
	/** Waiting until no earlier job is active. */
	Queued,

	/** Server timer is currently producing the next unit. */
	Active,

	/** Station-level pause froze this job's remaining time. */
	Paused,

	/** Output does not fit; the station retains the paid job and retries without dropping items. */
	BlockedOutput,

	/** Job has finished and is about to be removed from the queue. */
	Completed
};

/** One resource requirement consumed by a crafting station. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingResourceCost
{
	GENERATED_BODY()

	/** Item definition required by the recipe. Static recipe data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of items to consume across player inventory and linked storage. */
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

	/** Number of items produced. Stackable definitions may merge; equipment definitions create entries as needed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/** Server-only resource credit used to refund canceled crafting batches. */
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

	/** Stable original source, persisted independently of player connectivity or actor lifetime. */
	UPROPERTY(NotReplicated)
	FName InventoryId;

	UPROPERTY(NotReplicated)
	bool bRefundToBaseStorage = false;
};

/** Replicated read model for one active or queued crafting batch. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingJobEntry
{
	GENERATED_BODY()

	/** Stable id used by UI cancel commands and replication refreshes. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Jobs")
	FGuid JobId;

	/** Static recipe data processed by this job. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Jobs")
	TObjectPtr<URpgCraftingRecipeDefinition> Recipe = nullptr;

	/** Total number of recipe units requested by the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Jobs")
	int32 QuantityTotal = 0;

	/** Number of units already produced and output-handled by the server. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Jobs")
	int32 QuantityCompleted = 0;

	/** Current replicated lifecycle state. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Jobs")
	ERpgCraftingJobState State = ERpgCraftingJobState::Queued;

	/** Server world time when the current unit started. UI uses this for progress display. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Jobs")
	float StartServerTime = 0.0f;

	/** Server world time when the current unit should finish. UI uses this for progress display. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Jobs")
	float FinishServerTime = 0.0f;

	/** Remaining seconds captured when the station pauses this job. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting|Jobs")
	float PausedRemainingTime = 0.0f;

	/** Server-only credits for the not-yet-produced part of this batch. Used when canceling/refunding. */
	UPROPERTY(NotReplicated)
	TArray<FRpgCraftingRefundEntry> RefundEntries;
};

/** GameplayMessage payload for crafting queue, pause, and progress state changes. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCraftingStationChangeMessage
{
	GENERATED_BODY()

	/** Crafting station component whose replicated job state changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UActorComponent> Station = nullptr;

	/** Job id that changed, or invalid when the whole station state refreshed. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	FGuid JobId;

	/** Current state for the changed job, if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	ERpgCraftingJobState JobState = ERpgCraftingJobState::Queued;

	/** True when the station-level pause flag changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	bool bPauseStateChanged = false;
};

/**
 * V1 crafting station helper that consumes resources and stores outputs in a replicated inventory.
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

	/** Captures only relative production time; must share the world inventory snapshot. */
	FRpgCraftingStationSaveData ExportCraftingState() const;

	/** Restores validated settings and paid jobs without advancing production. */
	bool RestoreCraftingState(const FRpgCraftingStationSaveData& SaveData);

	/** Starts restored timers after all world inventories have been restored. */
	void ResumeRestoredCrafting();

	/** Resolves only the requesting player's inventory; never searches other players. */
	static URpgInventoryManagerComponent* FindRequestingPlayerInventory(const AActor* RequestingActor);

	/** Prepares exact ordinary-material debits, own inventory first; does not mutate inventory. */
	static bool BuildResourceConsumptionPlan(AActor* RequestingActor,
		const TArray<URpgInventoryManagerComponent*>& StorageInventories,
		const TArray<FRpgCraftingResourceCost>& RequiredItems, int32 Quantity,
		TArray<FRpgInventoryBatchOperation>& OutOperations,
		TArray<FRpgCraftingRefundEntry>& OutRefundEntries);

	/** Returns the requesting inventory first, followed by shared sources in this station's physical domain. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	TArray<URpgInventoryManagerComponent*> GetResourceInventories(AActor* RequestingActor) const;

	/** Runtime-links this placed or spawned station to a base camp. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Base Storage")
	void SetLinkedBaseCamp(ARpgBaseCampActor* NewBaseCamp);

	/** Returns the linked base camp this station may consume from and auto-deposit into. */
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

	/** Returns recipes from the configured set that match this station's tags and base unlock state. Globally locked recipes may still be returned for UI display. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Recipes")
	TArray<URpgCraftingRecipeDefinition*> GetAvailableRecipes() const;

	/** Returns true when the recipe is globally unlocked or marked unlocked by default. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Recipes")
	bool IsRecipeUnlocked(const URpgCraftingRecipeDefinition* RecipeDefinition) const;

	/** Returns true if this station can currently craft the recipe for the requesting actor. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Recipes")
	bool CanCraftRecipe(AActor* RequestingActor, const URpgCraftingRecipeDefinition* RecipeDefinition) const;

	/** Returns true if this station can enqueue this many recipe units for the requesting actor. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Recipes")
	bool CanCraftRecipeQuantity(AActor* RequestingActor, const URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity) const;

	/** Returns the maximum quantity currently accepted by authority after access, queue, output, unlock, and aggregated resource checks. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Recipes")
	int32 GetMaxCraftableQuantity(AActor* RequestingActor, const URpgCraftingRecipeDefinition* RecipeDefinition) const;

	/** Queues one or more recipe units. Resources are consumed immediately and refunded if the unfinished remainder is canceled. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Recipes")
	bool QueueCraftRecipe(AActor* RequestingActor, URpgCraftingRecipeDefinition* RecipeDefinition, int32 Quantity = 1);

	/** Backward-compatible one-unit craft path that now queues the recipe through the timed station pipeline. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Recipes")
	bool CraftRecipe(AActor* RequestingActor, URpgCraftingRecipeDefinition* RecipeDefinition);

	/** Cancels one active or queued job and refunds the unfinished resource credits. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Jobs")
	bool CancelCraftJob(AActor* RequestingActor, FGuid JobId);

	/** Pauses the whole station, freezing active progress and preventing queued jobs from starting. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Jobs")
	bool PauseCraftingStation(AActor* RequestingActor);

	/** Resumes the whole station and restarts the active or next queued job. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Jobs")
	bool ResumeCraftingStation(AActor* RequestingActor);

	/** Returns the current replicated queue, including the active job if present. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Jobs")
	TArray<FRpgCraftingJobEntry> GetCraftingJobs() const { return CraftingJobs; }

	/** Returns true and fills the active/paused job if one exists. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Jobs")
	bool GetActiveCraftingJob(FRpgCraftingJobEntry& OutJob) const;

	/** True when this station's queue is paused by a server-authoritative action. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Jobs")
	bool IsCraftingPaused() const { return bStationPaused; }

	/** Returns total available count across all resource inventories for one item definition. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	int32 GetAvailableResourceCount(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Consumes resources across player inventory and nearby/same-group storage after verifying the full cost is available. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	bool ConsumeResources(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems);

	/** Returns true when the requesting actor may use this station's output inventory. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	bool CanActorAccess(const AActor* RequestingActor) const;

	/** Replicated physical output tray; also a material source in the applicable storage domain. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	URpgInventoryManagerComponent* GetOutputInventory() const { return OutputInventoryComponent; }

	/** Server lifecycle gate: provisional BeginPlay contents cannot participate until the saved tray and queue are restored. */
	bool IsPersistenceRestorePending() const { return bPersistenceRestorePending; }

	/** Authority-only persistence seam; GameMode releases this after replacing synchronous authored seed grants. */
	void SetPersistenceRestorePending(bool bPending);

	/** Assigns the output inventory component, usually from a native or Blueprint crafting-station actor constructor. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Output")
	void SetOutputInventoryManager(URpgInventoryManagerComponent* InOutputInventory);

	/** Validates output definitions. A full tray blocks production while keeping the paid job intact. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	bool CanAcceptCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems) const;

	/** Atomically creates all recipe outputs in eligible chests or the tray. Failure creates no outputs. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Output")
	bool AddCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems);

	/** Atomically consumes costs and creates every output; failure changes no inventory. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	bool CraftItems(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems, const TArray<FRpgCraftingOutputItem>& OutputItems);

	/** Moves eligible ordinary outputs into assigned physical chests using the station domain. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Output")
	bool FlushOutputToBaseStorage();

	/** Physical chest output routing is available from the first chest without an upgrade gate. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	bool HasCraftingOutputAutoDepositAccess() const;

	/** Saved station preference: false keeps outputs in the tray, true uses assignment routing. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Output")
	bool IsCraftingOutputAutoDepositEnabled() const { return bAutoDepositCraftingOutputsEnabled; }

	/** Enables or disables output auto-deposit on this station. Server-authoritative and access-checked. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Output")
	bool SetCraftingOutputAutoDepositEnabled(AActor* RequestingActor, bool bEnabled);

	/** Returns true when this station should push crafted outputs into the linked base before using output slots. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	bool ShouldAutoDepositCraftingOutputs() const;

	/** Returns the storage station component that supplies output auto-deposit upgrade tags, if any. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	URpgBaseStorageStationComponent* GetOutputAutoDepositUpgradeProvider() const;

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

	/** Data-driven recipe list offered by this station. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Recipes")
	TObjectPtr<URpgCraftingRecipeSet> AvailableRecipeSet;

	/** Legacy asset field. Physical sources are selected by the containing base or outside radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FName StorageGroupId;

	/** Legacy asset field; physical crafting always resolves eligible shared chests and station trays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	bool bUseNearbyCraftingContainers = true;

	/** Outside-base radius in centimeters. A containing base overrides this radius; zero grants no outside sources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1", UIMin = "1", Units = "cm"))
	float StorageSearchRadius = 1200.0f;

	/** Maximum direct distance in centimeters for taking outputs from this station. Zero or below allows access at any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/** Server-resolved spatial base reference for presentation and existing Blueprint callers; not a material-count source. */
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category = "Crafting|Base Storage")
	TObjectPtr<ARpgBaseCampActor> LinkedBaseCamp;

	/** Legacy asset field; physical crafting uses spatial storage membership regardless of this value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Base Storage")
	bool bUseLinkedBaseStorage = true;

	/** Legacy content compatibility only. Physical crafting always consumes the requesting player first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Base Storage")
	ERpgCraftingResourceConsumeOrder ResourceConsumeOrder = ERpgCraftingResourceConsumeOrder::PlayerThenBase;

	/** Legacy upgrade-provider reference retained for existing assets; physical output routing uses the saved toggle. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Output")
	TObjectPtr<AActor> OutputAutoDepositUpgradeProviderActor;

	/** Legacy provider component; it does not gate physical output routing. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Output")
	TObjectPtr<URpgBaseStorageStationComponent> OutputAutoDepositUpgradeProvider;

	/** Legacy prototype override, ignored by physical output routing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output")
	bool bAlwaysAutoDepositCraftingOutputs = false;

	/** Saved, replicated preference. False retains outputs in the tray; true uses physical assignment routing. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_CraftingState, BlueprintReadOnly, Category = "Crafting|Output")
	bool bAutoDepositCraftingOutputsEnabled = false;

	/** Legacy armory preference, ignored by physical output routing. Concrete outputs retain their item state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output")
	bool bAutoDepositInstanceOutputsToArmory = true;

	/** Physical output tray used when automatic deposit is disabled or eligible chests cannot accept all outputs. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Output")
	TObjectPtr<URpgInventoryManagerComponent> OutputInventoryComponent;

	/**
	 * Uses the output inventory's authored spatial grid as its sole capacity contract.
	 * Keep enabled for Tarkov-style output trays; disable only for a deliberate legacy entry-count cap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output")
	bool bUseSpatialOutputCapacity = true;

	/** Legacy top-level entry cap used only when bUseSpatialOutputCapacity is disabled. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Crafting|Output",
		meta = (
			EditCondition = "!bUseSpatialOutputCapacity",
			EditConditionHides,
			ClampMin = "0",
			UIMin = "0"))
	int32 OutputSlotCount = 4;

	/** Maximum number of active plus queued jobs this station accepts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Jobs", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxQueuedJobs = 5;

	/** Fallback max quantity for recipes that have no resource costs. Prevents infinite Max buttons in UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Jobs", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxFreeRecipeCraftQuantity = 99;

	/** Pickup actor used when outputs or refunds cannot be stored in inventories/base storage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output")
	TSubclassOf<ARpgDroppedInventoryActor> DroppedOutputActorClass;

	/** Radius in centimeters used to merge new stackable world outputs into existing nearby drops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float OutputDropMergeRadius = 250.0f;

	/** Replicated station-level pause flag. Clients use it for UI; the server owns all timer behavior. */
	UPROPERTY(ReplicatedUsing = OnRep_CraftingState, BlueprintReadOnly, Category = "Crafting|Jobs", meta = (AllowPrivateAccess = "true"))
	bool bStationPaused = false;

	/** Replicated active and queued jobs. Server-only refund credits are kept inside each entry and are not replicated. */
	UPROPERTY(ReplicatedUsing = OnRep_CraftingState, BlueprintReadOnly, Category = "Crafting|Jobs", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgCraftingJobEntry> CraftingJobs;

	/** Lightweight replicated pulse that wakes clients even when the queue becomes empty after the final job. */
	UPROPERTY(ReplicatedUsing = OnRep_CraftingState, BlueprintReadOnly, Category = "Crafting|Jobs", meta = (AllowPrivateAccess = "true"))
	int32 CraftingStateRevision = 0;

private:
	UFUNCTION()
	void OnRep_CraftingState();

	bool IsRecipeOfferedByStation(const URpgCraftingRecipeDefinition* RecipeDefinition) const;
	URpgBaseStorageComponent* GetLinkedBaseStorage() const;
	ARpgBaseCampActor* ResolveSpatialBaseCamp() const;
	int32 GetAvailableInventoryResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const;
	TFunction<bool()> MakeContextRevalidator(AActor* RequestingActor, bool bRefund = false, FGuid ExpectedJobId = FGuid()) const;
	bool BuildOutputPlan(const TArray<FRpgCraftingOutputItem>& OutputItems, TArray<FRpgInventoryBatchOperation>& InOutOperations) const;
	bool ConsumeResourcesWithRefund(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems, int32 Quantity, TArray<FRpgCraftingRefundEntry>& OutRefundEntries);
	void SpendRefundCreditsForCompletedUnit(FRpgCraftingJobEntry& Job);
	bool RefundRemainingJobCosts(FGuid JobId, TFunction<void()> CommitSideEffects = {});
	bool FlushOutputToBaseStorageInternal();
	void RetryBlockedOutput();
	void TryStartNextQueuedJob();
	void StartJobAtIndex(int32 JobIndex, float DurationOverride = -1.0f, bool bPauseStateChanged = false);
	void CompleteActiveJobUnit();
	void CompleteActiveJobUnitInternal();
	int32 FindActiveJobIndex() const;
	int32 FindJobIndex(FGuid JobId) const;
	bool HasActiveOrPausedJob() const;
	float GetServerWorldTimeSeconds() const;
	float GetRecipeCraftTime(const URpgCraftingRecipeDefinition* RecipeDefinition) const;

	void MarkCraftingStateDirty(FGuid ChangedJobId = FGuid(), ERpgCraftingJobState ChangedState = ERpgCraftingJobState::Queued, bool bPauseStateChanged = false);

private:
	// Covers preparation, publication and synchronous observers; nested public commands cannot invalidate paid jobs.
	bool bMutationInProgress = false;
	bool bPersistenceRestorePending = false;
	FTimerHandle CraftingTimerHandle;
	FTimerHandle OutputRetryTimerHandle;
};
