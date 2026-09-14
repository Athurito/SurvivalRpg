// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Network/RpgMoverPredictionTestHelpers.h"
#include "Network/RpgMoverPredictionTestTypes.h"
#include "Components/PIENetworkComponent.h"
#include "Abilities/GameplayAbilityRepAnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Backends/MoverNetworkPredictionLiaison.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Misc/Guid.h"
#include "MotionWarpingComponent.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoverSimulationTypes.h"
#include "NetworkPredictionWorldManager.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"
#include "SurvivalRpg/Core/Character/RpgDeadMovementMode.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgMoverPawn.h"
#include "SurvivalRpg/Core/Character/RpgMoverTraversalTypes.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Traversal/RpgGameplayAbility_Mantle.h"
#include "SurvivalRpg/Traversal/RpgTraversalQueryComponent.h"
#include "UnrealEdGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspMoverMantleTests
{
	constexpr TCHAR MapPath[] = TEXT("/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMover");
	constexpr TCHAR AbilityPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/Traversal/GA_RpgGasp_MoverMantle.GA_RpgGasp_MoverMantle_C");
	const FName UnrelatedTarget(TEXT("MoverMantleFixtureUnrelated"));
	const FVector UnrelatedLocation(123.0, 456.0, 789.0);
	bool ActiveWorld(const UWorld* World)
	{
		if (!World || !GEngine) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(World) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		return false;
	}
	/** Event-bounded transport evidence; cumulative counters and wall time permit short-interval throughput calculation. */
	void ReportConnections(const TCHAR* Phase, UWorld* World)
	{
		UNetDriver* Driver = ActiveWorld(World) ? World->GetNetDriver() : nullptr;
		if (!Driver) return;
		if (const UNetworkPredictionWorldManager* Prediction = World->GetSubsystem<UNetworkPredictionWorldManager>())
		{
			const FFixedTickState& Clock = Prediction->GetFixedTickState();
			UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle prediction phase=%s world=%s local=%d offset=%d step=%d latestAP=%d latestSP=%d toFrame=%d pct=%.3f interpolationMs=%d"),
				Phase, *World->GetPathName(), Clock.PendingFrame, Clock.Offset, Clock.FixedStepMS,
				Clock.Interpolation.LatestRecvFrameAP, Clock.Interpolation.LatestRecvFrameSP, Clock.Interpolation.ToFrame,
				Clock.Interpolation.PCT, Clock.Interpolation.InterpolatedTimeMS);
		}
		auto ReportConnection = [Phase, World](const UNetConnection* Connection)
		{
			if (!Connection) return;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle transport phase=%s world=%s connection=%s wall=%.6f netSpeed=%d queuedBits=%d ready=%d outBytes=%d outPackets=%d outBps=%d inBytes=%d inBps=%d pingMs=%.2f"),
				Phase, *World->GetPathName(), *Connection->GetName(), FPlatformTime::Seconds(), Connection->CurrentNetSpeed,
				Connection->QueuedBits, Connection->IsNetReady(), Connection->OutTotalBytes, Connection->OutTotalPackets,
				Connection->OutBytesPerSecond, Connection->InTotalBytes, Connection->InBytesPerSecond, Connection->RawPingInSeconds * 1000.0);
		};
		ReportConnection(Driver->ServerConnection);
		for (const UNetConnection* Connection : Driver->ClientConnections) ReportConnection(Connection);
	}
	APawn* LocalPawn(UWorld* World)
	{
		const APlayerController* PC = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}
	ARpgPlayerState* Player(UWorld* World, int32 Id)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		if (State) for (APlayerState* Candidate : State->PlayerArray)
			if (Candidate && Candidate->GetPlayerId() == Id) return Cast<ARpgPlayerState>(Candidate);
		return nullptr;
	}
	APawn* Pawn(UWorld* World, int32 Id) { const ARpgPlayerState* State = Player(World, Id); return State ? State->GetPawn() : nullptr; }
	USkeletalMeshComponent* Mesh(const APawn* Character) { return URpgPawnExtensionComponent::FindGameplayMesh(Character); }
	UCapsuleComponent* Capsule(const APawn* Character) { return Character ? Cast<UCapsuleComponent>(Character->GetRootComponent()) : nullptr; }
	URpgCharacterMoverComponent* Mover(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgCharacterMoverComponent>() : nullptr; }
	UMotionWarpingComponent* Warping(const APawn* Character) { return Character ? Character->FindComponentByClass<UMotionWarpingComponent>() : nullptr; }
	URpgTraversalQueryComponent* Query(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgTraversalQueryComponent>() : nullptr; }
	URpgAbilitySystemComponent* ASC(const APawn* Character)
	{
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		return Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr;
	}
	FGameplayAbilitySpec* Spec(APawn* Character)
	{
		if (ASC(Character)) for (FGameplayAbilitySpec& Candidate : ASC(Character)->GetActivatableAbilities())
			if (Candidate.Ability && Candidate.Ability->GetClass()->GetPathName() == AbilityPath) return &Candidate;
		return nullptr;
	}
	bool IsMantle(const APawn* Character, const UAnimMontage* Montage)
	{
		return Montage && Query(Character) && Query(Character)->AllowedMantleAnimations.ContainsByPredicate(
			[Montage](const FRpgTraversalAnimationEntry& Row) { return Row.Montage == Montage && !Row.bAirborne; });
	}
	bool Ready(UWorld* World, APawn* Character)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = State ? State->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		return Experience && Experience->IsExperienceLoaded() && Character && Character->IsA<ARpgMoverPawn>() && Character->GetPlayerState()
			&& ASC(Character) && ASC(Character)->GetAvatarActor() == Character && Mesh(Character) && Mesh(Character)->GetAnimInstance()
			&& ASC(Character)->AbilityActorInfo.IsValid() && ASC(Character)->AbilityActorInfo->SkeletalMeshComponent.Get() == Mesh(Character)
			&& Mover(Character) && Mover(Character)->GetPrimaryVisualComponent() == Mesh(Character) && Capsule(Character) && Warping(Character)
			&& Query(Character) && !Query(Character)->AllowedMantleAnimations.IsEmpty();
	}
	bool Clean(APawn* Character)
	{
		if (!Mover(Character) || !Warping(Character)) return false;
		const FGameplayAbilitySpec* Ability = Spec(Character);
		return !Mover(Character)->HasTraversalLease() && !Mover(Character)->GetTraversalCollider()
			&& !Mover(Character)->FindActiveLayeredMoveByType(FRpgMoverAbilityRootMotion::StaticStruct())
			&& (!Ability || !Ability->IsActive()) && !Warping(Character)->FindWarpTarget(TEXT("FrontLedge"))
			&& !Warping(Character)->FindWarpTarget(TEXT("BackLedge")) && !Warping(Character)->FindWarpTarget(TEXT("BackFloor"));
	}
	/** The saved map remains unchanged and every test world is isolated before InitGame persistence work. */
	class FSaveIsolation final
	{
	public:
		~FSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_MoverMantleAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddLambda([this](AGameModeBase* Initialized)
			{
				ARpgGameModeBase* Mode = Cast<ARpgGameModeBase>(Initialized);
				if (!Mode || !Mode->GetWorld() || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
				Mode->bEnableDiskPersistence = false;
				Mode->WorldSaveSlotName = Prefix + FString::Printf(TEXT("_%u"), Mode->GetUniqueID());
				Mode->WorldSaveBackupSlotName = Mode->WorldSaveSlotName + TEXT("_Backup");
				Mode->WorldSaveRecoverySlotName = Mode->WorldSaveSlotName + TEXT("_Recovery");
				Mode->OfflineProfileKey = Prefix;
			});
		}
		bool Isolated(UWorld* World) const
		{
			const ARpgGameModeBase* Mode = ActiveWorld(World) ? World->GetAuthGameMode<ARpgGameModeBase>() : nullptr;
			return Mode && !Mode->bEnableDiskPersistence && Mode->WorldSaveSlotName.StartsWith(Prefix) && Mode->OfflineProfileKey == Prefix;
		}
	private:
		FString Prefix;
		FDelegateHandle Handle;
	};
	/** Observes Fixed NP restore/resimulation between network dispatch and forward simulation. */
	class FPredictionCorrection final
	{
	public:
		~FPredictionCorrection() { Stop(); }
		bool Inject(APawn* Character, bool bAfterHandoff, UAnimMontage* ObservedMontage)
		{
			if (bInjected || !Character || Character->GetLocalRole() != ROLE_AutonomousProxy) return false;
			const UNetworkPredictionWorldManager* Prediction = Character->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
			if (!Prediction || Prediction->GetSettings().PreferredTickingPolicy != ENetworkPredictionTickingPolicy::Fixed
				|| !Prediction->GetSettings().bEnableFixedTickSmoothing || Prediction->GetFixedTickState().PendingFrame <= 0) return false;
			UMoverNetworkPredictionLiaisonComponent* Backend = Character->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>();
			FMoverSyncState Sync;
			if (!Backend || !Backend->ReadPendingSyncState(Sync)) return false;
			const FRpgMoverTraversalSyncState* Traversal = Sync.SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
			FMoverDefaultSyncState* Default = Sync.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
			if (!Traversal || !Default || !Traversal->Command.Identity.AbilityHandle.IsValid()) return false;
			if (bAfterHandoff ? !Traversal->Command.IsTerminal() || !Traversal->bEndApplied : !Traversal->Command.IsActive()) return false;
			if (!bAfterHandoff && !ActiveWarp(*Traversal)) return false;
			OriginalIdentity = Traversal->Command.Identity; OriginalCollider = Traversal->Command.Context.Collider;
			OriginalPhase = Traversal->Command.Phase; OriginalTargetName = Traversal->Command.Context.WarpTargetName;
			OriginalTarget = Traversal->Command.Context.FrontLedgeTarget;
			OriginalMontage = bAfterHandoff ? ObservedMontage : Traversal->Command.Context.Montage.Get();
			const FAnimMontageInstance* Instance = OriginalMontage.IsValid()
				? Mesh(Character)->GetAnimInstance()->GetActiveInstanceForMontage(OriginalMontage.Get()) : nullptr;
			OriginalInstance = Instance ? Instance->GetInstanceID() : INDEX_NONE;
			if (!bAfterHandoff && !Instance) return false;
			Owner = Character; Liaison = Backend; bTerminalExpected = bAfterHandoff;
			CrossDirection = Character->GetActorRightVector().GetSafeNormal2D();
			const FString PriorBaseName = GetPathNameSafe(Default->GetMovementBase());
			Default->SetTransforms_WorldSpace(Default->GetLocation_WorldSpace() + CrossDirection * 50.0,
				Default->GetOrientation_WorldSpace(), Default->GetVelocity_WorldSpace(), Default->GetAngularVelocityDegrees_WorldSpace(),
				nullptr);
			if (!Backend->WritePendingSyncState(Sync)) return false;
			// The kinematic backend captures its UpdatedComponent after moving: both parts of the one-off
			// prediction error must agree or the next normal tick would erase it without reconciliation.
			Mover(Character)->GetUpdatedComponent()->SetWorldLocation(Default->GetLocation_WorldSpace(), false, nullptr, ETeleportType::TeleportPhysics);
			// Match FTeleportEffect's local cache invalidation. Otherwise a movable landing block's cached
			// floor contact moves the pawn straight back during a normal based-movement tick, before NP can correct it.
			if (UMoverBlackboard* Blackboard = Mover(Character)->GetSimBlackboard_Mutable())
			{
				Blackboard->Invalidate(CommonBlackboard::LastFloorResult);
				Blackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
			}
			RollbackObserver.Reset(NewObject<URpgMoverRollbackTestObserver>());
			Mover(Character)->OnPostSimulationRollback.AddDynamic(RollbackObserver.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
			bInjected = true;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverMantleCorrection injection frame=%d time=%.0f afterHandoff=%d location=%s component=%s priorBase=%s"),
				Backend->GetCurrentSimFrame(), Backend->GetCurrentSimTimeMs(), bAfterHandoff,
				*Default->GetLocation_WorldSpace().ToCompactString(), *Mover(Character)->GetUpdatedComponent()->GetComponentLocation().ToCompactString(), *PriorBaseName);
			BeforeHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FPredictionCorrection::BeforeDispatch);
			AfterHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FPredictionCorrection::AfterDispatch);
			Report(TEXT("injected"));
			return true;
		}
		bool Injected() const { return bInjected; }
		bool Observed() const { return bObserved; }
		bool PreservedContract() const { return bObserved && bSameFrame && bIdentity && bContext && bLifecycle && bMontage && bWarpHistory; }
		void Report(const TCHAR* Phase) const
		{
			UE_LOG(LogTemp, Display, TEXT("RpgMoverMantleCorrection phase=%s injected=%d observed=%d afterHandoff=%d sameFrame=%d frame=%d->%d time=%.0f->%.0f delta=%s identity=%d context=%d lifecycle=%d montage=%d warpHistory=%d"),
				Phase, bInjected, bObserved, bTerminalExpected, bSameFrame, BeforeFrame, AfterFrame, BeforeTime, AfterTime,
				*Delta.ToCompactString(), bIdentity, bContext, bLifecycle, bMontage, bWarpHistory);
			UE_LOG(LogTemp, Display, TEXT("RpgMoverMantleCorrection clock local=%d->%d offset=%d->%d step=%d->%d rollbacks=%d->%d restoredFrame=%d expungedFrame=%d"),
				BeforeClock.LocalPendingFrame, AfterClock.LocalPendingFrame, BeforeClock.ServerOffset, AfterClock.ServerOffset,
				BeforeClock.StepMs, AfterClock.StepMs, BeforeRollbackCount, RollbackObserver.IsValid() ? RollbackObserver->Count : 0,
				RollbackObserver.IsValid() ? RollbackObserver->LastRestored.ServerFrame : INDEX_NONE,
				RollbackObserver.IsValid() ? RollbackObserver->LastExpunged.ServerFrame : INDEX_NONE);
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(BeforeHandle); FWorldDelegates::OnWorldPreActorTick.Remove(AfterHandle);
			BeforeHandle.Reset(); AfterHandle.Reset();
			if (Owner.IsValid() && Mover(Owner.Get()) && RollbackObserver.IsValid())
				Mover(Owner.Get())->OnPostSimulationRollback.RemoveDynamic(RollbackObserver.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
			RollbackObserver.Reset();
			// A copied native NP frame can own history references; release it before EndPlayMap runs GC.
			Before = FMoverSyncState();
			bBeforeValid = false;
		}
	private:
		static const FRpgMoverWarpModifierState* ActiveWarp(const FRpgMoverTraversalSyncState& State)
		{
			return State.WarpModifiers.FindByPredicate([](const FRpgMoverWarpModifierState& Modifier)
				{ return Modifier.Value.State == ERootMotionModifierState::Active; });
		}
		void BeforeDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !Owner.IsValid() || Owner->GetWorld() != World || !Liaison.IsValid()) return;
			bBeforeValid = Liaison->ReadPendingSyncState(Before);
			BeforeRollbackCount = RollbackObserver.IsValid() ? RollbackObserver->Count : 0;
			BeforeClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison.Get());
			BeforeFrame = BeforeClock.ServerFrame; BeforeTime = BeforeClock.SimulationTimeMs;
		}
		void AfterDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !bBeforeValid || !Owner.IsValid() || Owner->GetWorld() != World || !Liaison.IsValid()) return;
			FMoverSyncState After;
			if (!Liaison->ReadPendingSyncState(After)) return;
			const FMoverDefaultSyncState* BeforeMove = Before.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const FMoverDefaultSyncState* AfterMove = After.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			if (!BeforeMove || !AfterMove) return;
			const FVector Movement = AfterMove->GetLocation_WorldSpace() - BeforeMove->GetLocation_WorldSpace();
			if (DiagnosticSamples++ < 12)
			{
				UE_LOG(LogTemp, Display, TEXT("RpgMoverMantleCorrection dispatch sample=%d afterHandoff=%d frame=%d->%d time=%.0f->%.0f before=%s after=%s component=%s"),
					DiagnosticSamples, bTerminalExpected, BeforeFrame, Liaison->GetCurrentSimFrame(), BeforeTime, Liaison->GetCurrentSimTimeMs(),
					*BeforeMove->GetLocation_WorldSpace().ToCompactString(), *AfterMove->GetLocation_WorldSpace().ToCompactString(),
					*Mover(Owner.Get())->GetUpdatedComponent()->GetComponentLocation().ToCompactString());
			}
			// Active Motion Warping can remove most of the 50 cm injection during legitimate forward ticks.
			// Require the actual engine rollback callback inside this dispatch bracket and a remaining
			// measurable correction; forward warping or based movement can satisfy neither callback proof.
			if (!RollbackObserver.IsValid() || RollbackObserver->Count <= BeforeRollbackCount
				|| FVector::DotProduct(Movement, CrossDirection) > -1.0) return;
			bObserved = true; Delta = Movement;
			AfterFrame = Liaison->GetCurrentSimFrame(); AfterTime = Liaison->GetCurrentSimTimeMs();
			AfterClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison.Get());
			bSameFrame = BeforeClock.IsSameLocalHead(AfterClock);
			const FRpgMoverTraversalSyncState* BeforeTraversal = Before.SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
			const FRpgMoverTraversalSyncState* AfterTraversal = After.SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
			if (BeforeTraversal && AfterTraversal)
			{
				bIdentity = BeforeTraversal->Command.Identity == OriginalIdentity && AfterTraversal->Command.Identity == OriginalIdentity;
				const FAnimMontageInstance* Instance = OriginalMontage.IsValid()
					? Mesh(Owner.Get())->GetAnimInstance()->GetActiveInstanceForMontage(OriginalMontage.Get()) : nullptr;
				if (bTerminalExpected)
				{
					// Applied terminal states retain identity but release assets and collision/warp resources.
					bContext = !BeforeTraversal->Command.Context.Collider.IsValid() && !BeforeTraversal->Command.Context.Montage
						&& !AfterTraversal->Command.Context.Collider.IsValid() && !AfterTraversal->Command.Context.Montage
						&& BeforeTraversal->Command.Context.WarpTargetName == OriginalTargetName
						&& AfterTraversal->Command.Context.WarpTargetName == OriginalTargetName;
					bLifecycle = BeforeTraversal->Command.IsTerminal() && BeforeTraversal->Command.Phase == OriginalPhase
						&& AfterTraversal->Command.Phase == OriginalPhase && BeforeTraversal->bEndApplied
						&& AfterTraversal->bEndApplied && Clean(Owner.Get());
					bMontage = OriginalMontage.IsValid() && (!Instance || !Instance->IsPlaying());
					bWarpHistory = BeforeTraversal->WarpModifiers.IsEmpty() && AfterTraversal->WarpModifiers.IsEmpty();
				}
				else
				{
					bContext = AfterTraversal->Command.Context.Collider == OriginalCollider.Get()
						&& AfterTraversal->Command.Context.Montage == OriginalMontage.Get()
						&& AfterTraversal->Command.Context.FrontLedgeTarget.GetLocation().Equals(OriginalTarget.GetLocation(), 1.0)
						&& AfterTraversal->Command.Context.FrontLedgeTarget.GetRotation().Equals(OriginalTarget.GetRotation(), 0.001);
					bLifecycle = BeforeTraversal->Command.IsActive() && AfterTraversal->Command.IsActive() && !AfterTraversal->bEndApplied
						&& Mover(Owner.Get())->HasTraversalLease() && Mover(Owner.Get())->GetTraversalCollider() == OriginalCollider.Get();
					bMontage = Instance && Instance->GetInstanceID() == OriginalInstance && Instance->IsPlaying();
					const FRpgMoverWarpModifierState* OldWarp = ActiveWarp(*BeforeTraversal);
					const FRpgMoverWarpModifierState* NewWarp = ActiveWarp(*AfterTraversal);
					// Compare immutable window/bone-cache identity, not a frozen trajectory or a particular allocation.
					bWarpHistory = OldWarp && NewWarp && OldWarp->WindowIndex == NewWarp->WindowIndex
						&& FMath::IsNearlyEqual(OldWarp->Value.StartTime, NewWarp->Value.StartTime)
						&& FMath::IsNearlyEqual(OldWarp->Value.EndTime, NewWarp->Value.EndTime)
						&& OldWarp->Value.CachedOffsetFromWarpPoint.IsSet() == NewWarp->Value.CachedOffsetFromWarpPoint.IsSet()
						&& (!OldWarp->Value.CachedOffsetFromWarpPoint.IsSet()
							|| OldWarp->Value.CachedOffsetFromWarpPoint.GetValue().Equals(NewWarp->Value.CachedOffsetFromWarpPoint.GetValue(), 0.01));
				}
			}
			Report(TEXT("reconciled"));
		}
		TWeakObjectPtr<APawn> Owner;
		TWeakObjectPtr<UMoverNetworkPredictionLiaisonComponent> Liaison;
		TWeakObjectPtr<UPrimitiveComponent> OriginalCollider;
		TWeakObjectPtr<UAnimMontage> OriginalMontage;
		TStrongObjectPtr<URpgMoverRollbackTestObserver> RollbackObserver;
		FRpgMoverTraversalIdentity OriginalIdentity;
		ERpgMoverTraversalPhase OriginalPhase = ERpgMoverTraversalPhase::None;
		FName OriginalTargetName;
		FTransform OriginalTarget = FTransform::Identity;
		FMoverSyncState Before;
		RpgMoverPredictionTests::FFixedPredictionHeadSnapshot BeforeClock, AfterClock;
		FDelegateHandle BeforeHandle, AfterHandle;
		FVector CrossDirection = FVector::ZeroVector, Delta = FVector::ZeroVector;
		int32 OriginalInstance = INDEX_NONE, BeforeFrame = INDEX_NONE, AfterFrame = INDEX_NONE, DiagnosticSamples = 0;
		int32 BeforeRollbackCount = 0;
		double BeforeTime = 0.0, AfterTime = 0.0;
		bool bInjected = false, bObserved = false, bTerminalExpected = false, bBeforeValid = false;
		bool bSameFrame = false, bIdentity = false, bContext = false, bLifecycle = false, bMontage = false, bWarpHistory = false;
	};
	enum class EScenario : uint8 { Success, Jump, HeldRetry, BlockedExit, Cancel, Death, ColliderLoss, LateJoin, CorrectDuringWarp, CorrectAfterWarp };
	enum class EGait : uint8 { Stand, Walk, Run };
	struct FObservation
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<APawn> Character;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		FDelegateHandle Committed, Ended;
		TWeakObjectPtr<UAnimMontage> Montage;
		FVector HandoffLocation = FVector::ZeroVector, HandoffVelocity = FVector::ZeroVector;
		FVector DeathLocation = FVector::ZeroVector;
		float FirstTime = -1.0f, LastTime = -1.0f, LastWarpEnd = 0.0f, WarpYaw = 0.0f;
		float BestAlignmentError = 180.0f, MaximumPostWarpError = 0.0f;
		float LastLeaseSpeed = 0.0f;
		float DeadSeconds = 0.0f, MaximumDeathDrift = 0.0f;
		int32 Commits = 0, Ends = 0, SuccessfulEnds = 0, CancelledEnds = 0, InstanceId = INDEX_NONE, PostWarpSamples = 0;
		bool bCancelled = false, bLease = false, bWarpTarget = false, bMontageRestarted = false;
		bool bLandedOnObstacle = false, bContinuedMoving = false, bRestoredFacing = false;
		bool bOrdinaryJump = false, bGroundedAfterJump = false, bCommittedAfterGroundedRetry = false;
		bool bCommittedWhileAirborne = false;
		bool bConfirmedPlayCancelled = false;
		bool bCleanAfterInterruption = false, bObservedDeath = false, bHandoffGrounded = false;
		bool bTerminalDeath = false, bInvalidDeath = false, bJoinedTerminalState = false;
		bool bWasLease = false, bCapturedHandoff = false, bStoppedAtHandoff = false, bLostUnrelatedTarget = false;
	};
}

/** Saved-map input coverage for the opt-in Mover traversal path; no fixture teleports or synthetic candidates. */
NETWORK_TEST_CLASS(GaspMoverMantlePIE, "SurvivalRpg.GASP.Mover.Mantle")
{
	using FObservation = RpgGaspMoverMantleTests::FObservation;
	using EScenario = RpgGaspMoverMantleTests::EScenario;
	using EGait = RpgGaspMoverMantleTests::EGait;
	RpgGaspMoverMantleTests::FSaveIsolation Isolation;
	RpgGaspMoverMantleTests::FPredictionCorrection Correction;
	TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
	FPrimaryAssetId PreviousExperience;
	TWeakObjectPtr<UWorld> ServerWorld, ClientWorld, ObserverWorld;
	TWeakObjectPtr<APlayerController> InputController;
	TWeakObjectPtr<UPrimitiveComponent> Obstacle, DisabledCollider;
	TWeakObjectPtr<AActor> Blocker;
	ECollisionEnabled::Type PreviousCollision = ECollisionEnabled::NoCollision;
	FBox Bounds{ForceInit};
	FObservation OwnerRecord, AuthorityRecord, ProxyRecord;
	FDelegateHandle TickHandle;
	FVector SpawnLocation = FVector::ZeroVector;
	int32 PlayerId = INDEX_NONE;
	EGait Gait = EGait::Run;
	EScenario Scenario = EScenario::Success;
	float ApproachYaw = 0.0f, PressSpeed = 0.0f, PressYaw = 0.0f, MaximumViewError = 0.0f;
	double StartedAt = 0.0, PressedAt = -1.0, ContactAt = -1.0;
	bool bConfigured = false, bOwnsSession = false, bDriving = false, bHost = false;
	bool bFinalHeading = false, bSpaceReleased = false, bMoveReleased = false, bInterrupted = false;
	bool bLateJoinRequested = false, bCheckpointLogged = false;

	BEFORE_EACH()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Mover Mantle automation refuses to interrupt an existing PIE session."));
				return;
			}
		Isolation.Start();
		PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = FPrimaryAssetId();
		bConfigured = true;
		TestCommandBuilder.OnTearDown(TEXT("Release Mover Mantle test input and only its own PIE session"), [this]() { Cleanup(); });
	}
	AFTER_EACH() { Cleanup(); }
	TEST_METHOD(RemoteStandingMantleKeepsMomentumAfterHandoff) { Queue(EGait::Stand); }
	TEST_METHOD(RemoteWalkingMantleUsesGroundedWalkingEntry) { Queue(EGait::Walk); }
	TEST_METHOD(RemoteRunningMantleKeepsMomentumAfterHandoff) { Queue(EGait::Run); }
	TEST_METHOD(PositiveAngleAlignsOwnerBodyWithoutTurningView) { Queue(EGait::Run, EScenario::Success, false, 35.0f); }
	TEST_METHOD(NegativeAngleAlignsListenHostWithoutTurningView) { Queue(EGait::Run, EScenario::Success, true, -35.0f); }
	TEST_METHOD(ListenHostStandingMantleUsesTheSameAbility) { Queue(EGait::Stand, EScenario::Success, true); }
	TEST_METHOD(SpaceAwayFromAnObstacleRemainsAnOrdinaryJump) { Queue(EGait::Run, EScenario::Jump); }
	TEST_METHOD(HeldSpaceRetriesOnlyAfterTheOrdinaryJumpLands) { Queue(EGait::Run, EScenario::HeldRetry); }
	TEST_METHOD(ServerBlockedLandingRejectsThePredictedMantle) { Queue(EGait::Run, EScenario::BlockedExit); }
	TEST_METHOD(CancellationReleasesMoverWarpAndColliderOwnership) { Queue(EGait::Run, EScenario::Cancel, false, 35.0f); }
	TEST_METHOD(DeathDuringMantleReleasesTraversalAndStopsMover) { Queue(EGait::Run, EScenario::Death); }
	TEST_METHOD(LostAuthorityColliderCancelsTheActiveMantle) { Queue(EGait::Run, EScenario::ColliderLoss); }
	TEST_METHOD(LateJoinReconstructsTheCurrentMantleState) { Queue(EGait::Stand, EScenario::LateJoin); }
	TEST_METHOD(FixedCorrectionPreservesActiveWarpAndCollider) { Queue(EGait::Stand, EScenario::CorrectDuringWarp); }
	TEST_METHOD(FixedCorrectionAfterHandoffCannotRestoreOldTraversal) { Queue(EGait::Stand, EScenario::CorrectAfterWarp); }

	UWorld* InputWorld() const { return bHost ? ServerWorld.Get() : ClientWorld.Get(); }
	APawn* Owner() const { return RpgGaspMoverMantleTests::LocalPawn(InputWorld()); }
	APawn* Authority() const { return RpgGaspMoverMantleTests::Pawn(ServerWorld.Get(), PlayerId); }
	APawn* Observer() const { return RpgGaspMoverMantleTests::Pawn(ObserverWorld.Get(), PlayerId); }
	bool InterruptedScenario() const { return Scenario == EScenario::Cancel || Scenario == EScenario::Death || Scenario == EScenario::ColliderLoss || Scenario == EScenario::BlockedExit; }
	bool HoldMovement() const { return Scenario != EScenario::LateJoin && Scenario != EScenario::CorrectDuringWarp && Scenario != EScenario::CorrectAfterWarp; }
	bool Finished(const FObservation& Record) const { return Record.bLandedOnObstacle && Record.bRestoredFacing && (!HoldMovement() || Record.bContinuedMoving); }
	void Queue(EGait InGait, EScenario InScenario = EScenario::Success, bool bInHost = false, float InYaw = 0.0f)
	{
		using namespace RpgGaspMoverMantleTests;
		if (!bConfigured) return;
		Gait = InGait; Scenario = InScenario; bHost = bInHost; ApproachYaw = InYaw;
		TestCommandBuilder.Do(TEXT("Start the approved saved Mover map with its authored Experience"), [this]()
		{
			Settings.Reset(NewObject<ULevelEditorPlaySettings>());
			Settings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
			Settings->SetPlayNumberOfClients(bHost || Scenario == EScenario::LateJoin ? 2 : 3);
			Settings->SetRunUnderOneProcess(true);
			Settings->bLaunchSeparateServer = false;
			Settings->GameGetsMouseControl = false;
			FRequestPlaySessionParams Params;
			Params.EditorPlaySettings = Settings.Get(); Params.GlobalMapOverride = MapPath; Params.bAllowOnlineSubsystem = false;
			bOwnsSession = true;
			GUnrealEd->RequestPlaySession(Params); GUnrealEd->StartQueuedPlaySessionRequest();
		})
		.Until(TEXT("Mover owner and authority receive real traversal composition and input"), [this]()
		{
			FindWorlds();
			APawn* Character = Owner();
			const URpgPawnGameplayComponent* Input = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character);
			if (!Ready(InputWorld(), Character) || !Input || !Input->IsReadyToBindInputs() || !Spec(Character) || !Mover(Character)->IsOnGround()) return false;
			PlayerId = Character->GetPlayerState()->GetPlayerId();
			if (!Ready(ServerWorld.Get(), Authority()) || !Isolation.Isolated(ServerWorld.Get())) return false;
			return Scenario == EScenario::LateJoin || (Ready(ObserverWorld.Get(), Observer()) && Observer()->GetLocalRole() == ROLE_SimulatedProxy);
		}, FTimespan::FromSeconds(60.0))
		.Then(TEXT("Find the prepared one-meter lane and start ordinary forward input"), [this]()
		{
			FindLane();
			ASSERT_THAT(IsTrue(Obstacle.IsValid() && Bounds.IsValid));
			if (!Obstacle.IsValid()) return;
			ASSERT_THAT(IsTrue(FMath::Abs(Bounds.GetSize().Z - 100.0) < 2.0 && Bounds.GetSize().X >= 350.0 && Bounds.GetSize().Y >= 350.0));
			InputController = Cast<APlayerController>(Owner()->GetController());
			ASSERT_THAT(IsTrue(InputController.IsValid()));
			if (!InputController.IsValid()) return;
			InputController->SetIgnoreLookInput(true);
			InputController->SetControlRotation(FRotator::ZeroRotator);
			SpawnLocation = Owner()->GetActorLocation(); StartedAt = InputWorld()->GetTimeSeconds();
			Subscribe(Owner(), OwnerRecord); Subscribe(Authority(), AuthorityRecord);
			if (Scenario != EScenario::LateJoin) SubscribeProxy();
			bDriving = true;
			TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &GaspMoverMantlePIE::Tick);
			if (Gait == EGait::Walk) Key(EKeys::LeftControl, true);
			Key(EKeys::W, true);
		})
		.Until(TEXT("Real input completes the chosen Mantle, fallback or lifecycle scenario"), [this]() { return Complete(); }, FTimespan::FromSeconds(45.0))
		.Then(TEXT("Validate observed montage, movement, warp and lifecycle contracts"), [this]() { Verify(); });
	}
	void FindWorlds()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE || !IsValid(World) || !World->GetMapName().Contains(TEXT("Lvl_RpgGaspMover"))) continue;
			if (World->GetNetMode() == NM_ListenServer) ServerWorld = World;
			if (World->GetNetMode() == NM_Client)
			{
				if (!ClientWorld.IsValid()) ClientWorld = World;
				else if (World != ClientWorld.Get() && !ObserverWorld.IsValid()) ObserverWorld = World;
			}
		}
		if (bHost) ObserverWorld = ClientWorld;
	}
	void FindLane()
	{
		using namespace RpgGaspMoverMantleTests;
		if (!Owner() || !ActiveWorld(InputWorld())) return;
		const FVector Position = Owner()->GetActorLocation();
		const double Feet = Position.Z - Capsule(Owner())->GetScaledCapsuleHalfHeight();
		// An idle Mover can retain its PlayerStart clearance until the first movement input.
		// Measure the authored floor itself so lane height does not depend on that spawn offset.
		FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(RpgMoverMantleFixtureFloor), false, Owner());
		TArray<AActor*> AttachedActors;
		Owner()->GetAttachedActors(AttachedActors, true, true);
		FloorParams.AddIgnoredActors(AttachedActors);
		FHitResult FloorHit;
		const bool bFloorHit = InputWorld()->LineTraceSingleByChannel(FloorHit, Position,
			Position - FVector(0.0, 0.0, 500.0), ECC_Visibility, FloorParams);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle lane floor pawn=%s position=%s feet=%.2f ground=%d hit=%d floor=%s impact=%s normal=%s"),
			*GetPathNameSafe(Owner()), *Position.ToCompactString(), Feet, Mover(Owner())->IsOnGround(), bFloorHit,
			*GetPathNameSafe(FloorHit.GetComponent()), *FloorHit.ImpactPoint.ToCompactString(), *FloorHit.ImpactNormal.ToCompactString());
		if (!bFloorHit || !FloorHit.IsValidBlockingHit() || FloorHit.ImpactNormal.Z < 0.7) return;
		const double FloorHeight = FloorHit.ImpactPoint.Z;
		double Nearest = TNumericLimits<double>::Max();
		for (TActorIterator<AActor> It(InputWorld()); It; ++It)
		{
			const UStaticMeshComponent* Physical = It->FindComponentByClass<UStaticMeshComponent>();
			if (!Physical || Physical->IsSimulatingPhysics() || !Physical->IsQueryCollisionEnabled()
				|| Physical->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block) continue;
			const FBox Box = Physical->Bounds.GetBox();
			const double Distance = Box.Min.X - Position.X;
			if (Distance <= 0 || Distance >= Nearest || FMath::Abs(Box.Max.Z - FloorHeight - 100.0) > 8.0
				|| Box.GetSize().X < 350.0 || Position.Y < Box.Min.Y + 50.0 || Position.Y > Box.Max.Y - 50.0) continue;
			Obstacle = const_cast<UStaticMeshComponent*>(Physical); Bounds = Box; Nearest = Distance;
		}
		UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle lane obstacle=%s bounds=%s..%s floorHeight=%.2f"),
			*GetPathNameSafe(Obstacle.Get()), *Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString(), FloorHeight);
	}
	UPrimitiveComponent* Collider(UWorld* World) const
	{
		if (!RpgGaspMoverMantleTests::ActiveWorld(World) || !Obstacle.IsValid()) return nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
			if (It->GetFName() == Obstacle->GetOwner()->GetFName())
				return It->FindComponentByClass<UStaticMeshComponent>();
		return nullptr;
	}
	void Key(FKey InKey, bool bPressed)
	{
		if (InputController.IsValid()) InputController->InputKey(FInputKeyEventArgs::CreateSimulated(InKey,
			bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
	}
	void Subscribe(APawn* Character, FObservation& Record)
	{
		using namespace RpgGaspMoverMantleTests;
		if (!Character || !ASC(Character)) return;
		Record.World = Character->GetWorld(); Record.Character = Character; Record.AbilitySystem = ASC(Character);
		Warping(Character)->AddOrUpdateWarpTargetFromLocationAndRotation(UnrelatedTarget, UnrelatedLocation, FRotator::ZeroRotator);
		Record.Committed = ASC(Character)->AbilityCommittedCallbacks.AddLambda([Snapshot = &Record](UGameplayAbility* Ability)
		{
			if (Ability && Ability->GetClass()->GetPathName() == AbilityPath)
			{
				++Snapshot->Commits;
				const bool bGroundedAtCommit = Mover(Snapshot->Character.Get()) && Mover(Snapshot->Character.Get())->IsOnGround();
				Snapshot->bCommittedWhileAirborne |= !bGroundedAtCommit;
				Snapshot->bCommittedAfterGroundedRetry |= Snapshot->bGroundedAfterJump || (Snapshot->bOrdinaryJump && bGroundedAtCommit);
				UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle commit pawn=%s attempt=%d grounded=%d"),
					*GetPathNameSafe(Snapshot->Character.Get()), Snapshot->Commits, bGroundedAtCommit);
				ReportConnections(TEXT("commit"), Snapshot->World.Get());
			}
		});
		Record.Ended = ASC(Character)->OnAbilityEnded.AddLambda([this, Snapshot = &Record](const FAbilityEndedData& Data)
		{
			APawn* Character = Snapshot->Character.Get();
			if (!Character || !Data.AbilityThatEnded || Data.AbilityThatEnded->GetClass()->GetPathName() != AbilityPath) return;
			++Snapshot->Ends; Snapshot->bCancelled |= Data.bWasCancelled;
			Snapshot->bConfirmedPlayCancelled |= Scenario == EScenario::HeldRetry && AuthorityRecord.Commits > 0 && Data.bWasCancelled;
			if (Data.bWasCancelled) ++Snapshot->CancelledEnds;
			else ++Snapshot->SuccessfulEnds;
		});
	}
	void SubscribeProxy()
	{
		ProxyRecord.World = ObserverWorld; ProxyRecord.Character = Observer();
		ProxyRecord.AbilitySystem = RpgGaspMoverMantleTests::ASC(Observer());
		RpgGaspMoverMantleTests::Warping(Observer())->AddOrUpdateWarpTargetFromLocationAndRotation(
			RpgGaspMoverMantleTests::UnrelatedTarget, RpgGaspMoverMantleTests::UnrelatedLocation, FRotator::ZeroRotator);
	}
	void ReportMontage(const TCHAR* Phase, const FObservation& Record, APawn* Character, UAnimInstance* Animation,
		UAnimMontage* Montage, const FAnimMontageInstance* Instance, float Position) const
	{
		const URpgAbilitySystemComponent* Component = RpgGaspMoverMantleTests::ASC(Character);
		// The engine getter is protected; inspect its existing replicated property without changing it.
		const FStructProperty* Property = FindFProperty<FStructProperty>(UAbilitySystemComponent::StaticClass(), TEXT("RepAnimMontageInfo"));
		const FGameplayAbilityRepAnimMontage* Replicated = Component && Property
			? Property->ContainerPtrToValuePtr<FGameplayAbilityRepAnimMontage>(Component) : nullptr;
		UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle montage %s pawn=%s animation=%s role=%d original=%s instance=%d current=%s instance=%d previousTime=%.4f currentTime=%.4f replicatedPlayId=%d replicatedStopped=%d localStopped=%d"),
			Phase, *GetPathNameSafe(Character), *GetPathNameSafe(Animation), static_cast<int32>(Character->GetLocalRole()),
			*GetPathNameSafe(Record.Montage.Get()), Record.InstanceId, *GetPathNameSafe(Montage), Instance ? Instance->GetInstanceID() : INDEX_NONE,
			Record.LastTime, Position, Replicated ? static_cast<int32>(Replicated->PlayInstanceId) : INDEX_NONE,
			Replicated ? static_cast<int32>(Replicated->IsStopped) : INDEX_NONE, Instance ? static_cast<int32>(Instance->IsStopped()) : INDEX_NONE);
		RpgGaspMoverMantleTests::ReportConnections(Phase, Record.World.Get());
	}
	void Observe(FObservation& Record, float DeltaSeconds)
	{
		using namespace RpgGaspMoverMantleTests;
		APawn* Character = Record.Character.Get();
		if (!Character || !ActiveWorld(Record.World.Get()) || PressedAt < 0.0 || !Mover(Character)) return;
		UAnimInstance* Animation = Mesh(Character)->GetAnimInstance();
		UAnimMontage* Montage = Animation ? Animation->GetCurrentActiveMontage() : nullptr;
		// A held retry may predict just before authority reaches the same grounded state. Keep those
		// rejected attempts in the counters, but observe the confirmed play's montage and handoff separately.
		const bool bObserveConfirmedPlay = Scenario != EScenario::HeldRetry || AuthorityRecord.Commits > 0;
		const bool bPlaying = bObserveConfirmedPlay && Animation && IsMantle(Character, Montage) && Animation->Montage_IsPlaying(Montage);
		const bool bLease = bObserveConfirmedPlay && Mover(Character)->HasTraversalLease();
		// NP owns collision/warp lifecycle independently of GAS montage replication and its presentation clock.
		// Record the full lease contract even when a remote visible montage has not arrived yet.
		UPrimitiveComponent* ExpectedCollider = Collider(Record.World.Get());
		Record.bLease |= bLease && ExpectedCollider && Mover(Character)->GetTraversalCollider() == ExpectedCollider
			&& Capsule(Character)->GetMoveIgnoreComponents().Contains(ExpectedCollider)
			&& Mover(Character)->FindActiveLayeredMoveByType(FRpgMoverAbilityRootMotion::StaticStruct());
		const FMotionWarpingTarget* CurrentWarpTarget = Warping(Character)->FindWarpTarget(TEXT("FrontLedge"));
		if (CurrentWarpTarget) { Record.bWarpTarget = true; Record.WarpYaw = static_cast<float>(CurrentWarpTarget->Rotator().Yaw); }
		const FMotionWarpingTarget* Unrelated = Warping(Character)->FindWarpTarget(UnrelatedTarget);
		Record.bLostUnrelatedTarget |= !Unrelated || !Unrelated->GetLocation().Equals(UnrelatedLocation, 0.01);
		if (Record.bWasLease && !bLease)
		{
			// GAS completion queues the transition; this is the actual finalized simulation handoff.
			Record.bCapturedHandoff = true;
			Record.HandoffLocation = Character->GetActorLocation(); Record.HandoffVelocity = Mover(Character)->GetVelocity();
			Record.bHandoffGrounded = Mover(Character)->IsOnGround();
			Record.bStoppedAtHandoff |= Record.LastLeaseSpeed > 100.0f && Record.HandoffVelocity.Size2D() < 1.0;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle simulation handoff role=%d grounded=%d previousSpeed=%.2f position=%s velocity=%s"),
				static_cast<int32>(Character->GetLocalRole()), Record.bHandoffGrounded, Record.LastLeaseSpeed,
				*Record.HandoffLocation.ToCompactString(), *Record.HandoffVelocity.ToCompactString());
			ReportConnections(TEXT("handoff"), Record.World.Get());
		}
		Record.bWasLease = bLease;
		if (bLease) Record.LastLeaseSpeed = static_cast<float>(Mover(Character)->GetVelocity().Size2D());
		Record.bOrdinaryJump |= Record.Commits == 0 && !Record.Montage.IsValid() && !bLease && Mover(Character)->IsFalling();
		Record.bGroundedAfterJump |= Record.bOrdinaryJump && !Record.Montage.IsValid() && Mover(Character)->IsOnGround();
		if (const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Character); Health && Health->IsDeadOrDying())
		{
			if (!Record.bObservedDeath) { Record.DeathLocation = Character->GetActorLocation(); Record.bObservedDeath = true; }
			Record.DeadSeconds += DeltaSeconds;
			const FMoverSyncState& Sync = Mover(Character)->GetSyncState();
			const FMoverDefaultSyncState* Move = Sync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const bool bTerminal = Sync.MovementMode == URpgDeadMovementMode::ModeName && Move && Move->GetVelocity_WorldSpace().IsNearlyZero(1.0)
				&& Move->GetAngularVelocityDegrees_WorldSpace().IsNearlyZero(1.0) && Move->GetIntent_WorldSpace().IsNearlyZero()
				&& !Move->GetMovementBase() && !Sync.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>();
			const bool bProxy = Character->GetLocalRole() == ROLE_SimulatedProxy;
			if (!Record.bTerminalDeath && bTerminal)
			{
				Record.bTerminalDeath = true;
				if (bProxy) Record.DeathLocation = Character->GetActorLocation();
			}
			Record.bInvalidDeath |= bProxy && !Record.bTerminalDeath && Record.DeadSeconds > 0.75f;
			if (bProxy ? Record.bTerminalDeath : Record.DeadSeconds > 0.05f)
			{
				Record.bInvalidDeath |= !bTerminal;
				Record.MaximumDeathDrift = FMath::Max(Record.MaximumDeathDrift, static_cast<float>(FVector::Dist(Record.DeathLocation, Character->GetActorLocation())));
			}
		}
		if (bPlaying)
		{
			const float Position = Animation->Montage_GetPosition(Montage);
			const FAnimMontageInstance* Instance = Animation->GetActiveInstanceForMontage(Montage);
			if (!Record.Montage.IsValid())
			{
				Record.Montage = Montage; Record.FirstTime = Position; Record.InstanceId = Instance ? Instance->GetInstanceID() : INDEX_NONE;
				ReportMontage(TEXT("first"), Record, Character, Animation, Montage, Instance, Position);
				TArray<FMotionWarpingWindowData> Windows;
				UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, TEXT("FrontLedge"), Windows);
				for (const FMotionWarpingWindowData& Window : Windows) Record.LastWarpEnd = FMath::Max(Record.LastWarpEnd, Window.EndTime);
			}
			const bool bDifferentPlay = Montage != Record.Montage.Get() || (Instance && Instance->GetInstanceID() != Record.InstanceId);
			if (bDifferentPlay && !Record.bMontageRestarted)
			{
				ReportMontage(TEXT("changed"), Record, Character, Animation, Montage, Instance, Position);
			}
			Record.LastTime = Position;
			Record.bMontageRestarted |= bDifferentPlay;
			if (Character->GetLocalRole() != ROLE_SimulatedProxy && bLease && Record.bWarpTarget && Character->GetActorLocation().X >= Bounds.Min.X)
			{
				const float Error = FMath::Abs(FMath::FindDeltaAngleDegrees(static_cast<float>(Character->GetActorRotation().Yaw), Record.WarpYaw));
				Record.BestAlignmentError = FMath::Min(Record.BestAlignmentError, Error);
				if (Position > Record.LastWarpEnd + KINDA_SMALL_NUMBER)
				{
					++Record.PostWarpSamples; Record.MaximumPostWarpError = FMath::Max(Record.MaximumPostWarpError, Error);
				}
			}
		}
		if (Character->GetLocalRole() == ROLE_SimulatedProxy && bObserveConfirmedPlay && bLease && Record.Montage.IsValid())
		{
			// GAS montage replication and NP interpolation have separate clocks. The finalized NP state
			// supplies both the capsule pose and the traversal phase; its retained From time is a conservative
			// post-warp boundary, even if the independently replicated visible montage has already stopped.
			const FMoverSyncState& Sync = Mover(Character)->GetSyncState();
			const FRpgMoverTraversalSyncState* Traversal = Sync.SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
			const FMoverDefaultSyncState* Move = Sync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const FMotionWarpingTarget* Target = Warping(Character)->FindWarpTarget(TEXT("FrontLedge"));
			if (Traversal && Move && Target && Traversal->Command.IsActive() && !Traversal->bEndApplied
				&& Traversal->Command.Identity.AbilityHandle.IsValid() && Traversal->Command.Context.Montage == Record.Montage.Get()
				&& Traversal->Command.Context.Collider.Get() == Mover(Character)->GetTraversalCollider()
				&& Target->GetLocation().Equals(Traversal->Command.Context.FrontLedgeTarget.GetLocation(), 1.0)
				&& Character->GetActorLocation().X >= Bounds.Min.X)
			{
				const float ExpectedYaw = static_cast<float>(Traversal->Command.Context.FrontLedgeTarget.Rotator().Yaw);
				const float ActorYaw = static_cast<float>(Character->GetActorRotation().Yaw);
				const float Error = FMath::Abs(FMath::FindDeltaAngleDegrees(ActorYaw, ExpectedYaw));
				Record.BestAlignmentError = FMath::Min(Record.BestAlignmentError, Error);
				if (Traversal->MontagePosition > Record.LastWarpEnd + KINDA_SMALL_NUMBER)
				{
					if (Record.PostWarpSamples == 0)
					{
						UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle proxy postwarp pawn=%s npMontageTime=%.4f gasMontageTime=%.4f gasPlaying=%d warpEnd=%.4f actorYaw=%.3f syncYaw=%.3f targetYaw=%.3f actorLocation=%s syncLocation=%s"),
							*GetPathNameSafe(Character), Traversal->MontagePosition, Record.LastTime, bPlaying, Record.LastWarpEnd,
							ActorYaw, Move->GetOrientation_WorldSpace().Yaw, ExpectedYaw, *Character->GetActorLocation().ToCompactString(),
							*Move->GetLocation_WorldSpace().ToCompactString());
					}
					++Record.PostWarpSamples; Record.MaximumPostWarpError = FMath::Max(Record.MaximumPostWarpError, Error);
				}
			}
		}
		const bool bClean = Clean(Character);
		if (Scenario == EScenario::LateJoin && &Record == &ProxyRecord && !Record.Montage.IsValid() && bClean)
		{
			const FRpgMoverTraversalSyncState* Remote = Mover(Character)->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
			const FRpgMoverTraversalSyncState* Server = Mover(Authority()) ? Mover(Authority())->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>() : nullptr;
			Record.bJoinedTerminalState |= Remote && Server && Remote->Command.Phase == ERpgMoverTraversalPhase::Finished
				&& Remote->Command.Phase == Server->Command.Phase && Remote->bEndApplied && Server->bEndApplied
				&& Remote->Command.Identity == Server->Command.Identity
				&& Remote->Command.Context.WarpTargetName == Server->Command.Context.WarpTargetName
				&& !Remote->Command.Context.Montage && !Remote->Command.Context.Collider.IsValid() && Remote->WarpModifiers.IsEmpty();
		}
		const bool bObservedInterruption = Record.Ends > 0 || (Character->GetLocalRole() == ROLE_SimulatedProxy
			&& (Record.bLease || Record.Montage.IsValid() || (Scenario == EScenario::BlockedExit && AuthorityRecord.Ends > 0)));
		Record.bCleanAfterInterruption |= InterruptedScenario() && bObservedInterruption
			&& bClean && (!Collider(Record.World.Get()) || !Capsule(Character)->GetMoveIgnoreComponents().Contains(Collider(Record.World.Get())));
		const FVector Position = Character->GetActorLocation();
		const double Radius = Capsule(Character)->GetScaledCapsuleRadius();
		const double Feet = Position.Z - Capsule(Character)->GetScaledCapsuleHalfHeight();
		Record.bLandedOnObstacle |= (Record.Montage.IsValid() || Record.bJoinedTerminalState) && bClean && Mover(Character)->IsOnGround() && FMath::Abs(Feet - Bounds.Max.Z) < 8.0
			&& Position.X >= Bounds.Min.X + Radius && Position.X <= Bounds.Max.X - Radius
			&& Position.Y >= Bounds.Min.Y + Radius && Position.Y <= Bounds.Max.Y - Radius;
		Record.bRestoredFacing |= (Record.bLandedOnObstacle || (Scenario == EScenario::Cancel && Record.bCleanAfterInterruption))
			&& FMath::Abs(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, static_cast<double>(ApproachYaw))) < 5.0;
		Record.bContinuedMoving |= Record.bLandedOnObstacle && Record.bCapturedHandoff && Mover(Character)->GetVelocity().X > 1.0
			&& Position.X > Record.HandoffLocation.X + 1.0;
	}
	void Tick(UWorld* World, ELevelTick, float DeltaSeconds)
	{
		using namespace RpgGaspMoverMantleTests;
		if (!bDriving || !ActiveWorld(World)) return;
		if (Scenario == EScenario::LateJoin && bLateJoinRequested && !ProxyRecord.Character.IsValid())
		{
			FindWorlds(); if (Ready(ObserverWorld.Get(), Observer())) SubscribeProxy();
		}
		if (World == ServerWorld.Get())
		{
			Observe(AuthorityRecord, DeltaSeconds);
			const FGameplayAbilitySpec* ActiveAbility = Spec(Authority());
			if (!bInterrupted && (Scenario == EScenario::Cancel || Scenario == EScenario::Death || Scenario == EScenario::ColliderLoss)
				&& AuthorityRecord.bLease && ProxyRecord.bLease && AuthorityRecord.LastTime > AuthorityRecord.FirstTime + 0.25f
				&& ActiveAbility && ActiveAbility->IsActive() && Mover(Authority())->HasTraversalLease())
			{
				bInterrupted = true;
				if (Scenario == EScenario::Death) URpgHealthComponent::FindHealthComponent(Authority())->DamageSelfDestruct(false);
				else if (Scenario == EScenario::Cancel) { if (FGameplayAbilitySpec* Ability = Spec(Authority())) ASC(Authority())->CancelAbilityHandle(Ability->Handle); }
				else if (UPrimitiveComponent* Physical = Collider(World))
				{
					DisabledCollider = Physical; PreviousCollision = Physical->GetCollisionEnabled(); Physical->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
		if (World == ObserverWorld.Get()) Observe(ProxyRecord, DeltaSeconds);
		if (World != InputWorld() || !Owner()) return;
		Observe(OwnerRecord, DeltaSeconds);
		if (!bCheckpointLogged && World->GetTimeSeconds() - StartedAt > 12.0) { Report(TEXT("checkpoint")); bCheckpointLogged = true; }
		if (bFinalHeading) MaximumViewError = FMath::Max(MaximumViewError,
			static_cast<float>(FMath::Abs(FMath::FindDeltaAngleDegrees(InputController->GetControlRotation().Yaw, static_cast<double>(ApproachYaw)))));
		if (PressedAt >= 0.0)
		{
			if (!Correction.Injected())
			{
				if (Scenario == EScenario::CorrectDuringWarp && OwnerRecord.LastTime > OwnerRecord.FirstTime + 0.2f
					&& OwnerRecord.LastTime < OwnerRecord.LastWarpEnd - 0.15f) Correction.Inject(Owner(), false, OwnerRecord.Montage.Get());
				if (Scenario == EScenario::CorrectAfterWarp && OwnerRecord.Ends > 0 && Clean(Owner()) && Mover(Owner())->IsOnGround()) Correction.Inject(Owner(), true, OwnerRecord.Montage.Get());
			}
			if (!bSpaceReleased && World->GetTimeSeconds() > PressedAt && (Scenario != EScenario::HeldRetry || AuthorityRecord.Commits > 0))
				{ Key(EKeys::SpaceBar, false); bSpaceReleased = true; }
			if (!HoldMovement() && OwnerRecord.Commits > 0 && !bMoveReleased) { Key(EKeys::W, false); bMoveReleased = true; }
			if (Scenario == EScenario::LateJoin && OwnerRecord.Commits > 0 && !bLateJoinRequested)
				{ bLateJoinRequested = true; GUnrealEd->RequestLateJoin(); }
			if (!bMoveReleased && ((bInterrupted && Scenario != EScenario::Death && Scenario != EScenario::Cancel) || (Scenario == EScenario::BlockedExit && OwnerRecord.Ends > 0)
				|| (Finished(OwnerRecord) && Finished(AuthorityRecord) && Finished(ProxyRecord))))
				{ Key(EKeys::W, false); bMoveReleased = true; }
			return;
		}
		const FVector Position = Owner()->GetActorLocation();
		const double Distance = Bounds.Min.X - Position.X;
		const float Speed = static_cast<float>(Mover(Owner())->GetVelocity().Size2D());
		if (!bFinalHeading)
		{
			if (Distance <= (ApproachYaw != 0.0f ? 400.0 : 220.0)) { InputController->SetControlRotation(FRotator(0, ApproachYaw, 0)); bFinalHeading = true; }
			else
			{
				const double Offset = ApproachYaw == 0.0f ? 0.0 : -FMath::Sign(ApproachYaw) * 130.0;
				InputController->SetControlRotation(FVector(250.0, Bounds.GetCenter().Y + Offset - Position.Y, 0.0).Rotation());
			}
		}
		if (!Mover(Owner())->IsOnGround()) return;
		if (Gait == EGait::Stand)
		{
			if (Distance > Capsule(Owner())->GetScaledCapsuleRadius() + 5.0 || Speed >= 5.0f) return;
			if (ContactAt < 0.0) ContactAt = World->GetTimeSeconds();
			if (World->GetTimeSeconds() - ContactAt < 0.15) return;
		}
		else if (Distance > (Scenario == EScenario::Jump || Scenario == EScenario::HeldRetry ? 900.0 : 170.0)) return;
		// Let Mover's ordinary turning reach the requested diagonal before testing traversal alignment.
		if (ApproachYaw != 0.0f && FMath::Sign(ApproachYaw) * FRotator::NormalizeAxis(Owner()->GetActorRotation().Yaw) < 25.0) return;
		if (Scenario == EScenario::BlockedExit) SpawnBlocker();
		PressSpeed = Speed; PressYaw = static_cast<float>(FRotator::NormalizeAxis(Owner()->GetActorRotation().Yaw));
		PressedAt = World->GetTimeSeconds(); Report(TEXT("space_pressed")); Key(EKeys::SpaceBar, true);
	}
	bool Complete() const
	{
		using namespace RpgGaspMoverMantleTests;
		if (PressedAt < 0.0) return false;
		if (Scenario == EScenario::Jump)
			return OwnerRecord.bGroundedAfterJump && AuthorityRecord.bGroundedAfterJump && Clean(Owner()) && Clean(Authority());
		if (Scenario == EScenario::Death && (OwnerRecord.DeadSeconds < 0.5f || AuthorityRecord.DeadSeconds < 0.5f || ProxyRecord.DeadSeconds < 0.5f)) return false;
		if (Scenario == EScenario::Cancel && (!OwnerRecord.bRestoredFacing || !AuthorityRecord.bRestoredFacing || !ProxyRecord.bRestoredFacing)) return false;
		if (InterruptedScenario())
			return OwnerRecord.Ends > 0 && AuthorityRecord.Ends > 0 && OwnerRecord.bCleanAfterInterruption
				&& AuthorityRecord.bCleanAfterInterruption && ProxyRecord.bCleanAfterInterruption;
		if ((Scenario == EScenario::CorrectDuringWarp || Scenario == EScenario::CorrectAfterWarp) && !Correction.Observed()) return false;
		return Finished(OwnerRecord) && Finished(AuthorityRecord) && Finished(ProxyRecord);
	}
	void Verify()
	{
		using namespace RpgGaspMoverMantleTests;
		Report(TEXT("completed"));
		ASSERT_THAT(IsTrue(Isolation.Isolated(ServerWorld.Get())));
		ASSERT_THAT(IsTrue(MaximumViewError < 1.0f));
		for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
		{
			ASSERT_THAT(IsFalse(Record->bLostUnrelatedTarget));
		}
		if (Scenario == EScenario::Jump)
		{
			ASSERT_THAT(IsTrue(OwnerRecord.bOrdinaryJump && AuthorityRecord.bOrdinaryJump));
			ASSERT_THAT(IsTrue(OwnerRecord.Commits == 0 && AuthorityRecord.Commits == 0));
			return;
		}
		ASSERT_THAT(IsTrue(Gait == EGait::Stand ? PressSpeed < 5.0f : Gait == EGait::Walk ? PressSpeed > 100.0f && PressSpeed < 250.0f : PressSpeed > 250.0f));
		if (Scenario == EScenario::BlockedExit)
		{
			ASSERT_THAT(IsTrue(OwnerRecord.Commits == 1 && AuthorityRecord.Commits == 0));
			ASSERT_THAT(IsTrue(OwnerRecord.bCancelled && AuthorityRecord.bCancelled));
			ASSERT_THAT(IsFalse(OwnerRecord.bLandedOnObstacle || AuthorityRecord.bLandedOnObstacle));
			return;
		}
		if (Scenario == EScenario::HeldRetry)
		{
			ASSERT_THAT(IsTrue(OwnerRecord.Commits >= 1 && AuthorityRecord.Commits == 1));
			ASSERT_THAT(IsTrue(OwnerRecord.SuccessfulEnds == 1 && AuthorityRecord.SuccessfulEnds == 1));
			ASSERT_THAT(IsFalse(OwnerRecord.bConfirmedPlayCancelled || AuthorityRecord.bConfirmedPlayCancelled));
			ASSERT_THAT(IsFalse(OwnerRecord.bCommittedWhileAirborne || AuthorityRecord.bCommittedWhileAirborne));
		}
		else
		{
			ASSERT_THAT(IsTrue(OwnerRecord.Commits == 1 && AuthorityRecord.Commits == 1));
		}
		if (InterruptedScenario())
		{
			ASSERT_THAT(IsTrue(bInterrupted && OwnerRecord.bCancelled && AuthorityRecord.bCancelled));
			ASSERT_THAT(IsTrue(OwnerRecord.Ends == 1 && AuthorityRecord.Ends == 1));
			if (Scenario == EScenario::Cancel)
			{
				ASSERT_THAT(IsTrue(OwnerRecord.bRestoredFacing && AuthorityRecord.bRestoredFacing && ProxyRecord.bRestoredFacing));
			}
			if (Scenario == EScenario::Death)
			{
				for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
				{
					ASSERT_THAT(IsTrue(Record->bObservedDeath && Record->bTerminalDeath));
					ASSERT_THAT(IsFalse(Record->bInvalidDeath));
					ASSERT_THAT(IsTrue(Record->MaximumDeathDrift < 5.0f));
				}
			}
			return;
		}
		if (Scenario != EScenario::HeldRetry)
		{
			ASSERT_THAT(IsFalse(OwnerRecord.bCancelled || AuthorityRecord.bCancelled));
		}
		if (Scenario == EScenario::HeldRetry)
		{
			ASSERT_THAT(IsTrue(OwnerRecord.bOrdinaryJump && AuthorityRecord.bOrdinaryJump
				&& OwnerRecord.bCommittedAfterGroundedRetry && AuthorityRecord.bCommittedAfterGroundedRetry));
		}
		else
		{
			ASSERT_THAT(IsFalse(OwnerRecord.bOrdinaryJump));
		}
		for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
		{
			if (Scenario == EScenario::LateJoin && Record == &ProxyRecord && Record->bJoinedTerminalState)
			{
				ASSERT_THAT(IsTrue(Record->bLandedOnObstacle && Record->bRestoredFacing));
				ASSERT_THAT(IsTrue(Clean(Record->Character.Get())));
				continue;
			}
			ASSERT_THAT(IsTrue(Record->Montage.IsValid() && Record->bLease && Record->bWarpTarget));
			ASSERT_THAT(IsFalse(Record->bMontageRestarted));
			ASSERT_THAT(IsTrue(Record->LastTime > Record->FirstTime + 0.1f));
			ASSERT_THAT(IsTrue(Record->bLandedOnObstacle && Record->bRestoredFacing));
			if (HoldMovement())
			{
				ASSERT_THAT(IsTrue(Record->bContinuedMoving));
				ASSERT_THAT(IsFalse(Record->bStoppedAtHandoff));
			}
			if (ApproachYaw != 0.0f)
			{
				ASSERT_THAT(IsTrue(Record->BestAlignmentError < 8.0f));
				ASSERT_THAT(IsTrue(Record->PostWarpSamples > 0 && Record->MaximumPostWarpError < 8.0f));
			}
			const FRpgTraversalAnimationEntry* Row = Query(Record->Character.Get())->AllowedMantleAnimations.FindByPredicate(
				[Record](const FRpgTraversalAnimationEntry& Entry) { return Entry.Montage == Record->Montage.Get(); });
			ASSERT_THAT(IsTrue(Row && !Row->bAirborne));
			if (Gait == EGait::Walk)
			{
				ASSERT_THAT(IsTrue(Row && Row->MinSpeed >= 100.0f && Row->MaxSpeed <= 250.0f));
			}
		}
		if (ApproachYaw != 0.0f)
		{
			ASSERT_THAT(IsTrue(FMath::Abs(PressYaw) >= 25.0f));
		}
		if (Scenario == EScenario::CorrectDuringWarp || Scenario == EScenario::CorrectAfterWarp)
		{
			ASSERT_THAT(IsTrue(Correction.PreservedContract()));
		}
	}
	void SpawnBlocker()
	{
		AActor* Actor = ServerWorld->SpawnActor<AActor>();
		if (!Actor) return;
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box); Actor->AddInstanceComponent(Box);
		Box->InitBoxExtent(FVector(65, 190, 100));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetWorldLocation(FVector(Bounds.Min.X + 80.0, Bounds.GetCenter().Y, Bounds.Max.Z + 100.0));
		Box->SetMobility(EComponentMobility::Static); Box->RegisterComponent(); Blocker = Actor;
	}
	void Report(const TCHAR* Phase) const
	{
		for (UWorld* World : { ServerWorld.Get(), ClientWorld.Get(), ObserverWorld.Get() })
			RpgGaspMoverMantleTests::ReportConnections(Phase, World);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle phase=%s scenario=%d gait=%d host=%d position=%s bounds=%s..%s pressSpeed=%.2f pressYaw=%.2f controlError=%.2f"),
			Phase, static_cast<int32>(Scenario), static_cast<int32>(Gait), bHost, Owner() ? *Owner()->GetActorLocation().ToCompactString() : TEXT("none"),
			*Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString(), PressSpeed, PressYaw, MaximumViewError);
		for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
			UE_LOG(LogTemp, Display, TEXT("RpgMoverMantle peer=%s commits=%d ends=%d cancelled=%d successfulEnds=%d cancelledEnds=%d airborneCommit=%d montage=%s time=%.3f..%.3f warpEnd=%.3f lease=%d target=%d restart=%d landed=%d moving=%d facing=%d lateError=%.2f postSamples=%d postError=%.2f jump=%d groundedRetry=%d cleanInterrupted=%d death=%d"),
				*GetPathNameSafe(Record->World.Get()), Record->Commits, Record->Ends, Record->bCancelled,
				Record->SuccessfulEnds, Record->CancelledEnds, Record->bCommittedWhileAirborne, *GetPathNameSafe(Record->Montage.Get()),
				Record->FirstTime, Record->LastTime, Record->LastWarpEnd, Record->bLease, Record->bWarpTarget, Record->bMontageRestarted,
				Record->bLandedOnObstacle, Record->bContinuedMoving, Record->bRestoredFacing, Record->BestAlignmentError, Record->PostWarpSamples,
				Record->MaximumPostWarpError, Record->bOrdinaryJump, Record->bCommittedAfterGroundedRetry, Record->bCleanAfterInterruption, Record->bObservedDeath);
	}
	void Cleanup()
	{
		if (bDriving) Report(TEXT("teardown"));
		if (Scenario == EScenario::CorrectDuringWarp || Scenario == EScenario::CorrectAfterWarp) Correction.Report(TEXT("teardown"));
		Correction.Stop();
		FWorldDelegates::OnWorldTickEnd.Remove(TickHandle); TickHandle.Reset();
		for (FObservation* Record : { &OwnerRecord, &AuthorityRecord })
			if (Record->AbilitySystem.IsValid())
			{
				Record->AbilitySystem->AbilityCommittedCallbacks.Remove(Record->Committed);
				Record->AbilitySystem->OnAbilityEnded.Remove(Record->Ended);
			}
		Key(EKeys::W, false); Key(EKeys::SpaceBar, false); Key(EKeys::LeftControl, false);
		if (InputController.IsValid()) InputController->SetIgnoreLookInput(false);
		InputController.Reset(); bDriving = false;
		if (DisabledCollider.IsValid()) DisabledCollider->SetCollisionEnabled(PreviousCollision);
		if (Blocker.IsValid()) Blocker->Destroy();
		if (bOwnsSession && GUnrealEd) { GUnrealEd->EndPlayMap(); bOwnsSession = false; }
		if (bConfigured) { GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience; bConfigured = false; }
		Settings.Reset();
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
