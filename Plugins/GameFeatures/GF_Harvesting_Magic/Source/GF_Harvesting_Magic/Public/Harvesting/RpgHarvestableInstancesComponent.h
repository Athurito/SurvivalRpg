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
	 * clients whenever the instance's stock is applied locally. Changed instances report their state when the
	 * component begins play, for example after streaming in. bInitialState is true for that state and for changes
	 * older than the live-change window, so presentation can snap instead of animating. Depleted instances are already
	 * hidden when bHideDepletedInstances is set. Never grant loot or change gameplay state from it.
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

	/** Hides depleted instances by scaling them to zero, which also removes their collision, and restores them on respawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting")
	bool bHideDepletedInstances = true;

private:
	friend class URpgHarvestInstanceStockComponent;

	bool IsValidResourceInstance(int32 InstanceIndex) const;
	URpgHarvestInstanceStockComponent* FindStock() const;
	FRpgHarvestStockSnapshot MakeStockSnapshot(int32 InstanceIndex) const;
	void BuildInstanceKeys();
	void HandleStockChanged(const FIntVector& Key, bool bInitialState);
	void PresentInstance(int32 InstanceIndex, bool bInitialState);

	/** Every addressable instance by stable key; built once at BeginPlay. Local, never replicated. */
	TMap<FIntVector, int32> InstanceIndexByKey;

	/** Authored local transforms of the instances whose presentation currently differs from them. */
	TMap<int32, FTransform> PresentedInstanceTransforms;

	/** Stock component this component is registered with. */
	mutable TWeakObjectPtr<URpgHarvestInstanceStockComponent> CachedStock;

	/** Server-only guard that rejects delegate-driven re-entry while one commit is in progress. */
	bool bCommitInProgress = false;
};
