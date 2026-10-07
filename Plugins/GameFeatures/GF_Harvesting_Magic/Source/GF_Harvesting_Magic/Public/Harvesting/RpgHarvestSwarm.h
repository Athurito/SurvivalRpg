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
	 * Creatures summoned per activation, 1 to 16. Every strike of a creature takes the ability's SectionsPerTarget
	 * sections; the creatures keep working until the selected resources are empty.
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
	 * Seconds a creature rests at a resource after a strike before it strikes again or leaves for the next resource.
	 * Longer rests make the swarm work through its area more slowly.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0", Units = "s"))
	float StrikeIntervalSeconds = 0.6f;

	/**
	 * Radius in centimeters around each strike in which every other resource the swarm works on is struck too, once per
	 * strike and with the strike's sections. Stock other creatures reserved is left to them. Zero strikes only the
	 * creature's own resource.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "800.0", Units = "cm"))
	float StrikeRadius = 0.0f;

	/**
	 * How often one creature may arrive at a resource that was emptied, removed or protected meanwhile before it gives
	 * up. After each such miss it heads for the nearest selected resource with stock no other creature reserved.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "0", ClampMax = "8", UIMin = "0", UIMax = "4"))
	int32 MaxReassignments = 2;

	/** Seconds after the summon at which every creature that has not finished dissipates. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Swarm", meta = (ClampMin = "0.5", UIMin = "2.0", UIMax = "30.0", Units = "s"))
	float MaxLifetimeSeconds = 20.0f;

	/**
	 * When true, the swarm only takes resources it can see from the summon point. Walls, buildings and terrain block
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

	/** On its way to a reserved resource, or resting there before its next strike; it harvests when it arrives. */
	Flying,

	/** Finished after harvesting at least once: nothing it may take is left. */
	Harvested,

	/** Finished without harvesting: nothing left to take, the summoner died, or the swarm expired. */
	Dissipated
};

/** Replicated flight of one swarm creature, timed in server world seconds so every machine shows it at the same time. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestSwarmCreature
{
	GENERATED_BODY()

	/** World-space start of the current leg in centimeters: the summon ring, or the resource it last struck. */
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

	/** Number of the current leg; it grows each time the creature heads for a resource again. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	uint8 Leg = 0;

	/** Successful strikes so far. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	uint8 Strikes = 0;

	/** Stock sections the creature harvested so far, over all its strikes. */
	UPROPERTY(BlueprintReadOnly, Category = "Swarm")
	uint8 SectionsTaken = 0;

	/** Returns whether the creature finished, with or without harvesting. */
	bool IsFinished() const
	{
		return State == ERpgHarvestSwarmCreatureState::Harvested || State == ERpgHarvestSwarmCreatureState::Dissipated;
	}
};

/**
 * Stateless helpers for the swarm. The ability preview and the authoritative swarm share them, so both agree on which
 * resources the swarm takes and how its creatures start.
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
 * A short-lived swarm a harvest ability summons at its aim point. Its creatures rise and work through the resources the
 * ability selected, resource by resource, until none of their stock is left. They harvest for the summoner and need
 * no orders.
 *
 * The server owns the swarm:
 * - Creatures start with FRpgHarvestSwarmPlanner's assignment. After every strike a creature rests briefly, then strikes
 *   the nearest selected resource that still has stock no other creature reserved, so the swarm never overbooks a
 *   resource. Reservations are not locks: other players keep harvesting, and only the stock left when a creature
 *   arrives counts.
 * - Every strike commits exactly once through IRpgHarvestableTarget, like any harvest. The player's player state is the
 *   request's beneficiary (rewards, XP, skill gate) and the swarm is its physical harvester (felling direction).
 * - With a StrikeRadius, a strike also commits every other resource of the swarm within it exactly once, taking only
 *   stock no other creature reserved.
 * - A strike that depletes a resource inside a URpgHarvestChainComponent box takes the box's other resources along.
 * - A creature that arrives at a resource emptied, removed or protected meanwhile heads for another selected resource,
 *   or gives up.
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
	 * RequestTemplate carries the ability id, tool, power and the sections each strike takes. The swarm works through
	 * the Targets that would be harvested, nearest to the aim point first, and through no others. Returns false when the
	 * swarm was already started or cannot start.
	 */
	bool StartSwarm(
		AActor* Summoner,
		const FRpgHarvestRequest& RequestTemplate,
		const FRpgHarvestSwarmParams& Params,
		const TArray<FRpgHarvestTargetEvaluation>& Targets);

	/** Returns the replicated creatures. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Swarm")
	const TArray<FRpgHarvestSwarmCreature>& GetCreatures() const { return Creatures; }

	/** Returns where CreatureIndex is at the current server time, the same on every machine. False for invalid indices. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Swarm")
	bool GetCreatureLocation(int32 CreatureIndex, FVector& OutLocation) const;

	/** Returns where CreatureIndex is at ServerTime: rising at its start, on its arc, or at its strike point. */
	FVector GetCreatureLocationAt(int32 CreatureIndex, double ServerTime) const;

	/** Returns whether every creature finished. Replicated through the creatures. */
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

	/**
	 * Radius in centimeters around each strike in which the swarm's other resources are struck too; zero for single
	 * strikes. Comes from the summoning ability and is replicated, so a strike presentation can scale to it.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Swarm")
	float GetStrikeRadius() const { return StrikeRadius; }

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
	 * Cosmetic: a creature was sent to a resource, at the summon or after a strike or a miss; it leaves From at the
	 * leg's launch time, after a strike once it rested. Called on every machine with a local player. CreatureActor is the
	 * spawned CreatureClass actor and may be null.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Harvesting|Swarm", DisplayName = "On Creature Launched")
	void K2_OnCreatureLaunched(int32 CreatureIndex, AActor* CreatureActor, FVector From, FVector To);

	/**
	 * Cosmetic: a creature struck a resource at Location and harvested it. Called on every machine with a local player,
	 * once per replicated change; SectionsTaken is the creature's total so far. The stock and rewards are already
	 * resolved on the server.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Harvesting|Swarm", DisplayName = "On Creature Struck")
	void K2_OnCreatureStruck(int32 CreatureIndex, AActor* CreatureActor, FVector Location, int32 SectionsTaken);

	/**
	 * Cosmetic: a creature finished at Location, after harvesting (bHarvested) or without. Called on every machine with a
	 * local player; the creature actor is destroyed CreatureLingerSeconds later.
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

	/** Seconds a finished creature's actor stays for its dissipation presentation. Cosmetic. */
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
		int32 Misses = 0;
	};

	/** Server-only resource the swarm works on. */
	struct FWorkTarget
	{
		TWeakObjectPtr<UObject> Receiver;
		FHitResult Hit;
	};

	UFUNCTION()
	void OnRep_Creatures();

	/** Server: processes every creature whose departure or arrival is due, then waits for the next one. */
	void Step();
	void ScheduleStep();
	void Strike(int32 CreatureIndex, double Now);

	/**
	 * Server: sends CreatureIndex from FromLocation, leaving at LaunchServerTime, to the nearest work target with stock
	 * no other creature reserved. Returns false when nothing is left to take.
	 */
	bool AssignNextTarget(int32 CreatureIndex, const FVector& FromLocation, double LaunchServerTime);

	/**
	 * Server: stock sections of the target Hit addresses on Receiver that no creature other than CreatureIndex reserved,
	 * evaluated from FromLocation as the target is now. Zero when the target cannot be harvested.
	 */
	int32 GetUnreservedSections(int32 CreatureIndex, UObject* Receiver, const FHitResult& Hit, const FVector& FromLocation) const;

	/**
	 * Server: strikes every work target other than the struck one within StrikeRadius of StruckHit once, with the
	 * strike's sections but never stock other creatures reserved. The reward batch must be open. Appends every
	 * successful commit to OutCommitted and returns the sections taken.
	 */
	int32 StrikeAround(
		int32 CreatureIndex,
		const UObject* StruckReceiver,
		const FHitResult& StruckHit,
		const FVector& FromLocation,
		TArray<FRpgHarvestTargetEvaluation>& OutCommitted);
	void Finish(int32 CreatureIndex, double Now);
	void FinishAll();
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

	/** Strike radius in centimeters from the summoning ability's swarm parameters; replicated for presentation. */
	UPROPERTY(Replicated)
	float StrikeRadius = 0.0f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CreatureActors;

	/** Last creature leg, strike count and phase each machine presented. */
	TArray<uint8> PresentedLegs;
	TArray<uint8> PresentedStrikes;
	TArray<ERpgHarvestSwarmCreatureState> PresentedStates;

	/** Server: request every strike starts from; its Harvester is the beneficiary and PhysicalHarvester this swarm. */
	UPROPERTY(Transient)
	FRpgHarvestRequest RequestTemplate;

	TArray<FAssignment> Assignments;
	TArray<FWorkTarget> WorkTargets;
	FRpgHarvestSwarmParams SwarmParams;
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
