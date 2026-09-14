// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgMoverPredictionTestHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Backends/MoverNetworkPredictionLiaison.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputLibrary.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "InputKeyEventArgs.h"
#include "InputAction.h"
#include "Misc/Guid.h"
#include "NetworkPredictionWorldManager.h"
#include "Retargeter/IKRetargeter.h"
#include "UObject/StrongObjectPtr.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_BasicWeaponAttack.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_Block.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetComponent.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetProfile.h"
#include "SurvivalRpg/Core/Character/RpgMoverPawn.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspMoverGameplayTests
{
	constexpr TCHAR PawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/DA_PawnData_GaspMover.DA_PawnData_GaspMover");
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMover/BP_Rpg_GaspMoverTestGameMode.BP_Rpg_GaspMoverTestGameMode_C");
	constexpr TCHAR SourceMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	constexpr TCHAR SwordPath[] = TEXT("/GF_Combat_Core/Equipment/Weapons/ED_BasicSword.ED_BasicSword_C");
	constexpr TCHAR TargetMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UE5_Mannequins/Meshes/SKM_Manny.SKM_Manny");
	constexpr TCHAR RetargeterPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UE5_Mannequins/Rigs/RTG_UEFN_to_UE5_Mannequin.RTG_UEFN_to_UE5_Mannequin");
	FPrimaryAssetId ExperienceId() { return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMoverExperience")); }
	FTimespan Timeout() { return FTimespan::FromSeconds(45.0); }
	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(Context.World()) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		return false;
	}
	/** Isolation precedes InitGame's persistence work and the initial host pawn's simulation. */
	class FScopedWorld final
	{
	public:
		~FScopedWorld() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_MoverGameplayAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedWorld::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!ActiveWorld(World)) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* Mode = World->GetAuthGameMode<ARpgGameModeBase>();
			return Mode && !Mode->bEnableDiskPersistence && Mode->WorldSaveSlotName.StartsWith(Prefix)
				&& Mode->WorldSaveBackupSlotName == Mode->WorldSaveSlotName + TEXT("_Backup")
				&& Mode->WorldSaveRecoverySlotName == Mode->WorldSaveSlotName + TEXT("_Recovery") && Mode->OfflineProfileKey == Prefix;
		}
	private:
		void OnInitialized(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* Mode = Cast<ARpgGameModeBase>(Initialized);
			if (!Mode || !Mode->GetWorld() || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
			Mode->bEnableDiskPersistence = false;
			Mode->WorldSaveSlotName = Prefix;
			Mode->WorldSaveBackupSlotName = Prefix + TEXT("_Backup");
			Mode->WorldSaveRecoverySlotName = Prefix + TEXT("_Recovery");
			Mode->OfflineProfileKey = Prefix;
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Mode->GetWorld()->SpawnActor<ARpgCombatNetworkFloorFixture>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
			for (int32 Index = 0; Index < 3; ++Index)
				Mode->GetWorld()->SpawnActor<APlayerStart>(FVector(0.0, (Index - 1) * 500.0, 120.0), FRotator::ZeroRotator, Spawn);
		}
		FString Prefix;
		FDelegateHandle Handle;
	};
	APawn* LocalPawn(UWorld* World)
	{
		const APlayerController* PC = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}
	APawn* FindPawn(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		if (!State || PlayerId == INDEX_NONE) return nullptr;
		for (APlayerState* Player : State->PlayerArray)
			if (Player && Player->GetPlayerId() == PlayerId) return Player->GetPawn();
		return nullptr;
	}
	USkeletalMeshComponent* Mesh(const APawn* Pawn) { return URpgPawnExtensionComponent::FindGameplayMesh(Pawn); }
	URpgEquipmentManagerComponent* Equipment(const APawn* Pawn) { return Pawn ? Pawn->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr; }
	URpgRuntimeRetargetComponent* Retarget(const APawn* Pawn) { return Pawn ? Pawn->FindComponentByClass<URpgRuntimeRetargetComponent>() : nullptr; }
	UCharacterMoverComponent* Mover(const APawn* Pawn) { return Pawn ? Pawn->FindComponentByClass<UCharacterMoverComponent>() : nullptr; }
	bool HasAttackRootMotion(const APawn* Pawn)
	{
		return Mover(Pawn) && Mover(Pawn)->FindActiveLayeredMoveByType(FRpgMoverAbilityRootMotion::StaticStruct()) != nullptr;
	}
	URpgAbilitySystemComponent* ASC(const APawn* Pawn)
	{
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
		return Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr;
	}
	const URpgRuntimeRetargetProfile* DefaultProfile(const APawn* Pawn)
	{
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
		const URpgPawnData* Data = Extension ? Extension->GetPawnData<URpgPawnData>() : nullptr;
		return Data ? Data->RuntimeRetargetProfile.Get() : nullptr;
	}
	URpgWeaponInstance* Weapon(const APawn* Pawn)
	{
		return Equipment(Pawn) ? Cast<URpgWeaponInstance>(Equipment(Pawn)->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand)) : nullptr;
	}
	FGameplayAbilitySpec* AttackSpec(APawn* Pawn)
	{
		URpgAbilitySystemComponent* AbilitySystem = ASC(Pawn);
		URpgWeaponInstance* Item = Weapon(Pawn);
		if (!AbilitySystem || !Item) return nullptr;
		const FGameplayTag Primary = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"));
		for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
			if (!Spec.PendingRemove && Spec.SourceObject.Get() == Item && Spec.GetDynamicSpecSourceTags().HasTagExact(Primary)
				&& Cast<URpgGameplayAbility_BasicWeaponAttack>(Spec.GetPrimaryInstance())) return &Spec;
		return nullptr;
	}
	URpgGameplayAbility_BasicWeaponAttack* Attack(APawn* Pawn)
	{
		const FGameplayAbilitySpec* Spec = AttackSpec(Pawn);
		return Spec ? Cast<URpgGameplayAbility_BasicWeaponAttack>(Spec->GetPrimaryInstance()) : nullptr;
	}
	FGameplayAbilitySpec* BlockSpec(APawn* Pawn)
	{
		if (!ASC(Pawn) || !Equipment(Pawn)) return nullptr;
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Block"));
		for (FGameplayAbilitySpec& Spec : ASC(Pawn)->GetActivatableAbilities())
		{
			const URpgEquipmentInstance* Item = Cast<URpgEquipmentInstance>(Spec.SourceObject.Get());
			if (!Spec.PendingRemove && Item && Spec.GetDynamicSpecSourceTags().HasTagExact(Tag)
				&& Cast<URpgGameplayAbility_Block>(Spec.GetPrimaryInstance()) && Equipment(Pawn)->IsEquipmentInstanceActiveForInputTag(Item, Tag)) return &Spec;
		}
		return nullptr;
	}
	bool Ready(UWorld* World, APawn* Pawn)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = State ? State->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
		const URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(Pawn);
		const ARpgPlayerState* Player = Pawn ? Pawn->GetPlayerState<ARpgPlayerState>() : nullptr;
		const URpgAbilitySystemComponent* AbilitySystem = ASC(Pawn);
		const USkeletalMeshComponent* Source = Mesh(Pawn);
		static const FGameplayTag GameplayReady = FGameplayTag::RequestGameplayTag(TEXT("InitState.GameplayReady"));
		return Experience && Experience->IsExperienceLoaded() && Experience->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Pawn && Pawn->IsA<ARpgMoverPawn>() && Player && Extension && Gameplay && AbilitySystem && Source && Mover(Pawn) && Equipment(Pawn) && Retarget(Pawn)
			&& Extension->HasReachedInitState(GameplayReady) && Gameplay->HasReachedInitState(GameplayReady)
			&& Extension->GetPawnData<URpgPawnData>() && Extension->GetPawnData<URpgPawnData>()->GetPathName() == PawnDataPath
			&& Extension->GetPawnData<URpgPawnData>() == Player->GetPawnData<URpgPawnData>()
			&& AbilitySystem == Player->GetRpgAbilitySystemComponent() && AbilitySystem->GetOwnerActor() == Player && AbilitySystem->GetAvatarActor() == Pawn
			&& AbilitySystem->AbilityActorInfo.IsValid() && AbilitySystem->AbilityActorInfo->SkeletalMeshComponent.Get() == Source
			&& Source == Mover(Pawn)->GetPrimaryVisualComponent() && Source->GetAnimInstance()
			&& Source->GetSkeletalMeshAsset() && Source->GetSkeletalMeshAsset()->GetPathName() == SourceMeshPath
			&& (!Pawn->IsLocallyControlled() || Gameplay->IsReadyToBindInputs());
	}
	bool EquipmentOnSource(APawn* Pawn, const URpgEquipmentDefinition* Definition)
	{
		const URpgWeaponInstance* Item = Weapon(Pawn);
		const USkeletalMeshComponent* Source = Mesh(Pawn);
		if (!Item || !Source || !Definition || Item->GetPawn() != Pawn || Item->GetEquippedSlot() != ERpgEquipmentSlot::MainHand) return false;
		const TArray<AActor*> Actors = Item->GetSpawnedActors();
		if (Actors.IsEmpty() || Actors.Num() != Definition->ActorsToSpawn.Num()) return false;
		for (int32 Index = 0; Index < Actors.Num(); ++Index)
		{
			const USceneComponent* Root = Actors[Index] ? Actors[Index]->GetRootComponent() : nullptr;
			const FName Socket = Definition->ActorsToSpawn[Index].GetAttachSocketForSlot(ERpgEquipmentSlot::MainHand);
			if (!Root || Root->GetAttachParent() != Source || Root->GetAttachSocketName() != Socket || !Source->DoesSocketExist(Socket)) return false;
		}
		return true;
	}
	bool VisibleTarget(APawn* Pawn)
	{
		const URpgRuntimeRetargetComponent* Component = Retarget(Pawn);
		const USkeletalMeshComponent* Target = Component ? Component->GetRetargetMesh() : nullptr;
		return Target && Target->IsRegistered() && Target->IsVisible() && Target->GetAnimInstance() && !Target->bHiddenInGame;
	}
	TArray<FQuat> LimbPose(USkeletalMeshComponent* Visual)
	{
		TArray<FQuat> Result;
		if (!Visual) return Result;
		const TArray<FTransform> Pose = Visual->GetBoneSpaceTransforms();
		for (const FName Bone : { FName(TEXT("thigh_l")), FName(TEXT("calf_r")), FName(TEXT("upperarm_l")), FName(TEXT("lowerarm_r")) })
		{
			const int32 Index = Visual->GetBoneIndex(Bone);
			if (!Pose.IsValidIndex(Index)) return {};
			Result.Add(Pose[Index].GetRotation());
		}
		return Result;
	}
	/** Real key events preserve the source Blueprint input producer and the RPG ability-input bindings. */
	class FScopedInput final
	{
	public:
		~FScopedInput() { Stop(); }
		void Start(APawn* Pawn)
		{
			Stop();
			Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
			if (!Controller.IsValid()) return;
			Controller->SetIgnoreLookInput(true);
			TickHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedInput::Tick);
		}
		void Move(bool bMove) { ForwardAxis = bMove ? 1.0f : 0.0f; }
		void PressAttack() { Key(true); ReleaseFrames = 2; }
		void HoldBlock(bool bHold)
		{
			if (Controller.IsValid()) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton,
				bHold ? IE_Pressed : IE_Released, bHold ? 1.0f : 0.0f));
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(TickHandle);
			TickHandle.Reset();
			if (Controller.IsValid())
			{
				Axis(0.0f);
				Key(false);
				HoldBlock(false);
				Controller->SetIgnoreLookInput(false);
			}
			Controller.Reset();
			ForwardAxis = 0.0f;
			ReleaseFrames = 0;
		}
	private:
		void Key(bool bPressed)
		{
			if (Controller.IsValid()) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton,
				bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
		}
		void Axis(float Amount)
		{
			if (Controller.IsValid()) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, Amount, 1));
		}
		void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			if (!Controller.IsValid() || Controller->GetWorld() != World) return;
			Axis(ForwardAxis);
			if (ReleaseFrames > 0 && --ReleaseFrames == 0) Key(false);
		}
		TWeakObjectPtr<APlayerController> Controller;
		FDelegateHandle TickHandle;
		float ForwardAxis = 0.0f;
		int32 ReleaseFrames = 0;
	};
	/** Delays only this test's network traffic; the actual reconciliation trigger is a prediction error below. */
	class FScopedLatency final
	{
	public:
		~FScopedLatency() { Stop(); }
		bool Start()
		{
#if DO_ENABLE_NET_TEST
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.WorldType == EWorldType::PIE ? Context.World() : nullptr;
				UNetDriver* Driver = ActiveWorld(World) ? World->GetNetDriver() : nullptr;
				if (!Driver) continue;
				Original.Emplace(Driver, Driver->PacketSimulationSettings);
				FPacketSimulationSettings Settings;
				Settings.PktLag = 150;
				Driver->SetPacketSimulationSettings(Settings);
			}
			return Original.Num() == 3;
#else
			return false;
#endif
		}
		void Stop()
		{
#if DO_ENABLE_NET_TEST
			for (const auto& Entry : Original)
				if (Entry.Key.IsValid()) Entry.Key->SetPacketSimulationSettings(Entry.Value);
			Original.Reset();
#endif
		}
	private:
#if DO_ENABLE_NET_TEST
		TArray<TPair<TWeakObjectPtr<UNetDriver>, FPacketSimulationSettings>> Original;
#endif
	};
	/** Observes one real NP restore/resimulation, before any subsequent forward simulation can repair it. */
	class FScopedPredictionCorrection final
	{
		struct FAttackIdentity
		{
			FGameplayAbilitySpecHandle Handle;
			int16 PredictionKey = 0;
			bool bServerInitiated = false;
			uint32 Sequence = 0;
			void Set(const FRpgMoverAbilityRootMotion& Move)
			{
				Handle = Move.AbilityHandle;
				PredictionKey = Move.ActivationPredictionKey;
				bServerInitiated = Move.bServerInitiatedKey;
				Sequence = Move.MontageSequence;
			}
			bool Matches(const FRpgMoverAbilityRootMotion* Move) const
			{
				return Move && Handle.IsValid() && Move->AbilityHandle == Handle && Move->ActivationPredictionKey == PredictionKey
					&& Move->bServerInitiatedKey == bServerInitiated && Move->MontageSequence == Sequence;
			}
		};
	public:
		~FScopedPredictionCorrection() { Stop(); }
		bool CaptureOriginal(APawn* Owner, UAnimMontage* InMontage)
		{
			Pawn = Owner;
			Montage = InMontage;
			Liaison = Owner ? Owner->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>() : nullptr;
			FMoverSyncState State;
			if (!Owner || Owner->GetLocalRole() != ROLE_AutonomousProxy || !Liaison.IsValid() || !Liaison->ReadPendingSyncState(State)) return false;
			const UNetworkPredictionWorldManager* Prediction = Owner->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
			if (!Prediction || Prediction->GetSettings().PreferredTickingPolicy != ENetworkPredictionTickingPolicy::Fixed
				|| !Prediction->GetSettings().bEnableFixedTickSmoothing || Prediction->GetFixedTickState().PendingFrame <= 0) return false;
			FMoverDefaultSyncState* Default = State.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
			const FRpgMoverAbilityRootMotion* Move = State.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>();
			const FAnimMontageInstance* Instance = Mesh(Owner)->GetAnimInstance()->GetActiveInstanceForMontage(InMontage);
			if (!Default || !Move || !Instance) return false;
			OriginalAttack.Set(*Move);
			OriginalInstanceId = Instance->GetInstanceID();
			OriginalFrame = Liaison->GetCurrentSimFrame();
			OriginalLocalFrame = Prediction->GetFixedTickState().PendingFrame;
			return true;
		}
		bool CaptureReplayAndInject()
		{
			FMoverSyncState State;
			if (bInjected) return bReplayCaptured;
			if (!Liaison.IsValid() || !Liaison->ReadPendingSyncState(State) || !Pawn.IsValid()) return false;
			FMoverDefaultSyncState* Default = State.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
			const FRpgMoverAbilityRootMotion* Move = State.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>();
			const FAnimMontageInstance* Instance = Mesh(Pawn.Get())->GetAnimInstance()->GetActiveInstanceForMontage(Montage.Get());
			const auto InjectionClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(Pawn->GetWorld(), Liaison.Get());
			if (!Default || !Move || OriginalAttack.Matches(Move) || !Instance || !Instance->IsPlaying()
				|| Instance->GetInstanceID() == OriginalInstanceId || !InjectionClock.bValid
				|| InjectionClock.LocalPendingFrame <= OriginalLocalFrame) return false;
			ReplayAttack.Set(*Move);
			ReplayInstanceId = Instance->GetInstanceID();
			InjectedFrame = Liaison->GetCurrentSimFrame();
			InjectedTimeMs = Liaison->GetCurrentSimTimeMs();
			InjectedLocalFrame = InjectionClock.LocalPendingFrame;
			CrossDirection = Pawn->GetActorRightVector().GetSafeNormal2D();
			// Fixed ticking can reconcile before Enhanced Input has started B. Introduce the error only
			// once B is already in simulation history, while A's recently cancelled history remains buffered.
			// Preserve all velocity, base, rotation and movement data. Only this owner's predicted location is wrong.
			Default->SetTransforms_WorldSpace(Default->GetLocation_WorldSpace() + CrossDirection * 50.0,
				Default->GetOrientation_WorldSpace(), Default->GetVelocity_WorldSpace(), Default->GetAngularVelocityDegrees_WorldSpace(),
				Default->GetMovementBase(), Default->GetMovementBaseBoneName());
			if (!Liaison->WritePendingSyncState(State)) return false;
			// Kinematic Mover advances its actual UpdatedComponent and captures that transform into the
			// next sync state. Keep both halves of this intentional prediction error consistent, otherwise
			// the next ordinary walking tick erases it before an authoritative packet can reconcile it.
			Mover(Pawn.Get())->GetUpdatedComponent()->SetWorldLocation(Default->GetLocation_WorldSpace(), false, nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogTemp, Display, TEXT("RpgMoverCorrection injection frame=%d time=%.0f state=%s component=%s direction=%s externalMovement=%d"),
				InjectedFrame, InjectedTimeMs, *Default->GetLocation_WorldSpace().ToCompactString(),
				*Mover(Pawn.Get())->GetUpdatedComponent()->GetComponentLocation().ToCompactString(), *CrossDirection.ToCompactString(), Mover(Pawn.Get())->bAcceptExternalMovement);
			bInjected = true;
			bReplayCaptured = true;
			BeforeHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedPredictionCorrection::BeforeDispatch);
			AfterHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FScopedPredictionCorrection::AfterDispatch);
			return true;
		}
		bool WasObserved() const { return bObserved; }
		bool PreservedReplay() const { return bObserved && bReplayCaptured && bPreservedReplay && !bForwardTickBetweenSamples; }
		void Report() const
		{
			UE_LOG(LogTemp, Display, TEXT("RpgMoverCorrection injected=%d originalFrame=%d injectionFrame=%d injectionTime=%.0f replayCaptured=%d originalInstance=%d replayInstance=%d observed=%d frameBefore=%d frameAfter=%d timeBefore=%.0f timeAfter=%.0f correction=%s preservedReplay=%d forwardTickBetween=%d"),
				bInjected, OriginalFrame, InjectedFrame, InjectedTimeMs, bReplayCaptured, OriginalInstanceId, ReplayInstanceId, bObserved,
				BeforeFrame, CorrectedFrame, BeforeTimeMs, CorrectedTimeMs, *CorrectionDelta.ToCompactString(), bPreservedReplay, bForwardTickBetweenSamples);
			UE_LOG(LogTemp, Display, TEXT("RpgMoverCorrection clock local=%d->%d offset=%d->%d step=%d->%d originalLocal=%d injectionLocal=%d"),
				BeforeClock.LocalPendingFrame, AfterClock.LocalPendingFrame, BeforeClock.ServerOffset, AfterClock.ServerOffset,
				BeforeClock.StepMs, AfterClock.StepMs, OriginalLocalFrame, InjectedLocalFrame);
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(BeforeHandle);
			FWorldDelegates::OnWorldPreActorTick.Remove(AfterHandle);
			BeforeHandle.Reset();
			AfterHandle.Reset();
			BeforeState = FMoverSyncState();
			bBeforeValid = false;
		}
	private:
		void BeforeDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !Pawn.IsValid() || Pawn->GetWorld() != World || !Liaison.IsValid()) return;
			bBeforeValid = Liaison->ReadPendingSyncState(BeforeState);
			BeforeClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison.Get());
			BeforeFrame = BeforeClock.ServerFrame;
			BeforeTimeMs = BeforeClock.SimulationTimeMs;
		}
		void AfterDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !bBeforeValid || !Pawn.IsValid() || Pawn->GetWorld() != World || !Liaison.IsValid()) return;
			FMoverSyncState AfterState;
			if (!Liaison->ReadPendingSyncState(AfterState)) return;
			const FMoverDefaultSyncState* Before = BeforeState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const FMoverDefaultSyncState* After = AfterState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			if (!Before || !After) return;
			const FVector Delta = After->GetLocation_WorldSpace() - Before->GetLocation_WorldSpace();
			if (DiagnosticSamples++ < 12)
			{
				UE_LOG(LogTemp, Display, TEXT("RpgMoverCorrection dispatch sample=%d frame=%d->%d time=%.0f->%.0f before=%s after=%s component=%s replayCaptured=%d beforeA=%d afterA=%d beforeB=%d afterB=%d"),
					DiagnosticSamples, BeforeFrame, Liaison->GetCurrentSimFrame(), BeforeTimeMs, Liaison->GetCurrentSimTimeMs(),
					*Before->GetLocation_WorldSpace().ToCompactString(), *After->GetLocation_WorldSpace().ToCompactString(),
					*Mover(Pawn.Get())->GetUpdatedComponent()->GetComponentLocation().ToCompactString(), bReplayCaptured,
					OriginalAttack.Matches(BeforeState.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>()),
					OriginalAttack.Matches(AfterState.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>()),
					ReplayAttack.Matches(BeforeState.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>()),
					ReplayAttack.Matches(AfterState.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>()));
			}
			if (FVector::DotProduct(Delta, CrossDirection) > -25.0) return;
			bObserved = true;
			CorrectionDelta = Delta;
			CorrectedFrame = Liaison->GetCurrentSimFrame();
			CorrectedTimeMs = Liaison->GetCurrentSimTimeMs();
			AfterClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison.Get());
			// UE5.8 calls newly added delegates first. PreActorTick therefore observes completed network
			// reconciliation before NP starts the next forward frame; verify the clock instead of assuming it.
			bForwardTickBetweenSamples = !BeforeClock.IsSameLocalHead(AfterClock);
			const FAnimMontageInstance* Instance = Mesh(Pawn.Get())->GetAnimInstance()->GetActiveInstanceForMontage(Montage.Get());
			bPreservedReplay = BeforeClock.LocalPendingFrame > OriginalLocalFrame && BeforeClock.LocalPendingFrame >= InjectedLocalFrame
				&& ReplayAttack.Matches(BeforeState.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>())
				&& ReplayAttack.Matches(AfterState.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>())
				&& Instance && Instance->GetInstanceID() == ReplayInstanceId && Instance->IsPlaying()
				&& ASC(Pawn.Get())->GetCurrentMontage() == Montage.Get();
			Report();
		}
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<UMoverNetworkPredictionLiaisonComponent> Liaison;
		TWeakObjectPtr<UAnimMontage> Montage;
		FAttackIdentity OriginalAttack, ReplayAttack;
		FMoverSyncState BeforeState;
		RpgMoverPredictionTests::FFixedPredictionHeadSnapshot BeforeClock, AfterClock;
		FDelegateHandle BeforeHandle, AfterHandle;
		FVector CrossDirection = FVector::ZeroVector, CorrectionDelta = FVector::ZeroVector;
		int32 OriginalInstanceId = INDEX_NONE, ReplayInstanceId = INDEX_NONE, OriginalFrame = INDEX_NONE, InjectedFrame = INDEX_NONE;
		int32 OriginalLocalFrame = INDEX_NONE, InjectedLocalFrame = INDEX_NONE;
		int32 BeforeFrame = INDEX_NONE, CorrectedFrame = INDEX_NONE;
		int32 DiagnosticSamples = 0;
		double InjectedTimeMs = 0.0, BeforeTimeMs = 0.0, CorrectedTimeMs = 0.0;
		bool bInjected = false, bReplayCaptured = false, bBeforeValid = false, bObserved = false;
		bool bPreservedReplay = false, bForwardTickBetweenSamples = false;
	};
	struct FObservation
	{
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<USkeletalMeshComponent> Source;
		TWeakObjectPtr<USkeletalMesh> SourceAsset;
		TWeakObjectPtr<UClass> AnimClass;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		FDelegateHandle EndHandle;
		FVector InitialLocation = FVector::ZeroVector;
		TArray<FQuat> InitialTargetPose;
		bool bSourceChanged = false, bTargetPoseMoved = false, bMontageAdvanced = false;
		bool bSawRootMotion = false, bRootMotionMoved = false, bMontageRestarted = false;
		FVector AttackStartLocation = FVector::ZeroVector;
		float InitialMontagePosition = -1.0f;
		int32 MontageInstanceId = INDEX_NONE;
		int32 Ends = 0, Cancellations = 0;
		uint32 OpenBaseline = 0, CloseBaseline = 0, TraceBaseline = 0;
		TWeakObjectPtr<URpgEquipmentInstance> BlockingItem;
		int32 BlockActorCount = 0;
		float BlockHeldSeconds = 0.0f;
		float MaxBlockHeldSeconds = 0.0f, BlockObservedSeconds = 0.0f;
		bool bBlockEquipmentChanged = false, bBlockUsedRootMotion = false;
		bool bBlockDiagnosticLogged = false, bSawBlockingTag = false, bSawBlockLoop = false;
	};
	/** Every world's natural montage and animation ticks are sampled together, including short notify windows. */
	class FScopedObservations final
	{
	public:
		~FScopedObservations() { Stop(); }
		void Start(int32 PlayerId, UAnimMontage* InMontage)
		{
			Stop();
			Montage = InMontage;
			Records.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				APawn* Pawn = Context.WorldType == EWorldType::PIE ? FindPawn(World, PlayerId) : nullptr;
				if (!Pawn) continue;
				FObservation& Record = Records.FindOrAdd(World);
				Record.Pawn = Pawn;
				Record.Source = Mesh(Pawn);
				Record.SourceAsset = Mesh(Pawn)->GetSkeletalMeshAsset();
				Record.AnimClass = Mesh(Pawn)->GetAnimClass();
				Record.AbilitySystem = ASC(Pawn);
				Record.InitialLocation = Pawn->GetActorLocation();
				Record.EndHandle = ASC(Pawn)->OnAbilityEnded.AddLambda([this, World](const FAbilityEndedData& Data)
				{
					if (Data.AbilityThatEnded && Data.AbilityThatEnded->IsA<URpgGameplayAbility_BasicWeaponAttack>())
					{
						FObservation& EndRecord = Records.FindChecked(World);
						++EndRecord.Ends;
						EndRecord.Cancellations += Data.bWasCancelled ? 1 : 0;
					}
				});
			}
			ResetAttack();
			TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedObservations::Tick);
		}
		void ResetAttack()
		{
			for (auto& Entry : Records)
			{
				FObservation& Record = Entry.Value;
				Record.Ends = Record.Cancellations = 0;
				Record.InitialMontagePosition = -1.0f;
				Record.MontageInstanceId = INDEX_NONE;
				Record.bMontageAdvanced = false;
				Record.bSawRootMotion = Record.bRootMotionMoved = Record.bMontageRestarted = false;
				Record.AttackStartLocation = Record.Pawn.IsValid() ? Record.Pawn->GetActorLocation() : FVector::ZeroVector;
				if (const URpgGameplayAbility_BasicWeaponAttack* Ability = Attack(Record.Pawn.Get()))
				{
					Record.OpenBaseline = Ability->GetAuthorityWindowOpenCountForTests();
					Record.CloseBaseline = Ability->GetAuthorityWindowCloseCountForTests();
					Record.TraceBaseline = Ability->GetAuthorityTraceSampleCountForTests();
				}
			}
		}
		bool AllFinished(bool bCancelled, bool bRequireRootMotionMovement = false) const
		{
			if (Records.Num() != 3) return false;
			for (const auto& Entry : Records)
			{
				const FObservation& Record = Entry.Value;
				APawn* Pawn = Record.Pawn.Get();
				const UAnimInstance* Animation = Mesh(Pawn) ? Mesh(Pawn)->GetAnimInstance() : nullptr;
				if (!Pawn || !Animation || Record.bSourceChanged || Record.bMontageRestarted || Animation->Montage_IsPlaying(Montage.Get())
					|| ASC(Pawn)->GetCurrentMontage() || HasAttackRootMotion(Pawn)) return false;
				if (!bCancelled && (!Record.bMontageAdvanced || !Record.bSawRootMotion)) return false;
				if (bRequireRootMotionMovement && !Record.bRootMotionMoved) return false;
				if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)
				{
					const URpgGameplayAbility_BasicWeaponAttack* Ability = Attack(Pawn);
					if (!Ability || Record.Ends != 1 || Record.Cancellations != (bCancelled ? 1 : 0) || Ability->HasResidualAttackRuntimeStateForTests()) return false;
					if (Pawn->HasAuthority() && (Ability->GetAuthorityWindowOpenCountForTests() != Record.OpenBaseline + 1
						|| Ability->GetAuthorityWindowCloseCountForTests() != Record.CloseBaseline + 1
						|| (!bCancelled && Ability->GetAuthorityTraceSampleCountForTests() <= Record.TraceBaseline))) return false;
				}
			}
			return true;
		}
		bool AllMoving() const
		{
			if (Records.Num() != 3) return false;
			for (const auto& Entry : Records)
			{
				const UCharacterMoverComponent* Movement = Mover(Entry.Value.Pawn.Get());
				if (!Movement || !Movement->IsOnGround() || Movement->GetVelocity().Size2D() < 100.0 || HasAttackRootMotion(Entry.Value.Pawn.Get())) return false;
			}
			return true;
		}
		bool AllIdle() const
		{
			if (Records.Num() != 3) return false;
			for (const auto& Entry : Records)
			{
				const UCharacterMoverComponent* Movement = Mover(Entry.Value.Pawn.Get());
				if (!Movement || !Movement->IsOnGround() || Movement->GetVelocity().Size2D() >= 5.0 || HasAttackRootMotion(Entry.Value.Pawn.Get())) return false;
			}
			return true;
		}
		bool AllVisualsMoved() const
		{
			if (Records.Num() != 3) return false;
			for (const auto& Entry : Records)
				if (!Entry.Value.bTargetPoseMoved || Entry.Value.bSourceChanged) return false;
			return true;
		}
		void ObserveBlock(UAnimMontage* Loop, ERpgEquipmentSlot Slot)
		{
			BlockLoop = Loop;
			BlockSlot = Slot;
			for (auto& Entry : Records)
			{
				FObservation& Record = Entry.Value;
				Record.BlockingItem = Equipment(Record.Pawn.Get())->GetEquipmentInstanceInSlot(Slot);
				Record.BlockActorCount = Record.BlockingItem.IsValid() ? Record.BlockingItem->GetSpawnedActors().Num() : 0;
			}
		}
		bool AllBlockHeld() const
		{
			if (Records.Num() != 3) return false;
			for (const auto& Entry : Records)
			{
				APawn* Pawn = Entry.Value.Pawn.Get();
				const FGameplayAbilitySpec* Spec = BlockSpec(Pawn);
				if (!Pawn || Entry.Value.BlockHeldSeconds < 0.25f || Entry.Value.bBlockEquipmentChanged || Entry.Value.bBlockUsedRootMotion
					|| (Pawn->GetLocalRole() != ROLE_SimulatedProxy && (!Spec || !Spec->IsActive()))) return false;
			}
			return true;
		}
		const FObservation& Get(UWorld* World) const { return Records.FindChecked(World); }
		void Report() const
		{
			for (const auto& Entry : Records)
			{
				const FObservation& Record = Entry.Value;
				const URpgGameplayAbility_BasicWeaponAttack* Ability = Attack(Record.Pawn.Get());
				UE_LOG(LogTemp, Display, TEXT("RpgMoverGameplay world=%s sourceChanged=%d targetMoved=%d montageAdvanced=%d rootLayer=%d rootMoved=%d montageRestarted=%d ends=%d cancelled=%d open=%u close=%u trace=%u"),
					*GetPathNameSafe(Entry.Key.Get()), Record.bSourceChanged, Record.bTargetPoseMoved, Record.bMontageAdvanced,
					Record.bSawRootMotion, Record.bRootMotionMoved, Record.bMontageRestarted, Record.Ends, Record.Cancellations,
					Ability ? Ability->GetAuthorityWindowOpenCountForTests() - Record.OpenBaseline : 0,
					Ability ? Ability->GetAuthorityWindowCloseCountForTests() - Record.CloseBaseline : 0,
					Ability ? Ability->GetAuthorityTraceSampleCountForTests() - Record.TraceBaseline : 0);
				if (BlockLoop.IsValid()) ReportBlock(Record);
			}
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickEnd.Remove(TickHandle);
			TickHandle.Reset();
			for (auto& Entry : Records)
				if (Entry.Value.AbilitySystem.IsValid()) Entry.Value.AbilitySystem->OnAbilityEnded.Remove(Entry.Value.EndHandle);
		}
	private:
		void ReportBlock(const FObservation& Record) const
		{
			APawn* Pawn = Record.Pawn.Get();
			if (!Pawn || !ASC(Pawn) || !Mesh(Pawn) || !Mesh(Pawn)->GetAnimInstance()) return;
			const FGameplayAbilitySpec* Spec = BlockSpec(Pawn);
			const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
			FGameplayTagContainer Tags;
			ASC(Pawn)->GetOwnedGameplayTags(Tags);
			const UInputAction* Action = FindObject<UInputAction>(nullptr, TEXT("/GF_Combat_Core/Input/IA_Block.IA_Block"));
			UE_LOG(LogTemp, Display, TEXT("RpgMoverBlock world=%s held=%.3f maxHeld=%.3f equipmentChanged=%d rootMotion=%d spec=%s active=%d inputPressed=%d rawRMB=%d boundAction=%s tags=%s sawTag=%d sawLoop=%d currentASC=%s currentAnim=%s expectedLoop=%s loopPlaying=%d loopPosition=%.3f"),
				*GetPathNameSafe(Pawn->GetWorld()), Record.BlockHeldSeconds, Record.MaxBlockHeldSeconds, Record.bBlockEquipmentChanged, Record.bBlockUsedRootMotion,
				Spec ? *Spec->Handle.ToString() : TEXT("None"), Spec && Spec->IsActive(), Spec && Spec->InputPressed,
				PC && PC->IsInputKeyDown(EKeys::RightMouseButton), Action ? *UEnhancedInputLibrary::GetBoundActionValue(Pawn, Action).ToString() : TEXT("Missing"),
				*Tags.ToStringSimple(), Record.bSawBlockingTag, Record.bSawBlockLoop, *GetNameSafe(ASC(Pawn)->GetCurrentMontage()),
				*GetNameSafe(Mesh(Pawn)->GetAnimInstance()->GetCurrentActiveMontage()), *GetNameSafe(BlockLoop.Get()),
				Mesh(Pawn)->GetAnimInstance()->Montage_IsPlaying(BlockLoop.Get()), Mesh(Pawn)->GetAnimInstance()->Montage_GetPosition(BlockLoop.Get()));
		}
		void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			if (!ActiveWorld(World)) return;
			FObservation* Record = Records.Find(World);
			APawn* Pawn = Record ? Record->Pawn.Get() : nullptr;
			if (!Pawn) return;
			USkeletalMeshComponent* Source = Mesh(Pawn);
			Record->bSourceChanged |= Source != Record->Source.Get() || !Source || Source->GetSkeletalMeshAsset() != Record->SourceAsset.Get()
				|| Source->GetAnimClass() != Record->AnimClass.Get() || ASC(Pawn) != Record->AbilitySystem.Get()
				|| !ASC(Pawn)->AbilityActorInfo.IsValid() || ASC(Pawn)->AbilityActorInfo->SkeletalMeshComponent.Get() != Source
				|| Mover(Pawn)->GetPrimaryVisualComponent() != Source;
			if (UAnimInstance* Animation = Source ? Source->GetAnimInstance() : nullptr; Animation && Animation->Montage_IsPlaying(Montage.Get()))
			{
				const float Position = Animation->Montage_GetPosition(Montage.Get());
				if (Record->InitialMontagePosition < 0.0f) Record->InitialMontagePosition = Position;
				Record->bMontageAdvanced |= Position > Record->InitialMontagePosition + 0.1f;
				if (const FAnimMontageInstance* Instance = Animation->GetActiveInstanceForMontage(Montage.Get()))
				{
					if (Record->MontageInstanceId == INDEX_NONE) Record->MontageInstanceId = Instance->GetInstanceID();
					else Record->bMontageRestarted |= Record->MontageInstanceId != Instance->GetInstanceID();
				}
			}
			Record->bSawRootMotion |= HasAttackRootMotion(Pawn);
			Record->bRootMotionMoved |= Record->bSawRootMotion && FVector::Dist2D(Record->AttackStartLocation, Pawn->GetActorLocation()) > 2.0;
			if (BlockLoop.IsValid())
			{
				const URpgEquipmentInstance* Item = Equipment(Pawn)->GetEquipmentInstanceInSlot(BlockSlot);
				Record->bBlockEquipmentChanged |= !Item || Item != Record->BlockingItem.Get();
				if (Item)
				{
					const TArray<AActor*> Actors = Item->GetSpawnedActors();
					Record->bBlockEquipmentChanged |= Actors.Num() != Record->BlockActorCount || Actors.IsEmpty();
					for (const AActor* Actor : Actors)
					{
						const USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
						Record->bBlockEquipmentChanged |= !Root || Root->GetAttachParent() != Source
							|| Root->GetAttachSocketName().IsNone() || !Source->DoesSocketExist(Root->GetAttachSocketName());
					}
				}
				Record->bBlockUsedRootMotion |= HasAttackRootMotion(Pawn);
				const UAnimInstance* Animation = Source->GetAnimInstance();
				const bool bHasTag = ASC(Pawn)->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Blocking")));
				const bool bLoopPlaying = Animation->Montage_IsPlaying(BlockLoop.Get());
				Record->bSawBlockingTag |= bHasTag;
				Record->bSawBlockLoop |= bLoopPlaying;
				if (bHasTag && bLoopPlaying && ASC(Pawn)->GetCurrentMontage() == BlockLoop.Get()) Record->BlockHeldSeconds += DeltaSeconds;
				else Record->BlockHeldSeconds = 0.0f;
				Record->MaxBlockHeldSeconds = FMath::Max(Record->MaxBlockHeldSeconds, Record->BlockHeldSeconds);
				Record->BlockObservedSeconds += DeltaSeconds;
				if (!Record->bBlockDiagnosticLogged && Record->BlockObservedSeconds >= 1.0f)
				{
					Record->bBlockDiagnosticLogged = true;
					ReportBlock(*Record);
				}
			}
			if (VisibleTarget(Pawn))
			{
				const TArray<FQuat> Pose = LimbPose(Retarget(Pawn)->GetRetargetMesh());
				if (Record->InitialTargetPose.IsEmpty()) Record->InitialTargetPose = Pose;
				if (Pose.Num() == 4 && Record->InitialTargetPose.Num() == Pose.Num()
					&& FVector::Dist2D(Record->InitialLocation, Pawn->GetActorLocation()) > 150.0 && Mover(Pawn)->GetVelocity().Size2D() > 100.0)
					for (int32 Index = 0; Index < Pose.Num(); ++Index)
						Record->bTargetPoseMoved |= Pose[Index].AngularDistance(Record->InitialTargetPose[Index]) > 0.15;
			}
		}
		TMap<TWeakObjectPtr<UWorld>, FObservation> Records;
		TWeakObjectPtr<UAnimMontage> Montage;
		TWeakObjectPtr<UAnimMontage> BlockLoop;
		ERpgEquipmentSlot BlockSlot = ERpgEquipmentSlot::None;
		FDelegateHandle TickHandle;
	};
	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(GaspMoverGameplayPIE, "SurvivalRpg.GASP.Mover.Gameplay")
{
	using FState = RpgGaspMoverGameplayTests::FState;
	RpgGaspMoverGameplayTests::FScopedWorld Isolation;
	RpgGaspMoverGameplayTests::FScopedInput Input;
	RpgGaspMoverGameplayTests::FScopedObservations Observations;
	RpgGaspMoverGameplayTests::FScopedLatency Latency;
	RpgGaspMoverGameplayTests::FScopedPredictionCorrection Correction;
	TStrongObjectPtr<URpgRuntimeRetargetProfile> EnabledProfile;
	TStrongObjectPtr<UClass> SwordClass;
	TStrongObjectPtr<UAnimMontage> Montage;
	TStrongObjectPtr<UAnimMontage> BlockLoop, BlockStart, BlockEnd;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId PreviousExperience;
	int32 SubjectId = INDEX_NONE;
	bool bConfigured = false, bRetarget = false;
	ERpgEquipmentSlot BlockSlot = ERpgEquipmentSlot::None;

	BEFORE_EACH()
	{
		using namespace RpgGaspMoverGameplayTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Mover gameplay automation refuses to interrupt an existing PIE session."));
				return;
			}
		UClass* GameMode = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameMode));
		if (!GameMode) return;
		Isolation.Start();
		PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameMode).Build(Network);
	}
	AFTER_EACH()
	{
		Input.Stop();
		if (TestRunner->HasAnyErrors()) Observations.Report();
		if (TestRunner->HasAnyErrors()) Correction.Report();
		Correction.Stop();
		Latency.Stop();
		Observations.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
	}
	TEST_METHOD(RemoteEquipmentInputMontageLateJoinAndCancellation) { Queue(false); }
	TEST_METHOD(OptionalFollowerPreservesGameplayEquipmentAndAttack) { Queue(true); }
	TEST_METHOD(FixedCorrectionPreservesImmediateMontageReplay) { Queue(false, true); }
	TEST_METHOD(RightMouseHoldReplicatesBlockAndReleasesCleanly) { Queue(false, false, true); }

	const URpgEquipmentDefinition* Sword() const { return SwordClass->GetDefaultObject<URpgEquipmentDefinition>(); }
	void VerifySource(UWorld* World)
	{
		using namespace RpgGaspMoverGameplayTests;
		APawn* Pawn = FindPawn(World, SubjectId);
		ASSERT_THAT(IsTrue(Isolation.IsIsolated(World)));
		ASSERT_THAT(IsTrue(Ready(World, Pawn)));
		ASSERT_THAT(IsTrue(EquipmentOnSource(Pawn, Sword())));
		if (!Pawn) return;
		ASSERT_THAT(IsFalse(Observations.Get(World).bSourceChanged));
		if (bRetarget)
		{
			ASSERT_THAT(IsTrue(VisibleTarget(Pawn)));
			USkeletalMeshComponent* Target = Retarget(Pawn)->GetRetargetMesh();
			if (!Target) return;
			ASSERT_THAT(IsTrue(Target != Mesh(Pawn) && Target->GetAttachParent() == Mesh(Pawn)));
			ASSERT_THAT(IsTrue(Target->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Target->IsSimulatingPhysics()));
			ASSERT_THAT(IsTrue(Target->PrimaryComponentTick.TickGroup == TG_PostPhysics));
		}
	}
	void Queue(bool bEnableRetarget, bool bForceCorrection = false, bool bTestBlock = false)
	{
		using namespace RpgGaspMoverGameplayTests;
		if (!bConfigured) return;
		bRetarget = bEnableRetarget;
		if (bRetarget)
		{
			EnabledProfile.Reset(NewObject<URpgRuntimeRetargetProfile>(GetTransientPackage()));
			EnabledProfile->TargetMesh = LoadObject<USkeletalMesh>(nullptr, TargetMeshPath);
			EnabledProfile->Retargeter = LoadObject<UIKRetargeter>(nullptr, RetargeterPath);
			const URpgPawnData* Data = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
			EnabledProfile->RetargetAnimClass = Data && Data->RuntimeRetargetProfile ? Data->RuntimeRetargetProfile->RetargetAnimClass : nullptr;
			ASSERT_THAT(IsNotNull(EnabledProfile->TargetMesh.Get()));
			ASSERT_THAT(IsNotNull(EnabledProfile->Retargeter.Get()));
			ASSERT_THAT(IsNotNull(EnabledProfile->RetargetAnimClass.Get()));
		}
		Network.UntilClient(TEXT("Remote Mover receives normal Experience, equipment foundation and ability input"), 0, [](FState& State)
			{ return Ready(State.World, LocalPawn(State.World)) && Mover(LocalPawn(State.World))->IsOnGround(); }, Timeout())
			.ThenClient(TEXT("Select the autonomous owner and retain ordinary player input"), 0, [this](FState& State)
			{
				APawn* Pawn = LocalPawn(State.World);
				ASSERT_THAT(IsTrue(Pawn->GetLocalRole() == ROLE_AutonomousProxy));
				ASSERT_THAT(IsNotNull(DefaultProfile(Pawn)));
				ASSERT_THAT(IsTrue(Retarget(Pawn)->GetRetargetProfile() == DefaultProfile(Pawn)));
				ASSERT_THAT(IsNull(Retarget(Pawn)->GetRetargetMesh()));
				SubjectId = Pawn->GetPlayerState()->GetPlayerId();
				Input.Start(Pawn);
			})
			.UntilServer(TEXT("Authority resolves the same initialized Mover pawn"), [this](FState& State)
				{ return Ready(State.World, FindPawn(State.World, SubjectId)); }, Timeout())
			.ThenServer(TEXT("Read weapon content after the Experience has activated its Combat Feature"), [this](FState&)
			{
				SwordClass.Reset(LoadClass<URpgEquipmentDefinition>(nullptr, SwordPath));
				ASSERT_THAT(IsTrue(SwordClass.IsValid()));
			})
			.UntilServer(TEXT("Combat Feature starter loadout equips and grants the existing sword on authority"), [this](FState& State)
				{ APawn* Pawn = FindPawn(State.World, SubjectId); return Attack(Pawn) && EquipmentOnSource(Pawn, Sword()); }, Timeout())
			.ThenServer(TEXT("Read the actual equipment-supplied attack montage without changing the loadout"), [this](FState& State)
			{
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World)));
				APawn* Pawn = FindPawn(State.World, SubjectId);
				const FRpgWeaponAttackDefinition* Definition = Weapon(Pawn) ? Weapon(Pawn)->FindAttackDefinition(
					FGameplayTag::RequestGameplayTag(TEXT("Weapon.Attack.Primary"))) : nullptr;
				ASSERT_THAT(IsNotNull(Definition));
				if (Definition) Montage.Reset(Definition->Montage.Get());
				ASSERT_THAT(IsTrue(Montage.IsValid()));
				ASSERT_THAT(IsTrue(Montage.IsValid() && Montage->HasRootMotion()));
			})
			.UntilClient(TEXT("Owner receives the equipment grant and weapon attachment"), 0, [this](FState& State)
				{ return Attack(LocalPawn(State.World)) && EquipmentOnSource(LocalPawn(State.World), Sword()); }, Timeout())
			.ThenClientJoins()
			.UntilClient(TEXT("Late join receives the equipped simulated proxy on its original gameplay mesh"), 1, [this](FState& State)
			{
				APawn* Pawn = FindPawn(State.World, SubjectId);
				return Ready(State.World, Pawn) && Pawn->GetLocalRole() == ROLE_SimulatedProxy && EquipmentOnSource(Pawn, Sword());
			}, Timeout())
			.ThenServer(TEXT("Observe source identity and montage lifecycle on all three peers"), [this](FState& State)
			{
				ASSERT_THAT(IsNotNull(Attack(FindPawn(State.World, SubjectId))));
				Observations.Start(SubjectId, Montage.Get());
			});
		if (bTestBlock)
		{
			QueueBlock();
			return;
		}
		if (bRetarget)
		{
			// ApplyProfile is local cosmetic configuration; this intentionally makes no profile-replication claim.
			Network.ThenServer(TEXT("Apply a transient optional presentation on authority"), [this](FState& State)
				{ ASSERT_THAT(IsTrue(Retarget(FindPawn(State.World, SubjectId))->ApplyProfile(EnabledProfile.Get()))); })
				.ThenClients(TEXT("Apply the same transient presentation on owner and late observer"), [this](FState& State)
					{ ASSERT_THAT(IsTrue(Retarget(FindPawn(State.World, SubjectId))->ApplyProfile(EnabledProfile.Get()))); })
				.UntilServer(TEXT("The authority follower presents a valid retargeted pose"), [this](FState& State)
					{ return VisibleTarget(FindPawn(State.World, SubjectId)); }, Timeout())
				.UntilClients(TEXT("Owner and late observer followers present valid poses"), [this](FState& State)
					{ return VisibleTarget(FindPawn(State.World, SubjectId)); }, Timeout())
				.ThenClient(TEXT("Move through the original Mover input with optional presentation active"), 0, [this](FState&) { Input.Move(true); })
				.UntilServer(TEXT("All three targets animate moving limbs without replacing source meshes"), [this](FState&)
					{ return Observations.AllVisualsMoved(); }, Timeout())
				.ThenClient(TEXT("Release movement before the equipment montage"), 0, [this](FState&) { Input.Move(false); })
				.UntilServer(TEXT("Mover returns to supported idle before attacking"), [this](FState& State)
				{
					const UCharacterMoverComponent* Movement = Mover(FindPawn(State.World, SubjectId));
					return Movement && Movement->IsOnGround() && Movement->GetVelocity().Size2D() < 5.0;
				}, Timeout());
		}
		Network.UntilServer(TEXT("All roles are stationary before measuring attack root motion"), [this](FState&)
				{ return Observations.AllIdle(); }, Timeout())
			.ThenServer(TEXT("Capture stationary root-motion and montage baselines"), [this](FState&) { Observations.ResetAttack(); })
			.ThenClient(TEXT("Press the real primary-attack key through RPG input routing"), 0, [this](FState&) { Input.PressAttack(); })
			.UntilServer(TEXT("All peers finish the real montage and authority completes usable hit-window traces"), [this](FState&)
				{ return Observations.AllFinished(false, true); }, Timeout())
			.ThenServer(TEXT("Verify source mesh and weapon contracts after the natural attack"), [this](FState& State) { VerifySource(State.World); })
			.ThenClients(TEXT("Owner and late observer preserve source equipment through the attack"), [this](FState& State) { VerifySource(State.World); });
		if (bForceCorrection)
		{
			QueueCorrection();
			return;
		}
		Network.ThenServer(TEXT("Record a second attack for explicit cancellation"), [this](FState&) { Observations.ResetAttack(); })
			.ThenClient(TEXT("Hold ordinary Mover movement and press primary attack again"), 0, [this](FState&) { Input.Move(true); Input.PressAttack(); })
			.UntilServer(TEXT("The second attack has an active authoritative hit window"), [this](FState& State)
			{
				const URpgGameplayAbility_BasicWeaponAttack* Ability = Attack(FindPawn(State.World, SubjectId));
				return Ability && Ability->IsAttackWindowOpenForTests();
			}, Timeout())
			.ThenServer(TEXT("Cancel the active equipment-granted ability through GAS"), [this](FState& State)
			{
				APawn* Pawn = FindPawn(State.World, SubjectId);
				const FGameplayAbilitySpec* Spec = AttackSpec(Pawn);
				ASSERT_THAT(IsTrue(Spec && Spec->IsActive()));
				if (Spec) ASC(Pawn)->CancelAbilityHandle(Spec->Handle);
			})
			.UntilServer(TEXT("Cancellation closes the authority window and clears owner and proxy montages"), [this](FState&)
				{ return Observations.AllFinished(true); }, Timeout())
			.UntilServer(TEXT("Held movement resumes through ordinary Mover simulation after cancellation"), [this](FState&)
				{ return Observations.AllMoving(); }, Timeout())
			.ThenServer(TEXT("Begin a fresh lifecycle for the same montage after cancellation"), [this](FState&) { Observations.ResetAttack(); })
			.ThenClient(TEXT("Replay the same equipment montage through primary input while moving"), 0, [this](FState&) { Input.PressAttack(); })
			.UntilServer(TEXT("Replay finishes once on every peer and leaves no attack root-motion layer"), [this](FState&)
				{ return Observations.AllFinished(false); }, Timeout())
			.ThenClient(TEXT("Release held movement after the replay"), 0, [this](FState&) { Input.Move(false); })
			.UntilServer(TEXT("Normal supported idle returns with no residual ability movement"), [this](FState&)
				{ return Observations.AllIdle(); }, Timeout());
		if (bRetarget)
		{
			Network.ThenServer(TEXT("Clear optional presentation and reject invalid configuration on authority"), [this](FState& State) { VerifyFallback(State.World); })
				.ThenClients(TEXT("Clear optional presentation and reject invalid configuration on both clients"), [this](FState& State) { VerifyFallback(State.World); });
		}
	}
	void QueueBlock()
	{
		using namespace RpgGaspMoverGameplayTests;
		Network.UntilServer(TEXT("Starter loadout supplies the active equipment block grant"), [this](FState& State)
				{ return BlockSpec(FindPawn(State.World, SubjectId)) != nullptr; }, Timeout())
			.ThenServer(TEXT("Read block montages from the equipment selected by normal input routing"), [this](FState& State)
			{
				const FGameplayAbilitySpec* Spec = BlockSpec(FindPawn(State.World, SubjectId));
				const URpgWeaponInstance* Item = Spec ? Cast<URpgWeaponInstance>(Spec->SourceObject.Get()) : nullptr;
				ASSERT_THAT(IsNotNull(Item));
				if (!Item) return;
				const FRpgWeaponBlockDefinition& Definition = Item->GetBlockDefinition();
				ASSERT_THAT(IsTrue(Definition.bCanBlock));
				BlockSlot = Item->GetEquippedSlot();
				BlockLoop.Reset(Definition.BlockLoopMontage.Get());
				BlockStart.Reset(Definition.BlockStartMontage.Get());
				BlockEnd.Reset(Definition.BlockEndMontage.Get());
				ASSERT_THAT(IsTrue(BlockLoop.IsValid()));
				for (const UAnimMontage* Clip : { BlockStart.Get(), BlockLoop.Get(), BlockEnd.Get() })
				{
					if (!Clip) continue;
					ASSERT_THAT(IsFalse(Clip->HasRootMotion()));
					UE_LOG(LogTemp, Display, TEXT("RpgMoverBlock content item=%s clip=%s length=%.3f rate=%.3f autoBlendOut=%d"),
						*GetPathNameSafe(Item), *Clip->GetPathName(), Clip->GetPlayLength(), Clip->RateScale, Clip->bEnableAutoBlendOut);
					for (const FCompositeSection& Section : Clip->CompositeSections)
						UE_LOG(LogTemp, Display, TEXT("RpgMoverBlock section clip=%s name=%s next=%s time=%.3f"),
							*Clip->GetName(), *Section.SectionName.ToString(), *Section.NextSectionName.ToString(), Section.GetTime());
				}
			})
			.UntilClients(TEXT("Owner and late proxy receive the same attached blocking equipment"), [this](FState& State)
			{
				APawn* Pawn = FindPawn(State.World, SubjectId);
				const URpgEquipmentInstance* Item = Equipment(Pawn) ? Equipment(Pawn)->GetEquipmentInstanceInSlot(BlockSlot) : nullptr;
				if (!Item || Item->GetSpawnedActors().IsEmpty() || (Pawn->IsLocallyControlled() && !BlockSpec(Pawn))) return false;
				for (const AActor* Actor : Item->GetSpawnedActors())
					if (!Actor || !Actor->GetRootComponent() || Actor->GetRootComponent()->GetAttachParent() != Mesh(Pawn)) return false;
				return true;
			}, Timeout())
			.ThenServer(TEXT("Observe the real block loop and equipment on all peers"), [this](FState&)
				{ Observations.ObserveBlock(BlockLoop.Get(), BlockSlot); })
			.ThenClient(TEXT("Press and hold RMB through the combat mapping context"), 0, [this](FState&) { Input.HoldBlock(true); })
			.UntilServer(TEXT("Owner, authority and proxy sustain the blocking tag and loop montage"), [this](FState&)
				{ return Observations.AllBlockHeld(); }, Timeout())
			.ThenServer(TEXT("Blocking retains the authority gameplay mesh and equipment"), [this](FState& State) { VerifySource(State.World); })
			.ThenClients(TEXT("Blocking retains owner and late-proxy gameplay sources"), [this](FState& State) { VerifySource(State.World); })
			.ThenClient(TEXT("Release RMB through the same input path"), 0, [this](FState&) { Input.HoldBlock(false); })
			.UntilServer(TEXT("Authority clears block state and finishes its authored release montage"), [this](FState& State)
				{ return BlockReleased(State.World); }, Timeout())
			.UntilClients(TEXT("Owner and late proxy clear replicated block state and montage playback"), [this](FState& State)
				{ return BlockReleased(State.World); }, Timeout())
			.ThenServer(TEXT("The authority loadout remains unchanged after releasing block"), [this](FState& State) { VerifySource(State.World); })
			.ThenClients(TEXT("Both clients retain their original gameplay mesh and attachments"), [this](FState& State) { VerifySource(State.World); });
	}
	bool BlockReleased(UWorld* World) const
	{
		using namespace RpgGaspMoverGameplayTests;
		APawn* Pawn = FindPawn(World, SubjectId);
		if (!Pawn || !ASC(Pawn) || !Mesh(Pawn) || !Mesh(Pawn)->GetAnimInstance()) return false;
		if (ASC(Pawn)->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Blocking")))
			|| ASC(Pawn)->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.PerfectBlockWindow")))
			|| ASC(Pawn)->GetCurrentMontage() || HasAttackRootMotion(Pawn)) return false;
		for (const UAnimMontage* Clip : { BlockStart.Get(), BlockLoop.Get(), BlockEnd.Get() })
			if (Clip && Mesh(Pawn)->GetAnimInstance()->Montage_IsPlaying(Clip)) return false;
		const FGameplayAbilitySpec* Spec = BlockSpec(Pawn);
		const FObservation& Record = Observations.Get(World);
		return (Pawn->GetLocalRole() == ROLE_SimulatedProxy || (Spec && !Spec->IsActive()))
			&& !Record.bBlockEquipmentChanged && !Record.bBlockUsedRootMotion;
	}
	void QueueCorrection()
	{
		using namespace RpgGaspMoverGameplayTests;
		Network.UntilServer(TEXT("All peers settle before introducing controlled network delay"), [this](FState&)
				{ return Observations.AllIdle(); }, Timeout())
			.ThenServer(TEXT("Delay acknowledgements so attack history outlives its local GAS instance"), [this](FState&)
				{ ASSERT_THAT(IsTrue(Latency.Start())); Observations.ResetAttack(); })
			.ThenClient(TEXT("Start attack A through the actual primary input"), 0, [this](FState&) { Input.PressAttack(); })
			.UntilServer(TEXT("Authority is simulating attack A before any prediction error is introduced"), [this](FState& State)
			{
				APawn* Pawn = FindPawn(State.World, SubjectId);
				return AttackSpec(Pawn) && AttackSpec(Pawn)->IsActive() && HasAttackRootMotion(Pawn);
			}, Timeout())
			.UntilClient(TEXT("Owner attack A has generated root-motion history"), 0, [this](FState& State)
			{
				APawn* Pawn = LocalPawn(State.World);
				const UAnimInstance* Animation = Mesh(Pawn)->GetAnimInstance();
				return HasAttackRootMotion(Pawn) && Animation->Montage_IsPlaying(Montage.Get())
					&& Animation->Montage_GetPosition(Montage.Get()) > 0.2f;
			}, Timeout())
			.ThenClient(TEXT("Record attack A, cancel it and immediately request the same montage as B"), 0, [this](FState& State)
			{
				APawn* Pawn = LocalPawn(State.World);
				ASSERT_THAT(IsTrue(Correction.CaptureOriginal(Pawn, Montage.Get())));
				const FGameplayAbilitySpec* Spec = AttackSpec(Pawn);
				ASSERT_THAT(IsTrue(Spec && Spec->IsActive()));
				if (Spec) ASC(Pawn)->CancelAbilityHandle(Spec->Handle);
				ASSERT_THAT(IsTrue(Spec && !Spec->IsActive()));
				ASSERT_THAT(IsTrue(Observations.Get(State.World).Cancellations == 1));
				Input.PressAttack();
			})
			.UntilClient(TEXT("Replay B enters simulation history before only its owner prediction is perturbed"), 0, [this](FState&)
				{ return Correction.CaptureReplayAndInject(); }, Timeout())
			.UntilClient(TEXT("A real authority correction restores and resimulates the perturbed owner history"), 0, [this](FState&)
				{ return Correction.WasObserved(); }, Timeout())
			.ThenClient(TEXT("The first correction preserves the active replay before another forward tick can repair it"), 0, [this](FState&)
				{ Correction.Report(); ASSERT_THAT(IsTrue(Correction.PreservedReplay())); Correction.Stop(); })
			.UntilServer(TEXT("Both network attacks finish and leave no root-motion or hit-window state on any peer"), [this](FState&)
			{
				int32 Peers = 0;
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					UWorld* World = Context.WorldType == EWorldType::PIE ? Context.World() : nullptr;
					APawn* Pawn = FindPawn(World, SubjectId);
					if (!Pawn) continue;
					++Peers;
					if (ASC(Pawn)->GetCurrentMontage() || Mesh(Pawn)->GetAnimInstance()->Montage_IsPlaying(Montage.Get()) || HasAttackRootMotion(Pawn)) return false;
					if (const URpgGameplayAbility_BasicWeaponAttack* Ability = Attack(Pawn); Ability && Ability->HasResidualAttackRuntimeStateForTests()) return false;
				}
				return Peers == 3;
			}, Timeout())
			.ThenServer(TEXT("Restore network timing and verify authority gameplay source after reconciliation"), [this](FState& State)
				{ Latency.Stop(); VerifySource(State.World); })
			.ThenClients(TEXT("Owner and late proxy retain the same gameplay mesh and equipment after reconciliation"), [this](FState& State)
				{ VerifySource(State.World); });
	}
	void VerifyFallback(UWorld* World)
	{
		using namespace RpgGaspMoverGameplayTests;
		APawn* Pawn = FindPawn(World, SubjectId);
		URpgRuntimeRetargetComponent* Component = Retarget(Pawn);
		TWeakObjectPtr<USkeletalMeshComponent> OldTarget = Component->GetRetargetMesh();
		ASSERT_THAT(IsTrue(Component->ApplyProfile(nullptr)));
		ASSERT_THAT(IsNull(Component->GetRetargetMesh()));
		ASSERT_THAT(IsTrue(!OldTarget.IsValid() || !OldTarget->IsRegistered()));
		ASSERT_THAT(IsTrue(Mesh(Pawn)->IsVisible()));
		TStrongObjectPtr<URpgRuntimeRetargetProfile> Invalid(NewObject<URpgRuntimeRetargetProfile>(GetTransientPackage()));
		Invalid->TargetMesh = EnabledProfile->TargetMesh;
		Invalid->RetargetAnimClass = EnabledProfile->RetargetAnimClass;
		ASSERT_THAT(IsFalse(Component->ApplyProfile(Invalid.Get())));
		ASSERT_THAT(IsNull(Component->GetRetargetMesh()));
		Component->RefreshFromPawnData();
		ASSERT_THAT(IsTrue(Component->GetRetargetProfile() == DefaultProfile(Pawn)));
		ASSERT_THAT(IsTrue(Mesh(Pawn) == Observations.Get(World).Source.Get() && Mesh(Pawn)->IsVisible()));
		ASSERT_THAT(IsTrue(Ready(World, Pawn) && EquipmentOnSource(Pawn, Sword()) && !Observations.Get(World).bSourceChanged));
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
