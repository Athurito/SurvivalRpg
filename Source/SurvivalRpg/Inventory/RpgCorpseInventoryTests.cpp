#include "RpgInventoryContainerActor.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "IPickupable.h"
#include "Misc/AutomationTest.h"
#include "RpgInventoryAutomationTestTypes.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"

namespace RpgCorpseInventoryTests
{
	class FScopedWorld
	{
	public:
		FScopedWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedWorld()
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

	AActor* SpawnActor(UWorld* World, const TCHAR* Name)
	{
		FActorSpawnParameters Parameters;
		Parameters.Name = MakeUniqueObjectName(World, AActor::StaticClass(), Name);
		Parameters.ObjectFlags = RF_Transient;
		Parameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<AActor>(Parameters);
	}

	URpgInventoryManagerComponent* AddInventory(AActor* Owner, const TCHAR* Name)
	{
		URpgInventoryManagerComponent* Inventory =
			NewObject<URpgInventoryManagerComponent>(Owner, Name, RF_Transient);
		Owner->AddInstanceComponent(Inventory);
		Inventory->RegisterComponent();
		return Inventory;
	}

	FRpgInventoryTransferIntent MakeTransferIntent(
		const FRpgInventoryEntryView& SourceEntry,
		const URpgInventoryManagerComponent* TargetInventory)
	{
		FRpgInventoryTransferIntent Intent;
		Intent.RequestId = FGuid::NewGuid();
		Intent.ItemId = SourceEntry.ItemId;
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
		Intent.ExpectedSourceQuantity = SourceEntry.StackCount;
		Intent.TargetContainer = FRpgInventoryContainerHandle::MakeRoot(
			TargetInventory->GetDefaultContainerId());
		Intent.Quantity = SourceEntry.StackCount;
		return Intent;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPostCommitDelegateTest,
	"SurvivalRpg.Inventory.Corpse.PostCommitSeesCompleteCrossInventoryGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPostCommitDelegateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RpgCorpseInventoryTests::FScopedWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("The post-commit test world exists"), World))
	{
		return false;
	}

	AActor* SourceOwner = RpgCorpseInventoryTests::SpawnActor(
		World,
		TEXT("PostCommitSource"));
	AActor* TargetOwner = RpgCorpseInventoryTests::SpawnActor(
		World,
		TEXT("PostCommitTarget"));
	URpgInventoryManagerComponent* Source =
		RpgCorpseInventoryTests::AddInventory(
			SourceOwner,
			TEXT("SourceInventory"));
	URpgInventoryManagerComponent* Target =
		RpgCorpseInventoryTests::AddInventory(
			TargetOwner,
			TEXT("TargetInventory"));
	URpgInventoryItemInstance* Item = Source->GrantItemDefinition(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1);
	if (!TestNotNull(TEXT("The transfer fixture item exists"), Item))
	{
		return false;
	}

	const TArray<FRpgInventoryEntryView> Entries = Source->GetAllEntries();
	if (!TestEqual(TEXT("The source exposes one complete entry"), Entries.Num(), 1))
	{
		return false;
	}
	const FRpgInventoryTransferIntent Intent =
		RpgCorpseInventoryTests::MakeTransferIntent(Entries[0], Target);

	int32 SourceBroadcasts = 0;
	int32 TargetBroadcasts = 0;
	bool bSourceSawCompleteGraph = false;
	bool bTargetSawCompleteGraph = false;
	bool bAttemptedReplayInsidePostCommit = false;
	bool bReplayInsidePostCommitSucceeded = false;
	Source->OnInventoryPostCommit.AddLambda(
		[&](URpgInventoryManagerComponent* Inventory)
		{
			++SourceBroadcasts;
			bSourceSawCompleteGraph = Inventory == Source &&
				Source->GetUsedEntryCount() == 0 &&
				Target->GetUsedEntryCount() == 1;
			if (!bAttemptedReplayInsidePostCommit)
			{
				bAttemptedReplayInsidePostCommit = true;
				bReplayInsidePostCommitSucceeded =
					Source->TransferItem(Target, Intent).IsSuccess();
			}
		});
	Target->OnInventoryPostCommit.AddLambda(
		[&](URpgInventoryManagerComponent* Inventory)
		{
			++TargetBroadcasts;
			bTargetSawCompleteGraph = Inventory == Target &&
				Source->GetUsedEntryCount() == 0 &&
				Target->GetUsedEntryCount() == 1;
		});

	const FRpgInventoryMutationResult Result = Source->TransferItem(Target, Intent);
	TestTrue(TEXT("The authoritative cross-inventory transfer succeeds"), Result.IsSuccess());
	TestEqual(TEXT("The source broadcasts once after commit"), SourceBroadcasts, 1);
	TestEqual(TEXT("The target broadcasts once after commit"), TargetBroadcasts, 1);
	TestTrue(TEXT("The source listener observes both final graphs"), bSourceSawCompleteGraph);
	TestTrue(TEXT("The target listener observes both final graphs"), bTargetSawCompleteGraph);
	TestTrue(
		TEXT("The source listener can replay the already-finalized command cache"),
		bAttemptedReplayInsidePostCommit && bReplayInsidePostCommitSucceeded);

	const FRpgInventoryMutationResult Replay = Source->TransferItem(Target, Intent);
	TestTrue(TEXT("The immutable transfer replay returns its cached success"), Replay.IsSuccess());
	TestEqual(TEXT("A cached replay does not rebroadcast the source"), SourceBroadcasts, 1);
	TestEqual(TEXT("A cached replay does not rebroadcast the target"), TargetBroadcasts, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPostCommitBatchOutputsTest,
	"SurvivalRpg.Inventory.Corpse.PostCommitSeesFinalizedBatchOutputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPostCommitBatchOutputsTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	RpgCorpseInventoryTests::FScopedWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("The batch-output test world exists"), World))
	{
		return false;
	}

	AActor* Owner = RpgCorpseInventoryTests::SpawnActor(
		World,
		TEXT("PostCommitBatchOwner"));
	URpgInventoryManagerComponent* Inventory =
		RpgCorpseInventoryTests::AddInventory(
			Owner,
			TEXT("BatchInventory"));
	if (!TestNotNull(TEXT("The batch inventory exists"), Inventory))
	{
		return false;
	}

	FInventoryPickup Pickup;
	FPickupTemplate& Template = Pickup.Templates.AddDefaulted_GetRef();
	Template.ItemDef =
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	Template.StackCount = 1;
	TArray<FRpgInventoryItemId> AffectedItemIds;
	int32 PostCommitBroadcasts = 0;
	bool bSawFinalizedOutputs = false;
	Inventory->OnInventoryPostCommit.AddLambda(
		[&](URpgInventoryManagerComponent* CommittedInventory)
		{
			++PostCommitBroadcasts;
			bSawFinalizedOutputs = CommittedInventory == Inventory &&
				AffectedItemIds.Num() == 1 &&
				AffectedItemIds[0].IsValid() &&
				Inventory->FindItemById(AffectedItemIds[0]) != nullptr;
		});

	const FRpgInventoryMutationResult Result =
		Inventory->AddPickupBatch(Pickup, AffectedItemIds);
	TestTrue(TEXT("The pickup batch commits"), Result.IsSuccess());
	TestEqual(TEXT("The batch broadcasts post-commit once"), PostCommitBroadcasts, 1);
	TestTrue(
		TEXT("Post-commit observes finalized affected-item outputs and graph"),
		bSawFinalizedOutputs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCorpseContainerTransferPolicyTest,
	"SurvivalRpg.Inventory.Corpse.WithdrawOnlyPolicyAndAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCorpseContainerTransferPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RpgCorpseInventoryTests::FScopedWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("The corpse-container test world exists"), World))
	{
		return false;
	}

	FActorSpawnParameters CorpseParameters;
	CorpseParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryContainerActor::StaticClass(),
		TEXT("WithdrawOnlyCorpse"));
	CorpseParameters.ObjectFlags = RF_Transient;
	CorpseParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryContainerActor* Corpse =
		World->SpawnActor<ARpgInventoryContainerActor>(CorpseParameters);
	AActor* PlayerOwner = RpgCorpseInventoryTests::SpawnActor(
		World,
		TEXT("WithdrawOnlyPlayer"));
	URpgInventoryManagerComponent* PlayerInventory =
		RpgCorpseInventoryTests::AddInventory(
			PlayerOwner,
			TEXT("PlayerInventory"));
	if (!TestNotNull(TEXT("The corpse actor exists"), Corpse) ||
		!TestNotNull(TEXT("The player inventory exists"), PlayerInventory))
	{
		return false;
	}

	URpgInventoryManagerComponent* CorpseInventory = Corpse->GetInventoryManager();
	URpgInventoryContainerComponent* Container = Corpse->GetContainerComponent();
	if (!TestNotNull(TEXT("The corpse inventory exists"), CorpseInventory) ||
		!TestNotNull(TEXT("The corpse container exists"), Container))
	{
		return false;
	}
	Container->ConfigureAsDeathLootContainer();
	Corpse->SetNetDormancy(DORM_Initial);
	Container->SetContainerAccessible(true);
	TestTrue(
		TEXT("A replicated access-state change flushes initial dormancy"),
		Corpse->NetDormancy == DORM_DormantAll);
	Container->SetInteractionRadius(100.0f);
	TestEqual(
		TEXT("Death loot uses the withdrawal-only contract"),
		Container->GetTransferPolicy(),
		ERpgInventoryContainerTransferPolicy::WithdrawOnly);
	TestFalse(
		TEXT("Death loot is not exposed as a crafting-resource source"),
		Container->AllowsCraftingAccess());

	USceneComponent* Anchor = NewObject<USceneComponent>(
		Corpse,
		TEXT("CorpseTestAnchor"),
		RF_Transient);
	Corpse->AddInstanceComponent(Anchor);
	Anchor->SetupAttachment(Corpse->GetRootComponent());
	Anchor->RegisterComponent();
	Anchor->SetWorldLocation(FVector(500.0f, 0.0f, 0.0f));
	Container->SetInteractionAnchor(Anchor);
	USceneComponent* PlayerRoot = NewObject<USceneComponent>(
		PlayerOwner,
		TEXT("PlayerTestRoot"),
		RF_Transient);
	PlayerOwner->AddInstanceComponent(PlayerRoot);
	PlayerOwner->SetRootComponent(PlayerRoot);
	PlayerRoot->RegisterComponent();
	PlayerOwner->SetActorLocation(FVector(550.0f, 0.0f, 0.0f));
	TestTrue(
		TEXT("Access range is evaluated from the explicit corpse anchor"),
		Container->CanActorAccess(PlayerOwner));
	TestTrue(
		TEXT("The prompt world location follows the explicit corpse anchor"),
		Container->GetInteractionWorldLocation().Equals(
			FVector(500.0f, 0.0f, 0.0f)));

	URpgInventoryItemInstance* DepositItem =
		PlayerInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	URpgInventoryItemInstance* LootItem =
		CorpseInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	if (!TestNotNull(TEXT("The candidate deposit exists"), DepositItem) ||
		!TestNotNull(TEXT("The corpse loot exists"), LootItem))
	{
		return false;
	}

	const FRpgInventoryEntryView DepositEntry =
		PlayerInventory->GetAllEntries()[0];
	const FRpgInventoryMutationResult DepositResult =
		PlayerInventory->TransferItem(
			CorpseInventory,
			RpgCorpseInventoryTests::MakeTransferIntent(
				DepositEntry,
				CorpseInventory));
	TestEqual(
		TEXT("The authoritative kernel rejects a deposit into death loot"),
		DepositResult.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	TestNotNull(
		TEXT("Rejected deposit remains in the source inventory"),
		PlayerInventory->FindItemById(DepositItem->GetItemId()));

	TArray<FRpgInventoryItemId> AffectedIds;
	TArray<FRpgInventoryContainerHandle> CorpseTargets;
	CorpseTargets.Add(FRpgInventoryContainerHandle::MakeRoot(
		CorpseInventory->GetDefaultContainerId()));
	const FRpgInventoryMutationResult BatchDepositResult =
		PlayerInventory->CollectRootItemsBatch(
			CorpseInventory,
			CorpseTargets,
			FGuid::NewGuid(),
			AffectedIds);
	TestEqual(
		TEXT("The root-batch path also rejects corpse deposits"),
		BatchDepositResult.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);

	FRpgInventoryEntryView LootEntry = CorpseInventory->GetAllEntries()[0];
	FRpgInventoryMoveIntent InternalMove;
	InternalMove.RequestId = FGuid::NewGuid();
	InternalMove.ItemId = LootEntry.ItemId;
	InternalMove.ExpectedEntryId = LootEntry.EntryId;
	InternalMove.ExpectedSourcePlacement = LootEntry.Placement;
	InternalMove.ExpectedQuantity = LootEntry.StackCount;
	InternalMove.TargetPlacement = LootEntry.Placement;
	InternalMove.TargetPlacement.X = 1;
	const FRpgInventoryMutationResult MoveResult =
		CorpseInventory->MoveItem(InternalMove);
	TestTrue(
		TEXT("Withdrawal-only still permits an internal corpse reordering"),
		MoveResult.IsSuccess());

	LootEntry = CorpseInventory->GetAllEntries()[0];
	const FRpgInventoryMutationResult WithdrawalResult =
		CorpseInventory->TransferItem(
			PlayerInventory,
			RpgCorpseInventoryTests::MakeTransferIntent(
				LootEntry,
				PlayerInventory));
	TestTrue(
		TEXT("Withdrawing corpse loot into the player inventory remains allowed"),
		WithdrawalResult.IsSuccess());
	TestEqual(
		TEXT("The corpse inventory is empty after the withdrawal"),
		CorpseInventory->GetUsedEntryCount(),
		0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
