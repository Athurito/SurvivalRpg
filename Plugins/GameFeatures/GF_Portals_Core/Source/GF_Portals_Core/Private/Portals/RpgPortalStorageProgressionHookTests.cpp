#include "Portals/RpgPortalStorageProgressionHook.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "Portals/RpgPortalEncounterDefinition.h"
#include "Portals/RpgPortalMessages.h"

namespace RpgPortalStorageProgressionTests
{
	class FScopedPortalProgressionWorld
	{
	public:
		FScopedPortalProgressionWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (GameInstance)
			{
				GameInstance->AddToRoot();
				GameInstance->InitializeStandalone();
				World = GameInstance->GetWorld();
			}
		}

		~FScopedPortalProgressionWorld()
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

	URpgLootTable* MakeGuaranteedRewardTable(UObject* Outer)
	{
		URpgLootTable* RewardTable = NewObject<URpgLootTable>(Outer);
		FRpgLootGroup& Group = RewardTable->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition =
			URpgInventoryAutomationTestStackItemDefinition::StaticClass();
		Entry.MinimumQuantity = 2;
		Entry.MaximumQuantity = 2;
		Entry.ChancePercent = 100.0f;
		Entry.bScaleChanceWithRareFind = false;
		Entry.bScaleQuantityWithYield = false;
		return RewardTable;
	}

	int32 CountLiveRewardDrops(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgDroppedInventoryActor> Iterator(World); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator) && !Iterator->IsActorBeingDestroyed())
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPortalStorageProgressionAuthorityAndIdempotencyTest,
	"SurvivalRpg.Portals.StorageProgression.AuthorityAndIdempotency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPortalStorageProgressionAuthorityAndIdempotencyTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	RpgPortalStorageProgressionTests::FScopedPortalProgressionWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone portal-progression world is available"), World))
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
	if (!TestNotNull(TEXT("Portal hook can resolve core storage knowledge"), Knowledge))
	{
		return false;
	}
	ARpgPlayerState* Recipient = World->SpawnActor<ARpgPlayerState>();
	AActor* PortalSource = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Reward recipient PlayerState spawned"), Recipient) ||
		!TestNotNull(TEXT("Authoritative portal reward source spawned"), PortalSource))
	{
		return false;
	}
	URpgInventoryManagerComponent* RecipientInventory =
		Recipient->GetInventoryManagerComponent();
	if (!TestNotNull(TEXT("Reward recipient owns the canonical inventory"), RecipientInventory))
	{
		return false;
	}

	URpgPortalEncounterDefinition* Encounter =
		NewObject<URpgPortalEncounterDefinition>(GetTransientPackage());
	URpgLootTable* RewardTable =
		RpgPortalStorageProgressionTests::MakeGuaranteedRewardTable(Encounter);
	Encounter->FirstEligibleKnowledgeRewardTable = RewardTable;

	FRpgPortalCompletedMessage FirstCompletion;
	FirstCompletion.Portal = PortalSource;
	FirstCompletion.Instigator = Recipient;
	FirstCompletion.EncounterDefinition = Encounter;
	FirstCompletion.bRewardsEligible = true;
	TestTrue(
		TEXT("First eligible completion creates Rift-containment knowledge"),
		FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
			World,
			FirstCompletion));
	TestTrue(
		TEXT("Authoritative world now owns Rift-containment knowledge"),
		Knowledge->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestTrue(
		TEXT("Completion payload identifies the newly granted knowledge"),
		FirstCompletion.NewlyGrantedWorldKnowledgeTags.HasTagExact(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestEqual(
		TEXT("Designer-authored soft reward hook is forwarded once"),
		FirstCompletion.FirstEligibleKnowledgeRewardTable.Get(),
		RewardTable);
	TArray<FRpgInventoryEntryView> RewardEntries =
		RecipientInventory->GetAllEntries();
	TestEqual(
		TEXT("Guaranteed reward creates one canonical inventory stack"),
		RewardEntries.Num(),
		1);
	if (RewardEntries.Num() == 1)
	{
		TestEqual(
			TEXT("Guaranteed reward delivers the fixed authored quantity"),
			RewardEntries[0].StackCount,
			2);
	}

	FRpgPortalCompletedMessage RepeatedCompletion;
	RepeatedCompletion.Portal = PortalSource;
	RepeatedCompletion.Instigator = Recipient;
	RepeatedCompletion.EncounterDefinition = Encounter;
	RepeatedCompletion.bRewardsEligible = true;
	TestFalse(
		TEXT("A later eligible completion cannot grant the same world discovery"),
		FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
			World,
			RepeatedCompletion));
	TestTrue(
		TEXT("Repeated completion exposes no duplicate reward trigger"),
		RepeatedCompletion.NewlyGrantedWorldKnowledgeTags.IsEmpty() &&
		RepeatedCompletion.FirstEligibleKnowledgeRewardTable.IsNull());
	RewardEntries = RecipientInventory->GetAllEntries();
	TestEqual(
		TEXT("Repeated completion cannot duplicate the delivered stack"),
		RewardEntries.Num() == 1 ? RewardEntries[0].StackCount : 0,
		2);

	FRpgWorldStorageKnowledgeSaveData EmptySnapshot;
	TestTrue(TEXT("Authority resets the fixture through the save hook"), Knowledge->ImportSaveData(EmptySnapshot));
	FRpgPortalCompletedMessage IneligibleCompletion;
	IneligibleCompletion.Portal = PortalSource;
	IneligibleCompletion.Instigator = Recipient;
	IneligibleCompletion.EncounterDefinition = Encounter;
	IneligibleCompletion.bRewardsEligible = false;
	TestFalse(
		TEXT("An ineligible completion cannot unlock storage progression"),
		FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
			World,
			IneligibleCompletion));
	TestFalse(
		TEXT("Ineligible completion leaves knowledge locked"),
		Knowledge->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));

	GameState->SetRole(ROLE_SimulatedProxy);
	FRpgPortalCompletedMessage ClientCompletion;
	ClientCompletion.Portal = PortalSource;
	ClientCompletion.Instigator = Recipient;
	ClientCompletion.EncounterDefinition = Encounter;
	ClientCompletion.bRewardsEligible = true;
	TestFalse(
		TEXT("The plugin hook refuses to mutate a non-authoritative GameState"),
		FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
			World,
			ClientCompletion));
	TestFalse(
		TEXT("Client-side hook evaluation cannot create world knowledge"),
		Knowledge->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	GameState->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPortalStorageProgressionFullInventoryDefersGrantTest,
	"SurvivalRpg.Portals.StorageProgression.FullInventoryDefersGrantWithoutEphemeralDrop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPortalStorageProgressionFullInventoryDefersGrantTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	RpgPortalStorageProgressionTests::FScopedPortalProgressionWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone drop-fallback world is available"), World))
	{
		return false;
	}

	ARpgGameStateBase* GameState = World->SpawnActor<ARpgGameStateBase>();
	if (!TestNotNull(TEXT("Drop-fallback GameState spawned"), GameState))
	{
		return false;
	}
	World->SetGameState(GameState);
	ARpgPlayerState* Recipient = World->SpawnActor<ARpgPlayerState>();
	AActor* PortalSource = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Drop-fallback recipient spawned"), Recipient) ||
		!TestNotNull(TEXT("Drop-fallback source spawned"), PortalSource))
	{
		return false;
	}

	URpgInventoryManagerComponent* RecipientInventory =
		Recipient->GetInventoryManagerComponent();
	if (!TestNotNull(TEXT("Drop-fallback recipient inventory exists"), RecipientInventory))
	{
		return false;
	}
	RecipientInventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	RecipientInventory->SetFixedMaxEntries(0);
	TestFalse(
		TEXT("Zero-entry recipient cannot accept the guaranteed batch"),
		RecipientInventory->CanAddItemDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2));

	URpgPortalEncounterDefinition* Encounter =
		NewObject<URpgPortalEncounterDefinition>(GetTransientPackage());
	Encounter->FirstEligibleKnowledgeRewardTable =
		RpgPortalStorageProgressionTests::MakeGuaranteedRewardTable(Encounter);

	FRpgPortalCompletedMessage Completion;
	Completion.Portal = PortalSource;
	Completion.Instigator = Recipient;
	Completion.EncounterDefinition = Encounter;
	Completion.bRewardsEligible = true;
	TestFalse(
		TEXT("Full inventory defers the atomic grant instead of accepting an unsaved world drop"),
		FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
			World,
			Completion));
	TestFalse(
		TEXT("Failed durable delivery rolls Rift-containment knowledge back"),
		GameState->GetWorldStorageKnowledgeComponent()->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestTrue(
		TEXT("Full recipient inventory remains untouched"),
		RecipientInventory->GetAllEntries().IsEmpty());
	TestEqual(
		TEXT("No ephemeral reward drop is accepted as completed delivery"),
		RpgPortalStorageProgressionTests::CountLiveRewardDrops(World),
		0);
	TestTrue(
		TEXT("Failed completion exposes no grant payload"),
		Completion.NewlyGrantedWorldKnowledgeTags.IsEmpty() &&
		Completion.FirstEligibleKnowledgeRewardTable.IsNull());

	RecipientInventory->SetFixedMaxEntries(1);
	FRpgPortalCompletedMessage Replay;
	Replay.Portal = PortalSource;
	Replay.Instigator = Recipient;
	Replay.EncounterDefinition = Encounter;
	Replay.bRewardsEligible = true;
	TestTrue(
		TEXT("A later eligible completion retries and completes after durable inventory space exists"),
		FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
			World,
			Replay));
	TestTrue(
		TEXT("Successful durable retry records Rift-containment knowledge"),
		GameState->GetWorldStorageKnowledgeComponent()->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestEqual(
		TEXT("Durable retry delivers the authored quantity exactly once"),
		RecipientInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		2);

	FRpgPortalCompletedMessage IdempotentReplay = Replay;
	TestFalse(
		TEXT("Completion after the successful retry is idempotent"),
		FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
			World,
			IdempotentReplay));
	TestEqual(
		TEXT("Idempotent replay cannot duplicate the durable reward"),
		RecipientInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
