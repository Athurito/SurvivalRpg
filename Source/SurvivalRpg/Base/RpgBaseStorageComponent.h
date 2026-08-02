#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "RpgBaseStorageSaveTypes.h"
#include "RpgBaseStorageTransactionTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include "RpgBaseStorageComponent.generated.h"

class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgBaseStorageUpgradeDefinition;
class APlayerController;
class FDataValidationContext;
struct FNetDeltaSerializeInfo;

/** Resource capacity contribution for one material definition. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceCapacity
{
	GENERATED_BODY()

	/** Material item definition this capacity applies to. Static designer data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage", meta = (AssetBundles = "Server"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Number of units this base or station can hold for the item definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage", meta = (ClampMin = "0", UIMin = "0"))
	int32 Capacity = 0;
};

/** Exact fungible material amount consumed by a base-owned operation such as Rift cleansing. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseStorageOperationCost
{
	GENERATED_BODY()

	/** Explicit BulkResource definition consumed from the shared Materials domain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Operation", meta = (AssetBundles = "Server"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Positive number of fungible units consumed atomically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Operation", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;
};

/** Read-only resource row exposed to UI and crafting. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceEntryView
{
	GENERATED_BODY()

	/** Material item definition represented by this resource row. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Current stored resource count. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 Count = 0;

	/** Current total capacity after base and station bonuses. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 Capacity = 0;

	/** Shared material-capacity points consumed by one stored unit of this definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 CapacityCost = 1;

	/** Shared replicated order key for UI sorting. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 SortIndex = 0;
};

/** GameplayMessage payload for replicated base-resource changes. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceChangeMessage
{
	GENERATED_BODY()

	/** Storage component whose resource row changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	TObjectPtr<UActorComponent> StorageOwner = nullptr;

	/** Material item definition that changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** New stored count after the change. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 NewCount = 0;

	/** Difference between new and previous count. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 Delta = 0;

	/** Current capacity for this resource. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 Capacity = 0;

	/** Shared replicated order key for UI refresh. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	int32 SortIndex = 0;

	/** True when capacity changed without necessarily changing count. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	bool bCapacityChanged = false;

	/** True when resource order changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Storage")
	bool bOrderChanged = false;
};

/** One replicated material resource count in a base storage pool. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

private:
	friend struct FRpgBaseResourceList;
	friend class URpgBaseStorageComponent;

	UPROPERTY()
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	int32 Capacity = 0;

	UPROPERTY()
	int32 SortIndex = 0;

	UPROPERTY(NotReplicated)
	int32 LastObservedCount = INDEX_NONE;

	UPROPERTY(NotReplicated)
	int32 LastObservedCapacity = INDEX_NONE;
};

/** Replicated resource list backing a base camp storage component. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseResourceList : public FFastArraySerializer
{
	GENERATED_BODY()

	FRpgBaseResourceList()
		: OwnerComponent(nullptr)
	{
	}

	explicit FRpgBaseResourceList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	TArray<FRpgBaseResourceEntryView> GetAllResources() const;
	int32 GetResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	int32 GetResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	int32 GetFreeResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	bool CanStoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;

	bool StoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);
	bool WithdrawResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);
	void AddResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 DeltaCapacity);
	bool ApplySort(ERpgInventorySortMode SortMode);
	bool MoveResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex);

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRpgBaseResourceEntry, FRpgBaseResourceList>(Entries, DeltaParms, *this);
	}

private:
	friend class URpgBaseStorageComponent;

	FRpgBaseResourceEntry* FindEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);
	const FRpgBaseResourceEntry* FindEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	FRpgBaseResourceEntry& FindOrAddEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);
	void BroadcastChangeMessage(FRpgBaseResourceEntry& Entry, int32 OldCount, int32 OldCapacity, bool bOrderChanged = false);
	int32 GetNextSortIndex() const;
	void SortEntriesBySortIndex();
	bool SetOrderFromSortedEntryPointers(const TArray<FRpgBaseResourceEntry*>& SortedEntries);

private:
	UPROPERTY()
	TArray<FRpgBaseResourceEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FRpgBaseResourceList> : public TStructOpsTypeTraitsBase2<FRpgBaseResourceList>
{
	enum { WithNetDeltaSerializer = true };
};

/** Native-only exact checkpoint for one bulk row used by cross-system transaction rollback. */
struct SURVIVALRPG_API FRpgBaseResourceMutationCheckpoint
{
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
	bool bHadEntry = false;
	FRpgBaseResourceEntry Entry;
};

/**
 * Server-authoritative shared resource pool for a player base.
 *
 * Resources are stored as counts for performance and UI clarity. Instance-based gear belongs in the
 * base camp's separate armory inventory manager.
 */
UCLASS(Blueprintable, ClassGroup = (Base), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgBaseStorageComponent : public UActorComponent
{
	GENERATED_BODY()
	friend struct FRpgBaseResourceList;

public:
	explicit URpgBaseStorageComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Returns all known resource rows sorted by replicated SortIndex. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure = false)
	TArray<FRpgBaseResourceEntryView> GetAllResources() const;

	/** Returns the current stored count for one material definition. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	int32 GetResourceCount(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns current total capacity for one material definition. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	int32 GetResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns the shared Materials-domain capacity in authored capacity points. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capacity", BlueprintPure)
	int32 GetMaterialCapacityPoints() const { return MaterialCapacityPoints; }

	/** Returns capacity points currently occupied by all BulkResource definitions. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capacity", BlueprintPure)
	int32 GetUsedMaterialCapacityPoints() const;

	/** Returns free shared material capacity, clamped to zero while a migrated save is over capacity. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capacity", BlueprintPure)
	int32 GetFreeMaterialCapacityPoints() const;

	/** True only for a migrated/rebalanced state whose preserved contents exceed current authored capacity. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capacity", BlueprintPure)
	bool IsMaterialDomainOverCapacity() const;

	/** True when preserved Armory placements exceed the currently configured upgrade grid. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capacity", BlueprintPure)
	bool IsArmoryDomainOverCapacity() const { return bArmoryDomainOverCapacity; }

	/** True when preserved contained objects exceed the currently configured sealed-slot layout. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capacity", BlueprintPure)
	bool IsContainmentDomainOverCapacity() const { return bContainmentDomainOverCapacity; }

	/** Returns the positive capacity-point cost authored by the definition's StorageProfile, or zero when not BulkResource. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capacity", BlueprintPure)
	int32 GetBulkCapacityCost(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns unused capacity for one material definition. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	int32 GetFreeResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns true if this many resources can be stored without exceeding capacity. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	bool CanStoreResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;

	/** Returns whether the definition explicitly opts into stateless bulk storage and fits the shared domain pool. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	bool CanStoreResourceDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;

	/** True when the profile permits a manual bulk deposit with this network's installed capabilities. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	bool CanManuallyDepositBulk(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** True when the profile and network explicitly permit one-action smart auto-deposit. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage", BlueprintPure)
	bool CanAutoDepositBulk(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** True when this explicit BulkResource is allowed as a shared crafting source by profile and base capability. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Crafting", BlueprintPure)
	bool CanCraftFromNetwork(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/**
	 * Returns whether a concrete material can be losslessly projected into this definition/count pool.
	 * Any semantic StatTag or fragment-owned payload fails closed because the pool cannot rehydrate instance state.
	 */
	bool CanStoreResourceInstance(const URpgInventoryItemInstance* Item, int32 Count) const;

	/**
	 * Adds a trusted synthetic/default resource credit to the base pool. Native-only and server-authoritative.
	 * This is reserved for recipe output, refunds, and rollback; concrete inventory items must use
	 * StoreResourceInstance so runtime variants and container subtrees cannot lose state.
	 */
	bool StoreDefinitionResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);

	/**
	 * Adds a validated stateless-material credit to the definition/count pool. Server-authoritative and fail-closed.
	 * This does not consume Item or verify inventory ownership/quantity; the authoritative caller owns consume and
	 * rollback around this credit write.
	 */
	bool StoreResourceInstance(const URpgInventoryItemInstance* Item, int32 Count);

	/** Captures one exact bulk row, including ordering, before a multi-system mutation. */
	bool CaptureResourceMutationCheckpoint(
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
		FRpgBaseResourceMutationCheckpoint& OutCheckpoint) const;

	/** Restores one exact bulk row without resetting replay history; rollback advances the monotonic revision. */
	bool RestoreResourceMutationCheckpoint(
		const FRpgBaseResourceMutationCheckpoint& Checkpoint);

	/** Removes resource counts from the base pool. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage")
	bool WithdrawResource(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count);

	/** Legacy native migration seam for pre-network station capacity. It is a no-op for the required shared-domain mode. */
	void AddResourceCapacity(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 DeltaCapacity);

	/** Native rollback/test seam for shared capacity; production capacity is rebuilt exclusively from installed upgrades. */
	bool AddMaterialCapacityPoints(int32 DeltaCapacityPoints);

	/** Monotonic revision advanced after each committed storage-network mutation. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Transaction", BlueprintPure)
	int64 GetNetworkRevision() const { return NetworkRevision; }

	/** True after an atomic rollback could not reconstruct a known-good state; authority rejects every later mutation until a successful restore. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Transaction", BlueprintPure)
	bool IsMutationTainted() const { return bMutationTainted; }

	/** Effective shared Rift strain including installed passive burden, clamped to the inclusive range 0..100. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Rift", BlueprintPure)
	int32 GetRiftStrain() const { return FMath::Clamp(RiftStrain + PassiveRiftStrain, 0, 100); }

	/** Returns raw cleanseable strain points; installed passive burden is deliberately excluded. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Rift", BlueprintPure)
	int32 GetCleanseableRiftStrain() const { return RiftStrain; }

	/** Applies installed Rift mitigation to a raw designer-authored strain delta without mutating state. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Rift", BlueprintPure)
	int32 GetMitigatedRiftStrainDelta(int32 RawStrainDelta) const
	{
		return FMath::Max(0, RawStrainDelta - RiftStrainMitigation);
	}

	/** Returns the replicated base-wide capabilities derived from baseline and installed upgrades. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capabilities", BlueprintPure)
	FGameplayTagContainer GetInstalledCapabilities() const { return InstalledCapabilities; }

	/** Returns whether this base currently owns the exact requested storage capability. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Capabilities", BlueprintPure)
	bool HasInstalledCapability(FGameplayTag CapabilityTag) const;

	/** Returns upgrades installed across fixed domain anchors. Multiple distinct progression packages may target one anchor. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Upgrades", BlueprintPure = false)
	TArray<URpgBaseStorageUpgradeDefinition*> GetInstalledUpgrades() const;

	/** Returns whether the exact PrimaryDataAsset is already installed anywhere in this base network. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Upgrades", BlueprintPure)
	bool HasInstalledUpgrade(const URpgBaseStorageUpgradeDefinition* UpgradeDefinition) const;

	/** Checks explicit anchor/domain targeting, world knowledge, existing capabilities, and safe effects before payment. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Upgrades", BlueprintPure = false)
	bool CanInstallUpgrade(const URpgBaseStorageUpgradeDefinition* UpgradeDefinition, FText& OutFailureReason) const;

	/** Installs a prevalidated upgrade and recomputes all domain effects. Costs remain owned by the caller transaction. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage|Upgrades")
	bool InstallUpgrade(URpgBaseStorageUpgradeDefinition* UpgradeDefinition);

	/** Returns whether removing the upgrade keeps bulk usage, concrete grids, and containment occupants valid. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Upgrades", BlueprintPure = false)
	bool CanDecommissionUpgrade(const URpgBaseStorageUpgradeDefinition* UpgradeDefinition, FText& OutFailureReason) const;

	/** Removes a safe upgrade and recomputes effects. Authored refunds are granted by the caller transaction. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage|Upgrades")
	bool DecommissionUpgrade(URpgBaseStorageUpgradeDefinition* UpgradeDefinition);

	/** Number of concrete Rift items that the currently installed containment configuration can hold. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Rift", BlueprintPure)
	int32 GetContainmentSlotCapacity() const { return ContainmentSlotCapacity; }

	/** Derived abstract strength available to contained item profiles; replicated and UI-read-only. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Rift", BlueprintPure)
	float GetContainmentStrength() const { return ContainmentStrength; }

	/** Derived abstract corruption shielding available to contained item profiles; replicated and UI-read-only. */
	UFUNCTION(BlueprintCallable, Category = "Base Storage|Rift", BlueprintPure)
	float GetCorruptionProtection() const { return CorruptionProtection; }

	/** Adds a raw authored Rift strain delta after installed mitigation if the effective 100-point ceiling is not exceeded. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage|Rift")
	bool TryAddRiftStrain(int32 StrainDelta);

	/** Reduces Rift strain by an authored positive amount after the caller atomically pays cleanse costs. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage|Rift")
	bool CleanseRiftStrain(int32 CleanseAmount);

	/** Restores raw strain captured before a failed authoritative command; native rollback seam, not a gameplay action. */
	bool RestoreRiftStrainCheckpoint(int32 PreviousCleanseableStrain);

	/** Designer-authored shared-material costs for one deterministic cleanse. */
	const TArray<FRpgBaseStorageOperationCost>& GetRiftCleanseCosts() const { return RiftCleanseCosts; }

	/** Amount removed by one successful cleanse transaction. */
	int32 GetRiftCleanseAmount() const { return RiftCleanseAmount; }

	/** Exports pointer-free bulk, capability, upgrade, and strain state into a base save envelope. */
	void ExportStorageState(FRpgBaseStorageSaveData& OutSaveData) const;

	/** Atomically restores internal storage state. Concrete Armory/Containment graphs are restored by the base actor. */
	bool RestoreStorageState(const FRpgBaseStorageSaveData& SaveData, FString& OutError);

	/** Re-evaluates concrete-domain overflow and shrinks restore-expanded grids once every placement fits again. */
	void RefreshConcreteDomainOverCapacityState();

	/**
	 * Admits a request for execution or returns a cached/rejected result.
	 * False means the caller must return OutResult without executing gameplay side effects.
	 */
	bool AdmitCommand(
		const FRpgBaseStorageRequestContext& Context,
		uint32 PayloadHash,
		APlayerController* RequestingController,
		FRpgBaseStorageCommandResult& OutResult);

	/** Records one immutable result in the bounded replay window. Successful callers mutate gameplay before invoking this. */
	FRpgBaseStorageCommandResult CompleteCommand(
		const FRpgBaseStorageRequestContext& Context,
		uint32 PayloadHash,
		ERpgBaseStorageResultCode Code,
		int32 RequestedCount = 0,
		int32 AppliedCount = 0,
		const TArray<FRpgBaseStorageResourceCommandOutcome>& ResourceOutcomes = {});

	/** Finalizes a result carrying operation-specific preview/delta fields and inserts it into the replay window. */
	FRpgBaseStorageCommandResult CompleteDetailedCommand(
		const FRpgBaseStorageRequestContext& Context,
		uint32 PayloadHash,
		FRpgBaseStorageCommandResult Result);

	/** Clears transient replay state and starts a new command epoch after disk restore. */
	void ResetCommandEpochAfterRestore();

	/** Coalesces an authority-owned non-ledger state change into the active command or one standalone network revision. */
	void NotifyExternalStorageStateMutation();

	/** Fail-closes this network after any atomic rollback cannot re-establish a known-good state and blocks host disk writes. */
	void TaintAfterRollbackFailure();

	/** Legacy native-only shared sort retained for migration tests; current terminal sorting is client-local and never calls this. */
	bool ApplyResourceSort(ERpgInventorySortMode SortMode);

	/** Legacy native-only shared row move retained for migration tests; current terminal ordering is client-local. */
	bool MoveResourceEntry(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 TargetIndex);

protected:
	/** Legacy per-definition capacity list retained for old assets when shared-domain capacity is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage")
	TArray<FRpgBaseResourceCapacity> DefaultResourceCapacities;

	/** Shared starting capacity for the Materials domain. One ordinary early resource consumes one point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Capacity", meta = (ClampMin = "0", UIMin = "0"))
	int32 BaseMaterialCapacityPoints = 300;

	/** Required V2 shared-domain pool switch. False is accepted only by isolated legacy migration tests and fails data validation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Capacity")
	bool bUseSharedMaterialCapacity = true;

	/** Baseline comfort capabilities available before progression upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Capabilities", meta = (Categories = "Storage.Capability"))
	FGameplayTagContainer BaselineCapabilities;

	/** Dormant V1 Rift anchor has no usable slots until Containment I contributes sealed slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Rift", meta = (ClampMin = "0", UIMin = "0"))
	int32 BaseContainmentSlots = 0;

	/** Initial Armory root width in inventory grid cells before domain upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Armory", meta = (ClampMin = "1", UIMin = "1"))
	int32 BaseArmoryGridColumns = 8;

	/** Initial Armory root height in inventory grid cells before domain upgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Armory", meta = (ClampMin = "1", UIMin = "1"))
	int32 BaseArmoryGridRows = 6;

	/** Early-plus-Rift BulkResource costs paid atomically for one cleanse operation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Rift")
	TArray<FRpgBaseStorageOperationCost> RiftCleanseCosts;

	/** Deterministic strain removed by one cleanse, in integer percentage points. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Rift", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100", Units = "Percent"))
	int32 RiftCleanseAmount = 25;

private:
	struct FRecentCommandResult
	{
		uint32 PayloadHash = 0;
		uint64 CommandEpoch = 0;
		TWeakObjectPtr<APlayerController> RequestingController;
		FRpgBaseStorageCommandResult Result;
	};

	void MarkStorageStateDirty();
	void RefreshDerivedResourceCapacities();
	/** Returns the saturating 64-bit material usage used by all authoritative capacity decisions. */
	int64 GetUsedMaterialCapacityPoints64() const;
	/** Returns authoritative free material capacity without narrowing an over-capacity usage total. */
	int64 GetFreeMaterialCapacityPoints64() const;
	/** Builds the base-owned capabilities that exist independently of installed upgrade grants. */
	FGameplayTagContainer BuildEffectiveBaselineCapabilities() const;
	/** Validates every candidate upgrade against aggregate grants and current world knowledge. */
	bool ValidateUpgradeSetRequirements(
		const TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>>& CandidateUpgrades,
		const FGameplayTagContainer& CandidateCapabilities,
		FText* OutFailureReason) const;
	/** Coalesces a retry when an inventory commit still owns its mutation lock. */
	void ScheduleConcreteDomainCapacityRefresh();
	bool RebuildDerivedUpgradeState(
		const TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>>& CandidateUpgrades,
		FText* OutFailureReason = nullptr,
		bool bAllowPreservedOverCapacityState = false);
	URpgBaseStorageUpgradeDefinition* ResolveUpgradeDefinition(const FPrimaryAssetId& UpgradeId) const;
	FName GetOwningBaseId() const;
	void CacheCommandResult(
		const FGuid& RequestId,
		uint32 PayloadHash,
		APlayerController* RequestingController,
		const FRpgBaseStorageCommandResult& Result);
	bool DeferResourceChangeMessage();
	void FlushDeferredResourceChangeMessages();

	UPROPERTY(Replicated)
	FRpgBaseResourceList ResourceList;

	/** Replicated shared Materials-domain capacity after anchor and upgrade effects. */
	UPROPERTY(Replicated)
	int32 MaterialCapacityPoints = 0;

	/** Replicated base-wide functional capabilities. Runtime mutation is authority-only and persisted. */
	UPROPERTY(Replicated)
	FGameplayTagContainer InstalledCapabilities;

	/** Installed PrimaryDataAssets. Server state is persisted by id and replicated for terminal presentation. */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>> InstalledUpgrades;

	/** Derived number of concrete entries accepted by the Rift containment inventory. */
	UPROPERTY(Replicated)
	int32 ContainmentSlotCapacity = 0;

	/** Derived configured Armory width before any restore-only preservation expansion. */
	UPROPERTY(Replicated)
	int32 ArmoryGridColumns = 8;

	/** Derived configured Armory height before any restore-only preservation expansion. */
	UPROPERTY(Replicated)
	int32 ArmoryGridRows = 6;

	/** Replicated warning while retained Armory entries or placements exceed configured capacity. */
	UPROPERTY(Replicated)
	bool bArmoryDomainOverCapacity = false;

	/** Replicated warning while retained Rift items exceed configured sealed slots or placement bounds. */
	UPROPERTY(Replicated)
	bool bContainmentDomainOverCapacity = false;

	/** Runtime-only guard preventing inventory-delta bursts from queuing duplicate grid-normalization retries. */
	bool bConcreteDomainCapacityRefreshPending = false;
	/** Runtime-only recursion guard for synchronous capacity-change messages emitted by grid normalization. */
	bool bConcreteDomainCapacityRefreshInProgress = false;

	/** Derived non-negative containment strength from installed Rift-domain upgrades. */
	UPROPERTY(Replicated)
	float ContainmentStrength = 0.0f;

	/** Derived non-negative corruption protection from installed Rift-domain upgrades. */
	UPROPERTY(Replicated)
	float CorruptionProtection = 0.0f;

	/** Derived non-cleanseable strain burden after installed passive strain and tolerance offset each other. */
	UPROPERTY(Replicated)
	int32 PassiveRiftStrain = 0;

	/** Derived whole-point mitigation subtracted from each positive authored Rift strain operation. */
	UPROPERTY(Replicated)
	int32 RiftStrainMitigation = 0;

	/** Replicated cleanseable Rift strain. Effective UI strain additionally includes PassiveRiftStrain. */
	UPROPERTY(Replicated)
	int32 RiftStrain = 0;

	/** Replicated storage-network revision used for optimistic concurrency and late-join UI refresh. */
	UPROPERTY(Replicated)
	int64 NetworkRevision = 0;

	/** Replicated fatal mutation guard set by the authority when rollback fails; cleared only by a successful persisted-state restore. */
	UPROPERTY(Replicated)
	bool bMutationTainted = false;

	TMap<FGuid, FRecentCommandResult> RecentCommandResults;
	TArray<FGuid> RecentCommandOrder;
	TMap<FGuid, TWeakObjectPtr<APlayerController>> AdmittedCommandRequesters;
	FGuid ActiveCommandRequestId;
	TArray<FRpgBaseResourceEntry> CommandResourceEntriesBefore;
	bool bCommandStorageStateDirty = false;
	bool bCommandResourceMessagesDeferred = false;
	uint64 CommandEpoch = 0;
	static constexpr int32 MaxRecentCommandResults = 64;
};
