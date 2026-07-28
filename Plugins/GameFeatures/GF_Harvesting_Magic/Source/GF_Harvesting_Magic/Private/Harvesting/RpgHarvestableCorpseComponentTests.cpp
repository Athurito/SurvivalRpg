#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemComponent.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgCorpseHarvestProfile.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestableCorpseComponent.h"
#include "Harvesting/RpgHarvestToolSelection.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseLifecycleComponent.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseProfile.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace RpgHarvestableCorpseTests
{
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (GameInstance)
			{
				GameInstance->AddToRoot();
				GameInstance->InitializeStandalone();
				World = GameInstance->GetWorld();
			}
		}

		~FScopedTestWorld()
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
			GFrameCounter = CachedFrameCounter;
		}

		UWorld* GetWorld() const { return World; }

		void PrimeTimerManager() const
		{
			if (World)
			{
				++GFrameCounter;
				World->GetTimerManager().Tick(0.0f);
			}
		}

		void AdvanceTimers(const float DeltaSeconds) const
		{
			if (World)
			{
				World->TimeSeconds += DeltaSeconds;
				World->UnpausedTimeSeconds += DeltaSeconds;
				World->RealTimeSeconds += DeltaSeconds;
				++GFrameCounter;
				World->GetTimerManager().Tick(DeltaSeconds);
			}
		}

	private:
		const uint64 CachedFrameCounter = GFrameCounter;
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	ARpgHarvestAutomationTestPlayerState* SpawnHarvester(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Parameters;
		Parameters.Name = MakeUniqueObjectName(
			World,
			ARpgHarvestAutomationTestPlayerState::StaticClass(),
			TEXT("CorpseHarvester"));
		Parameters.ObjectFlags = RF_Transient;
		Parameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ARpgHarvestAutomationTestPlayerState>(Parameters);
	}

	URpgCorpseHarvestProfile* MakeHarvestProfile(UObject* Outer)
	{
		URpgCorpseHarvestProfile* Profile = NewObject<URpgCorpseHarvestProfile>(Outer);
		URpgLootTable* Table = NewObject<URpgLootTable>(Profile);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition = URpgHarvestAutomationTestStackItemDefinition::StaticClass();
		Entry.MinimumQuantity = 2;
		Entry.MaximumQuantity = 2;
		Entry.ChancePercent = 100.0f;
		Profile->LootTable = Table;
		Profile->SkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Skinning;
		Profile->MinimumSkillLevel = 1;
		Profile->SkillExperience = 20;
		Profile->RequiredToolTag =
			RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning;
		Profile->CorpseCompletionTag =
			RpgHarvestingMagicGameplayTags::Rpg_Corpse_Completion_Harvest;
		Profile->ReservationTimeoutSeconds = 30.0f;
		return Profile;
	}

	bool SetHarvestProfile(
		URpgHarvestableCorpseComponent* Component,
		URpgCorpseHarvestProfile* Profile)
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(
			URpgHarvestableCorpseComponent::StaticClass(),
			TEXT("HarvestProfile"));
		if (!Component || !Property)
		{
			return false;
		}
		Property->SetObjectPropertyValue_InContainer(Component, Profile);
		return true;
	}

	ARpgHarvestAutomationCorpseActor* SpawnAvailableCorpse(
		UWorld* World,
		URpgCorpseHarvestProfile*& OutHarvestProfile)
	{
		ARpgHarvestAutomationCorpseActor* Corpse =
			World ? World->SpawnActorDeferred<ARpgHarvestAutomationCorpseActor>(
				ARpgHarvestAutomationCorpseActor::StaticClass(),
				FTransform::Identity,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
		if (!Corpse || !Corpse->CorpseLifecycle || !Corpse->HarvestableCorpse)
		{
			return nullptr;
		}

		URpgCorpseProfile* CorpseProfile = NewObject<URpgCorpseProfile>(Corpse);
		CorpseProfile->SettleDelaySeconds = 0.0f;
		CorpseProfile->EmptyDespawnDelaySeconds = 100.0f;
		CorpseProfile->MaximumLifetimeSeconds = 100.0f;
		CorpseProfile->bRequireInventoryEmpty = false;
		CorpseProfile->RequiredExternalCompletionTags.AddTag(
			RpgHarvestingMagicGameplayTags::Rpg_Corpse_Completion_Harvest);
		Corpse->CorpseLifecycle->CorpseProfile = CorpseProfile;

		OutHarvestProfile = MakeHarvestProfile(Corpse);
		if (!SetHarvestProfile(Corpse->HarvestableCorpse, OutHarvestProfile))
		{
			return nullptr;
		}

		Corpse->FinishSpawning(FTransform::Identity);
		if (!Corpse->HasActorBegunPlay())
		{
			Corpse->DispatchBeginPlay();
		}
		Corpse->CorpseLifecycle->NotifyDeathStarted(FVector::ZeroVector);
		Corpse->CorpseLifecycle->NotifyDeathFinished();
		return Corpse;
	}

	int32 CountWorldDrops(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestToolDeterministicSelectionTest,
	"SurvivalRpg.Interaction.Harvesting.Corpse.DeterministicBestOwnedTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestToolDeterministicSelectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableCorpseTests;
	FScopedTestWorld TestWorld;
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(TestWorld.GetWorld());
	URpgInventoryManagerComponent* Inventory =
		Harvester ? Harvester->GetInventoryManagerComponent() : nullptr;
	if (!TestNotNull(TEXT("Harvester exists"), Harvester) ||
		!TestNotNull(TEXT("Harvester inventory exists"), Inventory))
	{
		return false;
	}

	URpgInventoryItemInstance* Low = Inventory->GrantItemDefinition(
		URpgHarvestAutomationTestLowToolDefinition::StaticClass());
	URpgInventoryItemInstance* High = Inventory->GrantItemDefinition(
		URpgHarvestAutomationTestHighToolDefinition::StaticClass());
	URpgInventoryItemInstance* Tie = Inventory->GrantItemDefinition(
		URpgHarvestAutomationTestTieToolDefinition::StaticClass());
	if (!TestNotNull(TEXT("Low-power tool granted"), Low) ||
		!TestNotNull(TEXT("High-power tool granted"), High) ||
		!TestNotNull(TEXT("Equal-power tool granted"), Tie))
	{
		return false;
	}

	const FRpgSelectedHarvestTool Selected = FRpgHarvestToolSelection::FindBestOwnedTool(
		Inventory,
		RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning);
	const FRpgInventoryItemId ExpectedTieWinner =
		High->GetItemId().ToString() < Tie->GetItemId().ToString()
			? High->GetItemId()
			: Tie->GetItemId();
	TestTrue(TEXT("A valid best tool is selected"), Selected.IsValid());
	TestEqual(TEXT("Highest power wins before lower power"), Selected.HarvestPower, 2.0f);
	TestTrue(TEXT("Equal power resolves by stable item id"), Selected.ItemId == ExpectedTieWinner);
	TestTrue(
		TEXT("Exact owned-tool lookup resolves the selected persistent identity"),
		FRpgHarvestToolSelection::FindOwnedToolById(
			Inventory,
			Selected.ItemId,
			RpgHarvestingMagicGameplayTags::Tool_Harvesting_Skinning).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestableCorpseReservationCommitTest,
	"SurvivalRpg.Interaction.Harvesting.Corpse.ReservationToolLossCommitAndExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestableCorpseReservationCommitTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableCorpseTests;
	FScopedTestWorld TestWorld;
	URpgCorpseHarvestProfile* HarvestProfile = nullptr;
	ARpgHarvestAutomationCorpseActor* Corpse =
		SpawnAvailableCorpse(TestWorld.GetWorld(), HarvestProfile);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(TestWorld.GetWorld());
	URpgHarvestableCorpseComponent* Harvestable = Corpse ? Corpse->HarvestableCorpse : nullptr;
	URpgInventoryManagerComponent* Inventory =
		Harvester ? Harvester->GetInventoryManagerComponent() : nullptr;
	URpgTradeSkillProgressionComponent* Skills =
		Harvester ? Harvester->GetTradeSkillProgressionComponent() : nullptr;
	if (!TestNotNull(TEXT("Available corpse fixture exists"), Corpse) ||
		!TestNotNull(TEXT("Corpse harvest component exists"), Harvestable) ||
		!TestNotNull(TEXT("Harvester exists"), Harvester) ||
		!TestNotNull(TEXT("Harvester inventory exists"), Inventory) ||
		!TestNotNull(TEXT("Skinning progression exists"), Skills))
	{
		return false;
	}

	TestTrue(TEXT("Settled corpse is available"), Harvestable->IsHarvestAvailable());
	TestEqual(TEXT("Fresh corpse starts at revision zero"), Harvestable->GetHarvestRevision(), 0);
	TestFalse(
		TEXT("Harvest cannot begin without the required owned tool"),
		Harvestable->CanBeginHarvest(Harvester, 0, FRpgInventoryItemId()));

	URpgInventoryItemInstance* Tool = Inventory->GrantItemDefinition(
		URpgHarvestAutomationTestHighToolDefinition::StaticClass());
	if (!TestNotNull(TEXT("Skinning tool granted"), Tool))
	{
		return false;
	}
	const FRpgInventoryItemId FirstToolId = Tool->GetItemId();
	HarvestProfile->MinimumSkillLevel = 2;
	TestFalse(
		TEXT("Skinning minimum level is enforced before reservation"),
		Harvestable->CanBeginHarvest(Harvester, 0, FirstToolId));
	HarvestProfile->MinimumSkillLevel = 1;
	int32 ReservationRevision = INDEX_NONE;
	TestTrue(
		TEXT("Exact current revision reserves once"),
		Harvestable->TryReserveHarvest(Harvester, 0, FirstToolId, ReservationRevision));
	TestEqual(TEXT("Reservation advances revision"), ReservationRevision, 1);
	TestFalse(TEXT("Reserved corpse exposes no second harvest"), Harvestable->IsHarvestAvailable());
	int32 ConcurrentRevision = INDEX_NONE;
	TestFalse(
		TEXT("Concurrent request cannot reserve the spent revision"),
		Harvestable->TryReserveHarvest(Harvester, 0, FirstToolId, ConcurrentRevision));

	Harvestable->CancelHarvestReservation(Harvester, 0);
	TestTrue(TEXT("Stale cancellation cannot unlock current reservation"), Harvestable->GetHarvestState().bReserved);
	Harvestable->CancelHarvestReservation(Harvester, ReservationRevision);
	TestTrue(TEXT("Matching cancellation releases reservation"), Harvestable->IsHarvestAvailable());
	TestEqual(TEXT("Cancellation advances revision"), Harvestable->GetHarvestRevision(), 2);

	HarvestProfile->ReservationTimeoutSeconds = 0.01f;
	// Standalone automation worlds do not tick their timer manager during setup.
	// Prime it before installing the timeout so the next tick advances time instead
	// of merely establishing the timer manager's initial frame.
	TestWorld.PrimeTimerManager();
	int32 ReservationEndedSignals = 0;
	AActor* SignaledHarvester = nullptr;
	int32 SignaledRevision = INDEX_NONE;
	const FDelegateHandle ReservationEndedHandle =
		Harvestable->OnHarvestReservationEndedNative().AddLambda(
			[&ReservationEndedSignals, &SignaledHarvester, &SignaledRevision](
				URpgHarvestableCorpseComponent*,
				AActor* EndedHarvester,
				const int32 EndedRevision)
			{
				++ReservationEndedSignals;
				SignaledHarvester = EndedHarvester;
				SignaledRevision = EndedRevision;
			});
	TestTrue(
		TEXT("A fresh reservation is protected by the server timeout"),
		Harvestable->TryReserveHarvest(Harvester, 2, FirstToolId, ReservationRevision));
	const int32 TimedOutReservationRevision = ReservationRevision;
	TestWorld.AdvanceTimers(0.11f);
	TestTrue(TEXT("Reservation timeout releases a disconnected/stalled harvester"), Harvestable->IsHarvestAvailable());
	TestEqual(TEXT("Timeout release advances revision"), Harvestable->GetHarvestRevision(), 4);
	TestEqual(TEXT("Timeout emits one active-ability cancellation signal"), ReservationEndedSignals, 1);
	TestTrue(TEXT("Timeout identifies the reserved harvester"), SignaledHarvester == Harvester);
	TestEqual(TEXT("Timeout identifies the ended reservation revision"), SignaledRevision, TimedOutReservationRevision);
	Harvestable->OnHarvestReservationEndedNative().Remove(ReservationEndedHandle);
	HarvestProfile->ReservationTimeoutSeconds = 30.0f;

	TestTrue(
		TEXT("Fresh revision can reserve again"),
		Harvestable->TryReserveHarvest(
			Harvester,
			Harvestable->GetHarvestRevision(),
			FirstToolId,
			ReservationRevision));
	UAbilitySystemComponent* HarvesterAbilitySystem = Harvester->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Harvester ability system exists"), HarvesterAbilitySystem))
	{
		return false;
	}
	HarvesterAbilitySystem->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death, 1);
	TestFalse(
		TEXT("A dead harvester cannot commit at the montage notify"),
		Harvestable->TryCommitReservedHarvest(Harvester, ReservationRevision, FirstToolId));
	HarvesterAbilitySystem->SetLooseGameplayTagCount(RpgGameplayTags::Status_Death, 0);
	TestTrue(
		TEXT("Selected tool can be removed during the montage"),
		Inventory->ConsumeItemById(FirstToolId, 1).IsSuccess());
	TestFalse(
		TEXT("Tool loss makes the notify-time commit fail without reward"),
		Harvestable->TryCommitReservedHarvest(Harvester, ReservationRevision, FirstToolId));
	TestEqual(
		TEXT("Failed commit grants no material"),
		Inventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		0);
	TestEqual(
		TEXT("Failed commit grants no Skinning XP"),
		Skills->GetSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Skinning),
		0.0f);
	Harvestable->CancelHarvestReservation(Harvester, ReservationRevision);

	Tool = Inventory->GrantItemDefinition(
		URpgHarvestAutomationTestHighToolDefinition::StaticClass());
	if (!TestNotNull(TEXT("Replacement skinning tool granted"), Tool))
	{
		return false;
	}
	const FRpgInventoryItemId ReplacementToolId = Tool->GetItemId();
	const int32 FreshRevision = Harvestable->GetHarvestRevision();
	TestTrue(
		TEXT("Replacement tool reserves the current revision"),
		Harvestable->TryReserveHarvest(
			Harvester,
			FreshRevision,
			ReplacementToolId,
			ReservationRevision));
	int32 ReentrantPostCommitCalls = 0;
	bool bReentrantCommitSucceeded = false;
	const FDelegateHandle PostCommitHandle = Inventory->OnInventoryPostCommit.AddLambda(
		[&](URpgInventoryManagerComponent* CommittedInventory)
		{
			if (CommittedInventory != Inventory)
			{
				return;
			}
			++ReentrantPostCommitCalls;
			bReentrantCommitSucceeded = Harvestable->TryCommitReservedHarvest(
				Harvester,
				ReservationRevision,
				ReplacementToolId);
			Harvestable->CancelHarvestReservation(Harvester, ReservationRevision);
		});
	TestTrue(
		TEXT("Notify-time commit delivers the complete reward"),
		Harvestable->TryCommitReservedHarvest(
			Harvester,
			ReservationRevision,
			ReplacementToolId));
	Inventory->OnInventoryPostCommit.Remove(PostCommitHandle);
	TestEqual(TEXT("Reward delivery emits one player-inventory post-commit"), ReentrantPostCommitCalls, 1);
	TestFalse(TEXT("Player-inventory post-commit cannot reenter the corpse commit"), bReentrantCommitSucceeded);
	TestEqual(
		TEXT("Post-commit cancellation cannot advance the active reservation during delivery"),
		Harvestable->GetHarvestRevision(),
		ReservationRevision + 1);
	TestEqual(TEXT("Reentrant delivery creates no duplicate overflow drop"), CountWorldDrops(TestWorld.GetWorld()), 0);
	TestTrue(TEXT("Successful harvest permanently completes this target"), Harvestable->GetHarvestState().bCompleted);
	TestEqual(
		TEXT("Successful harvest grants exactly one complete material batch"),
		Inventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		2);
	TestEqual(
		TEXT("Successful harvest grants exactly one Skinning XP award"),
		Skills->GetSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Skinning),
		20.0f);
	TestFalse(
		TEXT("Committed reservation cannot replay"),
		Harvestable->TryCommitReservedHarvest(
			Harvester,
			ReservationRevision,
			ReplacementToolId));
	TestEqual(
		TEXT("Replay grants no duplicate material"),
		Inventory->GetTotalItemCountByDefinition(
			URpgHarvestAutomationTestStackItemDefinition::StaticClass()),
		2);
	TestEqual(
		TEXT("Replay grants no duplicate XP"),
		Skills->GetSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Skinning),
		20.0f);
	TestTrue(
		TEXT("External harvest completion advances the core corpse lifecycle"),
		Corpse->CorpseLifecycle->GetLifecycleState() == ERpgCorpseLifecycleState::Completed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHarvestableCorpseDisconnectedReservationTest,
	"SurvivalRpg.Interaction.Harvesting.Corpse.DisconnectReleasesReservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHarvestableCorpseDisconnectedReservationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgHarvestableCorpseTests;
	FScopedTestWorld TestWorld;
	URpgCorpseHarvestProfile* HarvestProfile = nullptr;
	ARpgHarvestAutomationCorpseActor* Corpse =
		SpawnAvailableCorpse(TestWorld.GetWorld(), HarvestProfile);
	ARpgHarvestAutomationTestPlayerState* Harvester = SpawnHarvester(TestWorld.GetWorld());
	URpgHarvestableCorpseComponent* Harvestable = Corpse ? Corpse->HarvestableCorpse : nullptr;
	URpgInventoryManagerComponent* Inventory =
		Harvester ? Harvester->GetInventoryManagerComponent() : nullptr;
	if (!TestNotNull(TEXT("Available corpse fixture exists"), Corpse) ||
		!TestNotNull(TEXT("Corpse harvest component exists"), Harvestable) ||
		!TestNotNull(TEXT("Harvester inventory exists"), Inventory))
	{
		return false;
	}

	HarvestProfile->ReservationTimeoutSeconds = 0.01f;
	TestWorld.PrimeTimerManager();
	URpgInventoryItemInstance* Tool = Inventory->GrantItemDefinition(
		URpgHarvestAutomationTestHighToolDefinition::StaticClass());
	if (!TestNotNull(TEXT("Skinning tool granted"), Tool))
	{
		return false;
	}
	int32 ReservationRevision = INDEX_NONE;
	TestTrue(
		TEXT("Harvester owns the reservation before disconnect"),
		Harvestable->TryReserveHarvest(Harvester, 0, Tool->GetItemId(), ReservationRevision));
	TestTrue(TEXT("Disconnect destroys the authoritative harvester actor"), Harvester->Destroy());
	TestWorld.AdvanceTimers(0.11f);
	TestTrue(
		TEXT("Timeout releases a reservation whose harvester weak pointer is no longer valid"),
		Harvestable->IsHarvestAvailable());
	TestEqual(TEXT("Disconnect timeout advances the revision exactly once"), Harvestable->GetHarvestRevision(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
