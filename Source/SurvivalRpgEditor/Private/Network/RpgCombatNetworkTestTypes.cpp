#include "Network/RpgCombatNetworkTestTypes.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "GameplayEffect.h"
#include "Misc/Guid.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCombatNetworkTestTypes)

namespace
{
	constexpr uint8 HostileTeamId = 2;
	constexpr float FixtureMaxHealth = 100000.0f;

	// Host, owning client, late joiner and one free start for a respawn, in lanes along Y.
	constexpr int32 GroundPlayerStartCount = 4;
	constexpr double GroundLaneSpacing = 500.0;
	// Capsule bottoms start clear of the floor top at z=0, so every pawn lands after a short drop.
	constexpr double GroundPlayerStartHeight = 120.0;
}

ARpgCombatNetworkFloorFixture::ARpgCombatNetworkFloorFixture(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(SceneRoot);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetupAttachment(SceneRoot);
	Collision->SetMobility(EComponentMobility::Static);
	Collision->SetRelativeLocation(FVector(0.0, 0.0, -50.0));
	Collision->InitBoxExtent(FVector(50000.0, 50000.0, 50.0));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCanEverAffectNavigation(false);
}

ARpgMoverRagdollFloorFixture::ARpgMoverRagdollFloorFixture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Collision->SetCollisionObjectType(ECC_WorldStatic);
}

FRpgScopedNetworkTestGround::~FRpgScopedNetworkTestGround()
{
	FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle);
}

void FRpgScopedNetworkTestGround::Start()
{
	FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle);
	Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FRpgScopedNetworkTestGround::OnGameModeInitialized);
}

void FRpgScopedNetworkTestGround::OnGameModeInitialized(AGameModeBase* GameMode)
{
	UWorld* World = GameMode ? GameMode->GetWorld() : nullptr;
	if (!World || World->WorldType != EWorldType::PIE)
	{
		return;
	}

	// The broadcast runs inside InitGame, before any player logs in. These are ordinary replicated
	// collision and spawn points; no pawn is relocated.
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ARpgCombatNetworkFloorFixture>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	for (int32 Index = 0; Index < GroundPlayerStartCount; ++Index)
	{
		const double LaneY = (Index - 0.5 * (GroundPlayerStartCount - 1)) * GroundLaneSpacing;
		World->SpawnActor<APlayerStart>(FVector(0.0, LaneY, GroundPlayerStartHeight), FRotator::ZeroRotator, Spawn);
	}
}

FRpgScopedPIEWorldSaveIsolation::~FRpgScopedPIEWorldSaveIsolation()
{
	FGameModeEvents::OnGameModeInitializedEvent().Remove(GameModeInitializedHandle);
	// The actual test actors stay isolated through Logout/EndPlay, even after an aborted test.
}

void FRpgScopedPIEWorldSaveIsolation::Start()
{
	check(!GameModeInitializedHandle.IsValid());
	SlotPrefix = TEXT("SurvivalRpg_CombatPIETest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	GameModeInitializedHandle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(
		this, &FRpgScopedPIEWorldSaveIsolation::IsolateGameMode);
}

bool FRpgScopedPIEWorldSaveIsolation::IsWorldIsolated(UWorld* World) const
{
	if (!IsValid(World) || World->WorldType != EWorldType::PIE)
	{
		return false;
	}

	// Network clients never own a GameMode; the authoritative world owns all world persistence.
	if (World->GetNetMode() == NM_Client)
	{
		return World->GetAuthGameMode() == nullptr;
	}

	const ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>();
	const FString* IsolatedSlot = GameMode ? GameModeSlots.Find(GameMode) : nullptr;
	return GameMode && IsolatedSlot && !GameMode->bEnableDiskPersistence &&
		GameMode->WorldSaveSlotName == *IsolatedSlot &&
		GameMode->WorldSaveBackupSlotName == *IsolatedSlot + TEXT("_Backup") &&
		GameMode->WorldSaveRecoverySlotName == *IsolatedSlot + TEXT("_Recovery");
}

void FRpgScopedPIEWorldSaveIsolation::IsolateGameMode(AGameModeBase* InitializedGameMode)
{
	ARpgGameModeBase* GameMode = Cast<ARpgGameModeBase>(InitializedGameMode);
	if (!GameMode || !GameMode->GetWorld() || GameMode->GetWorld()->WorldType != EWorldType::PIE)
	{
		return;
	}

	// AGameModeBase broadcasts inside Super::InitGame, before ARpgGameModeBase loads world saves.
	// This applies to every authoritative PIE world, including entry/travel/late-join setup worlds.
	// Editing only the Blueprint CDO did not isolate the instance in the previous PIE save incident.
	FString& IsolatedSlot = GameModeSlots.FindOrAdd(GameMode);
	if (IsolatedSlot.IsEmpty())
	{
		IsolatedSlot = FString::Printf(TEXT("%s_%u"), *SlotPrefix, GameMode->GetUniqueID());
	}
	GameMode->bEnableDiskPersistence = false;
	GameMode->WorldSaveSlotName = IsolatedSlot;
	GameMode->WorldSaveBackupSlotName = IsolatedSlot + TEXT("_Backup");
	GameMode->WorldSaveRecoverySlotName = IsolatedSlot + TEXT("_Recovery");
}

FRpgScopedNetworkTestFrameTimeLimit::~FRpgScopedNetworkTestFrameTimeLimit()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(Handle);
}

void FRpgScopedNetworkTestFrameTimeLimit::Start(const float InMaxFrameTime)
{
	check(InMaxFrameTime > 0.0f);
	MaxFrameTime = InMaxFrameTime;
	FWorldDelegates::OnPostWorldInitialization.Remove(Handle);
	Handle = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FRpgScopedNetworkTestFrameTimeLimit::OnPostWorldInitialization);
}

void FRpgScopedNetworkTestFrameTimeLimit::OnPostWorldInitialization(UWorld* World, const FWorldInitializationValues InitializationValues)
{
	AWorldSettings* Settings = World && World->WorldType == EWorldType::PIE ? World->GetWorldSettings() : nullptr;
	if (Settings)
	{
		// UWorld::Tick clamps each frame's game delta to this; the PIE copy of the settings dies with its world.
		Settings->MaxUndilatedFrameTime = FMath::Min(Settings->MaxUndilatedFrameTime, MaxFrameTime);
	}
}

ARpgCombatNetworkTargetFixture::ARpgCombatNetworkTargetFixture(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TeamId(HostileTeamId)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	HitCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitCollision"));
	SetRootComponent(HitCollision);
	HitCollision->InitCapsuleSize(20.0f, 30.0f);
	HitCollision->SetCollisionObjectType(ECC_WorldDynamic);
	HitCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitCollision->SetCollisionResponseToChannel(Rpg_TraceChannel_Weapon, ECR_Block);
	HitCollision->SetCollisionResponseToChannel(Rpg_TraceChannel_Projectile, ECR_Block);
	HitCollision->SetGenerateOverlapEvents(false);
	HitCollision->SetCanEverAffectNavigation(false);

	AbilitySystemComponent =
		CreateDefaultSubobject<URpgAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	HealthSet = CreateDefaultSubobject<URpgHealthSet>(TEXT("HealthSet"));
	DefenseSet = CreateDefaultSubobject<URpgDefenseSet>(TEXT("DefenseSet"));
	HealthComponent = CreateDefaultSubobject<URpgHealthComponent>(TEXT("HealthComponent"));
}

UAbilitySystemComponent* ARpgCombatNetworkTargetFixture::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARpgCombatNetworkTargetFixture::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	TeamId = NewTeamId;
}

FGenericTeamId ARpgCombatNetworkTargetFixture::GetGenericTeamId() const
{
	return TeamId;
}

void ARpgCombatNetworkTargetFixture::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (!AbilitySystemComponent || !HealthSet || !DefenseSet || !HealthComponent)
	{
		return;
	}

	AbilitySystemComponent->AddAttributeSetSubobject(HealthSet.Get());
	AbilitySystemComponent->AddAttributeSetSubobject(DefenseSet.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		AbilitySystemComponent->SetNumericAttributeBase(
			URpgHealthSet::GetMaxHealthAttribute(),
			FixtureMaxHealth);
		AbilitySystemComponent->SetNumericAttributeBase(
			URpgHealthSet::GetHealthAttribute(),
			FixtureMaxHealth);
	}

	HealthComponent->InitializeWithAbilitySystem(AbilitySystemComponent);
	HealthSet->OnHealthChanged.AddUObject(
		this,
		&ThisClass::HandleHealthChanged);
	ResetDamageObservations();
}

void ARpgCombatNetworkTargetFixture::ResetDamageObservations()
{
	HealthDropCount = 0;
	LastDamageInstigator.Reset();
	LastDamageCauser.Reset();
	LastDamageSource.Reset();
	LastHealthBeforeDamage = 0.0f;
	LastHealthAfterDamage = 0.0f;
}

void ARpgCombatNetworkTargetFixture::HandleHealthChanged(
	AActor* DamageInstigator,
	AActor* DamageCauser,
	const FGameplayEffectSpec* DamageEffectSpec,
	float DamageMagnitude,
	float OldValue,
	float NewValue)
{
	if (!HasAuthority() || !DamageEffectSpec || NewValue >= OldValue)
	{
		return;
	}

	++HealthDropCount;
	LastDamageInstigator = DamageInstigator;
	LastDamageCauser = DamageCauser;
	LastDamageSource = DamageEffectSpec->GetContext().GetSourceObject();
	LastHealthBeforeDamage = OldValue;
	LastHealthAfterDamage = NewValue;
}
