#include "RpgAIAttackDriverComponent.h"

#include "AbilitySystemGlobals.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "SurvivalRpg/Factions/RpgFactionSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace
{
	static TAutoConsoleVariable<int32> CVarRpgAIAttackDriverDebug(
		TEXT("rpg.Combat.AIAttack.Debug"),
		0,
		TEXT("Logs server-side AI attack driver target acquisition and activation decisions."));
}

URpgAIAttackDriverComponent::URpgAIAttackDriverComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	AttackInputTag = RpgGameplayTags::InputTag_Weapon_Primary;
}

void URpgAIAttackDriverComponent::OnRegister()
{
	Super::OnRegister();
	StartTargetAcquisitionIfNeeded();
}

void URpgAIAttackDriverComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	CachedHealthComponent = URpgHealthComponent::FindHealthComponent(Owner);
	if (CachedHealthComponent)
	{
		CachedHealthComponent->OnHealthChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
		CachedHealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::HandleDeathStarted);
	}

	StartTargetAcquisitionIfNeeded();
}

void URpgAIAttackDriverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
		World->GetTimerManager().ClearTimer(TargetAcquisitionTimerHandle);
		World->GetTimerManager().ClearTimer(ReleaseAttackInputTimerHandle);
	}

	ReleaseAttackInput();

	if (CachedHealthComponent)
	{
		CachedHealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		CachedHealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleDeathStarted);
		CachedHealthComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void URpgAIAttackDriverComponent::SetCombatTarget(AActor* NewTarget)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (!IsValidCombatTarget(NewTarget))
	{
		ClearCombatTarget();
		return;
	}

	CombatTarget = NewTarget;
	ScheduleNextAttack(FMath::Max(0.0f, RetaliationDelay));
}

void URpgAIAttackDriverComponent::ClearCombatTarget()
{
	CombatTarget.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
	}

	ReleaseAttackInput();
}

void URpgAIAttackDriverComponent::HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator)
{
	if (!bUseLastHostileDamageInstigator || !HealthComponent || HealthComponent->IsDeadOrDying() || NewValue >= OldValue)
	{
		return;
	}

	if (Instigator == GetOwner())
	{
		return;
	}

	if (IsValidCombatTarget(Instigator))
	{
		CombatTarget = Instigator;
		ScheduleNextAttack(FMath::Max(0.0f, RetaliationDelay));
	}
}

URpgAbilitySystemComponent* URpgAIAttackDriverComponent::GetRpgAbilitySystemComponent() const
{
	return Cast<URpgAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
}

bool URpgAIAttackDriverComponent::IsOwnerAlive() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	const URpgHealthComponent* HealthComponent = CachedHealthComponent.Get();
	if (!HealthComponent)
	{
		HealthComponent = URpgHealthComponent::FindHealthComponent(Owner);
	}

	return !HealthComponent || !HealthComponent->IsDeadOrDying();
}

bool URpgAIAttackDriverComponent::IsValidCombatTarget(const AActor* Target) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Target || Target == Owner)
	{
		return false;
	}

	const URpgHealthComponent* TargetHealthComponent = URpgHealthComponent::FindHealthComponent(Target);
	if (!TargetHealthComponent || TargetHealthComponent->IsDeadOrDying())
	{
		return false;
	}

	const UWorld* World = Owner->GetWorld();
	const URpgFactionSubsystem* FactionSubsystem = World ? World->GetSubsystem<URpgFactionSubsystem>() : nullptr;
	return FactionSubsystem && FactionSubsystem->IsHostile(Owner, Target);
}

bool URpgAIAttackDriverComponent::CanAttemptAttack() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !IsValidCombatTarget(CombatTarget.Get()))
	{
		if (ShouldDebugLog())
		{
			UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: cannot attack: invalid owner/auth/target. Target=%s"),
				*GetNameSafe(Owner),
				*GetNameSafe(CombatTarget.Get()));
		}
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World || (World->GetTimeSeconds() - LastAttackTime) < MinAttackInterval)
	{
		if (ShouldDebugLog())
		{
			UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: cannot attack: interval not ready."),
				*GetNameSafe(Owner));
		}
		return false;
	}

	const URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponent();
	if (!ASC)
	{
		if (ShouldDebugLog())
		{
			UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: cannot attack: no ASC."), *GetNameSafe(Owner));
		}
		return false;
	}

	if (ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Staggered) ||
		ASC->HasMatchingGameplayTag(RpgGameplayTags::State_GuardBroken) ||
		ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Dead) ||
		ASC->HasMatchingGameplayTag(RpgGameplayTags::Status_Death) ||
		ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Blocking))
	{
		if (ShouldDebugLog())
		{
			UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: cannot attack: blocked by combat state tags."), *GetNameSafe(Owner));
		}
		return false;
	}

	const bool bHasPrimary = HasActiveMainHandPrimaryAttack();
	if (!bHasPrimary && ShouldDebugLog())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: cannot attack: no active MainHand weapon with Weapon.Attack.Primary."),
			*GetNameSafe(Owner));
	}

	return bHasPrimary;
}

bool URpgAIAttackDriverComponent::IsInAttackRangeAndCone(const AActor* Target) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Target)
	{
		return false;
	}

	const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	if (ToTarget.SizeSquared2D() > FMath::Square(FMath::Max(0.0f, AttackRange)))
	{
		return false;
	}

	const float ClampedConeDegrees = FMath::Clamp(AttackConeDegrees, 0.0f, 360.0f);
	if (ClampedConeDegrees >= 359.0f)
	{
		return true;
	}

	const FVector OwnerForward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector TargetDirection = ToTarget.GetSafeNormal2D();
	if (OwnerForward.IsNearlyZero() || TargetDirection.IsNearlyZero())
	{
		return false;
	}

	const float RequiredDot = FMath::Cos(FMath::DegreesToRadians(ClampedConeDegrees * 0.5f));
	return FVector::DotProduct(OwnerForward, TargetDirection) >= RequiredDot;
}

bool URpgAIAttackDriverComponent::HasActiveMainHandPrimaryAttack() const
{
	const AActor* Owner = GetOwner();
	const URpgEquipmentManagerComponent* EquipmentManager = Owner ? Owner->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
	const URpgWeaponInstance* MainHandWeapon = EquipmentManager
		? Cast<URpgWeaponInstance>(EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand))
		: nullptr;

	return MainHandWeapon && MainHandWeapon->FindAttackDefinition(RpgGameplayTags::Weapon_Attack_Primary) != nullptr;
}

AActor* URpgAIAttackDriverComponent::FindNearbyHostileTarget() const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	const float SearchRadius = FMath::Max(0.0f, TargetAcquisitionRadius);
	if (SearchRadius <= 0.0f)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSq = TNumericLimits<float>::Max();
	const float SearchRadiusSq = FMath::Square(SearchRadius);
	const FVector OwnerLocation = Owner->GetActorLocation();

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!Candidate || Candidate == Owner || !IsValidCombatTarget(Candidate))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared2D(OwnerLocation, Candidate->GetActorLocation());
		if (DistanceSq > SearchRadiusSq || DistanceSq >= BestDistanceSq)
		{
			continue;
		}

		BestTarget = Candidate;
		BestDistanceSq = DistanceSq;
	}

	return BestTarget;
}

float URpgAIAttackDriverComponent::GetNextAttackInterval() const
{
	return FMath::Max(0.0f, MinAttackInterval) + FMath::FRandRange(0.0f, FMath::Max(0.0f, AttackIntervalRandomJitter));
}

void URpgAIAttackDriverComponent::FaceTarget(AActor* Target) const
{
	AActor* Owner = GetOwner();
	if (!bFaceTargetBeforeAttack || !Owner || !Target)
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator TargetRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	Owner->SetActorRotation(TargetRotation);
}

void URpgAIAttackDriverComponent::StartTargetAcquisitionIfNeeded()
{
	AActor* Owner = GetOwner();
	if (!bAcquireNearbyHostileTargets || !Owner || !Owner->HasAuthority() || !IsOwnerAlive())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (!World->GetTimerManager().IsTimerActive(TargetAcquisitionTimerHandle))
		{
			ScheduleTargetAcquisition(FMath::FRandRange(0.0f, FMath::Max(0.05f, TargetAcquisitionInterval)));
		}
	}
}

void URpgAIAttackDriverComponent::StopDriverActivity()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
		World->GetTimerManager().ClearTimer(TargetAcquisitionTimerHandle);
		World->GetTimerManager().ClearTimer(ReleaseAttackInputTimerHandle);
	}

	CombatTarget.Reset();
	ReleaseAttackInput();
}

void URpgAIAttackDriverComponent::ScheduleNextAttack(float Delay)
{
	UWorld* World = GetWorld();
	if (!World || !GetOwner() || !GetOwner()->HasAuthority() || !IsOwnerAlive())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(AttackTimerHandle);
	World->GetTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&ThisClass::TryAttack,
		FMath::Max(0.01f, Delay),
		false);
}

void URpgAIAttackDriverComponent::ScheduleTargetAcquisition(float Delay)
{
	if (!bAcquireNearbyHostileTargets)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !Owner->HasAuthority() || !IsOwnerAlive())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(TargetAcquisitionTimerHandle);
	World->GetTimerManager().SetTimer(
		TargetAcquisitionTimerHandle,
		this,
		&ThisClass::TryAcquireNearbyTarget,
		FMath::Max(0.05f, Delay),
		false);
}

void URpgAIAttackDriverComponent::TryAcquireNearbyTarget()
{
	if (!IsOwnerAlive())
	{
		StopDriverActivity();
		return;
	}

	AActor* CurrentTarget = CombatTarget.Get();
	if (!IsValidCombatTarget(CurrentTarget))
	{
		CombatTarget.Reset();

		if (AActor* NewTarget = FindNearbyHostileTarget())
		{
			CombatTarget = NewTarget;
			if (ShouldDebugLog())
			{
				UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: acquired nearby target [%s]."),
					*GetNameSafe(GetOwner()),
					*GetNameSafe(NewTarget));
			}
			ScheduleNextAttack(0.0f);
		}
		else if (ShouldDebugLog())
		{
			UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: no nearby hostile target within %.1f."),
				*GetNameSafe(GetOwner()),
				TargetAcquisitionRadius);
		}
	}

	ScheduleTargetAcquisition(FMath::Max(0.05f, TargetAcquisitionInterval));
}

void URpgAIAttackDriverComponent::TryAttack()
{
	if (!IsOwnerAlive())
	{
		StopDriverActivity();
		return;
	}

	AActor* Target = CombatTarget.Get();
	if (!IsValidCombatTarget(Target))
	{
		if (ShouldDebugLog())
		{
			UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: target became invalid before attack. Target=%s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Target));
		}
		ClearCombatTarget();
		return;
	}

	if (!CanAttemptAttack())
	{
		ScheduleNextAttack(FMath::Max(0.05f, AttackRetryInterval));
		return;
	}

	FaceTarget(Target);

	if (!IsInAttackRangeAndCone(Target))
	{
		if (ShouldDebugLog())
		{
			UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: target [%s] not in range/cone. Range=%.1f Cone=%.1f"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Target),
				AttackRange,
				AttackConeDegrees);
		}
		ScheduleNextAttack(FMath::Max(0.05f, AttackRetryInterval));
		return;
	}

	URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponent();
	if (!ASC)
	{
		if (ShouldDebugLog())
		{
			UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: cannot activate attack: no ASC."),
				*GetNameSafe(GetOwner()));
		}
		ScheduleNextAttack(FMath::Max(0.05f, AttackRetryInterval));
		return;
	}

	const FGameplayTag ResolvedAttackInputTag = AttackInputTag.IsValid()
		? AttackInputTag
		: RpgGameplayTags::InputTag_Weapon_Primary;

	const bool bActivated = ASC->TryActivateFirstAbilityByInputTag(ResolvedAttackInputTag, false);
	if (ShouldDebugLog())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: activate [%s] against [%s] -> %s."),
			*GetNameSafe(GetOwner()),
			*ResolvedAttackInputTag.ToString(),
			*GetNameSafe(Target),
			bActivated ? TEXT("success") : TEXT("failed"));
	}

	if (!bActivated)
	{
		ScheduleNextAttack(FMath::Max(0.05f, AttackRetryInterval));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		LastAttackTime = World->GetTimeSeconds();
	}

	ScheduleNextAttack(GetNextAttackInterval());
}

void URpgAIAttackDriverComponent::ReleaseAttackInput()
{
	if (!bAttackInputHeld)
	{
		return;
	}

	bAttackInputHeld = false;

	const FGameplayTag ResolvedAttackInputTag = AttackInputTag.IsValid()
		? AttackInputTag
		: RpgGameplayTags::InputTag_Weapon_Primary;

	if (URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(ResolvedAttackInputTag);
		ASC->ProcessAbilityInput(0.0f, false);
	}
}

void URpgAIAttackDriverComponent::HandleDeathStarted(AActor* OwningActor)
{
	if (ShouldDebugLog())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: stopping because death started."),
			*GetNameSafe(OwningActor));
	}

	StopDriverActivity();
}

bool URpgAIAttackDriverComponent::ShouldDebugLog() const
{
	return CVarRpgAIAttackDriverDebug.GetValueOnGameThread() != 0;
}
