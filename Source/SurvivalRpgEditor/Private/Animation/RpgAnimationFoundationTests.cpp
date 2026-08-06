#include "SurvivalRpg/Core/Character/RpgCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgReplicatedAccelerationRoundTripTest,
	"SurvivalRpg.Animation.Network.ReplicatedAccelerationRoundTrip",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgReplicatedAccelerationRoundTripTest::RunTest(const FString& Parameters)
{
	constexpr double MaxAcceleration = 2400.0;
	// At full acceleration, half of an 8-bit angular bin contributes about 29.6 uu/s^2.
	constexpr double QuantizationTolerance = 35.0;
	constexpr double HalfAngularBinRadians = UE_PI / 255.0;

	const FVector TestValues[] = {
		FVector::ZeroVector,
		FVector(MaxAcceleration, 0.0, 0.0),
		FVector(0.0, -MaxAcceleration, 0.0),
		FVector(
			MaxAcceleration * FMath::Cos(HalfAngularBinRadians),
			MaxAcceleration * FMath::Sin(HalfAngularBinRadians),
			0.0),
		FVector(MaxAcceleration * 2.0, 0.0, 0.0),
		FVector(600.0, -1600.0, 900.0),
		FVector(-450.0, 325.0, -1200.0),
	};

	for (const FVector& Input : TestValues)
	{
		FRpgReplicatedAcceleration Packed;
		Packed.SetFromAcceleration(Input, MaxAcceleration);

		const FVector Expected = Input.GetClampedToMaxSize(MaxAcceleration);
		const FVector Actual = Packed.ToAcceleration(MaxAcceleration);
		TestTrue(
			*FString::Printf(TEXT("%s round-trips within quantization tolerance"), *Input.ToCompactString()),
			Actual.Equals(Expected, QuantizationTolerance));
	}

	FRpgReplicatedAcceleration InvalidMaximum;
	InvalidMaximum.SetFromAcceleration(FVector(100.0, 200.0, 300.0), 0.0);
	TestEqual(TEXT("A non-positive maximum decodes to zero"), InvalidMaximum.ToAcceleration(0.0), FVector::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRegularAnimationRemainsParallelTest,
	"SurvivalRpg.Animation.Threading.RegularAnimationRemainsParallel",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgRegularAnimationRemainsParallelTest::RunTest(const FString& Parameters)
{
	USkeletalMeshComponent* MeshComponent = NewObject<USkeletalMeshComponent>();
	URpgAnimInstance* AnimInstance = NewObject<URpgAnimInstance>(MeshComponent);

	TestTrue(
		TEXT("Animation outside a listen-server autonomous move tick remains parallel"),
		AnimInstance->CanRunParallelWork());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCharacterAnimBlueprintParentTest,
	"SurvivalRpg.Animation.Assets.CharacterAnimBlueprintParent",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCharacterAnimBlueprintParentTest::RunTest(const FString& Parameters)
{
	static const FSoftObjectPath AnimBlueprintPath(
		TEXT("/Game/SurvivalRpg/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed"));
	const UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(AnimBlueprintPath.TryLoad());
	if (!TestNotNull(TEXT("ABP_Unarmed loads"), AnimBlueprint))
	{
		return false;
	}

	TestTrue(
		TEXT("ABP_Unarmed derives from URpgAnimInstance"),
		AnimBlueprint->ParentClass && AnimBlueprint->ParentClass->IsChildOf(URpgAnimInstance::StaticClass()));
	TestTrue(
		TEXT("ABP_Unarmed allows multi-threaded animation update"),
		AnimBlueprint->bUseMultiThreadedAnimationUpdate);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
