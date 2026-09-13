#include "RpgCharacterMoverComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
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
}

bool FRpgMoverAbilityRootMotion::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
	const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	const URpgCharacterMoverComponent* RpgMover = Cast<URpgCharacterMoverComponent>(MoverComp);
	if (!RpgMover)
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
		return Super::GenerateMove(StartState, TimeStep, MoverComp, SimBlackboard, OutProposedMove);
	}

	const FRpgMoverAbilityRootMotionInputs* Inputs = StartState.InputCmd.InputCollection.FindDataByType<FRpgMoverAbilityRootMotionInputs>();
	if (RpgMover->GetOwnerRole() != ROLE_AutonomousProxy || !Inputs ||
		!RpgAbilityRootMotion::MatchesPlayback(*this, Inputs->RootMotion))
	{
		DurationMs = 0.0f;
		return false;
	}

	// The NP backend does not set bIsResimulating. Its stored input nevertheless identifies the exact
	// historical interval. Only bypass the engine's present-day AnimInstance check, after that validation;
	// preserve the real timestep, simulation state, extraction and movement collision handling.
	FMoverTimeStep ExtractionTimeStep = TimeStep;
	ExtractionTimeStep.bIsResimulating = true;
	return Super::GenerateMove(StartState, ExtractionTimeStep, MoverComp, SimBlackboard, OutProposedMove);
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
		RetainMontageForHistory();
	}
	bOutSuccess = true;
	return true;
}

void FRpgMoverAbilityRootMotionInputs::AddReferencedObjects(FReferenceCollector& Collector)
{
	RootMotion.AddReferencedObjects(Collector);
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
	Super::BeginPlay();
	OnPreSimulationTick.AddUniqueDynamic(this, &ThisClass::HandleAbilityRootMotionPreSimulation);
}

void URpgCharacterMoverComponent::ProduceInput(int32 DeltaTimeMS, FMoverInputCmdContext* Cmd)
{
	Super::ProduceInput(DeltaTimeMS, Cmd);
	FRpgMoverAbilityRootMotionInputs& Inputs = Cmd->InputCollection.FindOrAddMutableDataByType<FRpgMoverAbilityRootMotionInputs>();
	if (!BackendLiaisonComp || !SampleAbilityRootMotion(BackendLiaisonComp->GetCurrentSimTimeMs(), Inputs.RootMotion))
	{
		Inputs.RootMotion = FRpgMoverAbilityRootMotion{};
	}
	Inputs.RetainMontageForHistory();
	CachedLastProducedInputCmd = *Cmd;
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
	if (!BackendLiaisonComp || !Simulation || IsBackendAsync() || TimeStep.StepMs <= 0.0f)
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
		if (!Inputs || !Inputs->RootMotion.MontageState.Montage)
		{
			return;
		}
		Move = Inputs->RootMotion;
	}
	else
	{
		return;
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
	ClearAbilityRootMotion();
	Super::EndPlay(EndPlayReason);
}
