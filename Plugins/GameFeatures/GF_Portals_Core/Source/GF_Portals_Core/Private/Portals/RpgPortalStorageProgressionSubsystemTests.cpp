#include "Portals/RpgPortalStorageProgressionSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Misc/AutomationTest.h"
#include "GameplayTags/RpgPortalGameplayTags.h"
#include "Portals/RpgPortalEncounterDefinition.h"
#include "Portals/RpgPortalMessages.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

namespace RpgPortalStorageProgressionSubsystemTests
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

	int32 GetRewardQuantity(const URpgInventoryManagerComponent* Inventory)
	{
		if (!Inventory)
		{
			return 0;
		}

		const TArray<FRpgInventoryEntryView> Entries = Inventory->GetAllEntries();
		return Entries.Num() == 1 ? Entries[0].StackCount : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPortalStorageProgressionBroadcastGrantOnceTest,
	"SurvivalRpg.Portals.StorageProgression.BroadcastGrantsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPortalStorageProgressionBroadcastGrantOnceTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgPortalStorageProgressionSubsystemTests;

	FScopedPortalProgressionWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("Standalone broadcast world is available"), World))
	{
		return false;
	}

	URpgPortalStorageProgressionSubsystem* ProgressionSubsystem =
		World->GetSubsystem<URpgPortalStorageProgressionSubsystem>();
	if (!TestNotNull(
			TEXT("Portal storage progression world subsystem is instantiated"),
			ProgressionSubsystem))
	{
		return false;
	}

	// InitializeStandalone assigns the GameInstance after the dummy world's
	// subsystem initialization. Drive the normal begin-play retry explicitly.
	if (!ProgressionSubsystem->HasCalledBeginPlay())
	{
		ProgressionSubsystem->OnWorldBeginPlay(*World);
	}

	ARpgGameStateBase* GameState = World->SpawnActor<ARpgGameStateBase>();
	if (!TestNotNull(TEXT("Broadcast test GameState spawned"), GameState))
	{
		return false;
	}
	World->SetGameState(GameState);

	URpgWorldStorageKnowledgeComponent* Knowledge =
		GameState->GetWorldStorageKnowledgeComponent();
	ARpgPlayerState* Recipient = World->SpawnActor<ARpgPlayerState>();
	AActor* PortalSource = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Broadcast test knowledge component exists"), Knowledge) ||
		!TestNotNull(TEXT("Broadcast reward recipient spawned"), Recipient) ||
		!TestNotNull(TEXT("Broadcast portal source spawned"), PortalSource))
	{
		return false;
	}

	URpgInventoryManagerComponent* RecipientInventory =
		Recipient->GetInventoryManagerComponent();
	if (!TestNotNull(
			TEXT("Broadcast reward recipient owns canonical inventory"),
			RecipientInventory))
	{
		return false;
	}

	URpgPortalEncounterDefinition* Encounter =
		NewObject<URpgPortalEncounterDefinition>(GetTransientPackage());
	Encounter->FirstEligibleKnowledgeRewardTable =
		MakeGuaranteedRewardTable(Encounter);

	FRpgPortalCompletedMessage Completion;
	Completion.Portal = PortalSource;
	Completion.Instigator = Recipient;
	Completion.EncounterDefinition = Encounter;
	Completion.bRewardsEligible = true;

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	int32 ObservedBroadcastCount = 0;
	FGameplayMessageListenerHandle ObserverHandle =
		MessageSubsystem.RegisterListener<FRpgPortalCompletedMessage>(
			RpgPortalGameplayTags::Rpg_Portal_Message_Completed,
			[&ObservedBroadcastCount](
				FGameplayTag Channel,
				const FRpgPortalCompletedMessage& Message)
			{
				(void)Channel;
				(void)Message;
				++ObservedBroadcastCount;
			});

	MessageSubsystem.BroadcastMessage(
		RpgPortalGameplayTags::Rpg_Portal_Message_Completed,
		Completion);
	TestTrue(
		TEXT("Generic completion broadcast grants Rift-containment knowledge"),
		Knowledge->HasKnowledgeTag(
			RpgGameplayTags::Storage_Knowledge_RiftContainment));
	TestEqual(
		TEXT("First completion broadcast delivers the guaranteed reward once"),
		GetRewardQuantity(RecipientInventory),
		2);

	MessageSubsystem.BroadcastMessage(
		RpgPortalGameplayTags::Rpg_Portal_Message_Completed,
		Completion);
	TestEqual(
		TEXT("Both completion messages reached the gameplay-message channel"),
		ObservedBroadcastCount,
		2);
	TestEqual(
		TEXT("Repeated completion broadcast cannot duplicate the reward"),
		GetRewardQuantity(RecipientInventory),
		2);
	TestEqual(
		TEXT("Repeated completion leaves exactly one world-knowledge discovery"),
		Knowledge->GetKnowledgeTags().Num(),
		1);

	ObserverHandle.Unregister();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
