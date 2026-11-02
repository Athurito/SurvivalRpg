#include "RpgProjectileShotAutomationTestTypes.h"

#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_ProjectileShot.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_Ammunition.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgProjectileShotAutomationTestTypes)

namespace RpgProjectileShotAutomationTestTypes
{
	void AddArrowFragments(URpgInventoryItemDefinition& Definition, TArray<TObjectPtr<URpgInventoryItemFragment>>& Fragments,
		URpgInventoryFragment_SpatialItem* Spatial, URpgInventoryFragment_ItemTraits* Traits,
		URpgInventoryFragment_Ammunition* Ammunition, TSubclassOf<ARpgCombatProjectile> ProjectileClass)
	{
		Spatial->Footprint.Width = 1;
		Spatial->Footprint.Height = 1;
		Spatial->bAllowRotation = true;
		Fragments.Add(Spatial);

		Traits->ItemCategory = ERpgInventoryItemCategory::Misc;
		Traits->bCanStack = true;
		Traits->MaxStackSize = 20;
		Fragments.Add(Traits);

		Ammunition->ProjectileClass = ProjectileClass;
		Fragments.Add(Ammunition);
	}
}

ARpgProjectileShotAutomationTestProjectile::ARpgProjectileShotAutomationTestProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Any instant effect gives the shot a damage spec whose SetByCaller values the tests read; it is never applied.
	DamageEffectClass = UGameplayEffect::StaticClass();
	GravityScale = 0.0f;
	CollisionRadius = 2.0f;
	MaxFlightSeconds = 5.0f;
	ImpactLingerSeconds = 0.5f;
}

void ARpgProjectileShotAutomationFailingProjectile::BeginPlay()
{
	Super::BeginPlay();
	Destroy();
}

URpgProjectileShotAutomationArrowDefinition::URpgProjectileShotAutomationArrowDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Arrow"));
	RpgProjectileShotAutomationTestTypes::AddArrowFragments(*this, Fragments,
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial")),
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits")),
		CreateDefaultSubobject<URpgInventoryFragment_Ammunition>(TEXT("Ammunition")),
		ARpgProjectileShotAutomationTestProjectile::StaticClass());
}

URpgProjectileShotAutomationFailingArrowDefinition::URpgProjectileShotAutomationFailingArrowDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Failing Arrow"));
	RpgProjectileShotAutomationTestTypes::AddArrowFragments(*this, Fragments,
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial")),
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits")),
		CreateDefaultSubobject<URpgInventoryFragment_Ammunition>(TEXT("Ammunition")),
		ARpgProjectileShotAutomationFailingProjectile::StaticClass());
}

URpgProjectileShotAutomationStaminaEffect::URpgProjectileShotAutomationStaminaEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = RpgGameplayTags::Data_StaminaDelta;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = URpgStaminaSet::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}

URpgProjectileShotAutomationAbility::URpgProjectileShotAutomationAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void URpgProjectileShotAutomationAbility::ResetObservations()
{
	ReleasedCount = 0;
	CancelledCount = 0;
	LastDrawFraction = -1.0f;
	LastResult = ERpgProjectileShotResult::Interrupted;
	LastProjectile.Reset();
}

void URpgProjectileShotAutomationAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ShotTask = URpgAbilityTask_ProjectileShot::StartProjectileShot(this, ShotDefinition, AmmunitionPolicy);
	ShotTask->OnReleased.AddDynamic(this, &ThisClass::HandleReleased);
	ShotTask->OnCancelled.AddDynamic(this, &ThisClass::HandleCancelled);
	ShotTask->ReadyForActivation();
}

void URpgProjectileShotAutomationAbility::HandleReleased(const float DrawFraction, ARpgCombatProjectile* Projectile)
{
	++ReleasedCount;
	LastDrawFraction = DrawFraction;
	LastResult = ERpgProjectileShotResult::Released;
	LastProjectile = Projectile;
	if (bEndOnRelease)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void URpgProjectileShotAutomationAbility::HandleCancelled(const ERpgProjectileShotResult Reason)
{
	++CancelledCount;
	LastResult = Reason;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
