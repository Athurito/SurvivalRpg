#include "RpgGameplayAbility_BasicWeaponAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "SurvivalRpg/Combat/RpgCombatDeveloperSettings.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"

#if ENABLE_DRAW_DEBUG
namespace
{
	void DrawWeaponAttackTrace(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		float TraceRadius,
		const TArray<FHitResult>& HitResults)
	{
		const URpgCombatDeveloperSettings* Settings = GetDefault<URpgCombatDeveloperSettings>();
		if (!World || !Settings || !Settings->bDrawWeaponAttackTraces)
		{
			return;
		}

		const float Duration = FMath::Max(0.0f, Settings->WeaponAttackTraceDebugDuration);
		const float Radius = FMath::Max(1.0f, TraceRadius);
		const FVector TraceDelta = TraceEnd - TraceStart;
		const FVector TraceCenter = (TraceStart + TraceEnd) * 0.5f;
		const FColor TraceColor = HitResults.IsEmpty() ? FColor::Green : FColor::Red;
		const FQuat TraceRotation = TraceDelta.IsNearlyZero()
			? FQuat::Identity
			: FRotationMatrix::MakeFromZ(TraceDelta).ToQuat();

		DrawDebugCapsule(
			World,
			TraceCenter,
			Radius + (TraceDelta.Size() * 0.5f),
			Radius,
			TraceRotation,
			TraceColor,
			false,
			Duration,
			0,
			1.5f);

		DrawDebugPoint(World, TraceStart, 12.0f, FColor::Blue, false, Duration);
		DrawDebugPoint(World, TraceEnd, 12.0f, FColor::Cyan, false, Duration);

		for (const FHitResult& HitResult : HitResults)
		{
			DrawDebugPoint(World, HitResult.ImpactPoint, 18.0f, FColor::Yellow, false, Duration);
		}
	}
}
#endif

URpgGameplayAbility_BasicWeaponAttack::URpgGameplayAbility_BasicWeaponAttack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ERpgAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Blocking;
	AttackDefinitionTag = RpgGameplayTags::Weapon_Attack_Primary;
}

bool URpgGameplayAbility_BasicWeaponAttack::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return Cast<URpgWeaponInstance>(GetSourceObject(Handle, ActorInfo)) != nullptr;
}

void URpgGameplayAbility_BasicWeaponAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	ActiveWeaponInstance = Cast<URpgWeaponInstance>(GetSourceObject(Handle, ActorInfo));
	const FRpgWeaponAttackDefinition* AttackDefinition = ActiveWeaponInstance ? ActiveWeaponInstance->FindAttackDefinition(AttackDefinitionTag) : nullptr;
	if (!ActiveWeaponInstance || !AttackDefinition || !AttackDefinition->CanApplyDamage())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveAttackDefinition = *AttackDefinition;
	bTraceHasFired = false;
	bWaitingForMontage = false;
	bFinishingAttack = false;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActiveAttackDefinition.CameraMode)
	{
		SetCameraMode(ActiveAttackDefinition.CameraMode);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (ActiveAttackDefinition.Montage)
	{
		bWaitingForMontage = true;
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			ActiveAttackDefinition.Montage,
			FMath::Max(0.01f, ActiveAttackDefinition.MontagePlayRate));

		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCancelled);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
		MontageTask->ReadyForActivation();
	}

	const float TraceDelay = FMath::Max(0.0f, ActiveAttackDefinition.DamageTraceDelay);
	if (TraceDelay <= 0.0f)
	{
		OnTraceDelayFinished();
	}
	else
	{
		UAbilityTask_WaitDelay* TraceDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, TraceDelay);
		TraceDelayTask->OnFinish.AddDynamic(this, &ThisClass::OnTraceDelayFinished);
		TraceDelayTask->ReadyForActivation();
	}

	if (!ActiveAttackDefinition.Montage && TraceDelay <= 0.0f)
	{
		FinishAttack(false);
	}
	else if (!ActiveAttackDefinition.Montage && NoMontageEndDelay > 0.0f)
	{
		UAbilityTask_WaitDelay* EndDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, TraceDelay + NoMontageEndDelay);
		EndDelayTask->OnFinish.AddDynamic(this, &ThisClass::OnMontageFinished);
		EndDelayTask->ReadyForActivation();
	}
}

void URpgGameplayAbility_BasicWeaponAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ActiveWeaponInstance = nullptr;
	ActiveAttackDefinition = FRpgWeaponAttackDefinition();
	bTraceHasFired = false;
	bWaitingForMontage = false;
	bFinishingAttack = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_BasicWeaponAttack::OnTraceDelayFinished()
{
	if (bTraceHasFired || !IsActive())
	{
		return;
	}

	bTraceHasFired = true;

	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		PerformDamageTrace();
	}

	if (!bWaitingForMontage)
	{
		FinishAttack(false);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::OnMontageFinished()
{
	bWaitingForMontage = false;
	FinishAttack(false);
}

void URpgGameplayAbility_BasicWeaponAttack::OnMontageCancelled()
{
	bWaitingForMontage = false;
	FinishAttack(true);
}

const FRpgWeaponAttackDefinition* URpgGameplayAbility_BasicWeaponAttack::GetAttackDefinitionFromEquipment() const
{
	const URpgWeaponInstance* WeaponInstance = Cast<URpgWeaponInstance>(GetAssociatedEquipment());
	return WeaponInstance ? WeaponInstance->FindAttackDefinition(AttackDefinitionTag) : nullptr;
}

bool URpgGameplayAbility_BasicWeaponAttack::TryGetSocketLocationFromWeapon(const URpgWeaponInstance* WeaponInstance, FName SocketName, FVector& OutLocation) const
{
	if (!WeaponInstance || SocketName.IsNone())
	{
		return false;
	}

	for (AActor* SpawnedActor : WeaponInstance->GetSpawnedActors())
	{
		if (!SpawnedActor)
		{
			continue;
		}

		TInlineComponentArray<USceneComponent*> SceneComponents;
		SpawnedActor->GetComponents(SceneComponents);
		for (const USceneComponent* SceneComponent : SceneComponents)
		{
			if (SceneComponent && SceneComponent->DoesSocketExist(SocketName))
			{
				OutLocation = SceneComponent->GetSocketLocation(SocketName);
				return true;
			}
		}
	}

	return false;
}

bool URpgGameplayAbility_BasicWeaponAttack::TryGetSocketLocationFromAvatar(FName SocketName, FVector& OutLocation) const
{
	if (SocketName.IsNone())
	{
		return false;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (Mesh && Mesh->DoesSocketExist(SocketName))
	{
		OutLocation = Mesh->GetSocketLocation(SocketName);
		return true;
	}

	return false;
}

void URpgGameplayAbility_BasicWeaponAttack::ResolveTrace(FVector& OutStart, FVector& OutEnd) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const FVector AvatarLocation = AvatarActor ? AvatarActor->GetActorLocation() : FVector::ZeroVector;
	const FVector Forward = AvatarActor ? AvatarActor->GetActorForwardVector() : FVector::ForwardVector;

	OutStart = AvatarLocation + FVector(0.0f, 0.0f, 55.0f);
	if (!TryGetSocketLocationFromWeapon(ActiveWeaponInstance, ActiveAttackDefinition.TraceStartSocket, OutStart))
	{
		TryGetSocketLocationFromAvatar(ActiveAttackDefinition.TraceStartSocket, OutStart);
	}

	if (!ActiveAttackDefinition.TraceEndSocket.IsNone())
	{
		if (TryGetSocketLocationFromWeapon(ActiveWeaponInstance, ActiveAttackDefinition.TraceEndSocket, OutEnd) ||
			TryGetSocketLocationFromAvatar(ActiveAttackDefinition.TraceEndSocket, OutEnd))
		{
			return;
		}
	}

	OutEnd = OutStart + Forward * FMath::Max(0.0f, ActiveAttackDefinition.TraceDistance);
}

void URpgGameplayAbility_BasicWeaponAttack::PerformDamageTrace()
{
	UWorld* World = GetWorld();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!World || !AvatarActor || !ActiveAttackDefinition.CanApplyDamage())
	{
		return;
	}

	FVector TraceStart;
	FVector TraceEnd;
	ResolveTrace(TraceStart, TraceEnd);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgBasicWeaponAttack), false, AvatarActor);
	QueryParams.AddIgnoredActor(AvatarActor);
	if (ActiveWeaponInstance)
	{
		for (AActor* SpawnedActor : ActiveWeaponInstance->GetSpawnedActors())
		{
			if (SpawnedActor)
			{
				QueryParams.AddIgnoredActor(SpawnedActor);
			}
		}
	}

	TArray<FHitResult> HitResults;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(1.0f, ActiveAttackDefinition.TraceRadius));
	World->SweepMultiByChannel(HitResults, TraceStart, TraceEnd, FQuat::Identity, Rpg_TraceChannel_Weapon, Shape, QueryParams);

#if ENABLE_DRAW_DEBUG
	DrawWeaponAttackTrace(World, TraceStart, TraceEnd, ActiveAttackDefinition.TraceRadius, HitResults);
#endif

	TSet<TObjectKey<AActor>> HitActors;
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* TargetActor = HitResult.GetActor();
		const TObjectKey<AActor> TargetKey(TargetActor);
		if (!TargetActor || TargetActor == AvatarActor || HitActors.Contains(TargetKey))
		{
			continue;
		}

		HitActors.Add(TargetKey);
		ApplyDamageToHitActor(TargetActor, HitResult);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::EvaluateConditionalModifiers(
	const UAbilitySystemComponent* TargetASC,
	float& Damage,
	float& StaggerDamage) const
{
	if (!TargetASC || ActiveAttackDefinition.ConditionalModifiers.IsEmpty())
	{
		return;
	}

	FGameplayTagContainer TargetTags;
	TargetASC->GetOwnedGameplayTags(TargetTags);

	for (const FRpgConditionalAttackModifier& Modifier : ActiveAttackDefinition.ConditionalModifiers)
	{
		if (Modifier.MatchesTargetTags(TargetTags))
		{
			Damage *= FMath::Max(0.0f, Modifier.DamageMultiplier);
			StaggerDamage *= FMath::Max(0.0f, Modifier.StaggerDamageMultiplier);
		}
	}
}

FGameplayEffectSpecHandle URpgGameplayAbility_BasicWeaponAttack::MakeWeaponDamageEffectSpec(
	const FHitResult& HitResult,
	const UAbilitySystemComponent* TargetASC) const
{
	FGameplayEffectSpecHandle DamageSpecHandle = MakeOutgoingGameplayEffectSpec(ActiveAttackDefinition.DamageEffect, GetAbilityLevel());
	if (FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get())
	{
		float Damage = ActiveAttackDefinition.Damage;
		float StaggerDamage = ActiveAttackDefinition.StaggerDamage;
		EvaluateConditionalModifiers(TargetASC, Damage, StaggerDamage);

		DamageSpec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_Damage, FMath::Max(0.0f, Damage));
		DamageSpec->SetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_StaggerDamage, FMath::Max(0.0f, StaggerDamage));
		DamageSpec->AppendDynamicAssetTags(ActiveAttackDefinition.DamageTypeTags);
		DamageSpec->GetContext().AddHitResult(HitResult, true);
	}

	return DamageSpecHandle;
}

void URpgGameplayAbility_BasicWeaponAttack::ApplyDamageToHitActor(AActor* TargetActor, const FHitResult& HitResult)
{
	if (!TargetActor)
	{
		return;
	}

	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(TargetActor);
	if (!HealthComponent || HealthComponent->IsDeadOrDying())
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC)
	{
		return;
	}

	FGameplayEffectSpecHandle DamageSpecHandle = MakeWeaponDamageEffectSpec(HitResult, TargetASC);
	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	if (!DamageSpec)
	{
		return;
	}

	if (UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo())
	{
		const float HealthBefore = HealthComponent->GetHealth();
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
		const float HealthAfter = HealthComponent->GetHealth();
		if (HealthAfter < HealthBefore)
		{
			SendHitReactionEvent(TargetActor, HitResult, DamageSpec);
		}
	}
}

void URpgGameplayAbility_BasicWeaponAttack::SendHitReactionEvent(AActor* TargetActor, const FHitResult& HitResult, const FGameplayEffectSpec* DamageSpec) const
{
	const URpgHealthComponent* HealthComponent = URpgHealthComponent::FindHealthComponent(TargetActor);
	if (!TargetActor || !HealthComponent || HealthComponent->IsDeadOrDying())
	{
		return;
	}

	const FGameplayTag EventTag = ActiveAttackDefinition.HitReactionEventTag.IsValid()
		? ActiveAttackDefinition.HitReactionEventTag
		: RpgGameplayTags::GameplayEvent_HitReaction;

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetAvatarActorFromActorInfo();
	Payload.Target = TargetActor;
	Payload.EventMagnitude = ActiveAttackDefinition.Damage;
	if (DamageSpec)
	{
		Payload.ContextHandle = DamageSpec->GetContext();
	}

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, EventTag, Payload);
}

void URpgGameplayAbility_BasicWeaponAttack::FinishAttack(bool bWasCancelled)
{
	if (bFinishingAttack || !IsActive())
	{
		return;
	}

	bFinishingAttack = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
}
