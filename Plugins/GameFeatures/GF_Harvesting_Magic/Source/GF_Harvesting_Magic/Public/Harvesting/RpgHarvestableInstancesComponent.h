#pragma once

#include "Components/InstancedStaticMeshComponent.h"
#include "Harvesting/RpgHarvestableTarget.h"

#include "RpgHarvestableInstancesComponent.generated.h"

class URpgHarvestInstanceStockComponent;
class URpgHarvestProfile;
struct FRpgHarvestStockSnapshot;

/**
 * Harvestable instanced static meshes, such as ore veins or rocks scattered by PCG, whose stock lives in the world's
 * URpgHarvestInstanceStockComponent.
 *
 * Assign a Blueprint subclass per resource type as the component class of a PCG static mesh spawner entry. The
 * subclass sets HarvestProfile and may present sections through OnInstanceStockChanged. PCG partition actors do not
 * replicate, so this component replicates nothing: each instance is identified by its authored world location, and
 * the GameState's stock component stores and replicates only instances whose stock changed. Every instance holds the
 * full stock of HarvestProfile, so instances, actor-backed nodes, and every harvest method share the same stock rules.
 *
 * Requirements: instances must not be added, removed, or moved after BeginPlay; no two instances may share a
 * location within one centimeter; collision must block the trace channel of the harvest abilities. Depleted instances
 * are hidden by scaling them to zero, which also removes their collision. Weak points and the manual interaction
 * harvest are not supported on instances.
 *
 * Resources whose visible mesh cannot be traced, such as instanced skinned trees without a physics asset, use this
 * component as an invisible collision proxy at the same PCG points and link the visible instances through
 * LinkedPresentationTag.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Harvestable Instances"))
class GF_HARVESTING_MAGIC_API URpgHarvestableInstancesComponent : public UInstancedStaticMeshComponent, public IRpgHarvestableTarget
{
	GENERATED_BODY()

public:
	explicit URpgHarvestableInstancesComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ IRpgHarvestableTarget interface
	virtual int32 GetHarvestRevision_Implementation(const FHitResult& Hit) const override;
	virtual FRpgHarvestResult EvaluateHarvest_Implementation(const FRpgHarvestRequest& Request) const override;
	virtual FRpgHarvestResult CommitHarvest_Implementation(const FRpgHarvestRequest& Request) override;
	//~ End IRpgHarvestableTarget interface

	/** Returns the static profile that defines the stock, rewards, tool, and respawn rules of every instance. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	const URpgHarvestProfile* GetHarvestProfile() const { return HarvestProfile; }

	/** Returns the total stock sections of each instance, or zero without a profile. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	int32 GetSectionCount() const;

	/** Returns the stock sections left on InstanceIndex; zero while it is depleted or the index is invalid. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	int32 GetRemainingSections(int32 InstanceIndex) const;

	/**
	 * Returns the transform InstanceIndex was authored with, before any presentation scaling. Local to this component
	 * unless bWorldSpace is set. Returns false for an invalid index.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	bool GetAuthoredInstanceTransform(int32 InstanceIndex, FTransform& OutTransform, bool bWorldSpace) const;

	/**
	 * Scales InstanceIndex uniformly relative to its authored transform, for example to shrink a resource as its
	 * sections are extracted. Zero hides the instance and removes its collision; one restores the authored transform.
	 * Cosmetic and local: call it from OnInstanceStockChanged, never to change gameplay state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Harvesting|Instances")
	bool SetInstancePresentationScale(int32 InstanceIndex, float Scale);

	/**
	 * Returns the first visible instance linked to InstanceIndex through LinkedPresentationTag: its instanced static or
	 * skinned mesh component and its authored world transform. False when nothing is linked. Use it to spawn cosmetic
	 * presentation with the linked mesh, such as a falling tree.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	bool GetLinkedPresentationInstance(int32 InstanceIndex, UMeshComponent*& OutComponent, FTransform& OutWorldTransform) const;

	/**
	 * Returns the horizontal unit direction from whatever last struck InstanceIndex, the harvester or its swarm, toward
	 * the instance, the same on the server and every client; false while the instance is untouched. Use it to present a
	 * change consistently, for example to fell a tree away from whoever felled it. Cosmetic.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Instances")
	bool GetInstanceHarvestDirection(int32 InstanceIndex, FVector& OutDirection) const;

	/** Returns the component tag that links visible presentation instances; None links nothing. */
	FName GetLinkedPresentationTag() const { return LinkedPresentationTag; }

	/** Returns the number of resource instances with at least one linked presentation instance. */
	int32 GetNumLinkedInstances() const { return NumLinkedResources; }

	/** Returns the stable stock key of InstanceIndex, derived from its authored world location. */
	bool GetInstanceKey(int32 InstanceIndex, FIntVector& OutKey) const;

	/** Returns the instance whose stable stock key is Key; false when this component has no such instance. */
	bool FindInstanceByKey(const FIntVector& Key, int32& OutInstanceIndex) const;

	/** Returns the number of instances addressable by key; instances sharing a location count once. */
	int32 GetNumKeyedInstances() const { return InstanceIndexByKey.Num(); }

protected:
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/**
	 * Cosmetic event for sections, depletion, and respawn presentation of one instance, fired on the server and on
	 * clients whenever the instance's stock is applied locally. A live change with a presentation delay, such as one
	 * tree of a Death Wave, is presented when its delay has passed, at the same server time on every machine.
	 * Changed instances report their state when the component begins play, for example after streaming in.
	 * bInitialState is true for that state and for changes older than the live-change window, so presentation can
	 * snap instead of animating. Depleted instances are already hidden when bHideDepletedInstances is set. Never grant
	 * loot or change gameplay state from it.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Rpg|Harvesting|Instances")
	void OnInstanceStockChanged(int32 InstanceIndex, int32 RemainingSections, int32 SectionCount, bool bActive, bool bInitialState);
	virtual void OnInstanceStockChanged_Implementation(
		int32 InstanceIndex,
		int32 RemainingSections,
		int32 SectionCount,
		bool bActive,
		bool bInitialState);

	/**
	 * Static stock, reward, tool, progression, and respawn rules shared by every instance. Designer-assigned per
	 * Blueprint subclass; required, a component without a profile rejects every request.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting")
	TObjectPtr<URpgHarvestProfile> HarvestProfile;

	/**
	 * Hides depleted instances and their linked presentation instances by scaling them to zero, which also removes
	 * their collision, and restores them on respawn.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting")
	bool bHideDepletedInstances = true;

	/**
	 * Component tag of sibling instanced meshes on the same owner that show these resources, for example the visible
	 * trees whose invisible trunk proxies this component holds. An instance of a tagged instanced static or skinned
	 * mesh component at the same authored location, to the centimeter, presents the resource there: it is hidden
	 * with a depleted resource and restored on respawn. Designer-set per Blueprint subclass, together with the same tag
	 * on the PCG spawner of the visible meshes. None links nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting")
	FName LinkedPresentationTag;

private:
	friend class URpgHarvestInstanceStockComponent;

	bool IsValidResourceInstance(int32 InstanceIndex) const;
	URpgHarvestInstanceStockComponent* FindStock() const;
	FRpgHarvestStockSnapshot MakeStockSnapshot(int32 InstanceIndex) const;
	void BuildInstanceKeys();
	void BuildLinkedInstances();
	void HandleStockChanged(const FIntVector& Key, bool bInitialState);
	void PresentInstance(int32 InstanceIndex, bool bInitialState);
	void ArmPresentationTimer();
	void HandlePresentationTimer();
	void SetLinkedInstancesVisible(int32 InstanceIndex, bool bShow);
	bool UpdatePresentedTransform(UInstancedStaticMeshComponent& Component, int32 InstanceIndex, const FTransform& LocalTransform);
	void FlushNavigationUpdates();

	/** One visible instance of a sibling component that presents a resource instance. */
	struct FLinkedInstance
	{
		TWeakObjectPtr<UMeshComponent> Component;
		int32 InstanceIndex = INDEX_NONE;
	};

	/** Every addressable instance by stable key; built once at BeginPlay. Local, never replicated. */
	TMap<FIntVector, int32> InstanceIndexByKey;

	/** Authored local transforms of the instances whose presentation currently differs from them. */
	TMap<int32, FTransform> PresentedInstanceTransforms;

	/** Linked presentation instances by resource instance index; built once at BeginPlay. Local, never replicated. */
	TMultiMap<int32, FLinkedInstance> LinkedInstances;

	/** Number of distinct resource instances in LinkedInstances. */
	int32 NumLinkedResources = 0;

	/** Authored local transforms of the linked instances that are currently hidden, by component and instance index. */
	TMap<TPair<TWeakObjectPtr<UMeshComponent>, int32>, FTransform> HiddenLinkedTransforms;

	/** Stock component this component is registered with. */
	mutable TWeakObjectPtr<URpgHarvestInstanceStockComponent> CachedStock;

	/** World-time deadlines of live changes whose presentation waits for their delay, by instance index. Local. */
	TMap<int32, double> PendingPresentations;

	/** One timer wakes only for the next due presentation; no tick is used. */
	FTimerHandle PresentationTimerHandle;

	/**
	 * World transforms of instances presented since the last navigation update, before and after each change. The
	 * next tick updates navigation once for all of them. Local.
	 */
	TArray<FTransform> DeferredNavigationTransforms;

	/** Wakes on the next tick to update navigation for the instances presented this frame. */
	FTimerHandle NavigationFlushTimerHandle;

	/** Server-only guard that rejects delegate-driven re-entry while one commit is in progress. */
	bool bCommitInProgress = false;
};
