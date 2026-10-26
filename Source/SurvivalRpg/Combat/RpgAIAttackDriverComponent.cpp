#include "RpgAIAttackDriverComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_BasicWeaponAttack.h"
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

	/** Seconds between checks of a running attack sequence for its next combo step. */
	constexpr float AttackSequencePollInterval = 0.05f;
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
		World->GetTimerManager().ClearTimer(AttackSequenceTimerHandle);
	}

	ReleaseAttackInput();
	bAttackSequenceActive = false;
	UnbindTargetAbilitySystem();
	CombatTarget.Reset();

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

	if (bExternalControl)
	{
		SetCombatTargetInternal(NewTarget);
		return;
	}

	// The StateTree target task confirms its target every tick; restarting the schedule each time starved the attack.
	const UWorld* World = GetWorld();
	if (CombatTarget.Get() == NewTarget && World && World->GetTimerManager().IsTimerActive(AttackTimerHandle))
	{
		return;
	}

	SetCombatTargetInternal(NewTarget);
	ScheduleNextAttack(FMath::Max(0.0f, RetaliationDelay));
}

void URpgAIAttackDriverComponent::ClearCombatTarget()
{
	SetCombatTargetInternal(nullptr);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
	}

	ReleaseAttackInput();
}

void URpgAIAttackDriverComponent::HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator)
{
	if (!HealthComponent || HealthComponent->IsDeadOrDying() || NewValue >= OldValue || Instigator == GetOwner())
	{
		return;
	}

	if (bExternalControl)
	{
		const UWorld* World = GetWorld();
		if (TargetPolicy == ERpgAICombatTargetPolicy::LastAttacker && World && Instigator != CombatTarget.Get() &&
			IsValidCombatTarget(Instigator) &&
			(!CombatTarget.IsValid() || World->GetTimeSeconds() - LastTargetSwitchTime >= TargetSwitchCooldown))
		{
			SetCombatTargetInternal(Instigator);
		}
		return;
	}

	if (bUseLastHostileDamageInstigator && IsValidCombatTarget(Instigator))
	{
		SetCombatTargetInternal(Instigator);
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

bool URpgAIAttackDriverComponent::CanAttemptAttack(const bool bRequireInterval) const
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
	if (!World || (bRequireInterval && (World->GetTimeSeconds() - LastAttackTime) < MinAttackInterval))
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

AActor* URpgAIAttackDriverComponent::FindNearestHostileTarget(const float SearchRadius) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || SearchRadius <= 0.0f)
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
	if (!bAcquireNearbyHostileTargets || bExternalControl || !Owner || !Owner->HasAuthority() || !IsOwnerAlive())
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

	SetCombatTargetInternal(nullptr);
	ReleaseAttackInput();
	FinishAttackSequence();
}

void URpgAIAttackDriverComponent::ScheduleNextAttack(float Delay)
{
	UWorld* World = GetWorld();
	if (!World || bExternalControl || !GetOwner() || !GetOwner()->HasAuthority() || !IsOwnerAlive())
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

	if (bExternalControl)
	{
		return;
	}

	AActor* CurrentTarget = CombatTarget.Get();
	if (!IsValidCombatTarget(CurrentTarget))
	{
		SetCombatTargetInternal(nullptr);

		if (AActor* NewTarget = FindNearestHostileTarget(TargetAcquisitionRadius))
		{
			SetCombatTargetInternal(NewTarget);
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
#if WITH_DEV_AUTOMATION_TESTS
	++AttackAttemptCountForTests;
#endif

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

	if (!CanAttemptAttack(true))
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

	const FGameplayTag ResolvedAttackInputTag = GetResolvedAttackInputTag();
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

	if (URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(GetResolvedAttackInputTag());
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

FGameplayTag URpgAIAttackDriverComponent::GetResolvedAttackInputTag() const
{
	return AttackInputTag.IsValid() ? AttackInputTag : RpgGameplayTags::InputTag_Weapon_Primary;
}

void URpgAIAttackDriverComponent::BeginExternalControl(const ERpgAICombatTargetPolicy InTargetPolicy, const float InTargetSwitchCooldown)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	bExternalControl = true;
	TargetPolicy = InTargetPolicy;
	TargetSwitchCooldown = FMath::Max(0.0f, InTargetSwitchCooldown);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
		World->GetTimerManager().ClearTimer(TargetAcquisitionTimerHandle);
	}
	ReleaseAttackInput();
}

void URpgAIAttackDriverComponent::EndExternalControl()
{
	if (!bExternalControl)
	{
		return;
	}

	bExternalControl = false;
	CancelAttackSequence();
	ClearCombatTarget();
	StartTargetAcquisitionIfNeeded();
}

AActor* URpgAIAttackDriverComponent::RefreshCombatTarget(const float SearchRadius, const float LoseRadius)
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World || !Owner->HasAuthority() || !IsOwnerAlive())
	{
		SetCombatTargetInternal(nullptr);
		return nullptr;
	}

	AActor* CurrentTarget = CombatTarget.Get();
	const bool bKeepCurrent = IsValidCombatTarget(CurrentTarget) &&
		FVector::DistSquared2D(Owner->GetActorLocation(), CurrentTarget->GetActorLocation()) <=
			FMath::Square(FMath::Max(0.0f, LoseRadius));
	if (!bKeepCurrent)
	{
		SetCombatTargetInternal(FindNearestHostileTarget(SearchRadius));
		return CombatTarget.Get();
	}

	if (TargetPolicy == ERpgAICombatTargetPolicy::Nearest &&
		World->GetTimeSeconds() - LastTargetSwitchTime >= TargetSwitchCooldown)
	{
		AActor* NearestTarget = FindNearestHostileTarget(SearchRadius);
		if (NearestTarget && NearestTarget != CurrentTarget)
		{
			SetCombatTargetInternal(NearestTarget);
		}
	}
	return CombatTarget.Get();
}

bool URpgAIAttackDriverComponent::StartAttackSequence(const int32 StepCount)
{
#if WITH_DEV_AUTOMATION_TESTS
	++AttackAttemptCountForTests;
#endif

	UWorld* World = GetWorld();
	URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponent();
	AActor* Target = CombatTarget.Get();
	if (bAttackSequenceActive || !World || !ASC || !IsOwnerAlive() || !IsValidCombatTarget(Target) || !CanAttemptAttack(false))
	{
		return false;
	}

	const FGameplayTag InputTag = GetResolvedAttackInputTag();
	FGameplayAbilitySpecHandle SpecHandle;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			SpecHandle = Spec.Handle;
			break;
		}
	}
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	FaceTarget(Target);
	const bool bActivated = ASC->TryActivateAbility(SpecHandle);
	const FGameplayAbilitySpec* StartedSpec = ASC->FindAbilitySpecFromHandle(SpecHandle);
	if (ShouldDebugLog())
	{
		UE_LOG(LogTemp, Warning, TEXT("AIAttack[%s]: attack sequence of %d against [%s] -> %s."),
			*GetNameSafe(GetOwner()), StepCount, *GetNameSafe(Target), bActivated ? TEXT("started") : TEXT("failed"));
	}
	if (!bActivated || !StartedSpec || !StartedSpec->IsActive())
	{
		return false;
	}

	bAttackSequenceActive = true;
	AttackSequenceSpecHandle = SpecHandle;
	AttackSequenceStepCount = FMath::Max(1, StepCount);
	AttackSequenceStepsStarted = 1;
	AttackSequencePressedDuringStep = -1;
	LastAttackTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(
		AttackSequenceTimerHandle, this, &ThisClass::TickAttackSequence, AttackSequencePollInterval, true);
	TickAttackSequence();
	return true;
}

void URpgAIAttackDriverComponent::CancelAttackSequence()
{
	AttackSequenceStepCount = AttackSequenceStepsStarted;
}

void URpgAIAttackDriverComponent::TapAttackInput(URpgAbilitySystemComponent& ASC) const
{
	const FGameplayTag InputTag = GetResolvedAttackInputTag();
	ASC.AbilityInputTagPressed(InputTag);
	ASC.ProcessAbilityInput(0.0f, false);
	ASC.AbilityInputTagReleased(InputTag);
	ASC.ProcessAbilityInput(0.0f, false);
}

void URpgAIAttackDriverComponent::TickAttackSequence()
{
	URpgAbilitySystemComponent* ASC = GetRpgAbilitySystemComponent();
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(AttackSequenceSpecHandle) : nullptr;
	if (!Spec || !Spec->IsActive() || !IsOwnerAlive())
	{
		FinishAttackSequence();
		return;
	}

	const URpgGameplayAbility_BasicWeaponAttack* Attack =
		Cast<URpgGameplayAbility_BasicWeaponAttack>(Spec->GetPrimaryInstance());
	if (!Attack)
	{
		return;
	}

	const int32 StepIndex = Attack->GetComboStepIndex();
	AttackSequenceStepsStarted = FMath::Max(AttackSequenceStepsStarted, StepIndex + 1);
	// One press per running step, early in it: the ability buffers the press and chains at the step's Combo Window.
	if (AttackSequenceStepsStarted < AttackSequenceStepCount && AttackSequencePressedDuringStep < StepIndex)
	{
		AttackSequencePressedDuringStep = StepIndex;
		TapAttackInput(*ASC);
	}
}

void URpgAIAttackDriverComponent::FinishAttackSequence()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackSequenceTimerHandle);
	}
	bAttackSequenceActive = false;
	AttackSequenceSpecHandle = FGameplayAbilitySpecHandle();
	AttackSequenceStepCount = 0;
	AttackSequencePressedDuringStep = -1;
}

void URpgAIAttackDriverComponent::SetCombatTargetInternal(AActor* NewTarget)
{
	if (CombatTarget.Get() == NewTarget)
	{
		return;
	}

	UnbindTargetAbilitySystem();
	CombatTarget = NewTarget;
	if (const UWorld* World = GetWorld())
	{
		LastTargetSwitchTime = World->GetTimeSeconds();
	}
	if (UAbilitySystemComponent* TargetAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewTarget))
	{
		ObservedTargetAbilitySystem = TargetAbilitySystem;
		TargetAbilityActivatedHandle =
			TargetAbilitySystem->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleTargetAbilityActivated);
	}
}

void URpgAIAttackDriverComponent::HandleTargetAbilityActivated(UGameplayAbility* ActivatedAbility)
{
	if (ActivatedAbility && ActivatedAbility->GetAssetTags().HasTag(RpgGameplayTags::Ability_Type_Attack))
	{
		++TargetAttackCount;
	}
}

void URpgAIAttackDriverComponent::UnbindTargetAbilitySystem()
{
	if (UAbilitySystemComponent* TargetAbilitySystem = ObservedTargetAbilitySystem.Get())
	{
		TargetAbilitySystem->AbilityActivatedCallbacks.Remove(TargetAbilityActivatedHandle);
	}
	ObservedTargetAbilitySystem.Reset();
	TargetAbilityActivatedHandle.Reset();
}
