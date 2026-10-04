#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Harvesting/RpgHarvestableTarget.h"
#include "TimerManager.h"

#include "RpgHarvestableComponent.generated.h"

class URpgHarvestableComponent;
class URpgHarvestProfile;
class FLifetimeProperty;
struct FRpgHarvestStockSnapshot;

/** Replicated stock of one actor-backed harvestable resource. Server-authored; clients only read it. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestNodeState
{
	GENERATED_BODY()

	/** Server revision used to reject stale requests; advances only when the resource depletes or respawns. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 Revision = 0;

	/** Stock sections already extracted in the current revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	uint8 HarvestedSections = 0;

	/** Whether the resource is available; false while depleted and waiting for respawn. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bActive = true;

	/**
	 * Server world time in seconds of the last change, taken from AGameStateBase::GetServerWorldTimeSeconds.
	 * Lets clients present changes that happened before they received the actor without replaying animations.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	float LastChangeServerTime = 0.0f;
};

/**
 * Cosmetic/read-only notification fired on the server and on clients whenever the resource state is applied locally.
 * bInitialState is true for the state present at BeginPlay and for changes older than the live-change window,
 * for example on late join, so presentation can snap instead of animating. Listeners must not grant loot.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FRpgHarvestNodeStateChangedEvent,
	URpgHarvestableComponent*, Component,
	int32, RemainingSections,
	int32, SectionCount,
	bool, bActive,
	bool, bInitialState);

/** Server-only read-only telemetry fired after a commit has delivered its reward and updated the stock. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRpgHarvestNodeHarvestedEvent,
	const FRpgHarvestRequest&, Request,
	const FRpgHarvestResult&, Result);

/**
 * Server-authoritative stock for one actor-backed harvestable resource, such as an ore vein or a tree.
 *
 * Every harvest method extracts logical stock sections through IRpgHarvestableTarget, so all methods share one stock.
 * The component is representation-independent: the owning actor's Blueprint presents sections, depletion, and respawn
 * through OnHarvestStateChanged. It never ticks; the owning actor must replicate, and should use a low net update
 * frequency and network dormancy (DORM_Initial for placed resources) so untouched resources cost no bandwidth.
 */
UCLASS(BlueprintType, ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Harvestable Node"))
class GF_HARVESTING_MAGIC_API URpgHarvestableComponent final : public UActorComponent, public IRpgHarvestableTarget
{
	GENERATED_BODY()

public:
	/** Changes older than this many seconds are reported as initial state when they reach a client. */
	static constexpr float LiveChangeWindowSeconds = 1.5f;

	explicit URpgHarvestableComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ IRpgHarvestableTarget interface
	virtual int32 GetHarvestRevision_Implementation(const FHitResult& Hit) const override;
	virtual FRpgHarvestResult EvaluateHarvest_Implementation(const FRpgHarvestRequest& Request) const override;
	virtual FRpgHarvestResult CommitHarvest_Implementation(const FRpgHarvestRequest& Request) override;
	//~ End IRpgHarvestableTarget interface

	/** Returns the static profile that defines this resource's stock, rewards, tool, and respawn rules. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting")
	const URpgHarvestProfile* GetHarvestProfile() const { return HarvestProfile; }

	/** Returns the replicated stock state; read-only on clients. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting")
	FRpgHarvestNodeState GetHarvestState() const { return HarvestState; }

	/** Returns the total stock sections per revision, or zero without a profile. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting")
	int32 GetSectionCount() const;

	/** Returns the stock sections left in the current revision; zero while depleted. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting")
	int32 GetRemainingSections() const;

	/** Returns whether the resource is active and has stock left. Tool and skill requirements are not checked. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting")
	bool IsHarvestable() const;

	/**
	 * Returns the active weak point in world space and its hit radius in centimeters. False while the resource has no
	 * weak points or stock, or its profile awards no weak-point bonus. Derived from replicated state, so presentation
	 * on every client marks the same point the server checks swings against.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting")
	bool GetActiveWeakPoint(FVector& OutWorldLocation, float& OutRadius) const;

	/**
	 * Restores the complete stock immediately and cancels a pending respawn, for scripted events or debugging.
	 * Authority only; returns false when nothing changed or the caller lacks authority.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Harvesting")
	bool RestoreHarvestStock();

	/** Cosmetic event for sections, depletion, and respawn presentation. Never grant loot from it. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Harvesting")
	FRpgHarvestNodeStateChangedEvent OnHarvestStateChanged;

	/** Server-only post-commit telemetry. Listeners must not grant additional loot or progression. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Harvesting")
	FRpgHarvestNodeHarvestedEvent OnHarvested;

protected:
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UActorComponent interface

	/**
	 * Static stock, reward, tool, progression, and respawn rules for this resource. Designer-assigned per actor
	 * class or placed instance; required, a resource without a profile rejects every request.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting")
	TObjectPtr<URpgHarvestProfile> HarvestProfile;

	/**
	 * Weak point locations in the local space of WeakPointFrame, for example points on the resource mesh surface in
	 * mesh units. One is active at a time and moves on with every extracted section. Empty disables weak points; the
	 * profile's WeakPointBonusSections sets what a hit is worth. Designer-placed static data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting|Weak Points")
	TArray<FVector> WeakPointLocations;

	/** Hit tolerance in centimeters around the active weak point; swing impacts within it strike the weak point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Harvesting|Weak Points", meta = (ClampMin = "1.0", UIMin = "10.0", UIMax = "100.0", Units = "cm"))
	float WeakPointRadius = 35.0f;

	/**
	 * Scene component whose transform places WeakPointLocations, usually the resource mesh, so weak points follow its
	 * placement and its section scaling. Empty uses the owning actor's transform. Designer-assigned per actor class.
	 */
	UPROPERTY(EditAnywhere, Category = "Rpg|Harvesting|Weak Points", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	FComponentReference WeakPointFrame;

private:
	UFUNCTION()
	void OnRep_HarvestState();

	/** Writes a new authoritative state, wakes the dormant owner for replication, and presents it locally. */
	void SetAuthoritativeState(const FRpgHarvestNodeState& NewState);
	FRpgHarvestStockSnapshot MakeStockSnapshot() const;
	void BroadcastStateChanged(bool bInitialState);
	float GetServerWorldTimeSeconds() const;
	void ScheduleRespawn();
	void HandleRespawnTimer();

	/** Server-authored stock; replicated to every client for presentation and target previews. */
	UPROPERTY(ReplicatedUsing = OnRep_HarvestState)
	FRpgHarvestNodeState HarvestState;

	/** Server-only guard that rejects delegate-driven re-entry while one commit is in progress. */
	bool bCommitInProgress = false;

	/** Server-only wakeup for the pending respawn; no tick is used. */
	FTimerHandle RespawnTimerHandle;
};
