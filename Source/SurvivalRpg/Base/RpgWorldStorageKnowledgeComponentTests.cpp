#include "RpgWorldStorageKnowledgeComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace RpgWorldStorageKnowledgeTests
{
	class FScopedKnowledgeWorld
	{
	public:
		FScopedKnowledgeWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (GameInstance)
			{
				GameInstance->AddToRoot();
				GameInstance->InitializeStandalone();
				World = GameInstance->GetWorld();
			}
		}

		~FScopedKnowledgeWorld()
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
	FRpgWorldStorageKnowledgeAuthorityAndSaveHooksTest,
	"SurvivalRpg.Storage.Knowledge.AuthorityIdempotencyAndSaveHooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgWorldStorageKnowledgeAuthorityAndSaveHooksTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	RpgWorldStorageKnowledgeTests::FScopedKnowledgeWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone storage-knowledge world is available"), World))
	{
		return false;
	}

	ARpgGameStateBase* GameState = World->SpawnActor<ARpgGameStateBase>();
	if (!TestNotNull(TEXT("RPG GameState spawned"), GameState))
	{
		return false;
	}
	World->SetGameState(GameState);

	URpgWorldStorageKnowledgeComponent* Knowledge =
		GameState->GetWorldStorageKnowledgeComponent();
	if (!TestNotNull(TEXT("GameState owns the world-storage knowledge component"), Knowledge))
	{
		return false;
	}
	TestTrue(TEXT("Knowledge state is configured for replication"), Knowledge->GetIsReplicated());
	TestTrue(TEXT("Standalone GameState has server authority"), GameState->HasAuthority());

	TestTrue(
		TEXT("First concrete discovery mutates authority state"),
		Knowledge->GrantKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestFalse(
		TEXT("Granting the same discovery twice is an idempotent no-op"),
		Knowledge->GrantKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestEqual(
		TEXT("Idempotent grant leaves exactly one discovery"),
		Knowledge->GetKnowledgeTags().Num(),
		1);

	GameState->SetRole(ROLE_SimulatedProxy);
	TestFalse(
		TEXT("A simulated client cannot grant world knowledge"),
		Knowledge->GrantKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftAnalysis));
	FRpgWorldStorageKnowledgeSaveData ClientImport;
	ClientImport.KnowledgeTags.AddTag(
		RpgGameplayTags::Storage_Knowledge_RiftAnalysis);
	TestFalse(
		TEXT("A simulated client cannot import host save state"),
		Knowledge->ImportSaveData(ClientImport));
	GameState->SetRole(ROLE_Authority);

	TestTrue(
		TEXT("Authority accepts a validated replacement snapshot"),
		Knowledge->ImportSaveData(ClientImport));
	TestFalse(
		TEXT("Replacement import removes discoveries absent from the snapshot"),
		Knowledge->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestTrue(
		TEXT("Replacement import restores the saved discovery"),
		Knowledge->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftAnalysis));
	TestTrue(
		TEXT("Importing the same validated snapshot remains successful"),
		Knowledge->ImportSaveData(ClientImport));

	FRpgWorldStorageKnowledgeSaveData InvalidImport;
	InvalidImport.KnowledgeTags.AddTag(RpgGameplayTags::Ability_Attack_Basic);
	TestFalse(
		TEXT("Save import rejects tags outside Storage.Knowledge"),
		Knowledge->ImportSaveData(InvalidImport));
	TestTrue(
		TEXT("Rejected save data cannot disturb the prior authoritative state"),
		Knowledge->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftAnalysis));

	const FRpgWorldStorageKnowledgeSaveData Exported = Knowledge->ExportSaveData();
	TestTrue(
		TEXT("Pointer-free export contains the authoritative discovery"),
		Exported.KnowledgeTags.HasTagExact(
			RpgGameplayTags::Storage_Knowledge_RiftAnalysis));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
