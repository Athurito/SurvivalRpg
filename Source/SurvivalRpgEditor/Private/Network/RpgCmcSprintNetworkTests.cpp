// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameplayEffect.h"
#include "InputKeyEventArgs.h"
#include "Misc/Guid.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#if ENABLE_PIE_NETWORK_TEST
namespace RpgCmcSprintNetworkTests
{
	constexpr float RunSpeed = 375.f, SprintSpeed = 585.f, DrainRate = 15.f;
	FPrimaryAssetId ExperienceId() { return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMantleExperience")); }
	FTimespan Timeout() { return FTimespan::FromSeconds(35.0); }
	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(World) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		return false;
	}
	ACharacter* LocalPawn(UWorld* World)
	{
		const APlayerController* Controller = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
	}
	ACharacter* Pawn(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		if (State && PlayerId != INDEX_NONE)
			for (const APlayerState* Player : State->PlayerArray)
				if (Player && Player->GetPlayerId() == PlayerId) return Cast<ACharacter>(Player->GetPawn());
		return nullptr;
	}
	URpgCharacterMovementComponent* Movement(const ACharacter* Character)
	{
		return Character ? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	}
	URpgAbilitySystemComponent* ASC(const ACharacter* Character)
	{
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		return Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr;
	}
	float Stamina(const ACharacter* Character)
	{
		const URpgStaminaSet* Set = ASC(Character) ? ASC(Character)->GetSet<URpgStaminaSet>() : nullptr;
		return Set ? Set->GetStamina() : -1.f;
	}
	FGameplayAbilitySpec* SprintSpec(ACharacter* Character)
	{
		if (URpgAbilitySystemComponent* AbilitySystem = ASC(Character))
			for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
				if (!Spec.PendingRemove && Spec.GetDynamicSpecSourceTags().HasTagExact(RpgGameplayTags::InputTag_Ability_Sprint)) return &Spec;
		return nullptr;
	}
	bool Blocking(const ACharacter* Character) { return ASC(Character) && ASC(Character)->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Blocking"))); }
	bool Ready(UWorld* World, ACharacter* Character)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = State ? State->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		const URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character);
		const ARpgPlayerState* Player = Character ? Character->GetPlayerState<ARpgPlayerState>() : nullptr;
		const FGameplayTag GameplayReady = FGameplayTag::RequestGameplayTag(TEXT("InitState.GameplayReady"));
		return Experience && Experience->IsExperienceLoaded() && Experience->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Player && Extension && Gameplay && ASC(Character) && Movement(Character)
			&& Extension->HasReachedInitState(GameplayReady) && Gameplay->HasReachedInitState(GameplayReady)
			&& Extension->GetPawnData<URpgPawnData>() == Experience->GetCurrentExperienceChecked()->DefaultPawnData
			&& Player->GetPawnData<URpgPawnData>() == Extension->GetPawnData<URpgPawnData>()
			&& ASC(Character) == Player->GetRpgAbilitySystemComponent() && ASC(Character)->GetAvatarActor() == Character
			&& Movement(Character)->bEnableGASSprint && Movement(Character)->IsMovingOnGround()
			&& FMath::IsNearlyEqual(Movement(Character)->MaxWalkSpeed, RunSpeed) && FMath::IsNearlyEqual(Movement(Character)->SprintSpeed, SprintSpeed)
			&& Stamina(Character) >= 0.f && (!Character->IsLocallyControlled() || Gameplay->IsReadyToBindInputs());
	}

	/** Installs isolated save names and collision-valid lanes before any gameplay pawn is spawned. */
	class FScopedWorld final
	{
	public:
		~FScopedWorld() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_CMCSprintAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedWorld::Initialize);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!ActiveWorld(World)) return false;
			if (World->GetNetMode() == NM_Client) return !World->GetAuthGameMode();
			const ARpgGameModeBase* Mode = World->GetAuthGameMode<ARpgGameModeBase>();
			return Mode && !Mode->bEnableDiskPersistence && Mode->WorldSaveSlotName == Prefix
				&& Mode->WorldSaveBackupSlotName == Prefix + TEXT("_Backup") && Mode->WorldSaveRecoverySlotName == Prefix + TEXT("_Recovery")
				&& Mode->OfflineProfileKey == Prefix;
		}
	private:
		void Initialize(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* Mode = Cast<ARpgGameModeBase>(Initialized);
			if (!Mode || !Mode->GetWorld() || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
			Mode->bEnableDiskPersistence = false;
			Mode->WorldSaveSlotName = Prefix; Mode->WorldSaveBackupSlotName = Prefix + TEXT("_Backup");
			Mode->WorldSaveRecoverySlotName = Prefix + TEXT("_Recovery"); Mode->OfflineProfileKey = Prefix;
			FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Mode->GetWorld()->SpawnActor<ARpgCombatNetworkFloorFixture>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
			// Separate real input routes from host/joining capsules without changing normal net relevancy.
			for (int32 Index = 0; Index < 3; ++Index)
				Mode->GetWorld()->SpawnActor<APlayerStart>(FVector(0.0, (Index - 1) * 5000.0, 120.0), FRotator::ZeroRotator, Spawn);
		}
		FString Prefix;
		FDelegateHandle Handle;
	};
	struct FNoFocusFlush final : FInputModeGameOnly { virtual bool ShouldFlushInputOnViewportFocus() const override { return false; } };
	/** The editor may move focus during late join; real press/release transitions remain the only gameplay input. */
	class FScopedInput final
	{
	public:
		~FScopedInput() { Stop(); }
		bool Start(ACharacter* Character)
		{
			Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
			if (!Controller.IsValid() || !Controller->IsLocalController() || !Controller->ShouldFlushKeysWhenViewportFocusChanges()) return false;
#if UE_ENABLE_DEBUG_DRAWING
			if (Controller->GetCurrentInputModeDebugString() != FInputModeGameOnly().GetDebugDisplayName()) { Controller.Reset(); return false; }
#endif
			bOldFlush = GetDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost;
			GetMutableDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost = false;
			Controller->SetInputMode(FNoFocusFlush()); Controller->SetIgnoreLookInput(true); bStarted = true;
			return true;
		}
		ACharacter* Character() const { return Controller.IsValid() ? Cast<ACharacter>(Controller->GetPawn()) : nullptr; }
		void Key(FKey KeyName, bool bPressed)
		{
			if (Controller.IsValid() && Controller->IsInputKeyDown(KeyName) != bPressed)
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(KeyName, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f));
		}
		void Stop()
		{
			if (!bStarted) { Controller.Reset(); return; }
			for (FKey KeyName : { EKeys::W, EKeys::LeftShift, EKeys::RightMouseButton }) Key(KeyName, false);
			if (Controller.IsValid()) { Controller->SetIgnoreLookInput(false); Controller->SetInputMode(FInputModeGameOnly()); }
			GetMutableDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost = bOldFlush;
			bStarted = false; Controller.Reset();
		}
	private:
		TWeakObjectPtr<APlayerController> Controller;
		bool bStarted = false, bOldFlush = true;
	};
	struct FWindow
	{
		int32 Samples = 0;
		double Elapsed = 0., FirstTime = 0., LastTime = 0.;
		float MinimumSpeed = TNumericLimits<float>::Max(), MaximumSpeed = 0.f;
		float FirstStamina = 0.f, LastStamina = 0.f, MinimumStamina = TNumericLimits<float>::Max();
		bool bInvalid = false;
	};
	struct FPeer
	{
		TWeakObjectPtr<ACharacter> Character;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		FGameplayAbilitySpecHandle SprintHandle;
		FDelegateHandle ActivatedHandle, EndedHandle;
		int32 Activations = 0, Ends = 0;
		float LastEndStamina = -1.f;
		FWindow Window;
		float PreviousStamina = -1.f;
		double LastDecrease = -1., FirstRegenDelay = -1.;
		bool bEarlyRegen = false;
	};
	class FObservations final
	{
	public:
		~FObservations() { Stop(); }
		void Start() { Handle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FObservations::Tick); }
		bool Add(ACharacter* Character)
		{
			if (!Character || !Movement(Character) || !ASC(Character) || Peers.Contains(Character->GetWorld())) return false;
			UWorld* World = Character->GetWorld();
			FPeer& Peer = Peers.Add(World); Peer.Character = Character; Peer.PreviousStamina = Stamina(Character); Peer.AbilitySystem = ASC(Character);
			if (const FGameplayAbilitySpec* Spec = SprintSpec(Character))
			{
				Peer.SprintHandle = Spec->Handle;
				Peer.ActivatedHandle = ASC(Character)->AbilityActivatedCallbacks.AddLambda([this, World](UGameplayAbility* Ability)
				{
					FPeer* Observed = Peers.Find(World);
					if (Observed && Ability && Ability->GetCurrentAbilitySpecHandle() == Observed->SprintHandle) ++Observed->Activations;
				});
				Peer.EndedHandle = ASC(Character)->OnAbilityEnded.AddLambda([this, World](const FAbilityEndedData& Ended)
				{
					FPeer* Observed = Peers.Find(World);
					if (Observed && Ended.AbilityThatEnded && Ended.AbilityThatEnded->GetCurrentAbilitySpecHandle() == Observed->SprintHandle)
					{
						++Observed->Ends; Observed->LastEndStamina = Stamina(Observed->Character.Get());
						UE_LOG(LogTemp, Display, TEXT("RpgCmcSprint abilityEnded world=%s role=%d activations=%d ends=%d cancelled=%d staminaAtReceipt=%.4f"),
							*GetPathNameSafe(World), int32(Observed->Character->GetLocalRole()), Observed->Activations, Observed->Ends, Ended.bWasCancelled, Observed->LastEndStamina);
					}
				});
			}
			return true;
		}
		void Begin(bool bSprint, bool bBlock, float InSpeed)
		{
			bExpectedSprint = bSprint; bExpectedBlock = bBlock; ExpectedSpeed = InSpeed; bSampling = true;
			for (auto& Pair : Peers) Pair.Value.Window = FWindow{};
		}
		bool Complete(int32 Count) const
		{
			if (Peers.Num() != Count) return false;
			for (const auto& Pair : Peers) if (Pair.Value.Window.Samples < 8 || Pair.Value.Window.LastTime - Pair.Value.Window.FirstTime < .65) return false;
			return true;
		}
		void End() { bSampling = false; }
		const TMap<TWeakObjectPtr<UWorld>, FPeer>& GetPeers() const { return Peers; }
		void Stop()
		{
			FWorldDelegates::OnWorldTickEnd.Remove(Handle); Handle.Reset();
			for (auto& Pair : Peers)
				if (Pair.Value.AbilitySystem.IsValid())
				{
					Pair.Value.AbilitySystem->AbilityActivatedCallbacks.Remove(Pair.Value.ActivatedHandle);
					Pair.Value.AbilitySystem->OnAbilityEnded.Remove(Pair.Value.EndedHandle);
				}
			Peers.Reset();
		}
		void Report(const TCHAR* Stage) const
		{
			for (const auto& Pair : Peers)
			{
				const FPeer& Peer = Pair.Value; const FWindow& Window = Peer.Window;
				UE_LOG(LogTemp, Display, TEXT("RpgCmcSprint stage=%s world=%s role=%d samples=%d speed=%.3f..%.3f stamina=%.3f->%.3f duration=%.4f invalid=%d regenDelay=%.4f earlyRegen=%d"),
					Stage, *GetPathNameSafe(Pair.Key.Get()), Peer.Character.IsValid() ? int32(Peer.Character->GetLocalRole()) : -1,
					Window.Samples, Window.MinimumSpeed, Window.MaximumSpeed, Window.FirstStamina, Window.LastStamina,
					Window.LastTime - Window.FirstTime, Window.bInvalid, Peer.FirstRegenDelay, Peer.bEarlyRegen);
			}
		}
	private:
		void Tick(UWorld* World, ELevelTick, float DeltaSeconds)
		{
			FPeer* Peer = Peers.Find(World);
			if (!Peer || !ActiveWorld(World) || !Peer->Character.IsValid()) return;
			ACharacter* Character = Peer->Character.Get(); const double Now = World->GetTimeSeconds();
			const float CurrentStamina = Stamina(Character);
			if (Character->HasAuthority())
			{
				if (CurrentStamina < Peer->PreviousStamina - .001f) { Peer->LastDecrease = Now; Peer->FirstRegenDelay = -1.; }
				else if (CurrentStamina > Peer->PreviousStamina + .001f && Peer->LastDecrease >= 0.)
				{
					if (Peer->FirstRegenDelay < 0.) Peer->FirstRegenDelay = Now - Peer->LastDecrease;
					Peer->bEarlyRegen |= Now - Peer->LastDecrease < 1.85;
				}
			}
			Peer->PreviousStamina = CurrentStamina;
			if (!bSampling) return;
			FWindow& Window = Peer->Window; Window.Elapsed += DeltaSeconds;
			if (Window.Elapsed < .5 || (Window.Samples >= 8 && Window.LastTime - Window.FirstTime >= .65)) return;
			const float CurrentSpeed = Movement(Character)->Velocity.Size2D();
			if (Window.Samples++ == 0) { Window.FirstTime = Now; Window.FirstStamina = CurrentStamina; }
			Window.LastTime = Now; Window.LastStamina = CurrentStamina;
			Window.MinimumStamina = FMath::Min(Window.MinimumStamina, CurrentStamina);
			Window.MinimumSpeed = FMath::Min(Window.MinimumSpeed, CurrentSpeed); Window.MaximumSpeed = FMath::Max(Window.MaximumSpeed, CurrentSpeed);
			Window.bInvalid |= !FMath::IsFinite(CurrentStamina) || !Movement(Character)->IsMovingOnGround()
				|| Movement(Character)->IsSprinting() != bExpectedSprint || Blocking(Character) != bExpectedBlock
				|| !FMath::IsNearlyEqual(CurrentSpeed, ExpectedSpeed, ExpectedSpeed > 0.f ? 15.f : 2.f);
			if (Character->GetLocalRole() != ROLE_SimulatedProxy && bExpectedSprint)
				Window.bInvalid |= !SprintSpec(Character) || !SprintSpec(Character)->IsActive() || !ASC(Character)->IsSprintMovementActive();
		}
		TMap<TWeakObjectPtr<UWorld>, FPeer> Peers;
		FDelegateHandle Handle;
		bool bSampling = false, bExpectedSprint = false, bExpectedBlock = false;
		float ExpectedSpeed = 0.f;
	};

	/** Observes the engine's real autonomous replay, including a changed endpoint at an unchanged saved-move timestamp. */
	class FCorrection final
	{
		struct FReplayEndpoint
		{
			FVector OriginalLocation = FVector::ZeroVector;
			float DeltaTime = 0.f;
		};
	public:
		~FCorrection() { Stop(); }
		bool Start(ACharacter* InCharacter)
		{
#if DO_ENABLE_NET_TEST
			UNetDriver* Driver = InCharacter && InCharacter->GetWorld() ? InCharacter->GetWorld()->GetNetDriver() : nullptr;
			if (!Driver || !Movement(InCharacter) || !ASC(InCharacter) || !SprintSpec(InCharacter)
				|| InCharacter->GetLocalRole() != ROLE_AutonomousProxy) return false;
			Character = InCharacter; LagDriver = Driver; OriginalPackets = Driver->PacketSimulationSettings;
			StartedAt = InCharacter->GetWorld()->GetTimeSeconds();
			AbilitySystem = ASC(InCharacter); SprintHandle = SprintSpec(InCharacter)->Handle;
			EndedHandle = AbilitySystem->OnAbilityEnded.AddLambda([this](const FAbilityEndedData& Ended)
			{
				if (Ended.AbilityThatEnded && Ended.AbilityThatEnded->GetCurrentAbilitySpecHandle() == SprintHandle)
					bEpochEnded = true;
			});
			FPacketSimulationSettings Settings = OriginalPackets; Settings.PktIncomingLagMin = Settings.PktIncomingLagMax = 200;
			Driver->SetPacketSimulationSettings(Settings);
			Scene = Movement(InCharacter)->UpdatedComponent;
			if (!Scene.IsValid()) { Stop(); return false; }
			TransformHandle = Scene->TransformUpdated.AddRaw(this, &FCorrection::OnTransform);
			TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FCorrection::Tick);
			return true;
#else
			return false;
#endif
		}
		bool Inject()
		{
			if (!Character.IsValid() || !Movement(Character.Get())->IsSprinting() || !ASC(Character.Get())->IsSprintMovementActive()) return false;
			bInjectionRequested = true;
			return true;
		}
		bool Complete() const { return bWitness; }
		void Report() const
		{
			UE_LOG(LogTemp, Display, TEXT("RpgCmcSprint correction injected=%d replayMoves=%d sameTimestamp=%.6f correctionCm=%.4f invalidReplay=%d witness=%d epochEnded=%d injectionTimestamp=%.6f pairedEndpoints=%d witnessFrame=%llu"),
				bInjected, ReplayTimestamps.Num(), CorrectedTimestamp, MaximumCorrection, bInvalidReplay, bWitness, bEpochEnded,
				InjectionTimestamp, PairedEndpoints, WitnessFrame);
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickEnd.Remove(TickHandle); TickHandle.Reset();
			if (Scene.IsValid()) Scene->TransformUpdated.Remove(TransformHandle);
			if (AbilitySystem.IsValid()) AbilitySystem->OnAbilityEnded.Remove(EndedHandle);
			EndedHandle.Reset(); AbilitySystem.Reset();
			TransformHandle.Reset(); Scene.Reset(); Character.Reset();
#if DO_ENABLE_NET_TEST
			if (LagDriver.IsValid()) LagDriver->SetPacketSimulationSettings(OriginalPackets);
#endif
			LagDriver.Reset();
		}
	private:
		void OnTransform(USceneComponent*, EUpdateTransformFlags, ETeleportType)
		{
			if (!bInjected || bWitness || bEpochEnded || !Character.IsValid() || !Character->bClientUpdating) return;
			const URpgCharacterMovementComponent* Move = Movement(Character.Get());
			const FSavedMove_Character* Saved = Move->GetCurrentReplayedSavedMove();
			if (!Saved || Saved->TimeStamp <= InjectionTimestamp) return;
			if (ReplayFrame != GFrameCounter) { ReplayEndpoints.Reset(); ReplayFrame = GFrameCounter; }
			if (!ReplayEndpoints.Contains(Saved->TimeStamp))
			{
				// The engine calls PostUpdate_Replay only after MoveAutonomous returns. Copy this
				// generation's original endpoint now, before that same saved object is overwritten.
				ReplayEndpoints.Add(Saved->TimeStamp, FReplayEndpoint{ Saved->SavedLocation, Saved->DeltaTime });
			}
			ReplayTimestamps.Add(Saved->TimeStamp);
			bInvalidReplay |= !(Saved->GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0)
				|| !FMath::IsNearlyEqual(Move->GetMaxSpeed(), SprintSpeed, .01f);
		}
		void Tick(UWorld* World, ELevelTick, float)
		{
			if (!bInjectionRequested || bWitness || bEpochEnded || !Character.IsValid() || Character->GetWorld() != World) return;
			const FNetworkPredictionData_Client_Character* Data = static_cast<const FNetworkPredictionData_Client_Character*>(Movement(Character.Get())->GetPredictionData_Client());
			if (!bInjected)
			{
				// Move combining reverts to PendingMove's old start. Inject only after it was sent,
				// with receipt lag established, so the next ordinary recorded move contains the error.
				// No pending/saved move, compressed flag, input or combining setting is modified.
				if (World->GetTimeSeconds() - StartedAt < .25 || Data->PendingMove.IsValid()) return;
				if (!Movement(Character.Get())->IsSprinting() || !AbilitySystem->IsSprintMovementActive()) { bEpochEnded = true; return; }
				const FVector Before = Character->GetActorLocation();
				InjectionTimestamp = Data->CurrentTimeStamp; InjectionY = Before.Y;
				Character->SetActorLocation(Before + FVector(0., 50., 0.), false, nullptr, ETeleportType::TeleportPhysics);
				bInjected = FMath::IsNearlyEqual(Character->GetActorLocation().Y - Before.Y, 50., .01);
				UE_LOG(LogTemp, Display, TEXT("RpgCmcSprint correction injection frame=%llu timestamp=%.6f pending=0 deltaY=%.4f"),
					GFrameCounter, InjectionTimestamp, Character->GetActorLocation().Y - Before.Y);
				return;
			}
			if (ReplayFrame != GFrameCounter || ReplayEndpoints.IsEmpty()) return;
			float FrameCorrection = 0.f;
			int32 FramePairs = 0;
			for (const FSavedMovePtr& Saved : Data->SavedMoves)
			{
				if (!Saved.IsValid()) continue;
				if (const FReplayEndpoint* Original = ReplayEndpoints.Find(Saved->TimeStamp);
					Original && Original->DeltaTime == Saved->DeltaTime)
				{
					++FramePairs; ++PairedEndpoints;
					const float Correction = Original->OriginalLocation.Y - Saved->SavedLocation.Y;
					FrameCorrection = FMath::Max(FrameCorrection, Correction);
					if (Original->OriginalLocation.Y - InjectionY > 1.f && Correction > MaximumCorrection)
					{
						MaximumCorrection = Correction; CorrectedTimestamp = Saved->TimeStamp;
					}
				}
			}
			if (++ReplayEpochs <= 8)
				UE_LOG(LogTemp, Display, TEXT("RpgCmcSprint correction replay frame=%llu originalEndpoints=%d pairs=%d deltaY=%.4f invalid=%d"),
					GFrameCounter, ReplayEndpoints.Num(), FramePairs, FrameCorrection, bInvalidReplay);
			bWitness = CorrectedTimestamp > InjectionTimestamp && MaximumCorrection > 1.f && !bInvalidReplay;
			if (bWitness) WitnessFrame = GFrameCounter;
			ReplayEndpoints.Reset();
		}
		TWeakObjectPtr<ACharacter> Character;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		FGameplayAbilitySpecHandle SprintHandle;
		TWeakObjectPtr<USceneComponent> Scene;
		TWeakObjectPtr<UNetDriver> LagDriver;
#if DO_ENABLE_NET_TEST
		FPacketSimulationSettings OriginalPackets;
#endif
		FDelegateHandle TransformHandle, TickHandle, EndedHandle;
		TMap<float, FReplayEndpoint> ReplayEndpoints;
		TSet<float> ReplayTimestamps;
		double StartedAt = 0., InjectionY = 0.;
		float CorrectedTimestamp = 0.f, MaximumCorrection = 0.f, InjectionTimestamp = 0.f;
		uint64 ReplayFrame = MAX_uint64, WitnessFrame = 0;
		int32 PairedEndpoints = 0, ReplayEpochs = 0;
		bool bInjectionRequested = false, bInjected = false, bInvalidReplay = false, bWitness = false, bEpochEnded = false;
	};
	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(CmcSprintPIE, "SurvivalRpg.GASP.CMC.Sprint")
{
	using FState = RpgCmcSprintNetworkTests::FState;
	RpgCmcSprintNetworkTests::FScopedWorld Isolation;
	RpgCmcSprintNetworkTests::FScopedInput Input;
	RpgCmcSprintNetworkTests::FObservations Observations;
	RpgCmcSprintNetworkTests::FCorrection Correction;
	TUniquePtr<FPIENetworkComponent<FState>> Network;
	FPrimaryAssetId PreviousExperience;
	int32 SubjectId = INDEX_NONE;
	bool bConfigured = false;
	float BlockSpeed = 0.f;
	double WaitStart = -1.;

	AFTER_EACH()
	{
		Input.Stop(); Correction.Report(); Correction.Stop();
		if (TestRunner->HasAnyErrors()) Observations.Report(TEXT("Failure"));
		Observations.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
	}
	TEST_METHOD(RealKeysDrainOnlyMovingSprintAndResumeAfterIdleAndBlock) { Queue(0); }
	TEST_METHOD(LateJoinReceivesEffectiveSprintWithoutOwningTheAbility) { Queue(1); }
	TEST_METHOD(AuthoritativeCorrectionReplaysAuthorizedSprintHistory) { Queue(2); }
	TEST_METHOD(ExhaustionAndDelayedAuthorityEndConsumeTheHoldUntilRelease) { Queue(3); }

	void Queue(int32 Scenario)
	{
		using namespace RpgCmcSprintNetworkTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) { TestRunner->AddError(TEXT("Sprint automation refuses to interrupt existing PIE.")); return; }
		UClass* Mode = LoadClass<ARpgGameModeBase>(nullptr, TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C"));
		ASSERT_THAT(IsNotNull(Mode)); if (!Mode) return;
		Isolation.Start(); PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId(); bConfigured = true;
		Network = MakeUnique<FPIENetworkComponent<FState>>(TestRunner, TestCommandBuilder, bInitializing);
		FNetworkComponentBuilder<FState>().WithClients(Scenario == 1 ? 1 : 2).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C"))).WithGameMode(Mode).Build(*Network);
		Network->UntilClient(TEXT("Owner has the actual CMC Sprint Experience, GAS grant and input"), 0, [](FState& State)
			{ return Ready(State.World, LocalPawn(State.World)) && SprintSpec(LocalPawn(State.World)); }, Timeout())
			.ThenClient(TEXT("Capture real owning keyboard input"), 0, [this](FState& State)
			{
				ACharacter* Character = LocalPawn(State.World); SubjectId = Character->GetPlayerState()->GetPlayerId();
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy && Isolation.IsIsolated(State.World)));
				ASSERT_THAT(IsTrue(Input.Start(Character)));
				Observations.Start(); ASSERT_THAT(IsTrue(Observations.Add(Character)));
			})
			.UntilServer(TEXT("Authority resolves the same CMC input and equipment composition"), [this](FState& State)
			{
				ACharacter* Character = Pawn(State.World, SubjectId);
				const URpgEquipmentManagerComponent* Equipment = Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
				return Ready(State.World, Character) && SprintSpec(Character) && Equipment && Cast<URpgWeaponInstance>(Equipment->GetActiveBlockSource());
			}, Timeout())
			.ThenServer(TEXT("Observe authority without modifying stamina or movement tuning"), [this](FState& State)
			{
				ACharacter* Character = Pawn(State.World, SubjectId);
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World) && Observations.Add(Character)));
				const URpgEquipmentManagerComponent* Equipment = Character->FindComponentByClass<URpgEquipmentManagerComponent>();
				BlockSpeed = CastChecked<URpgWeaponInstance>(Equipment->GetActiveBlockSource())->GetBlockDefinition().MovementSpeedLimit;
				ASSERT_THAT(IsTrue(BlockSpeed > 0.f && BlockSpeed < RunSpeed));
				const URpgStaminaSet* StaminaSet = ASC(Character)->GetSet<URpgStaminaSet>();
				FString Effects;
				for (const FActiveGameplayEffectHandle& Handle : ASC(Character)->GetActiveEffects(FGameplayEffectQuery{}))
					if (const FActiveGameplayEffect* Effect = ASC(Character)->GetActiveGameplayEffect(Handle))
						Effects += GetPathNameSafe(Effect->Spec.Def) + TEXT(";");
				UE_LOG(LogTemp, Display, TEXT("RpgCmcSprint startup world=%s set=%s stamina=%.4f regen=%.4f activeEffects=%s"),
					*GetPathNameSafe(State.World), *GetPathNameSafe(StaminaSet), StaminaSet->GetStamina(), StaminaSet->GetStaminaRegen(), *Effects);
				TestRunner->TestTrue(FString::Printf(TEXT("Authority receives the authored regen rate 12 (actual %.4f)"), StaminaSet->GetStaminaRegen()),
					FMath::IsNearlyEqual(StaminaSet->GetStaminaRegen(), 12.f));
			});
		if (Scenario != 1) QueueObserver(false);
		if (Scenario == 0) { QueueResources(); return; }
		if (Scenario == 3) { QueueExhaustion(); return; }
		Network->ThenClient(TEXT("Hold actual forward and sprint keys"), 0, [this](FState&) { Input.Key(EKeys::W, true); Input.Key(EKeys::LeftShift, true); });
		QueueWindow(TEXT("SprintBeforeSpecialCase"), true, false, SprintSpeed, Scenario == 1 ? 2 : 3, true);
		if (Scenario == 1)
		{
			Network->ThenClientJoins(); QueueObserver(true);
			QueueWindow(TEXT("LateJoinActiveSprint"), true, false, SprintSpeed, 3, true);
		}
		else
		{
			Network->ThenClient(TEXT("Add bounded receipt delay and one local 50cm prediction error"), 0, [this](FState&)
				{ ASSERT_THAT(IsTrue(Correction.Start(Input.Character()))); ASSERT_THAT(IsTrue(Correction.Inject())); })
				.UntilClient(TEXT("Real server correction replays an unchanged timestamp and removes the injected error"), 0, [this](FState&) { return Correction.Complete(); }, Timeout())
				.ThenClient(TEXT("Restore packet settings after the concrete replay witness"), 0, [this](FState&) { Correction.Report(); Correction.Stop(); });
			QueueWindow(TEXT("SprintAfterRealCorrection"), true, false, SprintSpeed, 3, true);
		}
		QueueRelease();
	}
	void QueueObserver(bool bLate)
	{
		using namespace RpgCmcSprintNetworkTests;
		Network->UntilClient(TEXT("Observer receives the selected subject and replicated effective sprint"), 1, [this, bLate](FState& State)
			{
				ACharacter* Character = Pawn(State.World, SubjectId);
				return Ready(State.World, Character) && Character->GetLocalRole() == ROLE_SimulatedProxy && (!bLate || Movement(Character)->IsSprinting());
			}, Timeout())
			.ThenClient(TEXT("Register the real simulated proxy without pressing its keys"), 1, [this, bLate](FState& State)
			{
				ACharacter* Character = Pawn(State.World, SubjectId);
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World) && Observations.Add(Character)));
				ASSERT_THAT(IsFalse(ASC(Character)->IsSprintMovementActive()));
				if (bLate) ASSERT_THAT(IsTrue(Movement(Character)->IsSprinting()));
			});
	}
	void QueueWindow(const TCHAR* Label, bool bSprint, bool bBlock, float Speed, int32 Count, bool bDrain)
	{
		using namespace RpgCmcSprintNetworkTests;
		const FString Stage(Label);
		Network->ThenServer(TEXT("Start a bounded settled observation window"), [this, bSprint, bBlock, Speed](FState&)
			{ Observations.Begin(bSprint, bBlock, Speed); })
			.UntilServer(TEXT("All roles retain enough actual movement and resource samples"), [this, Count](FState&) { return Observations.Complete(Count); }, Timeout())
			.ThenServer(TEXT("Check real movement, replicated gait and authority-only resource change"), [this, Stage, bDrain](FState&)
			{
				Observations.End(); Observations.Report(*Stage);
				for (const auto& Pair : Observations.GetPeers())
				{
					const FPeer& Peer = Pair.Value; const FWindow& Window = Peer.Window;
					TestRunner->TestFalse(Stage + TEXT(": every settled role keeps the expected gait/speed/block state"), Window.bInvalid);
					if (!Peer.Character.IsValid() || !Peer.Character->HasAuthority()) continue;
					if (bDrain)
					{
						const double Rate = (Window.FirstStamina - Window.LastStamina) / (Window.LastTime - Window.FirstTime);
						TestRunner->TestTrue(Stage + TEXT(": authority consumes the authored 15 stamina/s while moving"), FMath::Abs(Rate - DrainRate) < 3.0);
					}
					else TestRunner->TestTrue(Stage + TEXT(": non-sprint movement never consumes stamina"), Window.MinimumStamina >= Window.FirstStamina - .05f);
					TestRunner->TestFalse(TEXT("Regeneration never starts before its two-second recovery pause"), Peer.bEarlyRegen);
				}
			});
	}
	void QueueResources()
	{
		using namespace RpgCmcSprintNetworkTests;
		Network->ThenClient(TEXT("Holding Shift while idle acquires authorization without cost"), 0, [this](FState&) { Input.Key(EKeys::LeftShift, true); });
		QueueWindow(TEXT("HeldShiftIdle"), false, false, 0.f, 3, false);
		Network->ThenServer(TEXT("Idle retains the real GAS authorization without becoming effective sprint"), [this](FState&)
			{
				for (const auto& Pair : Observations.GetPeers())
					if (Pair.Value.Character->GetLocalRole() != ROLE_SimulatedProxy)
						ASSERT_THAT(IsTrue(ASC(Pair.Value.Character.Get())->IsSprintMovementActive()));
			})
			.ThenClient(TEXT("Real movement resumes the held sprint lease"), 0, [this](FState&) { Input.Key(EKeys::W, true); });
		QueueWindow(TEXT("MovingSprint"), true, false, SprintSpeed, 3, true);
		Network->ThenClient(TEXT("Hold RMB without releasing Shift or W"), 0, [this](FState&) { Input.Key(EKeys::RightMouseButton, true); })
			.ThenServer(TEXT("Sample the actual equipment block cap"), [this](FState&) { Observations.Begin(false, true, BlockSpeed); })
			.UntilServer(TEXT("Block overrides sprint on all roles"), [this](FState&) { return Observations.Complete(3); }, Timeout())
			.ThenServer(TEXT("The held sprint authorization survives block without spending stamina"), [this](FState&)
			{
				Observations.End(); Observations.Report(TEXT("BlockHeldShift"));
				for (const auto& Pair : Observations.GetPeers())
				{
					const FPeer& Peer = Pair.Value;
					ASSERT_THAT(IsFalse(Peer.Window.bInvalid));
					if (Peer.Character->GetLocalRole() != ROLE_SimulatedProxy) ASSERT_THAT(IsTrue(ASC(Peer.Character.Get())->IsSprintMovementActive()));
					if (Peer.Character->HasAuthority()) ASSERT_THAT(IsTrue(Peer.Window.MinimumStamina >= Peer.Window.FirstStamina - .05f));
				}
			})
			.ThenClient(TEXT("Release only RMB to resume the still-held Shift"), 0, [this](FState&) { Input.Key(EKeys::RightMouseButton, false); });
		QueueWindow(TEXT("ResumeHeldSprint"), true, false, SprintSpeed, 3, true);
		QueueRelease();
		Network->ThenServer(TEXT("Wait for actual delayed authority regeneration"), [this](FState& State) { WaitStart = State.World->GetTimeSeconds(); })
			.UntilServer(TEXT("Stamina regains at least one authored recovery increment"), [this](FState& State)
			{
				const FPeer* Peer = Observations.GetPeers().Find(State.World);
				return Peer && Peer->FirstRegenDelay >= 0. && State.World->GetTimeSeconds() - WaitStart >= .5;
			}, Timeout())
			.ThenServer(TEXT("The recovery pause and replicated stamina remain consistent"), [this](FState& State)
			{
				const FPeer* Authority = Observations.GetPeers().Find(State.World);
				ASSERT_THAT(IsNotNull(Authority)); if (!Authority) return;
				ASSERT_THAT(IsFalse(Authority->bEarlyRegen));
				ASSERT_THAT(IsTrue(Authority->FirstRegenDelay >= 1.85 && Authority->FirstRegenDelay <= 2.3));
				for (const auto& Pair : Observations.GetPeers())
					ASSERT_THAT(IsTrue(FMath::Abs(Stamina(Pair.Value.Character.Get()) - Stamina(Authority->Character.Get())) < 4.f));
			});
	}
	void QueueRelease()
	{
		using namespace RpgCmcSprintNetworkTests;
		Network->ThenClient(TEXT("Release real Shift while forward movement continues"), 0, [this](FState&) { Input.Key(EKeys::LeftShift, false); });
		QueueWindow(TEXT("ReleaseReturnsToNormalRun"), false, false, RunSpeed, 3, false);
		Network->ThenServer(TEXT("Authority and owner retired the sprint activation"), [this](FState&)
			{
				for (const auto& Pair : Observations.GetPeers()) ASSERT_THAT(IsFalse(ASC(Pair.Value.Character.Get())->IsSprintMovementActive()));
			})
			.ThenClient(TEXT("Stop forward input normally"), 0, [this](FState&) { Input.Key(EKeys::W, false); });
	}
	void QueueRepress()
	{
		Network->ThenClient(TEXT("Physically release Shift before the next activation"), 0, [this](FState& State)
			{ Input.Key(EKeys::LeftShift, false); WaitStart = State.World->GetTimeSeconds(); })
			.UntilClient(TEXT("Enhanced Input processes a real release frame"), 0, [this](FState& State)
				{ return State.World->GetTimeSeconds() - WaitStart >= .15; }, RpgCmcSprintNetworkTests::Timeout())
			.ThenClient(TEXT("A fresh Shift press may activate sprint again"), 0, [this](FState&) { Input.Key(EKeys::LeftShift, true); });
	}
	void QueueExhaustion()
	{
		using namespace RpgCmcSprintNetworkTests;
		Network->ThenServer(TEXT("Set only this transient authority pawn's initial stamina reserve to five"), [this](FState& State)
			{ ASC(Pawn(State.World, SubjectId))->SetNumericAttributeBase(URpgStaminaSet::GetStaminaAttribute(), 5.f); })
			.UntilClient(TEXT("Owner receives the short initial reserve before real input"), 0, [this](FState&)
				{ return FMath::IsNearlyEqual(Stamina(Input.Character()), 5.f, .01f); }, Timeout())
			.ThenClient(TEXT("Drain the actual reserve through ordinary Shift and W"), 0, [this](FState&)
				{ Input.Key(EKeys::W, true); Input.Key(EKeys::LeftShift, true); })
			.UntilServer(TEXT("Authority and owner end the exhausted activation while Shift stays held"), [this](FState&)
			{
				for (const auto& Pair : Observations.GetPeers())
				{
					const FPeer& Peer = Pair.Value;
					if (Peer.Character->GetLocalRole() == ROLE_SimulatedProxy) continue;
					if (Peer.Ends < 1 || ASC(Peer.Character.Get())->IsSprintMovementActive()) return false;
				}
				return true;
			}, Timeout())
			.ThenServer(TEXT("The exhaustion end follows actual depletion, not a failed activation"), [this](FState& State)
			{
				const FPeer* Authority = Observations.GetPeers().Find(State.World);
				ASSERT_THAT(IsTrue(Authority && Authority->LastEndStamina >= 0.f && Authority->LastEndStamina <= .01f));
			})
			.UntilServer(TEXT("Real regeneration restores a reserve without releasing the held key"), [this](FState&)
			{
				for (const auto& Pair : Observations.GetPeers()) if (Stamina(Pair.Value.Character.Get()) < 35.f) return false;
				return true;
			}, Timeout());
		QueueWindow(TEXT("RegeneratedButHeldExhaustedInput"), false, false, RunSpeed, 3, false);
		Network->ThenServer(TEXT("No repeated activation occurs while the exhausted physical hold remains"), [this](FState&)
			{
				for (const auto& Pair : Observations.GetPeers())
					if (Pair.Value.Character->GetLocalRole() != ROLE_SimulatedProxy)
					{
						ASSERT_THAT(AreEqual(1, Pair.Value.Activations)); ASSERT_THAT(AreEqual(1, Pair.Value.Ends));
						ASSERT_THAT(IsFalse(ASC(Pair.Value.Character.Get())->IsSprintMovementActive()));
					}
			});
		QueueRepress(); QueueWindow(TEXT("FreshPressAfterExhaustion"), true, false, SprintSpeed, 3, true);
		Network->ThenClient(TEXT("Delay only owner receipt before an authoritative cancellation with positive stamina"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(Correction.Start(Input.Character()))); WaitStart = Input.Character()->GetWorld()->GetTimeSeconds(); })
			.UntilClient(TEXT("Bounded incoming delay is established"), 0, [this](FState& State) { return State.World->GetTimeSeconds() - WaitStart >= .25; }, Timeout())
			.ThenServer(TEXT("Cancel the exact active sprint while its replicated reserve remains positive"), [this](FState& State)
			{
				ACharacter* Character = Pawn(State.World, SubjectId); FGameplayAbilitySpec* Spec = SprintSpec(Character);
				ASSERT_THAT(IsTrue(Spec && Spec->IsActive() && Stamina(Character) > 5.f));
				if (Spec) ASC(Character)->CancelAbilityHandle(Spec->Handle);
			})
			.UntilClient(TEXT("The original owner activation receives that real authority end"), 0, [this](FState& State)
			{
				const FPeer* Peer = Observations.GetPeers().Find(State.World);
				return Peer && Peer->Ends >= 2;
			}, Timeout())
			.ThenClient(TEXT("The owner end arrived before any zero-stamina gate could suppress input"), 0, [this](FState& State)
			{
				const FPeer* Peer = Observations.GetPeers().Find(State.World);
				ASSERT_THAT(IsTrue(Peer && Peer->LastEndStamina > 0.f)); Correction.Stop();
			});
		QueueWindow(TEXT("AuthorityEndedHeldPositiveReserve"), false, false, RunSpeed, 3, false);
		Network->ThenServer(TEXT("Cancellation also consumes the physical hold exactly once"), [this](FState&)
			{
				for (const auto& Pair : Observations.GetPeers())
					if (Pair.Value.Character->GetLocalRole() != ROLE_SimulatedProxy)
					{
						ASSERT_THAT(AreEqual(2, Pair.Value.Activations)); ASSERT_THAT(AreEqual(2, Pair.Value.Ends));
						ASSERT_THAT(IsFalse(ASC(Pair.Value.Character.Get())->IsSprintMovementActive()));
					}
			});
		QueueRepress(); QueueWindow(TEXT("FreshPressAfterAuthorityEnd"), true, false, SprintSpeed, 3, true);
		QueueRelease();
	}
};
#endif
#endif
