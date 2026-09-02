#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgGaspNetworkTestTypes.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFeatureTypes.h"
#include "GameFeaturesSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Animation/RpgCombatAnimationProfileProviderComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspPIENetworkTests
{
	constexpr TCHAR PilotExperienceName[] = TEXT("RpgGaspPilotExperience");
	constexpr TCHAR PrototypeExperienceName[] = TEXT("RpgPrototypeExperience");
	constexpr TCHAR PilotGameModeClassPath[] =
		TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameMode.BP_Rpg_GameMode_C");
	constexpr TCHAR PrototypeCharacterClassPath[] =
		TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character.BP_Rpg_Character_C");
	constexpr TCHAR PilotCharacterClassPath[] =
		TEXT("/Game/SurvivalRpg/Core/Character/GASP/BP_Rpg_Character_GASP.BP_Rpg_Character_GASP_C");
	constexpr TCHAR RootMotionAttackPath[] =
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01");
	constexpr TCHAR BasicSwordDefinitionClassPath[] =
		TEXT("/GF_Combat_Core/Equipment/Weapons/ED_BasicSword.ED_BasicSword_C");
	constexpr TCHAR BasicShieldDefinitionClassPath[] =
		TEXT("/GF_Combat_Core/Equipment/Weapons/ED_BasicShield.ED_BasicShield_C");
	const FName DefaultSlotName(TEXT("DefaultSlot"));
	const FName OneHandSwordProfileName(TEXT("OneHandSword"));
	const FName SwordShieldProfileName(TEXT("SwordShield"));
	const FName UnarmedProfileName(TEXT("Unarmed"));

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgGaspNetworkFloorFixture* Floor = nullptr;
		ARpgGaspNetworkMovingBaseFixture* MovingBase = nullptr;
		int32 SubjectPlayerId = INDEX_NONE;
		FTimerHandle MovementInputTimer;
		FTimerHandle ObservationTimer;
		FVector MovementInputDirection = FVector::ZeroVector;
		float MovementInputScale = 0.0f;
		float StableInputScale = -1.0f;
		ERpgLocomotionGait StableInputGait = ERpgLocomotionGait::Idle;
		double StableInputStartTime = 0.0;
		ERpgLocomotionGait StableCoastGait = ERpgLocomotionGait::Idle;
		ENetRole StableCoastRole = ROLE_None;
		bool bStableCoastBelowWalkCap = false;
		double StableCoastStartTime = 0.0;
		TWeakObjectPtr<ARpgCharacter> StableCoastSubject;
		double LastCoastDiagnosticTime = -1.0;
		double LastCombatAnimationDiagnosticTime = -1.0;
		FRpgCharacterMovementProfile BaselineCoastProfile;
		bool bHasBaselineCoastProfile = false;
		float BaselineSubjectNetCullDistanceSquared = 0.0f;
		bool bHasBaselineSubjectNetCullDistanceSquared = false;
		float StableDeadzoneInputScale = -1.0f;
		double PhysicalDeadzoneStableStartTime = -1.0;
		FVector PhysicalDeadzoneAnchorLocation = FVector::ZeroVector;
		double LastMovementInputDiagnosticTime = -1.0;
		double LastLandingDiagnosticTime = -1.0;
		FVector MovementStoppedAnchorLocation = FVector::ZeroVector;
		double MovementStoppedStableStartTime = -1.0;
		int32 AnimationResetBaseline = 0;
		int32 LastObservedAnimationResetDelta = MIN_int32;
		double AnimationResetStableStartTime = -1.0;
		uint64 AnimationResetStableStartFrame = 0;
		FVector MovingBaseObservationStartLocation = FVector::ZeroVector;
		uint32 ClientCorrectionCountBaseline = 0;
		uint32 LargeClientCorrectionCountBaseline = 0;
		uint32 AnimationDiscontinuityBaseline = 0;
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
		TWeakObjectPtr<ARpgCharacter> SubjectBeforeRelevancyLoss;
		TWeakObjectPtr<ARpgCharacter> CombatFeatureLifecycleSubject;
		TWeakObjectPtr<URpgAnimInstance> CombatFeatureLifecycleAnimInstance;
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

	template <typename TValue>
	bool ReadObjectProperty(
		const UObject* Object,
		const FName PropertyName,
		TValue& OutValue)
	{
		if (!IsValid(Object))
		{
			return false;
		}

		FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), PropertyName);
		if (!Property || Property->GetSize() != sizeof(TValue))
		{
			return false;
		}

		const void* Source = Property->ContainerPtrToValuePtr<void>(Object);
		Property->CopyCompleteValue(&OutValue, Source);
		return true;
	}

	bool IsExperienceReady(
		FNetworkState& State,
		const int32 ExpectedClients,
		const TCHAR* ExpectedExperienceName)
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
			FName(ExpectedExperienceName));
		return bExpectedWorld && Experience &&
			Experience->GetPrimaryAssetId() == ExpectedExperienceId;
	}

	bool IsPilotExperienceReady(FNetworkState& State, const int32 ExpectedClients)
	{
		return IsExperienceReady(State, ExpectedClients, PilotExperienceName);
	}

	bool IsCharacterReadyForClassPath(
		ARpgCharacter* Character,
		const TCHAR* ExpectedClassPath)
	{
		return IsValid(Character) && Character->GetClass()->GetPathName().Equals(
			ExpectedClassPath) &&
			IsValid(Character->GetPlayerState()) &&
			IsValid(Character->GetRpgAbilitySystemComponent());
	}

	bool IsPilotCharacterReady(ARpgCharacter* Character)
	{
		return IsCharacterReadyForClassPath(Character, PilotCharacterClassPath) &&
			IsValid(GetPilotAnimInstance(Character));
	}

	bool IsPrototypeCharacterReady(ARpgCharacter* Character)
	{
		return IsCharacterReadyForClassPath(
			Character,
			PrototypeCharacterClassPath);
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
		ERpgLocomotionGait CharacterCoastGait = ERpgLocomotionGait::Run;
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
			ReadObjectProperty(
				Character,
				TEXT("GroundCoastGait"),
				CharacterCoastGait) &&
			AnimGait == ERpgLocomotionGait::Idle &&
			CharacterCoastGait == ERpgLocomotionGait::Idle &&
			!bHasAcceleration && WorldAcceleration.Size2D() <= 5.0f &&
			MovementComponent->GetCurrentAcceleration().Size2D() <= 5.0f &&
			MovementComponent->GetAnalogInputModifier() <= 0.01f &&
			MovementComponent->GetDesiredGait() == ERpgLocomotionGait::Idle &&
			MovementComponent->GetGroundGait() == ERpgLocomotionGait::Idle &&
			MovementComponent->GetReplicatedGroundCoastGait() ==
				ERpgLocomotionGait::Idle &&
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
		const float ExpectedScale,
		const ERpgLocomotionGait ExpectedGait)
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
			!FMath::IsNearlyEqual(State.StableInputScale, ClampedScale, 0.001f) ||
			State.StableInputGait != ExpectedGait)
		{
			State.StableInputScale = bMatches ? ClampedScale : -1.0f;
			State.StableInputGait = bMatches
				? ExpectedGait
				: ERpgLocomotionGait::Idle;
			State.StableInputStartTime = bMatches ? FPlatformTime::Seconds() : 0.0;
			return false;
		}

		return Now - State.StableInputStartTime >= 0.2;
	}

	bool HasExpectedMovementInput(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole,
		const FVector& ExpectedDirection,
		const float ExpectedScale)
	{
		const ERpgLocomotionGait ExpectedGait = ExpectedScale >= 0.7f
			? ERpgLocomotionGait::Run
			: ERpgLocomotionGait::Walk;
		return HasExpectedMovementInput(
			State,
			ExpectedLocalRole,
			ExpectedDirection,
			ExpectedScale,
			ExpectedGait);
	}

	bool ApplyDeterministicCoastProfile(
		FNetworkState& State,
		const float BrakingDecelerationWithoutInput)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		if (!MovementComponent ||
			!FMath::IsFinite(BrakingDecelerationWithoutInput) ||
			BrakingDecelerationWithoutInput < 1.0f)
		{
			return false;
		}

		FRpgCharacterMovementProfile TestProfile =
			MovementComponent->GetMovementProfile();
		if (!TestProfile.bOverrideCharacterMovement)
		{
			return false;
		}

		const bool bCapturedBaseline = !State.bHasBaselineCoastProfile;
		if (bCapturedBaseline)
		{
			State.BaselineCoastProfile = TestProfile;
			State.bHasBaselineCoastProfile = true;
		}

		TestProfile.bUseSeparateBrakingFriction = false;
		TestProfile.BrakingFrictionFactor = 0.0f;
		TestProfile.BrakingDecelerationWithoutInput =
			BrakingDecelerationWithoutInput;
		if (!MovementComponent->ApplyMovementProfile(TestProfile))
		{
			if (bCapturedBaseline)
			{
				State.bHasBaselineCoastProfile = false;
			}
			return false;
		}
		return true;
	}

	bool SetDeterministicCoastVelocity(
		FNetworkState& State,
		const float GroundSpeed,
		const FVector& Direction = FVector::YAxisVector)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		if (!MovementComponent || !MovementComponent->IsMovingOnGround() ||
			!FMath::IsFinite(GroundSpeed) || GroundSpeed <= 0.0f ||
			Direction.IsNearlyZero())
		{
			return false;
		}

		MovementComponent->Velocity = Direction.GetSafeNormal2D() * GroundSpeed;
		MovementComponent->UpdateComponentVelocity();
		if (Character->HasAuthority())
		{
			MovementComponent->ForceReplicationUpdate();
			Character->ForceNetUpdate();
		}
		return true;
	}

	bool RestorePilotProfileAndStop(FNetworkState& State)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		if (!MovementComponent || !State.bHasBaselineCoastProfile ||
			!MovementComponent->ApplyMovementProfile(State.BaselineCoastProfile))
		{
			return false;
		}
		State.bHasBaselineCoastProfile = false;

		MovementComponent->StopMovementImmediately();
		MovementComponent->UpdateComponentVelocity();
		if (Character->HasAuthority())
		{
			MovementComponent->ForceReplicationUpdate();
			Character->ForceNetUpdate();
		}
		return true;
	}

	bool HasExpectedCoastGait(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole,
		const ERpgLocomotionGait ExpectedGait,
		const bool bRequireBelowWalkCap)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement())
			: nullptr;
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		ERpgLocomotionGait AnimGait = ERpgLocomotionGait::Idle;
		ERpgLocomotionGait CharacterCoastGait = ERpgLocomotionGait::Idle;
		const float GroundSpeed = Character
			? Character->GetVelocity().Size2D()
			: 0.0f;
		const float WalkCap = MovementComponent
			? MovementComponent->GetMovementProfile().WalkSpeeds.Forward
			: 0.0f;
		const ERpgLocomotionGait ExpectedCharacterCoastGait =
			ExpectedLocalRole == ROLE_AutonomousProxy
				? ERpgLocomotionGait::Idle
				: ExpectedGait;
		const ERpgLocomotionGait ExpectedMovementHint =
			ExpectedLocalRole == ROLE_SimulatedProxy
				? ExpectedGait
				: ERpgLocomotionGait::Idle;
		const FRepMovement* ReplicatedMovement = Character
			? &Character->GetReplicatedMovement()
			: nullptr;
		const bool bHasNativeZeroSnapshot =
			ExpectedLocalRole != ROLE_SimulatedProxy ||
			(ReplicatedMovement &&
			 ReplicatedMovement->bRepAcceleration &&
			 ReplicatedMovement->Acceleration.Size2D() <= 5.0f);
		const bool bReadAnimGait = ReadAnimProperty(
			AnimInstance,
			TEXT("LocomotionGait"),
			AnimGait);
		const bool bReadCharacterCoastGait = ReadObjectProperty(
			Character,
			TEXT("GroundCoastGait"),
			CharacterCoastGait);
		const bool bMatches = Character && MovementComponent && AnimInstance &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			MovementComponent->IsMovingOnGround() &&
			MovementComponent->GetMovementProfile().bOverrideCharacterMovement &&
			GroundSpeed >= MovementComponent->GetMovementProfile().StationarySpeedThreshold &&
			(!bRequireBelowWalkCap || GroundSpeed < WalkCap - 5.0f) &&
			MovementComponent->GetCurrentAcceleration().Size2D() <= 5.0f &&
			MovementComponent->GetAnalogInputModifier() <= 0.01f &&
			MovementComponent->GetDesiredGait() == ERpgLocomotionGait::Idle &&
			MovementComponent->GetGroundGait() == ExpectedGait &&
			MovementComponent->GetReplicatedGroundCoastGait() ==
				ExpectedMovementHint &&
			!MovementComponent->HasMoveIntent() &&
			bReadAnimGait && AnimGait == ExpectedGait &&
			bReadCharacterCoastGait &&
			CharacterCoastGait == ExpectedCharacterCoastGait &&
			bHasNativeZeroSnapshot;

		const double Now = FPlatformTime::Seconds();
		if (!bMatches && Now - State.LastCoastDiagnosticTime >= 1.0)
		{
			State.LastCoastDiagnosticTime = Now;
			UE_LOG(
				LogTemp,
				Display,
				TEXT("GASP coast mismatch: expected role=%d gait=%d belowWalk=%d | character=%d role=%d speed=%.1f walkCap=%.1f move=%d ground=%d desired=%d intent=%d accel=%.1f analog=%.3f hint=%d transportRead=%d transport=%d animRead=%d anim=%d nativeZero=%d"),
				static_cast<int32>(ExpectedLocalRole),
				static_cast<int32>(ExpectedGait),
				static_cast<int32>(bRequireBelowWalkCap),
				static_cast<int32>(Character != nullptr),
				Character ? static_cast<int32>(Character->GetLocalRole()) : -1,
				static_cast<double>(GroundSpeed),
				static_cast<double>(WalkCap),
				static_cast<int32>(MovementComponent != nullptr),
				MovementComponent ? static_cast<int32>(MovementComponent->GetGroundGait()) : -1,
				MovementComponent ? static_cast<int32>(MovementComponent->GetDesiredGait()) : -1,
				static_cast<int32>(MovementComponent ? MovementComponent->HasMoveIntent() : false),
				static_cast<double>(MovementComponent ? MovementComponent->GetCurrentAcceleration().Size2D() : -1.0f),
				static_cast<double>(MovementComponent ? MovementComponent->GetAnalogInputModifier() : -1.0f),
				MovementComponent ? static_cast<int32>(MovementComponent->GetReplicatedGroundCoastGait()) : -1,
				static_cast<int32>(bReadCharacterCoastGait),
				static_cast<int32>(CharacterCoastGait),
				static_cast<int32>(bReadAnimGait),
				static_cast<int32>(AnimGait),
				static_cast<int32>(bHasNativeZeroSnapshot));
		}

		if (!bMatches || State.StableCoastSubject.Get() != Character ||
			State.StableCoastRole != ExpectedLocalRole ||
			State.StableCoastGait != ExpectedGait ||
			State.bStableCoastBelowWalkCap != bRequireBelowWalkCap)
		{
			State.StableCoastSubject = bMatches ? Character : nullptr;
			State.StableCoastRole = bMatches ? ExpectedLocalRole : ROLE_None;
			State.StableCoastGait = bMatches
				? ExpectedGait
				: ERpgLocomotionGait::Idle;
			State.bStableCoastBelowWalkCap = bMatches && bRequireBelowWalkCap;
			State.StableCoastStartTime = bMatches ? Now : 0.0;
			return false;
		}

		return Now - State.StableCoastStartTime >= 0.2;
	}

	bool HasSubjectActorChannel(
		FNetworkState& State,
		const int32 ClientIndex,
		const bool bExpectedOpen)
	{
		ARpgCharacter* Subject = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		UNetConnection* Connection = State.ClientConnections.IsValidIndex(ClientIndex)
			? State.ClientConnections[ClientIndex]
			: nullptr;
		return Subject && IsValid(Connection) &&
			Connection->ContainsActorChannel(TWeakObjectPtr<AActor>(Subject)) ==
				bExpectedOpen;
	}

	bool HasStablePhysicalDeadzone(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole,
		const float ExpectedInputScale)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		const float ClampedScale = FMath::Clamp(ExpectedInputScale, 0.0f, 1.0f);
		const bool bMatches = Character &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			HasStoppedAnimation(State);
		const bool bSameInput = FMath::IsNearlyEqual(
			State.StableDeadzoneInputScale,
			ClampedScale,
			0.001f);
		if (!bMatches || !bSameInput)
		{
			State.StableDeadzoneInputScale = bMatches ? ClampedScale : -1.0f;
			State.PhysicalDeadzoneStableStartTime = bMatches
				? State.World->GetTimeSeconds()
				: -1.0;
			State.PhysicalDeadzoneAnchorLocation = bMatches
				? Character->GetActorLocation()
				: FVector::ZeroVector;
			return false;
		}

		const double Now = State.World->GetTimeSeconds();
		if (FVector::Dist2D(
				State.PhysicalDeadzoneAnchorLocation,
				Character->GetActorLocation()) > 2.0f)
		{
			State.PhysicalDeadzoneStableStartTime = Now;
			State.PhysicalDeadzoneAnchorLocation = Character->GetActorLocation();
			return false;
		}

		return Now - State.PhysicalDeadzoneStableStartTime >= 0.25;
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

	bool IsSubjectOnMovingBase(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		return Character &&
			IsValid(State.MovingBase) &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			Character->GetCharacterMovement()->IsMovingOnGround() &&
			Character->GetMovementBaseObject() ==
				State.MovingBase->GetMovementSurface();
	}

	bool HasObservedMovingBaseCorrection(FNetworkState& State)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgCharacterMovementComponent* MovementComponent = Character
			? Cast<URpgCharacterMovementComponent>(
				Character->GetCharacterMovement())
			: nullptr;
		const FMovementBaseInterfaceData ExpectedMovementBase(
			IsValid(State.MovingBase)
				? State.MovingBase->GetMovementSurface()
				: nullptr);
		return MovementComponent &&
			MovementComponent->GetClientCorrectionReceivedCountForTests() >
				State.ClientCorrectionCountBaseline &&
			MovementComponent->WasLastClientCorrectionBaseRelativeForTests(
				&ExpectedMovementBase,
				NAME_None);
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

	bool HasCombatAnimationPresentation(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole,
		const FName ExpectedProfileName,
		const bool bExpectedCombatReady)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		const URpgEquipmentManagerComponent* EquipmentManager = Character
			? Character->GetEquipmentManagerComponent()
			: nullptr;
		const URpgWeaponInstance* MainHandWeapon = EquipmentManager
			? Cast<URpgWeaponInstance>(EquipmentManager->GetEquipmentInstanceInSlot(
				ERpgEquipmentSlot::MainHand))
			: nullptr;
		const URpgWeaponInstance* OffHandWeapon = EquipmentManager
			? Cast<URpgWeaponInstance>(EquipmentManager->GetEquipmentInstanceInSlot(
				ERpgEquipmentSlot::OffHand))
			: nullptr;
		static const FGameplayTag OneHandSwordAnimationTag =
			FGameplayTag::RequestGameplayTag(
				TEXT("Equipment.AnimationProfile.OneHandSword"));
		static const FGameplayTag ShieldAnimationTag =
			FGameplayTag::RequestGameplayTag(
				TEXT("Equipment.AnimationProfile.Shield"));
		const bool bHasOneHandSword = MainHandWeapon &&
			MainHandWeapon->GetEquipmentTraitTags().HasTagExact(
				OneHandSwordAnimationTag);
		const bool bEquipmentMatches =
			(ExpectedProfileName == SwordShieldProfileName &&
			 bHasOneHandSword && OffHandWeapon &&
			 OffHandWeapon->GetEquipmentTraitTags().HasTagExact(
				 ShieldAnimationTag)) ||
			(ExpectedProfileName == OneHandSwordProfileName &&
			 bHasOneHandSword && !OffHandWeapon) ||
			(ExpectedProfileName != SwordShieldProfileName &&
			 ExpectedProfileName != OneHandSwordProfileName);
		FName ProfileName = NAME_None;
		float OverlayAlpha = 0.0f;
		bool bCombatReady = !bExpectedCombatReady;
		bool bFallback = true;
		TObjectPtr<UAnimSequence> EquippedAnimation = nullptr;
		TObjectPtr<UAnimSequence> CombatReadyAnimation = nullptr;
		const bool bReadProfileName = ReadAnimProperty(
				AnimInstance,
				TEXT("CombatAnimationProfileName"),
				ProfileName);
		const bool bReadOverlayAlpha = ReadAnimProperty(
				AnimInstance,
				TEXT("CombatAnimationOverlayAlpha"),
				OverlayAlpha);
		const bool bReadCombatReady = ReadAnimProperty(
				AnimInstance,
				TEXT("bCombatAnimationReady"),
				bCombatReady);
		const bool bReadFallback = ReadAnimProperty(
				AnimInstance,
				TEXT("bCombatAnimationProfileFallback"),
				bFallback);
		const bool bReadEquippedAnimation = ReadAnimProperty(
				AnimInstance,
				TEXT("CombatEquippedUpperBodyAnimation"),
				EquippedAnimation);
		const bool bReadCombatReadyAnimation = ReadAnimProperty(
				AnimInstance,
				TEXT("CombatReadyUpperBodyAnimation"),
				CombatReadyAnimation);
		const bool bMatches = Character &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			bEquipmentMatches &&
			bReadProfileName && bReadOverlayAlpha && bReadCombatReady &&
			bReadFallback && bReadEquippedAnimation &&
			bReadCombatReadyAnimation &&
			ProfileName == ExpectedProfileName &&
			FMath::IsNearlyEqual(OverlayAlpha, 1.0f, 0.01f) &&
			bCombatReady == bExpectedCombatReady &&
			!bFallback && EquippedAnimation && CombatReadyAnimation;
		const double Now = FPlatformTime::Seconds();
		if (!bMatches && Now - State.LastCombatAnimationDiagnosticTime >= 1.0)
		{
			State.LastCombatAnimationDiagnosticTime = Now;
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Combat presentation mismatch: expectedRole=%d expectedProfile=%s expectedReady=%d | character=%d role=%d anim=%d equipment=%d main=%s mainTraits=%s off=%s offTraits=%s profileRead=%d profile=%s alphaRead=%d alpha=%.3f readyRead=%d ready=%d fallbackRead=%d fallback=%d equippedRead=%d equipped=%s combatRead=%d combat=%s"),
				static_cast<int32>(ExpectedLocalRole),
				*ExpectedProfileName.ToString(),
				static_cast<int32>(bExpectedCombatReady),
				static_cast<int32>(Character != nullptr),
				Character ? static_cast<int32>(Character->GetLocalRole()) : -1,
				static_cast<int32>(AnimInstance != nullptr),
				static_cast<int32>(bEquipmentMatches),
				*GetNameSafe(MainHandWeapon),
				MainHandWeapon
					? *MainHandWeapon->GetEquipmentTraitTags().ToStringSimple()
					: TEXT("None"),
				*GetNameSafe(OffHandWeapon),
				OffHandWeapon
					? *OffHandWeapon->GetEquipmentTraitTags().ToStringSimple()
					: TEXT("None"),
				static_cast<int32>(bReadProfileName),
				*ProfileName.ToString(),
				static_cast<int32>(bReadOverlayAlpha),
				static_cast<double>(OverlayAlpha),
				static_cast<int32>(bReadCombatReady),
				static_cast<int32>(bCombatReady),
				static_cast<int32>(bReadFallback),
				static_cast<int32>(bFallback),
				static_cast<int32>(bReadEquippedAnimation),
				*GetNameSafe(EquippedAnimation),
				static_cast<int32>(bReadCombatReadyAnimation),
				*GetNameSafe(CombatReadyAnimation));
		}
		return bMatches;
	}

	int32 CountRegisteredCombatAnimationProfileProviders(
		const ARpgCharacter* Character)
	{
		if (!IsValid(Character))
		{
			return 0;
		}

		TInlineComponentArray<URpgCombatAnimationProfileProviderComponent*> Providers(
			Character);
		int32 RegisteredProviderCount = 0;
		for (const URpgCombatAnimationProfileProviderComponent* Provider : Providers)
		{
			if (IsValid(Provider) && Provider->IsRegistered())
			{
				++RegisteredProviderCount;
			}
		}
		return RegisteredProviderCount;
	}

	bool HasBoundCombatAnimationFeatureProfile(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		const URpgCombatAnimationProfileProviderComponent* Provider =
			URpgCombatAnimationProfileProviderComponent::FindForActor(Character);
		TObjectPtr<URpgCombatAnimationProfile> ActiveProfile = nullptr;
		return Character && AnimInstance && Provider &&
			Character == State.CombatFeatureLifecycleSubject.Get() &&
			AnimInstance == State.CombatFeatureLifecycleAnimInstance.Get() &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			CountRegisteredCombatAnimationProfileProviders(Character) == 1 &&
			ReadObjectProperty(
				AnimInstance,
				TEXT("ActiveCombatAnimationProfileSource"),
				ActiveProfile) &&
			ActiveProfile == Provider->GetCombatAnimationProfile() &&
			HasCombatAnimationPresentation(
				State,
				ExpectedLocalRole,
				SwordShieldProfileName,
				false);
	}

	bool HasNeutralCombatAnimationFeatureProfile(
		FNetworkState& State,
		const ENetRole ExpectedLocalRole)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		URpgAnimInstance* AnimInstance = GetPilotAnimInstance(Character);
		TObjectPtr<URpgCombatAnimationProfile> ActiveProfile = nullptr;
		TObjectPtr<UAnimSequence> EquippedAnimation = nullptr;
		TObjectPtr<UAnimSequence> CombatReadyAnimation = nullptr;
		FName ProfileName = NAME_None;
		float OverlayAlpha = 1.0f;
		bool bCombatReady = true;
		bool bFallback = false;
		return Character && AnimInstance &&
			Character == State.CombatFeatureLifecycleSubject.Get() &&
			AnimInstance == State.CombatFeatureLifecycleAnimInstance.Get() &&
			Character->GetLocalRole() == ExpectedLocalRole &&
			CountRegisteredCombatAnimationProfileProviders(Character) == 0 &&
			!URpgCombatAnimationProfileProviderComponent::FindForActor(Character) &&
			ReadObjectProperty(
				AnimInstance,
				TEXT("ActiveCombatAnimationProfileSource"),
				ActiveProfile) &&
			!ActiveProfile &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("CombatAnimationProfileName"),
				ProfileName) &&
			ProfileName == UnarmedProfileName &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("CombatAnimationOverlayAlpha"),
				OverlayAlpha) &&
			FMath::IsNearlyZero(OverlayAlpha) &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("bCombatAnimationReady"),
				bCombatReady) &&
			!bCombatReady &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("bCombatAnimationProfileFallback"),
				bFallback) &&
			bFallback &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("CombatEquippedUpperBodyAnimation"),
				EquippedAnimation) &&
			!EquippedAnimation &&
			ReadAnimProperty(
				AnimInstance,
				TEXT("CombatReadyUpperBodyAnimation"),
				CombatReadyAnimation) &&
			!CombatReadyAnimation;
	}

	bool SetCombatFeatureLifecycleMeshTickEnabled(
		FNetworkState& State,
		const bool bEnabled)
	{
		ARpgCharacter* Character = FindCharacterByPlayerId(
			State.World,
			State.SubjectPlayerId);
		USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		if (!Mesh)
		{
			return false;
		}

		Mesh->SetComponentTickEnabled(bEnabled);
		return Mesh->IsComponentTickEnabled() == bEnabled;
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
	float DivergentClientMoveTimeStamp = -1.0f;
	FVector AuthorityTeleportTarget = FVector::ZeroVector;
	FVector AuthorityMontageEnd = FVector::ZeroVector;
	int32 SubjectPlayerId = INDEX_NONE;
	bool bOriginalDiskPersistence = true;
	UClass* PilotGameModeClass = nullptr;
	FString CombatCorePluginURL;
	bool bPluginTransitionComplete = false;
	bool bPluginTransitionSucceeded = false;

	BEFORE_EACH()
	{
		using namespace RpgGaspPIENetworkTests;
		CombatCorePluginURL.Reset();
		bPluginTransitionComplete = false;
		bPluginTransitionSucceeded = false;
		DivergentClientMoveTimeStamp = -1.0f;
		TestCommandBuilder.OnTearDown(
			TEXT("Stop GASP network test timers"),
			[]()
			{
				ClearActiveTimers();
			});
		TestCommandBuilder.OnTearDown(
			TEXT("Restore GF_Combat_Core after the profile lifecycle test"),
			[this]()
			{
				if (!CombatCorePluginURL.IsEmpty() &&
					UGameFeaturesSubsystem::Get().GetPluginState(CombatCorePluginURL) !=
						EGameFeaturePluginState::Active)
				{
					UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
						CombatCorePluginURL,
						FGameFeaturePluginLoadComplete());
				}
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
		DeveloperSettings->ExperienceOverride = FPrimaryAssetId();
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

	TEST_METHOD(DefaultExperienceFallbackSelectsGasp)
	{
		using namespace RpgGaspPIENetworkTests;

		Network
			.UntilServer(
				TEXT("Global fallback selects the GASP Experience and host pawn without an override"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Default GASP Experience and pawn composition resolve on the client"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout());
	}

	TEST_METHOD(CombatProfileGameFeatureDeactivationReactivation)
	{
		using namespace RpgGaspPIENetworkTests;

		Network
			.UntilServer(
				TEXT("Pilot Experience loads before the combat-profile feature lifecycle"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial client loads the Pilot Experience before the feature lifecycle"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Bind the listen-server host pawn and resolve GF_Combat_Core"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(Character->GetPlayerState()));
					SubjectPlayerId = Character->GetPlayerState()->GetPlayerId();
					ASSERT_THAT(IsTrue(SubjectPlayerId != INDEX_NONE));
					State.SubjectPlayerId = SubjectPlayerId;
					State.CombatFeatureLifecycleSubject = Character;
					State.CombatFeatureLifecycleAnimInstance =
						GetPilotAnimInstance(Character);
					if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(
							TEXT("GF_Combat_Core"),
							CombatCorePluginURL))
					{
						TestRunner->AddError(
							TEXT("Could not resolve GF_Combat_Core plugin URL."));
					}
				})
			.ThenClients(
				TEXT("Bind the same host pawn in every client world"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					State.CombatFeatureLifecycleSubject = Character;
					State.CombatFeatureLifecycleAnimInstance =
						GetPilotAnimInstance(Character);
				})
			.ThenServer(
				TEXT("Equip the deterministic SwordShield lifecycle fixture on authority"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = State.CombatFeatureLifecycleSubject.Get();
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsTrue(Character && Character->HasAuthority()));
					URpgEquipmentManagerComponent* EquipmentManager = Character
						? Character->GetEquipmentManagerComponent()
						: nullptr;
					const TSubclassOf<URpgEquipmentDefinition> SwordDefinition =
						LoadClass<URpgEquipmentDefinition>(
							nullptr,
							BasicSwordDefinitionClassPath);
					const TSubclassOf<URpgEquipmentDefinition> ShieldDefinition =
						LoadClass<URpgEquipmentDefinition>(
							nullptr,
							BasicShieldDefinitionClassPath);
					ASSERT_THAT(IsNotNull(EquipmentManager));
					ASSERT_THAT(IsNotNull(SwordDefinition.Get()));
					ASSERT_THAT(IsNotNull(ShieldDefinition.Get()));
					if (!Character || !Character->HasAuthority() ||
						!EquipmentManager || !SwordDefinition || !ShieldDefinition)
					{
						return;
					}

					EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
					EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::OffHand);
					ASSERT_THAT(IsNotNull(EquipmentManager->EquipItemInSlot(
						SwordDefinition,
						ERpgEquipmentSlot::MainHand)));
					ASSERT_THAT(IsNotNull(EquipmentManager->EquipItemInSlot(
						ShieldDefinition,
						ERpgEquipmentSlot::OffHand)));
					Character->ForceNetUpdate();
				})
			.UntilServer(
				TEXT("Authority resolves the explicit SwordShield fixture through one profile provider"),
				[](FNetworkState& State)
				{
					return HasBoundCombatAnimationFeatureProfile(
						State,
						ROLE_Authority);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Simulated proxy receives the replicated SwordShield animation presentation"),
				[](FNetworkState& State)
				{
					return HasBoundCombatAnimationFeatureProfile(
						State,
						ROLE_SimulatedProxy);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Disable the authority mesh tick before feature removal"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(
						SetCombatFeatureLifecycleMeshTickEnabled(State, false)));
				})
			.ThenClients(
				TEXT("Disable the simulated-proxy mesh tick before feature removal"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(
						SetCombatFeatureLifecycleMeshTickEnabled(State, false)));
				})
			.ThenServer(
				TEXT("Deactivate GF_Combat_Core with both subject meshes unable to poll"),
				[this](FNetworkState& State)
				{
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(
						CombatCorePluginURL,
						FGameFeaturePluginDeactivateComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("GF_Combat_Core deactivation completes"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Record the feature-deactivation result without skipping cleanup"),
				[this](FNetworkState& State)
				{
					if (!bPluginTransitionSucceeded)
					{
						TestRunner->AddError(
							TEXT("GF_Combat_Core deactivation failed."));
					}
				})
			.UntilServer(
				TEXT("Authority releases the profile synchronously without a mesh tick"),
				[this](FNetworkState& State)
				{
					return !bPluginTransitionSucceeded ||
						HasNeutralCombatAnimationFeatureProfile(
							State,
							ROLE_Authority);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Simulated proxy releases the profile synchronously without a mesh tick"),
				[this](FNetworkState& State)
				{
					return !bPluginTransitionSucceeded ||
						HasNeutralCombatAnimationFeatureProfile(
							State,
							ROLE_SimulatedProxy);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Reactivate GF_Combat_Core"),
				[this](FNetworkState& State)
				{
					bPluginTransitionComplete = false;
					bPluginTransitionSucceeded = false;
					UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
						CombatCorePluginURL,
						FGameFeaturePluginLoadComplete::CreateLambda(
							[this](const UE::GameFeatures::FResult& Result)
							{
								bPluginTransitionSucceeded = !Result.HasError();
								bPluginTransitionComplete = true;
							}));
				})
			.UntilServer(
				TEXT("GF_Combat_Core reactivation completes"),
				[this](FNetworkState& State)
				{
					return bPluginTransitionComplete;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Record the feature-reactivation result without skipping tick restoration"),
				[this](FNetworkState& State)
				{
					if (!bPluginTransitionSucceeded)
					{
						TestRunner->AddError(
							TEXT("GF_Combat_Core reactivation failed."));
					}
				})
			.UntilServer(
				TEXT("Reactivation registers one authority provider while the mesh stays frozen"),
				[this](FNetworkState& State)
				{
					return !bPluginTransitionSucceeded ||
						CountRegisteredCombatAnimationProfileProviders(
							State.CombatFeatureLifecycleSubject.Get()) == 1;
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Reactivation registers one simulated-proxy provider while the mesh stays frozen"),
				[this](FNetworkState& State)
				{
					return !bPluginTransitionSucceeded ||
						CountRegisteredCombatAnimationProfileProviders(
							State.CombatFeatureLifecycleSubject.Get()) == 1;
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Restore the authority mesh tick"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(
						SetCombatFeatureLifecycleMeshTickEnabled(State, true)));
				})
			.ThenClients(
				TEXT("Restore the simulated-proxy mesh tick"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(
						SetCombatFeatureLifecycleMeshTickEnabled(State, true)));
				})
			.UntilServer(
				TEXT("Authority reacquires the feature profile on the same pawn and AnimInstance"),
				[this](FNetworkState& State)
				{
					return !bPluginTransitionSucceeded ||
						HasBoundCombatAnimationFeatureProfile(
							State,
							ROLE_Authority);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Simulated proxy reacquires the feature profile without duplicates"),
				[this](FNetworkState& State)
				{
					return !bPluginTransitionSucceeded ||
						HasBoundCombatAnimationFeatureProfile(
							State,
							ROLE_SimulatedProxy);
				},
				NetworkTimeout());
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
				TEXT("Authority applies Free rotation with the SwordShield presentation"),
				[](FNetworkState& State)
				{
					return HasFreeRotationPolicy(State, ROLE_Authority) &&
						HasCombatAnimationPresentation(
							State,
							ROLE_Authority,
							SwordShieldProfileName,
							false);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner applies Free rotation with the SwordShield presentation"),
				0,
				[](FNetworkState& State)
				{
					return HasFreeRotationPolicy(State, ROLE_AutonomousProxy) &&
						HasCombatAnimationPresentation(
							State,
							ROLE_AutonomousProxy,
							SwordShieldProfileName,
							false);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Moving late join reconstructs Free rotation and SwordShield presentation"),
				1,
				[](FNetworkState& State)
				{
					return HasFreeRotationPolicy(State, ROLE_SimulatedProxy) &&
						HasCombatAnimationPresentation(
							State,
							ROLE_SimulatedProxy,
							SwordShieldProfileName,
							false);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Unequip the replicated starter Shield"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgEquipmentManagerComponent* EquipmentManager = Character
						? Character->GetEquipmentManagerComponent()
						: nullptr;
					ASSERT_THAT(IsNotNull(EquipmentManager));
					if (EquipmentManager)
					{
						EquipmentManager->UnequipItemInSlot(
							ERpgEquipmentSlot::OffHand);
						Character->ForceNetUpdate();
					}
				})
			.UntilServer(
				TEXT("Authority blends from SwordShield to OneHandSword after unequip"),
				[](FNetworkState& State)
				{
					return HasCombatAnimationPresentation(
						State,
						ROLE_Authority,
						OneHandSwordProfileName,
						false);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Shield unequip and OneHandSword presentation reach owner and simulated proxy"),
				[](FNetworkState& State)
				{
					return HasCombatAnimationPresentation(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						OneHandSwordProfileName,
						false);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Re-equip the Shield into its authored OffHand slot"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgEquipmentManagerComponent* EquipmentManager = Character
						? Character->GetEquipmentManagerComponent()
						: nullptr;
					const TSubclassOf<URpgEquipmentDefinition> ShieldDefinition =
						LoadClass<URpgEquipmentDefinition>(
							nullptr,
							BasicShieldDefinitionClassPath);
					ASSERT_THAT(IsNotNull(EquipmentManager));
					ASSERT_THAT(IsNotNull(ShieldDefinition.Get()));
					if (EquipmentManager && ShieldDefinition)
					{
						ASSERT_THAT(IsNotNull(
							EquipmentManager->EquipItemInSlot(
								ShieldDefinition,
								ERpgEquipmentSlot::OffHand)));
						Character->ForceNetUpdate();
					}
				})
			.UntilServer(
				TEXT("Authority restores SwordShield after OffHand re-equip"),
				[](FNetworkState& State)
				{
					return HasCombatAnimationPresentation(
						State,
						ROLE_Authority,
						SwordShieldProfileName,
						false);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Shield re-equip and SwordShield presentation reach owner and simulated proxy"),
				[](FNetworkState& State)
				{
					return HasCombatAnimationPresentation(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						SwordShieldProfileName,
						false);
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
				TEXT("Authority resolves Aim and combat-ready SwordShield presentation"),
				[](FNetworkState& State)
				{
					return HasRotationMode(State, ERpgCharacterRotationMode::Aim) &&
						HasCombatAnimationPresentation(
							State,
							ROLE_Authority,
							SwordShieldProfileName,
							true);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Aim and combat-ready SwordShield reach owner and simulated proxy"),
				[](FNetworkState& State)
				{
					return HasRotationMode(State, ERpgCharacterRotationMode::Aim) &&
						HasCombatAnimationPresentation(
							State,
							State.ClientIndex == 0
								? ROLE_AutonomousProxy
								: ROLE_SimulatedProxy,
							SwordShieldProfileName,
							true) &&
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
			.UntilServer(
				TEXT("Authority restores CombatStrafe with combat-ready SwordShield"),
				[](FNetworkState& State)
				{
					return HasRotationMode(
							State,
							ERpgCharacterRotationMode::CombatStrafe) &&
						HasCombatAnimationPresentation(
							State,
							ROLE_Authority,
							SwordShieldProfileName,
							true);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("CombatStrafe and combat-ready SwordShield reach both clients"),
				[](FNetworkState& State)
				{
					return HasRotationMode(
						State,
						ERpgCharacterRotationMode::CombatStrafe) &&
						HasCombatAnimationPresentation(
							State,
							State.ClientIndex == 0
								? ROLE_AutonomousProxy
								: ROLE_SimulatedProxy,
							SwordShieldProfileName,
							true);
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

					if (State.ClientIndex == 0)
					{
						ARpgCharacter* Character = FindCharacterByPlayerId(
							State.World,
							State.SubjectPlayerId);
						URpgCharacterMovementComponent* MovementComponent = Character
							? Cast<URpgCharacterMovementComponent>(
								Character->GetCharacterMovement())
							: nullptr;
						ASSERT_THAT(IsNotNull(MovementComponent));
						if (MovementComponent)
						{
							State.ClientCorrectionCountBaseline =
								MovementComponent->GetClientCorrectionReceivedCountForTests();
							State.LargeClientCorrectionCountBaseline =
								MovementComponent->GetLargeClientCorrectionReceivedCountForTests();
							State.AnimationDiscontinuityBaseline =
								MovementComponent->GetAnimationDiscontinuitySerial();
						}
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
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(
							Character->GetCharacterMovement())
						: nullptr;
					ASSERT_THAT(IsNotNull(MovementComponent));
					const float DivergenceDistance = MovementComponent
						? FMath::Max(
							MovementComponent->NetworkLargeClientCorrectionDistance + 25.0f,
							100.0f)
						: 100.0f;
					const FVector DivergentLocation =
						Character->GetActorLocation() +
							FVector(DivergenceDistance, 0.0, 0.0);
					Character->SetActorLocation(
						DivergentLocation,
						false,
						nullptr,
						ETeleportType::None);
					ASSERT_THAT(IsTrue(FMath::Abs(
						Character->GetActorLocation().X -
							AuthorityCorrectionBaseline.X) >
								(MovementComponent
									? MovementComponent->NetworkLargeClientCorrectionDistance
									: 0.0f)));
					if (MovementComponent)
					{
						ASSERT_THAT(IsTrue(
							MovementComponent->GetAnimationDiscontinuitySerial() ==
								State.AnimationDiscontinuityBaseline));
					}
					StartMovementInput(State, FVector::YAxisVector);
				})
			.UntilClient(
				TEXT("Autonomous owner records the divergent SavedMove"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(
							Character->GetCharacterMovement())
						: nullptr;
					FNetworkPredictionData_Client_Character* ClientPrediction =
						MovementComponent
							? MovementComponent->GetPredictionData_Client_Character()
							: nullptr;
					if (!ClientPrediction)
					{
						return false;
					}

					for (int32 MoveIndex = ClientPrediction->SavedMoves.Num() - 1;
						 MoveIndex >= 0;
						 --MoveIndex)
					{
						const FSavedMovePtr& SavedMove =
							ClientPrediction->SavedMoves[MoveIndex];
						if (SavedMove.IsValid() &&
							SavedMove != ClientPrediction->PendingMove &&
							FMath::Abs(
								SavedMove->SavedLocation.X -
									AuthorityCorrectionBaseline.X) >
									MovementComponent->NetworkLargeClientCorrectionDistance &&
							FVector::DotProduct(
								SavedMove->Acceleration.GetSafeNormal2D(),
								FVector::YAxisVector) > 0.98)
						{
							DivergentClientMoveTimeStamp = SavedMove->TimeStamp;
							return true;
						}
					}
					return false;
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Authority processes the captured divergent SavedMove"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(
							Character->GetCharacterMovement())
						: nullptr;
					FNetworkPredictionData_Server_Character* ServerPrediction =
						MovementComponent
							? MovementComponent->GetPredictionData_Server_Character()
							: nullptr;
					const FVector AccelerationDirection = MovementComponent
						? MovementComponent->GetCurrentAcceleration().GetSafeNormal2D()
						: FVector::ZeroVector;
					return DivergentClientMoveTimeStamp > 0.0f &&
						ServerPrediction &&
						ServerPrediction->CurrentClientTimeStamp + UE_KINDA_SMALL_NUMBER >=
							DivergentClientMoveTimeStamp &&
						FVector::DotProduct(
							AccelerationDirection,
							FVector::YAxisVector) > 0.98 &&
						MovementComponent->GetAnalogInputModifier() > 0.9f;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Autonomous owner receives the divergent-move correction"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(
							Character->GetCharacterMovement())
						: nullptr;
					return MovementComponent &&
						MovementComponent->GetClientCorrectionReceivedCountForTests() >
							State.ClientCorrectionCountBaseline &&
						MovementComponent->GetLargeClientCorrectionReceivedCountForTests() >
							State.LargeClientCorrectionCountBaseline &&
						MovementComponent->GetLastLargeClientCorrectionTimeStampForTests() +
							UE_KINDA_SMALL_NUMBER >= DivergentClientMoveTimeStamp;
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Stop movement immediately after the owner correction"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.UntilClient(
				TEXT("Autonomous movement marks the correction discontinuity"),
				0,
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(
							Character->GetCharacterMovement())
						: nullptr;
					return MovementComponent &&
						MovementComponent->GetAnimationDiscontinuitySerial() >
							State.AnimationDiscontinuityBaseline;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Autonomous presentation observes the server correction"),
				0,
				[](FNetworkState& State)
				{
					return HasAnimationResetDeltaAtLeast(State, 1);
				},
				NetworkTimeout())
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

					if (State.ClientIndex == 0)
					{
						ARpgCharacter* Character = FindCharacterByPlayerId(
							State.World,
							State.SubjectPlayerId);
						URpgCharacterMovementComponent* MovementComponent = Character
							? Cast<URpgCharacterMovementComponent>(
								Character->GetCharacterMovement())
							: nullptr;
						ASSERT_THAT(IsNotNull(MovementComponent));
						if (MovementComponent)
						{
							ASSERT_THAT(IsTrue(
								MovementComponent->GetAnimationDiscontinuitySerial() ==
									State.AnimationDiscontinuityBaseline + 1));
						}
					}
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
				TEXT("Stationary late join reconstructs zero input and combat-ready SwordShield"),
				2,
				[](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character &&
						Character->GetLocalRole() == ROLE_SimulatedProxy &&
						HasStoppedAnimation(State) &&
						HasCombatAnimationPresentation(
							State,
							ROLE_SimulatedProxy,
							SwordShieldProfileName,
							true);
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

	TEST_METHOD(MovingBaseCorrectionPreservesAnimationHistory)
	{
		using namespace RpgGaspPIENetworkTests;

		Network
			.SpawnAndReplicate<
				ARpgGaspNetworkMovingBaseFixture,
				&FNetworkState::MovingBase>(
				NetworkTimeout())
			.UntilServer(
				TEXT("Pilot Experience loads for the moving-base correction test"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Moving-base client loads the Pilot pawn"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World)) &&
						IsValid(State.MovingBase);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Capture the moving-base autonomous subject"),
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
				TEXT("Place the autonomous subject on the replicated moving base"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
					ASSERT_THAT(IsNotNull(State.MovingBase));
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (State.MovingBase && Character)
					{
						State.MovingBase->SetActorLocation(FVector(-1500.0f, 0.0f, 200.0f));
						State.MovingBase->ForceNetUpdate();
						const FVector RelativeCharacterLocation(
							200.0f,
							0.0f,
							25.0f + Character->GetSimpleCollisionHalfHeight() + 2.0f);
						const FVector CharacterLocation =
							State.MovingBase->GetActorTransform().TransformPosition(
								RelativeCharacterLocation);
						Character->TeleportTo(CharacterLocation, FRotator::ZeroRotator);
						Character->GetCharacterMovement()->StopMovementImmediately();
						Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
						Character->ForceNetUpdate();
					}
				})
			.UntilServer(
				TEXT("Authority recognizes the moving platform as the subject base"),
				[](FNetworkState& State)
				{
					return IsSubjectOnMovingBase(State, ROLE_Authority);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner recognizes the replicated moving platform as its base"),
				0,
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
					return IsSubjectOnMovingBase(State, ROLE_AutonomousProxy);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilClient(
				TEXT("Late moving-base observer receives the replicated fixture"),
				1,
				[](FNetworkState& State)
				{
					return IsValid(State.MovingBase);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Moving-base test establishes the second listen-server connection"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Late moving-base observer loads the Pilot pawn"),
				1,
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 2) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Bind the moving-base subject in both client worlds"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.UntilClient(
				TEXT("Late observer recognizes the subject as a based simulated proxy"),
				1,
				[](FNetworkState& State)
				{
					return IsSubjectOnMovingBase(State, ROLE_SimulatedProxy);
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Capture moving-base animation-history baselines on both client roles"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
					int32 ResetCount = 0;
					ASSERT_THAT(IsTrue(ReadAnimationHistoryResetCount(
						State,
						ResetCount)));
					State.AnimationResetBaseline = ResetCount;
					State.LastObservedAnimationResetDelta = MIN_int32;
					State.AnimationResetStableStartTime = -1.0;
					State.AnimationResetStableStartFrame = 0;
					State.MovingBaseObservationStartLocation =
						State.MovingBase->GetActorLocation();
				})
			.ThenClient(
				TEXT("Start low owner input so corrections include real SavedMove replay"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.11f);
				})
			.ThenServer(
				TEXT("Start fast authoritative platform translation and rotation"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.MovingBase));
					if (State.MovingBase)
					{
						State.MovingBase->StartMotion();
					}
				})
			.UntilClients(
				TEXT("Both client roles receive more than one reset threshold of platform motion"),
				[](FNetworkState& State)
				{
					const ENetRole ExpectedRole = State.ClientIndex == 0
						? ROLE_AutonomousProxy
						: ROLE_SimulatedProxy;
					return IsSubjectOnMovingBase(State, ExpectedRole) &&
						FVector::Dist(
							State.MovingBaseObservationStartLocation,
							State.MovingBase->GetActorLocation()) > 100.0f;
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Create a small owner-only error below the animation reset threshold"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						Character->SetActorLocation(
							Character->GetActorLocation() + FVector(10.0f, 0.0f, 0.0f),
							false,
							nullptr,
							ETeleportType::None);
						URpgCharacterMovementComponent* MovementComponent =
							Cast<URpgCharacterMovementComponent>(
								Character->GetCharacterMovement());
						ASSERT_THAT(IsNotNull(MovementComponent));
						if (MovementComponent)
						{
							MovementComponent->SaveBaseLocation();
							State.ClientCorrectionCountBaseline =
								MovementComponent->GetClientCorrectionReceivedCountForTests();
						}
					}
				})
			.ThenServer(
				TEXT("Force a genuine base-relative client adjustment during platform motion"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(
							Character->GetCharacterMovement())
						: nullptr;
					ASSERT_THAT(IsNotNull(MovementComponent));
					if (MovementComponent)
					{
						FNetworkPredictionData_Server_Character* ServerPrediction =
							MovementComponent->GetPredictionData_Server_Character();
						ASSERT_THAT(IsNotNull(ServerPrediction));
						if (ServerPrediction)
						{
							ServerPrediction->bForceClientUpdate = true;
						}
						MovementComponent->ForceReplicationUpdate();
						MovementComponent->ForceClientAdjustment();
					}
				})
			.UntilClient(
				TEXT("Owner receives the forced moving-base correction"),
				0,
				[](FNetworkState& State)
				{
					return HasObservedMovingBaseCorrection(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Small moving-base correction leaves both client histories stable"),
				[](FNetworkState& State)
				{
					const ENetRole ExpectedRole = State.ClientIndex == 0
						? ROLE_AutonomousProxy
						: ROLE_SimulatedProxy;
					return IsSubjectOnMovingBase(State, ExpectedRole) &&
						HasStableAnimationResetDelta(State, 0);
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Re-arm history observations before the large relative correction"),
				[](FNetworkState& State)
				{
					State.AnimationResetStableStartTime = -1.0;
					State.AnimationResetStableStartFrame = 0;
				})
			.ThenClient(
				TEXT("Create an owner-only relative error above the animation reset threshold"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(
							Character->GetCharacterMovement())
						: nullptr;
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(MovementComponent));
					if (Character && MovementComponent)
					{
						const float RelativeError = FMath::Max(
							MovementComponent->NetworkLargeClientCorrectionDistance + 25.0f,
							100.0f);
						Character->SetActorLocation(
							Character->GetActorLocation() + FVector(RelativeError, 0.0f, 0.0f),
							false,
							nullptr,
							ETeleportType::None);
						MovementComponent->SaveBaseLocation();
						State.ClientCorrectionCountBaseline =
							MovementComponent->GetClientCorrectionReceivedCountForTests();
					}
				})
			.ThenServer(
				TEXT("Force the large base-relative client adjustment during platform motion"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					URpgCharacterMovementComponent* MovementComponent = Character
						? Cast<URpgCharacterMovementComponent>(
							Character->GetCharacterMovement())
						: nullptr;
					ASSERT_THAT(IsNotNull(MovementComponent));
					if (MovementComponent)
					{
						FNetworkPredictionData_Server_Character* ServerPrediction =
							MovementComponent->GetPredictionData_Server_Character();
						ASSERT_THAT(IsNotNull(ServerPrediction));
						if (ServerPrediction)
						{
							ServerPrediction->bForceClientUpdate = true;
						}
						MovementComponent->ForceReplicationUpdate();
						MovementComponent->ForceClientAdjustment();
					}
				})
			.UntilClient(
				TEXT("Owner receives the large moving-base correction"),
				0,
				[](FNetworkState& State)
				{
					return HasObservedMovingBaseCorrection(State);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Large relative correction resets owner animation history exactly once"),
				0,
				[](FNetworkState& State)
				{
					return IsSubjectOnMovingBase(State, ROLE_AutonomousProxy) &&
						HasStableAnimationResetDelta(State, 1);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner correction leaves the moving simulated proxy history stable"),
				1,
				[](FNetworkState& State)
				{
					return IsSubjectOnMovingBase(State, ROLE_SimulatedProxy) &&
						HasStableAnimationResetDelta(State, 0);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Stop the moving-base fixture after correction validation"),
				[](FNetworkState& State)
				{
					if (State.MovingBase)
					{
						State.MovingBase->StopMotion();
					}
				})
			.ThenClient(
				TEXT("Stop owner input after moving-base validation"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				});
	}

	TEST_METHOD(GroundCoastLateJoinAndRelevancyReturn)
	{
		using namespace RpgGaspPIENetworkTests;

		Network
			.SpawnAndReplicate<
				ARpgGaspNetworkFloorFixture,
				&FNetworkState::Floor>(
				NetworkTimeout())
			.UntilServer(
				TEXT("Pilot Experience loads for the coast test on the listen server"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Initial coast-test client loads the Pilot pawn"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 1) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Place the coast-test pawns on separate floor lanes"),
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
				TEXT("Initial coast-test client owns a grounded GASP pawn"),
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
				TEXT("Capture the coast-test subject identity"),
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
				TEXT("Bind the coast-test subject on authority"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.UntilServer(
				TEXT("Authority starts the coast test from a stable stop"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner starts the coast test from a stable stop"),
				0,
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Establish Walk before the first late join"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.50f);
				})
			.UntilServer(
				TEXT("Authority establishes Walk before the first late join"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.50f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner establishes Walk before the first late join"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_AutonomousProxy,
						FVector::YAxisVector,
						0.50f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Apply the deterministic Walk-coast window on authority"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(ApplyDeterministicCoastProfile(State, 1.0f)));
				})
			.ThenClient(
				TEXT("Apply the deterministic Walk-coast window on the owner"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(ApplyDeterministicCoastProfile(State, 1.0f)));
				})
			.UntilServer(
				TEXT("Authority re-establishes Walk after the test-profile copy"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.50f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner re-establishes Walk after the test-profile copy"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_AutonomousProxy,
						FVector::YAxisVector,
						0.50f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Release Walk input for the late-join coast"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.UntilServer(
				TEXT("Authority enters a real no-input Walk coast"),
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_Authority,
						ERpgLocomotionGait::Walk,
						false);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner enters the same no-input Walk coast"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_AutonomousProxy,
						ERpgLocomotionGait::Walk,
						false);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Set a deterministic physical Walk-coast speed on authority"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(SetDeterministicCoastVelocity(State, 140.0f)));
				})
			.ThenClient(
				TEXT("Set the matching physical Walk-coast speed on the owner"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(SetDeterministicCoastVelocity(State, 140.0f)));
				})
			.UntilServer(
				TEXT("Authority holds Walk coast below its forward cap"),
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_Authority,
						ERpgLocomotionGait::Walk,
						true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner holds Walk coast below its forward cap"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_AutonomousProxy,
						ERpgLocomotionGait::Walk,
						true);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Walk-coast late join establishes the second connection"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Walk-coast late client loads the Pilot Experience"),
				1,
				[](FNetworkState& State)
				{
					if (!IsValid(State.Floor))
					{
						for (TActorIterator<ARpgGaspNetworkFloorFixture> It(State.World);
							It; ++It)
						{
							State.Floor = *It;
							break;
						}
					}
					ARpgCharacter* LocalCharacter = FindLocalCharacter(State.World);
					return IsPilotExperienceReady(State, 2) &&
						IsValid(State.Floor) &&
						IsPilotCharacterReady(LocalCharacter) &&
						LocalCharacter->GetCharacterMovement()->IsMovingOnGround();
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Bind the Walk-coast subject in both client worlds"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.UntilServer(
				TEXT("Authority remains in Walk coast during late join"),
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_Authority,
						ERpgLocomotionGait::Walk,
						true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner remains in Walk coast during late join"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_AutonomousProxy,
						ERpgLocomotionGait::Walk,
						true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("New simulated proxy reconstructs Walk from its initial coast snapshot"),
				1,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_SimulatedProxy,
						ERpgLocomotionGait::Walk,
						true);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Restore normal braking and stop after the Walk late join"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(RestorePilotProfileAndStop(State)));
				})
			.ThenClient(
				TEXT("Restore normal owner braking after the Walk late join"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(RestorePilotProfileAndStop(State)));
				})
			.UntilServer(
				TEXT("Authority clears Walk coast at physical stop"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and late proxy clear Walk coast at physical stop"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Establish Run before the second late join"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 1.0f);
				})
			.UntilServer(
				TEXT("Authority establishes Run before the second late join"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						1.0f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and existing simulated proxy establish Run"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						1.0f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Apply the deterministic Run-coast window on authority"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(ApplyDeterministicCoastProfile(State, 1.0f)));
				})
			.ThenClient(
				TEXT("Apply the deterministic Run-coast window on the owner"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(ApplyDeterministicCoastProfile(State, 1.0f)));
				})
			.UntilServer(
				TEXT("Authority re-establishes Run after the test-profile copy"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						1.0f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy re-establish Run"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						1.0f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Release Run input for the late-join coast"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.UntilServer(
				TEXT("Authority enters a real no-input Run coast"),
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_Authority,
						ERpgLocomotionGait::Run,
						false);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner enters the same no-input Run coast"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_AutonomousProxy,
						ERpgLocomotionGait::Run,
						false);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Set Run coast below the Walk cap on authority"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(SetDeterministicCoastVelocity(State, 180.0f)));
				})
			.ThenClient(
				TEXT("Set matching Run coast below the Walk cap on the owner"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(SetDeterministicCoastVelocity(State, 180.0f)));
				})
			.UntilServer(
				TEXT("Authority retains Run below the Walk cap"),
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_Authority,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and existing proxy retain Run below the Walk cap"),
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(
				TEXT("Run-coast late join establishes the third connection"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 3);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Run-coast late client loads the Pilot Experience"),
				2,
				[](FNetworkState& State)
				{
					if (!IsValid(State.Floor))
					{
						for (TActorIterator<ARpgGaspNetworkFloorFixture> It(State.World);
							It; ++It)
						{
							State.Floor = *It;
							break;
						}
					}
					ARpgCharacter* LocalCharacter = FindLocalCharacter(State.World);
					return IsPilotExperienceReady(State, 3) &&
						IsValid(State.Floor) &&
						IsPilotCharacterReady(LocalCharacter) &&
						LocalCharacter->GetCharacterMovement()->IsMovingOnGround();
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Bind the Run-coast subject in every client world"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.UntilServer(
				TEXT("Authority remains in sub-Walk-cap Run coast"),
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_Authority,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner remains in sub-Walk-cap Run coast"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_AutonomousProxy,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Existing proxy retains Run through the second late join"),
				1,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_SimulatedProxy,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("New proxy reconstructs Run below the Walk cap from its initial snapshot"),
				2,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_SimulatedProxy,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Remember the existing proxy before relevancy loss"),
				1,
				[this](FNetworkState& State)
				{
					State.SubjectBeforeRelevancyLoss = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsTrue(State.SubjectBeforeRelevancyLoss.IsValid()));
				})
			.ThenServer(
				TEXT("Move one observer outside the subject relevancy radius"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Subject = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Subject));
					ASSERT_THAT(IsTrue(State.ClientConnections.IsValidIndex(2)));
					ASSERT_THAT(IsTrue(State.ClientConnections.IsValidIndex(1)));
					APlayerController* ControlController =
						State.ClientConnections[2]->PlayerController;
					APlayerController* RelevancyController =
						State.ClientConnections[1]->PlayerController;
					APawn* ControlPawn = ControlController
						? ControlController->GetPawn()
						: nullptr;
					APawn* RelevancyPawn = RelevancyController
						? RelevancyController->GetPawn()
						: nullptr;
					ASSERT_THAT(IsNotNull(ControlPawn));
					ASSERT_THAT(IsNotNull(RelevancyPawn));
					if (Subject && ControlPawn && RelevancyPawn)
					{
						State.BaselineSubjectNetCullDistanceSquared =
							Subject->GetNetCullDistanceSquared();
						State.bHasBaselineSubjectNetCullDistanceSquared = true;
						Subject->SetNetCullDistanceSquared(FMath::Square(25000.0f));
						ASSERT_THAT(IsTrue(ControlPawn->TeleportTo(
							Subject->GetActorLocation() + FVector(1000.0, 0.0, 0.0),
							FRotator::ZeroRotator)));
						ASSERT_THAT(IsTrue(RelevancyPawn->TeleportTo(
							Subject->GetActorLocation() + FVector(60000.0, 0.0, 0.0),
							FRotator::ZeroRotator)));
						ControlPawn->GetMovementComponent()->StopMovementImmediately();
						RelevancyPawn->GetMovementComponent()->StopMovementImmediately();
						Subject->ForceNetUpdate();
					}
				})
			.UntilServer(
				TEXT("Observer actor channel closes outside relevancy"),
				[](FNetworkState& State)
				{
					return HasSubjectActorChannel(State, 1, false) &&
						HasSubjectActorChannel(State, 2, true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Observer destroys the old subject proxy outside relevancy"),
				1,
				[](FNetworkState& State)
				{
					return !State.SubjectBeforeRelevancyLoss.IsValid() &&
						FindCharacterByPlayerId(
							State.World,
							State.SubjectPlayerId) == nullptr;
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Control proxy remains in Run coast while the observer is irrelevant"),
				2,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_SimulatedProxy,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Return the observer inside subject relevancy"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Subject = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					APlayerController* RelevancyController =
						State.ClientConnections.IsValidIndex(1)
							? State.ClientConnections[1]->PlayerController
							: nullptr;
					APawn* RelevancyPawn = RelevancyController
						? RelevancyController->GetPawn()
						: nullptr;
					ASSERT_THAT(IsNotNull(Subject));
					ASSERT_THAT(IsNotNull(RelevancyPawn));
					if (Subject && RelevancyPawn)
					{
						ASSERT_THAT(IsTrue(RelevancyPawn->TeleportTo(
							Subject->GetActorLocation() + FVector(500.0, 0.0, 0.0),
							FRotator::ZeroRotator)));
						RelevancyPawn->GetMovementComponent()->StopMovementImmediately();
						Subject->ForceNetUpdate();
					}
				})
			.UntilServer(
				TEXT("Observer actor channel reopens after relevancy return"),
				[](FNetworkState& State)
				{
					return HasSubjectActorChannel(State, 1, true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Recreated proxy reconstructs Run coast and SwordShield after relevancy return"),
				1,
				[](FNetworkState& State)
				{
					ARpgCharacter* RecreatedSubject = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return RecreatedSubject &&
						RecreatedSubject != State.SubjectBeforeRelevancyLoss.Get() &&
						HasExpectedCoastGait(
							State,
							ROLE_SimulatedProxy,
							ERpgLocomotionGait::Run,
							true) &&
						HasCombatAnimationPresentation(
							State,
							ROLE_SimulatedProxy,
							SwordShieldProfileName,
							false);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Authority stays in Run coast through relevancy return"),
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_Authority,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner stays in Run coast through relevancy return"),
				0,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_AutonomousProxy,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Control proxy stays in Run coast through relevancy return"),
				2,
				[](FNetworkState& State)
				{
					return HasExpectedCoastGait(
						State,
						ROLE_SimulatedProxy,
						ERpgLocomotionGait::Run,
						true);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Restore normal relevancy and braking after Run coast"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Subject = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Subject));
					ASSERT_THAT(IsTrue(
						State.bHasBaselineSubjectNetCullDistanceSquared));
					if (Subject)
					{
						Subject->SetNetCullDistanceSquared(
							State.BaselineSubjectNetCullDistanceSquared);
						State.bHasBaselineSubjectNetCullDistanceSquared = false;
					}
					ASSERT_THAT(IsTrue(RestorePilotProfileAndStop(State)));
				})
			.ThenClient(
				TEXT("Restore normal owner braking after Run coast"),
				0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(RestorePilotProfileAndStop(State)));
				})
			.UntilServer(
				TEXT("Authority clears Run coast at physical stop"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Every client clears Run coast and replicated hints at stop"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Cleanup coast-test server timers"),
				[](FNetworkState& State)
				{
					State.World->GetTimerManager().ClearTimer(State.ObservationTimer);
				})
			.ThenClients(
				TEXT("Cleanup coast-test client timers"),
				[](FNetworkState& State)
				{
					StopMovementInput(State);
					State.World->GetTimerManager().ClearTimer(State.ObservationTimer);
				});
	}

	TEST_METHOD(AnalogGaitPredictionAndCorrection)
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
				TEXT("Place the prediction-test pawns on separate floor lanes"),
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
				TEXT("Capture the prediction-test subject identity"),
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
				TEXT("Bind the prediction-test subject on authority"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.ThenClientJoins(NetworkTimeout())
			.UntilClient(
				TEXT("Neutral late client receives the replicated collision floor"),
				1,
				[](FNetworkState& State)
				{
					return IsValid(State.Floor);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Neutral late join establishes the second listen-server connection"),
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 2);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Neutral late client loads the Pilot Experience and pawn"),
				1,
				[](FNetworkState& State)
				{
					return IsPilotExperienceReady(State, 2) &&
						IsPilotCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.ThenClients(
				TEXT("Bind the neutral subject in both client worlds"),
				[this](FNetworkState& State)
				{
					State.SubjectPlayerId = SubjectPlayerId;
				})
			.UntilServer(
				TEXT("Authority observes a physically stable neutral subject"),
				[](FNetworkState& State)
				{
					return HasStablePhysicalDeadzone(State, ROLE_Authority, 0.0f);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy observe a stable neutral subject"),
				[](FNetworkState& State)
				{
					return HasStablePhysicalDeadzone(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						0.0f);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Apply five-percent owner input inside the physical deadzone"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.05f);
				})
			.UntilServer(
				TEXT("Authority discards five-percent physical input"),
				[](FNetworkState& State)
				{
					return HasStablePhysicalDeadzone(State, ROLE_Authority, 0.05f);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy remain still at five-percent input"),
				[](FNetworkState& State)
				{
					return HasStablePhysicalDeadzone(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						0.05f);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Apply the inclusive ten-percent physical deadzone edge"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.10f);
				})
			.UntilServer(
				TEXT("Authority discards the inclusive deadzone edge"),
				[](FNetworkState& State)
				{
					return HasStablePhysicalDeadzone(State, ROLE_Authority, 0.10f);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy remain still at the deadzone edge"),
				[](FNetworkState& State)
				{
					return HasStablePhysicalDeadzone(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						0.10f);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Apply eleven-percent owner input above the physical deadzone"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.11f);
				})
			.UntilServer(
				TEXT("Authority preserves eleven-percent analog Walk input"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.11f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy preserve eleven-percent Walk input"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.11f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Raise owner input to twenty-five percent"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.25f);
				})
			.UntilServer(
				TEXT("Authority preserves twenty-five-percent analog Walk input"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.25f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy preserve twenty-five-percent Walk input"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.25f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Raise owner input to fifty percent"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.50f);
				})
			.UntilServer(
				TEXT("Authority preserves fifty-percent analog Walk input"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.50f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy preserve fifty-percent Walk input"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.50f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Stop before entering the Run hysteresis sequence"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.UntilServer(
				TEXT("Authority settles before Run hysteresis"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client views settle before Run hysteresis"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Apply sixty-nine-percent input from Idle"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.69f);
				})
			.UntilServer(
				TEXT("Authority remains Walk below the Run-enter edge"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.69f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy remain Walk below the enter edge"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.69f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Meet the inclusive Run-enter edge"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.70f);
				})
			.UntilServer(
				TEXT("Authority enters Run at the inclusive upper edge"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.70f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy enter Run at the inclusive upper edge"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.70f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Return just below the Run-enter edge"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.69f);
				})
			.UntilServer(
				TEXT("Authority retains Run inside the hysteresis band"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.69f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy retain Run inside the band"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.69f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Cross the Run-enter edge a second time"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.71f);
				})
			.UntilClients(
				TEXT("Repeated upper-edge input remains Run on both clients"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.71f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilServer(
				TEXT("Repeated upper-edge input remains Run on authority"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.71f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Meet the inclusive Run-exit edge while Run is latched"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.65f);
				})
			.UntilServer(
				TEXT("Authority retains Run at the inclusive lower edge"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.65f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client views retain Run at the inclusive lower edge"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.65f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Drop below the Run-exit edge"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.64f);
				})
			.UntilServer(
				TEXT("Authority exits Run only below the lower edge"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.64f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Owner and simulated proxy exit Run below the lower edge"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.64f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Return inside the band from Walk"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.69f);
				})
			.UntilServer(
				TEXT("Authority cannot re-enter Run from inside the band"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.69f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client views cannot re-enter Run from inside the band"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.69f,
						ERpgLocomotionGait::Walk);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Re-enter Run before the correction replay"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.71f);
				})
			.UntilServer(
				TEXT("Authority re-enters Run before correction"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.71f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client views re-enter Run before correction"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.71f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Hold Run inside the band before correction"),
				0,
				[](FNetworkState& State)
				{
					StartMovementInput(State, FVector::YAxisVector, 0.69f);
				})
			.UntilServer(
				TEXT("Authority holds the Run latch before correction"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						ROLE_Authority,
						FVector::YAxisVector,
						0.69f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client views hold the Run latch before correction"),
				[](FNetworkState& State)
				{
					return HasExpectedMovementInput(
						State,
						State.ClientIndex == 0
							? ROLE_AutonomousProxy
							: ROLE_SimulatedProxy,
						FVector::YAxisVector,
						0.69f,
						ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Capture the authoritative lane and reset correction observations"),
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
					State.StableInputScale = -1.0f;
					State.StableInputGait = ERpgLocomotionGait::Idle;
					State.StableInputStartTime = 0.0;
				})
			.ThenClients(
				TEXT("Reset client gait observations before correction replay"),
				[](FNetworkState& State)
				{
					State.StableInputScale = -1.0f;
					State.StableInputGait = ERpgLocomotionGait::Idle;
					State.StableInputStartTime = 0.0;
				})
			.ThenClient(
				TEXT("Create an owner-only divergence while Run is latched at sixty-nine percent"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					ASSERT_THAT(IsNotNull(Character));
					if (Character)
					{
						Character->SetActorLocation(
							Character->GetActorLocation() + FVector(100.0, 0.0, 0.0),
							false,
							nullptr,
							ETeleportType::None);
						ASSERT_THAT(IsTrue(FMath::Abs(
							Character->GetActorLocation().X -
								AuthorityCorrectionBaseline.X) > 60.0));
					}
				})
			.ThenServer(
				TEXT("Force an authoritative correction during the retained Run move"),
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
						FNetworkPredictionData_Server_Character* ServerPrediction =
							MovementComponent->GetPredictionData_Server_Character();
						ASSERT_THAT(IsNotNull(ServerPrediction));
						if (ServerPrediction)
						{
							ServerPrediction->bForceClientUpdate = true;
						}
						MovementComponent->ForceReplicationUpdate();
						MovementComponent->ForceClientAdjustment();
					}
				})
			.UntilServer(
				TEXT("Authority preserves retained Run gait through correction"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character &&
						FMath::Abs(
							Character->GetActorLocation().X -
								AuthorityCorrectionBaseline.X) <= 10.0 &&
						HasExpectedMovementInput(
							State,
							ROLE_Authority,
							FVector::YAxisVector,
							0.69f,
							ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Owner replay restores retained Run gait and authoritative lane"),
				0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character &&
						FMath::Abs(
							Character->GetActorLocation().X -
								AuthorityCorrectionBaseline.X) <= 10.0 &&
						HasExpectedMovementInput(
							State,
							ROLE_AutonomousProxy,
							FVector::YAxisVector,
							0.69f,
							ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.UntilClient(
				TEXT("Simulated proxy remains converged with the retained Run gait"),
				1,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(
						State.World,
						State.SubjectPlayerId);
					return Character &&
						FMath::Abs(
							Character->GetActorLocation().X -
								AuthorityCorrectionBaseline.X) <= 10.0 &&
						HasExpectedMovementInput(
							State,
							ROLE_SimulatedProxy,
							FVector::YAxisVector,
							0.69f,
							ERpgLocomotionGait::Run);
				},
				NetworkTimeout())
			.ThenClient(
				TEXT("Stop prediction input after correction"),
				0,
				[](FNetworkState& State)
				{
					StopMovementInput(State);
				})
			.UntilServer(
				TEXT("Authority settles after the analog prediction test"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Client views settle after the analog prediction test"),
				[](FNetworkState& State)
				{
					return HasStoppedAnimation(State);
				},
				NetworkTimeout())
			.ThenServer(
				TEXT("Cleanup prediction-test server timers"),
				[](FNetworkState& State)
				{
					State.World->GetTimerManager().ClearTimer(State.ObservationTimer);
				})
			.ThenClients(
				TEXT("Cleanup prediction-test client timers"),
				[](FNetworkState& State)
				{
					StopMovementInput(State);
					State.World->GetTimerManager().ClearTimer(State.ObservationTimer);
				});
	}
};

NETWORK_TEST_CLASS(PrototypeExperiencePIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgGaspPIENetworkTests::FNetworkState;

	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	FPrimaryAssetId OriginalExperienceOverride;
	bool bOriginalDiskPersistence = true;
	UClass* PilotGameModeClass = nullptr;

	BEFORE_EACH()
	{
		using namespace RpgGaspPIENetworkTests;

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
			PrototypeExperienceName);
		GameModeDefaults->bEnableDiskPersistence = false;

		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(1)
			.AsListenServer()
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

	TEST_METHOD(OverrideRemainsSelectable)
	{
		using namespace RpgGaspPIENetworkTests;

		Network
			.UntilServer(
				TEXT("Explicit PIE override selects the Prototype Experience and host pawn"),
				[](FNetworkState& State)
				{
					return IsExperienceReady(
							State,
							1,
							PrototypeExperienceName) &&
						IsPrototypeCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.UntilClients(
				TEXT("Prototype Experience and local pawn composition resolve on the client"),
				[](FNetworkState& State)
				{
					return IsExperienceReady(
							State,
							1,
							PrototypeExperienceName) &&
						IsPrototypeCharacterReady(FindLocalCharacter(State.World));
				},
				NetworkTimeout());
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
