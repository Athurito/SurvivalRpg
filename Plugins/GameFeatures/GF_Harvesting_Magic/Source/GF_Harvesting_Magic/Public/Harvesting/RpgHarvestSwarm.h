#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Harvesting/RpgHarvestTargeting.h"
#include "TimerManager.h"

#include "RpgHarvestSwarm.generated.h"

class FRpgHarvestRewardBatch;
class UAbilitySystemComponent;
struct FCollisionQueryParams;

/** Designer-tuned behavior of the swarm a harvest ability summons. Static data on the ability. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestSwarmParams
{
	GENERATED_BODY()

	/**
	 * Creatures summoned per activation, 1 to 16. Each takes the ability's SectionsPerTarget sections from one resource.
	 * The creature count also caps how many resources the area preview selects.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "16"))
	int32 CreatureCount = 6;

	/** Flight speed of a creature in cm/s; it decides when a creature strikes and so when its resource is harvested. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "50.0", UIMin = "200.0", UIMax = "3000.0", ForceUnits = "cm/s"))
	float FlightSpeed = 900.0f;

	/** Seconds from the summon until the first creature leaves; the creatures rise from the ground meanwhile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0", Units = "s"))
	float EmergeSeconds = 0.6f;

	/** Seconds between the departures of consecutive creatures, so the swarm fans out instead of leaving as one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "0.5", Units = "s"))
	float LaunchIntervalSeconds = 0.12f;

	/**
	 * How often one creature may head for another resource when its own was emptied or removed before it arrived.
	 * A creature that finds nothing left to take dissipates.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "0", ClampMax = "8", UIMin = "0", UIMax = "4"))
	int32 MaxReassignments = 2;

	/** Seconds after the summon at which every creature that has not struck yet dissipates. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "0.5", UIMin = "2.0", UIMax = "20.0", Units = "s"))
	float MaxLifetimeSeconds = 8.0f;

	/**
	 * When true, creatures only take resources they can see from the summon point. Walls, buildings and terrain block
	 * them; other resources and pawns do not. Blocked resources are previewed as out of reach.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm")
	bool bRequireLineOfSight = true;
};

/** Phase of one swarm creature. Replicated; the presentation follows it. */
UENUM(BlueprintType)
enum class ERpgHarvestSwarmCreatureState : uint8
{
	/** Rising at its start without a resource; it looks for one when it would leave and dissipates if none is left. */
	Searching,

	/** On its way to a reserved resource; it harvests the resource when it arrives. */
	Flying,

	/** Struck its resource and harvested it. Finished. */
	Struck,

	/** Finished without harvesting: nothing left to take, the summoner died, or the swarm expired. */
	Dissipated
};

/** Replicated flight of one swarm creature, timed in server world seconds so every machine shows it at the same time. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestSwarmCreature
{
	GENERATED_BODY()

	/** World-space start of the current leg in centimeters. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	FVector_NetQuantize10 From = FVector::ZeroVector;

	/** World-space strike point of the current leg in centimeters; equals From while searching. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	FVector_NetQuantize10 To = FVector::ZeroVector;

	/** Server world time in seconds at which the creature leaves From. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	double LaunchServerTime = 0.0;

	/** Server world time in seconds at which the creature reaches To and strikes. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	double ArrivalServerTime = 0.0;

	/** Current phase. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	ERpgHarvestSwarmCreatureState State = ERpgHarvestSwarmCreatureState::Searching;

	/** Number of the current leg; it grows each time the creature heads for another resource. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	uint8 Leg = 0;

	/** Stock sections the creature harvested; valid once it struck. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	uint8 SectionsTaken = 0;

	/** Returns whether the creature struck or dissipated. */
	bool IsFinished() const
	{
		return State == ERpgHarvestSwarmCreatureState::Struck || State == ERpgHarvestSwarmCreatureState::Dissipated;
	}
};

/**
 * Stateless distribution of swarm creatures over resources. The ability preview and the authoritative swarm share it,
 * so the sections the preview shows are the sections the swarm reserves.
 */
struct GF_HARVESTING_MAGIC_API FRpgHarvestSwarmPlanner
{
	/**
	 * Assigns up to CreatureCount creatures to the Targets that would be harvested, in their order (nearest first). Every
	 * creature reserves up to SectionsPerCreature sections of one target. Creatures cover a target's remaining stock
	 * before the next target gets any, so several creatures share a large resource but never reserve more than its
	 * remaining stock. OutTargetIndices and OutSections get one entry per assigned creature; creatures beyond them found
	 * nothing to reserve.
	 */
	static void Distribute(
		const TArray<FRpgHarvestTargetEvaluation>& Targets,
		int32 CreatureCount,
		int32 SectionsPerCreature,
		TArray<int32>& OutTargetIndices,
		TArray<int32>& OutSections);

	/** Returns the stock sections Target has before the evaluated request takes any. */
	static int32 GetAvailableSections(const FRpgHarvestTargetEvaluation& Target);

	/**
	 * Returns whether a straight line from Origin reaches StrikePoint. Harvestable resources and pawns on the way do not
	 * block it; any other blocking hit on Channel does.
	 */
	static bool HasLineOfSight(
		const UWorld& World,
		const FVector& Origin,
		const FVector& StrikePoint,
		ECollisionChannel Channel,
		const FCollisionQueryParams& QueryParams);
};

/**
 * A short-lived swarm a harvest ability summons at its aim point. Its creatures rise, spread over the resources in
 * the ability's area, and harvest each resource when they arrive, for the summoner. They need no orders.
 *
 * The server owns the swarm:
 * - Creatures reserve stock with FRpgHarvestSwarmPlanner, so the swarm never overbooks a resource. Reservations are
 *   not locks: other players keep harvesting, and only the stock left when a creature arrives counts.
 * - A creature commits exactly once through IRpgHarvestableTarget, like any harvest. The player's player state is the
 *   request's beneficiary (rewards, XP, skill gate) and the swarm is its physical harvester (felling direction).
 * - A creature whose resource was emptied before it arrived heads for another one with unreserved stock in the area,
 *   or dissipates.
 * - The rewards of all strikes reach the player as one delivery when the last creature finishes: into the inventory,
 *   or one drop at the player's feet.
 * - The swarm keeps working when the player switches tools or the ability ends. It ends early, without further
 *   harvests, when the summoner dies or is gone; rewards already harvested are still delivered.
 *
 * Every machine with a local player spawns a non-replicated CreatureClass actor per creature and moves it along the
 * replicated flight at server time. Subclass it in Blueprint for the presentation.
 */
UCLASS(Blueprintable)
class GF_HARVESTING_MAGIC_API ARpgHarvestSwarm : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestSwarm(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~ARpgHarvestSwarm() override;

	/** Most creatures one swarm can have. */
	static constexpr int32 MaxCreatures = 16;

	/**
	 * Server only. Starts the swarm at its location for Summoner, whose player state becomes the beneficiary.
	 * RequestTemplate carries the ability id, tool, power and the sections each creature takes. InitialTargets are the
	 * evaluated targets the ability selected, nearest to the aim point first; SearchRadius and Channel bound later
	 * searches. Returns false when the swarm was already started or cannot start.
	 */
	bool StartSwarm(
		AActor* Summoner,
		const FRpgHarvestRequest& RequestTemplate,
		const FRpgHarvestSwarmParams& Params,
		float SearchRadius,
		ECollisionChannel Channel,
		const TArray<FRpgHarvestTargetEvaluation>& InitialTargets);

	/** Returns the replicated creatures. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Swarm")
	const TArray<FRpgHarvestSwarmCreature>& GetCreatures() const { return Creatures; }

	/** Returns where CreatureIndex is at the current server time, the same on every machine. False for invalid indices. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Swarm")
	bool GetCreatureLocation(int32 CreatureIndex, FVector& OutLocation) const;

	/** Returns where CreatureIndex is at ServerTime: rising at its start, on its arc, or at its strike point. */
	FVector GetCreatureLocationAt(int32 CreatureIndex, double ServerTime) const;

	/** Returns whether every creature struck or dissipated. Replicated through the creatures. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Swarm")
	bool IsFinished() const;

	/** Server: stock sections all creatures harvested so far. */
	int32 GetHarvestedSections() const { return HarvestedSections; }

	/** Server: how the swarm's rewards reached the beneficiary; None until the swarm delivered. */
	ERpgHarvestDelivery GetDelivery() const { return Delivery; }

	/** Server: the actor that receives the rewards, normally the summoner's player state. */
	AActor* GetBeneficiary() const { return RequestTemplate.Harvester; }

	/** Height in centimeters above a resource's location at which creatures strike it. */
	float GetStrikeHeight() const { return StrikeHeight; }

	/** Returns the current server world time in seconds, or the world time without a game state. */
	double GetServerWorldTimeSeconds() const;

protected:
	//~ AActor interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	/**
	 * Cosmetic: a creature left for a resource, at its first departure or when it heads for another one. Called on
	 * every machine with a local player. CreatureActor is the spawned CreatureClass actor and may be null.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Harvesting|Swarm", DisplayName = "On Creature Launched")
	void K2_OnCreatureLaunched(int32 CreatureIndex, AActor* CreatureActor, FVector From, FVector To);

	/**
	 * Cosmetic: a creature struck its resource (bHarvested) or dissipated, at Location. Called on every machine with a
	 * local player; the creature actor is destroyed CreatureLingerSeconds later. The stock and rewards are already
	 * resolved on the server.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Harvesting|Swarm", DisplayName = "On Creature Finished")
	void K2_OnCreatureFinished(int32 CreatureIndex, AActor* CreatureActor, FVector Location, bool bHarvested);

	/** Cosmetic actor spawned per creature on every machine with a local player; never replicated. Empty shows none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Swarm")
	TSubclassOf<AActor> CreatureClass;

	/** Height in centimeters of a creature's flight arc above the straight line between two points. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "600.0", Units = "cm"))
	float FlightArcHeight = 180.0f;

	/** Height in centimeters above a resource's location at which creatures strike it; also the height of their sight line. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "400.0", Units = "cm"))
	float StrikeHeight = 120.0f;

	/** Radius in centimeters of the ring around the summon point on which the creatures rise. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "300.0", Units = "cm"))
	float EmergeRingRadius = 90.0f;

	/** Depth in centimeters below their start from which the creatures rise. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "400.0", Units = "cm"))
	float EmergeDepth = 160.0f;

	/** Seconds a finished creature's actor stays for its strike or dissipation presentation. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0", Units = "s"))
	float CreatureLingerSeconds = 0.6f;

private:
	/** Server-only reservation of one creature. */
	struct FAssignment
	{
		TWeakObjectPtr<UObject> Receiver;
		FHitResult Hit;
		int32 ExpectedRevision = INDEX_NONE;
		int32 ReservedSections = 0;
		int32 Reassignments = 0;
	};

	UFUNCTION()
	void OnRep_Creatures();

	/** Server: processes every creature whose departure or arrival is due, then waits for the next one. */
	void Step();
	void ScheduleStep();
	void Strike(int32 CreatureIndex, double Now);
	bool Reassign(int32 CreatureIndex, const FVector& FromLocation, double Now);
	void Dissipate(int32 CreatureIndex, double Now);
	void DissipateAll();
	void FinishIfDone();
	bool IsSummonerAlive() const;
	void HandleSummonerDeathTagChanged(FGameplayTag Tag, int32 NewCount);
	void ReplicateCreatures();
	void ReleaseSummoner();
	void DeliverRewards();
	void Shutdown(bool bDeliverRewards);
	bool ShouldPresent() const;

	/** Every machine with a local player: spawns, announces and retires creature actors after a creature change. */
	void UpdatePresentation();

	UPROPERTY(ReplicatedUsing = OnRep_Creatures)
	TArray<FRpgHarvestSwarmCreature> Creatures;

	/** Server world time of the summon; the creatures of the first leg rise from it until they leave. */
	UPROPERTY(Replicated)
	double SummonServerTime = 0.0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CreatureActors;

	/** Last creature leg and phase each machine presented. */
	TArray<uint8> PresentedLegs;
	TArray<ERpgHarvestSwarmCreatureState> PresentedStates;

	/** Server: request every strike starts from; its Harvester is the beneficiary and PhysicalHarvester this swarm. */
	UPROPERTY(Transient)
	FRpgHarvestRequest RequestTemplate;

	TArray<FAssignment> Assignments;
	FRpgHarvestSwarmParams SwarmParams;
	float SearchRadius = 0.0f;
	TEnumAsByte<ECollisionChannel> SearchChannel = ECC_Visibility;
	TWeakObjectPtr<AActor> Summoner;
	TWeakObjectPtr<UAbilitySystemComponent> SummonerAbilitySystem;
	FDelegateHandle DeathTagHandle;
	TUniquePtr<FRpgHarvestRewardBatch> RewardBatch;
	FTimerHandle StepTimerHandle;
	double ExpireServerTime = 0.0;
	int32 HarvestedSections = 0;
	ERpgHarvestDelivery Delivery = ERpgHarvestDelivery::None;
	bool bStarted = false;
	bool bDelivered = false;
};
