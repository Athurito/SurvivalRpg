#include "RpgCombatProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayCueManager.h"
#include "GameplayEffect.h"
#include "GameplayPrediction.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/AbilitySystem/RpgGameplayEffectContext.h"
#include "SurvivalRpg/Combat/RpgCombatImpact.h"
#include "SurvivalRpg/Factions/RpgFactionSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCombatProjectile)

namespace RpgCombatProjectile
{
	/** Projectiles replicate as far as characters do, so a visible shooter's shot is visible too. */
	constexpr float NetCullDistance = 30000.0f;

	/** Actors one flight segment may pass through (dodging targets, the shooter) before the sweep gives up. */
	constexpr int32 MaxPassThroughsPerSweep = 8;

	/** Length in cm of the synthetic incoming segment when a hit carries no trace segment. */
	constexpr float IncomingSegmentLength = 100.0f;

	ERpgProjectileImpactResult ToImpactResult(ERpgCombatImpactOutcome Outcome)
	{
		switch (Outcome)
		{
		case ERpgCombatImpactOutcome::Block:
			return ERpgProjectileImpactResult::Blocked;
		case ERpgCombatImpactOutcome::PerfectBlock:
			return ERpgProjectileImpactResult::PerfectBlocked;
		case ERpgCombatImpactOutcome::Hit:
		default:
			return ERpgProjectileImpactResult::Hit;
		}
	}
}

ARpgCombatProjectile::ARpgCombatProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetCullDistanceSquared(FMath::Square(RpgCombatProjectile::NetCullDistance));
	SetNetUpdateFrequency(30.0f);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(CollisionRadius);
	CollisionSphere->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CollisionSphere->SetGenerateOverlapEvents(false);
	CollisionSphere->SetCanEverAffectNavigation(false);
	RootComponent = CollisionSphere;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(CollisionSphere);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionSphere);
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	// The server sweeps the flight itself on the projectile channel; the movement never collides.
	ProjectileMovement->bSweepCollision = false;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = GravityScale;
}

ARpgCombatProjectile* ARpgCombatProjectile::LaunchFromAbility(
	UGameplayAbility* LaunchingAbility,
	TSubclassOf<ARpgCombatProjectile> ProjectileClass,
	const FRpgProjectileLaunchParams& Params)
{
	const FGameplayAbilityActorInfo* ActorInfo = LaunchingAbility ? LaunchingAbility->GetCurrentActorInfo() : nullptr;
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !ProjectileClass)
	{
		return nullptr;
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UWorld* World = Avatar ? Avatar->GetWorld() : LaunchingAbility->GetWorld();
	const TSubclassOf<UGameplayEffect> EffectClass = ProjectileClass->GetDefaultObject<ARpgCombatProjectile>()->DamageEffectClass;

	FGameplayEffectSpecHandle Spec;
	if (EffectClass)
	{
		// The ability's context names the instigator and the source object (a bow, a staff, or nothing for a spell).
		Spec = LaunchingAbility->MakeOutgoingGameplayEffectSpec(EffectClass, LaunchingAbility->GetAbilityLevel());
	}
	else
	{
		UE_LOG(LogRpgAbilitySystem, Warning, TEXT("Projectile %s has no DamageEffectClass; %s launches it without damage."),
			*GetNameSafe(ProjectileClass), *GetNameSafe(LaunchingAbility));
	}

	return LaunchWithDamageSpec(World, ProjectileClass, Params, Spec, Cast<APawn>(Avatar));
}

ARpgCombatProjectile* ARpgCombatProjectile::LaunchWithDamageSpec(
	UWorld* World,
	TSubclassOf<ARpgCombatProjectile> ProjectileClass,
	const FRpgProjectileLaunchParams& Params,
	const FGameplayEffectSpecHandle& DamageSpec,
	APawn* InstigatorPawn)
{
	if (!World || World->GetNetMode() == NM_Client || !ProjectileClass)
	{
		return nullptr;
	}

	const FVector Direction = Params.Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		UE_LOG(LogRpgAbilitySystem, Warning, TEXT("Projectile %s was launched without a direction."), *GetNameSafe(ProjectileClass));
		return nullptr;
	}

	UAbilitySystemComponent* SourceASC = nullptr;
	if (const FGameplayEffectSpec* Spec = DamageSpec.Data.Get())
	{
		SourceASC = Spec->GetContext().GetInstigatorAbilitySystemComponent();
	}

	// Owned through the ability system's owner (the PlayerState), the projectile stays net-owned by the shooter after
	// the pawn dies.
	AActor* ProjectileOwner = SourceASC && SourceASC->GetOwnerActor() ? SourceASC->GetOwnerActor() : InstigatorPawn;
	const FTransform SpawnTransform(Direction.Rotation(), Params.Origin);
	ARpgCombatProjectile* Projectile = World->SpawnActorDeferred<ARpgCombatProjectile>(
		ProjectileClass,
		SpawnTransform,
		ProjectileOwner,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return nullptr;
	}

	// Everything the first replication and BeginPlay need is in place before the actor finishes spawning.
	Projectile->SourceAbilitySystem = SourceASC;
	Projectile->InitializeLaunch(Params, Direction, DamageSpec);
	Projectile->FinishSpawning(SpawnTransform);
	return IsValid(Projectile) ? Projectile : nullptr;
}

void ARpgCombatProjectile::InitializeLaunch(
	const FRpgProjectileLaunchParams& Params,
	const FVector& Direction,
	const FGameplayEffectSpecHandle& InDamageSpec)
{
	LaunchOrigin = Params.Origin;
	LastSweepLocation = Params.Origin;
	ShotId = Params.ShotId;
	ProjectileMovement->Velocity = Direction * (Params.Speed > 0.0f ? Params.Speed : DefaultSpeed);

	DamageSpec = InDamageSpec;
	if (FGameplayEffectSpec* Spec = DamageSpec.Data.Get())
	{
		Spec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_Damage, FMath::Max(0.0f, Params.Damage));
		Spec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_StaggerDamage, FMath::Max(0.0f, Params.StaggerDamage));
		Spec->AddDynamicAssetTag(RpgGameplayTags::Damage_Type_Projectile);
		Spec->AppendDynamicAssetTags(DamageTypeTags);
		if (HitCueTag.IsValid())
		{
			Spec->AddDynamicAssetTag(HitCueTag);
		}

		// The projectile, not the shooter's pawn, causes the damage: it keeps the launch faction and the impact side for
		// the whole flight, even after the pawn died or respawned.
		if (FGameplayEffectContext* Context = Spec->GetContext().Get())
		{
			Context->SetEffectCauser(this);
		}
	}

	if (const URpgFactionSubsystem* Factions = GetWorld()->GetSubsystem<URpgFactionSubsystem>())
	{
		int32 FactionId = INDEX_NONE;
		if (const UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get())
		{
			FactionId = Factions->FindFactionFromObject(SourceASC->GetOwnerActor());
		}
		if (FactionId == INDEX_NONE)
		{
			FactionId = Factions->FindFactionFromObject(GetInstigator());
		}
		LaunchTeamId = FactionId == INDEX_NONE ? FGenericTeamId::NoTeam : FGenericTeamId(static_cast<uint8>(FactionId));
	}
}

FGenericTeamId ARpgCombatProjectile::GetGenericTeamId() const
{
	return LaunchTeamId;
}

void ARpgCombatProjectile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	CollisionSphere->SetSphereRadius(CollisionRadius, false);
	ProjectileMovement->ProjectileGravityScale = GravityScale;
}

void ARpgCombatProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// Sweep the segment the movement just covered, after it moved this frame.
		AddTickPrerequisiteComponent(ProjectileMovement);
		SetActorTickEnabled(!ImpactState.HasStopped());
	}
	else
	{
		// Clients simulate the same ballistic flight and smooth the server's corrections through the visuals.
		ProjectileMovement->bInterpMovement = true;
		ProjectileMovement->bInterpRotation = true;
		ProjectileMovement->SetInterpolatedComponent(VisualRoot);
	}

	if (ImpactState.HasStopped())
	{
		// Late join or a point-blank impact: the projectile arrives already stopped.
		SetActorLocation(ImpactState.Location);
		StopFlightPresentation();
		return;
	}

	if (LaunchSounds.Num() > 0 && GetNetMode() != NM_DedicatedServer)
	{
		if (USoundBase* LaunchSound = LaunchSounds[FMath::RandHelper(LaunchSounds.Num())])
		{
			UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
		}
	}
}

void ARpgCombatProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || ImpactState.HasStopped())
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	FHitResult Hit;
	if (SweepFlightSegment(LastSweepLocation, CurrentLocation, Hit))
	{
		ResolveImpact(Hit);
		return;
	}

	LastSweepLocation = CurrentLocation;
	FlightSeconds += DeltaSeconds;
	if (FlightSeconds >= MaxFlightSeconds)
	{
		ExpireFlight();
	}
}

void ARpgCombatProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ImpactState);
	DOREPLIFETIME_CONDITION(ThisClass, ShotId, COND_InitialOnly);
}

void ARpgCombatProjectile::PostNetReceiveLocationAndRotation()
{
	if (ImpactState.HasStopped())
	{
		// The replicated stop already placed the projectile.
		return;
	}

	if (!ProjectileMovement || !ProjectileMovement->bInterpMovement)
	{
		Super::PostNetReceiveLocationAndRotation();
		return;
	}

	const FRepMovement& RepMovement = GetReplicatedMovement();
	ProjectileMovement->MoveInterpolationTarget(FRepMovement::RebaseOntoLocalOrigin(RepMovement.Location, this), RepMovement.Rotation);
}

void ARpgCombatProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	if (ProjectileMovement && !ImpactState.HasStopped())
	{
		ProjectileMovement->Velocity = NewVelocity;
	}
}

bool ARpgCombatProjectile::SweepFlightSegment(const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgCombatProjectileSweep), false, this);
	QueryParams.bReturnPhysicalMaterial = true;
	if (AActor* InstigatorActor = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorActor);
	}
	for (const TWeakObjectPtr<AActor>& PassedActor : PassedThroughActors)
	{
		if (AActor* Actor = PassedActor.Get())
		{
			QueryParams.AddIgnoredActor(Actor);
		}
	}

	const FCollisionShape Shape = FCollisionShape::MakeSphere(CollisionRadius);
	for (int32 Attempt = 0; Attempt < RpgCombatProjectile::MaxPassThroughsPerSweep; ++Attempt)
	{
		FHitResult Hit;
		if (!World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Rpg_TraceChannel_Projectile, Shape, QueryParams))
		{
			return false;
		}

		if (!ShouldPassThrough(Hit))
		{
			OutHit = Hit;
			return true;
		}

		AActor* PassedActor = Hit.GetActor();
		PassedThroughActors.AddUnique(PassedActor);
		QueryParams.AddIgnoredActor(PassedActor);
	}

	return false;
}

bool ARpgCombatProjectile::ShouldPassThrough(const FHitResult& Hit) const
{
	const AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return false;
	}

	if (HitActor == GetInstigator())
	{
		return true;
	}

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);
	if (!TargetASC)
	{
		return false;
	}

	// The shooter's ability system outlives its pawn, so this also spares a pawn the shooter respawned as.
	if (TargetASC == SourceAbilitySystem.Get())
	{
		return true;
	}

	// A dodge's invulnerability window lets the projectile fly on; the damage execution would evade the hit anyway.
	return TargetASC->HasMatchingGameplayTag(RpgGameplayTags::State_Evading);
}

void ARpgCombatProjectile::ResolveImpact(const FHitResult& Hit)
{
	if (!HasAuthority() || ImpactState.HasStopped())
	{
		return;
	}

	// Claim the impact and freeze the flight first: nothing the damage triggers (death, ragdoll, cues) can resolve it twice.
	ImpactState.Result = ERpgProjectileImpactResult::World;
	const FVector FlightDirection = ProjectileMovement->Velocity.GetSafeNormal();
	ProjectileMovement->StopMovementImmediately();
	SetActorTickEnabled(false);
	SetActorLocation(Hit.Location);

	ERpgProjectileImpactResult Result = ERpgProjectileImpactResult::World;
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hit.GetActor());
	if (TargetASC)
	{
		FHitResult ContextHit = Hit;
		if ((ContextHit.TraceEnd - ContextHit.TraceStart).IsNearlyZero() && !FlightDirection.IsNearlyZero())
		{
			// The block test reads the incoming direction from this segment, never from the shooter's position.
			ContextHit.TraceStart = Hit.Location - FlightDirection * RpgCombatProjectile::IncomingSegmentLength;
			ContextHit.TraceEnd = Hit.Location;
		}

		PreApplyImpactDamage(ContextHit, TargetASC);
		Result = ApplyImpactDamage(ContextHit, TargetASC);
	}

	if (IsActorBeingDestroyed())
	{
		return;
	}

	if (!TargetASC || bImpactCueOnTargets)
	{
		ExecuteImpactCue(Hit);
	}

	FinishFlight(Result, Hit.Location, Hit.ImpactNormal);
}

ERpgProjectileImpactResult ARpgCombatProjectile::ApplyImpactDamage(const FHitResult& Hit, UAbilitySystemComponent* TargetASC)
{
	FGameplayEffectSpec* Spec = DamageSpec.Data.Get();
	if (!Spec || !TargetASC)
	{
		return ERpgProjectileImpactResult::Unaffected;
	}

	FGameplayEffectContextHandle Context = Spec->GetContext();
	Context.AddHitResult(Hit, true);
	// After the hit result, which resets the origin to its trace start: range and defense measure from the launch.
	Context.AddOrigin(LaunchOrigin);

	FRpgGameplayEffectContext* RpgContext = FRpgGameplayEffectContext::ExtractEffectContext(Context);
	if (RpgContext)
	{
		RpgContext->ResetImpactOutcome();
	}

	// Applied by the target itself, so the hit lands even when the shooter's ability system is gone.
	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec);

	const TOptional<ERpgCombatImpactOutcome> Outcome = RpgContext ? RpgContext->GetImpactOutcome() : TOptional<ERpgCombatImpactOutcome>();
	return Outcome.IsSet() ? RpgCombatProjectile::ToImpactResult(Outcome.GetValue()) : ERpgProjectileImpactResult::Unaffected;
}

void ARpgCombatProjectile::ExecuteImpactCue(const FHitResult& Hit) const
{
	if (!ImpactCueTag.IsValid())
	{
		return;
	}

	// The GameState is always relevant and has its own avatar, unlike the shooter's ability system, which drops cues on
	// every machine where the shooter's pawn is dead or not relevant.
	const UWorld* World = GetWorld();
	UAbilitySystemComponent* CueASC = World
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(World->GetGameState())
		: nullptr;
	if (!CueASC)
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Hit.ImpactPoint;
	Parameters.Normal = Hit.ImpactNormal;
	Parameters.PhysicalMaterial = Hit.PhysMaterial;
	Parameters.RawMagnitude = ImpactCueMagnitude;
	Parameters.Instigator = GetInstigator();
	Parameters.EffectCauser = const_cast<ARpgCombatProjectile*>(this);
	if (const FGameplayEffectSpec* Spec = DamageSpec.Data.Get())
	{
		Parameters.SourceObject = Spec->GetContext().GetSourceObject();
	}

	FScopedPredictionWindow ClearedKey(CueASC, FPredictionKey(), false);
	CueASC->ExecuteGameplayCue(ImpactCueTag, Parameters);
}

void ARpgCombatProjectile::ExpireFlight()
{
	const FVector FlightDirection = ProjectileMovement->Velocity.GetSafeNormal();
	ProjectileMovement->StopMovementImmediately();
	SetActorTickEnabled(false);
	FinishFlight(ERpgProjectileImpactResult::Expired, GetActorLocation(), FlightDirection.IsNearlyZero() ? FVector::UpVector : -FlightDirection);
}

void ARpgCombatProjectile::FinishFlight(ERpgProjectileImpactResult Result, const FVector& Location, const FVector& Normal)
{
	ImpactState.Result = Result;
	ImpactState.Location = Location;
	ImpactState.Normal = Normal;
	StopFlightPresentation();
	ForceNetUpdate();

	// Lingering lets the stop replicate and the trail and sounds fade before the actor goes.
	SetLifeSpan(FMath::Max(ImpactLingerSeconds, 0.1f));
}

void ARpgCombatProjectile::StopFlightPresentation()
{
	if (bPresentationStopped)
	{
		return;
	}
	bPresentationStopped = true;

	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->ResetInterpolation();
	ProjectileMovement->SetComponentTickEnabled(false);

	TInlineComponentArray<UFXSystemComponent*> FXComponents(this);
	for (UFXSystemComponent* FXComponent : FXComponents)
	{
		FXComponent->Deactivate();
	}

	TInlineComponentArray<UAudioComponent*> AudioComponents(this);
	for (UAudioComponent* AudioComponent : AudioComponents)
	{
		AudioComponent->FadeOut(AudioFadeOutSeconds, 0.0f);
	}

	K2_OnFlightStopped(ImpactState.Result, ImpactState.Location, ImpactState.Normal);
}

void ARpgCombatProjectile::OnRep_ImpactState()
{
	if (ImpactState.HasStopped())
	{
		SetActorLocation(ImpactState.Location);
		StopFlightPresentation();
	}
}

#if !UE_BUILD_SHIPPING
namespace RpgCombatProjectile
{
	void LaunchFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogRpgAbilitySystem, Warning, TEXT("Rpg.Combat.LaunchProjectile runs on the server; use the host's window."));
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogRpgAbilitySystem, Warning, TEXT("Usage: Rpg.Combat.LaunchProjectile <ProjectileClassPath> [SpeedCmPerSec] [Damage]"));
			return;
		}

		UClass* ProjectileClass = LoadClass<ARpgCombatProjectile>(nullptr, *Args[0]);
		APlayerController* PlayerController = World->GetFirstPlayerController();
		APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!ProjectileClass || !Pawn)
		{
			UE_LOG(LogRpgAbilitySystem, Warning, TEXT("Rpg.Combat.LaunchProjectile needs a projectile class (%s) and a local pawn."), *Args[0]);
			return;
		}

		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

		FRpgProjectileLaunchParams Params;
		Params.Direction = ViewRotation.Vector();
		Params.Origin = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f) + Params.Direction * 100.0f;
		Params.Speed = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 0.0f;
		Params.Damage = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 10.0f;

		FGameplayEffectSpecHandle Spec;
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		const TSubclassOf<UGameplayEffect> EffectClass = ProjectileClass->GetDefaultObject<ARpgCombatProjectile>()->GetDamageEffectClass();
		if (ASC && EffectClass)
		{
			Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, ASC->MakeEffectContext());
		}
		ARpgCombatProjectile::LaunchWithDamageSpec(World, ProjectileClass, Params, Spec, Pawn);
	}

	static FAutoConsoleCommandWithWorldAndArgs LaunchProjectileCommand(
		TEXT("Rpg.Combat.LaunchProjectile"),
		TEXT("Server: launches a combat projectile along the first local player's view, without equipment. ")
		TEXT("Args: <ProjectileClassPath> [SpeedCmPerSec] [Damage, default 10]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LaunchFromConsole));
}
#endif
