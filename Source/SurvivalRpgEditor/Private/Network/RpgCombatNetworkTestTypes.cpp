#include "Network/RpgCombatNetworkTestTypes.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "GameplayEffect.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCombatNetworkTestTypes)

namespace
{
	constexpr uint8 HostileTeamId = 2;
	constexpr float FixtureMaxHealth = 100000.0f;
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
