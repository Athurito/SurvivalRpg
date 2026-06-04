#include "RpgGameplayAbility_BasicWeaponAttack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
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
#include "SurvivalRpg/SurvivalRpg.h"
#include "TimerManager.h"

#if ENABLE_DRAW_DEBUG
namespace
{
	float GetWeaponAttackTraceDebugDuration()
	{
		const URpgCombatDeveloperSettings* Settings = GetDefault<URpgCombatDeveloperSettings>();
		return Settings ? FMath::Max(0.0f, Settings->WeaponAttackTraceDebugDuration) : 0.0f;
	}

	bool ShouldDrawWeaponAttackTraces()
	{
		const URpgCombatDeveloperSettings* Settings = GetDefault<URpgCombatDeveloperSettings>();
		return Settings && Settings->bDrawWeaponAttackTraces;
	}

	FColor GetWeaponAttackTraceColor(const TArray<FHitResult>& HitResults)
	{
		return HitResults.IsEmpty() ? FColor::Green : FColor::Red;
	}

	void DrawWeaponAttackTraceLine(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const TArray<FHitResult>& HitResults)
	{
		if (!World || !ShouldDrawWeaponAttackTraces())
		{
			return;
		}

		const float Duration = GetWeaponAttackTraceDebugDuration();
		const FColor TraceColor = GetWeaponAttackTraceColor(HitResults);

		DrawDebugLine(World, TraceStart, TraceEnd, TraceColor, false, Duration, 0, 1.5f);
		DrawDebugPoint(World, TraceStart, 8.0f, FColor::Blue, false, Duration);
		DrawDebugPoint(World, TraceEnd, 8.0f, FColor::Cyan, false, Duration);

		for (const FHitResult& HitResult : HitResults)
		{
			DrawDebugPoint(World, HitResult.ImpactPoint, 18.0f, FColor::Yellow, false, Duration);
		}
	}

	void DrawWeaponAttackTraceSweep(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FRpgWeaponAttackDefinition& AttackDefinition,
		const TArray<FHitResult>& HitResults)
	{
		if (!World || !ShouldDrawWeaponAttackTraces())
		{
			return;
		}

		const float Duration = GetWeaponAttackTraceDebugDuration();
		const FVector TraceDelta = TraceEnd - TraceStart;
		const FColor TraceColor = GetWeaponAttackTraceColor(HitResults);

		DrawDebugPoint(World, TraceStart, 12.0f, FColor::Blue, false, Duration);
		DrawDebugPoint(World, TraceEnd, 12.0f, FColor::Cyan, false, Duration);
		DrawDebugLine(World, TraceStart, TraceEnd, TraceColor, false, Duration, 0, 1.0f);

		switch (AttackDefinition.TraceMode)
		{
		case ERpgWeaponAttackTraceMode::SphereSweep:
		{
			const float Radius = FMath::Max(1.0f, AttackDefinition.TraceRadius);
			const FVector TraceCenter = (TraceStart + TraceEnd) * 0.5f;
			const FQuat TraceRotation = TraceDelta.IsNearlyZero()
				? FQuat::Identity
				: FRotationMatrix::MakeFromZ(TraceDelta).ToQuat();
			DrawDebugCapsule(World, TraceCenter, Radius + (TraceDelta.Size() * 0.5f), Radius, TraceRotation, TraceColor, false, Duration, 0, 1.5f);
			break;
		}
		case ERpgWeaponAttackTraceMode::CapsuleSweep:
		{
			const float Radius = FMath::Max(1.0f, AttackDefinition.TraceRadius);
			const float HalfHeight = FMath::Max(Radius, AttackDefinition.TraceCapsuleHalfHeight);
			DrawDebugCapsule(World, TraceStart, HalfHeight, Radius, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			DrawDebugCapsule(World, TraceEnd, HalfHeight, Radius, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			break;
		}
		case ERpgWeaponAttackTraceMode::BoxSweep:
		{
			const FVector Extent(
				FMath::Max(1.0f, FMath::Abs(AttackDefinition.TraceBoxExtent.X)),
				FMath::Max(1.0f, FMath::Abs(AttackDefinition.TraceBoxExtent.Y)),
				FMath::Max(1.0f, FMath::Abs(AttackDefinition.TraceBoxExtent.Z)));
			DrawDebugBox(World, TraceStart, Extent, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			DrawDebugBox(World, TraceEnd, Extent, FQuat::Identity, TraceColor, false, Duration, 0, 1.0f);
			break;
		}
		default:
			break;
		}

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

	const URpgWeaponInstance* WeaponInstance = Cast<URpgWeaponInstance>(GetSourceObject(Handle, ActorInfo));
	if (!WeaponInstance)
	{
		return false;
	}

	const FGameplayTag InputTag = GetInputTagFromSpec(Handle, ActorInfo);
	if (!IsEquipmentActiveForInput(WeaponInstance, InputTag))
	{
		return false;
	}

	return WeaponInstance->FindAttackDefinition(ResolveAttackDefinitionTag(Handle, ActorInfo)) != nullptr;
}

void URpgGameplayAbility_BasicWeaponAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	check(ActorInfo);

	ActiveWeaponInstance = Cast<URpgWeaponInstance>(GetSourceObject(Handle, ActorInfo));
	const FRpgWeaponAttackDefinition* AttackDefinition = ActiveWeaponInstance ? ActiveWeaponInstance->FindAttackDefinition(ResolveAttackDefinitionTag(Handle, ActorInfo)) : nullptr;
	if (!ActiveWeaponInstance || !AttackDefinition || !AttackDefinition->CanApplyDamage())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ActiveAttackDefinition = *AttackDefinition;
	bWaitingForMontage = false;
	bFinishingAttack = false;
	bAttackWindowOpen = false;
	bReceivedAttackWindowStart = false;
	bReceivedAttackWindowEnd = false;
	PreviousTracePointLocations.Reset();
	HitActorsThisWindow.Reset();

	if (!ActiveAttackDefinition.Montage || !ActiveAttackDefinition.HasValidTraceData())
	{
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("BasicWeaponAttack[%s] cannot run: Montage=%s TraceMode=%d TracePoints=%d TraceRadius=%.2f TraceSampleInterval=%.3f."),
			*GetNameSafe(this),
			*GetNameSafe(ActiveAttackDefinition.Montage),
			static_cast<int32>(ActiveAttackDefinition.TraceMode),
			ActiveAttackDefinition.TracePointSockets.Num(),
			ActiveAttackDefinition.TraceRadius,
			ActiveAttackDefinition.TraceSampleInterval);

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

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

	UAbilityTask_WaitGameplayEvent* WindowStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		RpgGameplayTags::GameplayEvent_Weapon_Attack_Window_Start,
		nullptr,
		false,
		true);
	WindowStartTask->EventReceived.AddDynamic(this, &ThisClass::OnAttackWindowStarted);
	WindowStartTask->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* WindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		RpgGameplayTags::GameplayEvent_Weapon_Attack_Window_End,
		nullptr,
		false,
		true);
	WindowEndTask->EventReceived.AddDynamic(this, &ThisClass::OnAttackWindowEnded);
	WindowEndTask->ReadyForActivation();

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
}

void URpgGameplayAbility_BasicWeaponAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	CloseAttackWindow(false);

	ActiveWeaponInstance = nullptr;
	ActiveAttackDefinition = FRpgWeaponAttackDefinition();
	bWaitingForMontage = false;
	bFinishingAttack = false;
	bAttackWindowOpen = false;
	bReceivedAttackWindowStart = false;
	bReceivedAttackWindowEnd = false;
	PreviousTracePointLocations.Reset();
	HitActorsThisWindow.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_BasicWeaponAttack::OnAttackWindowStarted(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	bReceivedAttackWindowStart = true;
	OpenAttackWindow();
}

void URpgGameplayAbility_BasicWeaponAttack::OnAttackWindowEnded(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	bReceivedAttackWindowEnd = true;
	CloseAttackWindow(false);
}

void URpgGameplayAbility_BasicWeaponAttack::OnMontageFinished()
{
	bWaitingForMontage = false;

	if (bAttackWindowOpen)
	{
		CloseAttackWindow(true);
	}
	else if (!bReceivedAttackWindowStart)
	{
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("BasicWeaponAttack[%s] montage [%s] finished without an AttackWindowStart notify; no damage was applied."),
			*GetNameSafe(this),
			*GetNameSafe(ActiveAttackDefinition.Montage));
	}

	FinishAttack(false);
}

void URpgGameplayAbility_BasicWeaponAttack::OnMontageCancelled()
{
	bWaitingForMontage = false;
	CloseAttackWindow(false);
	FinishAttack(true);
}

FGameplayTag URpgGameplayAbility_BasicWeaponAttack::ResolveAttackDefinitionTag(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (bRouteAttackDefinitionFromInputTag)
	{
		const FGameplayTag InputTag = GetInputTagFromSpec(Handle, ActorInfo);
		if (InputTag == RpgGameplayTags::InputTag_Weapon_Secondary)
		{
			return RpgGameplayTags::Weapon_Attack_Secondary;
		}
		if (InputTag == RpgGameplayTags::InputTag_Weapon_Primary)
		{
			return RpgGameplayTags::Weapon_Attack_Primary;
		}
	}

	return AttackDefinitionTag.IsValid() ? AttackDefinitionTag : RpgGameplayTags::Weapon_Attack_Primary;
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
			if (SceneComponent && SceneComponent->GetFName() == SocketName)
			{
				OutLocation = SceneComponent->GetComponentLocation();
				return true;
			}

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

bool URpgGameplayAbility_BasicWeaponAttack::GatherTracePointLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();
	OutLocations.Reserve(ActiveAttackDefinition.TracePointSockets.Num());

	for (const FName SocketName : ActiveAttackDefinition.TracePointSockets)
	{
		FVector SocketLocation = FVector::ZeroVector;
		if (!TryGetSocketLocationFromWeapon(ActiveWeaponInstance, SocketName, SocketLocation) &&
			!TryGetSocketLocationFromAvatar(SocketName, SocketLocation))
		{
			UE_LOG(
				LogRpgWeapons,
				Warning,
				TEXT("BasicWeaponAttack[%s] could not resolve weapon trace point [%s] for [%s]."),
				*GetNameSafe(this),
				*SocketName.ToString(),
				*GetNameSafe(ActiveWeaponInstance));
			return false;
		}

		OutLocations.Add(SocketLocation);
	}

	return OutLocations.Num() >= 2;
}

void URpgGameplayAbility_BasicWeaponAttack::BuildInterpolatedTracePointPairs(
	const TArray<FVector>& PreviousSocketLocations,
	const TArray<FVector>& CurrentSocketLocations,
	TArray<FVector>& OutPreviousTraceLocations,
	TArray<FVector>& OutCurrentTraceLocations) const
{
	OutPreviousTraceLocations.Reset();
	OutCurrentTraceLocations.Reset();

	if (PreviousSocketLocations.Num() != CurrentSocketLocations.Num() || PreviousSocketLocations.IsEmpty())
	{
		return;
	}

	if (!ActiveAttackDefinition.bTraceBetweenSockets)
	{
		OutPreviousTraceLocations = PreviousSocketLocations;
		OutCurrentTraceLocations = CurrentSocketLocations;
		return;
	}

	const float InterpolationDistance = FMath::Max(1.0f, ActiveAttackDefinition.TraceInterpolationDistance);
	OutPreviousTraceLocations.Add(PreviousSocketLocations[0]);
	OutCurrentTraceLocations.Add(CurrentSocketLocations[0]);

	for (int32 SocketIndex = 0; SocketIndex < PreviousSocketLocations.Num() - 1; ++SocketIndex)
	{
		const FVector& PreviousA = PreviousSocketLocations[SocketIndex];
		const FVector& PreviousB = PreviousSocketLocations[SocketIndex + 1];
		const FVector& CurrentA = CurrentSocketLocations[SocketIndex];
		const FVector& CurrentB = CurrentSocketLocations[SocketIndex + 1];

		const float PreviousSegmentLength = FVector::Distance(PreviousA, PreviousB);
		const float CurrentSegmentLength = FVector::Distance(CurrentA, CurrentB);
		const int32 StepCount = FMath::Max(1, FMath::CeilToInt(FMath::Max(PreviousSegmentLength, CurrentSegmentLength) / InterpolationDistance));

		for (int32 StepIndex = 1; StepIndex <= StepCount; ++StepIndex)
		{
			const float Alpha = static_cast<float>(StepIndex) / static_cast<float>(StepCount);
			OutPreviousTraceLocations.Add(FMath::Lerp(PreviousA, PreviousB, Alpha));
			OutCurrentTraceLocations.Add(FMath::Lerp(CurrentA, CurrentB, Alpha));
		}
	}
}

void URpgGameplayAbility_BasicWeaponAttack::OpenAttackWindow()
{
	if (bAttackWindowOpen)
	{
		return;
	}

	bAttackWindowOpen = true;
	HitActorsThisWindow.Reset();
	PreviousTracePointLocations.Reset();

	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	if (!GatherTracePointLocations(PreviousTracePointLocations))
	{
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("BasicWeaponAttack[%s] opened an attack window but has no valid trace points; no damage will be applied."),
			*GetNameSafe(this));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float SampleInterval = FMath::Max(0.001f, ActiveAttackDefinition.TraceSampleInterval);
	World->GetTimerManager().SetTimer(
		TraceSampleTimerHandle,
		this,
		&ThisClass::PerformBladeTraceSample,
		SampleInterval,
		true,
		SampleInterval);
}

void URpgGameplayAbility_BasicWeaponAttack::CloseAttackWindow(bool bLogMissingEndNotify)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(TraceSampleTimerHandle);
	}

	if (bAttackWindowOpen && CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		PerformBladeTraceSample();
	}

	if (bAttackWindowOpen && bLogMissingEndNotify && !bReceivedAttackWindowEnd)
	{
		UE_LOG(
			LogRpgWeapons,
			Warning,
			TEXT("BasicWeaponAttack[%s] montage [%s] ended while the attack window was open; closing defensively. Check for missing AttackWindowEnd notify."),
			*GetNameSafe(this),
			*GetNameSafe(ActiveAttackDefinition.Montage));
	}

	bAttackWindowOpen = false;
	PreviousTracePointLocations.Reset();
}

void URpgGameplayAbility_BasicWeaponAttack::BuildTraceQueryParams(FCollisionQueryParams& QueryParams) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
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
}

void URpgGameplayAbility_BasicWeaponAttack::PerformBladeTraceSample()
{
	UWorld* World = GetWorld();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!World || !AvatarActor || !bAttackWindowOpen || !ActiveAttackDefinition.CanApplyDamage())
	{
		return;
	}

	TArray<FVector> CurrentTracePointLocations;
	if (!GatherTracePointLocations(CurrentTracePointLocations))
	{
		return;
	}

	if (PreviousTracePointLocations.Num() != CurrentTracePointLocations.Num())
	{
		PreviousTracePointLocations = CurrentTracePointLocations;
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RpgBasicWeaponAttack), false, AvatarActor);
	BuildTraceQueryParams(QueryParams);

	switch (ActiveAttackDefinition.TraceMode)
	{
	case ERpgWeaponAttackTraceMode::LineTrace:
		PerformLineTraceRibbon(PreviousTracePointLocations, CurrentTracePointLocations, QueryParams);
		break;
	case ERpgWeaponAttackTraceMode::SphereSweep:
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
	case ERpgWeaponAttackTraceMode::BoxSweep:
		PerformSweepTraceRibbon(PreviousTracePointLocations, CurrentTracePointLocations, QueryParams);
		break;
	default:
		break;
	}

	PreviousTracePointLocations = CurrentTracePointLocations;
}

void URpgGameplayAbility_BasicWeaponAttack::PerformLineTraceRibbon(
	const TArray<FVector>& PreviousSocketLocations,
	const TArray<FVector>& CurrentSocketLocations,
	const FCollisionQueryParams& QueryParams)
{
	if (PreviousSocketLocations.Num() != CurrentSocketLocations.Num() || PreviousSocketLocations.Num() < 2)
	{
		return;
	}

	TArray<FVector> PreviousTraceLocations;
	TArray<FVector> CurrentTraceLocations;
	BuildInterpolatedTracePointPairs(PreviousSocketLocations, CurrentSocketLocations, PreviousTraceLocations, CurrentTraceLocations);

	for (int32 TracePointIndex = 0; TracePointIndex < PreviousTraceLocations.Num(); ++TracePointIndex)
	{
		TraceDamageLine(PreviousTraceLocations[TracePointIndex], CurrentTraceLocations[TracePointIndex], QueryParams);
	}

	if (!ActiveAttackDefinition.bTraceBetweenSockets)
	{
		return;
	}

	for (int32 SocketIndex = 0; SocketIndex < PreviousSocketLocations.Num() - 1; ++SocketIndex)
	{
		const FVector& PreviousA = PreviousSocketLocations[SocketIndex];
		const FVector& PreviousB = PreviousSocketLocations[SocketIndex + 1];
		const FVector& CurrentA = CurrentSocketLocations[SocketIndex];
		const FVector& CurrentB = CurrentSocketLocations[SocketIndex + 1];

		TraceDamageLine(PreviousA, PreviousB, QueryParams);
		TraceDamageLine(CurrentA, CurrentB, QueryParams);

		// Diagonals approximate the swept quad between neighboring sockets without needing mesh collision.
		TraceDamageLine(PreviousA, CurrentB, QueryParams);
		TraceDamageLine(PreviousB, CurrentA, QueryParams);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::PerformSweepTraceRibbon(
	const TArray<FVector>& PreviousSocketLocations,
	const TArray<FVector>& CurrentSocketLocations,
	const FCollisionQueryParams& QueryParams)
{
	TArray<FVector> PreviousTraceLocations;
	TArray<FVector> CurrentTraceLocations;
	BuildInterpolatedTracePointPairs(PreviousSocketLocations, CurrentSocketLocations, PreviousTraceLocations, CurrentTraceLocations);

	for (int32 TracePointIndex = 0; TracePointIndex < PreviousTraceLocations.Num(); ++TracePointIndex)
	{
		TraceDamageSweep(PreviousTraceLocations[TracePointIndex], CurrentTraceLocations[TracePointIndex], QueryParams);
	}
}

void URpgGameplayAbility_BasicWeaponAttack::TraceDamageLine(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FCollisionQueryParams& QueryParams)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FHitResult> HitResults;
	World->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, Rpg_TraceChannel_Weapon, QueryParams);

#if ENABLE_DRAW_DEBUG
	DrawWeaponAttackTraceLine(World, TraceStart, TraceEnd, HitResults);
#endif

	HandleTraceHitResults(HitResults);
}

void URpgGameplayAbility_BasicWeaponAttack::TraceDamageSweep(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FCollisionQueryParams& QueryParams)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionShape Shape;
	switch (ActiveAttackDefinition.TraceMode)
	{
	case ERpgWeaponAttackTraceMode::SphereSweep:
		Shape = FCollisionShape::MakeSphere(FMath::Max(1.0f, ActiveAttackDefinition.TraceRadius));
		break;
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
		Shape = FCollisionShape::MakeCapsule(
			FMath::Max(1.0f, ActiveAttackDefinition.TraceRadius),
			FMath::Max(ActiveAttackDefinition.TraceRadius, ActiveAttackDefinition.TraceCapsuleHalfHeight));
		break;
	case ERpgWeaponAttackTraceMode::BoxSweep:
		Shape = FCollisionShape::MakeBox(FVector(
			FMath::Max(1.0f, FMath::Abs(ActiveAttackDefinition.TraceBoxExtent.X)),
			FMath::Max(1.0f, FMath::Abs(ActiveAttackDefinition.TraceBoxExtent.Y)),
			FMath::Max(1.0f, FMath::Abs(ActiveAttackDefinition.TraceBoxExtent.Z))));
		break;
	default:
		return;
	}

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, TraceStart, TraceEnd, FQuat::Identity, Rpg_TraceChannel_Weapon, Shape, QueryParams);

#if ENABLE_DRAW_DEBUG
	DrawWeaponAttackTraceSweep(World, TraceStart, TraceEnd, ActiveAttackDefinition, HitResults);
#endif

	HandleTraceHitResults(HitResults);
}

void URpgGameplayAbility_BasicWeaponAttack::HandleTraceHitResults(const TArray<FHitResult>& HitResults)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* TargetActor = HitResult.GetActor();
		const TObjectKey<AActor> TargetKey(TargetActor);
		if (!TargetActor || TargetActor == AvatarActor || HitActorsThisWindow.Contains(TargetKey))
		{
			continue;
		}

		HitActorsThisWindow.Add(TargetKey);
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
