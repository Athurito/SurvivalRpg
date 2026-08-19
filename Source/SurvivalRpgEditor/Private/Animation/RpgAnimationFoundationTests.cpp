#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprint.h"
#include "Components/SkeletalMeshComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "Engine/Blueprint.h"
#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"

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
