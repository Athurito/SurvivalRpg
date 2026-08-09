// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SurvivalRpg/Animation/AnimNode_RpgFootPlacement.h"
#include "SurvivalRpg/Animation/RpgFootPlacementTypes.h"
#include "UObject/UnrealType.h"

namespace RpgFootPlacementRuntimeTests
{
	bool IsSnapshotValueProperty(const FProperty* Property, bool bAllowLegSnapshot)
	{
		if (CastField<FBoolProperty>(Property) || CastField<FNumericProperty>(Property))
		{
			return true;
		}

		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty)
		{
			return false;
		}

		const UScriptStruct* ValueStruct = StructProperty->Struct;
		return ValueStruct == TBaseStructure<FVector>::Get() ||
			ValueStruct == TBaseStructure<FQuat>::Get() ||
			ValueStruct == TBaseStructure<FTransform>::Get() ||
			(bAllowLegSnapshot && ValueStruct == FRpgFootPlacementLegSnapshot::StaticStruct());
	}

	float AngleBetweenDegrees(const FVector& First, const FVector& Second)
	{
		const float CosAngle = FMath::Clamp(
			FVector::DotProduct(First.GetSafeNormal(), Second.GetSafeNormal()),
			-1.0f,
			1.0f);
		return FMath::RadiansToDegrees(FMath::Acos(CosAngle));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFootPlacementPlantPolicyTest,
	"SurvivalRpg.Animation.FootPlacement.Runtime.PlantPolicy",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFootPlacementPlantPolicyTest::RunTest(const FString& Parameters)
{
	FRpgFootPlacementSettings Settings;
	TestEqual(TEXT("The GASP lock releases outside 20 cm"), Settings.UnplantRadius, 20.0f);
	TestEqual(TEXT("The GASP lock replants inside 20 percent of its radius"), Settings.ReplantRadiusRatio, 0.2f);
	TestEqual(TEXT("The GASP lock tolerates a 60-degree angular deviation"), Settings.UnplantAngle, 60.0f);
	TestEqual(TEXT("The GASP angular replant bound is 20 percent"), Settings.ReplantAngleRatio, 0.2f);

	TestEqual(
		TEXT("Full GASP contact maps to zero pseudo-speed"),
		RpgFootPlacement::ConvertContactCurveToSpeed(1.0f),
		0.0f);
	TestEqual(
		TEXT("No GASP contact maps to 100 cm/s pseudo-speed"),
		RpgFootPlacement::ConvertContactCurveToSpeed(0.0f),
		100.0f);
	TestEqual(
		TEXT("Alignment is full at the plant-speed threshold"),
		RpgFootPlacement::CalculateAlignmentAlpha(60.0f, Settings),
		1.0f);
	TestEqual(
		TEXT("Alignment is released at the configured upper threshold"),
		RpgFootPlacement::CalculateAlignmentAlpha(200.0f, Settings),
		0.0f);
	TestEqual(
		TEXT("Alignment interpolates through the roll phase"),
		RpgFootPlacement::CalculateAlignmentAlpha(130.0f, Settings),
		0.5f);

	TestFalse(
		TEXT("A missing walkable surface cannot establish a plant lock"),
		RpgFootPlacement::ShouldPlantFoot(false, 0.0f, 0.0f, Settings));
	TestTrue(
		TEXT("The exact speed and positive distance boundaries may establish a plant lock"),
		RpgFootPlacement::ShouldPlantFoot(true, 60.0f, 10.0f, Settings));
	TestTrue(
		TEXT("The signed ground distance uses the same inclusive plant boundary"),
		RpgFootPlacement::ShouldPlantFoot(true, 60.0f, -10.0f, Settings));
	TestFalse(
		TEXT("Speed above the plant threshold cannot establish a lock"),
		RpgFootPlacement::ShouldPlantFoot(true, 60.01f, 0.0f, Settings));
	TestFalse(
		TEXT("A foot outside the plant distance cannot establish a lock"),
		RpgFootPlacement::ShouldPlantFoot(true, 0.0f, 10.01f, Settings));

	TestFalse(
		TEXT("An existing lock remains planted at all three inclusive boundaries"),
		RpgFootPlacement::ShouldUnplantFoot(60.0f, 20.0f, 60.0f, Settings));
	TestTrue(
		TEXT("Foot speed independently releases an existing lock"),
		RpgFootPlacement::ShouldUnplantFoot(60.01f, 0.0f, 0.0f, Settings));
	TestTrue(
		TEXT("Anchor drift independently releases an existing lock"),
		RpgFootPlacement::ShouldUnplantFoot(0.0f, 20.01f, 0.0f, Settings));
	TestTrue(
		TEXT("Ground-normal change independently releases an existing lock"),
		RpgFootPlacement::ShouldUnplantFoot(0.0f, 0.0f, 60.01f, Settings));
	TestTrue(
		TEXT("A released lock replants inside the tighter radius and angle bounds"),
		RpgFootPlacement::ShouldReplantFoot(4.0f, 12.0f, Settings));
	TestFalse(
		TEXT("A released lock remains unplanted outside the replant radius"),
		RpgFootPlacement::ShouldReplantFoot(4.01f, 0.0f, Settings));
	TestFalse(
		TEXT("A released lock remains unplanted outside the replant angle"),
		RpgFootPlacement::ShouldReplantFoot(0.0f, 12.01f, Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFootPlacementHalfLifeTest,
	"SurvivalRpg.Animation.FootPlacement.Runtime.HalfLife",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFootPlacementHalfLifeTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("A zero delta produces no interpolation"),
		FMath::IsNearlyZero(RpgFootPlacement::CalculateHalfLifeAlpha(0.0f, 1.0f)));
	TestTrue(
		TEXT("A negative delta produces no interpolation"),
		FMath::IsNearlyZero(RpgFootPlacement::CalculateHalfLifeAlpha(-0.1f, 1.0f)));
	TestTrue(
		TEXT("A non-positive half-life completes immediately for a positive delta"),
		FMath::IsNearlyEqual(RpgFootPlacement::CalculateHalfLifeAlpha(0.1f, 0.0f), 1.0f));
	TestTrue(
		TEXT("One half-life covers exactly half the remaining distance"),
		FMath::IsNearlyEqual(RpgFootPlacement::CalculateHalfLifeAlpha(1.0f, 1.0f), 0.5f));
	TestTrue(
		TEXT("Two half-lives cover three quarters of the remaining distance"),
		FMath::IsNearlyEqual(RpgFootPlacement::CalculateHalfLifeAlpha(2.0f, 1.0f), 0.75f));

	const float FrameRates[] = {20.0f, 60.0f, 120.0f};
	for (const float FrameRate : FrameRates)
	{
		const float DeltaSeconds = 1.0f / FrameRate;
		const float Alpha = RpgFootPlacement::CalculateHalfLifeAlpha(DeltaSeconds, 1.0f);
		float Value = 0.0f;
		for (int32 Frame = 0; Frame < FMath::RoundToInt(FrameRate); ++Frame)
		{
			Value = FMath::Lerp(Value, 1.0f, Alpha);
		}

		TestTrue(
			FString::Printf(TEXT("One elapsed half-life reaches 0.5 at %.0f FPS"), FrameRate),
			FMath::IsNearlyEqual(Value, 0.5f, 0.0001f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFootPlacementGroundAlignmentTest,
	"SurvivalRpg.Animation.FootPlacement.Runtime.FlatAndSlopeAlignment",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFootPlacementGroundAlignmentTest::RunTest(const FString& Parameters)
{
	const FTransform FKFootTransform(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(20.0f)),
		FVector(100.0f, 50.0f, 20.0f));
	const FTransform AuthoredFootToBall(
		FQuat(FVector::RightVector, FMath::DegreesToRadians(15.0f)),
		FVector(12.0f, 3.0f, -8.0f));
	const FTransform AuthoredBallTransform = AuthoredFootToBall * FKFootTransform;
	const FTransform IKFootTransform(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(-35.0f)),
		FVector(-25.0f, 80.0f, 30.0f));
	const FTransform DerivedIKBall = RpgFootPlacement::DeriveIKBallTransform(
		FKFootTransform,
		AuthoredBallTransform,
		IKFootTransform);
	TestTrue(
		TEXT("The IK ball uses Stock FootPlacement's FootToBall-times-IKFoot order"),
		DerivedIKBall.Equals(AuthoredFootToBall * IKFootTransform, 0.001f));
	TestTrue(
		TEXT("The IK ball preserves the authored FK-foot-to-ball relationship"),
		DerivedIKBall.GetRelativeTransform(IKFootTransform).Equals(AuthoredFootToBall, 0.001f));
	TestFalse(
		TEXT("The derived IK ball does not reuse the unrelated FK ball world transform"),
		DerivedIKBall.Equals(AuthoredBallTransform, 0.001f));
	const FTransform LockedFootTransform(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(60.0f)),
		FVector(40.0f, -30.0f, 15.0f));
	const FTransform PivotedFoot = RpgFootPlacement::PivotFootAroundBall(
		IKFootTransform,
		DerivedIKBall,
		LockedFootTransform);
	const FTransform CurrentFootToBall = DerivedIKBall.GetRelativeTransform(IKFootTransform);
	const FTransform CurrentBallToFoot = IKFootTransform.GetRelativeTransform(DerivedIKBall);
	const FTransform LockedBallTransform = CurrentFootToBall * LockedFootTransform;
	const FTransform ExpectedPinnedBallTransform(
		DerivedIKBall.GetRotation(),
		LockedBallTransform.GetLocation(),
		DerivedIKBall.GetScale3D());
	TestTrue(
		TEXT("Pivot locking uses Stock FootPlacement's BallToFoot-times-PinnedBall order"),
		PivotedFoot.Equals(CurrentBallToFoot * ExpectedPinnedBallTransform, 0.001f));
	TestTrue(
		TEXT("Pivot locking retains the ball location represented by the planted foot"),
		(CurrentFootToBall * PivotedFoot).GetLocation().Equals(
			(CurrentFootToBall * LockedFootTransform).GetLocation(),
			0.001f));
	TestFalse(
		TEXT("Pivot locking does not freeze the complete planted ankle transform"),
		PivotedFoot.GetRotation().Equals(LockedFootTransform.GetRotation(), 0.001f));

	FAnimNode_RpgFootPlacement RuntimeNode;
	TestEqual(TEXT("The worker raw-pose ball gate uses GASP's ten-centimeter bound"), RuntimeNode.PlantDistanceThreshold, 10.0f);
	TestEqual(TEXT("The worker raw-pose lock gate uses GASP's twenty-centimeter bound"), RuntimeNode.UnplantRadius, 20.0f);
	TestEqual(
		TEXT("Raw geometry is fully weighted inside both inner bounds"),
		RpgFootPlacement::CalculateGeometryWeight(5.0f, 10.0f, true, 10.0f, 20.0f),
		1.0f);
	TestTrue(
		TEXT("Raw geometry fades smoothly through both outer halves"),
		FMath::IsNearlyEqual(
			RpgFootPlacement::CalculateGeometryWeight(7.5f, 15.0f, true, 10.0f, 20.0f),
			0.25f,
			0.001f));
	TestEqual(
		TEXT("The exact ball-distance limit suppresses placement"),
		RpgFootPlacement::CalculateGeometryWeight(10.0f, 0.0f, false, 10.0f, 20.0f),
		0.0f);
	TestEqual(
		TEXT("Ball distance outside the limit suppresses placement even without a lock"),
		RpgFootPlacement::CalculateGeometryWeight(-10.01f, 0.0f, false, 10.0f, 20.0f),
		0.0f);
	TestEqual(
		TEXT("The exact planar lock-drift limit suppresses a lock"),
		RpgFootPlacement::CalculateGeometryWeight(0.0f, 20.0f, true, 10.0f, 20.0f),
		0.0f);
	TestEqual(
		TEXT("Planar drift is deliberately ignored for an unlocked foot"),
		RpgFootPlacement::CalculateGeometryWeight(0.0f, 200.0f, false, 10.0f, 20.0f),
		1.0f);

	float MaximumFlatGroundCorrection = 0.0f;
	for (int32 SampleIndex = 0; SampleIndex <= 100; ++SampleIndex)
	{
		const float BallHeight = static_cast<float>(SampleIndex) * 0.1f;
		const float GeometryWeight = RpgFootPlacement::CalculateGeometryWeight(
			BallHeight,
			0.0f,
			false,
			10.0f,
			20.0f);
		MaximumFlatGroundCorrection = FMath::Max(
			MaximumFlatGroundCorrection,
			BallHeight * GeometryWeight);
	}
	TestTrue(
		TEXT("The same-frame gate bounds the flat-ground pelvis contribution below 5.5 cm"),
		MaximumFlatGroundCorrection <= 5.5f);
	TestEqual(
		TEXT("A lifted raw foot outside the ten-centimeter bound cannot affect IK or pelvis"),
		RpgFootPlacement::CalculateGeometryWeight(15.0f, 0.0f, false, 10.0f, 20.0f),
		0.0f);

	const FTransform FlatIKTransform(FQuat::Identity, FVector(0.0f, 0.0f, 20.0f));
	const FTransform FlatBallTransform(FQuat::Identity, FVector(10.0f, 0.0f, 10.0f));
	const FTransform FlatResult = RpgFootPlacement::AlignFootToGroundPlane(
		FlatIKTransform,
		FlatBallTransform,
		FVector::ZeroVector,
		FVector::UpVector,
		FVector::UpVector,
		50.0f,
		45.0f);
	TestTrue(
		TEXT("A flat plane projects the ball down while preserving its IK-to-ball offset"),
		FlatResult.GetLocation().Equals(FVector(0.0f, 0.0f, 10.0f), 0.001f));
	TestTrue(
		TEXT("A flat plane preserves the authored foot rotation"),
		FlatResult.GetRotation().Equals(FQuat::Identity, 0.001f));

	const FVector ThirtyDegreeNormal(0.0f, 0.5f, FMath::Sqrt(3.0f) * 0.5f);
	const FTransform SlopeIKTransform(FQuat::Identity, FVector(0.0f, 0.0f, 10.0f));
	const FTransform SlopeBallTransform = FTransform::Identity;
	const FTransform SlopeResult = RpgFootPlacement::AlignFootToGroundPlane(
		SlopeIKTransform,
		SlopeBallTransform,
		FVector::ZeroVector,
		ThirtyDegreeNormal,
		FVector::UpVector,
		50.0f,
		45.0f);
	const FVector SlopeUp = SlopeResult.GetRotation().RotateVector(FVector::UpVector);
	TestTrue(
		TEXT("A slope inside the rotation limit aligns the foot up axis to the plane normal"),
		SlopeUp.Equals(ThirtyDegreeNormal, 0.001f));
	TestTrue(
		TEXT("Slope alignment rotates the IK-to-ball offset around the projected ball"),
		SlopeResult.GetLocation().Equals(ThirtyDegreeNormal * 10.0f, 0.001f));

	const FTransform ClampedSlopeResult = RpgFootPlacement::AlignFootToGroundPlane(
		SlopeIKTransform,
		SlopeBallTransform,
		FVector::ZeroVector,
		ThirtyDegreeNormal,
		FVector::UpVector,
		2.0f,
		15.0f);
	const float ClampedRotationDegrees = RpgFootPlacementRuntimeTests::AngleBetweenDegrees(
		FVector::UpVector,
		ClampedSlopeResult.GetRotation().RotateVector(FVector::UpVector));
	TestTrue(
		TEXT("Slope rotation respects the configured angular bound"),
		FMath::IsNearlyEqual(ClampedRotationDegrees, 15.0f, 0.01f));
	TestTrue(
		TEXT("Slope translation respects the configured distance bound"),
		FMath::IsNearlyEqual(
			FVector::Distance(ClampedSlopeResult.GetLocation(), SlopeIKTransform.GetLocation()),
			2.0f,
			0.001f));

	const FTransform PreviousBase(
		FQuat::Identity,
		FVector(100.0f, 0.0f, 0.0f));
	const FTransform CurrentBase(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0f)),
		FVector(120.0f, 10.0f, 0.0f));
	const FVector SurfacePoint(110.0f, 0.0f, 5.0f);
	const FVector RebasedPoint = RpgFootPlacement::RebasePointThroughSurface(
		SurfacePoint,
		PreviousBase,
		CurrentBase);
	TestTrue(
		TEXT("A surface-local point follows both moving-base translation and rotation around its pivot"),
		RebasedPoint.Equals(FVector(120.0f, 20.0f, 5.0f), 0.001f));
	const FVector RebasedNormal = RpgFootPlacement::RebaseNormalThroughSurface(
		FVector::ForwardVector,
		PreviousBase,
		CurrentBase);
	TestTrue(
		TEXT("A surface-local normal follows moving-base rotation without translation"),
		RebasedNormal.Equals(FVector::RightVector, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFootPlacementNeutralIKTargetTest,
	"SurvivalRpg.Animation.FootPlacement.Runtime.NeutralIKTarget",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFootPlacementNeutralIKTargetTest::RunTest(const FString& Parameters)
{
	const FTransform LeftFKTransform(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(10.0f)),
		FVector(-20.0f, -12.0f, 18.0f));
	const FTransform LeftProceduralTarget(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(35.0f)),
		FVector(-5.0f, -12.0f, 8.0f));
	const FTransform RightFKTransform(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(-15.0f)),
		FVector(30.0f, 12.0f, 20.0f));
	const FTransform RightProceduralTarget(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(-40.0f)),
		FVector(45.0f, 12.0f, 10.0f));

	const float NoGroundWeight = RpgFootPlacement::CalculateEffectivePlacementWeight(
		false,
		1.0f,
		1.0f);
	const float ZeroSnapshotWeight = RpgFootPlacement::CalculateEffectivePlacementWeight(
		true,
		0.0f,
		1.0f);
	const float ZeroGeometryWeight = RpgFootPlacement::CalculateEffectivePlacementWeight(
		true,
		1.0f,
		0.0f);
	TestEqual(TEXT("No ground disables only the procedural correction"), NoGroundWeight, 0.0f);
	TestEqual(TEXT("A zero snapshot weight disables only the procedural correction"), ZeroSnapshotWeight, 0.0f);
	TestEqual(TEXT("A zero raw-pose gate disables only the procedural correction"), ZeroGeometryWeight, 0.0f);
	TestTrue(
		TEXT("No ground writes the same-frame FK ankle instead of retaining raw IK"),
		RpgFootPlacement::ResolveIKFootTarget(
			LeftFKTransform,
			LeftProceduralTarget,
			NoGroundWeight).Equals(LeftFKTransform, 0.001f));
	TestTrue(
		TEXT("A zero snapshot weight writes the same-frame FK ankle"),
		RpgFootPlacement::ResolveIKFootTarget(
			LeftFKTransform,
			LeftProceduralTarget,
			ZeroSnapshotWeight).Equals(LeftFKTransform, 0.001f));
	TestTrue(
		TEXT("A zero geometry weight writes the same-frame FK ankle"),
		RpgFootPlacement::ResolveIKFootTarget(
			LeftFKTransform,
			LeftProceduralTarget,
			ZeroGeometryWeight).Equals(LeftFKTransform, 0.001f));

	const float LeftWeight = RpgFootPlacement::CalculateEffectivePlacementWeight(true, 0.0f, 1.0f);
	const float RightWeight = RpgFootPlacement::CalculateEffectivePlacementWeight(true, 1.0f, 1.0f);
	const FTransform LeftResult = RpgFootPlacement::ResolveIKFootTarget(
		LeftFKTransform,
		LeftProceduralTarget,
		LeftWeight);
	const FTransform RightResult = RpgFootPlacement::ResolveIKFootTarget(
		RightFKTransform,
		RightProceduralTarget,
		RightWeight);
	TestTrue(
		TEXT("The left swing leg independently follows its FK ankle"),
		LeftResult.Equals(LeftFKTransform, 0.001f));
	TestTrue(
		TEXT("The right planted leg independently reaches its procedural target"),
		RightResult.Equals(RightProceduralTarget, 0.001f));

	const FTransform StaticInputIK(FQuat::Identity, FVector(0.0f, 0.0f, 10.0f));
	const FTransform LiftedFKFoot(FQuat::Identity, FVector(40.0f, 0.0f, 25.0f));
	const FTransform LiftedAuthoredBall(FQuat::Identity, FVector(50.0f, 0.0f, 15.0f));
	const float LiftedBallGeometryWeight = RpgFootPlacement::CalculateGeometryWeight(
		LiftedAuthoredBall.GetLocation().Z,
		0.0f,
		true,
		10.0f,
		20.0f);
	const float LiftedBallEffectiveWeight = RpgFootPlacement::CalculateEffectivePlacementWeight(
		true,
		1.0f,
		LiftedBallGeometryWeight);
	const FTransform LiftedResult = RpgFootPlacement::ResolveIKFootTarget(
		LiftedFKFoot,
		StaticInputIK,
		LiftedBallEffectiveWeight);
	TestEqual(
		TEXT("A lifted authored FK ball outside the ten-centimeter gate has zero procedural weight"),
		LiftedBallGeometryWeight,
		0.0f);
	TestTrue(
		TEXT("A static input IK track cannot pin an FK swing foot after the geometry gate releases"),
		LiftedResult.Equals(LiftedFKFoot, 0.001f));
	TestFalse(
		TEXT("The released swing foot does not retain the unrelated static input IK target"),
		LiftedResult.Equals(StaticInputIK, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFootPlacementPelvisAndSnapshotPodTest,
	"SurvivalRpg.Animation.FootPlacement.Runtime.PelvisAndSnapshotPod",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFootPlacementPelvisAndSnapshotPodTest::RunTest(const FString& Parameters)
{
	const FStructProperty* FKFootBoneProperty = FindFProperty<FStructProperty>(
		FRpgFootPlacementNodeLegDefinition::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(FRpgFootPlacementNodeLegDefinition, FKFootBone));
	if (TestNotNull(TEXT("The AnyThread leg definition exposes its authored FK foot"), FKFootBoneProperty))
	{
		TestTrue(
			TEXT("The FK foot contract uses Unreal's bone-reference value type"),
			FKFootBoneProperty->Struct == FBoneReference::StaticStruct());
	}

	TestTrue(
		TEXT("The lower requested foot drives the pelvis downward"),
		FMath::IsNearlyEqual(RpgFootPlacement::CalculatePelvisOffset(-12.0f, -4.0f, 50.0f), -12.0f));
	TestTrue(
		TEXT("The pelvis offset is bounded by the maximum downward correction"),
		FMath::IsNearlyEqual(RpgFootPlacement::CalculatePelvisOffset(-80.0f, -20.0f, 50.0f), -50.0f));
	TestTrue(
		TEXT("Positive foot offsets never pull the pelvis upward"),
		FMath::IsNearlyZero(RpgFootPlacement::CalculatePelvisOffset(5.0f, 12.0f, 50.0f)));
	TestTrue(
		TEXT("A negative maximum is treated as zero correction"),
		FMath::IsNearlyZero(RpgFootPlacement::CalculatePelvisOffset(-10.0f, -20.0f, -1.0f)));
	TestTrue(
		TEXT("Pelvis smoothing does not advance without elapsed update time"),
		FMath::IsNearlyZero(RpgFootPlacement::SmoothPelvisOffset(0.0f, -50.0f, 0.0f, 0.08f, 120.0f)));
	TestTrue(
		TEXT("Pelvis smoothing obeys its hard speed bound on a large target step"),
		FMath::IsNearlyEqual(
			RpgFootPlacement::SmoothPelvisOffset(0.0f, -50.0f, 1.0f / 20.0f, 0.08f, 120.0f),
			-6.0f,
			0.001f));
	TestTrue(
		TEXT("Pelvis smoothing is monotonic toward a reachable target"),
		RpgFootPlacement::SmoothPelvisOffset(-10.0f, -20.0f, 1.0f / 60.0f, 0.08f, 120.0f) < -10.0f);
	const float RecoveredPelvisOffset = RpgFootPlacement::SmoothPelvisOffset(
		-12.0f,
		0.0f,
		1.0f / 60.0f,
		0.08f,
		120.0f);
	TestTrue(
		TEXT("Pelvis recovery toward zero is gradual instead of an upward pop"),
		RecoveredPelvisOffset > -12.0f && RecoveredPelvisOffset < 0.0f);
	TestTrue(
		TEXT("Pelvis recovery obeys the configured 120 cm/s speed bound"),
		RecoveredPelvisOffset + 12.0f <= 120.0f / 60.0f + UE_SMALL_NUMBER);

	auto ValidateValueOnlyStruct = [this](UScriptStruct* ScriptStruct, bool bAllowLegSnapshot)
	{
		int32 ReflectedPropertyCount = 0;
		for (TFieldIterator<FProperty> PropertyIt(ScriptStruct); PropertyIt; ++PropertyIt)
		{
			++ReflectedPropertyCount;
			const FProperty* Property = *PropertyIt;
			TestTrue(
				FString::Printf(
					TEXT("%s.%s is a pointer-free scalar or approved math/value struct"),
					*ScriptStruct->GetName(),
					*Property->GetName()),
				RpgFootPlacementRuntimeTests::IsSnapshotValueProperty(Property, bAllowLegSnapshot));
		}
		TestTrue(
			FString::Printf(TEXT("%s exposes reflected snapshot fields"), *ScriptStruct->GetName()),
			ReflectedPropertyCount > 0);
	};

	ValidateValueOnlyStruct(FRpgFootPlacementLegSnapshot::StaticStruct(), false);
	ValidateValueOnlyStruct(FRpgFootPlacementSnapshot::StaticStruct(), true);

	FRpgFootPlacementSnapshot Snapshot;
	TestTrue(TEXT("A fresh snapshot requests a reset before its first valid sample"), Snapshot.bReset);
	TestFalse(TEXT("A fresh snapshot is not consumable by AnyThread evaluation"), Snapshot.bValid);
	TestTrue(
		TEXT("A fresh snapshot carries safe normalized fallback normals"),
		Snapshot.FloorNormalWorld.Equals(FVector::UpVector) &&
			Snapshot.LeftFoot.GroundNormalWorld.Equals(FVector::UpVector) &&
			Snapshot.RightFoot.GroundNormalWorld.Equals(FVector::UpVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgFootPlacementAnyThreadSourceContractTest,
	"SurvivalRpg.Animation.FootPlacement.Runtime.AnyThreadSourceContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFootPlacementAnyThreadSourceContractTest::RunTest(const FString& Parameters)
{
	const TCHAR* SourceFiles[] = {
		TEXT("SurvivalRpg/Animation/AnimNode_RpgFootPlacement.h"),
		TEXT("SurvivalRpg/Animation/AnimNode_RpgFootPlacement.cpp"),
		TEXT("SurvivalRpg/Animation/RpgFootPlacementTypes.cpp"),
	};
	const TCHAR* BannedTokens[] = {
		TEXT("Engine/World.h"),
		TEXT("GameFramework/Character.h"),
		TEXT("GameFramework/CharacterMovementComponent.h"),
		TEXT("Components/SkeletalMeshComponent.h"),
		TEXT("UWorld"),
		TEXT("AActor"),
		TEXT("UCharacterMovementComponent"),
		TEXT("USkeletalMeshComponent"),
		TEXT("UObject*"),
		TEXT("TObjectPtr"),
		TEXT("TWeakObjectPtr"),
		TEXT("FHitResult"),
		TEXT("FCollisionQueryParams"),
		TEXT("FCollisionShape"),
		TEXT("GetWorld"),
		TEXT("GetOwner"),
		TEXT("GetOwningActor"),
		TEXT("GetCharacterMovement"),
		TEXT("GetMovementBase"),
		TEXT("GetSkelMeshComponent"),
		TEXT("GetAnimInstanceObject"),
		TEXT("GetProxyOnAnyThread"),
		TEXT("GetProxyOnGameThread"),
		TEXT("GetComponentTransform"),
		TEXT("GetSocketTransform"),
		TEXT("GetBoneTransform"),
		TEXT("GetBoneLocation"),
		TEXT("GetComponentSpaceTransform(IKFootIndex)"),
		TEXT("CurrentFloor"),
		TEXT("SweepSingle"),
		TEXT("LineTrace"),
		TEXT("Output.AnimInstanceProxy"),
	};

	for (const TCHAR* RelativeSourceFile : SourceFiles)
	{
		const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), RelativeSourceFile);
		FString Source;
		if (!TestTrue(
			FString::Printf(TEXT("The AnyThread contract source exists: %s"), RelativeSourceFile),
			FFileHelper::LoadFileToString(Source, *SourcePath)))
		{
			continue;
		}

		for (const TCHAR* BannedToken : BannedTokens)
		{
			TestFalse(
				FString::Printf(
					TEXT("%s does not use banned AnyThread token '%s'"),
					RelativeSourceFile,
					BannedToken),
				Source.Contains(BannedToken, ESearchCase::CaseSensitive));
		}

		if (FCString::Strstr(RelativeSourceFile, TEXT("AnimNode_RpgFootPlacement.cpp")))
		{
			TestTrue(
				TEXT("The audited implementation contains the worker-thread skeletal-control entry point"),
				Source.Contains(TEXT("EvaluateSkeletalControl_AnyThread"), ESearchCase::CaseSensitive));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
