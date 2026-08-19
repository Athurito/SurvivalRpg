#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgGaspNetworkTestTypes.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspPIENetworkTests
{
	constexpr TCHAR PilotExperienceName[] = TEXT("RpgGaspPilotExperience");
	constexpr TCHAR PilotGameModeClassPath[] =
		TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameMode.BP_Rpg_GameMode_C");
	constexpr TCHAR PilotCharacterClassPrefix[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/BP_Rpg_Character_GASP");
	constexpr TCHAR RootMotionAttackPath[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01");
	const FName DefaultSlotName(TEXT("DefaultSlot"));

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgGaspNetworkFloorFixture* Floor = nullptr;
		int32 SubjectPlayerId = INDEX_NONE;
		FTimerHandle MovementInputTimer;
		FTimerHandle ObservationTimer;
		FVector MovementInputDirection = FVector::ZeroVector;
		FVector MontageStartLocation = FVector::ZeroVector;
		double MontageConvergenceStartTime = 0.0;
		float MaximumMontageDisplacement = 0.0f;
		bool bTrackMontageDisplacement = false;
		bool bSawPivot = false;
		bool bSawAirborne = false;
		bool bSawLanding = false;
		bool bSawGroundedAfterAirborne = false;
		bool bSawAim = false;
		bool bSawTurnInPlace = false;
		bool bTurnObservationArmed = false;
		double TurnObservationBaselineYaw = 0.0;
		bool bSawDefaultSlotMontage = false;
		bool bSawMontageAnimGate = false;
	};

	TArray<FNetworkState*> ActiveTimerStates;

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(90.0);
	}

	ARpgCharacter* FindLocalCharacter(UWorld* World)
	{
		const APlayerController* PlayerController = IsValid(World)
			? World->GetFirstPlayerController()
			: nullptr;
		return PlayerController ? Cast<ARpgCharacter>(PlayerController->GetPawn()) : nullptr;
	}

	ARpgCharacter* FindCharacterByPlayerId(UWorld* World, const int32 PlayerId)
	{
		if (!IsValid(World) || PlayerId == INDEX_NONE)
		{
			return nullptr;
		}

		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
		{
			const APlayerState* PlayerState = It->GetPlayerState();
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	URpgAnimInstance* GetPilotAnimInstance(ARpgCharacter* Character)
	{
		USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
		return Mesh ? Cast<URpgAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	}

	template <typename TValue>
	bool ReadAnimProperty(
		URpgAnimInstance* AnimInstance,
		const FName PropertyName,
		TValue& OutValue)
	{
		if (!IsValid(AnimInstance))
		{
			return false;
		}

		USkeletalMeshComponent* Mesh = AnimInstance->GetSkelMeshComponent();
		if (!IsValid(Mesh))
		{
			return false;
		}

		// The reflected values are worker-owned. Complete the current evaluation before
		// reading them on the editor test's game thread instead of adding test-only getters.
		Mesh->HandleExistingParallelEvaluationTask(true, true);
		FProperty* Property = FindFProperty<FProperty>(AnimInstance->GetClass(), PropertyName);
		if (!Property || Property->GetSize() != sizeof(TValue))
		{
			return false;
		}

		const void* Source = Property->ContainerPtrToValuePtr<void>(AnimInstance);
		Property->CopyCompleteValue(&OutValue, Source);
		return true;
	}

	bool IsPilotExperienceReady(FNetworkState& State, const int32 ExpectedClients)
	{
		if (!IsValid(State.World))
		{
			return false;
		}

		const ENetMode NetMode = State.World->GetNetMode();
		const bool bExpectedWorld = State.World->GetNetDriver() &&
			((NetMode == NM_ListenServer &&
				State.World->GetNetDriver()->ClientConnections.Num() >= ExpectedClients) ||
			 NetMode == NM_Client);
		const AGameStateBase* GameState = State.World->GetGameState();
		const URpgExperienceManagerComponent* ExperienceManager = GameState
			? GameState->FindComponentByClass<URpgExperienceManagerComponent>()
			: nullptr;
		if (!GameState || !ExperienceManager || !ExperienceManager->IsExperienceLoaded())
		{
			return false;
		}

		const URpgExperienceDefinition* Experience =
			ExperienceManager->GetCurrentExperienceChecked();
		const FPrimaryAssetId ExpectedExperienceId(
			URpgExperienceDefinition::StaticClass()->GetFName(),
			PilotExperienceName);
		return bExpectedWorld && Experience &&
			Experience->GetPrimaryAssetId() == ExpectedExperienceId;
	}

	bool IsPilotCharacterReady(ARpgCharacter* Character)
	{
		return IsValid(Character) && Character->GetClass()->GetPathName().StartsWith(
			PilotCharacterClassPrefix) &&
			IsValid(Character->GetPlayerState()) &&
			IsValid(Character->GetRpgAbilitySystemComponent()) &&
			IsValid(GetPilotAnimInstance(Character));
	}

	bool HasRoleCorrectFootPlacement(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole,
		const ENetRole ExpectedRemoteRole)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		FRpgFootPlacementSnapshot Snapshot;
		return Character && Character->GetLocalRole() == ExpectedLocalRole &&
			Character->GetRemoteRole() == ExpectedRemoteRole &&
			ReadAnimProperty(AnimInstance, TEXT("FootPlacementSnapshot"), Snapshot) &&
			Snapshot.bValid && Snapshot.bGrounded &&
			Snapshot.LocalRole == static_cast<uint8>(ExpectedLocalRole) &&
			Snapshot.RemoteRole == static_cast<uint8>(ExpectedRemoteRole);
	}

	bool HasMovingAnimation(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole,
		const FVector& ExpectedDirection)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		if (!Character || Character->GetLocalRole() != ExpectedLocalRole || !AnimInstance)
		{
			return false;
		}

		FVector WorldAcceleration = FVector::ZeroVector;
		float GroundSpeed = 0.0f;
		bool bHasAcceleration = false;
		const FVector MovementAcceleration = Character->GetCharacterMovement()
			? Character->GetCharacterMovement()->GetCurrentAcceleration()
			: FVector::ZeroVector;
		const FVector Direction2D = ExpectedDirection.GetSafeNormal2D();
		return ReadAnimProperty(
				AnimInstance,
				TEXT("WorldAcceleration"),
				WorldAcceleration) &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("LocomotionGroundSpeed"),
				GroundSpeed) &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("bHasAcceleration"),
				bHasAcceleration) &&
			bHasAcceleration && GroundSpeed > 50.0f &&
			MovementAcceleration.Size2D() > 50.0f &&
			WorldAcceleration.Size2D() > 50.0f &&
			FVector::DotProduct(
				MovementAcceleration.GetSafeNormal2D(),
				Direction2D) > 0.75f &&
			FVector::DotProduct(
				WorldAcceleration.GetSafeNormal2D(),
				Direction2D) > 0.75f;
	}

	bool HasStoppedAnimation(FNetworkState& State)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		if (!Character || !AnimInstance || !Character->GetCharacterMovement())
		{
			return false;
		}

		FVector WorldAcceleration = FVector::ZeroVector;
		float GroundSpeed = 0.0f;
		bool bHasAcceleration = true;
		return ReadAnimProperty(
				AnimInstance,
				TEXT("WorldAcceleration"),
				WorldAcceleration) &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("LocomotionGroundSpeed"),
				GroundSpeed) &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("bHasAcceleration"),
				bHasAcceleration) &&
			!bHasAcceleration && WorldAcceleration.Size2D() <= 5.0f &&
			Character->GetCharacterMovement()->GetCurrentAcceleration().Size2D() <= 5.0f &&
			GroundSpeed <= 8.0f && Character->GetVelocity().Size2D() <= 8.0f;
	}

	bool HasRotationMode(FNetworkState& State, const ERpgCharacterRotationMode ExpectedMode)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		ERpgCharacterRotationMode AnimMode = ERpgCharacterRotationMode::Free;
		return Character && Character->GetRotationMode() == ExpectedMode &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("CharacterRotationMode"),
				AnimMode) &&
			AnimMode == ExpectedMode;
	}

	bool HasCrouchState(FNetworkState& State, const bool bExpectedCrouched)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		ERpgLocomotionStance Stance = ERpgLocomotionStance::Standing;
		const ERpgLocomotionStance ExpectedStance = bExpectedCrouched
			? ERpgLocomotionStance::Crouching
			: ERpgLocomotionStance::Standing;
		return Character && Character->IsCrouched() == bExpectedCrouched &&
			ReadAnimProperty(AnimInstance, TEXT("LocomotionStance"), Stance) &&
			Stance == ExpectedStance;
	}

	bool IsTurnInPlaceInactive(FNetworkState& State)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		ERpgTurnInPlaceState TurnState = ERpgTurnInPlaceState::Active;
		return ReadAnimProperty(
			GetPilotAnimInstance(Character),
			TEXT("TurnInPlaceState"),
			TurnState) && TurnState == ERpgTurnInPlaceState::Inactive;
	}

	bool IsExpectedDefaultSlotMontage(const UAnimMontage* Montage)
	{
		return IsValid(Montage) && Montage->IsDynamicMontage() &&
			Montage->GetFirstAnimReference() &&
			Montage->GetFirstAnimReference()->GetPathName() == RootMotionAttackPath &&
			Montage->SlotAnimTracks.Num() == 1 &&
			Montage->SlotAnimTracks[0].SlotName == DefaultSlotName;
	}

	void ObserveState(FNetworkState& State)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		if (!Character || !AnimInstance)
		{
			return;
		}

		FVector WorldAcceleration = FVector::ZeroVector;
		FVector WorldVelocity = FVector::ZeroVector;
		if (ReadAnimProperty(
				AnimInstance,
				TEXT("WorldAcceleration"),
				WorldAcceleration) &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("WorldVelocity"),
				WorldVelocity) &&
			FVector::DotProduct(WorldAcceleration.GetSafeNormal2D(),
				WorldVelocity.GetSafeNormal2D()) < -0.25f)
		{
			State.bSawPivot = true;
		}

		ERpgJumpPhase JumpPhase = ERpgJumpPhase::Grounded;
		if (ReadAnimProperty(AnimInstance, TEXT("JumpPhase"), JumpPhase))
		{
			State.bSawAirborne |= JumpPhase == ERpgJumpPhase::Airborne;
			State.bSawLanding |= JumpPhase == ERpgJumpPhase::Landing;
			State.bSawGroundedAfterAirborne |=
				State.bSawAirborne && JumpPhase == ERpgJumpPhase::Grounded &&
				Character->GetCharacterMovement() &&
				Character->GetCharacterMovement()->IsMovingOnGround();
		}

		ERpgCharacterRotationMode RotationMode = ERpgCharacterRotationMode::Free;
		State.bSawAim |= ReadAnimProperty(
			AnimInstance,
			TEXT("CharacterRotationMode"),
			RotationMode) && RotationMode == ERpgCharacterRotationMode::Aim;

		ERpgTurnInPlaceState TurnState = ERpgTurnInPlaceState::Inactive;
		const bool bObservedStimulus = State.bTurnObservationArmed &&
			FMath::Abs(FMath::FindDeltaAngleDegrees(
				State.TurnObservationBaselineYaw,
				Character->GetActorRotation().Yaw)) >= 45.0f;
		State.bSawTurnInPlace |= bObservedStimulus && ReadAnimProperty(
			AnimInstance,
			TEXT("TurnInPlaceState"),
			TurnState) && TurnState == ERpgTurnInPlaceState::Active;

		if (UAbilitySystemComponent* AbilitySystem =
			Character->GetRpgAbilitySystemComponent())
		{
			State.bSawDefaultSlotMontage |= IsExpectedDefaultSlotMontage(
				AbilitySystem->GetCurrentMontage());
		}

		bool bIsAnyMontagePlaying = false;
		State.bSawMontageAnimGate |= ReadAnimProperty(
			AnimInstance,
			TEXT("bIsAnyMontagePlaying"),
			bIsAnyMontagePlaying) && bIsAnyMontagePlaying;

		if (State.bTrackMontageDisplacement)
		{
			State.MaximumMontageDisplacement = FMath::Max(
				State.MaximumMontageDisplacement,
				static_cast<float>(FVector::Dist2D(
					Character->GetActorLocation(),
					State.MontageStartLocation)));
		}
	}

	void StartObservation(FNetworkState& State)
	{
		if (!IsValid(State.World) || State.SubjectPlayerId == INDEX_NONE ||
			State.World->GetTimerManager().IsTimerActive(State.ObservationTimer))
		{
			return;
		}

		State.World->GetTimerManager().SetTimer(
			State.ObservationTimer,
			FTimerDelegate::CreateLambda([&State]()
			{
				ObserveState(State);
			}),
			0.01f,
			true);
		ActiveTimerStates.AddUnique(&State);
	}

	void StartMovementInput(FNetworkState& State, const FVector& Direction)
	{
		if (!IsValid(State.World))
		{
			return;
		}

		State.MovementInputDirection = Direction.GetSafeNormal2D();
		if (State.World->GetTimerManager().IsTimerActive(State.MovementInputTimer))
		{
			return;
		}

		State.World->GetTimerManager().SetTimer(
			State.MovementInputTimer,
			FTimerDelegate::CreateLambda([&State]()
			{
				ARpgCharacter* Character = FindCharacterByPlayerId(
					State.World,
					State.SubjectPlayerId);
				if (Character && Character->IsLocallyControlled())
				{
					Character->AddMovementInput(State.MovementInputDirection, 1.0f);
				}
			}),
			0.01f,
			true);
		ActiveTimerStates.AddUnique(&State);
	}

	void StopMovementInput(FNetworkState& State)
	{
		if (IsValid(State.World))
		{
			State.World->GetTimerManager().ClearTimer(State.MovementInputTimer);
		}
		State.MovementInputDirection = FVector::ZeroVector;
	}

	void ClearActiveTimers()
	{
		for (FNetworkState* State : ActiveTimerStates)
		{
			if (State && IsValid(State->World))
			{
				State->World->GetTimerManager().ClearTimer(State->MovementInputTimer);
				State->World->GetTimerManager().ClearTimer(State->ObservationTimer);
			}
		}
		ActiveTimerStates.Reset();
	}
}

NETWORK_TEST_CLASS(GaspPilotPIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgGaspPIENetworkTests::FNetworkState;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	FPacketSimulationSettings PacketSettings;
	FPrimaryAssetId OriginalExperienceOverride;
	FVector AuthorityCorrectionBaseline = FVector::ZeroVector;
	FVector AuthorityMontageEnd = FVector::ZeroVector;
	double CorrectionStartTime = 0.0;
	int32 SubjectPlayerId = INDEX_NONE;
	bool bOriginalDiskPersistence = true;
	UClass* PilotGameModeClass = nullptr;

	BEFORE_EACH()
	{
		using namespace RpgGaspPIENetworkTests;
		TestCommandBuilder.OnTearDown(
			TEXT("Stop GASP network test timers"),
			[]()
			{
				ClearActiveTimers();
			});

		URpgDeveloperSettings* DeveloperSettings =
			GetMutableDefault<URpgDeveloperSettings>();
		OriginalExperienceOverride = DeveloperSettings->ExperienceOverride;
		PilotGameModeClass = LoadClass<ARpgGameModeBase>(
			nullptr,
			PilotGameModeClassPath);
		ASSERT_THAT(IsNotNull(PilotGameModeClass));
		ARpgGameModeBase* GameModeDefaults = CastChecked<ARpgGameModeBase>(
			PilotGameModeClass->GetDefaultObject());
		bOriginalDiskPersistence = GameModeDefaults->bEnableDiskPersistence;
		DeveloperSettings->ExperienceOverride = FPrimaryAssetId(
			URpgExperienceDefinition::StaticClass()->GetFName(),
			PilotExperienceName);
		GameModeDefaults->bEnableDiskPersistence = false;

		PacketSettings = FPacketSimulationSettings();
		PacketSettings.PktLag = 60;
		PacketSettings.PktLagVariance = 10;
		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithPacketSimulationSettings(&PacketSettings)
			.WithGameInstanceClass(FSoftClassPath(
				TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(PilotGameModeClass)
			.Build(Network);
	}

	AFTER_EACH()
	{
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride =
			OriginalExperienceOverride;
		if (IsValid(PilotGameModeClass))
		{
			CastChecked<ARpgGameModeBase>(PilotGameModeClass->GetDefaultObject())
				->bEnableDiskPersistence = bOriginalDiskPersistence;
		}
	}

	TEST_METHOD(ReplicationLateJoinCorrectionAndDefaultSlotMontage)
	{
		using namespace RpgGaspPIENetworkTests;

		Network
			.SpawnAndReplicate<
				ARpgGaspNetworkFloorFixture,
				&FNetworkState::Floor>(
				NetworkTimeout())
			.UntilServer(
				TEXT("Pilot Experience loads on the listen server"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Pilot Experience and GASP pawn load on the initial client"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Place the initial pawns on separate floor lanes"),
				[](FNetworkState& State)
				{
					int32 LaneIndex = 0;
					for (TActorIterator<ARpgCharacter> It(State.World); It; ++It)
					{
						ARpgCharacter* Character = *It;
						const FVector Location(
							-800.0 + static_cast<double>(LaneIndex++) * 800.0,
							0.0,
							100.0);
						Character->TeleportTo(Location, FRotator::ZeroRotator);
						Character->GetCharacterMovement()->StopMovementImmediately();
						Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
						Character->ForceNetUpdate();
					}
				})
			.UntilClient(
				TEXT("Initial client owns a grounded GASP pawn"),
				0,
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					return IsPilotCharacterReady(Character) &&
						Character->GetLocalRole() == ROLE_AutonomousProxy &&
						Character->GetCharacterMovement()->IsMovingOnGround();
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Capture the autonomous pawn identity"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(Character->GetPlayerState()));
					SubjectPlayerId = Character->GetPlayerState()->GetPlayerId();
					ASSERT_THAT(IsTrue(SubjectPlayerId != INDEX_NONE));
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.ThenServer(
				TEXT("Bind the remote autonomous subject on the listen server"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
					StartObservation(State);
				})
			.ThenClient(
				TEXT("Start owner observations and forward movement"),
				0,
				[](FNetworkState& State)
				{
					StartObservation(State);
					StartMovementInput(State, FVector::YAxisVector);
				})
			.UntilClient(
				TEXT("Autonomous AnimInstance consumes local acceleration"),
				0,
				[](FNetworkState& State)
				{
					return HasMovingAnimation(
						State,
						ROLE_AutonomousProxy,
						FVector::YAxisVector);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Listen server consumes the remote autonomous move"),
				[](FNetworkState& State)
				{
					return HasMovingAnimation(
						State,
						ROLE_Authority,
						FVector::YAxisVector);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilClient(
				TEXT("Late client receives the replicated collision floor"),
				1,
				[](FNetworkState& State)
				{
					return IsValid(State.Floor);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Late join establishes the second listen-server connection"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late client loads the Pilot Experience and pawn"),
				1,
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 2) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Bind the original subject in both client worlds"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
					StartObservation(State);
				})
			.UntilClient(
				TEXT("Late join reconstructs moving simulated-proxy acceleration"),
				1,
				[](FNetworkState& State)
				{
					return HasMovingAnimation(
						State,
						ROLE_SimulatedProxy,
						FVector::YAxisVector);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Authority Foot Placement snapshot records remote ownership"),
				[](FNetworkState& State)
				{
					return HasRoleCorrectFootPlacement(
						State,
						ROLE_Authority,
						ROLE_AutonomousProxy);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner Foot Placement snapshot records autonomous role"),
				0,
				[](FNetworkState& State)
				{
					return HasRoleCorrectFootPlacement(
						State,
						ROLE_AutonomousProxy,
						ROLE_Authority);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late join Foot Placement snapshot records simulated role"),
				1,
				[](FNetworkState& State)
				{
					return HasRoleCorrectFootPlacement(
						State,
						ROLE_SimulatedProxy,
						ROLE_Authority);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Reverse the autonomous movement input"),
				0,
				[](FNetworkState& State)
				{
					State.MovementInputDirection = -FVector::YAxisVector;
				})
			.UntilClients(
				TEXT("Owner and simulated proxy observe the pivot acceleration"),
				[](FNetworkState& State)
				{
					return State.bSawPivot && HasMovingAnimation(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						-FVector::YAxisVector);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Listen server observes the remote pivot"),
				[](FNetworkState& State)
				{
					return State.bSawPivot && HasMovingAnimation(
						State,
						ROLE_Authority,
						-FVector::YAxisVector);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Stop autonomous movement input"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.UntilServer(
				TEXT("Authority settles to an idle animation snapshot"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy clear replicated acceleration"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Enable the server-authoritative Aim rotation request"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(Character->GetRpgAbilitySystemComponent()));
					Character->GetRpgAbilitySystemComponent()->AddLooseGameplayTag(
						RpgGameplayTags::State_Rotation_Aim.GetTag(),
						1,
						EGameplayTagReplicationState::TagAndCountToAll);
				})
			.UntilServer(
				TEXT("Authority resolves Aim locomotion"),
				[](FNetworkState& State)
				{
					return HasRotationMode(State, ERpgCharacterRotationMode::Aim);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Aim locomotion reaches owner and simulated proxy"),
				[](FNetworkState& State)
				{
					return HasRotationMode(State, ERpgCharacterRotationMode::Aim) &&
						State.bSawAim;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Replace the Aim request with CombatStrafe"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					URpgAbilitySystemComponent* AbilitySystem = Character
						? Character->GetRpgAbilitySystemComponent()
						: nullptr;
					ASSERT_THAT(IsNotNull(AbilitySystem));
					if (!AbilitySystem)
					{
						return;
					}
					AbilitySystem->RemoveLooseGameplayTag(
						RpgGameplayTags::State_Rotation_Aim.GetTag(),
						1,
						EGameplayTagReplicationState::TagAndCountToAll);
					AbilitySystem->AddLooseGameplayTag(
						RpgGameplayTags::State_Rotation_CombatStrafe.GetTag(),
						1,
						EGameplayTagReplicationState::TagAndCountToAll);
				})
			.UntilClients(
				TEXT("CombatStrafe locomotion is restored after Aim"),
				[](FNetworkState& State)
				{
					return HasRotationMode(
						State,
						ERpgCharacterRotationMode::CombatStrafe);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Authority is inactive before the deliberate turn-in-place"),
				[](FNetworkState& State)
				{
					return IsTurnInPlaceInactive(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Clients are inactive before the deliberate turn-in-place"),
				[](FNetworkState& State)
				{
					return IsTurnInPlaceInactive(State);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Arm the authority turn-in-place observation"),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					State.bSawTurnInPlace = false;
					State.bTurnObservationArmed = Character != nullptr;
					State.TurnObservationBaselineYaw = Character
						? Character->GetActorRotation().Yaw
						: 0.0;
				})
			.ThenClients(
				TEXT("Arm the client turn-in-place observations"),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					State.bSawTurnInPlace = false;
					State.bTurnObservationArmed = Character != nullptr;
					State.TurnObservationBaselineYaw = Character
						? Character->GetActorRotation().Yaw
						: 0.0;
				})
			.ThenClient(
				TEXT("Owner produces a stationary ninety-degree facing change"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					APlayerController* PlayerController = Character
						? Cast<APlayerController>(Character->GetController())
						: nullptr;
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(PlayerController));
					const FRotator TurnRotation(
						0.0,
						Character->GetActorRotation().Yaw + 90.0,
						0.0);
					PlayerController->SetControlRotation(TurnRotation);
					Character->SetActorRotation(TurnRotation);
				})
			.UntilClients(
				TEXT("Owner and simulated proxy activate turn-in-place"),
				[](FNetworkState& State)
				{
					return State.bSawTurnInPlace;
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Listen server activates the remote turn-in-place"),
				[](FNetworkState& State)
				{
					return State.bSawTurnInPlace;
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Authority turn-in-place lifecycle settles"),
				[](FNetworkState& State)
				{
					return State.bSawTurnInPlace && IsTurnInPlaceInactive(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client turn-in-place lifecycles settle"),
				[](FNetworkState& State)
				{
					return State.bSawTurnInPlace && IsTurnInPlaceInactive(State);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Owner requests crouch"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						Character->ToggleCrouch();
					}
				})
			.UntilServer(
				TEXT("Authority receives crouch stance"),
				[](FNetworkState& State)
				{
					return HasCrouchState(State, true);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Crouch stance reaches owner and simulated proxy"),
				[](FNetworkState& State)
				{
					return HasCrouchState(State, true);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Owner exits crouch"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						Character->ToggleCrouch();
					}
				})
			.UntilClients(
				TEXT("Standing stance is restored"),
				[](FNetworkState& State)
				{
					return HasCrouchState(State, false);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Owner starts a physical jump"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						Character->Jump();
					}
				})
			.UntilServer(
				TEXT("Authority observes the airborne animation phase"),
				[](FNetworkState& State)
				{
					return State.bSawAirborne;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy observe the airborne phase"),
				[](FNetworkState& State)
				{
					return State.bSawAirborne;
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Owner releases jump input"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						Character->StopJumping();
					}
				})
			.UntilServer(
				TEXT("Authority completes physical landing"),
				[](FNetworkState& State)
				{
					return State.bSawLanding && State.bSawGroundedAfterAirborne;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy complete landing"),
				[](FNetworkState& State)
				{
					return State.bSawLanding && State.bSawGroundedAfterAirborne;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Capture the authority baseline for correction"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						AuthorityCorrectionBaseline = Character->GetActorLocation();
					}
				})
			.ThenClient(
				TEXT("Create an owner-only lateral prediction divergence"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					const FVector DivergentLocation =
						Character->GetActorLocation() + FVector(250.0, 0.0, 0.0);
					Character->SetActorLocation(
						DivergentLocation,
						false,
						nullptr,
						ETeleportType::TeleportPhysics);
					ASSERT_THAT(IsTrue(FMath::Abs(
						Character->GetActorLocation().X -
						AuthorityCorrectionBaseline.X) > 150.0));
					StartMovementInput(State, FVector::YAxisVector);
					CorrectionStartTime = FPlatformTime::Seconds();
				})
			.UntilServer(
				TEXT("Authority rejects the owner-only lateral displacement"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return FPlatformTime::Seconds() - CorrectionStartTime >= 0.5 &&
						Character && FMath::Abs(
							Character->GetActorLocation().X -
							AuthorityCorrectionBaseline.X) <= 50.0;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Autonomous pawn reconverges to the server lane"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character && FMath::Abs(
						Character->GetActorLocation().X -
						AuthorityCorrectionBaseline.X) <= 50.0;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Simulated proxy remains converged to authority"),
				1,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character && FMath::Abs(
						Character->GetActorLocation().X -
						AuthorityCorrectionBaseline.X) <= 75.0;
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Stop movement after correction"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.UntilServer(
				TEXT("Authority settles before the montage"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("All client views settle before the montage"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Capture the server montage start"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						State.MontageStartLocation = Character->GetActorLocation();
						State.MaximumMontageDisplacement = 0.0f;
						State.bTrackMontageDisplacement = true;
					}
				})
			.ThenClients(
				TEXT("Capture both client montage starts"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						State.MontageStartLocation = Character->GetActorLocation();
						State.MaximumMontageDisplacement = 0.0f;
						State.bTrackMontageDisplacement = true;
						State.MontageConvergenceStartTime = 0.0;
					}
				})
			.ThenClient(
				TEXT("Owner locally starts the DefaultSlot montage through its ASC"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgAbilitySystemComponent* AbilitySystem = Character
						? Character->GetRpgAbilitySystemComponent()
						: nullptr;
					UAnimSequence* Attack = LoadObject<UAnimSequence>(
						nullptr,
						RootMotionAttackPath);
					ASSERT_THAT(IsNotNull(AbilitySystem));
					ASSERT_THAT(IsNotNull(Attack));
					UAnimMontage* Montage = AbilitySystem
						->PlaySlotAnimationAsDynamicMontage_WithFractionalLoops(
							nullptr,
							FGameplayAbilityActivationInfo(),
							Attack,
							DefaultSlotName,
							0.05f,
							0.05f,
							1.0f,
							0.0f,
							1.0f);
					ASSERT_THAT(IsTrue(IsExpectedDefaultSlotMontage(Montage)));
				})
			.ThenServer(
				TEXT("Authority starts the replicated root-motion montage through its ASC"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgAbilitySystemComponent* AbilitySystem = Character
						? Character->GetRpgAbilitySystemComponent()
						: nullptr;
					UAnimSequence* Attack = LoadObject<UAnimSequence>(
						nullptr,
						RootMotionAttackPath);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(AbilitySystem));
					ASSERT_THAT(IsNotNull(Attack));
					ASSERT_THAT(IsTrue(Attack->HasRootMotion()));
					UAnimMontage* Montage = AbilitySystem
						->PlaySlotAnimationAsDynamicMontage_WithFractionalLoops(
							nullptr,
							FGameplayAbilityActivationInfo(),
							Attack,
							DefaultSlotName,
							0.05f,
							0.05f,
							1.0f,
							0.0f,
							1.0f);
					ASSERT_THAT(IsTrue(IsExpectedDefaultSlotMontage(Montage)));
				})
			.UntilServer(
				TEXT("Authority applies DefaultSlot montage root motion"),
				[](FNetworkState& State)
				{
					return State.bSawDefaultSlotMontage &&
						State.bSawMontageAnimGate &&
						State.MaximumMontageDisplacement > 2.0f;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("GAS montage and AnimInstance gate reach owner and simulated proxy"),
				[](FNetworkState& State)
				{
					return State.bSawDefaultSlotMontage &&
						State.bSawMontageAnimGate;
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Authority completes the dynamic montage"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgAbilitySystemComponent* AbilitySystem = Character
						? Character->GetRpgAbilitySystemComponent()
						: nullptr;
					if (!Character || !AbilitySystem || AbilitySystem->GetCurrentMontage())
					{
						return false;
					}
					AuthorityMontageEnd = Character->GetActorLocation();
					return true;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy converge after root motion"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgAbilitySystemComponent* AbilitySystem = Character
						? Character->GetRpgAbilitySystemComponent()
						: nullptr;
					const bool bConverged = Character && AbilitySystem &&
						!AbilitySystem->GetCurrentMontage() && FVector::Dist2D(
							Character->GetActorLocation(),
							AuthorityMontageEnd) <= 100.0;
					if (!bConverged)
					{
						State.MontageConvergenceStartTime = 0.0;
						return false;
					}

					const double Now = FPlatformTime::Seconds();
					if (State.MontageConvergenceStartTime <= 0.0)
					{
						State.MontageConvergenceStartTime = Now;
					}
					return Now - State.MontageConvergenceStartTime >= 0.25;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Cleanup server observers"),
				[](FNetworkState& State)
				{
					State.World->GetTimerManager().ClearTimer(State.ObservationTimer);
				})
			.ThenClients(
				TEXT("Cleanup client observers"),
				[](FNetworkState& State)
				{
					StopMovementInput(State);
					State.World->GetTimerManager().ClearTimer(State.ObservationTimer);
				});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
