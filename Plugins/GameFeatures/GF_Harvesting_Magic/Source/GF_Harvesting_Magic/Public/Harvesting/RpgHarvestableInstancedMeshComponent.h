#pragma once

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "TimerManager.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "Harvesting/RpgHarvestableTarget.h"

#include "RpgHarvestableInstancedMeshComponent.generated.h"

class URpgHarvestableInstancedMeshComponent;
class URpgHarvestProfile;
class FLifetimeProperty;
struct FInteractionQuery;
struct FInteractionOption;

/** Server-only notification emitted after one resource instance is successfully harvested. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FRpgResourceInstanceHarvestedEvent,
	int32, InstanceIndex,
	int32, NewRevision,
	const FRpgHarvestRequest&, Request);

/** Cosmetic/read-only notification emitted whenever replicated instance state is applied locally. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FRpgResourceInstanceStateChangedEvent,
	int32, InstanceIndex,
	bool, bIsActive,
	int32, Revision);

/** Replicated state for one resource instance whose authored active state has changed at runtime. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestedInstanceStateEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

private:
	friend struct FRpgHarvestedInstanceStateList;
	friend class URpgHarvestableInstancedMeshComponent;

	/** Stable runtime instance index. Instances are never removed or reordered after BeginPlay. */
	UPROPERTY()
	int32 InstanceIndex = INDEX_NONE;

	/** Monotonically increasing server revision used to reject stale interaction requests. */
	UPROPERTY()
	int32 Revision = 0;

	/** Server-authored availability; inactive instances keep their index but use a zero-scale transform. */
	UPROPERTY()
	bool bActive = true;
};

/** FastArray containing only resource instances that have changed from their authored state. */
USTRUCT()
struct GF_HARVESTING_MAGIC_API FRpgHarvestedInstanceStateList : public FFastArraySerializer
{
	GENERATED_BODY()

	FRpgHarvestedInstanceStateList() = default;
	explicit FRpgHarvestedInstanceStateList(URpgHarvestableInstancedMeshComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRpgHarvestedInstanceStateEntry, FRpgHarvestedInstanceStateList>(
			Entries,
			DeltaParams,
			*this);
	}

private:
	friend class URpgHarvestableInstancedMeshComponent;

	const FRpgHarvestedInstanceStateEntry* Find(int32 InstanceIndex) const;
	FRpgHarvestedInstanceStateEntry* FindMutable(int32 InstanceIndex);

	UPROPERTY()
	TArray<FRpgHarvestedInstanceStateEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<URpgHarvestableInstancedMeshComponent> OwnerComponent = nullptr;
};

template<>
struct TStructOpsTypeTraits<FRpgHarvestedInstanceStateList> : public TStructOpsTypeTraitsBase2<FRpgHarvestedInstanceStateList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 * Project-controlled HISM resource target with stable instance indices and server-authoritative depletion.
 *
 * Designers author the mesh and instances normally, but must not add, remove, or reorder instances after BeginPlay.
 * Only changed instance state is replicated; FastArray callbacks reconstruct depleted visuals for late joiners.
 */
UCLASS(BlueprintType, ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Harvestable Instanced Mesh"))
class GF_HARVESTING_MAGIC_API URpgHarvestableInstancedMeshComponent final
	: public UHierarchicalInstancedStaticMeshComponent
	, public IInteractableTarget
	, public IRpgHarvestableTarget
{
	GENERATED_BODY()

public:
	explicit URpgHarvestableInstancedMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ UInstancedStaticMeshComponent interface
	virtual int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace = false) override;
	virtual TArray<int32> AddInstances(const TArray<FTransform>& InstanceTransforms, bool bShouldReturnIndices, bool bWorldSpace = false, bool bUpdateNavigation = true) override;
	virtual bool RemoveInstance(int32 InstanceIndex) override;
	virtual bool RemoveInstances(const TArray<int32>& InstancesToRemove) override;
	virtual bool RemoveInstances(const TArray<int32>& InstancesToRemove, bool bInstanceArrayAlreadySortedInReverseOrder) override;
	virtual void RemoveInstancesById(const TArrayView<const FPrimitiveInstanceId>& InstanceIds, bool bUpdateNavigation = true) override;
	virtual void ClearInstances() override;
	//~ End UInstancedStaticMeshComponent interface

	//~ IInteractableTarget interface
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder) override;
	virtual bool CommitInteraction(const FInteractionQuery& AuthoritativeQuery, const FInteractionOption& ValidatedOption) override;
	//~ End IInteractableTarget interface

	//~ IRpgHarvestableTarget interface
	virtual bool CanAcceptHarvest_Implementation(const FRpgHarvestRequest& Request) const override;
	virtual bool CommitHarvest_Implementation(const FRpgHarvestRequest& Request) override;
	//~ End IRpgHarvestableTarget interface

	/** Returns whether the server-authored instance is available for focus or harvesting. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	bool IsResourceInstanceActive(int32 InstanceIndex) const;

	/** Returns the current replicated revision, or INDEX_NONE when the instance index is invalid. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	int32 GetResourceInstanceRevision(int32 InstanceIndex) const;

	/**
	 * Changes one instance on authority without removing or reordering it.
	 * Returns true only when a valid instance changed state; clients cannot mutate this state.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Harvesting|Instances")
	bool SetResourceInstanceActive(int32 InstanceIndex, bool bNewActive);

	/**
	 * Server-only post-commit telemetry notification.
	 * For profile-backed nodes, native code has already delivered the complete reward and awarded XP before this fires;
	 * listeners must treat the event as read-only and must not grant additional loot or progression.
	 * Profile-less legacy nodes may continue to use it for migration-era feature orchestration.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Harvesting|Instances")
	FRpgResourceInstanceHarvestedEvent OnResourceInstanceHarvested;

	/**
	 * Cosmetic/read-only Blueprint event fired when state is applied locally, including late-join FastArray updates.
	 * Listeners must not grant loot or mutate authoritative gameplay state.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Harvesting|Instances")
	FRpgResourceInstanceStateChangedEvent OnResourceInstanceStateChanged;

protected:
	//~ UActorComponent interface
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent interface

	/** Static prompt text, ranges, icon, widget overrides, and priority used by every instance in this component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting|Interaction")
	FRpgInteractionPromptDefinition InteractionPrompt;

	/**
	 * Static rewards, progression gate, and respawn tuning for every instance in this component.
	 * When unset, the component retains its legacy deplete-only behavior for existing reference content.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting|Rewards")
	TObjectPtr<URpgHarvestProfile> HarvestProfile;

private:
	friend struct FRpgHarvestedInstanceStateList;

	bool IsValidResourceInstanceIndex(int32 InstanceIndex) const;
	bool CanMutateInstanceTopology(const TCHAR* OperationName) const;
	bool BuildInteractionOption(int32 InstanceIndex, const FInteractionQuery& InteractQuery, FInteractionOption& OutOption) const;
	void CacheAuthoredInstanceTransforms();
	void ApplyAllReplicatedInstanceStates();
	void ApplyReplicatedInstanceState(int32 InstanceIndex, bool bInstanceActive, int32 Revision);
	bool CanHarvesterMeetSkillGate(const FRpgHarvestRequest& Request) const;
	bool TryDeliverHarvestReward(const FRpgHarvestRequest& Request);
	void AwardHarvestExperience(const FRpgHarvestRequest& Request) const;
	void ScheduleResourceRespawn(int32 InstanceIndex);
	void ArmNextRespawnTimer();
	void HandleRespawnTimer();

	/** Sparse, server-authored changes from the active transforms stored in the authored HISM instance array. */
	UPROPERTY(Replicated)
	FRpgHarvestedInstanceStateList ReplicatedInstanceStates;

	/** Runtime-only local transforms used to restore an instance without changing its stable index. */
	TArray<FTransform> AuthoredInstanceTransforms;

	/** Server-only world-time deadlines; resource depletion remains session-scoped and is never saved. */
	TMap<int32, double> RespawnDeadlines;

	/** Prevents delegate-driven server re-entry from granting the same active revision twice. */
	TSet<int32> HarvestsInProgress;

	/** One component timer wakes only for the next due resource instance. */
	FTimerHandle RespawnTimerHandle;
};
