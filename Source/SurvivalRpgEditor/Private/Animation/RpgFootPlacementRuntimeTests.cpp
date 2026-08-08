// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
	Settings.PlantSpeedThreshold = 60.0f;
	Settings.PlantDistanceThreshold = 10.0f;
	Settings.UnplantRadius = 35.0f;
	Settings.ReplantRadiusRatio = 0.35f;
	Settings.UnplantAngle = 45.0f;
	Settings.ReplantAngleRatio = 0.5f;

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
		RpgFootPlacement::ShouldUnplantFoot(60.0f, 35.0f, 45.0f, Settings));
	TestTrue(
		TEXT("Foot speed independently releases an existing lock"),
		RpgFootPlacement::ShouldUnplantFoot(60.01f, 0.0f, 0.0f, Settings));
	TestTrue(
		TEXT("Anchor drift independently releases an existing lock"),
		RpgFootPlacement::ShouldUnplantFoot(0.0f, 35.01f, 0.0f, Settings));
	TestTrue(
		TEXT("Ground-normal change independently releases an existing lock"),
		RpgFootPlacement::ShouldUnplantFoot(0.0f, 0.0f, 45.01f, Settings));
	TestTrue(
		TEXT("A released lock replants inside the tighter radius and angle bounds"),
		RpgFootPlacement::ShouldReplantFoot(12.25f, 22.5f, Settings));
	TestFalse(
		TEXT("A released lock remains unplanted outside the replant radius"),
		RpgFootPlacement::ShouldReplantFoot(12.26f, 0.0f, Settings));
	TestFalse(
		TEXT("A released lock remains unplanted outside the replant angle"),
		RpgFootPlacement::ShouldReplantFoot(0.0f, 22.51f, Settings));
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
	FRpgFootPlacementPelvisAndSnapshotPodTest,
	"SurvivalRpg.Animation.FootPlacement.Runtime.PelvisAndSnapshotPod",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgFootPlacementPelvisAndSnapshotPodTest::RunTest(const FString& Parameters)
{
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
