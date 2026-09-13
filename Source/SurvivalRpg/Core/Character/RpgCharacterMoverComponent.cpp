#include "RpgCharacterMoverComponent.h"

#include "RpgDeadMovementMode.h"
#include "RpgMoverMotionWarpingComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulation.h"
#include "MoverSimulationTypes.h"
#include "SurvivalRpg/SurvivalRpg.h"

namespace RpgAbilityRootMotion
{
bool MatchesPlayback(const FRpgMoverAbilityRootMotion& A, const FRpgMoverAbilityRootMotion& B)
{
	return A.AbilityHandle == B.AbilityHandle && A.ActivationPredictionKey == B.ActivationPredictionKey &&
		A.bServerInitiatedKey == B.bServerInitiatedKey && A.MontageSequence == B.MontageSequence &&
		A.MontageState.Montage == B.MontageState.Montage;
}

bool MatchesTraversal(const FRpgMoverAbilityRootMotion& Move, const FRpgMoverTraversalCommand& Command)
{
	return Command.IsActive() && Command.Identity.AbilityHandle == Move.AbilityHandle &&
		Command.Identity.ActivationPredictionKey == Move.ActivationPredictionKey &&
		Command.Identity.bServerInitiatedKey == Move.bServerInitiatedKey &&
		Command.Identity.MontageSequence == Move.MontageSequence && Command.Context.Montage == Move.MontageState.Montage;
}
}

bool FRpgMoverAbilityRootMotion::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
	const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const URpgCharacterMoverComponent* RpgMover = Cast<URpgCharacterMoverComponent>(MoverComp);
	if (!RpgMover || RpgMover->IsMovementDisabledForDeath(StartState.SyncState, TimeStep))
	{
		DurationMs = 0.0f;
		return false;
	}

	if (RpgMover->GetOwnerRole() == ROLE_Authority)
	{
		// Authority uses only the montage started by its own GAS activation, never a client's playback input.
		if (!RpgMover->IsAbilityRootMotionCurrent(*this))
		{
			DurationMs = 0.0f;
			return false;
		}
	}
	else
	{
		const FRpgMoverAbilityRootMotionInputs* Inputs = StartState.InputCmd.InputCollection.FindDataByType<FRpgMoverAbilityRootMotionInputs>();
		if (RpgMover->GetOwnerRole() != ROLE_AutonomousProxy ||
			(!(Inputs && RpgAbilityRootMotion::MatchesPlayback(*this, Inputs->RootMotion)) &&
			 !RpgAbilityRootMotion::MatchesTraversal(*this, RpgMover->TraversalSimulationState.Command)))
		{
			DurationMs = 0.0f;
			return false;
		}
	}

	// The NP backend does not set bIsResimulating. Its stored input nevertheless identifies the exact
	// historical interval. Only bypass the engine's present-day AnimInstance check, after that validation;
	// preserve the real timestep, simulation state, extraction and movement collision handling.
	FMoverTimeStep ExtractionTimeStep = TimeStep;
	ExtractionTimeStep.bIsResimulating = RpgMover->GetOwnerRole() == ROLE_AutonomousProxy || TimeStep.bIsResimulating;
	URpgCharacterMoverComponent* MutableMover = const_cast<URpgCharacterMoverComponent*>(RpgMover);
	const FRpgMoverTraversalCommand& Traversal = MutableMover->TraversalSimulationState.Command;
	const bool bTraversalIdentity = Traversal.Identity.AbilityHandle == AbilityHandle &&
		Traversal.Identity.ActivationPredictionKey == ActivationPredictionKey &&
		Traversal.Identity.bServerInitiatedKey == bServerInitiatedKey && Traversal.Identity.MontageSequence == MontageSequence;
	if (bTraversalIdentity && !Traversal.IsActive()) { DurationMs = 0.f; return false; }
	if (Traversal.IsActive() && !bTraversalIdentity) { DurationMs = 0.f; return false; }
	if (bTraversalIdentity && MutableMover->bTraversalGeometryInvalidThisTick)
	{
		OutProposedMove = FProposedMove{};
		OutProposedMove.MixMode = EMoveMixMode::OverrideAll;
		OutProposedMove.PreferredMode = TEXT("Traversing");
		return true;
	}
	const bool bTraversalScope = MutableMover->BeginTraversalRootMotion(*this, StartState);
	if (bTraversalIdentity && !bTraversalScope) { DurationMs = 0.f; return false; }
	const bool bGenerated = Super::GenerateMove(StartState, ExtractionTimeStep, MoverComp, SimBlackboard, OutProposedMove);
	if (bTraversalScope)
	{
		MutableMover->EndTraversalRootMotion(MontageState.CurrentPosition);
		OutProposedMove.PreferredMode = TEXT("Traversing");
	}
	return bGenerated;
}

FLayeredMoveBase* FRpgMoverAbilityRootMotion::Clone() const
{
	return new FRpgMoverAbilityRootMotion(*this);
}

void FRpgMoverAbilityRootMotion::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	Ar << AbilityHandle;
	Ar << ActivationPredictionKey;
	Ar << bServerInitiatedKey;
	Ar << MontageSequence;
}

UScriptStruct* FRpgMoverAbilityRootMotion::GetScriptStruct() const
{
	return StaticStruct();
}

void FRpgMoverAbilityRootMotion::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(MontageState.Montage);
}

void FRpgMoverAbilityRootMotionInputs::RetainMontageForHistory()
{
	MontageLifetime.Reset(RootMotion.MontageState.Montage.Get());
	Traversal.RetainObjectsForHistory();
}

FMoverDataStructBase* FRpgMoverAbilityRootMotionInputs::Clone() const
{
	return new FRpgMoverAbilityRootMotionInputs(*this);
}

UScriptStruct* FRpgMoverAbilityRootMotionInputs::GetScriptStruct() const
{
	return StaticStruct();
}

bool FRpgMoverAbilityRootMotionInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	// IndependentRollback replaces Sync/Aux only, then replays the owner's original InputFrameData.
	// Keep these playback intervals in that local history; authority samples its own GAS instance.
	// No client-selected asset, play rate or root-motion displacement crosses the network in this input.
	if (Ar.IsLoading())
	{
		RootMotion = FRpgMoverAbilityRootMotion{};
		Traversal = FRpgMoverTraversalCommand{};
		RetainMontageForHistory();
	}
	bOutSuccess = true;
	return true;
}

void FRpgMoverAbilityRootMotionInputs::AddReferencedObjects(FReferenceCollector& Collector)
{
	RootMotion.AddReferencedObjects(Collector);
	Traversal.AddReferencedObjects(Collector);
}

bool FRpgMoverAbilityRootMotionInputs::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	return false;
}

void FRpgMoverAbilityRootMotionInputs::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	// Playback identities and stop boundaries are discrete, including when inputs accompany interpolated proxies.
	*this = static_cast<const FRpgMoverAbilityRootMotionInputs&>(Pct < 1.0f ? From : To);
}

void URpgCharacterMoverComponent::BeginPlay()
{
	// The copied Blueprint serializes its own MovementModes map, so register this engine-facing lifecycle
	// mode on the instance as well as the simulation. Authored GASP modes and tuning remain untouched.
	if (!MovementModes.Contains(URpgDeadMovementMode::ModeName))
	{
		AddMovementModeFromClass(URpgDeadMovementMode::ModeName, URpgDeadMovementMode::StaticClass());
	}
	PersistentSyncStateDataTypes.Add(FMoverDataPersistence(FRpgMoverTraversalSyncState::StaticStruct(), true));
	TraversalWarping = GetOwner()->FindComponentByClass<URpgMoverMotionWarpingComponent>();
	Super::BeginPlay();
	OnPreSimulationTick.AddUniqueDynamic(this, &ThisClass::HandleAbilityRootMotionPreSimulation);
	OnPostFinalize.AddUniqueDynamic(this, &ThisClass::HandleTraversalPostFinalize);
}

void URpgCharacterMoverComponent::DisableMovementForDeath()
{
	if (bDeathMovementRequested)
	{
		return;
	}
	bDeathMovementRequested = true;
	DeathMovementStartTimeMs = BackendLiaisonComp ? BackendLiaisonComp->GetCurrentSimTimeMs() : 0.0;
	if (TraversalCommand.IsActive())
	{
		TraversalCommand.Phase = ERpgMoverTraversalPhase::Cancelled;
		TraversalCommand.bHasRecoveryLocation = false;
		TraversalCommand.bPreserveMomentum = false;
	}
	ClearAbilityRootMotion();
}

bool URpgCharacterMoverComponent::IsMovementDisabledForDeath(const FMoverSyncState& SyncState, const FMoverTimeStep& TimeStep) const
{
	return SyncState.MovementMode == URpgDeadMovementMode::ModeName ||
		(bDeathMovementRequested && TimeStep.BaseSimTimeMs >= DeathMovementStartTimeMs);
}

void URpgCharacterMoverComponent::OnPreSimulate(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartingData)
{
	bSuppressMovementForDeathThisTick = IsMovementDisabledForDeath(StartingData.SyncState, TimeStep);
	PrepareTraversalSimulation(TimeStep, StartingData);
	if (bSuppressMovementForDeathThisTick)
	{
		// The NP liaison deep-copies its stored command into StartingData before this callback. Mutating
		// its collection changes only this simulation tick, preserving living input history for rollback.
		if (FCharacterDefaultInputs* Inputs = StartingData.InputCmd.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>())
		{
			*Inputs = FCharacterDefaultInputs{};
		}
		if (FRpgMoverAbilityRootMotionInputs* Inputs = StartingData.InputCmd.InputCollection.FindMutableDataByType<FRpgMoverAbilityRootMotionInputs>())
		{
			Inputs->RootMotion = FRpgMoverAbilityRootMotion{};
			Inputs->RetainMontageForHistory();
		}
		// Preserve the collision stance reached at death; held GASP crouch input must not change it.
		bWantsToCrouch = IsCrouching();
	}

	// A copied GASP custom-input handler can update crouch intent during this broadcast. Disable native
	// stance processing for the scope, then restore its setting and the already-reached stance.
	const bool bLockStance = bSuppressMovementForDeathThisTick || TraversalSimulationState.Command.IsActive();
	const bool bSavedStanceHandling = bHandleStanceChanges;
	if (bLockStance) { bHandleStanceChanges = false; }
	Super::OnPreSimulate(TimeStep, StartingData);
	bHandleStanceChanges = bSavedStanceHandling;
	if (bLockStance) { bWantsToCrouch = IsCrouching(); }

	if (!bSuppressMovementForDeathThisTick && BackendLiaisonComp && Simulation && !IsBackendAsync() && TimeStep.StepMs > 0.f)
	{
		FRpgMoverTraversalCommand& Command = TraversalSimulationState.Command;
		const bool bStart = Command.IsActive() && !TraversalSimulationState.bStartApplied;
		const bool bEnd = Command.IsTerminal() && !TraversalSimulationState.bEndApplied;
		if (bStart || bEnd || bTraversalGeometryInvalidThisTick)
		{
			const FMoverTime FrameTime(TimeStep.ServerFrame, TimeStep.BaseSimTimeMs);
			const FMoverSchedulingInfo Scheduling(FrameTime, FrameTime, BackendLiaisonComp->IsFixedDt());
			const FMoverDefaultSyncState* Default = StartingData.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			if (bEnd && Command.bHasRecoveryLocation)
			{
				TSharedPtr<FTeleportEffect> Recovery = MakeShared<FTeleportEffect>();
				Recovery->TargetLocation = Command.RecoveryCapsuleLocation;
				Recovery->bUseActorRotation = true;
				Simulation->QueueInstantMovementEffect(FScheduledInstantMovementEffect(Scheduling, Recovery));
			}
			TSharedPtr<FApplyVelocityEffect> Transition = MakeShared<FApplyVelocityEffect>();
			Transition->VelocityToApply = Default && !bTraversalGeometryInvalidThisTick && (bStart || Command.bPreserveMomentum) ? Default->GetVelocity_WorldSpace() : FVector::ZeroVector;
			Transition->bAdditiveVelocity = false;
			// Falling performs a fresh floor check with the obstacle restored; no actor-only landing snap.
			Transition->ForceMovementMode = Command.IsActive() ? FName(TEXT("Traversing")) : DefaultModeNames::Falling;
			Simulation->QueueInstantMovementEffect(FScheduledInstantMovementEffect(Scheduling, Transition));
			TraversalSimulationState.bStartApplied |= bStart;
			TraversalSimulationState.bEndApplied |= bEnd;
		}
	}

	if (bSuppressMovementForDeathThisTick && BackendLiaisonComp && Simulation && !IsBackendAsync() && TimeStep.StepMs > 0.0f)
	{
		// Queue after engine/GASP callbacks. The instant effect wins over queued jump impulses and layered
		// preferred modes in this tick; the terminal mode discards any remaining movement contribution.
		TSharedPtr<FApplyVelocityEffect> StopEffect = MakeShared<FApplyVelocityEffect>();
		StopEffect->VelocityToApply = FVector::ZeroVector;
		StopEffect->bAdditiveVelocity = false;
		StopEffect->ForceMovementMode = URpgDeadMovementMode::ModeName;
		const FMoverTime FrameTime(TimeStep.ServerFrame, TimeStep.BaseSimTimeMs);
		const FMoverSchedulingInfo Scheduling(FrameTime, FrameTime, BackendLiaisonComp->IsFixedDt());
		Simulation->QueueInstantMovementEffect(FScheduledInstantMovementEffect(Scheduling, StopEffect));
	}
}

void URpgCharacterMoverComponent::ProduceInput(int32 DeltaTimeMS, FMoverInputCmdContext* Cmd)
{
	Super::ProduceInput(DeltaTimeMS, Cmd);
	FRpgMoverAbilityRootMotionInputs& Inputs = Cmd->InputCollection.FindOrAddMutableDataByType<FRpgMoverAbilityRootMotionInputs>();
	Inputs.Traversal = TraversalCommand;
	const bool bDeathInput = bDeathMovementRequested || GetSyncState().MovementMode == URpgDeadMovementMode::ModeName;
	if (!BackendLiaisonComp || bDeathInput || !SampleAbilityRootMotion(BackendLiaisonComp->GetCurrentSimTimeMs(), Inputs.RootMotion))
	{
		Inputs.RootMotion = FRpgMoverAbilityRootMotion{};
	}
	if (bDeathInput)
	{
		Cmd->InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>() = FCharacterDefaultInputs{};
	}
	Inputs.RetainMontageForHistory();
	CachedLastProducedInputCmd = *Cmd;
}

uint32 URpgCharacterMoverComponent::BeginTraversal(UGameplayAbility* Ability, const FPredictionKey& ActivationKey,
	const FRpgMoverTraversalRequest& Request)
{
	if (!Ability || Ability->GetAvatarActorFromActorInfo() != GetOwner() || !Ability->GetCurrentAbilitySpecHandle().IsValid() ||
		(GetOwnerRole() != ROLE_Authority && GetOwnerRole() != ROLE_AutonomousProxy) ||
		!BackendLiaisonComp || !Simulation || IsBackendAsync() || bDeathMovementRequested || HasTraversalLease() ||
		!IsOnGround() || IsCrouching() || !MovementModes.Contains(TEXT("Traversing")) ||
		!Request.Collider.IsValid() || Request.Collider->IsSimulatingPhysics() ||
		!Request.Collider->GetComponentTransform().Equals(Request.ColliderTransform, .1f) ||
		Request.ColliderTransform.ContainsNaN() || Request.FrontLedgeTarget.ContainsNaN() ||
		Request.EntryCapsuleLocation.ContainsNaN() || Request.LandingCapsuleLocation.ContainsNaN() ||
		!FMath::IsFinite(Request.StartTimeSeconds) || !FMath::IsFinite(Request.PlayRate) ||
		!FMath::IsFinite(Request.HandoffTimeSeconds) || Request.StartTimeSeconds < 0.f ||
		Request.PlayRate <= UE_SMALL_NUMBER || Request.HandoffTimeSeconds <= Request.StartTimeSeconds ||
		!TraversalWarping || !TraversalWarping->SupportsTraversal(Request)) { return 0; }

	const FRpgMoverTraversalCommand PreviousCommand = TraversalCommand;
	const FRpgMoverTraversalSyncState* ObservedSync = GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
	TraversalCommand = FRpgMoverTraversalCommand{};
	TraversalCommand.RecordPredecessors(PreviousCommand, ObservedSync ? ObservedSync->Command.Identity : FRpgMoverTraversalIdentity{});
	TraversalCommand.Identity.AbilityHandle = Ability->GetCurrentAbilitySpecHandle();
	TraversalCommand.Identity.ActivationPredictionKey = ActivationKey.Current;
	TraversalCommand.Identity.bServerInitiatedKey = ActivationKey.bIsServerInitiated;
	TraversalCommand.Identity.MontageSequence = LastAbilityHandle == Ability->GetCurrentAbilitySpecHandle() && LastActivationKey == ActivationKey ?
		LastMontageSequence + 1 : 1;
	if (!TraversalCommand.Identity.MontageSequence) { TraversalCommand.Identity.MontageSequence = 1; }
	TraversalCommand.Phase = ERpgMoverTraversalPhase::Active;
	TraversalCommand.Context = Request;
	TraversalCommand.BaseVisualTransform = GetBaseVisualComponentTransform();
	TraversalCommand.RetainObjectsForHistory();
	return TraversalCommand.Identity.MontageSequence;
}

void URpgCharacterMoverComponent::EndTraversal(FGameplayAbilitySpecHandle Handle, const FPredictionKey& ActivationKey,
	uint32 LeaseSequence, bool bPreserveMomentum, TOptional<FVector> RecoveryCapsuleLocation)
{
	const FRpgMoverTraversalIdentity& Identity = TraversalCommand.Identity;
	if (!TraversalCommand.IsActive() || Identity.AbilityHandle != Handle || Identity.ActivationPredictionKey != ActivationKey.Current ||
		Identity.bServerInitiatedKey != ActivationKey.bIsServerInitiated || Identity.MontageSequence != LeaseSequence) { return; }
	TraversalCommand.Phase = bPreserveMomentum ? ERpgMoverTraversalPhase::Finished : ERpgMoverTraversalPhase::Cancelled;
	TraversalCommand.bPreserveMomentum = bPreserveMomentum && !bDeathMovementRequested;
	TraversalCommand.bHasRecoveryLocation = !bDeathMovementRequested && RecoveryCapsuleLocation.IsSet() && !RecoveryCapsuleLocation->ContainsNaN();
	TraversalCommand.RecoveryCapsuleLocation = TraversalCommand.bHasRecoveryLocation ? RecoveryCapsuleLocation.GetValue() : FVector::ZeroVector;
}

const FRpgMoverTraversalCommand& URpgCharacterMoverComponent::GetVisibleTraversalCommand() const
{
	if (const FRpgMoverTraversalSyncState* Sync = GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>())
	{
		if (GetOwnerRole() == ROLE_SimulatedProxy ||
			(Sync->Command.Identity == TraversalCommand.Identity && Sync->Command.IsTerminal())) { return Sync->Command; }
	}
	return TraversalCommand;
}

bool URpgCharacterMoverComponent::HasTraversalLease() const { return !bDeathMovementRequested && GetVisibleTraversalCommand().IsActive(); }
bool URpgCharacterMoverComponent::OwnsTraversalLease(FGameplayAbilitySpecHandle Handle, const FPredictionKey& ActivationKey, uint32 LeaseSequence) const
{
	const FRpgMoverTraversalCommand& Command = GetVisibleTraversalCommand();
	return HasTraversalLease() && Command.Identity.AbilityHandle == Handle && Command.Identity.ActivationPredictionKey == ActivationKey.Current &&
		Command.Identity.bServerInitiatedKey == ActivationKey.bIsServerInitiated && Command.Identity.MontageSequence == LeaseSequence;
}
UPrimitiveComponent* URpgCharacterMoverComponent::GetTraversalCollider() const
{
	return HasTraversalLease() ? GetVisibleTraversalCommand().Context.Collider.Get() : nullptr;
}
const UMotionWarpingBaseAdapter* URpgCharacterMoverComponent::GetTraversalWarpingAdapter() const
{
	return TraversalWarping ? TraversalWarping->GetTraversalAdapter(const_cast<URpgCharacterMoverComponent*>(this)) : nullptr;
}
bool URpgCharacterMoverComponent::IsTraversalWalkable(const FHitResult& Hit) const
{
	const UCommonLegacyMovementSettings* Settings = FindSharedSettings<UCommonLegacyMovementSettings>();
	return Settings && UFloorQueryUtils::IsHitSurfaceWalkable(Hit, GetUpDirection(), Settings->MaxWalkSlopeCosine);
}

void URpgCharacterMoverComponent::PrepareTraversalSimulation(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartingData)
{
	bTraversalGeometryInvalidThisTick = false;
	bRestoreTraversalPresentationInputs = false;
	const FRpgMoverTraversalSyncState* Prior = StartingData.SyncState.SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
	TraversalSimulationState = Prior ? *Prior : FRpgMoverTraversalSyncState{};
	const FRpgMoverTraversalCommand* Command = nullptr;
	if (GetOwnerRole() == ROLE_Authority) { Command = &TraversalCommand; }
	else if (GetOwnerRole() == ROLE_AutonomousProxy)
	{
		if (const FRpgMoverAbilityRootMotionInputs* Input = StartingData.InputCmd.InputCollection.FindDataByType<FRpgMoverAbilityRootMotionInputs>())
		{
			Command = &Input->Traversal;
		}
	}
	if (Command && Command->Phase != ERpgMoverTraversalPhase::None)
	{
		if (!(TraversalSimulationState.Command.Identity == Command->Identity) &&
			(GetOwnerRole() == ROLE_Authority || TraversalSimulationState.Command.Phase == ERpgMoverTraversalPhase::None ||
			 Command->IsSuccessorOf(TraversalSimulationState.Command.Identity)))
		{
			TraversalSimulationState = FRpgMoverTraversalSyncState{};
			TraversalSimulationState.Command = *Command;
			TraversalSimulationState.MontagePosition = Command->Context.StartTimeSeconds;
		}
		else if (TraversalSimulationState.Command.Identity == Command->Identity &&
			!TraversalSimulationState.Command.IsTerminal() && Command->IsTerminal())
		{
			// Preserve corrected immutable target/base values and mutable warp caches. Only the historical end
			// intent is local; an authoritative terminal state cannot be resurrected by an older active input.
			TraversalSimulationState.Command.Phase = Command->Phase;
			TraversalSimulationState.Command.bPreserveMomentum = Command->bPreserveMomentum;
			TraversalSimulationState.Command.bHasRecoveryLocation = Command->bHasRecoveryLocation;
			TraversalSimulationState.Command.RecoveryCapsuleLocation = Command->RecoveryCapsuleLocation;
		}
	}
	FRpgMoverTraversalCommand& Active = TraversalSimulationState.Command;
	if (Active.IsActive())
	{
		const bool bModeTakenOver = TraversalSimulationState.bStartApplied && StartingData.SyncState.MovementMode != TEXT("Traversing");
		if (bSuppressMovementForDeathThisTick || bModeTakenOver)
		{
			Active.Phase = ERpgMoverTraversalPhase::Cancelled;
			Active.bHasRecoveryLocation = false;
			Active.bPreserveMomentum = false;
			// A newer movement owner has already selected its mode/velocity; relinquish only our resources.
			TraversalSimulationState.bEndApplied |= bModeTakenOver;
		}
		else
		{
			// Stop before using changed geometry, then let GAS' post-finalize validator select its already
			// established collision-safe recovery. Keeping ownership until that callback permits exact cleanup.
			bTraversalGeometryInvalidThisTick = !Active.Context.Collider.IsValid() ||
				!Active.Context.Collider->GetComponentTransform().Equals(Active.Context.ColliderTransform, .01f) ||
				!TraversalWarping || !TraversalWarping->SupportsTraversal(Active.Context);
			if (FCharacterDefaultInputs* Input = StartingData.InputCmd.InputCollection.FindMutableDataByType<FCharacterDefaultInputs>())
			{
				// Full motion, including orientation, belongs to the authored root motion while traversing.
				TraversalPresentationInputs = *Input;
				bRestoreTraversalPresentationInputs = true;
				*Input = FCharacterDefaultInputs{};
			}
		}
	}
	if (bSuppressMovementForDeathThisTick && Active.IsTerminal()) { TraversalSimulationState.bEndApplied = true; }
	ApplyTraversalCollisionLease(Active.IsActive() ? Active.Context.Collider.Get() : nullptr);
}

bool URpgCharacterMoverComponent::BeginTraversalRootMotion(const FRpgMoverAbilityRootMotion& Move, const FMoverTickStartData& StartState)
{
	const FRpgMoverTraversalCommand& Command = TraversalSimulationState.Command;
	if (!Command.IsActive() || !TraversalWarping || Command.Identity.AbilityHandle != Move.AbilityHandle ||
		Command.Identity.ActivationPredictionKey != Move.ActivationPredictionKey || Command.Identity.bServerInitiatedKey != Move.bServerInitiatedKey ||
		Command.Identity.MontageSequence != Move.MontageSequence || Command.Context.Montage != Move.MontageState.Montage) { return false; }
	const FMoverDefaultSyncState* Default = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!Default) { return false; }
	bTraversalRootMotionScope = TraversalWarping->BeginSimulationWarp(this, TraversalSimulationState,
		FTransform(Default->GetOrientation_WorldSpace(), Default->GetLocation_WorldSpace()));
	return bTraversalRootMotionScope;
}
void URpgCharacterMoverComponent::EndTraversalRootMotion(float MontagePosition)
{
	if (!bTraversalRootMotionScope) { return; }
	TraversalWarping->EndSimulationWarp();
	TraversalSimulationState.MontagePosition = MontagePosition;
	bTraversalRootMotionScope = false;
}

void URpgCharacterMoverComponent::OnPostSimulate(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartingData, FMoverTickEndData& EndingData)
{
	if (TraversalSimulationState.bEndApplied && TraversalSimulationState.Command.IsTerminal())
	{
		// Historical active frames retain their complete values. The applied terminal tombstone needs only
		// identity and target name, avoiding persistent modifier/asset payload and strong collider ownership.
		TraversalSimulationState.Command.CompactAppliedEnd();
		TraversalSimulationState.WarpModifiers.Reset();
		TraversalSimulationState.MontagePosition = 0.f;
		TraversalSimulationState.bStartApplied = false;
	}
	EndingData.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FRpgMoverTraversalSyncState>() = TraversalSimulationState;
	if (bRestoreTraversalPresentationInputs)
	{
		// Only the completed-frame read model receives the original input. GASP's existing WithMovementInput
		// montage notify needs this on authority, owner and proxies, while the actual simulation uses neutral input.
		FMoverTickStartData PresentationData = StartingData;
		PresentationData.InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>() = TraversalPresentationInputs;
		Super::OnPostSimulate(TimeStep, PresentationData, EndingData);
	}
	else { Super::OnPostSimulate(TimeStep, StartingData, EndingData); }
}

void URpgCharacterMoverComponent::ApplyTraversalCollisionLease(UPrimitiveComponent* Collider)
{
	UPrimitiveComponent* Capsule = Cast<UPrimitiveComponent>(GetUpdatedComponent());
	if (!Capsule || LeasedCollisionComponent.Get() == Collider) { return; }
	if (bAddedCollisionIgnore && LeasedCollisionComponent)
	{
		Capsule->IgnoreComponentWhenMoving(LeasedCollisionComponent.Get(), false);
	}
	LeasedCollisionComponent = Collider;
	bAddedCollisionIgnore = Collider && !Capsule->GetMoveIgnoreComponents().Contains(Collider);
	if (bAddedCollisionIgnore) { Capsule->IgnoreComponentWhenMoving(Collider, true); }
}

void URpgCharacterMoverComponent::HandleTraversalPostFinalize(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	const FRpgMoverTraversalSyncState* State = SyncState.SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
	const bool bActive = State && State->Command.IsActive() && SyncState.MovementMode != URpgDeadMovementMode::ModeName;
	if (State && State->bEndApplied && State->Command.IsTerminal() &&
		TraversalCommand.IsTerminal() && State->Command.Identity == TraversalCommand.Identity)
	{
		TraversalCommand.CompactAppliedEnd();
	}
	ApplyTraversalCollisionLease(bActive ? State->Command.Context.Collider.Get() : nullptr);
	if (TraversalWarping && State && State->Command.Phase != ERpgMoverTraversalPhase::None)
	{
		if (bActive) { TraversalWarping->AddOrUpdateWarpTargetFromTransform(State->Command.Context.WarpTargetName, State->Command.Context.FrontLedgeTarget); }
		else { TraversalWarping->RemoveWarpTarget(State->Command.Context.WarpTargetName); }
	}
}

bool URpgCharacterMoverComponent::SampleAbilityRootMotion(double SimTimeMs, FRpgMoverAbilityRootMotion& OutMove) const
{
	if (!IsAbilityRootMotionCurrent(AbilityRootMotion) || !FMath::IsFinite(SimTimeMs) || SimTimeMs < AbilityRootMotionStartTimeMs)
	{
		return false;
	}
	OutMove = AbilityRootMotion;
	const double ElapsedSeconds = (SimTimeMs - AbilityRootMotionStartTimeMs) * 0.001;
	const double Position = AbilityRootMotion.MontageState.StartingMontagePosition +
		ElapsedSeconds * AbilityRootMotion.MontageState.PlayRate * AbilityRootMotion.MontageState.Montage->RateScale;
	if (!FMath::IsFinite(Position) || Position >= AbilityRootMotion.MontageState.Montage->GetPlayLength())
	{
		return false;
	}
	OutMove.MontageState.StartingMontagePosition = Position;
	OutMove.MontageState.CurrentPosition = Position;
	return true;
}

void URpgCharacterMoverComponent::HandleAbilityRootMotionPreSimulation(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	if (!BackendLiaisonComp || !Simulation || IsBackendAsync() || TimeStep.StepMs <= 0.0f || bSuppressMovementForDeathThisTick)
	{
		return;
	}
	FRpgMoverAbilityRootMotion Move;
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (!SampleAbilityRootMotion(TimeStep.BaseSimTimeMs, Move))
		{
			return;
		}
	}
	else if (GetOwnerRole() == ROLE_AutonomousProxy)
	{
		const FRpgMoverAbilityRootMotionInputs* Inputs = InputCmd.InputCollection.FindDataByType<FRpgMoverAbilityRootMotionInputs>();
		const FRpgMoverTraversalCommand& Active = TraversalSimulationState.Command;
		if (Active.IsActive() && (!Inputs || !RpgAbilityRootMotion::MatchesTraversal(Inputs->RootMotion, Active)))
		{
			// GAS RPCs can start B on authority while NP still consumes an older input frame from A. When
			// corrected B reaches that historical frame, continue its approved sync trajectory; do not replay A
			// or wait for today's montage/actor to catch up. This context never comes from client wire input.
			Move.AbilityHandle = Active.Identity.AbilityHandle;
			Move.ActivationPredictionKey = Active.Identity.ActivationPredictionKey;
			Move.bServerInitiatedKey = Active.Identity.bServerInitiatedKey;
			Move.MontageSequence = Active.Identity.MontageSequence;
			Move.MontageState.Montage = Active.Context.Montage;
			Move.MontageState.PlayRate = Active.Context.PlayRate;
			Move.MontageState.BlendOutTimeSeconds = Active.Context.Montage ? Active.Context.Montage->GetDefaultBlendOutTime() : 0.f;
			Move.MontageState.bEnableAutoBlendOut = Active.Context.Montage && Active.Context.Montage->bEnableAutoBlendOut;
		}
		else if (!Inputs || !Inputs->RootMotion.MontageState.Montage)
		{
			return;
		}
		else { Move = Inputs->RootMotion; }
	}
	else
	{
		return;
	}
	const FRpgMoverTraversalCommand& Traversal = TraversalSimulationState.Command;
	const bool bTraversalIdentity = Traversal.Identity.AbilityHandle == Move.AbilityHandle &&
		Traversal.Identity.ActivationPredictionKey == Move.ActivationPredictionKey &&
		Traversal.Identity.bServerInitiatedKey == Move.bServerInitiatedKey && Traversal.Identity.MontageSequence == Move.MontageSequence;
	if (bTraversalIdentity)
	{
		if (!Traversal.IsActive()) { return; }
		Move.MontageState.StartingMontagePosition = TraversalSimulationState.MontagePosition;
		Move.MontageState.CurrentPosition = TraversalSimulationState.MontagePosition;
	}

	// The input history is the start/stop history. Reconstruct one contribution for this exact tick,
	// including after rollback before the original GAS start. No montage is replayed by reconstruction.
	Move.DurationMs = TimeStep.StepMs;
	const FMoverTime FrameTime(TimeStep.ServerFrame, TimeStep.BaseSimTimeMs);
	const FMoverSchedulingInfo Scheduling(FrameTime, FrameTime, BackendLiaisonComp->IsFixedDt());
	Simulation->QueueLayeredMove(FScheduledLayeredMove(Scheduling, MakeShared<FRpgMoverAbilityRootMotion>(Move)));
}

bool URpgCharacterMoverComponent::CanPlayAbilityRootMotion(const UAbilitySystemComponent* AbilitySystem,
	const UAnimMontage* Montage, float PlayRate, FName StartSection, float StartTimeSeconds) const
{
	const FGameplayAbilityActorInfo* ActorInfo = AbilitySystem ? AbilitySystem->AbilityActorInfo.Get() : nullptr;
	const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(GetPrimaryVisualComponent());
	// The asynchronous physics backend has a different montage lifecycle and is intentionally a later integration.
	if (!BackendLiaisonComp || !Simulation || IsBackendAsync() || !GetUpdatedComponent() ||
		!ActorInfo || ActorInfo->AvatarActor.Get() != GetOwner() ||
		!Mesh || ActorInfo->SkeletalMeshComponent.Get() != Mesh || !Mesh->GetAnimInstance() ||
		!Montage || !Montage->HasRootMotion() || !FMath::IsFinite(PlayRate) || PlayRate <= UE_SMALL_NUMBER ||
		!FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= UE_SMALL_NUMBER ||
		!FMath::IsFinite(PlayRate * Montage->RateScale) ||
		!FMath::IsFinite(StartTimeSeconds) || StartTimeSeconds < 0.0f || StartTimeSeconds >= Montage->GetPlayLength())
	{
		return false;
	}

	// The engine game-thread move extracts linearly in simulation time; section jumps and time stretching
	// would move the capsule along a different interval than GAS' montage and its authored hit windows.
	return !Montage->TimeStretchCurve.IsValid() && Montage->CompositeSections.Num() == 1 &&
		Montage->CompositeSections[0].NextSectionName.IsNone() &&
		FMath::IsNearlyZero(Montage->CompositeSections[0].GetTime(), KINDA_SMALL_NUMBER) &&
		(StartSection.IsNone() || StartSection == Montage->CompositeSections[0].SectionName);
}

bool URpgCharacterMoverComponent::StartAbilityRootMotion(UAbilitySystemComponent* AbilitySystem,
	UGameplayAbility* Ability, const FPredictionKey& ActivationKey, UAnimMontage* Montage, float PlayRate)
{
	USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(GetPrimaryVisualComponent());
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	FAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetActiveInstanceForMontage(Montage) : nullptr;
	if (!AbilitySystem || AbilitySystem->GetAvatarActor() != GetOwner() || !Ability || !MontageInstance ||
		AbilitySystem->GetAnimatingAbility() != Ability ||
		AbilitySystem->GetCurrentMontage() != Montage || !Ability->GetCurrentAbilitySpecHandle().IsValid())
	{
		return false;
	}
	if (AbilityAnimInstance.Get() == AnimInstance && AbilityMontageInstanceId == MontageInstance->GetInstanceID())
	{
		return true;
	}

	const FGameplayAbilitySpecHandle AbilityHandle = Ability->GetCurrentAbilitySpecHandle();
	if (LastAbilityHandle != AbilityHandle || LastActivationKey != ActivationKey)
	{
		LastMontageSequence = 0;
	}
	LastAbilityHandle = AbilityHandle;
	LastActivationKey = ActivationKey;
	++LastMontageSequence;
	if (LastMontageSequence == 0)
	{
		++LastMontageSequence;
	}

	AbilityAnimInstance = AnimInstance;
	AbilityMontage = Montage;
	AbilityMontageInstanceId = MontageInstance->GetInstanceID();
	// Like PlayMoverMontage, keep extraction disabled through this instance's blend-out. Each new play
	// allocates a fresh montage instance; there is no shared asset-level counter to accumulate or restore.
	MontageInstance->PushDisableRootMotion();

	AbilityRootMotion = FRpgMoverAbilityRootMotion{};
	AbilityRootMotion.AbilityHandle = AbilityHandle;
	AbilityRootMotion.ActivationPredictionKey = ActivationKey.Current;
	AbilityRootMotion.bServerInitiatedKey = ActivationKey.bIsServerInitiated;
	AbilityRootMotion.MontageSequence = LastMontageSequence;
	AbilityRootMotion.MontageState.Montage = Montage;
	AbilityRootMotion.MontageState.PlayRate = PlayRate;
	AbilityRootMotion.MontageState.StartingMontagePosition = MontageInstance->GetPosition();
	AbilityRootMotion.MontageState.CurrentPosition = MontageInstance->GetPosition();
	AbilityRootMotion.MontageState.BlendOutTimeSeconds = Montage->GetDefaultBlendOutTime();
	AbilityRootMotion.MontageState.bEnableAutoBlendOut = Montage->bEnableAutoBlendOut;
	AbilityRootMotionStartTimeMs = BackendLiaisonComp->GetCurrentSimTimeMs();

	UE_LOG(LogRpgAbilitySystem, Verbose,
		TEXT("Mover GAS root motion started: Pawn=%s Montage=%s Instance=%d Ability=%s Key=%s Sequence=%u Rate=%.3f Start=%.3f"),
		*GetNameSafe(GetOwner()), *GetNameSafe(Montage), AbilityMontageInstanceId, *AbilityHandle.ToString(),
		*ActivationKey.ToString(), LastMontageSequence, PlayRate, MontageInstance->GetPosition());
	return true;
}

void URpgCharacterMoverComponent::StopAbilityRootMotion(const UAnimInstance* AnimInstance, int32 MontageInstanceId)
{
	if (AbilityAnimInstance.Get() == AnimInstance && AbilityMontageInstanceId == MontageInstanceId)
	{
		ClearAbilityRootMotion();
	}
}

void URpgCharacterMoverComponent::ClearAbilityRootMotion()
{
	AbilityAnimInstance.Reset();
	AbilityMontage.Reset();
	AbilityMontageInstanceId = INDEX_NONE;
	AbilityRootMotion = FRpgMoverAbilityRootMotion{};
	// Retain the per-activation play counter so stopping and replaying the same asset cannot reuse its identity.
}

bool URpgCharacterMoverComponent::IsAbilityRootMotionCurrent(const FRpgMoverAbilityRootMotion& Move) const
{
	if (Move.AbilityHandle != LastAbilityHandle || Move.ActivationPredictionKey != LastActivationKey.Current ||
		Move.bServerInitiatedKey != LastActivationKey.bIsServerInitiated || Move.MontageSequence != LastMontageSequence ||
		Move.MontageState.Montage != AbilityMontage.Get() || AbilityMontageInstanceId == INDEX_NONE)
	{
		return false;
	}

	const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(GetPrimaryVisualComponent());
	UAnimInstance* AnimInstance = AbilityAnimInstance.Get();
	const FAnimMontageInstance* Instance = AnimInstance ? AnimInstance->GetMontageInstanceForID(AbilityMontageInstanceId) : nullptr;
	return Mesh && Mesh->GetAnimInstance() == AnimInstance && Instance && Instance->IsActive() && Instance->IsPlaying() &&
		Instance == AnimInstance->GetActiveInstanceForMontage(AbilityMontage.Get());
}

void URpgCharacterMoverComponent::UpdateSyncedMontageState(const FMoverTimeStep& TimeStep,
	const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	if (SyncState.LayeredMoves.FindActiveMove(FRpgMoverAbilityRootMotion::StaticStruct()))
	{
		// ASC's replicated montage owns playback, position correction and stop on simulated proxies.
		// Running the engine's second playback path would restart the same montage and duplicate notifies.
		// FMoverAnimMontageState::Reset is not exported by Mover; value initialization clears the same ownership.
		SyncedMontageState = FMoverAnimMontageState{};
		return;
	}
	Super::UpdateSyncedMontageState(TimeStep, SyncState, AuxState);
}

void URpgCharacterMoverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnPreSimulationTick.RemoveDynamic(this, &ThisClass::HandleAbilityRootMotionPreSimulation);
	OnPostFinalize.RemoveDynamic(this, &ThisClass::HandleTraversalPostFinalize);
	ApplyTraversalCollisionLease(nullptr);
	TraversalCommand = FRpgMoverTraversalCommand{};
	TraversalSimulationState = FRpgMoverTraversalSyncState{};
	ClearAbilityRootMotion();
	Super::EndPlay(EndPlayReason);
}
