#include "RpgGameplayAbility_Mantle.h"

#include "RpgMantleAnchorComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Mantle)

namespace RpgMantle
{
	// A component identity names exactly one immutable entry. Ambiguous authored obstacles are rejected.
	URpgMantleAnchorComponent* ResolveAnchor(const UPrimitiveComponent* Collider)
	{
		AActor* Owner = Collider ? Collider->GetOwner() : nullptr;
		if (!Owner) return nullptr;
		URpgMantleAnchorComponent* Result = nullptr;
		TInlineComponentArray<URpgMantleAnchorComponent*> Anchors(Owner);
		for (URpgMantleAnchorComponent* Anchor : Anchors)
		{
			if (Anchor && Anchor->GetTraversedComponent() == Collider)
			{
				if (Result) return nullptr;
				Result = Anchor;
			}
		}
		return Result;
	}

	FCollisionShape CapsuleShape(const ACharacter& Character)
	{
		return FCollisionShape::MakeCapsule(Character.GetCapsuleComponent()->GetScaledCapsuleRadius(),
			Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}

	bool IsIncapacitated(const ACharacter& Character)
	{
		const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(&Character);
		const URpgDownedComponent* Downed = URpgDownedComponent::FindDownedComponent(&Character);
		return (Health && Health->IsDeadOrDying()) || (Downed && Downed->IsDowned());
	}
}

URpgGameplayAbility_Mantle::URpgGameplayAbility_Mantle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationGroup = ERpgAbilityActivationGroup::Exclusive_Replaceable;
}

bool URpgGameplayAbility_Mantle::IsCharacterReady(const ACharacter& Character) const
{
	const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
	const USkeletalMeshComponent* Mesh = Character.GetMesh();
	const UAnimInstance* Animation = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const UMotionWarpingComponent* Warping = Character.FindComponentByClass<UMotionWarpingComponent>();
	const URpgCharacterMovementComponent* RpgMovement = Cast<URpgCharacterMovementComponent>(Movement);
	return Movement && Character.GetCapsuleComponent() && Character.GetCapsuleComponent()->IsQueryCollisionEnabled()
		&& Movement->IsMovingOnGround() && !Character.bIsCrouched && !RpgMantle::IsIncapacitated(Character)
		&& FMath::IsFinite(MaxGroundSpeed) && MaxGroundSpeed >= 0.0f && Character.GetVelocity().Size2D() <= MaxGroundSpeed
		&& Montage && Montage->HasRootMotion() && MeshAsset && Montage->GetSkeleton() == MeshAsset->GetSkeleton()
		&& Animation && Animation->RootMotionMode == ERootMotionMode::RootMotionFromMontagesOnly
		&& FMath::IsFinite(PlayRate) && PlayRate > 0.0f && FMath::IsFinite(StartTimeSeconds)
		&& StartTimeSeconds >= 0.0f && StartTimeSeconds < Montage->GetPlayLength()
		&& !WarpTargetName.IsNone() && FMath::IsFinite(LedgeVerticalOffset)
		&& FMath::IsFinite(CandidateSearchDistance) && CandidateSearchDistance > 0.0f
		&& FMath::IsFinite(PathClearance) && PathClearance >= 0.0f
		&& Warping && !Warping->FindWarpTarget(WarpTargetName) && RpgMovement && !RpgMovement->GetMantleCollisionComponent();
}

bool URpgGameplayAbility_Mantle::IsCapsuleClear(const ACharacter& Character, const FVector& Location) const
{
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleClearance), false, &Character);
	return !Character.GetWorld()->OverlapBlockingTestByChannel(Location, Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), RpgMantle::CapsuleShape(Character), Params,
		FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
}

bool URpgGameplayAbility_Mantle::ValidateAnchor(const ACharacter& Character, const URpgMantleAnchorComponent& Anchor) const
{
	if (!IsCharacterReady(Character) || !Anchor.IsEntryInRange(Character)
		|| RpgMantle::ResolveAnchor(Anchor.GetTraversedComponent()) != &Anchor || !IsCapsuleClear(Character, Character.GetActorLocation()))
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	const UWorld* World = Character.GetWorld();
	const FVector Landing = Anchor.GetLandingLocation();
	const FVector Destination = Landing + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight() + 2.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleRoute), false, &Character);
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	FHitResult Hit;
	// A real front face must separate the current capsule and the authored ledge; ordinary walls also occlude it.
	FVector Front = Anchor.GetComponentLocation();
	Front.Z = Character.GetActorLocation().Z;
	Front += Anchor.GetForwardVector() * 5.0;
	if (!World->LineTraceSingleByChannel(Hit, Character.GetActorLocation(), Front, Channel, Params, Responses)
		|| Hit.GetComponent() != Anchor.GetTraversedComponent()) return false;
	// The destination is supported by this exact prepared component, not an unchecked point above empty space.
	if (!World->LineTraceSingleByChannel(Hit, Landing + FVector(0, 0, 10), Landing - FVector(0, 0, 10), Channel, Params, Responses)
		|| Hit.GetComponent() != Anchor.GetTraversedComponent() || !Character.GetCharacterMovement()->IsWalkable(Hit)
		|| FMath::Abs(Hit.ImpactPoint.Z - Landing.Z) > 3.0 || !IsCapsuleClear(Character, Destination)) return false;
	const double RouteZ = FMath::Max(Destination.Z, Anchor.GetComponentLocation().Z + Capsule->GetScaledCapsuleHalfHeight()) + PathClearance;
	const FVector Route[] = {Character.GetActorLocation(), FVector(Character.GetActorLocation().X, Character.GetActorLocation().Y, RouteZ),
		FVector(Destination.X, Destination.Y, RouteZ), Destination};
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Route); ++Index)
	{
		if (World->SweepSingleByChannel(Hit, Route[Index - 1], Route[Index], Capsule->GetComponentQuat(), Channel,
			RpgMantle::CapsuleShape(Character), Params, Responses)) return false;
	}
	return true;
}

URpgMantleAnchorComponent* URpgGameplayAbility_Mantle::FindCandidate(const ACharacter& Character, FHitResult* OutHit) const
{
	if (!IsCharacterReady(Character)) return nullptr;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleCandidate), false, &Character);
	FHitResult Hit;
	const FVector Start = Character.GetActorLocation();
	if (!Character.GetWorld()->LineTraceSingleByChannel(Hit, Start,
		Start + Character.GetActorForwardVector().GetSafeNormal2D() * CandidateSearchDistance, ECC_GameTraceChannel1, Params)) return nullptr;
	URpgMantleAnchorComponent* Anchor = RpgMantle::ResolveAnchor(Hit.GetComponent());
	if (!Anchor || !ValidateAnchor(Character, *Anchor)) return nullptr;
	if (OutHit) *OutHit = Hit;
	return Anchor;
}

bool URpgGameplayAbility_Mantle::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character || !IsCharacterReady(*Character)) return false;
	// The remote server validates the correlated proposal after receiving it; it never substitutes another entry.
	return !ActorInfo->IsLocallyControlled() && ActorInfo->IsNetAuthority() ? true : FindCandidate(*Character) != nullptr;
}

void URpgGameplayAbility_Mantle::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEnding = false;
	bReceivedTargetData = false;
	ActiveCharacter = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !ActiveCharacter.IsValid()) { CancelCurrentMantle(); return; }
	if (ActorInfo->IsLocallyControlled())
	{
		FHitResult Hit;
		if (!FindCandidate(*ActiveCharacter, &Hit)) { CancelCurrentMantle(); return; }
		FGameplayAbilityTargetDataHandle Data(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
		FScopedPredictionWindow Prediction(ASC, !ActorInfo->IsNetAuthority());
		if (!ActorInfo->IsNetAuthority())
		{
			ASC->CallServerSetReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey(), Data, FGameplayTag(), ASC->ScopedPredictionKey);
		}
		HandleTargetData(Data, FGameplayTag());
	}
	else
	{
		TargetDataDelegate = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &ThisClass::HandleTargetData);
		GetWorld()->GetTimerManager().SetTimer(TimeoutHandle, this, &ThisClass::CancelCurrentMantle, FMath::Max(0.1f, TargetDataTimeout));
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
	}
}

void URpgGameplayAbility_Mantle::HandleTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	if (!IsActive() || bEnding || bReceivedTargetData) return;
	bReceivedTargetData = true;
	// Hold our own handle while consuming the ASC's cached entry and invoking gameplay callbacks.
	const FGameplayAbilityTargetDataHandle LocalData = Data;
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	if (LocalData.Num() != 1 || !LocalData.Get(0)
		|| LocalData.Get(0)->GetScriptStruct() != FGameplayAbilityTargetData_SingleTargetHit::StaticStruct())
	{
		CancelCurrentMantle(); return;
	}
	const FHitResult* Hit = LocalData.Get(0)->GetHitResult();
	URpgMantleAnchorComponent* Anchor = Hit ? RpgMantle::ResolveAnchor(Hit->GetComponent()) : nullptr;
	if (!Anchor || !ActiveCharacter.IsValid() || !ValidateAnchor(*ActiveCharacter, *Anchor))
	{
		CancelCurrentMantle(); return;
	}
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo)) { CancelCurrentMantle(); return; }
	// Authored costs/effects can synchronously kill, unpossess or cancel the avatar, including deferred EndAbility calls.
	if (!IsActive() || bEnding || !ActiveCharacter.IsValid() || !IsValid(Anchor)
		|| !ValidateAnchor(*ActiveCharacter, *Anchor)) { CancelCurrentMantle(); return; }
	BeginMantle(*Anchor);
}

void URpgGameplayAbility_Mantle::BeginMantle(URpgMantleAnchorComponent& Anchor)
{
	ACharacter* Character = ActiveCharacter.Get();
	UMotionWarpingComponent* Warping = Character ? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	URpgCharacterMovementComponent* Movement = Character ? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Character || !Warping || Warping->FindWarpTarget(WarpTargetName) || !Movement
		|| !Movement->BeginMantleCollisionIgnore(Anchor.GetTraversedComponent())) { CancelCurrentMantle(); return; }
	ActiveAnchor = &Anchor;
	ActiveWarping = Warping;
	IgnoredComponent = Anchor.GetTraversedComponent();
	AnchorAtActivation = Anchor.GetComponentTransform();
	ColliderAtActivation = IgnoredComponent->GetComponentTransform();
	LandingAtActivation = Anchor.GetLandingLocation();
	EntryLocation = LastClearLocation = Character->GetActorLocation();
	// UE's animated SkewWarp branch measures from the cached mesh base, while the landing contract measures capsule feet.
	// Keep the gameplay mesh unchanged and compensate their vertical separation (e.g. half-height 90 + mesh Z -88 = 2 cm).
	const double MeshAboveCapsuleFeet = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		+ Character->GetBaseTranslationOffset().Z;
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName,
		Anchor.GetComponentLocation() + FVector(0, 0, LedgeVerticalOffset + MeshAboveCapsuleFeet), Anchor.GetComponentRotation());
	Character->StopJumping();
	Character->GetCharacterMovement()->StopMovementImmediately();
	bOwnsMovement = true;
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	Character->OnCharacterMovementUpdated.AddDynamic(this, &ThisClass::HandleMovementUpdated);
	Character->MovementModeChangedDelegate.AddDynamic(this, &ThisClass::HandleMovementModeChanged);
	const float MaximumDuration = (Montage->GetPlayLength() - StartTimeSeconds) / PlayRate + 1.0f;
	GetWorld()->GetTimerManager().SetTimer(TimeoutHandle, this, &ThisClass::CancelCurrentMantle, MaximumDuration);
	// Concrete Blueprint content starts its GAS montage task only after the native movement lease and commit succeed.
	Super::ActivateAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, nullptr);
}

void URpgGameplayAbility_Mantle::HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	if (bEnding || !bOwnsMovement) return;
	ACharacter* Character = ActiveCharacter.Get();
	if (!Character || !ActiveAnchor.IsValid() || !IgnoredComponent.IsValid()
		|| !ActiveAnchor->GetComponentTransform().Equals(AnchorAtActivation, 0.01)
		|| ActiveAnchor->GetTraversedComponent() != IgnoredComponent.Get()
		|| !IgnoredComponent->GetComponentTransform().Equals(ColliderAtActivation, 0.01)
		|| !ActiveAnchor->GetLandingLocation().Equals(LandingAtActivation, 0.01)
		|| !IsCapsuleClear(*Character, LandingAtActivation + FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f))
		|| RpgMantle::IsIncapacitated(*Character))
	{
		CancelCurrentMantle(); return;
	}
	if (IsCapsuleClear(*Character, Character->GetActorLocation())) LastClearLocation = Character->GetActorLocation();
}

void URpgGameplayAbility_Mantle::HandleMovementModeChanged(ACharacter* Character, EMovementMode PreviousMode, uint8 PreviousCustomMode)
{
	if (!bEnding && bOwnsMovement && Character && Character->GetCharacterMovement()->MovementMode != MOVE_Flying)
	{
		CancelCurrentMantle();
	}
}

void URpgGameplayAbility_Mantle::CancelCurrentMantle()
{
	if (IsActive() && !bEnding) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

bool URpgGameplayAbility_Mantle::RestoreSafeCapsuleLocation(ACharacter& Character) const
{
	if (IsCapsuleClear(Character, Character.GetActorLocation())) return true;
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMantleRecovery), false, &Character);
	// The capsule can be inside only the explicitly traversed surface. Recovery still sweeps every other obstacle.
	if (IgnoredComponent.IsValid()) Params.AddIgnoredComponent(IgnoredComponent.Get());
	for (const FVector& Candidate : {LastClearLocation, EntryLocation})
	{
		if (!IsCapsuleClear(Character, Candidate)) continue;
		FHitResult Hit;
		if (!Character.GetWorld()->SweepSingleByChannel(Hit, Character.GetActorLocation(), Candidate, Capsule->GetComponentQuat(),
			Capsule->GetCollisionObjectType(), RpgMantle::CapsuleShape(Character), Params,
			FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())))
		{
			// This is rollback along a checked route to a previously clear position, never a teleport to the ledge destination.
			Character.SetActorLocation(Candidate, false, nullptr, ETeleportType::TeleportPhysics);
			return true;
		}
	}
	return false;
}

void URpgGameplayAbility_Mantle::CleanupMovement()
{
	ACharacter* Character = ActiveCharacter.Get();
	if (Character)
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
		Character->MovementModeChangedDelegate.RemoveDynamic(this, &ThisClass::HandleMovementModeChanged);
		if (bOwnsMovement)
		{
			RestoreSafeCapsuleLocation(*Character);
			if (URpgCharacterMovementComponent* RpgMovement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()))
			{
				RpgMovement->EndMantleCollisionIgnore(IgnoredComponent.Get());
			}
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			// Death/downed or a newer movement owner may have changed the mode before cancelling this ability.
			if (Movement->MovementMode == MOVE_Flying && !RpgMantle::IsIncapacitated(*Character))
			{
				Movement->StopMovementImmediately();
				FFindFloorResult Floor;
				Movement->FindFloor(Character->GetActorLocation(), Floor, false);
				Movement->SetMovementMode(Floor.IsWalkableFloor() && Floor.FloorDist <= UCharacterMovementComponent::MAX_FLOOR_DIST ? MOVE_Walking : MOVE_Falling);
			}
		}
	}
	if (ActiveWarping.IsValid()) ActiveWarping->RemoveWarpTarget(WarpTargetName);
	ActiveCharacter.Reset();
	ActiveAnchor.Reset();
	IgnoredComponent.Reset();
	ActiveWarping.Reset();
	bOwnsMovement = false;
}

void URpgGameplayAbility_Mantle::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo) || bEnding) return;
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	bEnding = true;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(TimeoutHandle);
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataDelegate);
		ASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
		if (ASC->GetAnimatingAbility() == this) ASC->CurrentMontageStop();
	}
	TargetDataDelegate.Reset();
	// Stop our montage and release movement before EndAbility broadcasts can activate the next action.
	CleanupMovement();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
