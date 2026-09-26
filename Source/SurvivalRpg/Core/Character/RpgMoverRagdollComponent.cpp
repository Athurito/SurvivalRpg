#include "RpgMoverRagdollComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "RpgCharacterMoverComponent.h"
#include "RpgHealthComponent.h"
#include "RpgPawnExtensionComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogRpgMoverRagdoll, Log, All);

namespace
{
bool HasSimulatingBodies(const USkeletalMeshComponent* Mesh)
{
	// IsSimulatingPhysics(NAME_None) returns false for ComponentTransformIsKinematic even when bodies simulate.
	return Mesh && Mesh->Bodies.ContainsByPredicate([](const FBodyInstance* Body)
	{
		return Body && Body->IsInstanceSimulatingPhysics();
	});
}
}

URpgMoverRagdollComponent::URpgMoverRagdollComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void URpgMoverRagdollComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, State);
}

void URpgMoverRagdollComponent::BeginPlay()
{
	Super::BeginPlay();
	if (URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(GetOwner()))
	{
		Health->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::OnDeathStarted);
		bDeathStarted = Health->IsDeadOrDying();
	}
	if (URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		Extension->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemInitialized));
		Extension->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemUninitialized));
	}
	ApplyState();
}

void URpgMoverRagdollComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	bAvatarDetached = true;
	OnAbilitySystemUninitialized();
	ApplyPresentation();
	// There is no future PhysicsControl tick during teardown. Stop surviving bodies before returning settings.
	TryReturnPresentationScope(true);
	if (URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(GetOwner()))
	{
		Health->OnDeathStarted.RemoveDynamic(this, &ThisClass::OnDeathStarted);
	}
	if (URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		Extension->OnAbilitySystemInitialized.RemoveAll(this);
		Extension->OnAbilitySystemUninitialized.RemoveAll(this);
	}
	OnStateChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void URpgMoverRagdollComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TryReturnPresentationScope();
}

URpgCharacterMoverComponent* URpgMoverRagdollComponent::FindMover() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgCharacterMoverComponent>() : nullptr;
}

bool URpgMoverRagdollComponent::IsAlive() const
{
	const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(GetOwner());
	return !bDeathStarted && Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.f;
}

void URpgMoverRagdollComponent::OnAbilitySystemInitialized()
{
	const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	URpgAbilitySystemComponent* ASC = Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr;
	if (ASC == BoundAbilitySystem.Get()) { return; }
	OnAbilitySystemUninitialized();
	if (ASC && ASC->GetAvatarActor() == GetOwner())
	{
		bAvatarDetached = false;
		BoundAbilitySystem = ASC;
		AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::OnAbilityEnded);
		ApplyState();
	}
}

void URpgMoverRagdollComponent::OnAbilitySystemUninitialized()
{
	if (BoundAbilitySystem.IsValid()) { bAvatarDetached = true; }
	EndEpisode(true);
	if (URpgAbilitySystemComponent* ASC = BoundAbilitySystem.Get()) { ASC->OnAbilityEnded.Remove(AbilityEndedHandle); }
	AbilityEndedHandle.Reset();
	BoundAbilitySystem.Reset();
	OwningAbility.Reset();
	if (bAvatarDetached) { ApplyPresentation(); }
}

bool URpgMoverRagdollComponent::InitializeRagdollPresentation(bool bControlsCreated)
{
	USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(GetOwner());
	if (!bControlsCreated || !Mesh || !Mesh->GetAnimInstance() || !FindMover()) { return false; }
	bPresentationReady = true;
	ApplyState();
	return true;
}

bool URpgMoverRagdollComponent::ValidateSupport(FRpgMoverRagdollState& Candidate) const
{
	const UCapsuleComponent* Capsule = GetOwner() ? Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()) : nullptr;
	const URpgCharacterMoverComponent* Mover = FindMover();
	if (!Capsule || !Mover || !GetWorld() || !Capsule->IsQueryCollisionEnabled()) { return false; }
	const FVector Center = Capsule->GetComponentLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FHitResult Floor;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RpgMoverRagdollFloor), false, GetOwner());
	if (!GetWorld()->LineTraceSingleByChannel(Floor, Center, Center - FVector(0, 0, HalfHeight + 10.f), ECC_Visibility, Query)) { return false; }
	UPrimitiveComponent* Support = Floor.GetComponent();
	// The anchored presentation scope enables physical contacts only against WorldStatic support.
	if (!Support || Support->Mobility != EComponentMobility::Static || Support->IsSimulatingPhysics() ||
		Support->GetCollisionObjectType() != ECC_WorldStatic ||
		Floor.bStartPenetrating || Floor.ImpactNormal.Z < .99f || !Mover->IsTraversalWalkable(Floor) ||
		FMath::Abs(Center.Z - HalfHeight - Floor.ImpactPoint.Z) > 5.f ||
		Support->GetCollisionResponseToChannel(Capsule->GetCollisionObjectType()) != ECR_Block ||
		Capsule->GetCollisionResponseToChannel(Support->GetCollisionObjectType()) != ECR_Block) { return false; }
	if (Candidate.Support.IsValid() && (Candidate.Support.Get() != Support ||
		!Candidate.SupportTransform.Equals(Support->GetComponentTransform(), .01f))) { return false; }
	Candidate.Support = Support;
	Candidate.SupportTransform = Support->GetComponentTransform();
	return true;
}

bool URpgMoverRagdollComponent::BeginRagdoll(UGameplayAbility* Ability)
{
	URpgCharacterMoverComponent* Mover = FindMover();
	URpgAbilitySystemComponent* ASC = BoundAbilitySystem.Get();
	if (!GetOwner()->HasAuthority() || !IsAlive() || !bPresentationReady || bEndingEpisode || bEndingPlay ||
		bAvatarDetached || bChangingPresentationScope || State.IsActive() ||
		!Ability || !Ability->IsActive() || Ability->GetAvatarActorFromActorInfo() != GetOwner() ||
		Ability->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::ServerInitiated ||
		!ASC || ASC->GetAvatarActor() != GetOwner() || Ability->GetAbilitySystemComponentFromActorInfo() != ASC ||
		!Mover || !Mover->CanBeginRagdoll(MaximumEntrySpeed) || State.Revision >= MAX_int32 - 2)
	{
		UE_LOG(LogRpgMoverRagdoll, Verbose, TEXT("Entry rejected pawn=%s authority=%d alive=%d presentation=%d ending=%d active=%d ability=%s abilityActive=%d ASC=%s moverReady=%d"),
			*GetNameSafe(GetOwner()), GetOwner()->HasAuthority(), IsAlive(), bPresentationReady, bEndingEpisode, State.IsActive(),
			*GetNameSafe(Ability), Ability && Ability->IsActive(), *GetNameSafe(ASC), Mover && Mover->CanBeginRagdoll(MaximumEntrySpeed));
		return false;
	}
	FRpgMoverRagdollState Candidate;
	Candidate.Revision = State.Revision + 1;
	Candidate.Episode = Candidate.Revision;
	Candidate.Phase = ERpgMoverRagdollPhase::Ragdoll;
	Candidate.Anchor = GetOwner()->GetActorTransform();
	Candidate.AbilityHandle = Ability->GetCurrentAbilitySpecHandle();
	const FPredictionKey& Key = Ability->GetCurrentActivationInfo().GetActivationPredictionKey();
	Candidate.ActivationPredictionKey = Key.Current;
	Candidate.bServerInitiatedKey = Key.bIsServerInitiated;
	if (!Candidate.AbilityHandle.IsValid() || !ValidateSupport(Candidate))
	{
		UE_LOG(LogRpgMoverRagdoll, Verbose, TEXT("Entry support rejected pawn=%s handleValid=%d location=%s"),
			*GetNameSafe(GetOwner()), Candidate.AbilityHandle.IsValid(), *GetOwner()->GetActorLocation().ToString());
		return false;
	}
	OwningAbility = Ability;
	RagdollStartTime = GetWorld()->GetTimeSeconds();
	State = Candidate;
	Mover->ClearAbilityRootMotion();
	ApplyState();
	GetOwner()->ForceNetUpdate();
	return true;
}

bool URpgMoverRagdollComponent::RequestGetUpSelection(UGameplayAbility* Ability)
{
	if (!GetOwner()->HasAuthority() || !IsAlive() || !bPresentationReady || bEndingEpisode ||
		State.Phase != ERpgMoverRagdollPhase::Ragdoll || OwningAbility.Get() != Ability ||
		!State.MatchesAbility(Ability) || GetWorld()->GetTimeSeconds() - RagdollStartTime < MinimumRagdollDuration ||
		!OnGetUpSelectionRequested.IsBound()) { return false; }
	OnGetUpSelectionRequested.Broadcast(Ability);
	return State.Phase == ERpgMoverRagdollPhase::GettingUp && State.MatchesAbility(Ability);
}

bool URpgMoverRagdollComponent::PublishGetUpSelection(UGameplayAbility* Ability, UAnimMontage* Montage, float StartTimeSeconds)
{
	if (!GetOwner()->HasAuthority() || !IsAlive() || bEndingEpisode || !bPresentationReady ||
		State.Phase != ERpgMoverRagdollPhase::Ragdoll || OwningAbility.Get() != Ability || !State.MatchesAbility(Ability) ||
		GetWorld()->GetTimeSeconds() - RagdollStartTime < MinimumRagdollDuration || !IsGetUpMontage(Montage) ||
		!Montage->HasRootMotion() || !FMath::IsFinite(StartTimeSeconds) || StartTimeSeconds < 0.f || StartTimeSeconds >= Montage->GetPlayLength() ||
		Montage->TimeStretchCurve.IsValid() || Montage->CompositeSections.Num() != 1 ||
		!Montage->CompositeSections[0].NextSectionName.IsNone() || !FMath::IsNearlyZero(Montage->CompositeSections[0].GetTime()) ||
		!FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= UE_SMALL_NUMBER) { return false; }
	FRpgMoverRagdollState Candidate = State;
	if (!ValidateSupport(Candidate)) { return false; }
	++Candidate.Revision;
	Candidate.Phase = ERpgMoverRagdollPhase::GettingUp;
	Candidate.GetUpMontage = Montage;
	Candidate.GetUpStartTime = StartTimeSeconds;
	Candidate.RetainObjectsForHistory();
	State = Candidate;
	ApplyState();
	GetOwner()->ForceNetUpdate();
	return true;
}

void URpgMoverRagdollComponent::OnAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (EndedData.AbilityThatEnded == OwningAbility.Get() && State.MatchesAbility(EndedData.AbilityThatEnded))
	{
		EndEpisode(EndedData.bWasCancelled || State.Phase != ERpgMoverRagdollPhase::GettingUp);
	}
}

void URpgMoverRagdollComponent::EndEpisode(bool bCancelled)
{
	if (bEndingEpisode || !State.IsActive() || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	TGuardValue<bool> Guard(bEndingEpisode, true);
	// Retire the episode before callbacks. A callback cannot release a replacement activation.
	OwningAbility.Reset();
	++State.Revision;
	State.Phase = ERpgMoverRagdollPhase::Inactive;
	State.bCancelled = bCancelled;
	State.GetUpMontage = nullptr;
	State.Support.Reset();
	State.RetainObjectsForHistory();
	ApplyState();
	GetOwner()->ForceNetUpdate();
}

void URpgMoverRagdollComponent::OnDeathStarted(AActor* OwningActor) { HandleMovementDeath(); }
void URpgMoverRagdollComponent::HandleMovementDeath()
{
	if (bDeathStarted) { return; }
	bDeathStarted = true;
	// A simulated proxy has no getup ability task to cancel. Health can arrive before the ASC montage
	// stop, so close the existing GAS-owned playback as well as rejecting future replicated starts.
	// The selected state may already be terminal; inspect only this avatar's current whitelisted montage.
	if (URpgAbilitySystemComponent* ASC = BoundAbilitySystem.Get(); ASC && ASC->GetAvatarActor() == GetOwner())
	{
		if (UAnimMontage* Montage = ASC->GetCurrentMontage(); IsGetUpMontage(Montage))
		{
			UE_LOG(LogRpgMoverRagdoll, Verbose, TEXT("Death stops GAS getup world=%s pawn=%s role=%d montage=%s"),
				*GetPathNameSafe(GetWorld()), *GetNameSafe(GetOwner()), static_cast<int32>(GetOwner()->GetLocalRole()), *GetNameSafe(Montage));
			ASC->StopMontageIfCurrent(*Montage);
		}
	}
	EndEpisode(true);
	ApplyPresentation();
	OnStateChanged.Broadcast();
}

void URpgMoverRagdollComponent::OnRep_State() { ApplyState(); }
void URpgMoverRagdollComponent::ApplyState()
{
	State.RetainObjectsForHistory();
	if (URpgCharacterMoverComponent* Mover = FindMover()) { Mover->SetRagdollCommand(State); }
	ApplyPresentation();
	OnStateChanged.Broadcast();
	if (URpgAbilitySystemComponent* ASC = BoundAbilitySystem.Get()) { ASC->RefreshReplicatedRagdollMontage(); }
}

void URpgMoverRagdollComponent::ApplyPresentation()
{
	if (!bPresentationReady || !HasBegunPlay() || bChangingPresentationScope) { return; }
	FRpgMoverRagdollState Next = State;
	if (bDeathStarted || bAvatarDetached || bEndingPlay) { Next.Phase = ERpgMoverRagdollPhase::Inactive; }
	if (PresentedState.Revision == Next.Revision && PresentedState.Phase == Next.Phase) { return; }
	if (Next.Phase == ERpgMoverRagdollPhase::Ragdoll)
	{
		if (!EnterPresentationScope()) { return; }
		// Collision callbacks may synchronously cancel the episode or start death during scope entry.
		if (State.Revision != Next.Revision || bDeathStarted || bAvatarDetached || bEndingPlay)
		{
			bPresentationReturnPending = true;
			SetComponentTickEnabled(true);
			ApplyPresentation();
			return;
		}
	}
	else if (bPresentationScopeActive)
	{
		// The Blueprint notification applies the normal profile, but its bodies change on PhysicsControl's next tick.
		bPresentationReturnPending = true;
		SetComponentTickEnabled(true);
	}
	const ERpgMoverRagdollPhase Previous = PresentedState.Phase;
	PresentedState = Next;
	UE_LOG(LogRpgMoverRagdoll, Verbose, TEXT("Presentation world=%s pawn=%s role=%d phase=%d->%d revision=%d bound=%d"),
		*GetPathNameSafe(GetWorld()), *GetNameSafe(GetOwner()), static_cast<int32>(GetOwner()->GetLocalRole()),
		static_cast<int32>(Previous), static_cast<int32>(Next.Phase), Next.Revision, OnRagdollPresentationChanged.IsBound());
	OnRagdollPresentationChanged.Broadcast(Previous, Next.Phase, bDeathStarted);
}

bool URpgMoverRagdollComponent::EnterPresentationScope()
{
	USkeletalMeshComponent* Mesh = URpgPawnExtensionComponent::FindGameplayMesh(GetOwner());
	URpgCharacterMoverComponent* Mover = FindMover();
	if (!Mesh || !Mesh->IsRegistered() || !Mover || Mover->GetPrimaryVisualComponent() != Mesh)
	{
		UE_LOG(LogRpgMoverRagdoll, Verbose, TEXT("Presentation scope rejected pawn=%s mesh=%s registered=%d mover=%s primary=%s"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Mesh), Mesh && Mesh->IsRegistered(), *GetNameSafe(Mover),
			*GetNameSafe(Mover ? Mover->GetPrimaryVisualComponent() : nullptr));
		return false;
	}
	if (bPresentationScopeActive)
	{
		if (ScopedMesh.Get() != Mesh || ScopedMover.Get() != Mover)
		{
			UE_LOG(LogRpgMoverRagdoll, Verbose, TEXT("Presentation scope identity changed pawn=%s mesh=%s mover=%s"),
				*GetNameSafe(GetOwner()), *GetNameSafe(Mesh), *GetNameSafe(Mover));
			return false;
		}
		// A replacement episode before the normal-profile tick retains the original baseline, never the scoped values.
		bPresentationReturnPending = false;
		SetComponentTickEnabled(false);
		return true;
	}
	if (Mesh->Bodies.IsEmpty() || HasSimulatingBodies(Mesh))
	{
		UE_LOG(LogRpgMoverRagdoll, Verbose, TEXT("Presentation scope bodies not ready pawn=%s mesh=%s bodies=%d simulating=%d"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Mesh), Mesh->Bodies.Num(), HasSimulatingBodies(Mesh));
		return false;
	}
	TGuardValue<bool> Guard(bChangingPresentationScope, true);
	ScopedMesh = Mesh;
	ScopedMover = Mover;
	SavedMeshCollisionProfile = Mesh->GetCollisionProfileName();
	SavedMeshCollisionEnabled = Mesh->GetCollisionEnabled();
	SavedMeshResponses = Mesh->GetCollisionResponseToChannels();
	SavedPhysicsTransformUpdateMode = Mesh->PhysicsTransformUpdateMode;
	SavedSmoothingMode = Mover->SmoothingMode;
	bSavedMeshSimulatePhysics = Mesh->BodyInstance.bSimulatePhysics;
	bSavedMeshBlendPhysics = Mesh->bBlendPhysics;
	bPresentationScopeActive = true;
	bPresentationReturnPending = false;
	// Fixed Mover smoothing otherwise writes to simulated bodies. Keep the canonical visual identity/base offset
	// and normalize once while still kinematic; the anchored scope then leaves that relative transform unchanged.
	Mover->SmoothingMode = EMoverSmoothingMode::None;
	Mesh->SetRelativeTransform(Mover->GetBaseVisualComponentTransform());
	Mesh->PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::ComponentTransformIsKinematic;
	// RpgPawnMesh is QueryOnly and ignores WorldStatic. Physics blending and floor contacts require both changes.
	// Other authored responses, including ignoring pawns/the owning capsule, remain intact.
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	return true;
}

void URpgMoverRagdollComponent::TryReturnPresentationScope(bool bForEndPlay)
{
	if (!bPresentationScopeActive || (!bPresentationReturnPending && !bForEndPlay) || bChangingPresentationScope) { return; }
	USkeletalMeshComponent* Mesh = ScopedMesh.Get();
	if (HasSimulatingBodies(Mesh))
	{
		if (!bForEndPlay) { return; }
		Mesh->SetAllBodiesSimulatePhysics(false);
	}
	{
		TGuardValue<bool> Guard(bChangingPresentationScope, true);
		URpgCharacterMoverComponent* Mover = ScopedMover.Get();
		// Retire local ownership before collision callbacks; a later episode must capture a fresh baseline.
		bPresentationScopeActive = false;
		bPresentationReturnPending = false;
		ScopedMesh.Reset();
		ScopedMover.Reset();
		SetComponentTickEnabled(false);
		if (Mesh)
		{
			// Blueprint SetSimulatePhysics also changes these component flags. PhysicsControl's kinematic
			// profile only stops individual bodies. Restore the flags before disabling physics collision;
			// do not call SetSimulatePhysics(true), which could restart the bodies just released by the profile.
			Mesh->SetSimulatePhysics(false);
			Mesh->BodyInstance.bSimulatePhysics = bSavedMeshSimulatePhysics;
			Mesh->SetEnablePhysicsBlending(bSavedMeshBlendPhysics);
			Mesh->SetCollisionProfileName(SavedMeshCollisionProfile);
			Mesh->SetCollisionResponseToChannels(SavedMeshResponses);
			Mesh->SetCollisionEnabled(SavedMeshCollisionEnabled);
			Mesh->PhysicsTransformUpdateMode = SavedPhysicsTransformUpdateMode;
		}
		if (Mover) { Mover->SmoothingMode = SavedSmoothingMode; }
	}
	if (!bForEndPlay)
	{
		ApplyPresentation();
		// Selection may have arrived a frame earlier. Wake the GAS task and proxy montage gate only after release.
		OnStateChanged.Broadcast();
		if (URpgAbilitySystemComponent* ASC = BoundAbilitySystem.Get()) { ASC->RefreshReplicatedRagdollMontage(); }
	}
}

bool URpgMoverRagdollComponent::CanConsumeSelection(const UGameplayAbility* Ability) const
{
	return !bDeathStarted && !bAvatarDetached && !bEndingPlay && bPresentationReady && !bPresentationScopeActive &&
		!bChangingPresentationScope && State.Phase == ERpgMoverRagdollPhase::GettingUp &&
		PresentedState.Revision == State.Revision && State.MatchesAbility(Ability) && State.GetUpMontage;
}
bool URpgMoverRagdollComponent::AllowsRootMotion(const UAnimMontage* Montage) const
{
	return !State.IsActive() || (!bDeathStarted && State.Phase == ERpgMoverRagdollPhase::GettingUp && State.GetUpMontage == Montage);
}
bool URpgMoverRagdollComponent::IsGetUpMontage(const UAnimMontage* Montage) const
{
	return Montage && GetUpMontages.Contains(Montage);
}
bool URpgMoverRagdollComponent::IsReadyForReplicatedGetUp(const UAnimMontage* Montage, uint8 PlayId) const
{
	return !IsGetUpMontage(Montage) || (!bDeathStarted && !bAvatarDetached && !bEndingPlay && bPresentationReady &&
		!bPresentationScopeActive && !bChangingPresentationScope &&
		State.Phase == ERpgMoverRagdollPhase::GettingUp && State.GetUpMontage == Montage &&
		State.bHasGetUpPlayId && State.GetUpPlayId == PlayId && PresentedState.Revision == State.Revision);
}

void URpgMoverRagdollComponent::NotifyGetUpMontageStarted(const UGameplayAbility* Ability, uint8 PlayId)
{
	if (!GetOwner()->HasAuthority() || !IsAlive() || State.Phase != ERpgMoverRagdollPhase::GettingUp || !State.MatchesAbility(Ability)) { return; }
	State.GetUpPlayId = PlayId;
	State.bHasGetUpPlayId = true;
	GetOwner()->ForceNetUpdate();
}
