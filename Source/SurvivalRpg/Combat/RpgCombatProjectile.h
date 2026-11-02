#pragma once

#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"

#include "RpgCombatProjectile.generated.h"

class APawn;
class ARpgEmbeddedProjectilePickup;
class URpgEmbeddedProjectileHostComponent;
class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UPhysicalMaterial;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class URpgInventoryItemDefinition;
class USoundBase;
class USphereComponent;

/** What becomes of the projectile's ammunition when it lands. */
UENUM(BlueprintType)
enum class ERpgProjectileImpactRecovery : uint8
{
	/** The server rolls once at impact whether the projectile stays intact and can be recovered (standard arrows). */
	Recoverable,
	/** Burns, shatters or dissolves on impact: no break roll, embedding, pickup or recovery credit (magic arrows, spells). */
	ConsumedOnImpact,
};

/** What the server's impact made of a recoverable projectile's ammunition. Replicated with the stop; presentation only. */
UENUM(BlueprintType)
enum class ERpgProjectileRecoveryOutcome : uint8
{
	/** Nothing to recover: consumed on impact, no recovery credit, a character hit, or an expired flight. */
	None,
	/** The break roll broke the projectile; it grants no ammunition. */
	Broken,
	/** Intact and embedded where it hit, as a world pickup that follows a moving hit component. */
	Embedded,
	/** Intact but unable to embed, such as off a shield or an unsuitable surface: a pickup on the ground near the contact. */
	Dropped,
};

/** How one physical material changes recoverable projectiles that hit it. Designer data. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgProjectileSurfaceRecoveryRule
{
	GENERATED_BODY()

	/** Physical material of the hit surface this rule applies to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	TObjectPtr<UPhysicalMaterial> PhysicalMaterial;

	/** Factor on the launch's break chance for hits on this material; the result is clamped to 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0"))
	float BreakChanceMultiplier = 1.0f;

	/** Whether an intact projectile can embed in this material. Otherwise it drops to the ground near the contact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	bool bCanEmbed = true;
};

/**
 * How a recoverable projectile class leaves its ammunition in the world (ranged combat roadmap RNG-03b). Designer data
 * on the projectile class; the server reads it once at the impact.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgProjectileRecoveryDefinition
{
	GENERATED_BODY()

	/** Pickup an intact projectile leaves, embedded or on the ground. Required for recovery; none recovers nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	TSubclassOf<ARpgEmbeddedProjectilePickup> PickupClass;

	/**
	 * Chance from 0 to 1 that the projectile breaks at its impact; zero never breaks. The launching ability tunes it once
	 * per activation through Ability.Tuning.Ranged.BreakChance; surface rules then scale it at the impact.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseBreakChance = 0.0f;

	/** Per-material break chance factors and embedding; a material without a rule embeds with the unscaled chance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	TArray<FRpgProjectileSurfaceRecoveryRule> SurfaceRules;

	/** Seconds after the impact until an uncollected pickup expires with its remaining ammunition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "1.0", Units = "Seconds"))
	float RecoveryLifetimeSeconds = 600.0f;

	/**
	 * Separate pickups one hit component holds before further intact projectiles join the nearest pickup of the same
	 * ammunition on it within GroupRadius instead of adding an actor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "1"))
	int32 MaxSeparatePickupsPerHost = 10;

	/** Distance in cm within which a projectile joins an existing pickup once its hit component holds the maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float GroupRadius = 100.0f;

	/**
	 * Shafts one AI character shows at most. Further intact projectiles join the nearest shaft, preferring the same
	 * bone, and still count in full toward its corpse loot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "1"))
	int32 MaxVisibleArrowsPerCharacter = 10;

	/** Largest fall in cm from the contact to the ground for a dropped projectile; without ground in reach it is lost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float MaxDropHeight = 500.0f;
};

/** How the server resolved a projectile's flight. Replicated with the stop for presentation; content only reads it. */
UENUM(BlueprintType)
enum class ERpgProjectileImpactResult : uint8
{
	/** Still in flight. */
	None,
	/** Hit world geometry or an actor without an ability system. */
	World,
	/** Hit a target and applied its damage; armor may still have reduced it to nothing. */
	Hit,
	/** A paid normal block reduced the damage. */
	Blocked,
	/** A perfect block stopped all damage. */
	PerfectBlocked,
	/** Hit a target the damage did not affect, for example an ally. */
	Unaffected,
	/** The flight time ran out without an impact. */
	Expired,
};

/**
 * Per-shot values the launching ability resolves once on the server. The projectile keeps them for its whole flight and
 * never reads live tuning, equipment or the shooter's pawn again.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgProjectileLaunchParams
{
	GENERATED_BODY()

	/** World launch point in cm, such as the bow or the casting hand. The first collision sweep starts here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FVector Origin = FVector::ZeroVector;

	/** World flight direction at launch; normalized by the launch. A zero vector fails the launch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FVector Direction = FVector::ForwardVector;

	/** Launch speed in cm/s; zero uses the projectile class's DefaultSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float Speed = 0.0f;

	/** SetByCaller.Damage of the damage spec. The damage execution still adds source BaseDamage and applies armor, crit and block. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0"))
	float Damage = 0.0f;

	/** SetByCaller.StaggerDamage of the damage spec, in posture points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (ClampMin = "0.0"))
	float StaggerDamage = 0.0f;

	/**
	 * Optional shot identity chosen by the launching ability, replicated once so client presentation can match the shot
	 * it predicted. Zero means none. Never used for authority.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	int32 ShotId = 0;

	/**
	 * Ammunition an intact recoverable projectile returns. Native only: the shot commit sets it when the launch consumed
	 * a real item, such as an arrow from the shooter's inventory, so no Blueprint launch can create ammunition. None
	 * grants no recovery credit.
	 */
	UPROPERTY()
	TSubclassOf<URpgInventoryItemDefinition> RecoveryItemDefinition;

	/**
	 * Chance from 0 to 1 that a recoverable projectile breaks, resolved once by the launching ability with its passive
	 * tuning. Negative uses the projectile class's BaseBreakChance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Recovery", meta = (ClampMax = "1.0"))
	float BreakChance = -1.0f;
};

/** The replicated end of a flight: where it stopped and how the server resolved it. */
USTRUCT()
struct FRpgProjectileImpactState
{
	GENERATED_BODY()

	/** None while the projectile flies; set once by the server. */
	UPROPERTY()
	ERpgProjectileImpactResult Result = ERpgProjectileImpactResult::None;

	/** World location of the projectile's center when it stopped, in cm. */
	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	/** Surface normal at the impact point; the reversed flight direction when the flight expired. */
	UPROPERTY()
	FVector_NetQuantizeNormal Normal = FVector::UpVector;

	/** What the impact made of the ammunition. */
	UPROPERTY()
	ERpgProjectileRecoveryOutcome Recovery = ERpgProjectileRecoveryOutcome::None;

	bool HasStopped() const { return Result != ERpgProjectileImpactResult::None; }
};

/**
 * The one shared projectile for arrows and spells (ranged combat roadmap RNG-01).
 *
 * The server owns collision, the single impact and its damage. Each frame it sweeps the flight segment on
 * Rpg_TraceChannel_Projectile, so the projectile never blocks pawn movement. At the first valid hit it applies the
 * damage spec built at launch to the target (Damage.Type.Projectile, so shields can block it from the incoming flight
 * direction), executes ImpactCueTag through the GameState ability system and replicates the stop. Clients only simulate
 * and smooth the flight and stop the presentation when the stop arrives. Damage, attribution, faction and launch origin
 * are captured at launch, so a weapon swap, the shooter's death or a respawn never changes a projectile in flight.
 *
 * Launching needs no equipment or inventory: any ability, NPC or system can launch one. Blueprint subclasses set the
 * class data below and attach their flight presentation under VisualRoot; when the flight stops, the projectile hides
 * its meshes, deactivates every FX system and fades every audio component on its own.
 *
 * A Recoverable projectile launched with a RecoveryItemDefinition returns its ammunition: the server rolls once at the
 * impact whether it broke (RNG-03b). An intact one at a world impact or a block leaves an ARpgEmbeddedProjectilePickup,
 * embedded in the hit surface or dropped near a shield. In an AI character, or its corpse, it lodges as corpse loot of
 * that character's URpgEmbeddedProjectileHostComponent, recorded before its damage applies (RNG-03c). A player
 * character never keeps one: the hit only plays its feedback.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgCombatProjectile : public AActor, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ARpgCombatProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Server: launches a projectile from the calling ability's current activation. Builds the damage spec from the class's
	 * DamageEffectClass with the ability's effect context (instigator, source object), so the ability needs no equipment.
	 * Returns null on clients, without an active ability, or when the launch fails.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Combat|Projectile",
		meta = (DefaultToSelf = "LaunchingAbility", HidePin = "LaunchingAbility"))
	static ARpgCombatProjectile* LaunchFromAbility(
		UGameplayAbility* LaunchingAbility,
		TSubclassOf<ARpgCombatProjectile> ProjectileClass,
		const FRpgProjectileLaunchParams& Params);

	/**
	 * Server: launches a projectile with a damage spec the caller built, for launches outside an ability. The projectile
	 * takes over DamageSpec and completes it (SetByCaller values, damage tags, itself as effect causer), so pass a new spec
	 * per projectile. The spec context's instigator decides attribution and faction. Returns null on clients or on failure.
	 */
	static ARpgCombatProjectile* LaunchWithDamageSpec(
		UWorld* World,
		TSubclassOf<ARpgCombatProjectile> ProjectileClass,
		const FRpgProjectileLaunchParams& Params,
		const FGameplayEffectSpecHandle& DamageSpec,
		APawn* InstigatorPawn);

	/**
	 * Server: the first half of LaunchFromAbility. Spawns and prepares the projectile but does not finish spawning it, so
	 * a caller can commit resources in between. Nothing begins play or replicates until FinishDeferredLaunch, and
	 * AbortDeferredLaunch removes it without a trace. Returns null on clients or on failure.
	 */
	static ARpgCombatProjectile* BeginLaunchFromAbility(
		UGameplayAbility* LaunchingAbility,
		TSubclassOf<ARpgCombatProjectile> ProjectileClass,
		const FRpgProjectileLaunchParams& Params);

	/** Server: the first half of LaunchWithDamageSpec; see BeginLaunchFromAbility. */
	static ARpgCombatProjectile* BeginLaunchWithDamageSpec(
		UWorld* World,
		TSubclassOf<ARpgCombatProjectile> ProjectileClass,
		const FRpgProjectileLaunchParams& Params,
		const FGameplayEffectSpecHandle& DamageSpec,
		APawn* InstigatorPawn);

	/** Server: finishes a deferred launch. Returns false when the projectile did not survive its BeginPlay. */
	bool FinishDeferredLaunch();

	/** Server: destroys a deferred launch before it began play or replicated. */
	void AbortDeferredLaunch();

	//~ IGenericTeamAgentInterface
	/** The shooter's faction captured at launch (server only), so faction checks outlive the shooter. */
	virtual FGenericTeamId GetGenericTeamId() const override;
	//~ End IGenericTeamAgentInterface

	//~ AActor
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	//~ End AActor

	/** World launch point in cm; replicated once. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Combat|Projectile")
	FVector GetLaunchOrigin() const { return LaunchOrigin; }

	/** The launching ability's shot identity, or zero; replicated once. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Combat|Projectile")
	int32 GetShotId() const { return ShotId; }

	/** How the flight ended, or None while it flies; replicated. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Combat|Projectile")
	ERpgProjectileImpactResult GetImpactResult() const { return ImpactState.Result; }

	/** The ammunition recovery policy of this projectile class. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Combat|Projectile")
	ERpgProjectileImpactRecovery GetImpactRecovery() const { return ImpactRecovery; }

	/** What the impact made of the ammunition, or None while the projectile flies; replicated. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Combat|Projectile")
	ERpgProjectileRecoveryOutcome GetRecoveryOutcome() const { return ImpactState.Recovery; }

	/** The class's recovery data; only a Recoverable class uses it. */
	const FRpgProjectileRecoveryDefinition& GetRecoveryDefinition() const { return Recovery; }

	/** Server: the intact projectile's pickup, or null when the impact left none or it is gone. */
	ARpgEmbeddedProjectilePickup* GetRecoveredPickup() const { return RecoveredPickup.Get(); }

	/** Server: the record id in the character that keeps this projectile, or zero. */
	int32 GetCharacterRecordId() const { return CharacterRecordId; }

	UProjectileMovementComponent* GetProjectileMovement() const { return ProjectileMovement; }
	TSubclassOf<UGameplayEffect> GetDamageEffectClass() const { return DamageEffectClass; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Runs the server's impact resolution for Hit without a flight, as a sweep would. */
	void ResolveImpactForTests(const FHitResult& Hit) { ResolveImpact(Hit); }
	/** Runs the server's filtered flight sweep from Start to End. */
	bool SweepFlightSegmentForTests(const FVector& Start, const FVector& End, FHitResult& OutHit) { return SweepFlightSegment(Start, End, OutHit); }
	const FGameplayEffectSpecHandle& GetDamageSpecForTests() const { return DamageSpec; }
	FGameplayTag GetImpactCueTagForTests() const { return ImpactCueTag; }
	FVector GetImpactLocationForTests() const { return ImpactState.Location; }
	float GetMaxFlightSecondsForTests() const { return MaxFlightSeconds; }
	void SetImpactCueForTests(const FGameplayTag& CueTag, bool bOnTargets) { ImpactCueTag = CueTag; bImpactCueOnTargets = bOnTargets; }
	/** Replaces the break roll's random value from 0 to 1; the projectile stays intact when the value reaches the chance. */
	void SetBreakRollForTests(float Roll) { BreakRollForTests = Roll; }
	float GetBreakChanceForTests() const { return LaunchBreakChance; }
	TSubclassOf<URpgInventoryItemDefinition> GetRecoveryItemDefinitionForTests() const { return RecoveryItemDefinition; }
#endif

protected:
	/**
	 * Server hook just before the impact damage applies to TargetASC. Runs once, before a lethal hit can kill the target,
	 * so ammunition recovery can register its records first.
	 */
	virtual void PreApplyImpactDamage(const FHitResult& Hit, UAbilitySystemComponent* TargetASC) {}

	/** Every machine, once: the flight stopped at Location with Result. Use it for content such as hiding a mesh. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Combat|Projectile", meta = (DisplayName = "On Flight Stopped"))
	void K2_OnFlightStopped(ERpgProjectileImpactResult Result, FVector Location, FVector Normal);

	/** Root, collision-free on every machine; its radius shows CollisionRadius in the editor. The projectile movement moves it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionSphere;

	/**
	 * Parent for all flight presentation (meshes, Niagara, audio). Clients smooth network corrections through it. Nothing
	 * under it collides: the projectile disables its actor collision, so only the server's sweep detects impacts.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USceneComponent> VisualRoot;

	/** Integrates the ballistic flight on every machine. Only the server resolves collisions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Instant damage effect the launch builds its spec from, usually GE_Damage_Instant (RpgDamageExecution). Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Extra damage asset tags, such as an element. Damage.Type.Projectile is always added. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage", meta = (Categories = "Damage.Type"))
	FGameplayTagContainer DamageTypeTags;

	/** Optional body-hit cue (GameplayCue.Combat.Hit.*) the damage execution plays on a damaged target. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage", meta = (Categories = "GameplayCue.Combat.Hit"))
	FGameplayTag HitCueTag;

	/** Speed in cm/s when the launch passes none. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Flight", meta = (ClampMin = "1.0", Units = "CentimetersPerSecond"))
	float DefaultSpeed = 5000.0f;

	/** Multiplier of world gravity: 1 for arrows, 0 for a straight spell. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Flight", meta = (ClampMin = "0.0"))
	float GravityScale = 1.0f;

	/** Radius in cm of the server's collision sweep. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Flight", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float CollisionRadius = 2.0f;

	/** Seconds the flight may last before it expires without an impact. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Flight", meta = (ClampMin = "0.1", Units = "Seconds"))
	float MaxFlightSeconds = 10.0f;

	/** Seconds the stopped projectile stays before the server destroys it, so trails and sounds can fade. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Flight", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ImpactLingerSeconds = 2.0f;

	/** Seconds the audio components take to fade out when the flight stops. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation", meta = (ClampMin = "0.0", Units = "Seconds"))
	float AudioFadeOutSeconds = 0.25f;

	/**
	 * One of these plays at the launch point on every machine that receives the projectile in flight within half a second
	 * of its launch; a late joiner hears none. Designer data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
	TArray<TObjectPtr<USoundBase>> LaunchSounds;

	/**
	 * Cue the server executes at every impact through the GameState ability system, so it plays on every machine even
	 * after the shooter died. Location and Normal are the impact point and surface normal. Designer data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation", meta = (Categories = "GameplayCue"))
	FGameplayTag ImpactCueTag;

	/** RawMagnitude of the impact cue, for example a blast radius in cm. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
	float ImpactCueMagnitude = 0.0f;

	/** Whether the impact cue also plays when the projectile hits a target with an ability system. Off for arrows, whose body-hit cue covers it. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Presentation")
	bool bImpactCueOnTargets = true;

	/** Ammunition recovery policy of this projectile class. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Recovery")
	ERpgProjectileImpactRecovery ImpactRecovery = ERpgProjectileImpactRecovery::ConsumedOnImpact;

	/** How an intact projectile of this class is recovered. Designer data; read only under Recoverable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Recovery",
		meta = (EditCondition = "ImpactRecovery == ERpgProjectileImpactRecovery::Recoverable"))
	FRpgProjectileRecoveryDefinition Recovery;

private:
	void InitializeLaunch(const FRpgProjectileLaunchParams& Params, const FVector& Direction, const FGameplayEffectSpecHandle& InDamageSpec);
	bool SweepFlightSegment(const FVector& Start, const FVector& End, FHitResult& OutHit);
	bool ShouldPassThrough(const FHitResult& Hit) const;
	void ResolveImpact(const FHitResult& Hit);
	/** Server, once per impact: the break roll and the pickup of an intact projectile. Returns what became of it. */
	ERpgProjectileRecoveryOutcome ResolveRecovery(const FHitResult& Hit, ERpgProjectileImpactResult Result, const FVector& FlightDirection);
	/** Server, before the damage: records an intact projectile in an AI character that keeps projectiles. */
	void PrepareCharacterRecovery(const FHitResult& Hit, const FVector& FlightDirection);
	/** Server: the one break roll of this impact; true when the projectile stays intact. Later calls return the same. */
	bool RollIntact(const FHitResult& Hit);
	const FRpgProjectileSurfaceRecoveryRule* FindSurfaceRule(const FHitResult& Hit) const;
	ERpgProjectileImpactResult ApplyImpactDamage(const FHitResult& Hit, UAbilitySystemComponent* TargetASC);
	void ExecuteImpactCue(const FHitResult& Hit) const;
	void ExpireFlight();
	void FinishFlight(ERpgProjectileImpactResult Result, const FVector& Location, const FVector& Normal);
	void StopFlightPresentation();

	UFUNCTION()
	void OnRep_ImpactState();

	/** Set once by the server when the flight stops. */
	UPROPERTY(ReplicatedUsing = OnRep_ImpactState)
	FRpgProjectileImpactState ImpactState;

	/** The launching ability's shot identity; replicated once. */
	UPROPERTY(Replicated)
	int32 ShotId = 0;

	/** Server: the damage spec built at launch, applied once at impact. */
	FGameplayEffectSpecHandle DamageSpec;

	/** Server: the shooter's ability system; its other avatars (a respawned pawn) are passed through, never hit. */
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;

	/** Server: actors this flight passed through, such as a dodging target. */
	TArray<TWeakObjectPtr<AActor>> PassedThroughActors;

	/** World launch point in cm. Range and defense measure from it on the server; clients play the launch sound there. */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 LaunchOrigin = FVector::ZeroVector;

	/** Server: the ammunition an intact projectile returns; none grants no recovery credit. */
	TSubclassOf<URpgInventoryItemDefinition> RecoveryItemDefinition;

	/** Server: the break chance the launch resolved, before surface rules. */
	float LaunchBreakChance = 0.0f;

	/** Server: the pickup this projectile's ammunition went to. */
	TWeakObjectPtr<ARpgEmbeddedProjectilePickup> RecoveredPickup;

	/** Server: the character that keeps this projectile and the id of its record there. */
	TWeakObjectPtr<URpgEmbeddedProjectileHostComponent> CharacterHost;
	int32 CharacterRecordId = 0;

	/** Server: the result of the one break roll, once rolled. */
	TOptional<bool> bIntactRoll;

#if WITH_DEV_AUTOMATION_TESTS
	TOptional<float> BreakRollForTests;
#endif

	/** Server: the spawn transform of a deferred launch that has not finished yet. */
	FTransform DeferredSpawnTransform;
	bool bDeferredLaunchPending = false;

	FVector LastSweepLocation = FVector::ZeroVector;
	float FlightSeconds = 0.0f;
	FGenericTeamId LaunchTeamId = FGenericTeamId::NoTeam;
	bool bPresentationStopped = false;
};
