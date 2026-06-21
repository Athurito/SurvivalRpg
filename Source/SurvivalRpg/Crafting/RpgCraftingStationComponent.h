#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"

#include "RpgCraftingStationComponent.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;
class ARpgBaseCampActor;
class ARpgDroppedInventoryActor;
class URpgBaseStorageStationComponent;
class URpgBaseStorageComponent;
class URpgCraftingRecipeDefinition;
class URpgCraftingRecipeSet;

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

	/** Output could not be stored or dropped; the station is waiting for a retry path. */
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;

	/** Returns player inventory plus crafting-accessible containers in range or in the same storage group. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting")
	TArray<URpgInventoryManagerComponent*> GetResourceInventories(AActor* RequestingActor) const;

	/** Runtime-links this placed or spawned station to a base camp. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Base Storage")
	void SetLinkedBaseCamp(ARpgBaseCampActor* NewBaseCamp);

	/** Returns the linked base camp this station may consume from and auto-deposit into. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Base Storage")
	ARpgBaseCampActor* GetLinkedBaseCamp() const { return LinkedBaseCamp; }

	/** Semantic station tags used by recipe filters, such as Crafting.Station.Smelter. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting|Recipes", meta = (Categories = "Crafting.Station"))
	FGameplayTagContainer GetStationTags() const { return StationTags; }

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

	/** Returns the maximum quantity the requesting actor can currently enqueue from available resources. */
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

	/** Replicated inventory where crafted outputs wait when they are not auto-deposited into the base. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	URpgInventoryManagerComponent* GetOutputInventory() const { return OutputInventoryComponent; }

	/** Assigns the output inventory component, usually from a native or Blueprint crafting-station actor constructor. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Output")
	void SetOutputInventoryManager(URpgInventoryManagerComponent* InOutputInventory);

	/** Returns true when every output can either auto-deposit or fit into the output inventory. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	bool CanAcceptCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems) const;

	/** Adds already-crafted outputs to base storage/armory and the output inventory. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Output")
	bool AddCraftingOutputs(const TArray<FRpgCraftingOutputItem>& OutputItems);

	/** Convenience V1 craft path: verifies output room, consumes costs, then stores outputs. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	bool CraftItems(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems, const TArray<FRpgCraftingOutputItem>& OutputItems);

	/** Attempts to move current output inventory contents into linked base storage/armory. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Output")
	bool FlushOutputToBaseStorage();

	/** Returns true when this station has an upgrade/config source that permits auto-deposit. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Crafting|Output")
	bool HasCraftingOutputAutoDepositAccess() const;

	/** Runtime station toggle for auto-deposit. The upgrade/config source must still grant access. */
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
	/** Interaction option shown by the Lyra-style interaction scan when this station can open its crafting UI. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Interaction")
	FInteractionOption OpenCraftingOption;

	/** Station identity tags used by recipe definitions to decide where they can be crafted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Recipes", meta = (Categories = "Crafting.Station"))
	FGameplayTagContainer StationTags;

	/** Data-driven recipe list offered by this station. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Recipes")
	TObjectPtr<URpgCraftingRecipeSet> AvailableRecipeSet;

	/** Shared storage group this station belongs to. Empty means radius-only shared-container lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	FName StorageGroupId;

	/** Whether old shared containers in range/storage group are included as recipe input sources. Disabled by default for V1 basislager flow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	bool bUseNearbyCraftingContainers = false;

	/** Radius in centimeters for including nearby shared containers as crafting resource sources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (EditCondition = "bUseNearbyCraftingContainers", ClampMin = "0", UIMin = "0", Units = "cm"))
	float StorageSearchRadius = 1200.0f;

	/** Maximum direct distance in centimeters for taking outputs from this station. Zero or below allows access at any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/** Optional base camp resource pool this station may pull material counts from. */
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category = "Crafting|Base Storage")
	TObjectPtr<ARpgBaseCampActor> LinkedBaseCamp;

	/** Whether recipe checks and consumption include the linked base camp's material-count pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Base Storage")
	bool bUseLinkedBaseStorage = true;

	/** Resource source order used by recipe cost checks and consumption. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Base Storage")
	ERpgCraftingResourceConsumeOrder ResourceConsumeOrder = ERpgCraftingResourceConsumeOrder::BaseThenPlayer;

	/** Actor whose base storage station component supplies output auto-deposit upgrade tags. Set this to the placed terminal/storage-unit actor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Output")
	TObjectPtr<AActor> OutputAutoDepositUpgradeProviderActor;

	/** Direct component fallback for advanced Blueprint setups. Prefer OutputAutoDepositUpgradeProviderActor for placed level actors. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Output")
	TObjectPtr<URpgBaseStorageStationComponent> OutputAutoDepositUpgradeProvider;

	/** Debug/prototype override that enables auto-deposit without requiring the upgrade provider tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output")
	bool bAlwaysAutoDepositCraftingOutputs = false;

	/** Runtime toggle for this station. Replicated for UI; auto-deposit still requires the upgrade/config access source. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_CraftingState, BlueprintReadOnly, Category = "Crafting|Output")
	bool bAutoDepositCraftingOutputsEnabled = true;

	/** Whether instance-based outputs may go directly to the linked base armory when auto-deposit is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output")
	bool bAutoDepositInstanceOutputsToArmory = true;

	/** Output inventory used when auto-deposit is disabled or linked storage is full. Usually a fixed 4-slot component. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Crafting|Output")
	TObjectPtr<URpgInventoryManagerComponent> OutputInventoryComponent;

	/** Fixed slot count configured on output inventories assigned through SetOutputInventoryManager. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting|Output", meta = (ClampMin = "0", UIMin = "0"))
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

	URpgBaseStorageComponent* GetLinkedBaseStorage() const;
	URpgInventoryManagerComponent* GetLinkedArmoryInventory() const;
	int32 GetAvailableInventoryResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const;
	bool ConsumeInventoryResources(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, const TArray<URpgInventoryManagerComponent*>& ResourceInventories) const;
	bool ConsumeBaseResources(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;
	bool ConsumeResourcesWithRefund(AActor* RequestingActor, const TArray<FRpgCraftingResourceCost>& RequiredItems, int32 Quantity, TArray<FRpgCraftingRefundEntry>& OutRefundEntries);
	void SpendRefundCreditsForCompletedUnit(FRpgCraftingJobEntry& Job);
	void RefundRemainingJobCosts(FRpgCraftingJobEntry& Job);
	bool RefundResourceCredit(const FRpgCraftingRefundEntry& RefundEntry);
	void TryStartNextQueuedJob();
	void StartJobAtIndex(int32 JobIndex, float DurationOverride = -1.0f, bool bPauseStateChanged = false);
	void CompleteActiveJobUnit();
	int32 FindActiveJobIndex() const;
	int32 FindJobIndex(FGuid JobId) const;
	bool HasActiveOrPausedJob() const;
	float GetServerWorldTimeSeconds() const;
	float GetRecipeCraftTime(const URpgCraftingRecipeDefinition* RecipeDefinition) const;
	bool AddOutputItemOrDrop(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);
	bool SpawnOrMergeDroppedOutput(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);
	bool TryMergeDroppedOutput(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;
	void MarkCraftingStateDirty(FGuid ChangedJobId = FGuid(), ERpgCraftingJobState ChangedState = ERpgCraftingJobState::Queued, bool bPauseStateChanged = false);

private:
	FTimerHandle CraftingTimerHandle;
};
