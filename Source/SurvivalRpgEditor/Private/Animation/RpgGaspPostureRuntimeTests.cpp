// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "SurvivalRpg/Animation/RpgGaspLocomotionConfig.h"
#include "SurvivalRpg/Animation/RpgGaspPostureRuntime.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementProfile.h"
#include "SurvivalRpg/Core/Character/RpgCharacterRotationMode.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgGaspPostureRuntimeTest,
	"SurvivalRpg.Animation.Gasp.PostureRuntime",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgGaspPostureRuntimeTest::RunTest(const FString& Parameters)
{
	const FRpgGaspLocomotionTuning DefaultTuning;
	TestTrue(
		TEXT("Default posture tuning is runtime-valid"),
		RpgGaspLocomotionConfig::IsTuningRuntimeValid(DefaultTuning));
	TestTrue(
		TEXT("Unarmed fallback keeps the correction in Free rotation"),
		RpgGaspPostureRuntime::ShouldApplyCorrection(
			ERpgCharacterRotationMode::Free,
			true));
	TestTrue(
		TEXT("A resolved authored profile keeps the relaxed correction in Free locomotion"),
		RpgGaspPostureRuntime::ShouldApplyCorrection(
			ERpgCharacterRotationMode::Free,
			false));
	TestTrue(
		TEXT("Unarmed fallback keeps the correction in CombatStrafe"),
		RpgGaspPostureRuntime::ShouldApplyCorrection(
			ERpgCharacterRotationMode::CombatStrafe,
			true));
	TestTrue(
		TEXT("Unarmed fallback keeps the correction while aiming"),
		RpgGaspPostureRuntime::ShouldApplyCorrection(
			ERpgCharacterRotationMode::Aim,
			true));
	TestFalse(
		TEXT("A resolved authored profile uses its combat torso posture in CombatStrafe"),
		RpgGaspPostureRuntime::ShouldApplyCorrection(
			ERpgCharacterRotationMode::CombatStrafe,
			false));
	TestFalse(
		TEXT("A resolved authored profile uses its aiming torso posture in Aim"),
		RpgGaspPostureRuntime::ShouldApplyCorrection(
			ERpgCharacterRotationMode::Aim,
			false));

	FRpgGaspLocomotionTuning Tuning = DefaultTuning;
	Tuning.UnarmedIdlePostureCorrectionDegrees = 2.0f;
	Tuning.UnarmedWalkPostureCorrectionDegrees = 8.0f;
	Tuning.UnarmedRunPostureCorrectionDegrees = 14.0f;
	Tuning.UnarmedPostureCorrectionSpeed = 60.0f;
	TestEqual(
		TEXT("Idle resolves the designer-owned Idle target"),
		RpgGaspPostureRuntime::ResolveTargetCorrectionDegrees(
			ERpgLocomotionGait::Idle,
			Tuning),
		Tuning.UnarmedIdlePostureCorrectionDegrees);
	TestEqual(
		TEXT("Walk resolves the designer-owned Walk target"),
		RpgGaspPostureRuntime::ResolveTargetCorrectionDegrees(
			ERpgLocomotionGait::Walk,
			Tuning),
		Tuning.UnarmedWalkPostureCorrectionDegrees);
	TestEqual(
		TEXT("Run resolves the designer-owned Run target"),
		RpgGaspPostureRuntime::ResolveTargetCorrectionDegrees(
			ERpgLocomotionGait::Run,
			Tuning),
		Tuning.UnarmedRunPostureCorrectionDegrees);
	TestEqual(
		TEXT("Reserved Sprint reuses the Run posture instead of inventing another gait contract"),
		RpgGaspPostureRuntime::ResolveTargetCorrectionDegrees(
			ERpgLocomotionGait::Sprint,
			Tuning),
		Tuning.UnarmedRunPostureCorrectionDegrees);

	const float FirstRunStep = RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
		Tuning.UnarmedIdlePostureCorrectionDegrees,
		ERpgLocomotionGait::Run,
		true,
		0.1f,
		Tuning);
	TestEqual(
		TEXT("Run transition advances at the designer-owned constant rate"),
		FirstRunStep,
		8.0f);
	TestEqual(
		TEXT("A second Run step reaches the target without overshoot"),
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			FirstRunStep,
			ERpgLocomotionGait::Run,
			true,
			0.1f,
			Tuning),
		Tuning.UnarmedRunPostureCorrectionDegrees);
	const float FirstIdleDecayStep = RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
		Tuning.UnarmedRunPostureCorrectionDegrees,
		ERpgLocomotionGait::Idle,
		true,
		0.1f,
		Tuning);
	TestEqual(
		TEXT("Run-to-Idle decay uses the same constant rate"),
		FirstIdleDecayStep,
		8.0f);
	TestEqual(
		TEXT("Run-to-Idle decay reaches the target without undershoot"),
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			FirstIdleDecayStep,
			ERpgLocomotionGait::Idle,
			true,
			0.1f,
			Tuning),
		Tuning.UnarmedIdlePostureCorrectionDegrees);
	const float FirstSuppressedRunStep =
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			Tuning.UnarmedRunPostureCorrectionDegrees,
			ERpgLocomotionGait::Run,
			false,
			0.1f,
			Tuning);
	TestEqual(
		TEXT("Suppressing the relaxed posture decays at the same bounded rate"),
		FirstSuppressedRunStep,
		8.0f);
	TestEqual(
		TEXT("A second suppressed step reaches zero without undershoot"),
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			FirstSuppressedRunStep,
			ERpgLocomotionGait::Run,
			false,
			0.2f,
			Tuning),
		0.0f);
	TestEqual(
		TEXT("A suppressed correction stays at zero across gait changes"),
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			0.0f,
			ERpgLocomotionGait::Sprint,
			false,
			0.25f,
			Tuning),
		0.0f);
	TestEqual(
		TEXT("A non-positive delta preserves the current correction"),
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			Tuning.UnarmedWalkPostureCorrectionDegrees,
			ERpgLocomotionGait::Idle,
			true,
			0.0f,
			Tuning),
		Tuning.UnarmedWalkPostureCorrectionDegrees);
	TestEqual(
		TEXT("A negative current correction is clamped before a zero-delta update"),
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			-10.0f,
			ERpgLocomotionGait::Idle,
			true,
			0.0f,
			Tuning),
		0.0f);
	TestEqual(
		TEXT("An excessive current correction is clamped before a zero-delta update"),
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			100.0f,
			ERpgLocomotionGait::Idle,
			true,
			0.0f,
			Tuning),
		Tuning.UnarmedRunPostureCorrectionDegrees);
	TestEqual(
		TEXT("A non-finite delta preserves the finite bounded current correction"),
		RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			Tuning.UnarmedWalkPostureCorrectionDegrees,
			ERpgLocomotionGait::Run,
			true,
			std::numeric_limits<float>::quiet_NaN(),
			Tuning),
		Tuning.UnarmedWalkPostureCorrectionDegrees);
	TestTrue(
		TEXT("A non-finite current value recovers to a finite bounded correction"),
		FMath::IsFinite(RpgGaspPostureRuntime::AdvanceCorrectionDegrees(
			std::numeric_limits<float>::quiet_NaN(),
			ERpgLocomotionGait::Run,
			true,
			0.1f,
			Tuning)));

	FRpgGaspLocomotionTuning UnorderedTuning = Tuning;
	UnorderedTuning.UnarmedWalkPostureCorrectionDegrees =
		UnorderedTuning.UnarmedRunPostureCorrectionDegrees + 1.0f;
	TestFalse(
		TEXT("Designer posture targets must remain ordered Idle through Run"),
		RpgGaspLocomotionConfig::IsTuningRuntimeValid(UnorderedTuning));
	TestEqual(
		TEXT("Invalid posture tuning fails closed to zero correction"),
		RpgGaspPostureRuntime::ResolveTargetCorrectionDegrees(
			ERpgLocomotionGait::Run,
			UnorderedTuning),
		0.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
