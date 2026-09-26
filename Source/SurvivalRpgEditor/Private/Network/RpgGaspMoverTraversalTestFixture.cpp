// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Network/RpgGaspMoverTraversalTestFixture.h"
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
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "SurvivalRpg/Traversal/RpgGameplayAbility_Mantle.h"
#include "SurvivalRpg/Traversal/RpgTraversalQueryComponent.h"
#include "UnrealEdGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspMoverTraversalTests
{
	constexpr TCHAR MapPath[] = TEXT("/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMover");
	constexpr TCHAR AbilityPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/Traversal/GA_RpgGasp_MoverMantle.GA_RpgGasp_MoverMantle_C");
	const FName UnrelatedTarget(TEXT("MoverMantleFixtureUnrelated"));
	const FName VaultTag(TEXT("Rpg.TraversalTest.Vault"));
	const FName VaultApproachTag(TEXT("Rpg.TraversalTest.Vault.Approach"));
	const FName HurdleTag(TEXT("Rpg.TraversalTest.Hurdle"));
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
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal prediction phase=%s world=%s local=%d offset=%d step=%d latestAP=%d latestSP=%d toFrame=%d pct=%.3f interpolationMs=%d"),
				Phase, *World->GetPathName(), Clock.PendingFrame, Clock.Offset, Clock.FixedStepMS,
				Clock.Interpolation.LatestRecvFrameAP, Clock.Interpolation.LatestRecvFrameSP, Clock.Interpolation.ToFrame,
				Clock.Interpolation.PCT, Clock.Interpolation.InterpolatedTimeMS);
		}
		auto ReportConnection = [Phase, World](const UNetConnection* Connection)
		{
			if (!Connection) return;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal transport phase=%s world=%s connection=%s wall=%.6f netSpeed=%d queuedBits=%d ready=%d outBytes=%d outPackets=%d outBps=%d inBytes=%d inBps=%d pingMs=%.2f"),
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
	const FRpgWeaponAttackDefinition* PrimaryAttack(const APawn* Character)
	{
		const URpgEquipmentManagerComponent* Equipment = Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
		const URpgWeaponInstance* Weapon = Equipment ? Cast<URpgWeaponInstance>(Equipment->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand)) : nullptr;
		return Weapon ? Weapon->FindAttackDefinition(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Attack.Primary"))) : nullptr;
	}
	const TArray<FRpgTraversalAnimationEntry>& AnimationEntries(const APawn* Character, EAction Action)
	{
		if (Action == EAction::Hurdle) return Query(Character)->AllowedHurdleAnimations;
		return Action == EAction::Vault ? Query(Character)->AllowedVaultAnimations : Query(Character)->AllowedMantleAnimations;
	}
	bool IsTraversal(const APawn* Character, const UAnimMontage* Montage, EAction Action)
	{
		return Montage && Query(Character) && AnimationEntries(Character, Action).ContainsByPredicate(
			[Montage](const FRpgTraversalAnimationEntry& Row) { return Row.Montage == Montage && !Row.bAirborne; });
	}
	bool Ready(UWorld* World, APawn* Character, EAction Action = EAction::Mantle)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = State ? State->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		return Experience && Experience->IsExperienceLoaded() && Character && Character->IsA<ARpgMoverPawn>() && Character->GetPlayerState()
			&& ASC(Character) && ASC(Character)->GetAvatarActor() == Character && Mesh(Character) && Mesh(Character)->GetAnimInstance()
			&& ASC(Character)->AbilityActorInfo.IsValid() && ASC(Character)->AbilityActorInfo->SkeletalMeshComponent.Get() == Mesh(Character)
			&& Mover(Character) && Mover(Character)->GetPrimaryVisualComponent() == Mesh(Character) && Capsule(Character) && Warping(Character)
			&& Query(Character) && !AnimationEntries(Character, Action).IsEmpty();
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
	bool IsTerminalDeath(const FMoverSyncState& Sync)
	{
		const FMoverDefaultSyncState* Move = Sync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
		return Sync.MovementMode == URpgDeadMovementMode::ModeName && Move && Move->GetVelocity_WorldSpace().IsNearlyZero(1.0)
			&& Move->GetAngularVelocityDegrees_WorldSpace().IsNearlyZero(1.0) && Move->GetIntent_WorldSpace().IsNearlyZero()
			&& !Move->GetMovementBase() && !Sync.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>();
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
		bool Inject(APawn* Character, bool bAfterHandoff, UAnimMontage* ObservedMontage, FName RequiredActiveTarget = NAME_None)
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
			if (!bAfterHandoff && !RequiredActiveTarget.IsNone())
			{
				if (!ObservedMontage) return false;
				TArray<FMotionWarpingWindowData> RequiredWindows;
				UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(ObservedMontage, RequiredActiveTarget, RequiredWindows);
				const bool bRequiredWindowActive = Traversal->WarpModifiers.ContainsByPredicate([&RequiredWindows](const FRpgMoverWarpModifierState& Warp)
				{
					return Warp.Value.State == ERootMotionModifierState::Active && RequiredWindows.ContainsByPredicate([&Warp](const FMotionWarpingWindowData& Window)
					{ return FMath::IsNearlyEqual(Window.StartTime, Warp.Value.StartTime) && FMath::IsNearlyEqual(Window.EndTime, Warp.Value.EndTime); });
				});
				if (!bRequiredWindowActive) return false;
				if (RequiredActiveTarget == TEXT("BackFloor") || RequiredActiveTarget == TEXT("FrontLedge"))
				{
					const FRpgMoverWarpModifierState* SelectedWarp = Traversal->WarpModifiers.FindByPredicate([&RequiredWindows](const FRpgMoverWarpModifierState& Warp)
					{
						return Warp.Value.State == ERootMotionModifierState::Active && RequiredWindows.ContainsByPredicate([&Warp](const FMotionWarpingWindowData& Window)
						{ return FMath::IsNearlyEqual(Window.StartTime, Warp.Value.StartTime) && FMath::IsNearlyEqual(Window.EndTime, Warp.Value.EndTime); });
					});
					// Use simulation history, not the smoothed visible montage clock. Leave room on both
					// sides for a received active snapshot and replay within the same authored window.
					if (RequiredActiveTarget == TEXT("FrontLedge")
						&& (SelectedWarp->Value.CurrentPosition < SelectedWarp->Value.StartTime + 0.06f
							|| SelectedWarp->Value.CurrentPosition > SelectedWarp->Value.EndTime - 0.1f)) return false;
					RequiredWarpWindow = SelectedWarp->WindowIndex;
				}
			}
			OriginalIdentity = Traversal->Command.Identity; OriginalCollider = Traversal->Command.Context.Collider;
			OriginalPhase = Traversal->Command.Phase; OriginalTargetName = Traversal->Command.Context.WarpTargetName;
			OriginalTarget = Traversal->Command.Context.FrontLedgeTarget;
			OriginalRearName = Traversal->Command.Context.BackLedgeWarpTargetName;
			OriginalRearTarget = Traversal->Command.Context.BackLedgeTarget;
			OriginalFloorName = Traversal->Command.Context.BackFloorWarpTargetName;
			OriginalFloorTarget = Traversal->Command.Context.BackFloorTarget;
			OriginalSupport = Traversal->Command.Context.LandingSupport;
			OriginalSupportTransform = Traversal->Command.Context.LandingSupportTransform;
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
			InjectedLocalFrame = Prediction->GetFixedTickState().PendingFrame;
			bTrackInjectedFrame = !bAfterHandoff && RequiredActiveTarget == TEXT("FrontLedge");
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
			if (bTrackInjectedFrame) RollbackObserver->TrackPredictedFrame(Mover(Character), InjectedLocalFrame, Sync);
			Mover(Character)->OnPostSimulationRollback.AddDynamic(RollbackObserver.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
			bInjected = true;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversalCorrection injection frame=%d time=%.0f afterHandoff=%d location=%s component=%s priorBase=%s"),
				Backend->GetCurrentSimFrame(), Backend->GetCurrentSimTimeMs(), bAfterHandoff,
				*Default->GetLocation_WorldSpace().ToCompactString(), *Mover(Character)->GetUpdatedComponent()->GetComponentLocation().ToCompactString(), *PriorBaseName);
			BeforeHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FPredictionCorrection::BeforeDispatch);
			AfterHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FPredictionCorrection::AfterDispatch);
			Report(TEXT("injected"));
			return true;
		}
		bool Injected() const { return bInjected; }
		bool Observed() const { return bObserved; }
		bool PreservedContract() const { return bObserved && bSameFrame && bIdentity && bContext && bLifecycle && bMontage && bWarpHistory && (!bTrackInjectedFrame || bHistoricalContract); }
		void Report(const TCHAR* Phase) const
		{
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversalCorrection phase=%s injected=%d observed=%d afterHandoff=%d sameFrame=%d frame=%d->%d time=%.0f->%.0f delta=%s identity=%d context=%d lifecycle=%d montage=%d warpHistory=%d"),
				Phase, bInjected, bObserved, bTerminalExpected, bSameFrame, BeforeFrame, AfterFrame, BeforeTime, AfterTime,
				*Delta.ToCompactString(), bIdentity, bContext, bLifecycle, bMontage, bWarpHistory);
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversalCorrection clock local=%d->%d offset=%d->%d step=%d->%d rollbacks=%d->%d restoredFrame=%d expungedFrame=%d"),
				BeforeClock.LocalPendingFrame, AfterClock.LocalPendingFrame, BeforeClock.ServerOffset, AfterClock.ServerOffset,
				BeforeClock.StepMs, AfterClock.StepMs, BeforeRollbackCount, RollbackObserver.IsValid() ? RollbackObserver->Count : 0,
				RollbackObserver.IsValid() ? RollbackObserver->LastRestored.ServerFrame : INDEX_NONE,
				RollbackObserver.IsValid() ? RollbackObserver->LastExpunged.ServerFrame : INDEX_NONE);
			if (bTrackInjectedFrame)
				UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversalCorrection history injectedLocal=%d restoredLocal=%d witnessLocal=%d replaySteps=%d replacement=%d window=%d delta=%s contract=%d"),
					InjectedLocalFrame, WitnessRestoredFrame, WitnessLocalFrame, WitnessReplaySteps, bReplacedPredictedFrame, RequiredWarpWindow,
					*HistoricalDelta.ToCompactString(), bHistoricalContract);
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(BeforeHandle); FWorldDelegates::OnWorldPreActorTick.Remove(AfterHandle);
			BeforeHandle.Reset(); AfterHandle.Reset();
			if (Owner.IsValid() && Mover(Owner.Get()) && RollbackObserver.IsValid())
				Mover(Owner.Get())->OnPostSimulationRollback.RemoveDynamic(RollbackObserver.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
			if (RollbackObserver.IsValid()) RollbackObserver->StopTrackingFrame();
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
			if (bTrackInjectedFrame && RollbackObserver.IsValid())
			{
				if (bBeforeValid) RollbackObserver->BeginDispatch(BeforeClock.LocalPendingFrame);
				else RollbackObserver->EndDispatch();
			}
		}
		void AfterDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !bBeforeValid || !Owner.IsValid() || Owner->GetWorld() != World || !Liaison.IsValid()) return;
			if (bTrackInjectedFrame && RollbackObserver.IsValid()) RollbackObserver->EndDispatch();
			FMoverSyncState After;
			if (!Liaison->ReadPendingSyncState(After)) return;
			const FMoverDefaultSyncState* BeforeMove = Before.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const FMoverDefaultSyncState* AfterMove = After.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			if (!BeforeMove || !AfterMove) return;
			const FVector Movement = AfterMove->GetLocation_WorldSpace() - BeforeMove->GetLocation_WorldSpace();
			if (DiagnosticSamples++ < 12)
			{
				UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversalCorrection dispatch sample=%d afterHandoff=%d frame=%d->%d time=%.0f->%.0f before=%s after=%s component=%s"),
					DiagnosticSamples, bTerminalExpected, BeforeFrame, Liaison->GetCurrentSimFrame(), BeforeTime, Liaison->GetCurrentSimTimeMs(),
					*BeforeMove->GetLocation_WorldSpace().ToCompactString(), *AfterMove->GetLocation_WorldSpace().ToCompactString(),
					*Mover(Owner.Get())->GetUpdatedComponent()->GetComponentLocation().ToCompactString());
			}
			if (!RollbackObserver.IsValid() || RollbackObserver->Count <= BeforeRollbackCount) return;
			if (bTrackInjectedFrame)
			{
				// The current head may already have warped back during forward simulation. Require the
				// error to be replaced at an actually recorded affected frame by restore/replay in THIS dispatch.
				// A later unrelated rollback or normal forward movement cannot provide that snapshot.
				WitnessRestoredFrame = RollbackObserver->RestoredLocalFrame;
				WitnessReplaySteps = RollbackObserver->ReplaySteps;
				WitnessLocalFrame = RollbackObserver->ReplacedLocalFrame;
				bReplacedPredictedFrame = RollbackObserver->bHasReplacement;
				const FMoverDefaultSyncState* PredictedMove = RollbackObserver->PredictedSync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
				const FMoverDefaultSyncState* ReplacementMove = RollbackObserver->ReplacementSync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
				if (!bReplacedPredictedFrame || WitnessReplaySteps <= 0 || !PredictedMove || !ReplacementMove) return;
				HistoricalDelta = ReplacementMove->GetLocation_WorldSpace() - PredictedMove->GetLocation_WorldSpace();
				if (FVector::DotProduct(HistoricalDelta, CrossDirection) > -1.0) return;
				const auto ActiveHistoricalContract = [this](const FMoverSyncState& State)
				{
					const FRpgMoverTraversalSyncState* Traversal = State.SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
					return Traversal && Traversal->Command.Identity == OriginalIdentity && Traversal->Command.IsActive()
						&& !Traversal->bEndApplied && Traversal->Command.Context.Collider == OriginalCollider.Get()
						&& Traversal->Command.Context.Montage == OriginalMontage.Get()
						&& Traversal->Command.Context.FrontLedgeTarget.Equals(OriginalTarget, 0.01)
						&& Traversal->WarpModifiers.ContainsByPredicate([this](const FRpgMoverWarpModifierState& Warp)
						{ return Warp.WindowIndex == RequiredWarpWindow && Warp.Value.State == ERootMotionModifierState::Active; });
				};
				bHistoricalContract = ActiveHistoricalContract(RollbackObserver->RestoredSync)
					&& ActiveHistoricalContract(RollbackObserver->PredictedSync)
					&& ActiveHistoricalContract(RollbackObserver->ReplacementSync);
			}
			else if (FVector::DotProduct(Movement, CrossDirection) > -1.0) return;
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
						&& AfterTraversal->Command.Context.WarpTargetName == OriginalTargetName
						&& BeforeTraversal->Command.Context.BackLedgeWarpTargetName == OriginalRearName
						&& AfterTraversal->Command.Context.BackLedgeWarpTargetName == OriginalRearName
						&& BeforeTraversal->Command.Context.BackFloorWarpTargetName == OriginalFloorName
						&& AfterTraversal->Command.Context.BackFloorWarpTargetName == OriginalFloorName
						&& !BeforeTraversal->Command.Context.LandingSupport.IsValid()
						&& !AfterTraversal->Command.Context.LandingSupport.IsValid();
					bLifecycle = BeforeTraversal->Command.IsTerminal() && BeforeTraversal->Command.Phase == OriginalPhase
						&& AfterTraversal->Command.Phase == OriginalPhase && BeforeTraversal->bEndApplied
						&& AfterTraversal->bEndApplied && Clean(Owner.Get());
					const FGameplayAbilitySpec* Ability = Spec(Owner.Get());
					UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversalCorrection terminal expectedPhase=%d beforePhase=%d afterPhase=%d beforeEndApplied=%d afterEndApplied=%d clean=%d lease=%d collider=%s rootMove=%d abilityActive=%d frontTarget=%d rearTarget=%d floorTarget=%d"),
						static_cast<int32>(OriginalPhase), static_cast<int32>(BeforeTraversal->Command.Phase), static_cast<int32>(AfterTraversal->Command.Phase),
						BeforeTraversal->bEndApplied, AfterTraversal->bEndApplied, Clean(Owner.Get()), Mover(Owner.Get())->HasTraversalLease(),
						*GetPathNameSafe(Mover(Owner.Get())->GetTraversalCollider()),
						Mover(Owner.Get())->FindActiveLayeredMoveByType(FRpgMoverAbilityRootMotion::StaticStruct()) != nullptr,
						Ability && Ability->IsActive(), Warping(Owner.Get())->FindWarpTarget(TEXT("FrontLedge")) != nullptr,
						Warping(Owner.Get())->FindWarpTarget(TEXT("BackLedge")) != nullptr, Warping(Owner.Get())->FindWarpTarget(TEXT("BackFloor")) != nullptr);
					bMontage = OriginalMontage.IsValid() && (!Instance || !Instance->IsPlaying());
					bWarpHistory = BeforeTraversal->WarpModifiers.IsEmpty() && AfterTraversal->WarpModifiers.IsEmpty();
				}
				else
				{
					bContext = AfterTraversal->Command.Context.Collider == OriginalCollider.Get()
						&& AfterTraversal->Command.Context.Montage == OriginalMontage.Get()
						&& AfterTraversal->Command.Context.FrontLedgeTarget.GetLocation().Equals(OriginalTarget.GetLocation(), 1.0)
						&& AfterTraversal->Command.Context.FrontLedgeTarget.GetRotation().Equals(OriginalTarget.GetRotation(), 0.001)
						&& AfterTraversal->Command.Context.BackLedgeWarpTargetName == OriginalRearName
						&& (OriginalRearName.IsNone() || AfterTraversal->Command.Context.BackLedgeTarget.Equals(OriginalRearTarget, 0.01))
						&& AfterTraversal->Command.Context.BackFloorWarpTargetName == OriginalFloorName
						&& (OriginalFloorName.IsNone() || AfterTraversal->Command.Context.BackFloorTarget.Equals(OriginalFloorTarget, 0.01))
						&& AfterTraversal->Command.Context.LandingSupport == OriginalSupport
						&& AfterTraversal->Command.Context.LandingSupportTransform.Equals(OriginalSupportTransform, 0.01);
					bLifecycle = BeforeTraversal->Command.IsActive() && AfterTraversal->Command.IsActive() && !AfterTraversal->bEndApplied
						&& Mover(Owner.Get())->HasTraversalLease() && Mover(Owner.Get())->GetTraversalCollider() == OriginalCollider.Get();
					bMontage = Instance && Instance->GetInstanceID() == OriginalInstance && Instance->IsPlaying();
					// Mantle and Hurdle must witness rollback while the selected window is active on both sides;
					// a later correction during another window cannot stand in for this contract.
					const auto RelevantWarp = [this](const FRpgMoverTraversalSyncState& State)
					{
						return RequiredWarpWindow == INDEX_NONE ? ActiveWarp(State) : State.WarpModifiers.FindByPredicate(
							[this](const FRpgMoverWarpModifierState& Warp) { return Warp.WindowIndex == RequiredWarpWindow && Warp.Value.State == ERootMotionModifierState::Active; });
					};
					const FRpgMoverWarpModifierState* OldWarp = RelevantWarp(*BeforeTraversal);
					const FRpgMoverWarpModifierState* NewWarp = RelevantWarp(*AfterTraversal);
					// Compare immutable window/bone-cache identity, not a frozen trajectory or a particular allocation.
					bWarpHistory = OldWarp && NewWarp && OldWarp->WindowIndex == NewWarp->WindowIndex
						&& FMath::IsNearlyEqual(OldWarp->Value.StartTime, NewWarp->Value.StartTime)
						&& FMath::IsNearlyEqual(OldWarp->Value.EndTime, NewWarp->Value.EndTime)
						&& OldWarp->Value.CachedOffsetFromWarpPoint.IsSet() == NewWarp->Value.CachedOffsetFromWarpPoint.IsSet()
						&& (!OldWarp->Value.CachedOffsetFromWarpPoint.IsSet()
							|| OldWarp->Value.CachedOffsetFromWarpPoint.GetValue().Equals(NewWarp->Value.CachedOffsetFromWarpPoint.GetValue(), 0.01));
					// Earlier windows remain in the historical snapshot. Reconciliation may change their
					// accumulated transforms, but cannot exchange or drop any authored window identity.
					bWarpHistory &= BeforeTraversal->WarpModifiers.Num() == AfterTraversal->WarpModifiers.Num();
					for (const FRpgMoverWarpModifierState& Old : BeforeTraversal->WarpModifiers)
					{
						const FRpgMoverWarpModifierState* New = AfterTraversal->WarpModifiers.FindByPredicate(
							[&Old](const FRpgMoverWarpModifierState& Candidate) { return Candidate.WindowIndex == Old.WindowIndex; });
						bWarpHistory &= New && FMath::IsNearlyEqual(Old.Value.StartTime, New->Value.StartTime)
							&& FMath::IsNearlyEqual(Old.Value.EndTime, New->Value.EndTime);
					}
				}
			}
			Report(TEXT("reconciled"));
		}
		TWeakObjectPtr<APawn> Owner;
		TWeakObjectPtr<UMoverNetworkPredictionLiaisonComponent> Liaison;
		TWeakObjectPtr<UPrimitiveComponent> OriginalCollider;
		TWeakObjectPtr<UPrimitiveComponent> OriginalSupport;
		TWeakObjectPtr<UAnimMontage> OriginalMontage;
		TStrongObjectPtr<URpgMoverRollbackTestObserver> RollbackObserver;
		FRpgMoverTraversalIdentity OriginalIdentity;
		ERpgMoverTraversalPhase OriginalPhase = ERpgMoverTraversalPhase::None;
		FName OriginalTargetName, OriginalRearName, OriginalFloorName;
		FTransform OriginalTarget = FTransform::Identity, OriginalRearTarget = FTransform::Identity;
		FTransform OriginalFloorTarget = FTransform::Identity, OriginalSupportTransform = FTransform::Identity;
		FMoverSyncState Before;
		RpgMoverPredictionTests::FFixedPredictionHeadSnapshot BeforeClock, AfterClock;
		FDelegateHandle BeforeHandle, AfterHandle;
		FVector CrossDirection = FVector::ZeroVector, Delta = FVector::ZeroVector;
		int32 OriginalInstance = INDEX_NONE, BeforeFrame = INDEX_NONE, AfterFrame = INDEX_NONE, DiagnosticSamples = 0;
		int32 BeforeRollbackCount = 0;
		int32 RequiredWarpWindow = INDEX_NONE;
		int32 InjectedLocalFrame = INDEX_NONE, WitnessRestoredFrame = INDEX_NONE, WitnessLocalFrame = INDEX_NONE, WitnessReplaySteps = 0;
		FVector HistoricalDelta = FVector::ZeroVector;
		bool bTrackInjectedFrame = false, bReplacedPredictedFrame = false, bHistoricalContract = false;
		double BeforeTime = 0.0, AfterTime = 0.0;
		bool bInjected = false, bObserved = false, bTerminalExpected = false, bBeforeValid = false;
		bool bSameFrame = false, bIdentity = false, bContext = false, bLifecycle = false, bMontage = false, bWarpHistory = false;
	};
	struct FObservation
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<APawn> Character;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		FDelegateHandle Committed, Ended;
		FRpgMoverTraversalIdentity ObservedSimulationIdentity;
		TStrongObjectPtr<URpgMoverRollbackTestObserver> DeathRollbackObserver;
		FMoverSyncState BeforeDeathDispatch;
		RpgMoverPredictionTests::FFixedPredictionHeadSnapshot BeforeDeathClock;
		TWeakObjectPtr<UAnimMontage> Montage;
		FVector HandoffLocation = FVector::ZeroVector, HandoffVelocity = FVector::ZeroVector;
		FVector BackFloorLocation = FVector::ZeroVector;
		TWeakObjectPtr<UPrimitiveComponent> LandingSupport;
		float HandoffMontageTime = -1.0f, AbilityEndMontageTime = -1.0f, LastFrontWarpEnd = 0.0f;
		FVector DeathLocation = FVector::ZeroVector;
		FVector FirstTerminalDeathLocation = FVector::ZeroVector, BeforeDeathActorLocation = FVector::ZeroVector;
		FVector MaximumDeathDriftLocation = FVector::ZeroVector, MaximumDeathDriftSyncLocation = FVector::ZeroVector;
		float FirstTime = -1.0f, LastTime = -1.0f, LastWarpEnd = 0.0f, WarpYaw = 0.0f;
		float BestAlignmentError = 180.0f, MaximumPostWarpError = 0.0f;
		float LastLeaseSpeed = 0.0f;
		float MaximumProxyMontagePhaseError = 0.0f;
		float ProxyOnsetGapSeconds = 0.0f, ProxyOnsetGapFrameSeconds = 0.0f, MaximumProxyOnsetGapSeconds = 0.0f;
		int32 ProxyMontagePhaseSamples = 0, ProxyPreTraversalSamples = 0;
		bool bProxyMontageBeforeTraversal = false, bProxyMontagePhaseMismatch = false, bProxyActiveMontageMissing = false;
		bool bProxyTraversalOnsetDelayed = false;
		float DeadSeconds = 0.0f, MaximumDeathDrift = 0.0f;
		float MaximumDeathDriftSeconds = 0.0f, OwnerDeathConvergenceSeconds = 0.0f;
		int32 BeforeDeathRollbacks = 0, OwnerDeathAnchorAdjustments = 0;
		int32 Commits = 0, Ends = 0, SuccessfulEnds = 0, CancelledEnds = 0, InstanceId = INDEX_NONE, PostWarpSamples = 0;
		bool bCancelled = false, bLease = false, bWarpTarget = false, bBackWarpTarget = false, bMontageRestarted = false;
		bool bLandedOnObstacle = false, bContinuedMoving = false, bRestoredFacing = false;
		bool bCrossedRear = false, bReleasedFalling = false, bLandedBeyond = false, bHandoffFalling = false;
		bool bBackFloorTarget = false, bSupportContext = false, bNeedsBackWarp = false, bHandoffSupportedBeyond = false;
		bool bOrdinaryJump = false, bGroundedAfterJump = false, bCommittedAfterGroundedRetry = false;
		bool bCommittedWhileAirborne = false;
		bool bConfirmedPlayCancelled = false;
		bool bCleanAfterInterruption = false, bObservedDeath = false, bHandoffGrounded = false;
		bool bTerminalDeath = false, bInvalidDeath = false, bJoinedTerminalState = false;
		bool bBeforeDeathDispatchValid = false, bOwnerDeathConverged = false;
		bool bSawActiveSimulation = false, bCapturedHandoff = false, bStoppedAtHandoff = false, bLostUnrelatedTarget = false;
	};
}

/** Saved-map input coverage for the opt-in Mover traversal path; no fixture teleports or synthetic candidates. */
struct FRpgGaspMoverTraversalTestFixture::FState
{
	FAutomationTestBase* TestRunner;
	FNoDiscardAsserter& Assert;
	FTestCommandBuilder& TestCommandBuilder;
	FState(FAutomationTestBase* InRunner, FNoDiscardAsserter& InAssert, FTestCommandBuilder& InBuilder)
		: TestRunner(InRunner), Assert(InAssert), TestCommandBuilder(InBuilder) {}
	using FObservation = RpgGaspMoverTraversalTests::FObservation;
	using EScenario = RpgGaspMoverTraversalTests::EScenario;
	using EGait = RpgGaspMoverTraversalTests::EGait;
	RpgGaspMoverTraversalTests::FSaveIsolation Isolation;
	RpgGaspMoverTraversalTests::FPredictionCorrection Correction;
	TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
	FPrimaryAssetId PreviousExperience;
	TWeakObjectPtr<UWorld> ServerWorld, ClientWorld, ObserverWorld;
	TWeakObjectPtr<APlayerController> InputController;
	TWeakObjectPtr<UPrimitiveComponent> Obstacle, DisabledCollider, DisabledSupport;
	TWeakObjectPtr<AActor> Blocker;
	ECollisionEnabled::Type PreviousCollision = ECollisionEnabled::NoCollision;
	ECollisionEnabled::Type PreviousSupportCollision = ECollisionEnabled::NoCollision;
	FBox Bounds{ForceInit}, DeckBounds{ForceInit};
	FObservation OwnerRecord, AuthorityRecord, ProxyRecord;
	TStrongObjectPtr<URpgMoverTraversalNotifyTestObserver> ProxyNotifyObserver;
	TWeakObjectPtr<UAnimInstance> ProxyNotifyAnimation;
	TWeakObjectPtr<UAnimMontage> ReplacementMontage, RepeatedTraversalMontage;
	FRpgMoverTraversalIdentity FirstTraversalIdentity;
	int32 FirstTraversalInstanceId = INDEX_NONE, FirstProxyInstanceId = INDEX_NONE;
	int32 ReplacementOwnerInstance = INDEX_NONE, ReplacementProxyInstance = INDEX_NONE, AttackReleaseFrames = 0;
	float ReplacementOwnerFirst = -1.f, ReplacementOwnerLast = -1.f, ReplacementProxyFirst = -1.f, ReplacementProxyLast = -1.f;
	bool bReplacementTriggered = false, bReplayStarted = false, bReplacementRestarted = false, bTraversalReplacedTheAttack = false;
	bool bReplacementOverlappedPresentedTraversal = false, bFirstProxyHadTraversal = false;
	FDelegateHandle TickHandle, BeforeDeathDispatchHandle, AfterDeathDispatchHandle;
	FVector SpawnLocation = FVector::ZeroVector;
	int32 PlayerId = INDEX_NONE, LaneIndex = 0, Waypoint = 0;
	EAction Action = EAction::Mantle;
	EGait Gait = EGait::Run;
	EScenario Scenario = EScenario::Success;
	float ApproachYaw = 0.0f, PressSpeed = 0.0f, PressYaw = 0.0f, MaximumViewError = 0.0f;
	double StartedAt = 0.0, PressedAt = -1.0, ContactAt = -1.0;
	double FloorZ = 0.0;
	bool bConfigured = false, bOwnsSession = false, bDriving = false, bHost = false;
	bool bFinalHeading = false, bSpaceReleased = false, bMoveReleased = false, bInterrupted = false;
	bool bLateJoinRequested = false, bCheckpointLogged = false;
	bool bResumedStandingInput = false;

	void Initialize()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Mover traversal automation refuses to interrupt an existing PIE session."));
				return;
			}
		Isolation.Start();
		PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = FPrimaryAssetId();
		bConfigured = true;
		TestCommandBuilder.OnTearDown(TEXT("Release Mover traversal test input and only its own PIE session"), [this]() { Cleanup(); });
	}

	UWorld* InputWorld() const { return bHost ? ServerWorld.Get() : ClientWorld.Get(); }
	APawn* Owner() const { return RpgGaspMoverTraversalTests::LocalPawn(InputWorld()); }
	APawn* Authority() const { return RpgGaspMoverTraversalTests::Pawn(ServerWorld.Get(), PlayerId); }
	APawn* Observer() const { return RpgGaspMoverTraversalTests::Pawn(ObserverWorld.Get(), PlayerId); }
	bool InterruptedScenario() const { return Scenario == EScenario::Cancel || Scenario == EScenario::Death || Scenario == EScenario::ColliderLoss || Scenario == EScenario::BlockedExit || Scenario == EScenario::SupportLoss; }
	bool ReplacementScenario() const { return Scenario == EScenario::ReplaceActiveAndReplay || Scenario == EScenario::ReplacePendingAndReplay; }
	bool HoldMovement() const { return Scenario != EScenario::LateJoin && Scenario != EScenario::CorrectDuringWarp && Scenario != EScenario::CorrectAfterWarp && Scenario != EScenario::NaturalEnd; }
	bool Landed(const FObservation& Record) const { return Action == EAction::Mantle ? Record.bLandedOnObstacle : Record.bLandedBeyond; }
	bool Finished(const FObservation& Record) const { return Landed(Record) && Record.bRestoredFacing && (!HoldMovement() || Record.bContinuedMoving); }
	void Queue(EGait InGait, EScenario InScenario = EScenario::Success, bool bInHost = false, float InYaw = 0.0f)
	{
		using namespace RpgGaspMoverTraversalTests;
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
			if (!Ready(InputWorld(), Character, Action) || !Input || !Input->IsReadyToBindInputs() || !Spec(Character) || !Mover(Character)->IsOnGround()) return false;
			PlayerId = Character->GetPlayerState()->GetPlayerId();
			if (!Ready(ServerWorld.Get(), Authority(), Action) || !Isolation.Isolated(ServerWorld.Get())) return false;
			if (ReplacementScenario() && (!PrimaryAttack(Authority()) || !PrimaryAttack(Character))) return false;
			return Scenario == EScenario::LateJoin || (Ready(ObserverWorld.Get(), Observer(), Action) && Observer()->GetLocalRole() == ROLE_SimulatedProxy);
		}, FTimespan::FromSeconds(60.0))
		.Then(TEXT("Find the prepared one-meter lane and start ordinary forward input"), [this]()
		{
			FindLane();
			ASSERT_THAT(IsTrue(Obstacle.IsValid() && Bounds.IsValid));
			if (!Obstacle.IsValid()) return;
			if (Action == EAction::Vault)
			{
				ASSERT_THAT(IsTrue(DeckBounds.IsValid && FMath::Abs(Bounds.Max.Z - DeckBounds.Max.Z - 100.0) < 2.0
					&& Bounds.GetSize().X >= 29.0 && Bounds.GetSize().X <= 59.0 && Bounds.GetSize().Y >= 350.0));
			}
			else if (Action == EAction::Hurdle)
			{
				ASSERT_THAT(IsTrue(FMath::Abs(Bounds.Max.Z - FloorZ - 100.0) < 5.0
					&& Bounds.GetSize().X > 10.0 && Bounds.GetSize().X <= 59.0 && Bounds.GetSize().Y >= 350.0));
				ASSERT_THAT(IsTrue(LaneIndex == 0 ? Bounds.GetSize().X < 25.0 : Bounds.GetSize().X > 25.0));
				if (LaneIndex == 2) ASSERT_THAT(IsTrue(FMath::Abs(Bounds.GetSize().X - 59.0) < 0.1));
			}
			else ASSERT_THAT(IsTrue(FMath::Abs(Bounds.GetSize().Z - 100.0) < 2.0 && Bounds.GetSize().X >= 350.0 && Bounds.GetSize().Y >= 350.0));
			InputController = Cast<APlayerController>(Owner()->GetController());
			ASSERT_THAT(IsTrue(InputController.IsValid()));
			if (!InputController.IsValid()) return;
			InputController->SetIgnoreLookInput(true);
			InputController->SetControlRotation(FRotator::ZeroRotator);
			SpawnLocation = Owner()->GetActorLocation(); StartedAt = InputWorld()->GetTimeSeconds();
			Subscribe(Owner(), OwnerRecord); Subscribe(Authority(), AuthorityRecord);
			if (Scenario != EScenario::LateJoin) SubscribeProxy();
			bDriving = true;
			TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FState::Tick);
			if (Scenario == EScenario::Death)
			{
				BeforeDeathDispatchHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FState::BeforeDeathDispatch);
				AfterDeathDispatchHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FState::AfterDeathDispatch);
			}
			if (Gait == EGait::Walk) Key(EKeys::LeftControl, true);
			Key(EKeys::W, true);
		})
		.Until(TEXT("Real input completes the chosen traversal, fallback or lifecycle scenario"), [this]() { return Complete(); }, FTimespan::FromSeconds(60.0))
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
		using namespace RpgGaspMoverTraversalTests;
		if (!Owner() || !ActiveWorld(InputWorld())) return;
		if (Action == EAction::Vault)
		{
			TArray<AActor*> Barriers;
			for (TActorIterator<AActor> It(InputWorld()); It; ++It)
				if (It->ActorHasTag(VaultTag)) Barriers.Add(*It);
			Barriers.Sort([](const AActor& A, const AActor& B) { return A.GetActorLocation().Y < B.GetActorLocation().Y; });
			if (!Barriers.IsValidIndex(LaneIndex)) return;
			Obstacle = Barriers[LaneIndex]->FindComponentByClass<UStaticMeshComponent>();
			if (!Obstacle.IsValid()) return;
			Bounds = Obstacle->Bounds.GetBox();
			for (TActorIterator<AActor> It(InputWorld()); It; ++It)
			{
				const UStaticMeshComponent* Physical = It->FindComponentByClass<UStaticMeshComponent>();
				if (!It->ActorHasTag(VaultApproachTag) || !Physical || !Physical->IsQueryCollisionEnabled()) continue;
				const FBox Box = Physical->Bounds.GetBox();
				if (FMath::Abs(Box.GetCenter().Y - Bounds.GetCenter().Y) < 10.0) { DeckBounds = Box; break; }
			}
			return;
		}
		const FVector Position = Owner()->GetActorLocation();
		const double Feet = Position.Z - Capsule(Owner())->GetScaledCapsuleHalfHeight();
		// An idle Mover can retain its PlayerStart clearance until the first movement input.
		// Measure the authored floor itself so lane height does not depend on that spawn offset.
		FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(RpgMoverTraversalFixtureFloor), false, Owner());
		TArray<AActor*> AttachedActors;
		Owner()->GetAttachedActors(AttachedActors, true, true);
		FloorParams.AddIgnoredActors(AttachedActors);
		FHitResult FloorHit;
		const bool bFloorHit = InputWorld()->LineTraceSingleByChannel(FloorHit, Position,
			Position - FVector(0.0, 0.0, 500.0), ECC_Visibility, FloorParams);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal lane floor pawn=%s position=%s feet=%.2f ground=%d hit=%d floor=%s impact=%s normal=%s"),
			*GetPathNameSafe(Owner()), *Position.ToCompactString(), Feet, Mover(Owner())->IsOnGround(), bFloorHit,
			*GetPathNameSafe(FloorHit.GetComponent()), *FloorHit.ImpactPoint.ToCompactString(), *FloorHit.ImpactNormal.ToCompactString());
		if (!bFloorHit || !FloorHit.IsValidBlockingHit() || FloorHit.ImpactNormal.Z < 0.7) return;
		const double FloorHeight = FloorHit.ImpactPoint.Z;
		if (Action == EAction::Hurdle)
		{
			FloorZ = FloorHeight;
			TArray<AActor*> Barriers;
			for (TActorIterator<AActor> It(InputWorld()); It; ++It)
				if (It->ActorHasTag(HurdleTag)) Barriers.Add(*It);
			Barriers.Sort([](const AActor& A, const AActor& B) { return A.GetActorLocation().Y < B.GetActorLocation().Y; });
			if (!Barriers.IsValidIndex(LaneIndex)) return;
			Obstacle = Barriers[LaneIndex]->FindComponentByClass<UStaticMeshComponent>();
			if (Obstacle.IsValid() && Obstacle->IsQueryCollisionEnabled()) Bounds = Obstacle->Bounds.GetBox();
			return;
		}
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
		UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal lane obstacle=%s bounds=%s..%s floorHeight=%.2f"),
			*GetPathNameSafe(Obstacle.Get()), *Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString(), FloorHeight);
	}
	UPrimitiveComponent* Collider(UWorld* World) const
	{
		if (!RpgGaspMoverTraversalTests::ActiveWorld(World) || !Obstacle.IsValid()) return nullptr;
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
		using namespace RpgGaspMoverTraversalTests;
		if (!Character || !ASC(Character)) return;
		Record.World = Character->GetWorld(); Record.Character = Character; Record.AbilitySystem = ASC(Character);
		if (Scenario == EScenario::Death && Character->GetLocalRole() == ROLE_AutonomousProxy)
		{
			Record.DeathRollbackObserver.Reset(NewObject<URpgMoverRollbackTestObserver>());
			Mover(Character)->OnPostSimulationRollback.AddDynamic(Record.DeathRollbackObserver.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
		}
		Warping(Character)->AddOrUpdateWarpTargetFromLocationAndRotation(UnrelatedTarget, UnrelatedLocation, FRotator::ZeroRotator);
		Record.Committed = ASC(Character)->AbilityCommittedCallbacks.AddLambda([Snapshot = &Record](UGameplayAbility* Ability)
		{
			if (Ability && Ability->GetClass()->GetPathName() == AbilityPath)
			{
				++Snapshot->Commits;
				const bool bGroundedAtCommit = Mover(Snapshot->Character.Get()) && Mover(Snapshot->Character.Get())->IsOnGround();
				Snapshot->bCommittedWhileAirborne |= !bGroundedAtCommit;
				Snapshot->bCommittedAfterGroundedRetry |= Snapshot->bGroundedAfterJump || (Snapshot->bOrdinaryJump && bGroundedAtCommit);
				UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal commit pawn=%s attempt=%d grounded=%d"),
					*GetPathNameSafe(Snapshot->Character.Get()), Snapshot->Commits, bGroundedAtCommit);
				ReportConnections(TEXT("commit"), Snapshot->World.Get());
			}
		});
		Record.Ended = ASC(Character)->OnAbilityEnded.AddLambda([this, Snapshot = &Record](const FAbilityEndedData& Data)
		{
			APawn* Character = Snapshot->Character.Get();
			if (!Character || !Data.AbilityThatEnded || Data.AbilityThatEnded->GetClass()->GetPathName() != AbilityPath) return;
			UAnimInstance* Animation = Mesh(Character) ? Mesh(Character)->GetAnimInstance() : nullptr;
			const FAnimMontageInstance* Instance = Animation ? Animation->GetMontageInstanceForID(Snapshot->InstanceId) : nullptr;
			if (Snapshot->Montage.IsValid() && Instance && Instance->IsStopped()
				&& (Instance->Montage == Snapshot->Montage.Get() || Instance->Montage == nullptr))
			{
				// Auto blend-out removes the active montage lookup before completion. The exact play instance
				// remains available through this GAS end callback, even after natural termination clears its asset.
				Snapshot->AbilityEndMontageTime = Instance->GetPosition();
			}
			++Snapshot->Ends; Snapshot->bCancelled |= Data.bWasCancelled;
			Snapshot->bConfirmedPlayCancelled |= Scenario == EScenario::HeldRetry && AuthorityRecord.Commits > 0 && Data.bWasCancelled;
			if (Data.bWasCancelled) ++Snapshot->CancelledEnds;
			else ++Snapshot->SuccessfulEnds;
		});
	}
	void SubscribeProxy()
	{
		ProxyRecord.World = ObserverWorld; ProxyRecord.Character = Observer();
		ProxyRecord.AbilitySystem = RpgGaspMoverTraversalTests::ASC(Observer());
		RpgGaspMoverTraversalTests::Warping(Observer())->AddOrUpdateWarpTargetFromLocationAndRotation(
			RpgGaspMoverTraversalTests::UnrelatedTarget, RpgGaspMoverTraversalTests::UnrelatedLocation, FRotator::ZeroRotator);
		ProxyNotifyObserver.Reset(NewObject<URpgMoverTraversalNotifyTestObserver>());
		for (const FRpgTraversalAnimationEntry& Entry : RpgGaspMoverTraversalTests::AnimationEntries(Observer(), Action))
			ProxyNotifyObserver->AddMontage(Entry.Montage.Get());
		ProxyNotifyAnimation = RpgGaspMoverTraversalTests::Mesh(Observer())->GetAnimInstance();
		ProxyNotifyAnimation->OnPlayMontageNotifyBegin.AddDynamic(ProxyNotifyObserver.Get(), &URpgMoverTraversalNotifyTestObserver::ObserveBegin);
		ProxyNotifyAnimation->OnPlayMontageNotifyEnd.AddDynamic(ProxyNotifyObserver.Get(), &URpgMoverTraversalNotifyTestObserver::ObserveEnd);
	}
	void ReportMontage(const TCHAR* Phase, const FObservation& Record, APawn* Character, UAnimInstance* Animation,
		UAnimMontage* Montage, const FAnimMontageInstance* Instance, float Position) const
	{
		const URpgAbilitySystemComponent* Component = RpgGaspMoverTraversalTests::ASC(Character);
		// The engine getter is protected; inspect its existing replicated property without changing it.
		const FStructProperty* Property = FindFProperty<FStructProperty>(UAbilitySystemComponent::StaticClass(), TEXT("RepAnimMontageInfo"));
		const FGameplayAbilityRepAnimMontage* Replicated = Component && Property
			? Property->ContainerPtrToValuePtr<FGameplayAbilityRepAnimMontage>(Component) : nullptr;
		UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal montage %s pawn=%s animation=%s role=%d original=%s instance=%d current=%s instance=%d previousTime=%.4f currentTime=%.4f replicatedPlayId=%d replicatedStopped=%d localStopped=%d"),
			Phase, *GetPathNameSafe(Character), *GetPathNameSafe(Animation), static_cast<int32>(Character->GetLocalRole()),
			*GetPathNameSafe(Record.Montage.Get()), Record.InstanceId, *GetPathNameSafe(Montage), Instance ? Instance->GetInstanceID() : INDEX_NONE,
			Record.LastTime, Position, Replicated ? static_cast<int32>(Replicated->PlayInstanceId) : INDEX_NONE,
			Replicated ? static_cast<int32>(Replicated->IsStopped) : INDEX_NONE, Instance ? static_cast<int32>(Instance->IsStopped()) : INDEX_NONE);
		RpgGaspMoverTraversalTests::ReportConnections(Phase, Record.World.Get());
	}
	static void RecordDeathDrift(FObservation& Record, const FVector& ActorLocation, const FVector& SyncLocation)
	{
		const float Drift = static_cast<float>(FVector::Dist(Record.DeathLocation, ActorLocation));
		if (Drift > Record.MaximumDeathDrift)
		{
			Record.MaximumDeathDrift = Drift; Record.MaximumDeathDriftLocation = ActorLocation;
			Record.MaximumDeathDriftSyncLocation = SyncLocation; Record.MaximumDeathDriftSeconds = Record.DeadSeconds;
		}
	}
	void BeforeDeathDispatch(UWorld* World, ELevelTick, float)
	{
		using namespace RpgGaspMoverTraversalTests;
		FObservation& Record = OwnerRecord;
		if (World != Record.World.Get() || !ActiveWorld(World) || !Record.bObservedDeath || !Record.bTerminalDeath
			|| Record.bOwnerDeathConverged || !Record.DeathRollbackObserver.IsValid()) return;
		APawn* Character = Record.Character.Get();
		UMoverNetworkPredictionLiaisonComponent* Liaison = Character ? Character->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>() : nullptr;
		Record.bBeforeDeathDispatchValid = Liaison && Liaison->ReadPendingSyncState(Record.BeforeDeathDispatch);
		Record.BeforeDeathClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison);
		Record.BeforeDeathRollbacks = Record.DeathRollbackObserver->Count;
		Record.BeforeDeathActorLocation = Character ? Character->GetActorLocation() : FVector::ZeroVector;
	}
	void AfterDeathDispatch(UWorld* World, ELevelTick, float DeltaSeconds)
	{
		using namespace RpgGaspMoverTraversalTests;
		FObservation& Record = OwnerRecord;
		if (World != Record.World.Get() || !Record.bBeforeDeathDispatchValid || Record.bOwnerDeathConverged
			|| !Record.DeathRollbackObserver.IsValid() || Record.DeathRollbackObserver->Count <= Record.BeforeDeathRollbacks) return;
		APawn* Character = Record.Character.Get();
		UMoverNetworkPredictionLiaisonComponent* Liaison = Character ? Character->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>() : nullptr;
		FMoverSyncState After;
		if (!Liaison || !Liaison->ReadPendingSyncState(After)) return;
		const FMoverDefaultSyncState* BeforeMove = Record.BeforeDeathDispatch.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
		const FMoverDefaultSyncState* AfterMove = After.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
		const float ConvergenceSeconds = Record.DeadSeconds + DeltaSeconds;
		const bool bSameHead = Record.BeforeDeathClock.IsSameLocalHead(RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison));
		const bool bTerminalBefore = IsTerminalDeath(Record.BeforeDeathDispatch), bTerminalAfter = IsTerminalDeath(After);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal death rollback seconds=%.4f rollbacks=%d->%d sameHead=%d terminal=%d->%d before=%s after=%s actor=%s authorityAnchor=%s"),
			ConvergenceSeconds, Record.BeforeDeathRollbacks, Record.DeathRollbackObserver->Count, bSameHead, bTerminalBefore, bTerminalAfter,
			BeforeMove ? *BeforeMove->GetLocation_WorldSpace().ToCompactString() : TEXT("missing"),
			AfterMove ? *AfterMove->GetLocation_WorldSpace().ToCompactString() : TEXT("missing"),
			*Character->GetActorLocation().ToCompactString(), *AuthorityRecord.DeathLocation.ToCompactString());
		if (!AuthorityRecord.bTerminalDeath || ConvergenceSeconds > 0.75f || !bSameHead || !bTerminalBefore || !bTerminalAfter
			|| !BeforeMove || !AfterMove || Record.OwnerDeathAnchorAdjustments != 0
			|| FVector::DistSquared(AfterMove->GetLocation_WorldSpace(), AuthorityRecord.DeathLocation) > 1.0
			|| FVector::DistSquared(Character->GetActorLocation(), AfterMove->GetLocation_WorldSpace()) > 1.0) return;
		// Preserve every ordinary-motion sample around this one proven authoritative reconciliation.
		// No timer, arbitrary position jump or second rollback may silently re-anchor the corpse.
		RecordDeathDrift(Record, Record.BeforeDeathActorLocation, BeforeMove->GetLocation_WorldSpace());
		Record.DeathLocation = AuthorityRecord.DeathLocation;
		Record.bOwnerDeathConverged = true; Record.OwnerDeathConvergenceSeconds = ConvergenceSeconds;
		Record.OwnerDeathAnchorAdjustments = 1;
		Record.BeforeDeathDispatch = FMoverSyncState(); Record.bBeforeDeathDispatchValid = false;
	}
	void TryReplaceTraversal()
	{
		using namespace RpgGaspMoverTraversalTests;
		if (!ReplacementScenario() || bReplacementTriggered || !AuthorityRecord.bSawActiveSimulation || !AuthorityRecord.Montage.IsValid()) return;
		if (Scenario == EScenario::ReplaceActiveAndReplay && (!ProxyRecord.bSawActiveSimulation || !ProxyRecord.Montage.IsValid())) return;
		const FGameplayAbilitySpec* Ability = Spec(Authority());
		const FRpgWeaponAttackDefinition* Attack = PrimaryAttack(Authority());
		if (!Ability || !Ability->IsActive() || !Attack || !Attack->Montage) return;
		bFirstProxyHadTraversal = ProxyRecord.bSawActiveSimulation;
		FirstTraversalIdentity = AuthorityRecord.ObservedSimulationIdentity;
		FirstTraversalInstanceId = AuthorityRecord.InstanceId;
		FirstProxyInstanceId = ProxyRecord.InstanceId;
		RepeatedTraversalMontage = AuthorityRecord.Montage;
		ReplacementMontage = Attack->Montage;
		bReplacementTriggered = bInterrupted = true;
		Key(EKeys::W, false); bMoveReleased = true;
		Key(EKeys::SpaceBar, false); bSpaceReleased = true;
		// Exercise the real equipment input and ordinary replicated GAS montage immediately after a
		// gameplay cancellation. The observer still has the old traversal in its interpolation history.
		ASC(Authority())->CancelAbilityHandle(Ability->Handle);
		Key(EKeys::LeftMouseButton, true); AttackReleaseFrames = 2;
		Report(TEXT("replacement_requested"));
	}
	void ObserveReplacement(APawn* Character, bool bProxy)
	{
		using namespace RpgGaspMoverTraversalTests;
		if (!ReplacementScenario() || !bReplacementTriggered || bReplayStarted || !Character || !Mesh(Character)) return;
		UAnimInstance* Animation = Mesh(Character)->GetAnimInstance();
		if (!Animation) return;
		UAnimMontage* Current = Animation->GetCurrentActiveMontage();
		int32& InstanceId = bProxy ? ReplacementProxyInstance : ReplacementOwnerInstance;
		float& First = bProxy ? ReplacementProxyFirst : ReplacementOwnerFirst;
		float& Last = bProxy ? ReplacementProxyLast : ReplacementOwnerLast;
		if (Current == ReplacementMontage.Get() && Animation->Montage_IsPlaying(Current))
		{
			const FAnimMontageInstance* Instance = Animation->GetActiveInstanceForMontage(Current);
			if (!Instance) return;
			if (InstanceId == INDEX_NONE) { InstanceId = Instance->GetInstanceID(); First = Instance->GetPosition(); }
			else bReplacementRestarted |= InstanceId != Instance->GetInstanceID();
			Last = Instance->GetPosition();
			if (bProxy)
			{
				const FRpgMoverTraversalSyncState* Traversal = Mover(Character)->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
				bReplacementOverlappedPresentedTraversal |= Traversal && Traversal->Command.IsActive() && Traversal->Command.Identity == FirstTraversalIdentity;
			}
		}
		else if (bProxy && InstanceId != INDEX_NONE && IsTraversal(Character, Current, Action))
		{
			bTraversalReplacedTheAttack = true;
		}
	}
	void TryReplayTraversal()
	{
		using namespace RpgGaspMoverTraversalTests;
		if (!ReplacementScenario() || !bReplacementTriggered || bReplayStarted
			|| ReplacementOwnerLast < ReplacementOwnerFirst + .1f || ReplacementProxyLast < ReplacementProxyFirst + .1f) return;
		for (APawn* Character : { Owner(), Authority(), Observer() })
		{
			if (!Character || !Clean(Character) || !Mover(Character)->IsOnGround()
				|| Mesh(Character)->GetAnimInstance()->Montage_IsPlaying(ReplacementMontage.Get())) return;
		}
		TestRunner->TestTrue(TEXT("Equipment replacement overlapped the observer's old active NP traversal"), bReplacementOverlappedPresentedTraversal);
		TestRunner->TestFalse(TEXT("Buffered traversal cannot take over the equipment montage"), bTraversalReplacedTheAttack);
		TestRunner->TestFalse(TEXT("Ordinary equipment replacement keeps one montage instance"), bReplacementRestarted);
		TestRunner->TestEqual(TEXT("Replacement occurred at the requested observer lifecycle boundary"),
			bFirstProxyHadTraversal, Scenario == EScenario::ReplaceActiveAndReplay);
		TestRunner->TestFalse(TEXT("First traversal did not start before its presented movement"), ProxyRecord.bProxyMontageBeforeTraversal);
		TestRunner->TestFalse(TEXT("First traversal kept its presented montage phase"), ProxyRecord.bProxyMontagePhaseMismatch);
		TestRunner->TestFalse(TEXT("First traversal did not restart"), ProxyRecord.bMontageRestarted);
		Report(TEXT("replacement_completed"));
		// Keep the same worlds, actors, ASC, mesh and callback bindings; only per-play observations reset.
		for (FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
		{
			const auto World = Record->World; const auto Character = Record->Character; const auto AbilitySystem = Record->AbilitySystem;
			const FDelegateHandle Committed = Record->Committed, Ended = Record->Ended;
			*Record = FObservation();
			Record->World = World; Record->Character = Character; Record->AbilitySystem = AbilitySystem;
			Record->Committed = Committed; Record->Ended = Ended;
		}
		bReplayStarted = true; bInterrupted = false; bFinalHeading = false;
		bSpaceReleased = false; bMoveReleased = false; PressedAt = -1.0; ContactAt = -1.0;
		InputController->SetControlRotation(FRotator::ZeroRotator);
		Key(EKeys::W, true);
	}
	/** Sample after NP finalization and skeletal animation, when both values describe this displayed frame. */
	void ObserveProxyPresentation(FObservation& Record, APawn* Character, UAnimInstance* Animation,
		UAnimMontage* VisibleMontage, const FRpgMoverTraversalSyncState* Presented, bool bPlaying, bool bActive,
		bool bPresentedTraversing, float DeltaSeconds)
	{
		using namespace RpgGaspMoverTraversalTests;
		const bool bWasInvalid = Record.bProxyMontageBeforeTraversal || Record.bProxyMontagePhaseMismatch || Record.bProxyActiveMontageMissing || Record.bProxyTraversalOnsetDelayed;
		if (!bActive && !Record.bSawActiveSimulation && bPresentedTraversing && !bInterrupted)
		{
			Record.ProxyOnsetGapSeconds += DeltaSeconds;
			Record.ProxyOnsetGapFrameSeconds = FMath::Max(Record.ProxyOnsetGapFrameSeconds, DeltaSeconds);
			Record.MaximumProxyOnsetGapSeconds = FMath::Max(Record.MaximumProxyOnsetGapSeconds, Record.ProxyOnsetGapSeconds);
			const UNetworkPredictionWorldManager* Prediction = Character->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
			const float FixedStepSeconds = Prediction ? Prediction->GetFixedTickState().FixedStepMS * .001f : 0.0f;
			// Pct=0 preserves the inactive endpoint although stock Mover already names Traversing.
			// Allow that boundary and one observed render frame, but reject a multi-frame gap where
			// the capsule traverses while its animation state still describes the approach.
			Record.bProxyTraversalOnsetDelayed |= Record.ProxyOnsetGapSeconds > FixedStepSeconds + Record.ProxyOnsetGapFrameSeconds + .005f;
		}
		else
		{
			Record.ProxyOnsetGapSeconds = Record.ProxyOnsetGapFrameSeconds = 0.0f;
		}
		if (!bActive && !Record.bSawActiveSimulation)
		{
			++Record.ProxyPreTraversalSamples;
			// A late packet must not start traversal while the observer still displays the approach.
			// An already-presented play may retain its authored blend-out after a terminal snapshot.
			Record.bProxyMontageBeforeTraversal |= bPlaying;
		}
		UAnimMontage* ExpectedMontage = bActive ? Presented->Command.Context.Montage.Get() : nullptr;
		const FAnimMontageInstance* Instance = ExpectedMontage && Animation ? Animation->GetActiveInstanceForMontage(ExpectedMontage) : nullptr;
		float PhaseError = 0.0f;
		if (bActive && ExpectedMontage && !bInterrupted)
		{
			const bool bExpectedPlaying = bPlaying && VisibleMontage == ExpectedMontage && Instance;
			// Immediate gameplay cancellation is allowed even before its delayed NP tombstone.
			// Natural montage blend-out is likewise not required to retain an active instance.
			float SourceHandoff = Presented->Command.Context.HandoffTimeSeconds;
			const FRpgTraversalAnimationEntry* Entry = AnimationEntries(Character, Action).FindByPredicate(
				[ExpectedMontage](const FRpgTraversalAnimationEntry& Candidate) { return Candidate.Montage == ExpectedMontage; });
			const FCharacterDefaultInputs* PresentedInputs = Mover(Character)->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>();
			if (Entry && Entry->MovementInputHandoffTime > 0.f && PresentedInputs
				&& !PresentedInputs->GetMoveInput().Equals(FVector::ZeroVector, .1))
			{
				// Match the approved source notify's conditional handoff rather than requiring a visible
				// instance after its legitimate WithMovementInput blend-out has already begun.
				SourceHandoff = FMath::Min(SourceHandoff, Entry->MovementInputHandoffTime);
			}
			const float RequiredUntil = FMath::Min(SourceHandoff,
				ExpectedMontage->GetPlayLength() - ExpectedMontage->GetDefaultBlendOutTime());
			Record.bProxyActiveMontageMissing |= Presented->MontagePosition < RequiredUntil - KINDA_SMALL_NUMBER && !bExpectedPlaying;
			if (bExpectedPlaying)
			{
				++Record.ProxyMontagePhaseSamples;
				PhaseError = FMath::Abs(Instance->GetPosition() - Presented->MontagePosition);
				Record.MaximumProxyMontagePhaseError = FMath::Max(Record.MaximumProxyMontagePhaseError, PhaseError);
				const UNetworkPredictionWorldManager* Prediction = Character->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
				const float FixedStepSeconds = Prediction ? Prediction->GetFixedTickState().FixedStepMS * .001f : 0.0f;
				// Permit one simulation sample plus numerical tolerance, never the interpolation buffer's
				// ~100 ms lead that caused the observer to roll before reaching the obstacle.
				const float AllowedPhaseError = FixedStepSeconds * Presented->Command.Context.PlayRate * ExpectedMontage->RateScale + .005f;
				Record.bProxyMontagePhaseMismatch |= PhaseError > AllowedPhaseError;
			}
		}
		const bool bInvalid = Record.bProxyMontageBeforeTraversal || Record.bProxyMontagePhaseMismatch || Record.bProxyActiveMontageMissing || Record.bProxyTraversalOnsetDelayed;
		if (bInvalid && !bWasInvalid)
		{
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal proxy presentation mismatch pawn=%s active=%d phase=%d npTime=%.4f visibleTime=%.4f error=%.4f early=%d wrongPhase=%d missing=%d onsetDelayed=%d onsetGap=%.4f expected=%s visible=%s"),
				*GetPathNameSafe(Character), bActive, Presented ? static_cast<int32>(Presented->Command.Phase) : INDEX_NONE,
				Presented ? Presented->MontagePosition : -1.0f, Instance ? Instance->GetPosition() : -1.0f, PhaseError,
				Record.bProxyMontageBeforeTraversal, Record.bProxyMontagePhaseMismatch, Record.bProxyActiveMontageMissing,
				Record.bProxyTraversalOnsetDelayed, Record.MaximumProxyOnsetGapSeconds,
				*GetPathNameSafe(ExpectedMontage), *GetPathNameSafe(VisibleMontage));
		}
	}
	/** Check the measured floor under the finalized capsule, independently of ordinary motion after handoff. */
	bool SupportedBeyondHurdle(APawn* Character, const FVector& Position, UPrimitiveComponent* ExpectedSupport) const
	{
		using namespace RpgGaspMoverTraversalTests;
		if (!Character || !ExpectedSupport || !ExpectedSupport->IsQueryCollisionEnabled() || !Capsule(Character)) return false;
		if (Position.X - Capsule(Character)->GetScaledCapsuleRadius() <= Bounds.Max.X
			|| FMath::Abs(Position.Z - Capsule(Character)->GetScaledCapsuleHalfHeight() - FloorZ) >= 8.0) return false;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMoverHurdleFixtureFloor), false, Character);
		TArray<AActor*> Attached;
		Character->GetAttachedActors(Attached, true, true); Params.AddIgnoredActors(Attached);
		FHitResult Floor;
		return Character->GetWorld()->LineTraceSingleByChannel(Floor, Position,
			Position - FVector(0, 0, Capsule(Character)->GetScaledCapsuleHalfHeight() + 30.0), ECC_Visibility, Params)
			&& Floor.IsValidBlockingHit() && Floor.GetComponent() == ExpectedSupport && Floor.ImpactNormal.Z >= 0.7
			&& FMath::Abs(Floor.ImpactPoint.Z - FloorZ) < 5.0 && Floor.GetActor() && !Floor.GetActor()->ActorHasTag(HurdleTag);
	}
	void Observe(FObservation& Record, float DeltaSeconds)
	{
		using namespace RpgGaspMoverTraversalTests;
		APawn* Character = Record.Character.Get();
		if (!Character || !ActiveWorld(Record.World.Get()) || PressedAt < 0.0 || !Mover(Character)) return;
		UAnimInstance* Animation = Mesh(Character)->GetAnimInstance();
		UAnimMontage* Montage = Animation ? Animation->GetCurrentActiveMontage() : nullptr;
		// A held retry may predict just before authority reaches the same grounded state. Keep those
		// rejected attempts in the counters, but observe the confirmed play's montage and handoff separately.
		const bool bObserveConfirmedPlay = Scenario != EScenario::HeldRetry || AuthorityRecord.Commits > 0;
		const bool bPlaying = bObserveConfirmedPlay && Animation && IsTraversal(Character, Montage, Action) && Animation->Montage_IsPlaying(Montage);
		const bool bLease = bObserveConfirmedPlay && Mover(Character)->HasTraversalLease();
		// NP owns collision/warp lifecycle independently of GAS montage replication and its presentation clock.
		// Record the full lease contract even when a remote visible montage has not arrived yet.
		UPrimitiveComponent* ExpectedCollider = Collider(Record.World.Get());
		Record.bLease |= bLease && ExpectedCollider && Mover(Character)->GetTraversalCollider() == ExpectedCollider
			&& Capsule(Character)->GetMoveIgnoreComponents().Contains(ExpectedCollider)
			&& Mover(Character)->FindActiveLayeredMoveByType(FRpgMoverAbilityRootMotion::StaticStruct());
		const FMotionWarpingTarget* CurrentWarpTarget = Warping(Character)->FindWarpTarget(TEXT("FrontLedge"));
		if (CurrentWarpTarget) { Record.bWarpTarget = true; Record.WarpYaw = static_cast<float>(CurrentWarpTarget->Rotator().Yaw); }
		const FMotionWarpingTarget* BackWarpTarget = Warping(Character)->FindWarpTarget(TEXT("BackLedge"));
		const FMoverSyncState& FinalizedSync = Mover(Character)->GetSyncState();
		const FRpgMoverTraversalSyncState* TraversalState = FinalizedSync.SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
		const FMoverDefaultSyncState* FinalizedMovement = FinalizedSync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
		Record.bBackWarpTarget |= BackWarpTarget && TraversalState && TraversalState->Command.IsActive()
			&& TraversalState->Command.Context.BackLedgeWarpTargetName == TEXT("BackLedge")
			&& BackWarpTarget->GetLocation().Equals(TraversalState->Command.Context.BackLedgeTarget.GetLocation(), 1.0);
		const FMotionWarpingTarget* FloorWarpTarget = Warping(Character)->FindWarpTarget(TEXT("BackFloor"));
		if (Action == EAction::Hurdle && TraversalState && TraversalState->Command.IsActive())
		{
			const FRpgMoverTraversalRequest& Context = TraversalState->Command.Context;
			Record.bBackFloorTarget |= FloorWarpTarget && Context.BackFloorWarpTargetName == TEXT("BackFloor")
				&& FloorWarpTarget->GetLocation().Equals(Context.BackFloorTarget.GetLocation(), 1.0);
			Record.BackFloorLocation = Context.BackFloorTarget.GetLocation();
			Record.LandingSupport = Context.LandingSupport;
			Record.bSupportContext |= Context.LandingSupport.IsValid() && Context.LandingSupport != Context.Collider
				&& Context.LandingSupport->GetWorld() == Character->GetWorld()
				&& Context.LandingSupport->GetComponentTransform().Equals(Context.LandingSupportTransform, 0.01);
		}
		const FMotionWarpingTarget* Unrelated = Warping(Character)->FindWarpTarget(UnrelatedTarget);
		Record.bLostUnrelatedTarget |= !Unrelated || !Unrelated->GetLocation().Equals(UnrelatedLocation, 0.01);
		const bool bSimulationActive = bObserveConfirmedPlay && TraversalState && TraversalState->Command.IsActive() && !TraversalState->bEndApplied;
		if (Character->GetLocalRole() == ROLE_SimulatedProxy && bObserveConfirmedPlay)
		{
			ObserveProxyPresentation(Record, Character, Animation, Montage, TraversalState, bPlaying, bSimulationActive,
				FinalizedSync.MovementMode == TEXT("Traversing"), DeltaSeconds);
		}
		if (bSimulationActive && FinalizedMovement)
		{
			Record.bSawActiveSimulation = true;
			Record.ObservedSimulationIdentity = TraversalState->Command.Identity;
			Record.LastLeaseSpeed = static_cast<float>(FinalizedMovement->GetVelocity_WorldSpace().Size2D());
		}
		if (!Record.bCapturedHandoff && Record.bSawActiveSimulation && TraversalState && FinalizedMovement
			&& TraversalState->Command.IsTerminal() && TraversalState->bEndApplied
			&& TraversalState->Command.Identity == Record.ObservedSimulationIdentity)
		{
			// HasTraversalLease follows the live GAS command on owner/authority and can be false
			// before a fixed step consumes that command. Measure the applied terminal simulation
			// state itself, with mode, location and velocity from the same finalized snapshot.
			Record.bCapturedHandoff = true;
			Record.HandoffLocation = FinalizedMovement->GetLocation_WorldSpace();
			Record.HandoffVelocity = FinalizedMovement->GetVelocity_WorldSpace();
			Record.bHandoffGrounded = FinalizedSync.MovementMode == DefaultModeNames::Walking;
			Record.bHandoffFalling = FinalizedSync.MovementMode == DefaultModeNames::Falling;
			Record.HandoffMontageTime = FMath::Max3(Record.LastTime, Record.AbilityEndMontageTime, TraversalState->Command.PresentationEndPosition);
			Record.bHandoffSupportedBeyond = Action == EAction::Hurdle && Record.bHandoffGrounded
				&& SupportedBeyondHurdle(Character, Record.HandoffLocation, Record.LandingSupport.Get());
			Record.bStoppedAtHandoff |= Record.LastLeaseSpeed > 100.0f && Record.HandoffVelocity.Size2D() < 1.0;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal simulation handoff role=%d mode=%s endApplied=%d grounded=%d terminalPhase=%d previousSpeed=%.2f position=%s velocity=%s"),
				static_cast<int32>(Character->GetLocalRole()), *FinalizedSync.MovementMode.ToString(), TraversalState->bEndApplied, Record.bHandoffGrounded, static_cast<int32>(TraversalState->Command.Phase), Record.LastLeaseSpeed,
				*Record.HandoffLocation.ToCompactString(), *Record.HandoffVelocity.ToCompactString());
			ReportConnections(TEXT("handoff"), Record.World.Get());
		}
		Record.bOrdinaryJump |= Record.Commits == 0 && !Record.Montage.IsValid() && !bLease && Mover(Character)->IsFalling();
		Record.bGroundedAfterJump |= Record.bOrdinaryJump && !Record.Montage.IsValid() && Mover(Character)->IsOnGround();
		if (const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Character); Health && Health->IsDeadOrDying())
		{
			if (!Record.bObservedDeath) { Record.DeathLocation = Character->GetActorLocation(); Record.bObservedDeath = true; }
			Record.DeadSeconds += DeltaSeconds;
			const FMoverSyncState& Sync = Mover(Character)->GetSyncState();
			const FMoverDefaultSyncState* Move = Sync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const bool bTerminal = IsTerminalDeath(Sync);
			const bool bProxy = Character->GetLocalRole() == ROLE_SimulatedProxy;
			if (!Record.bTerminalDeath && bTerminal)
			{
				Record.bTerminalDeath = true;
				Record.FirstTerminalDeathLocation = Character->GetActorLocation();
				if (bProxy) Record.DeathLocation = Character->GetActorLocation();
			}
			Record.bInvalidDeath |= bProxy && !Record.bTerminalDeath && Record.DeadSeconds > 0.75f;
			if (Character->GetLocalRole() == ROLE_AutonomousProxy)
			{
				// Matching the stationary server already requires no correction. A differing predicted
				// anchor can only be replaced by the explicit, one-time NP rollback proof below.
				if (!Record.bOwnerDeathConverged && AuthorityRecord.bTerminalDeath && bTerminal && Record.DeadSeconds <= 0.75f
					&& FVector::DistSquared(Record.DeathLocation, AuthorityRecord.DeathLocation) <= 1.0
					&& FVector::DistSquared(Character->GetActorLocation(), AuthorityRecord.DeathLocation) <= 1.0)
				{
					Record.bOwnerDeathConverged = true;
					Record.OwnerDeathConvergenceSeconds = Record.DeadSeconds;
					Record.BeforeDeathDispatch = FMoverSyncState(); Record.bBeforeDeathDispatchValid = false;
				}
				Record.bInvalidDeath |= !Record.bOwnerDeathConverged && Record.DeadSeconds > 0.75f;
			}
			if (bProxy ? Record.bTerminalDeath : Record.DeadSeconds > 0.05f)
			{
				Record.bInvalidDeath |= !bTerminal;
				RecordDeathDrift(Record, Character->GetActorLocation(), Move ? Move->GetLocation_WorldSpace() : FVector::ZeroVector);
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
				for (const FName Name : {FName(TEXT("FrontLedge")), FName(TEXT("BackLedge"))})
				{
					TArray<FMotionWarpingWindowData> Windows;
					UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, Name, Windows);
					if (Name == TEXT("BackLedge")) Record.bNeedsBackWarp = !Windows.IsEmpty();
					for (const FMotionWarpingWindowData& Window : Windows)
					{
						Record.LastWarpEnd = FMath::Max(Record.LastWarpEnd, Window.EndTime);
						if (Name == TEXT("FrontLedge")) Record.LastFrontWarpEnd = FMath::Max(Record.LastFrontWarpEnd, Window.EndTime);
					}
				}
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
				if (Position > (Action == EAction::Hurdle ? Record.LastFrontWarpEnd : Record.LastWarpEnd) + KINDA_SMALL_NUMBER)
				{
					++Record.PostWarpSamples; Record.MaximumPostWarpError = FMath::Max(Record.MaximumPostWarpError, Error);
				}
			}
		}
		if (Character->GetLocalRole() == ROLE_SimulatedProxy && bObserveConfirmedPlay && bLease && Record.Montage.IsValid())
		{
			// The finalized NP state supplies capsule pose and visible traversal phase on the same clock.
			// ObserveProxyPresentation also checks the actual montage; location checks alone missed an
			// independently replicated GAS montage finishing while this displayed traversal was still active.
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
				if (Traversal->MontagePosition > (Action == EAction::Hurdle ? Record.LastFrontWarpEnd : Record.LastWarpEnd) + KINDA_SMALL_NUMBER)
				{
					if (Record.PostWarpSamples == 0)
					{
						UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal proxy postwarp pawn=%s npMontageTime=%.4f gasMontageTime=%.4f gasPlaying=%d warpEnd=%.4f actorYaw=%.3f syncYaw=%.3f targetYaw=%.3f actorLocation=%s syncLocation=%s"),
							*GetPathNameSafe(Character), Traversal->MontagePosition, Record.LastTime, bPlaying, Record.LastWarpEnd,
							ActorYaw, Move->GetOrientation_WorldSpace().Yaw, ExpectedYaw, *Character->GetActorLocation().ToCompactString(),
							*Move->GetLocation_WorldSpace().ToCompactString());
					}
					++Record.PostWarpSamples; Record.MaximumPostWarpError = FMath::Max(Record.MaximumPostWarpError, Error);
				}
			}
		}
		const UGameplayAbility* AnimatingAbility = ASC(Character) ? ASC(Character)->GetAnimatingAbility() : nullptr;
		const bool bClean = Clean(Character) && (Action != EAction::Hurdle
			|| (!bPlaying && (!AnimatingAbility || !AnimatingAbility->IsA<URpgGameplayAbility_Mantle>())));
		if (Scenario == EScenario::LateJoin && &Record == &ProxyRecord && !Record.Montage.IsValid() && bClean)
		{
			const FRpgMoverTraversalSyncState* Remote = Mover(Character)->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
			const FRpgMoverTraversalSyncState* Server = Mover(Authority()) ? Mover(Authority())->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>() : nullptr;
			Record.bJoinedTerminalState |= Remote && Server && Remote->Command.Phase == ERpgMoverTraversalPhase::Finished
				&& Remote->Command.Phase == Server->Command.Phase && Remote->bEndApplied && Server->bEndApplied
				&& Remote->Command.Identity == Server->Command.Identity
				&& Remote->Command.Context.WarpTargetName == Server->Command.Context.WarpTargetName
				&& Remote->Command.Context.BackLedgeWarpTargetName == Server->Command.Context.BackLedgeWarpTargetName
				&& Remote->Command.Context.BackFloorWarpTargetName == Server->Command.Context.BackFloorWarpTargetName
				&& !Remote->Command.Context.LandingSupport.IsValid()
				&& !Remote->Command.Context.Montage && !Remote->Command.Context.Collider.IsValid() && Remote->WarpModifiers.IsEmpty();
		}
		const bool bObservedInterruption = Record.Ends > 0 || (Character->GetLocalRole() == ROLE_SimulatedProxy
			&& (Record.bLease || Record.Montage.IsValid() || (Scenario == EScenario::BlockedExit && AuthorityRecord.Ends > 0)));
		Record.bCleanAfterInterruption |= InterruptedScenario() && bObservedInterruption
			&& bClean && (!Collider(Record.World.Get()) || !Capsule(Character)->GetMoveIgnoreComponents().Contains(Collider(Record.World.Get())));
		const FVector Position = Character->GetActorLocation();
		const double Radius = Capsule(Character)->GetScaledCapsuleRadius();
		const double Feet = Position.Z - Capsule(Character)->GetScaledCapsuleHalfHeight();
		if (Action == EAction::Vault)
		{
			// NP phase, visible montage identity and capsule share the displayed traversal clock.
			const bool bActiveVault = bLease && TraversalState && TraversalState->Command.IsActive()
				&& IsTraversal(Character, TraversalState->Command.Context.Montage, Action);
			Record.bCrossedRear |= bActiveVault && Position.X > Bounds.Max.X;
			Record.bReleasedFalling |= Record.Montage.IsValid() && !bLease && Mover(Character)->IsFalling();
			if ((Record.bReleasedFalling || Record.bJoinedTerminalState) && bClean && Mover(Character)->IsOnGround()
				&& Position.X - Radius > Bounds.Max.X && Feet < DeckBounds.Max.Z - 50.0)
			{
				FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMoverVaultFixtureFloor), false, Character);
				TArray<AActor*> Attached;
				Character->GetAttachedActors(Attached, true, true); Params.AddIgnoredActors(Attached);
				FHitResult Floor;
				if (Record.World->LineTraceSingleByChannel(Floor, Position, Position - FVector(0, 0, Capsule(Character)->GetScaledCapsuleHalfHeight() + 30.0), ECC_Visibility, Params)
					&& Floor.IsValidBlockingHit() && Floor.ImpactNormal.Z >= 0.7 && Floor.GetActor()
					&& !Floor.GetActor()->ActorHasTag(VaultTag) && !Floor.GetActor()->ActorHasTag(VaultApproachTag)
					&& Floor.ImpactPoint.Z < DeckBounds.Max.Z - 50.0)
					Record.bLandedBeyond = true;
			}
		}
		if (Action == EAction::Hurdle)
		{
			Record.bCrossedRear |= bLease && TraversalState && TraversalState->Command.IsActive()
				&& IsTraversal(Character, TraversalState->Command.Context.Montage, Action) && Position.X > Bounds.Max.X;
			// A terminal-only late join has no active support pointer; measure the actual world support there.
			if (Record.bJoinedTerminalState && !Record.LandingSupport.IsValid())
			{
				FCollisionQueryParams Params(SCENE_QUERY_STAT(RpgMoverHurdleLateJoinFloor), false, Character);
				TArray<AActor*> Attached; Character->GetAttachedActors(Attached, true, true); Params.AddIgnoredActors(Attached);
				FHitResult Floor;
				if (Record.World->LineTraceSingleByChannel(Floor, Position,
					Position - FVector(0, 0, Capsule(Character)->GetScaledCapsuleHalfHeight() + 30.0), ECC_Visibility, Params))
					Record.LandingSupport = Floor.GetComponent();
			}
			Record.bLandedBeyond |= (Record.bHandoffSupportedBeyond || Record.bJoinedTerminalState) && bClean
				&& Mover(Character)->IsOnGround() && SupportedBeyondHurdle(Character, Position, Record.LandingSupport.Get());
		}
		Record.bLandedOnObstacle |= (Record.Montage.IsValid() || Record.bJoinedTerminalState) && bClean && Mover(Character)->IsOnGround() && FMath::Abs(Feet - Bounds.Max.Z) < 8.0
			&& Position.X >= Bounds.Min.X + Radius && Position.X <= Bounds.Max.X - Radius
			&& Position.Y >= Bounds.Min.Y + Radius && Position.Y <= Bounds.Max.Y - Radius;
		Record.bRestoredFacing |= (Landed(Record) || (Scenario == EScenario::Cancel && Record.bCleanAfterInterruption))
			&& FMath::Abs(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, static_cast<double>(ApproachYaw))) < 5.0;
		Record.bContinuedMoving |= Landed(Record) && Record.bCapturedHandoff && Mover(Character)->GetVelocity().X > 1.0
			&& Position.X > Record.HandoffLocation.X + 1.0;
	}
	void Tick(UWorld* World, ELevelTick, float DeltaSeconds)
	{
		using namespace RpgGaspMoverTraversalTests;
		if (!bDriving || !ActiveWorld(World)) return;
		if (Scenario == EScenario::LateJoin && bLateJoinRequested && !ProxyRecord.Character.IsValid())
		{
			FindWorlds(); if (Ready(ObserverWorld.Get(), Observer(), Action)) SubscribeProxy();
		}
		if (World == ServerWorld.Get())
		{
			Observe(AuthorityRecord, DeltaSeconds);
			TryReplaceTraversal();
			const FGameplayAbilitySpec* ActiveAbility = Spec(Authority());
			if (!bInterrupted && (Scenario == EScenario::Cancel || Scenario == EScenario::Death || Scenario == EScenario::ColliderLoss || Scenario == EScenario::SupportLoss)
				&& AuthorityRecord.bLease && ProxyRecord.bLease && AuthorityRecord.LastTime > AuthorityRecord.FirstTime + 0.25f
				&& ActiveAbility && ActiveAbility->IsActive() && Mover(Authority())->HasTraversalLease())
			{
				if (Scenario == EScenario::SupportLoss)
				{
					UPrimitiveComponent* Support = AuthorityRecord.LandingSupport.Get();
					if (AuthorityRecord.bBackFloorTarget && Support && Support->GetWorld() == World
						&& Support != Collider(World) && Support->IsQueryCollisionEnabled())
					{
						DisabledSupport = Support; PreviousSupportCollision = Support->GetCollisionEnabled();
						Support->SetCollisionEnabled(ECollisionEnabled::NoCollision); bInterrupted = true;
						UE_LOG(LogTemp, Display, TEXT("RpgMoverHurdle disabled authority support=%s montageTime=%.3f"),
							*Support->GetPathName(), AuthorityRecord.LastTime);
					}
				}
				else
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
		}
		if (World == ObserverWorld.Get())
		{
			Observe(ProxyRecord, DeltaSeconds);
			ObserveReplacement(Observer(), true);
		}
		if (World != InputWorld() || !Owner()) return;
		Observe(OwnerRecord, DeltaSeconds);
		ObserveReplacement(Owner(), false);
		if (AttackReleaseFrames > 0 && --AttackReleaseFrames == 0) Key(EKeys::LeftMouseButton, false);
		TryReplayTraversal();
		if (!bCheckpointLogged && World->GetTimeSeconds() - StartedAt > 12.0) { Report(TEXT("checkpoint")); bCheckpointLogged = true; }
		if (bFinalHeading) MaximumViewError = FMath::Max(MaximumViewError,
			static_cast<float>(FMath::Abs(FMath::FindDeltaAngleDegrees(InputController->GetControlRotation().Yaw, static_cast<double>(ApproachYaw)))));
		if (PressedAt >= 0.0)
		{
			if (Scenario == EScenario::ResumeStandingInput && !bResumedStandingInput && OwnerRecord.bLease
				&& OwnerRecord.LastTime > OwnerRecord.FirstTime + 0.15f && OwnerRecord.Ends == 0)
			{
				Key(EKeys::W, true); bResumedStandingInput = true; bMoveReleased = false;
			}
			if (!Correction.Injected())
			{
				if (Scenario == EScenario::CorrectDuringWarp)
				{
					if (Action == EAction::Vault) Correction.Inject(Owner(), false, OwnerRecord.Montage.Get(), TEXT("BackLedge"));
					else if (Action == EAction::Hurdle) Correction.Inject(Owner(), false, OwnerRecord.Montage.Get(), TEXT("BackFloor"));
					else Correction.Inject(Owner(), false, OwnerRecord.Montage.Get(), TEXT("FrontLedge"));
				}
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
		if (Action == EAction::Hurdle && Waypoint < 4)
		{
			// Use the approved CMC corridor through the copied same-floor map without teleporting across obstacles.
			const FVector Route[] = { FVector(-2000.0, SpawnLocation.Y, Position.Z), FVector(-2000.0, -1600.0, Position.Z),
				FVector(600.0, -1600.0, Position.Z), FVector(600.0, Bounds.GetCenter().Y, Position.Z) };
			const FVector Destination = Route[Waypoint];
			if (FVector::Dist2D(Position, Destination) < 40.0) { ++Waypoint; if (Waypoint < 4) return; }
			else { InputController->SetControlRotation((Destination - Position).Rotation()); return; }
		}
		if (Action == EAction::Vault && Waypoint < 2)
		{
			// Walk around the original Mantle lanes, then climb the authored stairs normally.
			const FVector Destination(-2800.0, Waypoint == 0 ? SpawnLocation.Y : Bounds.GetCenter().Y, Position.Z);
			if (FVector::Dist2D(Position, Destination) < 40.0) { ++Waypoint; if (Waypoint < 2) return; }
			else { InputController->SetControlRotation((Destination - Position).Rotation()); return; }
		}
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
		if (Action == EAction::Vault && FMath::Abs(Position.Z - Capsule(Owner())->GetScaledCapsuleHalfHeight() - DeckBounds.Max.Z) > 8.0) return;
		if (Action == EAction::Hurdle && FMath::Abs(Position.Z - Capsule(Owner())->GetScaledCapsuleHalfHeight() - FloorZ) > 8.0) return;
		if (Gait == EGait::Stand)
		{
			if (Distance > Capsule(Owner())->GetScaledCapsuleRadius() + 5.0 || Speed >= 5.0f) return;
			if (ContactAt < 0.0)
			{
				ContactAt = World->GetTimeSeconds();
				// Standing Hurdle's source window consumes movement input; prove a truly neutral start.
				if (Action == EAction::Hurdle) { Key(EKeys::W, false); bMoveReleased = true; }
			}
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
		using namespace RpgGaspMoverTraversalTests;
		if (PressedAt < 0.0) return false;
		if (ReplacementScenario() && !bReplayStarted) return false;
		if (Scenario == EScenario::Jump)
			return OwnerRecord.bGroundedAfterJump && AuthorityRecord.bGroundedAfterJump && Clean(Owner()) && Clean(Authority());
		if (Scenario == EScenario::Death && (OwnerRecord.DeadSeconds < 0.5f || AuthorityRecord.DeadSeconds < 0.5f || ProxyRecord.DeadSeconds < 0.5f)) return false;
		if (Scenario == EScenario::Death && !bHost && !OwnerRecord.bOwnerDeathConverged && OwnerRecord.DeadSeconds < 0.75f) return false;
		if (Scenario == EScenario::Cancel && (!OwnerRecord.bRestoredFacing || !AuthorityRecord.bRestoredFacing || !ProxyRecord.bRestoredFacing)) return false;
		if (InterruptedScenario())
			return OwnerRecord.Ends > 0 && AuthorityRecord.Ends > 0 && OwnerRecord.bCleanAfterInterruption
				&& AuthorityRecord.bCleanAfterInterruption && ProxyRecord.bCleanAfterInterruption;
		if ((Scenario == EScenario::CorrectDuringWarp || Scenario == EScenario::CorrectAfterWarp) && !Correction.Observed()) return false;
		return Finished(OwnerRecord) && Finished(AuthorityRecord) && Finished(ProxyRecord);
	}
	void Verify()
	{
		using namespace RpgGaspMoverTraversalTests;
		Report(TEXT("completed"));
		ASSERT_THAT(IsTrue(Isolation.Isolated(ServerWorld.Get())));
		ASSERT_THAT(IsTrue(MaximumViewError < 1.0f));
		for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
		{
			ASSERT_THAT(IsFalse(Record->bLostUnrelatedTarget));
		}
		TestRunner->TestFalse(TEXT("Observer cannot start a traversal montage before its displayed NP traversal starts"), ProxyRecord.bProxyMontageBeforeTraversal);
		TestRunner->TestFalse(TEXT("Observer montage phase follows the same finalized NP snapshot as displayed movement"), ProxyRecord.bProxyMontagePhaseMismatch);
		TestRunner->TestFalse(TEXT("Observer keeps its montage through the active presentation interval"), ProxyRecord.bProxyActiveMontageMissing);
		TestRunner->TestFalse(TEXT("Observer traversal mode cannot move through a multi-frame onset gap without its traversal animation state"), ProxyRecord.bProxyTraversalOnsetDelayed);
		TestRunner->TestFalse(TEXT("Observer retains the same montage instance for one traversal play"), ProxyRecord.bMontageRestarted);
		TestRunner->TestTrue(TEXT("Observer branching-window callbacks occur at most once per linear traversal play"),
			ProxyNotifyObserver.IsValid() && !ProxyNotifyObserver->HasDuplicateCallbacks());
		if (Scenario == EScenario::Jump)
		{
			ASSERT_THAT(IsTrue(OwnerRecord.bOrdinaryJump && AuthorityRecord.bOrdinaryJump));
			ASSERT_THAT(IsTrue(OwnerRecord.Commits == 0 && AuthorityRecord.Commits == 0));
			return;
		}
		ASSERT_THAT(IsTrue(Gait == EGait::Stand ? PressSpeed < 5.0f : Gait == EGait::Walk ? PressSpeed > 100.0f && PressSpeed < 250.0f : PressSpeed > 250.0f));
		if (Action == EAction::Vault || Action == EAction::Hurdle)
		{
			ASSERT_THAT(IsFalse(OwnerRecord.bCommittedWhileAirborne || AuthorityRecord.bCommittedWhileAirborne));
		}
		if (Scenario == EScenario::BlockedExit)
		{
			ASSERT_THAT(IsTrue(OwnerRecord.Commits == 1 && AuthorityRecord.Commits == 0));
			ASSERT_THAT(IsTrue(OwnerRecord.bCancelled && AuthorityRecord.bCancelled));
			ASSERT_THAT(IsFalse(Landed(OwnerRecord) || Landed(AuthorityRecord)));
			if (Action != EAction::Mantle) ASSERT_THAT(IsFalse(OwnerRecord.bCrossedRear || AuthorityRecord.bCrossedRear));
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
			if (Scenario == EScenario::SupportLoss)
			{
				ASSERT_THAT(IsTrue(DisabledSupport.IsValid() && !DisabledSupport->IsQueryCollisionEnabled()));
				ASSERT_THAT(IsTrue(AuthorityRecord.bSupportContext && AuthorityRecord.bBackFloorTarget));
				ASSERT_THAT(IsFalse(AuthorityRecord.bHandoffSupportedBeyond || AuthorityRecord.bLandedBeyond));
			}
			if (Scenario == EScenario::Death)
			{
				if (!bHost) ASSERT_THAT(IsTrue(OwnerRecord.bOwnerDeathConverged && OwnerRecord.OwnerDeathAnchorAdjustments <= 1));
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
		if (!ProxyRecord.bJoinedTerminalState)
		{
			TestRunner->TestTrue(TEXT("Observer phase was compared against a real visible montage"), ProxyRecord.ProxyMontagePhaseSamples > 0);
		}
		if (ReplacementScenario())
		{
			TestRunner->TestTrue(TEXT("A second traversal was activated through ordinary movement and Space input"), bReplayStarted);
			TestRunner->TestTrue(TEXT("The replay keeps the configured montage asset on owner and observer"),
				OwnerRecord.Montage == RepeatedTraversalMontage && ProxyRecord.Montage == RepeatedTraversalMontage);
			TestRunner->TestTrue(TEXT("A same-asset replay has a new traversal identity and animation instance"),
				!(AuthorityRecord.ObservedSimulationIdentity == FirstTraversalIdentity) && OwnerRecord.InstanceId != FirstTraversalInstanceId
				&& ProxyRecord.InstanceId != FirstProxyInstanceId && ProxyRecord.ObservedSimulationIdentity == AuthorityRecord.ObservedSimulationIdentity);
		}
		if ((Action == EAction::Vault || Action == EAction::Hurdle) && bHost && Gait == EGait::Run && ApproachYaw == 0.0f && Scenario == EScenario::Success)
		{
			TestRunner->TestTrue(TEXT("Listen-host observer was sampled during the delayed approach before traversal"), ProxyRecord.ProxyPreTraversalSamples > 0);
			TestRunner->TestTrue(TEXT("Listen-host running traversal compares successive displayed montage phases"), ProxyRecord.ProxyMontagePhaseSamples > 1);
			if (Action == EAction::Vault)
			{
				// The audited Relaxed Hurdle clips use queued notify states, which do not emit
				// these branching delegates. Keep the existing Vault callback coverage.
				TestRunner->TestTrue(TEXT("Listen-host running traversal exercised actual source branching callbacks"),
					ProxyNotifyObserver.IsValid() && ProxyNotifyObserver->ObservedBegins() > 0);
			}
		}
		for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
		{
			if (Action == EAction::Hurdle)
			{
				const FRpgMoverTraversalSyncState* Remote = Mover(Record->Character.Get())->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
				const FRpgMoverTraversalSyncState* Server = Mover(Authority())->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
				// Geometry alone cannot distinguish a successful natural end from a cancelled zero-speed exit.
				// A peer that first sees the terminal snapshot still has to match the authority's exact play.
				TestRunner->TestTrue(FString::Printf(TEXT("Successful Hurdle peer %s applies Finished for the authoritative traversal identity"),
					*GetPathNameSafe(Record->World.Get())), Remote && Server && Remote->bEndApplied && Server->bEndApplied
					&& Remote->Command.Phase == ERpgMoverTraversalPhase::Finished && Server->Command.Phase == ERpgMoverTraversalPhase::Finished
					&& Remote->Command.Identity == Server->Command.Identity
					&& (Record->bJoinedTerminalState || (Record->bSawActiveSimulation && Record->bCapturedHandoff
						&& Remote->Command.Identity == Record->ObservedSimulationIdentity)));
			}
			if (Scenario == EScenario::LateJoin && Record == &ProxyRecord && Record->bJoinedTerminalState)
			{
				ASSERT_THAT(IsTrue(Landed(*Record) && Record->bRestoredFacing));
				ASSERT_THAT(IsTrue(Clean(Record->Character.Get())));
				continue;
			}
			ASSERT_THAT(IsTrue(Record->Montage.IsValid() && Record->bLease && Record->bWarpTarget));
			ASSERT_THAT(IsFalse(Record->bMontageRestarted));
			if (Action != EAction::Mantle && Scenario == EScenario::LateJoin && Record == &ProxyRecord)
			{
				// A peer can receive the final few milliseconds of a short crossing clip. Require real
				// visible advancement and the same authoritative active->applied-terminal identity,
				// rather than inventing another 100 ms of animation after the source handoff.
				ASSERT_THAT(IsTrue(Record->LastTime > Record->FirstTime + KINDA_SMALL_NUMBER));
				const FRpgMoverTraversalSyncState* Remote = Mover(Record->Character.Get())->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
				const FRpgMoverTraversalSyncState* Server = Mover(Authority())->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverTraversalSyncState>();
				ASSERT_THAT(IsTrue(Record->bSawActiveSimulation && Record->bCapturedHandoff && Remote && Server
					&& Remote->bEndApplied && Server->bEndApplied && Remote->Command.Phase == ERpgMoverTraversalPhase::Finished
					&& Server->Command.Phase == Remote->Command.Phase && Remote->Command.Identity == Record->ObservedSimulationIdentity
					&& Server->Command.Identity == Remote->Command.Identity && Clean(Record->Character.Get())));
			}
			else ASSERT_THAT(IsTrue(Record->LastTime > Record->FirstTime + 0.1f));
			ASSERT_THAT(IsTrue(Landed(*Record) && Record->bRestoredFacing));
			if (Action == EAction::Vault)
			{
				ASSERT_THAT(IsTrue(Record->bCrossedRear && Record->bReleasedFalling && Record->bHandoffFalling));
				ASSERT_THAT(IsTrue(Record->HandoffVelocity.X > 1.0));
				ASSERT_THAT(IsFalse(Record->bLandedOnObstacle));
				if (Gait == EGait::Stand) ASSERT_THAT(IsTrue(Record->bBackWarpTarget));
			}
			if (Action == EAction::Hurdle)
			{
				ASSERT_THAT(IsTrue(Record->bCrossedRear && Record->bHandoffGrounded && Record->bHandoffSupportedBeyond));
				ASSERT_THAT(IsTrue(Record->bBackFloorTarget && Record->bSupportContext));
				ASSERT_THAT(IsTrue(Record->bBackWarpTarget == Record->bNeedsBackWarp));
				const double VisualBaseAboveFeet = Mover(Record->Character.Get())->GetBaseVisualComponentTransform().GetLocation().Z
					+ Capsule(Record->Character.Get())->GetScaledCapsuleHalfHeight();
				ASSERT_THAT(IsTrue(Record->BackFloorLocation.X > Bounds.Max.X
					&& FMath::Abs(Record->BackFloorLocation.Z - VisualBaseAboveFeet - FloorZ) < 5.0));
				ASSERT_THAT(IsFalse(Record->bHandoffFalling || Record->bLandedOnObstacle));
				// The standing Relaxed clip can reach its input handoff with zero source velocity
				// (observed owner: 0 -> 0). Held input must resume ordinary Walking below;
				// only a walking/running entry is required to inherit positive forward momentum.
				if (HoldMovement() && !(Gait == EGait::Stand && Scenario == EScenario::ResumeStandingInput))
					ASSERT_THAT(IsTrue(Record->HandoffVelocity.X > 1.0));
			}
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
			const FRpgTraversalAnimationEntry* Row = AnimationEntries(Record->Character.Get(), Action).FindByPredicate(
				[Record](const FRpgTraversalAnimationEntry& Entry) { return Entry.Montage == Record->Montage.Get(); });
			ASSERT_THAT(IsTrue(Row && !Row->bAirborne));
			if (Action == EAction::Hurdle)
			{
				ASSERT_THAT(IsTrue(Row && (LaneIndex == 0 ? Row->MaxDepth <= 25.0f : Row->MinDepth >= 25.0f)));
				if (Record != &ProxyRecord && Scenario == EScenario::NaturalEnd)
				{
					ASSERT_THAT(IsTrue(Row && Row->MovementInputHandoffTime > 0.0f && Row->HandoffTime > Row->MovementInputHandoffTime));
					ASSERT_THAT(IsTrue(Row && Record->HandoffMontageTime >= Row->HandoffTime - 0.05f));
				}
				if (Record != &ProxyRecord && Scenario == EScenario::ResumeStandingInput)
				{
					ASSERT_THAT(IsTrue(bResumedStandingInput && Row && Row->MovementInputHandoffTime > 0.0f));
					ASSERT_THAT(IsTrue(Row && Record->HandoffMontageTime >= Row->MovementInputHandoffTime - 0.05f
						&& Record->HandoffMontageTime < Row->HandoffTime - 0.1f));
				}
			}
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
		Box->InitBoxExtent(Action == EAction::Hurdle ? FVector(180, 180, 100)
			: Action == EAction::Vault ? FVector(45, 180, 100) : FVector(65, 190, 100));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetWorldLocation(Action == EAction::Hurdle ? FVector(Bounds.Max.X + 190.0, Bounds.GetCenter().Y, FloorZ + 100.0)
			: Action == EAction::Vault
			? FVector(Bounds.Max.X + 75.0, Bounds.GetCenter().Y, DeckBounds.Max.Z + 50.0)
			: FVector(Bounds.Min.X + 80.0, Bounds.GetCenter().Y, Bounds.Max.Z + 100.0));
		Box->SetMobility(EComponentMobility::Static); Box->RegisterComponent(); Blocker = Actor;
	}
	void Report(const TCHAR* Phase) const
	{
		for (UWorld* World : { ServerWorld.Get(), ClientWorld.Get(), ObserverWorld.Get() })
			RpgGaspMoverTraversalTests::ReportConnections(Phase, World);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal phase=%s action=%d scenario=%d gait=%d host=%d position=%s bounds=%s..%s pressSpeed=%.2f pressYaw=%.2f controlError=%.2f"),
			Phase, static_cast<int32>(Action), static_cast<int32>(Scenario), static_cast<int32>(Gait), bHost, Owner() ? *Owner()->GetActorLocation().ToCompactString() : TEXT("none"),
			*Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString(), PressSpeed, PressYaw, MaximumViewError);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal proxy presentation phase=%s samples=%d beforeActive=%d maxError=%.4f early=%d wrongPhase=%d missing=%d onsetDelayed=%d onsetGap=%.4f"),
			Phase, ProxyRecord.ProxyMontagePhaseSamples, ProxyRecord.ProxyPreTraversalSamples, ProxyRecord.MaximumProxyMontagePhaseError,
			ProxyRecord.bProxyMontageBeforeTraversal, ProxyRecord.bProxyMontagePhaseMismatch, ProxyRecord.bProxyActiveMontageMissing,
			ProxyRecord.bProxyTraversalOnsetDelayed, ProxyRecord.MaximumProxyOnsetGapSeconds);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal proxy notifies phase=%s begins=%d duplicated=%d"), Phase,
			ProxyNotifyObserver.IsValid() ? ProxyNotifyObserver->ObservedBegins() : 0,
			ProxyNotifyObserver.IsValid() && ProxyNotifyObserver->HasDuplicateCallbacks());
		if (ReplacementScenario())
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal replacement phase=%s requested=%d replay=%d proxyHadTraversal=%d overlap=%d oldTookOver=%d restarted=%d ownerInstance=%d ownerTime=%.3f..%.3f proxyInstance=%d proxyTime=%.3f..%.3f"),
				Phase, bReplacementTriggered, bReplayStarted, bFirstProxyHadTraversal, bReplacementOverlappedPresentedTraversal,
				bTraversalReplacedTheAttack, bReplacementRestarted, ReplacementOwnerInstance, ReplacementOwnerFirst, ReplacementOwnerLast,
				ReplacementProxyInstance, ReplacementProxyFirst, ReplacementProxyLast);
		for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
			UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal peer=%s commits=%d ends=%d cancelled=%d successfulEnds=%d cancelledEnds=%d airborneCommit=%d montage=%s time=%.3f..%.3f warpEnd=%.3f lease=%d target=%d restart=%d landed=%d moving=%d facing=%d lateError=%.2f postSamples=%d postError=%.2f jump=%d groundedRetry=%d cleanInterrupted=%d death=%d"),
				*GetPathNameSafe(Record->World.Get()), Record->Commits, Record->Ends, Record->bCancelled,
				Record->SuccessfulEnds, Record->CancelledEnds, Record->bCommittedWhileAirborne, *GetPathNameSafe(Record->Montage.Get()),
				Record->FirstTime, Record->LastTime, Record->LastWarpEnd, Record->bLease, Record->bWarpTarget, Record->bMontageRestarted,
				Record->bLandedOnObstacle, Record->bContinuedMoving, Record->bRestoredFacing, Record->BestAlignmentError, Record->PostWarpSamples,
				Record->MaximumPostWarpError, Record->bOrdinaryJump, Record->bCommittedAfterGroundedRetry, Record->bCleanAfterInterruption, Record->bObservedDeath);
		if (Action == EAction::Vault)
			for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
				UE_LOG(LogTemp, Display, TEXT("RpgMoverVault peer=%s rearTarget=%d crossed=%d releasedFalling=%d landedBeyond=%d handoffFalling=%d handoffVelocity=%s joinedTerminal=%d"),
					*GetPathNameSafe(Record->World.Get()), Record->bBackWarpTarget, Record->bCrossedRear, Record->bReleasedFalling,
					Record->bLandedBeyond, Record->bHandoffFalling, *Record->HandoffVelocity.ToCompactString(), Record->bJoinedTerminalState);
		if (Action == EAction::Hurdle)
			for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
				UE_LOG(LogTemp, Display, TEXT("RpgMoverHurdle peer=%s floorTarget=%d floor=%s support=%s validSupport=%d rearTarget=%d needsRear=%d crossed=%d groundedHandoff=%d supportedHandoff=%d handoffTime=%.3f abilityEndTime=%.3f landed=%d resumedInput=%d"),
					*GetPathNameSafe(Record->World.Get()), Record->bBackFloorTarget, *Record->BackFloorLocation.ToCompactString(),
					*GetPathNameSafe(Record->LandingSupport.Get()), Record->bSupportContext, Record->bBackWarpTarget, Record->bNeedsBackWarp,
					Record->bCrossedRear, Record->bHandoffGrounded, Record->bHandoffSupportedBeyond, Record->HandoffMontageTime, Record->AbilityEndMontageTime,
					Record->bLandedBeyond, bResumedStandingInput);
		if (Scenario == EScenario::Death)
			for (const FObservation* Record : { &OwnerRecord, &AuthorityRecord, &ProxyRecord })
				UE_LOG(LogTemp, Display, TEXT("RpgMoverTraversal death peer=%s anchor=%s firstTerminal=%s maxDrift=%.4f maxActor=%s maxSync=%s maxSeconds=%.4f seconds=%.4f converged=%d convergenceSeconds=%.4f anchorAdjustments=%d rollbacks=%d"),
					*GetPathNameSafe(Record->World.Get()), *Record->DeathLocation.ToCompactString(), *Record->FirstTerminalDeathLocation.ToCompactString(),
					Record->MaximumDeathDrift, *Record->MaximumDeathDriftLocation.ToCompactString(), *Record->MaximumDeathDriftSyncLocation.ToCompactString(),
					Record->MaximumDeathDriftSeconds, Record->DeadSeconds, Record->bOwnerDeathConverged, Record->OwnerDeathConvergenceSeconds,
					Record->OwnerDeathAnchorAdjustments, Record->DeathRollbackObserver.IsValid() ? Record->DeathRollbackObserver->Count : 0);
	}
	void Cleanup()
	{
		if (bDriving) Report(TEXT("teardown"));
		if (Scenario == EScenario::CorrectDuringWarp || Scenario == EScenario::CorrectAfterWarp) Correction.Report(TEXT("teardown"));
		Correction.Stop();
		FWorldDelegates::OnWorldTickEnd.Remove(TickHandle); TickHandle.Reset();
		FWorldDelegates::OnWorldTickStart.Remove(BeforeDeathDispatchHandle); BeforeDeathDispatchHandle.Reset();
		FWorldDelegates::OnWorldPreActorTick.Remove(AfterDeathDispatchHandle); AfterDeathDispatchHandle.Reset();
		if (ProxyNotifyAnimation.IsValid() && ProxyNotifyObserver.IsValid())
		{
			ProxyNotifyAnimation->OnPlayMontageNotifyBegin.RemoveDynamic(ProxyNotifyObserver.Get(), &URpgMoverTraversalNotifyTestObserver::ObserveBegin);
			ProxyNotifyAnimation->OnPlayMontageNotifyEnd.RemoveDynamic(ProxyNotifyObserver.Get(), &URpgMoverTraversalNotifyTestObserver::ObserveEnd);
		}
		ProxyNotifyAnimation.Reset(); ProxyNotifyObserver.Reset();
		for (FObservation* Record : { &OwnerRecord, &AuthorityRecord })
		{
			if (Record->AbilitySystem.IsValid())
			{
				Record->AbilitySystem->AbilityCommittedCallbacks.Remove(Record->Committed);
				Record->AbilitySystem->OnAbilityEnded.Remove(Record->Ended);
			}
			if (Record->Character.IsValid() && RpgGaspMoverTraversalTests::Mover(Record->Character.Get()) && Record->DeathRollbackObserver.IsValid())
				RpgGaspMoverTraversalTests::Mover(Record->Character.Get())->OnPostSimulationRollback.RemoveDynamic(Record->DeathRollbackObserver.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
			Record->DeathRollbackObserver.Reset(); Record->BeforeDeathDispatch = FMoverSyncState(); Record->bBeforeDeathDispatchValid = false;
		}
		Key(EKeys::W, false); Key(EKeys::SpaceBar, false); Key(EKeys::LeftControl, false); Key(EKeys::LeftMouseButton, false);
		if (InputController.IsValid()) InputController->SetIgnoreLookInput(false);
		InputController.Reset(); bDriving = false;
		if (DisabledCollider.IsValid()) DisabledCollider->SetCollisionEnabled(PreviousCollision);
		if (DisabledSupport.IsValid()) DisabledSupport->SetCollisionEnabled(PreviousSupportCollision);
		DisabledSupport.Reset();
		if (Blocker.IsValid()) Blocker->Destroy();
		if (bOwnsSession && GUnrealEd) { GUnrealEd->EndPlayMap(); bOwnsSession = false; }
		if (bConfigured) { GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience; bConfigured = false; }
		Settings.Reset();
	}
};


FRpgGaspMoverTraversalTestFixture::FRpgGaspMoverTraversalTestFixture(FAutomationTestBase* Runner,
	FNoDiscardAsserter& Assert, FTestCommandBuilder& Builder)
	: State(MakeUnique<FState>(Runner, Assert, Builder)) {}
FRpgGaspMoverTraversalTestFixture::~FRpgGaspMoverTraversalTestFixture() { State->Cleanup(); }
void FRpgGaspMoverTraversalTestFixture::Initialize() { State->Initialize(); }
void FRpgGaspMoverTraversalTestFixture::Cleanup() { State->Cleanup(); }
void FRpgGaspMoverTraversalTestFixture::Queue(EGait Gait, EScenario Scenario, bool bHost, float Yaw, EAction Action, int32 Lane)
{
	State->Action = Action;
	State->LaneIndex = Lane;
	State->Queue(Gait, Scenario, bHost, Yaw);
}

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
