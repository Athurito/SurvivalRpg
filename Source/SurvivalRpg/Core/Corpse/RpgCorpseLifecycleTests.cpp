#include "RpgCorpseLifecycleComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCorpseProfile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace RpgCorpseLifecycleTests
{
	class FScopedCorpseWorld
	{
	public:
		FScopedCorpseWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (GameInstance)
			{
				GameInstance->AddToRoot();
				GameInstance->InitializeStandalone();
				World = GameInstance->GetWorld();
			}
		}

		~FScopedCorpseWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		UWorld* GetWorld() const { return World; }

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCorpseRagdollVelocityTest,
	"SurvivalRpg.Corpse.Ragdoll.VelocityIsSanitizedAndClamped",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCorpseRagdollVelocityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FVector Clamped =
		URpgCorpseLifecycleComponent::CalculateRagdollStartVelocity(
			FVector(30.0f, 40.0f, 0.0f),
			2.0f,
			50.0f);
	TestTrue(
		TEXT("Scaled velocity is clamped to the configured maximum"),
		Clamped.Equals(FVector(30.0f, 40.0f, 0.0f), KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Negative multipliers fail closed to zero velocity"),
		URpgCorpseLifecycleComponent::CalculateRagdollStartVelocity(
			FVector(100.0f, 0.0f, 0.0f),
			-1.0f,
			500.0f).IsNearlyZero());
	TestTrue(
		TEXT("Non-finite velocity fails closed to zero"),
		URpgCorpseLifecycleComponent::CalculateRagdollStartVelocity(
			FVector(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f),
			1.0f,
			500.0f).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCorpseLifecycleAuthorityTest,
	"SurvivalRpg.Corpse.Lifecycle.RagdollRevisionAndCompletionAreIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCorpseLifecycleAuthorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RpgCorpseLifecycleTests::FScopedCorpseWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone corpse test world is available"), World))
	{
		return false;
	}

	ACharacter* Owner = World->SpawnActor<ACharacter>();
	if (!TestNotNull(TEXT("Corpse test character spawned"), Owner))
	{
		return false;
	}

	URpgCorpseLifecycleComponent* Corpse =
		NewObject<URpgCorpseLifecycleComponent>(Owner, TEXT("CorpseLifecycle"), RF_Transient);
	Corpse->SetupAttachment(Owner->GetMesh());
	Owner->AddInstanceComponent(Corpse);
	Corpse->RegisterComponent();

	TestTrue(TEXT("Test world actor has authority"), Owner->HasAuthority());
	Corpse->NotifyDeathStarted(FVector(300.0f, 0.0f, 0.0f));
	Corpse->NotifyDeathFinished();

	TestEqual(
		TEXT("Death finish increments exactly one replicated ragdoll revision"),
		Corpse->GetRagdollState().Revision,
		1);
	TestEqual(
		TEXT("Ragdoll revision is applied locally once"),
		Corpse->GetLastAppliedRagdollRevisionForTesting(),
		1);
	TestEqual(
		TEXT("Death finish enters the settle phase"),
		Corpse->GetLifecycleState(),
		ERpgCorpseLifecycleState::Settling);
	TestEqual(
		TEXT("Ragdoll keeps skeletal mesh world collision enabled"),
		Owner->GetMesh()->GetCollisionEnabled(),
		ECollisionEnabled::QueryAndPhysics);
	TestTrue(
		TEXT("Corpse presentation does not disable collision on the whole actor"),
		Owner->GetActorEnableCollision());

	Corpse->ApplyReplicatedPresentationForTesting();
	TestEqual(
		TEXT("Reapplying the same replicated revision is idempotent"),
		Corpse->GetLastAppliedRagdollRevisionForTesting(),
		1);

	Corpse->HandleSettleElapsedForTesting();
	TestTrue(TEXT("Settled corpse becomes available"), Corpse->IsCorpseAvailable());
	TestEqual(
		TEXT("Available corpse enables only query collision on its anchor"),
		Corpse->GetCollisionEnabled(),
		ECollisionEnabled::QueryOnly);

	Corpse->SetInventoryRequirementComplete(true);
	TestEqual(
		TEXT("Completing the configured inventory gate completes the corpse"),
		Corpse->GetLifecycleState(),
		ERpgCorpseLifecycleState::Completed);
	TestFalse(TEXT("Completed corpse is no longer interactable"), Corpse->IsCorpseAvailable());
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCorpseProfileValidationTest,
	"SurvivalRpg.Corpse.Profile.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCorpseProfileValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	URpgCorpseProfile* Profile = NewObject<URpgCorpseProfile>();
	FDataValidationContext ValidContext;
	TestEqual(
		TEXT("Native profile defaults are valid"),
		Profile->IsDataValid(ValidContext),
		EDataValidationResult::Valid);

	Profile->RagdollBoneName = NAME_None;
	Profile->InteractionRadius = 0.0f;
	Profile->MaximumLifetimeSeconds = 0.0f;
	Profile->RagdollCollisionProfileName = TEXT("NoCollision");
	FDataValidationContext InvalidContext;
	TestEqual(
		TEXT("Missing bones, invalid ranges, and a non-physical collision profile fail validation"),
		Profile->IsDataValid(InvalidContext),
		EDataValidationResult::Invalid);
	return true;
}
#endif

#endif
