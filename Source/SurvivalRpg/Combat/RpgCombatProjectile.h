#pragma once

#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"

#include "RpgCombatProjectile.generated.h"

class APawn;
class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UProjectileMovementComponent;
class USoundBase;
class USphereComponent;

/** What becomes of the projectile's ammunition when it lands. The projectile only carries the policy; recovery acts on it. */
UENUM(BlueprintType)
enum class ERpgProjectileImpactRecovery : uint8
{
	/** The server rolls once at impact whether the projectile stays intact and can be recovered (standard arrows). */
	Recoverable,
	/** Burns, shatters or dissolves on impact: no break roll, embedding, pickup or recovery credit (magic arrows, spells). */
	ConsumedOnImpact,
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
 * class data below and attach their flight presentation under VisualRoot; when the flight stops, the projectile
 * deactivates every FX system and fades every audio component on its own.
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

private:
	void InitializeLaunch(const FRpgProjectileLaunchParams& Params, const FVector& Direction, const FGameplayEffectSpecHandle& InDamageSpec);
	bool SweepFlightSegment(const FVector& Start, const FVector& End, FHitResult& OutHit);
	bool ShouldPassThrough(const FHitResult& Hit) const;
	void ResolveImpact(const FHitResult& Hit);
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

	/** Server: the spawn transform of a deferred launch that has not finished yet. */
	FTransform DeferredSpawnTransform;
	bool bDeferredLaunchPending = false;

	FVector LastSweepLocation = FVector::ZeroVector;
	float FlightSeconds = 0.0f;
	FGenericTeamId LaunchTeamId = FGenericTeamId::NoTeam;
	bool bPresentationStopped = false;
};
