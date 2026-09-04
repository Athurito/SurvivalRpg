// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "Net/Core/Serialization/QuantizedVectorSerialization.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterMovementProfileTest,
	"SurvivalRpg.Character.Movement.ProfileAndGait",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCharacterMovementProfileTest::RunTest(const FString& Parameters)
{
	FRpgCharacterMovementProfile Profile;
	TestTrue(
		TEXT("The legacy-compatible default profile is runtime-valid"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(Profile));
	TestFalse(
		TEXT("The default profile preserves Character/Blueprint movement values"),
		Profile.bOverrideCharacterMovement);

	Profile.bOverrideCharacterMovement = true;
	Profile.WalkSpeeds = FRpgDirectionalGroundSpeeds(200.0f, 180.0f, 150.0f);
	Profile.RunSpeeds = FRpgDirectionalGroundSpeeds(500.0f, 350.0f, 300.0f);
	Profile.MinAnalogGroundSpeed = 150.0f;
	Profile.MaxAcceleration = 800.0f;
	Profile.GroundFriction = 5.0f;
	Profile.bUseSeparateBrakingFriction = false;
	Profile.BrakingFrictionFactor = 0.0f;
	Profile.BrakingFriction = 0.0f;
	Profile.BrakingDecelerationWithInput = 500.0f;
	Profile.BrakingDecelerationWithoutInput = 2000.0f;
	Profile.FreeRotationRateYaw = 360.0f;
	Profile.MoveIntentThreshold = 0.1f;
	Profile.RunInputThreshold = 0.7f;
	Profile.RunInputExitThreshold = 0.65f;
	TestTrue(
		TEXT("The curated GASP-style profile is runtime-valid"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(Profile));

	struct FGaitCase
	{
		const TCHAR* Label;
		bool bMovingOnGround;
		float GroundSpeed;
		float InputMagnitude;
		ERpgLocomotionGait DesiredGait;
		ERpgLocomotionGait PreviousGait;
		ERpgLocomotionGait ExpectedGait;
	};
	const FGaitCase GaitCases[] = {
		{TEXT("Airborne movement has no grounded gait"), false, 300.0f, 1.0f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run, ERpgLocomotionGait::Idle},
		{TEXT("Stationary without intent is Idle"), true, 0.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Idle},
		{TEXT("Low analog intent enters Walk"), true, 0.0f, 0.5f, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Walk},
		{TEXT("Walk remains below the Run-enter threshold"), true, 200.0f, 0.69f, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Walk},
		{TEXT("Walk enters Run at the inclusive upper threshold"), true, 200.0f, 0.7f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Run},
		{TEXT("Run remains inside the hysteresis band"), true, 400.0f, 0.69f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run},
		{TEXT("Run remains on the lower threshold"), true, 400.0f, 0.65f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run},
		{TEXT("Run exits below the lower threshold"), true, 400.0f, 0.64f, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Run, ERpgLocomotionGait::Walk},
		{TEXT("Walk coast retains the stop database"), true, 100.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Walk},
		{TEXT("Run coast retains the stop database"), true, 300.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run},
		{TEXT("GASP late-join Walk coast seeds from replicated speed"), true, 150.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Walk},
		{TEXT("Late-join moving state without gait history fails toward Run"), true, 300.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Run},
		{TEXT("Physical stop clears the previous Run gait"), true, 2.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Run, ERpgLocomotionGait::Idle},
	};
	for (const FGaitCase& GaitCase : GaitCases)
	{
		TestEqual(
			GaitCase.Label,
			RpgCharacterMovementRuntime::ResolveGroundGait(
				GaitCase.bMovingOnGround,
				GaitCase.GroundSpeed,
				GaitCase.InputMagnitude,
				GaitCase.DesiredGait,
				GaitCase.PreviousGait,
				ERpgLocomotionGait::Idle,
				Profile),
			GaitCase.ExpectedGait);
	}

	struct FCoastHintCase
	{
		const TCHAR* Label;
		float GroundSpeed;
		ERpgLocomotionGait PreviousGait;
		ERpgLocomotionGait CoastGaitHint;
		ERpgLocomotionGait ExpectedGait;
	};
	const FCoastHintCase CoastHintCases[] = {
		{TEXT("Late Run coast remains Run above the Walk cap"), 300.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run},
		{TEXT("Late Run coast remains Run on the Walk cap"), 200.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run},
		{TEXT("Late Run coast remains Run below the Walk cap"), 150.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run},
		{TEXT("Late Walk coast remains Walk"), 150.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Walk},
		{TEXT("Relevancy return replaces stale local gait history"), 150.0f, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run},
		{TEXT("Physical stop wins over a stale Run coast hint"), 2.0f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run, ERpgLocomotionGait::Idle},
	};
	for (const FCoastHintCase& CoastHintCase : CoastHintCases)
	{
		TestEqual(
			CoastHintCase.Label,
			RpgCharacterMovementRuntime::ResolveGroundGait(
				true,
				CoastHintCase.GroundSpeed,
				0.0f,
				ERpgLocomotionGait::Idle,
				CoastHintCase.PreviousGait,
				CoastHintCase.CoastGaitHint,
				Profile),
			CoastHintCase.ExpectedGait);
	}

	TestEqual(
		TEXT("Late active input inside the retention band consumes current server Run state"),
		RpgCharacterMovementRuntime::ResolveGroundGait(
			true,
			150.0f,
			0.69f,
			ERpgLocomotionGait::Walk,
			ERpgLocomotionGait::Idle,
			ERpgLocomotionGait::Run,
			Profile),
		ERpgLocomotionGait::Run);
	TestEqual(
		TEXT("A server Walk transition repairs a proxy that missed the Run exit"),
		RpgCharacterMovementRuntime::ResolveGroundGait(
			true, 150.0f, 0.69f,
			ERpgLocomotionGait::Run, ERpgLocomotionGait::Run,
			ERpgLocomotionGait::Walk, Profile),
		ERpgLocomotionGait::Walk);
	FRpgCharacterMovementProfile PassiveCoastProfile = Profile;
	PassiveCoastProfile.bOverrideCharacterMovement = false;
	TestEqual(
		TEXT("A passive movement profile ignores the coast hint"),
		RpgCharacterMovementRuntime::ResolveGroundGait(
			true,
			150.0f,
			0.0f,
			ERpgLocomotionGait::Idle,
			ERpgLocomotionGait::Idle,
			ERpgLocomotionGait::Walk,
			PassiveCoastProfile),
		RpgCharacterMovementRuntime::ResolveGroundGait(
			true,
			150.0f,
			0.0f,
			ERpgLocomotionGait::Idle,
			ERpgLocomotionGait::Idle,
			ERpgLocomotionGait::Idle,
			PassiveCoastProfile));

	TestFalse(
		TEXT("Input at the exclusive intent threshold has no move intent"),
		RpgCharacterMovementRuntime::HasMoveIntent(Profile.MoveIntentThreshold, Profile));
	TestTrue(
		TEXT("Input above the intent threshold has move intent"),
		RpgCharacterMovementRuntime::HasMoveIntent(Profile.MoveIntentThreshold + 0.01f, Profile));
	const float PhysicalInputCases[][2] = {
		{0.0f, 0.0f},
		{0.05f, 0.0f},
		{0.1f, 0.0f},
		{0.11f, 0.11f},
		{0.25f, 0.25f},
		{0.5f, 0.5f},
	};
	for (const auto& PhysicalInputCase : PhysicalInputCases)
	{
		TestTrue(
			*FString::Printf(
				TEXT("Physical input %.2f resolves to %.2f"),
				PhysicalInputCase[0],
				PhysicalInputCase[1]),
			FMath::IsNearlyEqual(
				RpgCharacterMovementRuntime::ResolvePhysicalInputMagnitude(
					PhysicalInputCase[0],
					Profile),
				PhysicalInputCase[1]));
	}

	TestEqual(
		TEXT("Walk remains below the Run-enter threshold"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(
			0.69f,
			ERpgLocomotionGait::Walk,
			Profile),
		ERpgLocomotionGait::Walk);
	TestEqual(
		TEXT("The inclusive enter threshold selects Run"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(
			0.7f,
			ERpgLocomotionGait::Walk,
			Profile),
		ERpgLocomotionGait::Run);
	TestEqual(
		TEXT("Run remains latched just below its enter threshold"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(
			0.69f,
			ERpgLocomotionGait::Run,
			Profile),
		ERpgLocomotionGait::Run);
	TestEqual(
		TEXT("Run remains latched on its exit threshold"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(
			0.65f,
			ERpgLocomotionGait::Run,
			Profile),
		ERpgLocomotionGait::Run);
	TestEqual(
		TEXT("Run exits below its lower threshold"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(
			0.64f,
			ERpgLocomotionGait::Run,
			Profile),
		ERpgLocomotionGait::Walk);
	TestEqual(
		TEXT("Walk does not enter Run from inside the hysteresis band"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(
			0.69f,
			ERpgLocomotionGait::Walk,
			Profile),
		ERpgLocomotionGait::Walk);

	const FQuat BaseRotation(FRotator(0.0f, 50.0f, 0.0f));
	const FVector OwnerAcceleration(520.0f, 0.0f, 0.0f);
	const FVector NetworkRelativeAcceleration = UE::Net::QuantizeVector(
		10, BaseRotation.UnrotateVector(OwnerAcceleration));
	const FVector ServerAcceleration = UE::Net::QuantizeVector(
		10, BaseRotation.RotateVector(NetworkRelativeAcceleration));
	const float ServerInput = static_cast<float>(ServerAcceleration.Size()) / Profile.MaxAcceleration;
	TestTrue(
		TEXT("The actual Quantize10 base roundtrip crosses the inclusive Run-exit boundary"),
		ServerInput < Profile.RunInputExitThreshold);
	TestEqual(
		TEXT("A saved Run request survives that movement-base quantization boundary"),
		RpgCharacterMovementRuntime::ResolveSavedMoveDesiredGait(
			ServerInput, true, Profile.MaxAcceleration, Profile),
		ERpgLocomotionGait::Run);
	TestEqual(
		TEXT("A saved Walk request cannot be promoted by upward transport roundoff"),
		RpgCharacterMovementRuntime::ResolveSavedMoveDesiredGait(
			0.7001f, false, Profile.MaxAcceleration, Profile),
		ERpgLocomotionGait::Walk);
	TestEqual(
		TEXT("An invalid Run request below the allowed quantization margin remains Walk"),
		RpgCharacterMovementRuntime::ResolveSavedMoveDesiredGait(
			0.64f, true, Profile.MaxAcceleration, Profile),
		ERpgLocomotionGait::Walk);
	TestEqual(
		TEXT("Run flags never restore input after a physical stop"),
		RpgCharacterMovementRuntime::ResolveSavedMoveDesiredGait(
			0.0f, true, Profile.MaxAcceleration, Profile),
		ERpgLocomotionGait::Idle);
	TestTrue(
		TEXT("The movement profile never infers Sprint"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(
			1.0f,
			ERpgLocomotionGait::Run,
			Profile) !=
			ERpgLocomotionGait::Sprint);
	TestTrue(
		TEXT("Any movement input selects GASP moving-input braking"),
		FMath::IsNearlyEqual(
			RpgCharacterMovementRuntime::ResolveGroundBrakingDeceleration(true, Profile),
			500.0f));
	TestTrue(
		TEXT("Released input selects GASP stop braking"),
		FMath::IsNearlyEqual(
			RpgCharacterMovementRuntime::ResolveGroundBrakingDeceleration(false, Profile),
			2000.0f));

	TestTrue(
		TEXT("Free Walk uses the forward physical cap"),
		FMath::IsNearlyEqual(
			RpgCharacterMovementRuntime::ResolveGroundSpeedCap(
				ERpgLocomotionGait::Walk,
				false,
				180.0f,
				Profile),
			200.0f));
	TestTrue(
		TEXT("Controller-facing Run uses the sideways cap at ninety degrees"),
		FMath::IsNearlyEqual(
			RpgCharacterMovementRuntime::ResolveGroundSpeedCap(
				ERpgLocomotionGait::Run,
				true,
				90.0f,
				Profile),
			350.0f));
	TestTrue(
		TEXT("Controller-facing Run uses the backward cap at one hundred eighty degrees"),
		FMath::IsNearlyEqual(
			RpgCharacterMovementRuntime::ResolveGroundSpeedCap(
				ERpgLocomotionGait::Run,
				true,
				180.0f,
				Profile),
			300.0f));
	TestTrue(
		TEXT("Directional mapping blends linearly between forward and sideways"),
		FMath::IsNearlyEqual(
			RpgCharacterMovementRuntime::ResolveDirectionalSpeed(
				Profile.RunSpeeds,
				62.5f),
			425.0f));
	struct FDirectionCapCase
	{
		float Angle;
		float ExpectedRunCap;
	};
	const FDirectionCapCase DirectionCapCases[] = {
		{0.0f, 500.0f},
		{45.0f, 500.0f},
		{80.0f, 350.0f},
		{100.0f, 350.0f},
		{135.0f, 300.0f},
		{180.0f, 300.0f},
	};
	for (const FDirectionCapCase& DirectionCase : DirectionCapCases)
	{
		TestTrue(
			*FString::Printf(
				TEXT("GASP direction-map point %.0f resolves its exact Run cap"),
				DirectionCase.Angle),
			FMath::IsNearlyEqual(
				RpgCharacterMovementRuntime::ResolveDirectionalSpeed(
					Profile.RunSpeeds,
					DirectionCase.Angle),
				DirectionCase.ExpectedRunCap));
	}

	FRpgCharacterMovementProfile InvalidProfile = Profile;
	InvalidProfile.RunInputThreshold = 0.05f;
	TestFalse(
		TEXT("A Run threshold below the move-intent threshold is rejected"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(InvalidProfile));
	InvalidProfile = Profile;
	InvalidProfile.RunInputExitThreshold = Profile.MoveIntentThreshold;
	TestFalse(
		TEXT("The Run-exit threshold must remain above the physical deadzone"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(InvalidProfile));
	InvalidProfile = Profile;
	InvalidProfile.RunInputExitThreshold = Profile.RunInputThreshold + 0.01f;
	TestFalse(
		TEXT("The Run-exit threshold cannot exceed the Run-enter threshold"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(InvalidProfile));
	InvalidProfile = Profile;
	InvalidProfile.MinAnalogGroundSpeed = Profile.WalkSpeeds.Backward + 1.0f;
	TestFalse(
		TEXT("Minimum analog speed cannot exceed the ground-speed cap"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(InvalidProfile));
	InvalidProfile = Profile;
	InvalidProfile.WalkSpeeds = FRpgDirectionalGroundSpeeds(200.0f, 180.0f, 500.0f);
	InvalidProfile.RunSpeeds = FRpgDirectionalGroundSpeeds(500.0f, 350.0f, 500.0f);
	InvalidProfile.MinAnalogGroundSpeed = 300.0f;
	TestFalse(
		TEXT("A high backward cap cannot hide an analog floor above the active forward Walk cap"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(InvalidProfile));
	InvalidProfile = Profile;
	InvalidProfile.WalkSpeeds.Sideways = InvalidProfile.WalkSpeeds.Forward + 1.0f;
	TestFalse(
		TEXT("Directional Walk caps must remain ordered Forward, Sideways, Backward"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(InvalidProfile));
	InvalidProfile = Profile;
	InvalidProfile.MaxAcceleration = std::numeric_limits<float>::quiet_NaN();
	TestFalse(
		TEXT("Non-finite physical tuning is rejected"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(InvalidProfile));

	URpgCharacterMovementComponent* MovementComponent =
		NewObject<URpgCharacterMovementComponent>();
	if (!TestNotNull(TEXT("A movement component can be created for value-contract tests"), MovementComponent))
	{
		return false;
	}

	MovementComponent->MaxWalkSpeed = 777.0f;
	MovementComponent->MinAnalogWalkSpeed = 17.0f;
	MovementComponent->MaxAcceleration = 1234.0f;
	MovementComponent->GroundFriction = 9.0f;
	MovementComponent->bUseSeparateBrakingFriction = true;
	MovementComponent->BrakingFrictionFactor = 2.0f;
	MovementComponent->BrakingFriction = 7.0f;
	MovementComponent->BrakingDecelerationWalking = 1333.0f;
	MovementComponent->BrakingDecelerationFalling = 321.0f;
	FRpgCharacterMovementProfile PassiveProfile = Profile;
	PassiveProfile.bOverrideCharacterMovement = false;
	TestTrue(
		TEXT("A valid passive profile is accepted"),
		MovementComponent->ApplyMovementProfile(PassiveProfile));
	TestTrue(
		TEXT("Passive legacy profile does not overwrite Blueprint-authored speed"),
		FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, 777.0f));
	TestTrue(
		TEXT("A passive profile still supplies gait thresholds"),
		FMath::IsNearlyEqual(
			MovementComponent->GetMovementProfile().RunInputThreshold,
			PassiveProfile.RunInputThreshold));

	TestTrue(
		TEXT("The active GASP profile applies to CharacterMovement"),
		MovementComponent->ApplyMovementProfile(Profile));
	TestTrue(TEXT("Profile application preserves the Blueprint MaxWalkSpeed property"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, 777.0f));
	TestTrue(TEXT("Profile application preserves the Blueprint analog property"), FMath::IsNearlyEqual(MovementComponent->MinAnalogWalkSpeed, 17.0f));
	TestTrue(TEXT("Profile application preserves the Blueprint acceleration property"), FMath::IsNearlyEqual(MovementComponent->MaxAcceleration, 1234.0f));
	TestTrue(TEXT("Profile application preserves the Blueprint walking deceleration property"), FMath::IsNearlyEqual(MovementComponent->BrakingDecelerationWalking, 1333.0f));
	TestTrue(TEXT("Profile application preserves the Blueprint ground-friction property"), FMath::IsNearlyEqual(MovementComponent->GroundFriction, 9.0f));
	TestTrue(TEXT("Profile application preserves the Blueprint braking-friction factor"), FMath::IsNearlyEqual(MovementComponent->BrakingFrictionFactor, 2.0f));
	TestTrue(TEXT("Profile application preserves separate-braking mode"), MovementComponent->bUseSeparateBrakingFriction);

	USceneComponent* UpdatedComponent = NewObject<USceneComponent>(MovementComponent);
	if (!TestNotNull(TEXT("A transient updated component can exercise physical accessors"), UpdatedComponent))
	{
		return false;
	}
	MovementComponent->UpdatedComponent = UpdatedComponent;
	MovementComponent->MovementMode = MOVE_Walking;
	MovementComponent->DesiredGait = ERpgLocomotionGait::Run;
	MovementComponent->AnalogInputModifier = 0.69f;
	TestTrue(
		TEXT("The prediction-owned Run latch preserves the Run cap inside the hysteresis band"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxSpeed(), 500.0f));
	MovementComponent->DesiredGait = ERpgLocomotionGait::Walk;
	TestTrue(
		TEXT("Walk does not enter Run from inside the hysteresis band"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxSpeed(), 200.0f));
	TestTrue(
		TEXT("Standing ground resolves the profile minimum analog speed"),
		FMath::IsNearlyEqual(MovementComponent->GetMinAnalogSpeed(), 150.0f));
	TestTrue(
		TEXT("Standing ground resolves the profile acceleration"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxAcceleration(), 800.0f));
	const float ScaledInputCases[][2] = {
		{0.0f, 0.0f},
		{0.05f, 0.0f},
		{0.1f, 0.0f},
		{0.10001f, 0.0f},
		{0.1001f, 80.1f},
		{0.11f, 88.0f},
		{0.25f, 200.0f},
		{0.5f, 400.0f},
		{0.69999f, 560.0f},
	};
	for (const auto& ScaledInputCase : ScaledInputCases)
	{
		const FVector ScaledAcceleration =
			MovementComponent->ScaleInputAcceleration(
				FVector(ScaledInputCase[0], 0.0f, 0.0f));
		TestTrue(
			*FString::Printf(
				TEXT("Standing input %.2f produces %.1f cm/s^2"),
				ScaledInputCase[0],
				ScaledInputCase[1]),
			FMath::IsNearlyEqual(
				ScaledAcceleration.Size(),
				ScaledInputCase[1],
				0.01f));
	}
	MovementComponent->Acceleration = FVector(1024.0f, 0.0f, 0.0f);
	MovementComponent->AnalogInputModifier = 0.5f;
	MovementComponent->CalcVelocity(
		1.0f / 60.0f,
		MovementComponent->GroundFriction,
		false,
		MovementComponent->BrakingDecelerationWalking);
	TestTrue(
		TEXT("Same-tick landing or uncrouch rescales prior-mode acceleration from raw analog input"),
		FMath::IsNearlyEqual(MovementComponent->Acceleration.Size(), 400.0f));
	const float TransitionInputCases[][4] = {
		{0.05f, 0.0f, 0.0f, 2000.0f},
		{0.1f, 0.0f, 0.0f, 2000.0f},
		{0.10001f, 0.0f, 0.0f, 2000.0f},
		{0.1001f, 80.1f, 0.100125f, 500.0f},
		{0.11f, 88.0f, 0.11f, 500.0f},
		{0.69999f, 560.0f, 0.70f, 500.0f},
	};
	for (const auto& TransitionInputCase : TransitionInputCases)
	{
		MovementComponent->Velocity = FVector::ZeroVector;
		MovementComponent->Acceleration = FVector(1024.0f, 0.0f, 0.0f);
		MovementComponent->AnalogInputModifier = TransitionInputCase[0];
		MovementComponent->CalcVelocity(
			1.0f / 60.0f,
			MovementComponent->GroundFriction,
			false,
			MovementComponent->BrakingDecelerationWalking);
		TestTrue(
			*FString::Printf(
				TEXT("Same-tick standing transition canonicalizes %.2f input acceleration"),
				TransitionInputCase[0]),
			FMath::IsNearlyEqual(
				MovementComponent->Acceleration.Size(),
				TransitionInputCase[1],
				0.01f));
		TestTrue(
			*FString::Printf(
				TEXT("Same-tick standing transition canonicalizes %.2f analog input"),
				TransitionInputCase[0]),
			FMath::IsNearlyEqual(
				MovementComponent->AnalogInputModifier,
				TransitionInputCase[2],
				0.0001f));
		TestTrue(
			*FString::Printf(
				TEXT("Same-tick standing transition selects %.0f braking"),
				TransitionInputCase[3]),
			FMath::IsNearlyEqual(
				MovementComponent->GetMaxBrakingDeceleration(),
				TransitionInputCase[3]));
	}
	MovementComponent->DesiredGait = ERpgLocomotionGait::Walk;
	MovementComponent->AnalogInputModifier = 0.70f;
	MovementComponent->UpdatePredictedGaitFromInput(0.70f);
	TestTrue(
		TEXT("The inclusive threshold uses the Run cap on its first physical query"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxSpeed(), 500.0f));

	MovementComponent->MovementMode = MOVE_Falling;
	TestTrue(
		TEXT("Falling preserves the Blueprint-authored MaxWalkSpeed"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxSpeed(), 777.0f));
	TestTrue(
		TEXT("Falling preserves the Blueprint-authored analog floor"),
		FMath::IsNearlyEqual(MovementComponent->GetMinAnalogSpeed(), 17.0f));
	TestTrue(
		TEXT("Falling preserves the Blueprint-authored acceleration"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxAcceleration(), 1234.0f));
	TestTrue(
		TEXT("Falling preserves its movement-mode braking"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxBrakingDeceleration(), 321.0f));

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterGroundCoastReplicationContractTest,
	"SurvivalRpg.Character.Movement.GroundCoastReplicationContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCharacterGroundCoastReplicationContractTest::RunTest(
	const FString& Parameters)
{
	const FProperty* CoastGaitProperty =
		ARpgCharacter::StaticClass()->FindPropertyByName(TEXT("GroundMovementGait"));
	TestNotNull(
		TEXT("Character exposes reflected ground-coast state"),
		CoastGaitProperty);
	if (CoastGaitProperty)
	{
		TestTrue(
			TEXT("Ground-coast state participates in actor replication"),
			CoastGaitProperty->HasAnyPropertyFlags(CPF_Net));
		TestEqual(
			TEXT("Ground-coast state reconciles through its RepNotify"),
			CoastGaitProperty->RepNotifyFunc,
			FName(TEXT("OnRep_GroundMovementGait")));
	}

	URpgCharacterMovementComponent* MovementComponent =
		NewObject<URpgCharacterMovementComponent>();
	TestNotNull(TEXT("Movement component can store a coast hint"), MovementComponent);
	if (MovementComponent)
	{
		MovementComponent->NotifyReplicatedGroundMovementGait(
			ERpgLocomotionGait::Run);
		TestEqual(
			TEXT("Run is retained as a valid replicated coast hint"),
			MovementComponent->GetReplicatedGroundMovementGait(),
			ERpgLocomotionGait::Run);

		FRpgCharacterMovementProfile ActiveProfile;
		ActiveProfile.bOverrideCharacterMovement = true;
		TestTrue(
			TEXT("Applying PawnData movement tuning does not discard an earlier RepNotify"),
			MovementComponent->ApplyMovementProfile(ActiveProfile));
		TestEqual(
			TEXT("A pre-PawnData coast hint survives movement-profile initialization"),
			MovementComponent->GetReplicatedGroundMovementGait(),
			ERpgLocomotionGait::Run);
		MovementComponent->StopMovementImmediately();
		TestEqual(
			TEXT("A local stop never discards the authority hint cached by a simulated proxy"),
			MovementComponent->GetReplicatedGroundMovementGait(),
			ERpgLocomotionGait::Run);

		MovementComponent->NotifyReplicatedGroundMovementGait(
			ERpgLocomotionGait::Sprint);
		TestEqual(
			TEXT("Unsupported Sprint never enters the replicated coast contract"),
			MovementComponent->GetReplicatedGroundMovementGait(),
			ERpgLocomotionGait::Idle);
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterMovementSavedMoveTest,
	"SurvivalRpg.Character.Movement.SavedMovePrediction",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCharacterMovementSavedMoveTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper WorldWrapper;
	if (!TestTrue(
		TEXT("A transient gameplay world can be created"),
		WorldWrapper.CreateTestWorld(EWorldType::Game)))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	if (!TestTrue(
		TEXT("The transient gameplay world can begin play"),
		WorldWrapper.BeginPlayInTestWorld()))
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = WorldWrapper.GetTestWorld();
	ARpgCharacter* Character = World
		? World->SpawnActor<ARpgCharacter>()
		: nullptr;
	if (!TestNotNull(TEXT("An RPG character can be spawned"), Character))
	{
		return false;
	}

	URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
	if (!TestNotNull(
		TEXT("The RPG character owns the custom movement component"),
		MovementComponent))
	{
		return false;
	}

	FRpgCharacterMovementProfile Profile;
	Profile.bOverrideCharacterMovement = true;
	Profile.WalkSpeeds = FRpgDirectionalGroundSpeeds(200.0f, 180.0f, 150.0f);
	Profile.RunSpeeds = FRpgDirectionalGroundSpeeds(500.0f, 350.0f, 300.0f);
	Profile.MinAnalogGroundSpeed = 150.0f;
	Profile.MaxAcceleration = 800.0f;
	Profile.GroundFriction = 0.0f;
	Profile.BrakingFrictionFactor = 0.0f;
	Profile.BrakingDecelerationWithInput = 500.0f;
	Profile.BrakingDecelerationWithoutInput = 2000.0f;
	Profile.MoveIntentThreshold = 0.1f;
	Profile.RunInputThreshold = 0.7f;
	Profile.RunInputExitThreshold = 0.65f;
	if (!TestTrue(
		TEXT("The saved-move fixture applies the active profile"),
		MovementComponent->ApplyMovementProfile(Profile)))
	{
		return false;
	}
	MovementComponent->SetMovementMode(MOVE_Walking);
	const FEnumProperty* CoastGaitProperty =
		FindFProperty<FEnumProperty>(
			ARpgCharacter::StaticClass(),
			TEXT("GroundMovementGait"));
	if (!TestNotNull(
		TEXT("The authority coast transport is available for lifecycle checks"),
		CoastGaitProperty))
	{
		return false;
	}
	auto ReadAuthorityCoastGait = [Character, CoastGaitProperty]()
	{
		const void* ValueAddress =
			CoastGaitProperty->ContainerPtrToValuePtr<void>(Character);
		return static_cast<ERpgLocomotionGait>(
			CoastGaitProperty->GetUnderlyingProperty()
				->GetSignedIntPropertyValue(ValueAddress));
	};
	auto SeedAuthorityRunCoast = [MovementComponent]()
	{
		MovementComponent->Velocity = FVector(150.0f, 0.0f, 0.0f);
		MovementComponent->Acceleration = FVector::ZeroVector;
		MovementComponent->AnalogInputModifier = 0.0f;
		MovementComponent->DesiredGait = ERpgLocomotionGait::Idle;
		MovementComponent->GroundGait = ERpgLocomotionGait::Run;
		MovementComponent->RefreshLocomotionSnapshot();
	};

	SeedAuthorityRunCoast();
	TestEqual(
		TEXT("Authority publishes Run while a physical Run coast is active"),
		ReadAuthorityCoastGait(),
		ERpgLocomotionGait::Run);
	MovementComponent->StopMovementImmediately();
	TestEqual(
		TEXT("Synchronous stop clears local grounded presentation"),
		MovementComponent->GetGroundGait(),
		ERpgLocomotionGait::Idle);
	TestEqual(
		TEXT("Synchronous stop clears the authority coast transport"),
		ReadAuthorityCoastGait(),
		ERpgLocomotionGait::Idle);

	SeedAuthorityRunCoast();
	MovementComponent->DisableMovement();
	TestEqual(
		TEXT("Leaving standing-ground movement clears the authority coast transport"),
		ReadAuthorityCoastGait(),
		ERpgLocomotionGait::Idle);
	MovementComponent->SetMovementMode(MOVE_Walking);
	MovementComponent->DesiredGait = ERpgLocomotionGait::Run;
	MovementComponent->Acceleration = FVector(560.0f, 0.0f, 0.0f);
	MovementComponent->AnalogInputModifier = 0.70f;
	Character->SetIsCrouched(true);
	MovementComponent->RefreshLocomotionSnapshot();
	TestEqual(
		TEXT("Entering crouch preserves the gait magnitude scaled by the prior standing cap"),
		MovementComponent->GetDesiredGait(),
		ERpgLocomotionGait::Run);
	const float CrouchedMaxAcceleration = MovementComponent->GetMaxAcceleration();
	MovementComponent->DesiredGait = ERpgLocomotionGait::Walk;
	MovementComponent->Acceleration = FVector(
		0.40f * CrouchedMaxAcceleration,
		0.0f,
		0.0f);
	MovementComponent->AnalogInputModifier = 0.40f;
	Character->SetIsCrouched(false);
	MovementComponent->RefreshLocomotionSnapshot();
	TestEqual(
		TEXT("Leaving crouch preserves the gait magnitude scaled by the prior crouch cap"),
		MovementComponent->GetDesiredGait(),
		ERpgLocomotionGait::Walk);
	MovementComponent->Velocity = FVector(1000.0f, 0.0f, 0.0f);
	MovementComponent->Acceleration = FVector(1024.0f, 0.0f, 0.0f);
	MovementComponent->AnalogInputModifier = 0.10001f;
	MovementComponent->CalcVelocity(
		0.1f,
		0.0f,
		false,
		Profile.BrakingDecelerationWithInput);
	TestTrue(
		TEXT("Same-tick deadzone uses release braking instead of the stale caller value"),
		FMath::IsNearlyEqual(MovementComponent->Velocity.Size(), 800.0f, 0.1f));
	MovementComponent->Velocity = FVector::ZeroVector;

	FNetworkPredictionData_Client_RpgCharacter ClientData(*MovementComponent);
	ClientData.CurrentTimeStamp = 1.0f;

	auto RecordMove = [MovementComponent, Character, &ClientData](
		FSavedMove_RpgCharacter& Move,
		float InputMagnitude)
	{
		Move.Clear();
		ClientData.CurrentTimeStamp += 1.0f / 60.0f;
		const FVector Acceleration(
			InputMagnitude * MovementComponent->GetMaxAcceleration(),
			0.0f,
			0.0f);
		MovementComponent->AnalogInputModifier = InputMagnitude;
		Move.SetMoveFor(
			Character,
			1.0f / 60.0f,
			Acceleration,
			ClientData);
	};

	MovementComponent->DesiredGait = ERpgLocomotionGait::Walk;
	FSavedMove_RpgCharacter QuantizedEnterMove;
	RecordMove(QuantizedEnterMove, 0.69999f);
	TestTrue(
		TEXT("Net-quantized input at the enter edge records Run"),
		QuantizedEnterMove.bSavedRunGait);
	TestTrue(
		TEXT("The first quantized Run move captures the 500 cm/s cap"),
		FMath::IsNearlyEqual(QuantizedEnterMove.MaxSpeed, 500.0f));
	TestTrue(
		TEXT("The Run move exports its custom compressed flag"),
		(QuantizedEnterMove.GetCompressedFlags() &
			FSavedMove_Character::FLAG_Custom_0) != 0);

	FSavedMove_RpgCharacter HoldMove;
	RecordMove(HoldMove, 0.69f);
	TestTrue(
		TEXT("Input inside the hysteresis band records the retained Run gait"),
		HoldMove.bSavedRunGait);
	TestTrue(
		TEXT("The retained Run move captures the Run cap"),
		FMath::IsNearlyEqual(HoldMove.MaxSpeed, 500.0f));

	FSavedMove_RpgCharacter ExitMove;
	RecordMove(ExitMove, 0.64f);
	TestFalse(
		TEXT("Input below the exit edge records Walk"),
		ExitMove.bSavedRunGait);
	TestTrue(
		TEXT("The first Walk move after Run captures the 200 cm/s cap"),
		FMath::IsNearlyEqual(ExitMove.MaxSpeed, 200.0f));

	MovementComponent->DesiredGait = ERpgLocomotionGait::Idle;
	FSavedMove_RpgCharacter QuantizedDeadzoneMove;
	RecordMove(QuantizedDeadzoneMove, 0.10001f);
	TestTrue(
		TEXT("Input quantized onto the inclusive deadzone is zero before SavedMove storage"),
		QuantizedDeadzoneMove.Acceleration.IsZero());
	TestFalse(
		TEXT("A deadzone move never records Run"),
		QuantizedDeadzoneMove.bSavedRunGait);

	FSavedMove_RpgCharacter AboveDeadzoneMove;
	RecordMove(AboveDeadzoneMove, 0.1001f);
	TestFalse(
		TEXT("Input quantized just above the deadzone remains physically represented"),
		AboveDeadzoneMove.Acceleration.IsZero());
	TestTrue(
		TEXT("Input just above the deadzone captures the Walk cap"),
		FMath::IsNearlyEqual(AboveDeadzoneMove.MaxSpeed, 200.0f));

	MovementComponent->DesiredGait = ERpgLocomotionGait::Walk;
	HoldMove.PrepMoveFor(Character);
	TestEqual(
		TEXT("SavedMove replay restores the retained Run gait"),
		MovementComponent->GetDesiredGait(),
		ERpgLocomotionGait::Run);
	MovementComponent->UpdateFromCompressedFlags(
		FSavedMove_Character::FLAG_Custom_0);
	TestEqual(
		TEXT("Server compressed flags restore Run before movement"),
		MovementComponent->GetDesiredGait(),
		ERpgLocomotionGait::Run);
	MovementComponent->UpdateFromCompressedFlags(0);
	TestEqual(
		TEXT("A move without the custom flag restores the non-Run state"),
		MovementComponent->GetDesiredGait(),
		ERpgLocomotionGait::Walk);

	FSavedMovePtr RunCombineMove(new FSavedMove_RpgCharacter());
	FSavedMovePtr WalkCombineMove(new FSavedMove_RpgCharacter());
	auto* RunMove = static_cast<FSavedMove_RpgCharacter*>(RunCombineMove.Get());
	auto* WalkMove = static_cast<FSavedMove_RpgCharacter*>(WalkCombineMove.Get());
	RunMove->Clear();
	WalkMove->Clear();
	RunMove->bSavedRunGait = true;
	WalkMove->bSavedRunGait = false;
	RunMove->MaxSpeed = 300.0f;
	WalkMove->MaxSpeed = 300.0f;
	TestFalse(
		TEXT("Move combining rejects different predicted gaits even with equal synthetic caps"),
		RunMove->CanCombineWith(WalkCombineMove, Character, 1.0f));
	WalkMove->bSavedRunGait = true;
	TestTrue(
		TEXT("Matching predicted gait does not independently block move combining"),
		RunMove->CanCombineWith(WalkCombineMove, Character, 1.0f));

	MovementComponent->ApplyMovementProfile(Profile);
	MovementComponent->SetMovementMode(MOVE_Falling);
	MovementComponent->DesiredGait = ERpgLocomotionGait::Walk;
	const FVector RawFallingAcceleration(
		0.69999f * MovementComponent->GetMaxAcceleration(),
		0.0f,
		0.0f);
	FSavedMove_RpgCharacter FallingMove;
	FallingMove.Clear();
	ClientData.CurrentTimeStamp += 1.0f / 60.0f;
	FallingMove.SetMoveFor(
		Character,
		1.0f / 60.0f,
		RawFallingAcceleration,
		ClientData);
	TestTrue(
		TEXT("Falling SavedMoves preserve UE's raw client-only combine magnitude"),
		FMath::IsNearlyEqual(
			FallingMove.AccelMag,
			RawFallingAcceleration.Size(),
			0.001f));
	TestTrue(
		TEXT("Falling gait still resolves from the network-canonical acceleration"),
		FallingMove.bSavedRunGait);

	FRpgCharacterMovementProfile PassiveProfile = Profile;
	PassiveProfile.bOverrideCharacterMovement = false;
	MovementComponent->ApplyMovementProfile(PassiveProfile);
	MovementComponent->SetMovementMode(MOVE_Walking);
	const FVector RawPrototypeAcceleration(123.456f, -78.901f, 0.0f);
	FSavedMove_RpgCharacter PrototypeMove;
	PrototypeMove.Clear();
	ClientData.CurrentTimeStamp += 1.0f / 60.0f;
	PrototypeMove.SetMoveFor(
		Character,
		1.0f / 60.0f,
		RawPrototypeAcceleration,
		ClientData);
	TestTrue(
		TEXT("Prototype SavedMoves preserve UE's raw client-only combine magnitude"),
		FMath::IsNearlyEqual(
			PrototypeMove.AccelMag,
			RawPrototypeAcceleration.Size(),
			0.001f));
	TestTrue(
		TEXT("Prototype SavedMoves preserve UE's raw client-only combine direction"),
		PrototypeMove.AccelNormal.Equals(
			RawPrototypeAcceleration.GetSafeNormal(),
			0.0001f));
	TestTrue(
		TEXT("Prototype SavedMoves retain the engine's native transmitted quantization"),
		PrototypeMove.Acceleration.Equals(
			MovementComponent->RoundAcceleration(RawPrototypeAcceleration),
			0.001f));
	TestFalse(
		TEXT("Prototype SavedMoves do not export the pilot Run flag"),
		(PrototypeMove.GetCompressedFlags() &
			FSavedMove_Character::FLAG_Custom_0) != 0);

	FNetworkPredictionData_Client* PredictionData =
		MovementComponent->GetPredictionData_Client();
	auto* RpgPredictionData =
		static_cast<FNetworkPredictionData_Client_RpgCharacter*>(PredictionData);
	FSavedMovePtr AllocatedMove = RpgPredictionData
		? RpgPredictionData->AllocateNewMove()
		: nullptr;
	TestTrue(
		TEXT("The movement component allocates project SavedMoves"),
		AllocatedMove.IsValid());
	if (AllocatedMove.IsValid())
	{
		auto* AllocatedRpgMove =
			static_cast<FSavedMove_RpgCharacter*>(AllocatedMove.Get());
		AllocatedRpgMove->bSavedRunGait = true;
		TestTrue(
			TEXT("Allocated project moves expose the Run compressed flag"),
			(AllocatedMove->GetCompressedFlags() &
				FSavedMove_Character::FLAG_Custom_0) != 0);
	}

	AActor* MovingBaseActor = World->SpawnActor<AActor>();
	UBoxComponent* MovingBaseComponent = MovingBaseActor
		? NewObject<UBoxComponent>(MovingBaseActor, TEXT("CorrectionMovingBase"))
		: nullptr;
	if (!TestNotNull(
		TEXT("A movable base actor can be created for correction-space tests"),
		MovingBaseActor) ||
		!TestNotNull(
			TEXT("The correction-space fixture owns a primitive movement base"),
			MovingBaseComponent))
	{
		return false;
	}
	MovingBaseComponent->SetMobility(EComponentMobility::Movable);
	MovingBaseComponent->InitBoxExtent(FVector(200.0f, 200.0f, 20.0f));
	MovingBaseActor->SetRootComponent(MovingBaseComponent);
	MovingBaseActor->AddInstanceComponent(MovingBaseComponent);
	MovingBaseComponent->RegisterComponent();
	FMovementBaseInterfaceData MovingBaseData(MovingBaseComponent);
	if (!TestTrue(
		TEXT("The movable primitive resolves as dynamic movement-base data"),
		MovementBaseUtility::IsMovementBaseDataValid(&MovingBaseData) &&
			MovementBaseUtility::UseRelativeLocation(&MovingBaseData)))
	{
		return false;
	}

	const FVector SharedRelativeLocation(25.0f, -40.0f, 96.0f);
	MovingBaseActor->SetActorTransform(FTransform(
		FRotator::ZeroRotator,
		FVector(100.0f, 200.0f, 0.0f)));
	FVector SavedWorldLocation = FVector::ZeroVector;
	if (!TestTrue(
		TEXT("The initial base-relative move resolves to world space"),
		MovementBaseUtility::TransformLocationToWorld(
			&MovingBaseData,
			NAME_None,
			SharedRelativeLocation,
			SavedWorldLocation)))
	{
		return false;
	}

	FSavedMovePtr BaseRelativeMove(new FSavedMove_RpgCharacter());
	BaseRelativeMove->Clear();
	BaseRelativeMove->SavedLocation = SavedWorldLocation;
	BaseRelativeMove->SavedRelativeLocation = SharedRelativeLocation;
	BaseRelativeMove->EndMovementBaseInterfaceData = MovingBaseData;
	BaseRelativeMove->EndBoneName = NAME_None;
	BaseRelativeMove->TimeStamp = ClientData.CurrentTimeStamp;
	ClientData.LastAckedMove = BaseRelativeMove;
	MovementComponent->NetworkLargeClientCorrectionDistance = 50.0f;

	MovingBaseActor->SetActorTransform(FTransform(
		FRotator(0.0f, 65.0f, 0.0f),
		FVector(450.0f, -120.0f, 40.0f)));
	FVector SameRelativeServerWorldLocation = FVector::ZeroVector;
	TestTrue(
		TEXT("The moved and rotated base resolves the unchanged server-relative position"),
		MovementBaseUtility::TransformLocationToWorld(
			&MovingBaseData,
			NAME_None,
			SharedRelativeLocation,
			SameRelativeServerWorldLocation));
	TestTrue(
		TEXT("The fixture's world-space base motion exceeds the reset threshold"),
		FVector::DistSquared(
			SavedWorldLocation,
			SameRelativeServerWorldLocation) >
			FMath::Square(MovementComponent->NetworkLargeClientCorrectionDistance));
	MovementComponent->ResetPendingAnimationCorrectionState();
	MovementComponent->OnClientCorrectionReceived(
		ClientData,
		BaseRelativeMove->TimeStamp,
		SameRelativeServerWorldLocation,
		FVector::ZeroVector,
		&MovingBaseData,
		NAME_None,
		true,
		true,
		static_cast<uint8>(MOVE_Walking),
		FVector::DownVector);
	TestFalse(
		TEXT("Legitimate translation and rotation of the same base do not reset animation history"),
		MovementComponent->bPendingAnimationCorrectionDiscontinuity);

	// Keep the exact threshold check on an unrotated transform so floating-point
	// inverse-rotation noise cannot turn the strict greater-than boundary flaky.
	MovingBaseActor->SetActorTransform(FTransform(
		FRotator::ZeroRotator,
		FVector(450.0f, -120.0f, 40.0f)));
	FVector ThresholdRelativeServerWorldLocation = FVector::ZeroVector;
	TestTrue(
		TEXT("A correction exactly on the reset threshold resolves on the moved base"),
		MovementBaseUtility::TransformLocationToWorld(
			&MovingBaseData,
			NAME_None,
			SharedRelativeLocation + FVector(50.0f, 0.0f, 0.0f),
			ThresholdRelativeServerWorldLocation));
	MovementComponent->ResetPendingAnimationCorrectionState();
	MovementComponent->OnClientCorrectionReceived(
		ClientData,
		BaseRelativeMove->TimeStamp,
		ThresholdRelativeServerWorldLocation,
		FVector::ZeroVector,
		&MovingBaseData,
		NAME_None,
		true,
		true,
		static_cast<uint8>(MOVE_Walking),
		FVector::DownVector);
	TestFalse(
		TEXT("The inclusive threshold preserves UE's strict greater-than reset contract"),
		MovementComponent->bPendingAnimationCorrectionDiscontinuity);

	FVector DivergentRelativeServerWorldLocation = FVector::ZeroVector;
	TestTrue(
		TEXT("A material player-relative correction resolves on the moved base"),
		MovementBaseUtility::TransformLocationToWorld(
			&MovingBaseData,
			NAME_None,
			SharedRelativeLocation + FVector(100.0f, 0.0f, 0.0f),
			DivergentRelativeServerWorldLocation));
	MovementComponent->ResetPendingAnimationCorrectionState();
	MovementComponent->OnClientCorrectionReceived(
		ClientData,
		BaseRelativeMove->TimeStamp,
		DivergentRelativeServerWorldLocation,
		FVector::ZeroVector,
		&MovingBaseData,
		NAME_None,
		true,
		true,
		static_cast<uint8>(MOVE_Walking),
		FVector::DownVector);
	TestTrue(
		TEXT("A large correction inside the shared base frame still resets animation history"),
		MovementComponent->bPendingAnimationCorrectionDiscontinuity);

	AActor* OtherBaseActor = World->SpawnActor<AActor>();
	UBoxComponent* OtherBaseComponent = OtherBaseActor
		? NewObject<UBoxComponent>(OtherBaseActor, TEXT("CorrectionOtherBase"))
		: nullptr;
	if (!TestNotNull(
		TEXT("A second base actor can be created for base-switch fallback"),
		OtherBaseActor) ||
		!TestNotNull(
			TEXT("The second base owns a primitive component"),
			OtherBaseComponent))
	{
		return false;
	}
	OtherBaseComponent->SetMobility(EComponentMobility::Movable);
	OtherBaseActor->SetRootComponent(OtherBaseComponent);
	OtherBaseActor->AddInstanceComponent(OtherBaseComponent);
	OtherBaseComponent->RegisterComponent();
	OtherBaseActor->SetActorLocation(SavedWorldLocation + FVector(300.0f, 0.0f, 0.0f));
	FMovementBaseInterfaceData OtherBaseData(OtherBaseComponent);
	FVector OtherBaseServerWorldLocation = FVector::ZeroVector;
	TestTrue(
		TEXT("The switched base resolves a valid server world location"),
		MovementBaseUtility::TransformLocationToWorld(
			&OtherBaseData,
			NAME_None,
			SharedRelativeLocation,
			OtherBaseServerWorldLocation));
	MovementComponent->ResetPendingAnimationCorrectionState();
	MovementComponent->OnClientCorrectionReceived(
		ClientData,
		BaseRelativeMove->TimeStamp,
		OtherBaseServerWorldLocation,
		FVector::ZeroVector,
		&OtherBaseData,
		NAME_None,
		true,
		true,
		static_cast<uint8>(MOVE_Walking),
		FVector::DownVector);
	TestTrue(
		TEXT("A movement-base switch deliberately falls back to the large world-space correction"),
		MovementComponent->bPendingAnimationCorrectionDiscontinuity);

	FMovementBaseInterfaceData UnresolvedBaseData;
	MovementComponent->ResetPendingAnimationCorrectionState();
	MovementComponent->OnClientCorrectionReceived(
		ClientData,
		BaseRelativeMove->TimeStamp,
		SavedWorldLocation + FVector(100.0f, 0.0f, 0.0f),
		FVector::ZeroVector,
		&UnresolvedBaseData,
		NAME_None,
		true,
		false,
		static_cast<uint8>(MOVE_Walking),
		FVector::DownVector);
	TestTrue(
		TEXT("An unresolved absolute-base correction retains the world-space fallback"),
		MovementComponent->bPendingAnimationCorrectionDiscontinuity);
	MovementComponent->ResetPendingAnimationCorrectionState();
	TestTrue(
		TEXT("An unresolved relative-base classification explicitly falls back to world space"),
		MovementComponent->IsLargeAcknowledgedAnimationCorrection(
			BaseRelativeMove.Get(),
			SavedWorldLocation + FVector(100.0f, 0.0f, 0.0f),
			&UnresolvedBaseData,
			NAME_None,
			true,
			true));

	MovingBaseActor->SetActorTransform(FTransform(
		FRotator(0.0f, 65.0f, 0.0f),
		FVector(450.0f, -120.0f, 40.0f)));
	MovementComponent->SetBase(&MovingBaseData, NAME_None);
	Character->SetActorLocation(SameRelativeServerWorldLocation);
	MovementComponent->SaveBaseLocation();
	MovementComponent->ResetPendingAnimationCorrectionState();
	MovementComponent->CapturePendingAnimationCorrectionStart();
	TestTrue(
		TEXT("The pre-replay snapshot captures a dynamic-base-relative position"),
		MovementComponent->bHasPendingAnimationCorrectionStartRelativeLocation);
	MovingBaseActor->SetActorTransform(FTransform(
		FRotator(0.0f, 110.0f, 0.0f),
		FVector(900.0f, 75.0f, 80.0f)));
	FVector ReplayedSameRelativeWorldLocation = FVector::ZeroVector;
	TestTrue(
		TEXT("The post-replay fixture resolves the unchanged local position"),
		MovementBaseUtility::TransformLocationToWorld(
			&MovingBaseData,
			NAME_None,
			SharedRelativeLocation,
			ReplayedSameRelativeWorldLocation));
	Character->SetActorLocation(ReplayedSameRelativeWorldLocation);
	MovementComponent->SaveBaseLocation();
	TestFalse(
		TEXT("Base motion between correction receipt and replay does not look like a live jump"),
		MovementComponent->IsLargePendingAnimationCorrection());

	FVector ReplayedDivergentWorldLocation = FVector::ZeroVector;
	TestTrue(
		TEXT("The post-replay fixture resolves a material relative divergence"),
		MovementBaseUtility::TransformLocationToWorld(
			&MovingBaseData,
			NAME_None,
			SharedRelativeLocation + FVector(100.0f, 0.0f, 0.0f),
			ReplayedDivergentWorldLocation));
	Character->SetActorLocation(ReplayedDivergentWorldLocation);
	MovementComponent->SaveBaseLocation();
	TestTrue(
		TEXT("A material live replay jump inside the shared base frame remains detectable"),
		MovementComponent->IsLargePendingAnimationCorrection());
	MovementComponent->ResetPendingAnimationCorrectionState();

	// Exercise UE's real missing-move extrapolation path, which calls PerformMovement
	// directly rather than MoveAutonomous. Reuse the fixture away from other test geometry.
	MovingBaseComponent->SetBoxExtent(FVector(10000.0f, 10000.0f, 25.0f));
	MovingBaseComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MovingBaseComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MovingBaseActor->SetActorTransform(FTransform(FRotator::ZeroRotator, FVector(-20000.0, 0.0, -25.0)));
	MovementComponent->SetMovementMode(MOVE_Falling);
	Character->SetActorLocation(FVector(-20000.0, 0.0, Character->GetSimpleCollisionHalfHeight() + 2.0));
	Character->SetAutonomousProxy(true);
	MovementComponent->bRunPhysicsWithNoController = true;
	TestTrue(TEXT("Extrapolation fixture restores the standing profile"), MovementComponent->ApplyMovementProfile(Profile));
	MovementComponent->SetMovementMode(MOVE_Walking);
	MovementComponent->Velocity = FVector(325.0, 0.0, 0.0);
	MovementComponent->MoveAutonomous(2.0f, 1.0f / 60.0f,
		FSavedMove_Character::FLAG_Custom_0, FVector(519.9, 0.0, 0.0));
	TestEqual(TEXT("Server validates the Run flag despite base-roundtrip acceleration below 0.65"),
		MovementComponent->GetDesiredGait(), ERpgLocomotionGait::Run);
	const FVector BeforeExtrapolation = Character->GetActorLocation();
	TestTrue(TEXT("Authority extrapolates a remote owner through the native forced-update path"),
		MovementComponent->ForcePositionUpdate(1.0f / 60.0f));
	TestTrue(TEXT("Forced update actually advances the grounded subject"),
		MovementComponent->IsMovingOnGround() && Character->GetActorLocation().X > BeforeExtrapolation.X);
	TestEqual(TEXT("Missing-move extrapolation retains the last validated Run decision"),
		MovementComponent->GetDesiredGait(), ERpgLocomotionGait::Run);
	TestTrue(TEXT("Extrapolation preserves the Run cap"), FMath::IsNearlyEqual(MovementComponent->GetMaxSpeed(), 500.0f));
	TestFalse(TEXT("Extrapolation does not leave a SavedMove scope active"), MovementComponent->bResolvingSavedMove);
	MovementComponent->MoveAutonomous(2.1f, 1.0f / 60.0f, 0, FVector(552.0, 0.0, 0.0));
	TestTrue(TEXT("Authority can extrapolate a validated Walk move too"),
		MovementComponent->ForcePositionUpdate(1.0f / 60.0f));
	TestEqual(TEXT("Forced update never invents Run from a Walk request inside the retention band"),
		MovementComponent->GetDesiredGait(), ERpgLocomotionGait::Walk);

	WorldWrapper.ForwardErrorMessages(this);
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
