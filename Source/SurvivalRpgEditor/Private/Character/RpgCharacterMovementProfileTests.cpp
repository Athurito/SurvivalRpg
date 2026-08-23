// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
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
	TestTrue(
		TEXT("The curated GASP-style profile is runtime-valid"),
		RpgCharacterMovementRuntime::IsProfileRuntimeValid(Profile));

	struct FGaitCase
	{
		const TCHAR* Label;
		bool bMovingOnGround;
		float GroundSpeed;
		float InputMagnitude;
		ERpgLocomotionGait PreviousGait;
		ERpgLocomotionGait ExpectedGait;
	};
	const FGaitCase GaitCases[] = {
		{TEXT("Airborne movement has no grounded gait"), false, 300.0f, 1.0f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Idle},
		{TEXT("Stationary without intent is Idle"), true, 0.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Idle},
		{TEXT("Low analog intent enters Walk"), true, 0.0f, 0.5f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Walk},
		{TEXT("Walk remains below the Run-enter threshold"), true, 200.0f, 0.69f, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Walk},
		{TEXT("Walk enters Run at the inclusive upper threshold"), true, 200.0f, 0.7f, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Run},
		{TEXT("Run exits below the stateless GASP threshold"), true, 400.0f, 0.69f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Walk},
		{TEXT("Walk coast retains the stop database"), true, 100.0f, 0.0f, ERpgLocomotionGait::Walk, ERpgLocomotionGait::Walk},
		{TEXT("Run coast retains the stop database"), true, 300.0f, 0.0f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Run},
		{TEXT("GASP late-join Walk coast seeds from replicated speed"), true, 150.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Walk},
		{TEXT("Late-join moving state without gait history fails toward Run"), true, 300.0f, 0.0f, ERpgLocomotionGait::Idle, ERpgLocomotionGait::Run},
		{TEXT("Physical stop clears the previous Run gait"), true, 2.0f, 0.0f, ERpgLocomotionGait::Run, ERpgLocomotionGait::Idle},
	};
	for (const FGaitCase& GaitCase : GaitCases)
	{
		TestEqual(
			GaitCase.Label,
			RpgCharacterMovementRuntime::ResolveGroundGait(
				GaitCase.bMovingOnGround,
				GaitCase.GroundSpeed,
				GaitCase.InputMagnitude,
				GaitCase.PreviousGait,
				Profile),
			GaitCase.ExpectedGait);
	}

	TestFalse(
		TEXT("Input at the exclusive intent threshold has no move intent"),
		RpgCharacterMovementRuntime::HasMoveIntent(Profile.MoveIntentThreshold, Profile));
	TestTrue(
		TEXT("Input above the intent threshold has move intent"),
		RpgCharacterMovementRuntime::HasMoveIntent(Profile.MoveIntentThreshold + 0.01f, Profile));

	TestEqual(
		TEXT("Sub-threshold input has a stateless Walk desired gait"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(0.69f, Profile),
		ERpgLocomotionGait::Walk);
	TestEqual(
		TEXT("The inclusive threshold has a stateless Run desired gait"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(0.7f, Profile),
		ERpgLocomotionGait::Run);
	TestTrue(
		TEXT("The movement profile never infers Sprint"),
		RpgCharacterMovementRuntime::ResolveDesiredGait(1.0f, Profile) !=
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
		TEXT("Current analog input, not stale Run presentation state, owns the Walk cap"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxSpeed(), 200.0f));
	TestTrue(
		TEXT("Standing ground resolves the profile minimum analog speed"),
		FMath::IsNearlyEqual(MovementComponent->GetMinAnalogSpeed(), 150.0f));
	TestTrue(
		TEXT("Standing ground resolves the profile acceleration"),
		FMath::IsNearlyEqual(MovementComponent->GetMaxAcceleration(), 800.0f));
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
	MovementComponent->DesiredGait = ERpgLocomotionGait::Walk;
	MovementComponent->AnalogInputModifier = 0.70f;
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

#endif // WITH_DEV_AUTOMATION_TESTS
