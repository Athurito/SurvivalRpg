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
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
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
		float MovementInputScale = 0.0f;
		float StableInputScale = -1.0f;
		double StableInputStartTime = 0.0;
		double LastMovementInputDiagnosticTime = -1.0;
		double LastLandingDiagnosticTime = -1.0;
		FVector MovementStoppedAnchorLocation = FVector::ZeroVector;
		double MovementStoppedStableStartTime = -1.0;
		int32 AnimationResetBaseline = 0;
		int32 LastObservedAnimationResetDelta = MIN_int32;
		double AnimationResetStableStartTime = -1.0;
		uint64 AnimationResetStableStartFrame = 0;
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

	const FGameplayTag& GetMovementStoppedTag()
	{
		static const FGameplayTag Tag =
			FGameplayTag::RequestGameplayTag(TEXT("Gameplay.MovementStopped"));
		return Tag;
	}

	bool HasStableMovementStoppedContract(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAbilitySystemComponent* AbilitySystem = Character
			? Character->GetRpgAbilitySystemComponent()
			: nullptr;
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		const bool bHasStopContract = Character && AbilitySystem && MovementComponent &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			AbilitySystem->HasMatchingGameplayTag(GetMovementStoppedTag()) &&
			FMath::IsNearlyZero(MovementComponent->GetMaxSpeed()) &&
			FMath::IsNearlyZero(MovementComponent->GetMinAnalogSpeed()) &&
			Character->GetVelocity().Size2D() <= 5.0f;
		if (!bHasStopContract)
		{
			State.MovementStoppedStableStartTime = -1.0;
			return false;
		}

		const double Now = State.World->GetTimeSeconds();
		if (State.MovementStoppedStableStartTime < 0.0)
		{
			State.MovementStoppedStableStartTime = Now;
			State.MovementStoppedAnchorLocation = Character->GetActorLocation();
			return false;
		}

		if (FVector::Dist2D(
				State.MovementStoppedAnchorLocation,
				Character->GetActorLocation()) > 5.0f)
		{
			State.MovementStoppedStableStartTime = Now;
			State.MovementStoppedAnchorLocation = Character->GetActorLocation();
			return false;
		}

		return Now - State.MovementStoppedStableStartTime >= 0.5;
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
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		if (!Character || !AnimInstance || !MovementComponent)
		{
			return false;
		}

		FVector WorldAcceleration = FVector::ZeroVector;
		float GroundSpeed = 0.0f;
		bool bHasAcceleration = true;
		ERpgLocomotionGait AnimGait = ERpgLocomotionGait::Run;
		const bool bHasNativeZeroSnapshot =
			Character->GetLocalRole() != ROLE_SimulatedProxy ||
			(Character->GetReplicatedMovement().bRepAcceleration &&
			 Character->GetReplicatedMovement().Acceleration.Size2D() <= 5.0f);
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
			ReadAnimProperty(
				AnimInstance,
				TEXT("LocomotionGait"),
				AnimGait) &&
			AnimGait == ERpgLocomotionGait::Idle &&
			!bHasAcceleration && WorldAcceleration.Size2D() <= 5.0f &&
			MovementComponent->GetCurrentAcceleration().Size2D() <= 5.0f &&
			MovementComponent->GetAnalogInputModifier() <= 0.01f &&
			MovementComponent->GetDesiredGait() == ERpgLocomotionGait::Idle &&
			MovementComponent->GetGroundGait() == ERpgLocomotionGait::Idle &&
			!MovementComponent->HasMoveIntent() &&
			FMath::IsNearlyEqual(
				MovementComponent->GetMaxBrakingDeceleration(),
				2000.0f) &&
			bHasNativeZeroSnapshot &&
			GroundSpeed <= 8.0f && Character->GetVelocity().Size2D() <= 8.0f;
	}

	bool HasCompletedLanding(FNetworkState& State)
	{
		if (State.bSawLanding && State.bSawGroundedAfterAirborne)
		{
			return true;
		}

		const double Now = FPlatformTime::Seconds();
		if (Now - State.LastLandingDiagnosticTime < 1.0)
		{
			return false;
		}
		State.LastLandingDiagnosticTime = Now;

		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		const UCharacterMovementComponent* MovementComponent = Character
			? Character->GetCharacterMovement()
			: nullptr;
		ERpgJumpPhase JumpPhase = ERpgJumpPhase::Grounded;
		ERpgMotionMatchingDatabaseRole ActiveLandingRole =
			ERpgMotionMatchingDatabaseRole::None;
		FRpgLandingSelectionSnapshot LandingSnapshot;
		const bool bReadJumpPhase = ReadAnimProperty(
			AnimInstance,
			TEXT("JumpPhase"),
			JumpPhase);
		const bool bReadLandingRole = ReadAnimProperty(
			AnimInstance,
			TEXT("ActiveLandingDatabaseRole"),
			ActiveLandingRole);
		const bool bReadLandingSnapshot = ReadAnimProperty(
			AnimInstance,
			TEXT("PreTouchdownLandingSnapshot"),
			LandingSnapshot);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("GASP landing mismatch: sawAir=%d sawLanding=%d sawGround=%d character=%d role=%d movement=%d mode=%d ground=%d falling=%d velocity=%s phaseRead=%d phase=%d roleRead=%d landingRole=%d snapshotRead=%d valid=%d epoch=%d maxDown=%.1f"),
			static_cast<int32>(State.bSawAirborne),
			static_cast<int32>(State.bSawLanding),
			static_cast<int32>(State.bSawGroundedAfterAirborne),
			static_cast<int32>(Character != nullptr),
			Character ? static_cast<int32>(Character->GetLocalRole()) : -1,
			static_cast<int32>(MovementComponent != nullptr),
			MovementComponent ? static_cast<int32>(MovementComponent->MovementMode) : -1,
			static_cast<int32>(MovementComponent ? MovementComponent->IsMovingOnGround() : false),
			static_cast<int32>(MovementComponent ? MovementComponent->IsFalling() : false),
			MovementComponent ? *MovementComponent->Velocity.ToCompactString() : TEXT("none"),
			static_cast<int32>(bReadJumpPhase),
			static_cast<int32>(JumpPhase),
			static_cast<int32>(bReadLandingRole),
			static_cast<int32>(ActiveLandingRole),
			static_cast<int32>(bReadLandingSnapshot),
			static_cast<int32>(LandingSnapshot.bIsValid),
			LandingSnapshot.AirborneEpoch,
			static_cast<double>(LandingSnapshot.MaximumDownwardSpeed));
		return false;
	}

	bool HasExpectedMovementInput(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole,
		const FVector& ExpectedDirection,
		const float ExpectedScale)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		FVector AnimAcceleration = FVector::ZeroVector;
		ERpgLocomotionGait AnimGait = ERpgLocomotionGait::Idle;
		const FVector Direction2D = ExpectedDirection.GetSafeNormal2D();
		const float ClampedScale = FMath::Clamp(ExpectedScale, 0.0f, 1.0f);
		const float MaxAcceleration = MovementComponent
			? MovementComponent->GetMaxAcceleration()
			: 0.0f;
		const float ExpectedMagnitude = MaxAcceleration * ClampedScale;
		const float AccelerationTolerance = FMath::Max(2.0f, MaxAcceleration * 0.02f);
		const FVector CurrentAcceleration = MovementComponent
			? MovementComponent->GetCurrentAcceleration()
			: FVector::ZeroVector;
		const ERpgLocomotionGait ExpectedGait = ClampedScale >= 0.7f
			? ERpgLocomotionGait::Run
			: ERpgLocomotionGait::Walk;
		const float ExpectedSpeedCap = ExpectedGait == ERpgLocomotionGait::Run
			? 500.0f
			: 200.0f;
		const float ExpectedGroundSpeed = FMath::Max(
			ExpectedSpeedCap * ClampedScale,
			150.0f);
		const FRepMovement* ReplicatedMovement = Character
			? &Character->GetReplicatedMovement()
			: nullptr;
		const bool bHasNativeProxySnapshot =
			ExpectedLocalRole != ROLE_SimulatedProxy ||
			(ReplicatedMovement &&
			 ReplicatedMovement->bRepAcceleration &&
			 ReplicatedMovement->Acceleration.Equals(
				 CurrentAcceleration,
				 AccelerationTolerance));
		const bool bReadAnimAcceleration = ReadAnimProperty(
			AnimInstance,
			TEXT("WorldAcceleration"),
			AnimAcceleration);
		const bool bReadAnimGait = ReadAnimProperty(
			AnimInstance,
			TEXT("LocomotionGait"),
			AnimGait);
		const bool bMatches =
			Character &&
			MovementComponent &&
			AnimInstance &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			MovementComponent->IsMovingOnGround() &&
			MovementComponent->GetMovementProfile().bOverrideCharacterMovement &&
			MovementComponent->GetDesiredGait() == ExpectedGait &&
			MovementComponent->GetGroundGait() == ExpectedGait &&
			MovementComponent->HasMoveIntent() &&
			FMath::IsNearlyEqual(MovementComponent->GetMaxAcceleration(), 800.0f) &&
			FMath::IsNearlyEqual(MovementComponent->GetMinAnalogSpeed(), 150.0f) &&
			FMath::IsNearlyEqual(MovementComponent->GetMaxSpeed(), ExpectedSpeedCap) &&
			FMath::IsNearlyEqual(
				Character->GetVelocity().Size2D(),
				ExpectedGroundSpeed,
				15.0f) &&
			FMath::IsNearlyEqual(
				MovementComponent->GetMaxBrakingDeceleration(),
				500.0f) &&
			!CurrentAcceleration.ContainsNaN() &&
			FMath::IsNearlyEqual(
				CurrentAcceleration.Size2D(),
				ExpectedMagnitude,
				AccelerationTolerance) &&
			FVector::DotProduct(
				CurrentAcceleration.GetSafeNormal2D(),
				Direction2D) > 0.98f &&
			FMath::IsNearlyEqual(
				MovementComponent->GetAnalogInputModifier(),
				ClampedScale,
				0.025f) &&
			bReadAnimAcceleration &&
			bReadAnimGait &&
			AnimGait == ExpectedGait &&
			!AnimAcceleration.ContainsNaN() &&
			FMath::IsNearlyEqual(
				AnimAcceleration.Size2D(),
				ExpectedMagnitude,
				AccelerationTolerance) &&
			FVector::DotProduct(
				AnimAcceleration.GetSafeNormal2D(),
				Direction2D) > 0.98f &&
			bHasNativeProxySnapshot;

		const double Now = FPlatformTime::Seconds();
		if (!bMatches &&
			Now - State.LastMovementInputDiagnosticTime >= 1.0)
		{
			State.LastMovementInputDiagnosticTime = Now;
			const FRepMovement* SafeReplicatedMovement = Character
				? &Character->GetReplicatedMovement()
				: nullptr;
			UE_LOG(
				LogTemp,
				Display,
				TEXT("GASP input mismatch: expected role=%d scale=%.3f gait=%d cap=%.1f | character=%d role=%d move=%d anim=%d ground=%d profile=%d"),
				static_cast<int32>(ExpectedLocalRole),
				static_cast<double>(ClampedScale),
				static_cast<int32>(ExpectedGait),
				static_cast<double>(ExpectedSpeedCap),
				static_cast<int32>(Character != nullptr),
				Character ? static_cast<int32>(Character->GetLocalRole()) : -1,
				static_cast<int32>(MovementComponent != nullptr),
				static_cast<int32>(AnimInstance != nullptr),
				static_cast<int32>(MovementComponent ? MovementComponent->IsMovingOnGround() : false),
				static_cast<int32>(MovementComponent ? MovementComponent->GetMovementProfile().bOverrideCharacterMovement : false));
			UE_LOG(
				LogTemp,
				Display,
				TEXT("GASP input values: desired=%d groundGait=%d intent=%d analog=%.3f accel=%s speed=%.1f/%.1f cap=%.1f brake=%.1f animRead=%d/%d animGait=%d animAccel=%s nativeProxy=%d repAccel=%d repValue=%s"),
				MovementComponent ? static_cast<int32>(MovementComponent->GetDesiredGait()) : -1,
				MovementComponent ? static_cast<int32>(MovementComponent->GetGroundGait()) : -1,
				static_cast<int32>(MovementComponent ? MovementComponent->HasMoveIntent() : false),
				static_cast<double>(MovementComponent ? MovementComponent->GetAnalogInputModifier() : -1.0f),
				*CurrentAcceleration.ToCompactString(),
				static_cast<double>(Character ? Character->GetVelocity().Size2D() : -1.0f),
				static_cast<double>(ExpectedGroundSpeed),
				static_cast<double>(MovementComponent ? MovementComponent->GetMaxSpeed() : -1.0f),
				static_cast<double>(MovementComponent ? MovementComponent->GetMaxBrakingDeceleration() : -1.0f),
				static_cast<int32>(bReadAnimAcceleration),
				static_cast<int32>(bReadAnimGait),
				static_cast<int32>(AnimGait),
				*AnimAcceleration.ToCompactString(),
				static_cast<int32>(bHasNativeProxySnapshot),
				static_cast<int32>(SafeReplicatedMovement ? SafeReplicatedMovement->bRepAcceleration : false),
				SafeReplicatedMovement
					? *SafeReplicatedMovement->Acceleration.ToCompactString()
					: TEXT("none"));
		}

		if (!bMatches ||
			!FMath::IsNearlyEqual(State.StableInputScale, ClampedScale, 0.001f))
		{
			State.StableInputScale = bMatches ? ClampedScale : -1.0f;
			State.StableInputStartTime = bMatches ? FPlatformTime::Seconds() : 0.0;
			return false;
		}

		return Now - State.StableInputStartTime >= 0.2;
	}

	bool ReadAnimationHistoryResetCount(
		FNetworkState& State,
		int32& OutResetCount)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		return ReadAnimProperty(
			GetPilotAnimInstance(Character),
			TEXT("AnimationHistoryResetCount"),
			OutResetCount);
	}

	bool ReadAnimationHistoryResetDelta(
		FNetworkState& State,
		int32& OutResetDelta)
	{
		int32 ResetCount = 0;
		if (!ReadAnimationHistoryResetCount(State, ResetCount))
		{
			return false;
		}

		OutResetDelta = ResetCount - State.AnimationResetBaseline;
		if (OutResetDelta != State.LastObservedAnimationResetDelta)
		{
			State.LastObservedAnimationResetDelta = OutResetDelta;
			UE_LOG(
				LogTemp,
				Display,
				TEXT("GASP history-reset observation: ClientIndex=%d Baseline=%d Current=%d Delta=%d"),
				State.ClientIndex,
				State.AnimationResetBaseline,
				ResetCount,
				OutResetDelta);
		}
		return true;
	}

	bool HasAnimationResetDeltaAtLeast(
		FNetworkState& State,
		const int32 MinimumDelta)
	{
		int32 ResetDelta = 0;
		return ReadAnimationHistoryResetDelta(State, ResetDelta) &&
			ResetDelta >= MinimumDelta;
	}

	bool HasStableAnimationResetDelta(
		FNetworkState& State,
		const int32 ExpectedDelta)
	{
		int32 ActualDelta = 0;
		if (!ReadAnimationHistoryResetDelta(State, ActualDelta))
		{
			State.AnimationResetStableStartTime = -1.0;
			State.AnimationResetStableStartFrame = 0;
			return false;
		}

		if (ActualDelta != ExpectedDelta)
		{
			State.AnimationResetStableStartTime = -1.0;
			State.AnimationResetStableStartFrame = 0;
			return false;
		}

		const double Now = IsValid(State.World)
			? State.World->GetTimeSeconds()
			: -1.0;
		if (Now < 0.0)
		{
			return false;
		}
		if (State.AnimationResetStableStartTime < 0.0)
		{
			State.AnimationResetStableStartTime = Now;
			State.AnimationResetStableStartFrame = GFrameCounter;
		}
		return Now - State.AnimationResetStableStartTime >= 0.35 &&
			GFrameCounter - State.AnimationResetStableStartFrame >= 10;
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
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		ERpgLocomotionStance Stance = ERpgLocomotionStance::Standing;
		const ERpgLocomotionStance ExpectedStance = bExpectedCrouched
			? ERpgLocomotionStance::Crouching
			: ERpgLocomotionStance::Standing;
		const bool bCrouchPreservesLegacyPhysics = !bExpectedCrouched ||
			(MovementComponent &&
			 FMath::IsNearlyEqual(
				 MovementComponent->GetMaxSpeed(),
				 MovementComponent->MaxWalkSpeedCrouched) &&
			 FMath::IsNearlyEqual(
				 MovementComponent->GetMinAnalogSpeed(),
				 MovementComponent->MinAnalogWalkSpeed) &&
			 FMath::IsNearlyEqual(
				 MovementComponent->GetMaxAcceleration(),
				 MovementComponent->MaxAcceleration) &&
			 FMath::IsNearlyEqual(
				 MovementComponent->GetMaxBrakingDeceleration(),
				 MovementComponent->BrakingDecelerationWalking));
		return Character && MovementComponent &&
			Character->IsCrouched() == bExpectedCrouched &&
			ReadAnimProperty(AnimInstance, TEXT("LocomotionStance"), Stance) &&
			Stance == ExpectedStance &&
			bCrouchPreservesLegacyPhysics;
	}

	bool HasFreeRotationPolicy(FNetworkState& State, const ENetRole ExpectedLocalRole)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		ERpgCharacterRotationMode AnimRotationMode = ERpgCharacterRotationMode::Aim;
		return Character && MovementComponent && AnimInstance &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			Character->GetRotationMode() == ERpgCharacterRotationMode::Free &&
			!Character->bUseControllerRotationYaw &&
			MovementComponent->bOrientRotationToMovement &&
			!MovementComponent->bUseControllerDesiredRotation &&
			FMath::IsNearlyEqual(MovementComponent->RotationRate.Yaw, 360.0f) &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("CharacterRotationMode"),
				AnimRotationMode) &&
			AnimRotationMode == ERpgCharacterRotationMode::Free;
	}

	bool HasLegacyAirborneMovementPolicy(FNetworkState& State)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		return Character && MovementComponent && MovementComponent->IsFalling() &&
			FMath::IsNearlyEqual(
				MovementComponent->GetMaxSpeed(),
				MovementComponent->MaxWalkSpeed) &&
			FMath::IsNearlyEqual(
				MovementComponent->GetMinAnalogSpeed(),
				MovementComponent->MinAnalogWalkSpeed) &&
			FMath::IsNearlyEqual(
				MovementComponent->GetMaxAcceleration(),
				MovementComponent->MaxAcceleration) &&
			FMath::IsNearlyEqual(
				MovementComponent->GetMaxBrakingDeceleration(),
				MovementComponent->BrakingDecelerationFalling);
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
			0.001f,
			FTimerManagerTimerParameters{
				.bLoop = true,
				.bMaxOncePerFrame = true,
				.FirstDelay = 0.0f });
		ActiveTimerStates.AddUnique(&State);
	}

	void StartMovementInput(
		FNetworkState& State,
		const FVector& Direction,
		const float Scale = 1.0f)
	{
		if (!IsValid(State.World))
		{
			return;
		}

		State.MovementInputDirection = Direction.GetSafeNormal2D();
		State.MovementInputScale = FMath::Clamp(Scale, 0.0f, 1.0f);
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
					Character->ConsumeMovementInputVector();
					Character->AddMovementInput(
						State.MovementInputDirection,
						State.MovementInputScale);
				}
			}),
			0.001f,
			FTimerManagerTimerParameters{
				.bLoop = true,
				.bMaxOncePerFrame = true,
				.FirstDelay = 0.0f });
		ActiveTimerStates.AddUnique(&State);
	}

	void StopMovementInput(FNetworkState& State)
	{
		if (IsValid(State.World))
		{
			State.World->GetTimerManager().ClearTimer(State.MovementInputTimer);
		}
		State.MovementInputDirection = FVector::ZeroVector;
		State.MovementInputScale = 0.0f;
		if (ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId))
		{
			Character->ConsumeMovementInputVector();
		}
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
	FVector AuthorityTeleportTarget = FVector::ZeroVector;
	FVector AuthorityMontageEnd = FVector::ZeroVector;
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
					StartMovementInput(State, FVector::YAxisVector, 0.5f);
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
				TEXT("Authority applies the GASP Free rotation policy"),
				[](FNetworkState& State)
				{
					return HasFreeRotationPolicy(State, ROLE_Authority);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner applies the GASP Free rotation policy"),
				0,
				[](FNetworkState& State)
				{
					return HasFreeRotationPolicy(State, ROLE_AutonomousProxy);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Moving late join applies the GASP Free rotation policy"),
				1,
				[](FNetworkState& State)
				{
					return HasFreeRotationPolicy(State, ROLE_SimulatedProxy);
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
			.UntilServer(
				TEXT("Authority preserves the owner's fifty-percent input magnitude"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.5f);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner preserves fifty-percent input magnitude"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_AutonomousProxy,
						FVector::YAxisVector,
						0.5f);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Moving late join reconstructs fifty-percent proxy input"),
				1,
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.5f);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Reduce owner input to twenty-five percent"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.25f);
				})
			.UntilServer(
				TEXT("Authority preserves twenty-five-percent input magnitude"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.25f);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy preserve twenty-five-percent input"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.25f);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Restore full owner input before the pivot"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 1.0f);
				})
			.UntilServer(
				TEXT("Authority restores full input magnitude"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						1.0f);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy restore full input magnitude"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						1.0f);
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
				TEXT("Apply the server-authoritative movement-stop contract"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(Character->GetRpgAbilitySystemComponent()));
					Character->GetRpgAbilitySystemComponent()->AddLooseGameplayTag(
						GetMovementStoppedTag(),
						1,
						EGameplayTagReplicationState::TagAndCountToAll);
				})
			.UntilClients(
				TEXT("MovementStopped reaches owner and simulated proxy before input"),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					const URpgAbilitySystemComponent* AbilitySystem = Character
						? Character->GetRpgAbilitySystemComponent()
						: nullptr;
					const URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
						: nullptr;
					return AbilitySystem && MovementComponent &&
						AbilitySystem->HasMatchingGameplayTag(GetMovementStoppedTag()) &&
						FMath::IsNearlyZero(MovementComponent->GetMaxSpeed()) &&
						FMath::IsNearlyZero(MovementComponent->GetMinAnalogSpeed());
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Hold full movement input while MovementStopped is active"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 1.0f);
				})
			.UntilServer(
				TEXT("Authority remains physically stopped with the GASP analog floor suppressed"),
				[](FNetworkState& State)
				{
					return HasStableMovementStoppedContract(State, ROLE_Authority);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy remain physically stopped under held input"),
				[](FNetworkState& State)
				{
					return HasStableMovementStoppedContract(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Release movement input before clearing MovementStopped"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.ThenServer(
				TEXT("Clear the server-authoritative movement-stop contract"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(Character->GetRpgAbilitySystemComponent()));
					Character->GetRpgAbilitySystemComponent()->RemoveLooseGameplayTag(
						GetMovementStoppedTag(),
						1,
						EGameplayTagReplicationState::TagAndCountToAll);
				})
			.UntilServer(
				TEXT("Authority restores the standing GASP analog floor"),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					const URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
						: nullptr;
					return MovementComponent &&
						FMath::IsNearlyEqual(MovementComponent->GetMinAnalogSpeed(), 150.0f);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy restore the standing GASP analog floor"),
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					const URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
						: nullptr;
					return MovementComponent &&
						FMath::IsNearlyEqual(MovementComponent->GetMinAnalogSpeed(), 150.0f);
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
				[this](FNetworkState& State)
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
				[this](FNetworkState& State)
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
					return State.bSawAirborne &&
						HasLegacyAirborneMovementPolicy(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy observe the airborne phase"),
				[](FNetworkState& State)
				{
					return State.bSawAirborne &&
						HasLegacyAirborneMovementPolicy(State);
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
					return HasCompletedLanding(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy complete landing"),
				[](FNetworkState& State)
				{
					return HasCompletedLanding(State);
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
					int32 ResetCount = 0;
					ASSERT_THAT(IsTrue(ReadAnimationHistoryResetCount(
						State,
						ResetCount)));
					State.AnimationResetBaseline = ResetCount;
					State.LastObservedAnimationResetDelta = MIN_int32;
					State.AnimationResetStableStartTime = -1.0;
					State.AnimationResetStableStartFrame = 0;
				})
			.ThenClients(
				TEXT("Capture client animation-history baselines for correction"),
				[this](FNetworkState& State)
				{
					int32 ResetCount = 0;
					ASSERT_THAT(IsTrue(ReadAnimationHistoryResetCount(
						State,
						ResetCount)));
					State.AnimationResetBaseline = ResetCount;
					State.LastObservedAnimationResetDelta = MIN_int32;
					State.AnimationResetStableStartTime = -1.0;
					State.AnimationResetStableStartFrame = 0;
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
						Character->GetActorLocation() + FVector(100.0, 0.0, 0.0);
					Character->SetActorLocation(
						DivergentLocation,
						false,
						nullptr,
						ETeleportType::None);
					ASSERT_THAT(IsTrue(FMath::Abs(
						Character->GetActorLocation().X -
							AuthorityCorrectionBaseline.X) > 60.0));
					StartMovementInput(State, FVector::YAxisVector);
				})
			.ThenServer(
				TEXT("Force the authoritative adjustment for the deliberate divergence"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
						: nullptr;
					ASSERT_THAT(IsNotNull(MovementComponent));
					if (MovementComponent)
					{
						FVector AuthoritativeLocation = Character->GetActorLocation();
						AuthoritativeLocation.X = AuthorityCorrectionBaseline.X;
						Character->SetActorLocation(
							AuthoritativeLocation,
							false,
							nullptr,
							ETeleportType::None);
						ASSERT_THAT(IsTrue(FMath::Abs(
							Character->GetActorLocation().X -
								AuthorityCorrectionBaseline.X) <= 1.0));

						FNetworkPredictionData_Server_Character* ServerPrediction =
							MovementComponent->GetPredictionData_Server_Character();
						ASSERT_THAT(IsNotNull(ServerPrediction));
						if (ServerPrediction)
						{
							// Force the next genuine ServerMove through UE's correction path. The
							// replication and adjustment calls bypass its update and send throttles;
							// neither one queues a correction without bForceClientUpdate.
							ServerPrediction->bForceClientUpdate = true;
						}
						MovementComponent->ForceReplicationUpdate();
						MovementComponent->ForceClientAdjustment();
					}
				})
			.UntilClient(
				TEXT("Autonomous presentation observes the server correction"),
				0,
				[](FNetworkState& State)
				{
					return HasAnimationResetDeltaAtLeast(State, 1);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Stop movement immediately after the owner correction"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.UntilServer(
				TEXT("Authority settles after the owner correction"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client views settle after the owner correction"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Authority remains on the server lane after correction"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character && FMath::Abs(
						Character->GetActorLocation().X -
							AuthorityCorrectionBaseline.X) <= 10.0;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Autonomous pawn converges tightly to the server lane"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character && FMath::Abs(
						Character->GetActorLocation().X -
							AuthorityCorrectionBaseline.X) <= 10.0;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Simulated proxy remains tightly converged to authority"),
				1,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character && FMath::Abs(
						Character->GetActorLocation().X -
							AuthorityCorrectionBaseline.X) <= 10.0;
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Owner correction leaves authority history stable"),
				[](FNetworkState& State)
				{
					return HasStableAnimationResetDelta(State, 0);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner correction resets autonomous history exactly once"),
				0,
				[](FNetworkState& State)
				{
					return HasStableAnimationResetDelta(State, 1);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner correction leaves simulated-proxy history stable"),
				1,
				[](FNetworkState& State)
				{
					return HasStableAnimationResetDelta(State, 0);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Assert the final authority correction reset count"),
				[this](FNetworkState& State)
				{
					int32 ResetDelta = INDEX_NONE;
					ASSERT_THAT(IsTrue(ReadAnimationHistoryResetDelta(
						State,
						ResetDelta)));
					ASSERT_THAT(IsTrue(ResetDelta == 0));
				})
			.ThenClients(
				TEXT("Assert the final client correction reset counts"),
				[this](FNetworkState& State)
				{
					int32 ResetDelta = INDEX_NONE;
					ASSERT_THAT(IsTrue(ReadAnimationHistoryResetDelta(
						State,
						ResetDelta)));
					ASSERT_THAT(IsTrue(
						ResetDelta == (State.ClientIndex == 0 ? 1 : 0)));
				})
			.ThenServer(
				TEXT("Capture authority history before a semantic teleport"),
				[this](FNetworkState& State)
				{
					int32 ResetCount = 0;
					ASSERT_THAT(IsTrue(ReadAnimationHistoryResetCount(
						State,
						ResetCount)));
					State.AnimationResetBaseline = ResetCount;
					State.LastObservedAnimationResetDelta = MIN_int32;
					State.AnimationResetStableStartTime = -1.0;
					State.AnimationResetStableStartFrame = 0;
				})
			.ThenClients(
				TEXT("Capture client histories before a semantic teleport"),
				[this](FNetworkState& State)
				{
					int32 ResetCount = 0;
					ASSERT_THAT(IsTrue(ReadAnimationHistoryResetCount(
						State,
						ResetCount)));
					State.AnimationResetBaseline = ResetCount;
					State.LastObservedAnimationResetDelta = MIN_int32;
					State.AnimationResetStableStartTime = -1.0;
					State.AnimationResetStableStartFrame = 0;
				})
			.ThenServer(
				TEXT("Teleport the authoritative subject beyond the no-smoothing range"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (!Character)
					{
						return;
					}
					AuthorityTeleportTarget =
						Character->GetActorLocation() + FVector(500.0, 0.0, 0.0);
					ASSERT_THAT(IsTrue(Character->TeleportTo(
						AuthorityTeleportTarget,
						Character->GetActorRotation())));
					Character->GetCharacterMovement()->StopMovementImmediately();
					Character->ForceNetUpdate();
				})
			.UntilServer(
				TEXT("Authority consumes the semantic teleport exactly once"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character &&
						FVector::Dist2D(
							Character->GetActorLocation(),
							AuthorityTeleportTarget) <= 5.0 &&
						HasStableAnimationResetDelta(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy consume the semantic teleport once"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character &&
						FVector::Dist2D(
							Character->GetActorLocation(),
							AuthorityTeleportTarget) <= 100.0 &&
						HasStableAnimationResetDelta(State, 1);
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
			.UntilServer(
				TEXT("Authority settles before the stationary late join"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Existing clients settle before the stationary late join"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Stationary late join establishes the third server connection"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 3);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Stationary late client loads the Pilot Experience and pawn"),
				2,
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 3) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Bind the stationary subject in the final late-client world"),
				2,
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.UntilClient(
				TEXT("Stationary late join reconstructs native zero input"),
				2,
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character &&
						Character->GetLocalRole() == ROLE_SimulatedProxy &&
						HasStoppedAnimation(State);
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
