#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace RpgInventoryIntentTransactionTests
{
	class FScopedInventoryWorld
	{
	public:
		FScopedInventoryWorld()
		{
			GameInstance =
				NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedInventoryWorld()
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

		bool IsValid() const
		{
			return GameInstance != nullptr && World != nullptr;
		}

		URpgInventoryManagerComponent* CreateInventory(
			const TCHAR* DebugName)
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				AActor::StaticClass(),
				FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			AActor* OwnerActor =
				World->SpawnActor<AActor>(SpawnParameters);
			if (!OwnerActor)
			{
				return nullptr;
			}

			URpgInventoryManagerComponent* Inventory =
				NewObject<URpgInventoryManagerComponent>(
					OwnerActor,
					MakeUniqueObjectName(
						OwnerActor,
						URpgInventoryManagerComponent::StaticClass(),
						TEXT("Inventory")),
					RF_Transient);
			OwnerActor->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	bool InitializeTest(
		FAutomationTestBase& Test,
		FScopedInventoryWorld& TestWorld)
	{
		if (!TestWorld.IsValid())
		{
			Test.AddError(
				TEXT("Could not create an isolated inventory intent world."));
			return false;
		}
		return true;
	}

	FRpgInventoryContainerHandle MakeRoot(
		const URpgInventoryManagerComponent* Inventory)
	{
		return Inventory
			? FRpgInventoryContainerHandle::MakeRoot(
				Inventory->GetDefaultContainerId())
			: FRpgInventoryContainerHandle();
	}

	FRpgInventoryGridPlacement MakePlacement(
		const FRpgInventoryContainerHandle& Container,
		int32 X,
		int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(Container);
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}

	bool GetEntryView(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId,
		FRpgInventoryEntryView& OutEntry)
	{
		if (!Inventory)
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry :
			Inventory->GetAllEntries())
		{
			if (Entry.ItemId == ItemId)
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	FString MakeInventorySignature(
		const URpgInventoryManagerComponent* Inventory)
	{
		TArray<FString> Rows;
		if (!Inventory)
		{
			return TEXT("<null>");
		}

		for (const FRpgInventoryEntryView& Entry :
			Inventory->GetAllEntries())
		{
			Rows.Add(FString::Printf(
				TEXT("%s|%s|%d|%s|%d|%d|%d|%d|%d"),
				*Entry.EntryId.ToString(),
				*Entry.ItemId.ToString(),
				Entry.StackCount,
				*Entry.Placement.GetContainerHandle().ToString(),
				Entry.Placement.X,
				Entry.Placement.Y,
				Entry.Placement.Width,
				Entry.Placement.Height,
				Entry.Placement.bRotated ? 1 : 0));
		}
		Rows.Sort();
		return FString::Join(Rows, TEXT(";"));
	}

	FRpgInventoryMoveIntent MakeMoveIntent(
		const FRpgInventoryEntryView& SourceEntry,
		const FRpgInventoryGridPlacement& TargetPlacement,
		const FGuid& RequestId = FGuid())
	{
		FRpgInventoryMoveIntent Intent;
		Intent.RequestId =
			RequestId.IsValid() ? RequestId : FGuid::NewGuid();
		Intent.ItemId = SourceEntry.ItemId;
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
		Intent.ExpectedQuantity = SourceEntry.StackCount;
		Intent.TargetPlacement = TargetPlacement;
		return Intent;
	}

	FRpgInventoryTransferIntent MakeTransferIntent(
		const FRpgInventoryEntryView& SourceEntry,
		const URpgInventoryManagerComponent* TargetInventory,
		int32 Quantity,
		const FGuid& RequestId = FGuid())
	{
		FRpgInventoryTransferIntent Intent;
		Intent.RequestId =
			RequestId.IsValid() ? RequestId : FGuid::NewGuid();
		Intent.ItemId = SourceEntry.ItemId;
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
		Intent.ExpectedSourceQuantity = SourceEntry.StackCount;
		Intent.TargetContainer = MakeRoot(TargetInventory);
		Intent.Quantity = Quantity;
		return Intent;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTypedMovePlannerContractTest,
	"SurvivalRpg.Inventory.Intent.Planner.MoveDerivationStaleSnapshotsAndPurity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTypedMovePlannerContractTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("TypedMovePlannerInventory"));
	if (!TestNotNull(TEXT("The move planner inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("The move planner item exists"), Item))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TestTrue(
			TEXT("The complete move planner snapshot resolves"),
			GetEntryView(
				Inventory,
				Item->GetItemId(),
				SourceEntry)))
	{
		return false;
	}

	const FString GraphBefore = MakeInventorySignature(Inventory);
	const int32 RevisionBefore = Inventory->GetInventoryRevision();
	const uint64 EpochBefore = Inventory->GetMutationEpoch();

	const FRpgInventoryMoveIntent NoOpIntent =
		MakeMoveIntent(SourceEntry, SourceEntry.Placement);
	const FRpgInventoryMutationResult NoOpPlan =
		Inventory->PlanMoveItem(NoOpIntent);
	TestEqual(
		TEXT("An identical same-cell placement derives Move semantics"),
		NoOpPlan.Operation,
		ERpgInventoryMutationOperation::Move);
	TestEqual(
		TEXT("An identical same-cell placement plans successfully"),
		NoOpPlan.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("A same-cell no-op covers the complete entry"),
		NoOpPlan.AppliedQuantity,
		SourceEntry.StackCount);
	TestTrue(
		TEXT("A same-cell no-op emits no mutation delta"),
		NoOpPlan.Deltas.IsEmpty());

	FRpgInventoryGridPlacement RotatedPlacement = SourceEntry.Placement;
	RotatedPlacement.bRotated = !RotatedPlacement.bRotated;
	const FRpgInventoryMoveIntent RotateIntent =
		MakeMoveIntent(SourceEntry, RotatedPlacement);
	const FRpgInventoryMutationResult RotatePlan =
		Inventory->PlanMoveItem(RotateIntent);
	TestEqual(
		TEXT("A same-cell orientation change derives Rotate semantics"),
		RotatePlan.Operation,
		ERpgInventoryMutationOperation::Rotate);
	TestEqual(
		TEXT("A valid same-cell rotation plans successfully"),
		RotatePlan.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("A rotation plan emits exactly one delta"),
		RotatePlan.Deltas.Num(),
		1);
	if (RotatePlan.Deltas.Num() == 1)
	{
		TestEqual(
			TEXT("The derived rotation delta has Rotated semantics"),
			RotatePlan.Deltas[0].Kind,
			ERpgInventoryMutationDeltaKind::Rotated);
		TestEqual(
			TEXT("The derived rotation preserves the requested orientation"),
			RotatePlan.Deltas[0].AfterPlacement.bRotated,
			RotatedPlacement.bRotated);
	}

	FRpgInventoryMoveIntent StaleEntryIntent =
		MakeMoveIntent(
			SourceEntry,
			MakePlacement(Root, 3, 0));
	StaleEntryIntent.ExpectedEntryId = FGuid::NewGuid();
	TestEqual(
		TEXT("Move planning rejects a stale entry identity"),
		Inventory->PlanMoveItem(StaleEntryIntent).Code,
		ERpgInventoryMutationResultCode::SourceMismatch);

	FRpgInventoryMoveIntent StalePlacementIntent =
		MakeMoveIntent(
			SourceEntry,
			MakePlacement(Root, 3, 0));
	++StalePlacementIntent.ExpectedSourcePlacement.X;
	TestEqual(
		TEXT("Move planning rejects a stale full source placement"),
		Inventory->PlanMoveItem(StalePlacementIntent).Code,
		ERpgInventoryMutationResultCode::SourceMismatch);

	FRpgInventoryMoveIntent StaleQuantityIntent =
		MakeMoveIntent(
			SourceEntry,
			MakePlacement(Root, 3, 0));
	++StaleQuantityIntent.ExpectedQuantity;
	TestEqual(
		TEXT("Move planning rejects a stale complete source quantity"),
		Inventory->PlanMoveItem(StaleQuantityIntent).Code,
		ERpgInventoryMutationResultCode::SourceMismatch);

	TestEqual(
		TEXT("All direct move plans preserve the complete graph"),
		MakeInventorySignature(Inventory),
		GraphBefore);
	TestEqual(
		TEXT("All direct move plans preserve the replicated revision"),
		Inventory->GetInventoryRevision(),
		RevisionBefore);
	TestEqual(
		TEXT("All direct move plans preserve the mutation epoch"),
		Inventory->GetMutationEpoch(),
		EpochBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryDropPlannerContractTest,
	"SurvivalRpg.Inventory.Intent.Planner.DropPartialStaleOversizedAndPurity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryDropPlannerContractTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("DropPlannerInventory"));
	if (!TestNotNull(TEXT("The drop planner inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			5,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("The drop planner stack exists"), Item))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TestTrue(
			TEXT("The complete drop planner snapshot resolves"),
			GetEntryView(
				Inventory,
				Item->GetItemId(),
				SourceEntry)))
	{
		return false;
	}

	const FString GraphBefore = MakeInventorySignature(Inventory);
	const int32 RevisionBefore = Inventory->GetInventoryRevision();
	const uint64 EpochBefore = Inventory->GetMutationEpoch();

	constexpr int32 PartialDropQuantity = 2;
	const FRpgInventoryTransferIntent PartialDropIntent =
		MakeTransferIntent(
			SourceEntry,
			nullptr,
			PartialDropQuantity);
	const FRpgInventoryMutationResult PartialDropPlan =
		Inventory->PlanDropItem(PartialDropIntent);
	TestEqual(
		TEXT("An ordinary partial drop retains Drop semantics"),
		PartialDropPlan.Operation,
		ERpgInventoryMutationOperation::Drop);
	TestEqual(
		TEXT("An ordinary partial drop plans successfully"),
		PartialDropPlan.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The partial drop plans the requested quantity"),
		PartialDropPlan.AppliedQuantity,
		PartialDropQuantity);
	TestEqual(
		TEXT("An ordinary partial drop emits one stack delta"),
		PartialDropPlan.Deltas.Num(),
		1);
	if (PartialDropPlan.Deltas.Num() == 1)
	{
		TestEqual(
			TEXT("The partial drop changes the source stack"),
			PartialDropPlan.Deltas[0].Kind,
			ERpgInventoryMutationDeltaKind::StackChanged);
		TestEqual(
			TEXT("The partial drop keeps the uncommitted remainder"),
			PartialDropPlan.Deltas[0].NewQuantity,
			SourceEntry.StackCount - PartialDropQuantity);
	}

	FRpgInventoryTransferIntent StaleDropIntent = PartialDropIntent;
	++StaleDropIntent.ExpectedSourceQuantity;
	TestEqual(
		TEXT("Drop planning rejects a stale complete source quantity"),
		Inventory->PlanDropItem(StaleDropIntent).Code,
		ERpgInventoryMutationResultCode::SourceMismatch);

	FRpgInventoryTransferIntent OversizedDropIntent = PartialDropIntent;
	OversizedDropIntent.Quantity = SourceEntry.StackCount + 1;
	const FRpgInventoryMutationResult OversizedDropPlan =
		Inventory->PlanDropItem(OversizedDropIntent);
	TestEqual(
		TEXT("Drop planning rejects more than the complete source stack"),
		OversizedDropPlan.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("An oversized drop applies no quantity"),
		OversizedDropPlan.AppliedQuantity,
		0);

	TestEqual(
		TEXT("All direct drop plans preserve the complete graph"),
		MakeInventorySignature(Inventory),
		GraphBefore);
	TestEqual(
		TEXT("All direct drop plans preserve the replicated revision"),
		Inventory->GetInventoryRevision(),
		RevisionBefore);
	TestEqual(
		TEXT("All direct drop plans preserve the mutation epoch"),
		Inventory->GetMutationEpoch(),
		EpochBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTypedMoveStaleSnapshotTest,
	"SurvivalRpg.Inventory.Intent.Move.StaleSnapshotIsAtomic",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTypedMoveStaleSnapshotTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("TypedMoveStaleInventory"));
	if (!TestNotNull(TEXT("The move inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("The move source exists"), Item))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TestTrue(
			TEXT("The authoritative move snapshot resolves"),
			GetEntryView(
				Inventory,
				Item->GetItemId(),
				SourceEntry)))
	{
		return false;
	}

	const FString BeforeRejections =
		MakeInventorySignature(Inventory);
	FRpgInventoryMoveIntent StaleEntryIntent = MakeMoveIntent(
		SourceEntry,
		MakePlacement(Root, 2, 0));
	StaleEntryIntent.ExpectedEntryId = FGuid::NewGuid();
	const FRpgInventoryMutationResult StaleEntryResult =
		Inventory->MoveItem(StaleEntryIntent);
	TestEqual(
		TEXT("A stale entry identity is rejected explicitly"),
		StaleEntryResult.Code,
		ERpgInventoryMutationResultCode::SourceMismatch);
	TestEqual(
		TEXT("A stale entry identity applies no quantity"),
		StaleEntryResult.AppliedQuantity,
		0);
	TestEqual(
		TEXT("A stale entry identity preserves the graph"),
		MakeInventorySignature(Inventory),
		BeforeRejections);

	FRpgInventoryMoveIntent StalePlacementIntent = MakeMoveIntent(
		SourceEntry,
		MakePlacement(Root, 2, 0));
	StalePlacementIntent.ExpectedSourcePlacement.X += 1;
	const FRpgInventoryMutationResult StalePlacementResult =
		Inventory->MoveItem(StalePlacementIntent);
	TestEqual(
		TEXT("A stale full source placement is rejected explicitly"),
		StalePlacementResult.Code,
		ERpgInventoryMutationResultCode::SourceMismatch);
	TestEqual(
		TEXT("A stale source placement applies no quantity"),
		StalePlacementResult.AppliedQuantity,
		0);
	TestEqual(
		TEXT("Every stale-snapshot rejection is atomic"),
		MakeInventorySignature(Inventory),
		BeforeRejections);

	FRpgInventoryMoveIntent StaleQuantityIntent = MakeMoveIntent(
		SourceEntry,
		MakePlacement(Root, 2, 0));
	++StaleQuantityIntent.ExpectedQuantity;
	const FRpgInventoryMutationResult StaleQuantityResult =
		Inventory->MoveItem(StaleQuantityIntent);
	TestEqual(
		TEXT("A stale complete source quantity is rejected explicitly"),
		StaleQuantityResult.Code,
		ERpgInventoryMutationResultCode::SourceMismatch);
	TestEqual(
		TEXT("A stale source quantity applies no quantity"),
		StaleQuantityResult.AppliedQuantity,
		0);
	TestEqual(
		TEXT("A stale source quantity preserves the graph"),
		MakeInventorySignature(Inventory),
		BeforeRejections);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTypedMoveDerivationTest,
	"SurvivalRpg.Inventory.Intent.Move.DerivesMergeAndSwap",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTypedMoveDerivationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("TypedMoveDerivationInventory"));
	if (!TestNotNull(TEXT("The derivation inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* MergeSource =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			3,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* MergeTarget =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4,
			MakePlacement(Root, 1, 0));
	if (!TestNotNull(TEXT("The merge source exists"), MergeSource) ||
		!TestNotNull(TEXT("The merge target exists"), MergeTarget))
	{
		return false;
	}

	const FRpgInventoryItemId MergeSourceId =
		MergeSource->GetItemId();
	const FRpgInventoryItemId MergeTargetId =
		MergeTarget->GetItemId();
	FRpgInventoryEntryView MergeSourceEntry;
	FRpgInventoryEntryView MergeTargetEntry;
	if (!GetEntryView(
			Inventory,
			MergeSourceId,
			MergeSourceEntry) ||
		!GetEntryView(
			Inventory,
			MergeTargetId,
			MergeTargetEntry))
	{
		AddError(TEXT("Could not resolve both merge entry snapshots."));
		return false;
	}

	const FRpgInventoryMoveIntent MergeIntent = MakeMoveIntent(
		MergeSourceEntry,
		MergeTargetEntry.Placement);
	const FString BeforeMergePlan =
		MakeInventorySignature(Inventory);
	const FRpgInventoryMutationResult MergePlan =
		Inventory->PlanMoveItem(MergeIntent);
	TestEqual(
		TEXT("A typed Move keeps its public operation semantic"),
		MergePlan.Operation,
		ERpgInventoryMutationOperation::Move);
	TestEqual(
		TEXT("Compatible overlap derives a successful full merge"),
		MergePlan.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("A full merge exposes source and target deltas"),
		MergePlan.Deltas.Num(),
		2);
	const FRpgInventoryMutationDelta* MergeSourceDelta =
		MergePlan.Deltas.FindByPredicate(
			[&MergeSourceId](
				const FRpgInventoryMutationDelta& Delta)
			{
				return Delta.ItemId == MergeSourceId;
			});
	const FRpgInventoryMutationDelta* MergeTargetDelta =
		MergePlan.Deltas.FindByPredicate(
			[&MergeTargetId](
				const FRpgInventoryMutationDelta& Delta)
			{
				return Delta.ItemId == MergeTargetId;
			});
	if (TestNotNull(
			TEXT("The merge plan contains the source delta"),
			MergeSourceDelta))
	{
		TestEqual(
			TEXT("The fully merged source is removed"),
			MergeSourceDelta->Kind,
			ERpgInventoryMutationDeltaKind::Removed);
	}
	if (TestNotNull(
			TEXT("The merge plan contains the target delta"),
			MergeTargetDelta))
	{
		TestEqual(
			TEXT("The compatible target receives a stack delta"),
			MergeTargetDelta->Kind,
			ERpgInventoryMutationDeltaKind::StackChanged);
	}
	TestEqual(
		TEXT("Typed move planning remains read-only"),
		MakeInventorySignature(Inventory),
		BeforeMergePlan);

	const FRpgInventoryMutationResult MergeCommit =
		Inventory->MoveItem(MergeIntent);
	TestEqual(
		TEXT("The derived merge commits"),
		MergeCommit.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The derived merge applies the complete source stack"),
		MergeCommit.AppliedQuantity,
		3);
	TestNull(
		TEXT("The fully merged source identity leaves the inventory"),
		Inventory->FindItemById(MergeSourceId));
	TestEqual(
		TEXT("The target contains the merged total"),
		Inventory->GetItemStackCount(MergeTarget),
		7);
	const FString AfterFullMerge =
		MakeInventorySignature(Inventory);
	const FRpgInventoryMutationResult FullMergeReplay =
		Inventory->MoveItem(MergeIntent);
	TestEqual(
		TEXT("A full-merge retry replays the original success"),
		FullMergeReplay.Code,
		MergeCommit.Code);
	TestEqual(
		TEXT("A full-merge retry preserves the original quantity"),
		FullMergeReplay.AppliedQuantity,
		MergeCommit.AppliedQuantity);
	TestEqual(
		TEXT("A full-merge retry cannot merge twice"),
		MakeInventorySignature(Inventory),
		AfterFullMerge);

	URpgInventoryManagerComponent* PartialInventory =
		TestWorld.CreateInventory(TEXT("TypedMovePartialMergeInventory"));
	if (!TestNotNull(
			TEXT("The partial-merge inventory exists"),
			PartialInventory))
	{
		return false;
	}
	const FRpgInventoryContainerHandle PartialRoot =
		MakeRoot(PartialInventory);
	URpgInventoryItemInstance* PartialSource =
		PartialInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			3,
			MakePlacement(PartialRoot, 0, 0));
	URpgInventoryItemInstance* PartialTarget =
		PartialInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			9,
			MakePlacement(PartialRoot, 1, 0));
	if (!TestNotNull(
			TEXT("The partial-merge source exists"),
			PartialSource) ||
		!TestNotNull(
			TEXT("The partial-merge target exists"),
			PartialTarget))
	{
		return false;
	}

	FRpgInventoryEntryView PartialSourceEntry;
	FRpgInventoryEntryView PartialTargetEntry;
	if (!GetEntryView(
			PartialInventory,
			PartialSource->GetItemId(),
			PartialSourceEntry) ||
		!GetEntryView(
			PartialInventory,
			PartialTarget->GetItemId(),
			PartialTargetEntry))
	{
		AddError(TEXT("Could not resolve partial-merge snapshots."));
		return false;
	}
	const FRpgInventoryMoveIntent PartialMergeIntent =
		MakeMoveIntent(
			PartialSourceEntry,
			PartialTargetEntry.Placement);
	const FString BeforePartialMerge =
		MakeInventorySignature(PartialInventory);
	const FRpgInventoryMutationResult PartialMergePlan =
		PartialInventory->PlanMoveItem(PartialMergeIntent);
	TestEqual(
		TEXT("The public whole-entry Move plan rejects limited target capacity"),
		PartialMergePlan.Code,
		ERpgInventoryMutationResultCode::StackLimitReached);
	TestEqual(
		TEXT("The rejected whole-entry Move plan applies no quantity"),
		PartialMergePlan.AppliedQuantity,
		0);
	TestEqual(
		TEXT("Partial move planning preserves the complete source entry"),
		MakeInventorySignature(PartialInventory),
		BeforePartialMerge);

	const FRpgInventoryMutationResult PartialMoveRejection =
		PartialInventory->MoveItem(PartialMergeIntent);
	TestEqual(
		TEXT("A whole-entry Move rejects a capacity-limited merge"),
		PartialMoveRejection.Code,
		ERpgInventoryMutationResultCode::StackLimitReached);
	TestEqual(
		TEXT("The rejected whole-entry Move applies no quantity"),
		PartialMoveRejection.AppliedQuantity,
		0);
	TestEqual(
		TEXT("The rejected whole-entry Move preserves both stacks"),
		MakeInventorySignature(PartialInventory),
		BeforePartialMerge);
	const FRpgInventoryMutationResult PartialMoveReplay =
		PartialInventory->MoveItem(PartialMergeIntent);
	TestEqual(
		TEXT("A rejected whole-entry Move retry replays its original result"),
		PartialMoveReplay.Code,
		PartialMoveRejection.Code);
	TestEqual(
		TEXT("A rejected whole-entry Move retry preserves zero application"),
		PartialMoveReplay.AppliedQuantity,
		PartialMoveRejection.AppliedQuantity);
	TestEqual(
		TEXT("A rejected whole-entry Move retry cannot consume the source"),
		MakeInventorySignature(PartialInventory),
		BeforePartialMerge);

	URpgInventoryItemInstance* SwapSource =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 2));
	URpgInventoryItemInstance* SwapTarget =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 1, 2));
	if (!TestNotNull(TEXT("The swap source exists"), SwapSource) ||
		!TestNotNull(TEXT("The swap target exists"), SwapTarget))
	{
		return false;
	}

	const FRpgInventoryItemId SwapSourceId =
		SwapSource->GetItemId();
	const FRpgInventoryItemId SwapTargetId =
		SwapTarget->GetItemId();
	FRpgInventoryEntryView SwapSourceEntry;
	FRpgInventoryEntryView SwapTargetEntry;
	if (!GetEntryView(
			Inventory,
			SwapSourceId,
			SwapSourceEntry) ||
		!GetEntryView(
			Inventory,
			SwapTargetId,
			SwapTargetEntry))
	{
		AddError(TEXT("Could not resolve both swap entry snapshots."));
		return false;
	}

	const FRpgInventoryMoveIntent SwapIntent = MakeMoveIntent(
		SwapSourceEntry,
		SwapTargetEntry.Placement);
	const FString BeforeSwapPlan =
		MakeInventorySignature(Inventory);
	const FRpgInventoryMutationResult SwapPlan =
		Inventory->PlanMoveItem(SwapIntent);
	TestEqual(
		TEXT("Unlike overlap derives a successful swap"),
		SwapPlan.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("A derived swap exposes both concrete moves"),
		SwapPlan.Deltas.Num(),
		2);
	for (const FRpgInventoryMutationDelta& Delta : SwapPlan.Deltas)
	{
		TestEqual(
			TEXT("Every derived swap delta is a move"),
			Delta.Kind,
			ERpgInventoryMutationDeltaKind::Moved);
	}
	TestEqual(
		TEXT("Typed swap planning remains read-only"),
		MakeInventorySignature(Inventory),
		BeforeSwapPlan);

	const FRpgInventoryMutationResult SwapCommit =
		Inventory->MoveItem(SwapIntent);
	TestEqual(
		TEXT("The derived swap commits atomically"),
		SwapCommit.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The derived swap applies the complete source entry"),
		SwapCommit.AppliedQuantity,
		1);

	FRpgInventoryEntryView MovedSwapSource;
	FRpgInventoryEntryView MovedSwapTarget;
	if (GetEntryView(
			Inventory,
			SwapSourceId,
			MovedSwapSource) &&
		GetEntryView(
			Inventory,
			SwapTargetId,
			MovedSwapTarget))
	{
		TestEqual(
			TEXT("The typed source reaches the requested target cell"),
			MovedSwapSource.Placement.X,
			SwapTargetEntry.Placement.X);
		TestEqual(
			TEXT("The displaced item returns to the source cell"),
			MovedSwapTarget.Placement.X,
			SwapSourceEntry.Placement.X);
		TestEqual(
			TEXT("The source row is preserved"),
			MovedSwapSource.Placement.Y,
			SwapTargetEntry.Placement.Y);
		TestEqual(
			TEXT("The target row is preserved"),
			MovedSwapTarget.Placement.Y,
			SwapSourceEntry.Placement.Y);
	}
	else
	{
		AddError(TEXT("Could not resolve the committed swap entries."));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryRejectedIntentFingerprintTest,
	"SurvivalRpg.Inventory.Intent.RejectedRequestFingerprintIsImmutable",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryRejectedIntentFingerprintTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("RejectedIntentSource"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("RejectedIntentTarget"));
	if (!TestNotNull(
			TEXT("The rejected-intent source exists"),
			SourceInventory) ||
		!TestNotNull(
			TEXT("The rejected-intent target exists"),
			TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle SourceRoot =
		MakeRoot(SourceInventory);
	URpgInventoryItemInstance* MoveItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(SourceRoot, 0, 0));
	URpgInventoryItemInstance* TransferItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(SourceRoot, 1, 0));
	if (!TestNotNull(
			TEXT("The rejected move item exists"),
			MoveItem) ||
		!TestNotNull(
			TEXT("The rejected transfer item exists"),
			TransferItem))
	{
		return false;
	}

	FRpgInventoryEntryView MoveEntry;
	FRpgInventoryEntryView TransferEntry;
	if (!GetEntryView(
			SourceInventory,
			MoveItem->GetItemId(),
			MoveEntry) ||
		!GetEntryView(
			SourceInventory,
			TransferItem->GetItemId(),
			TransferEntry))
	{
		AddError(TEXT("Could not resolve rejected-intent source snapshots."));
		return false;
	}

	const FString SourceBefore =
		MakeInventorySignature(SourceInventory);
	const FString TargetBefore =
		MakeInventorySignature(TargetInventory);

	FRpgInventoryMoveIntent InvalidMove =
		MakeMoveIntent(
			MoveEntry,
			MakePlacement(SourceRoot, 3, 0));
	InvalidMove.ExpectedEntryId.Invalidate();
	const FRpgInventoryMutationResult InvalidMoveResult =
		SourceInventory->MoveItem(InvalidMove);
	TestEqual(
		TEXT("A structurally invalid move is rejected"),
		InvalidMoveResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);

	FRpgInventoryMoveIntent ValidMove = InvalidMove;
	ValidMove.ExpectedEntryId = MoveEntry.EntryId;
	const FRpgInventoryMutationResult ChangedMoveResult =
		SourceInventory->MoveItem(ValidMove);
	TestEqual(
		TEXT("The rejected move request id cannot later gain a valid payload"),
		ChangedMoveResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("The rejected move command never mutates its source"),
		MakeInventorySignature(SourceInventory),
		SourceBefore);

	FRpgInventoryTransferIntent InvalidTransfer =
		MakeTransferIntent(
			TransferEntry,
			TargetInventory,
			1);
	InvalidTransfer.ExpectedEntryId.Invalidate();
	const FRpgInventoryMutationResult InvalidTransferResult =
		SourceInventory->TransferItem(
			TargetInventory,
			InvalidTransfer);
	TestEqual(
		TEXT("A structurally invalid transfer is rejected"),
		InvalidTransferResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);

	FRpgInventoryTransferIntent ValidTransfer = InvalidTransfer;
	ValidTransfer.ExpectedEntryId = TransferEntry.EntryId;
	const FRpgInventoryMutationResult ChangedTransferResult =
		SourceInventory->TransferItem(
			TargetInventory,
			ValidTransfer);
	TestEqual(
		TEXT("The rejected transfer request id cannot later gain a valid payload"),
		ChangedTransferResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("The rejected transfer command preserves its source"),
		MakeInventorySignature(SourceInventory),
		SourceBefore);
	TestEqual(
		TEXT("The rejected transfer command preserves its target"),
		MakeInventorySignature(TargetInventory),
		TargetBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTypedMoveReplayFingerprintTest,
	"SurvivalRpg.Inventory.Intent.Move.RequestFingerprintIsImmutable",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTypedMoveReplayFingerprintTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("TypedMoveReplayInventory"));
	if (!TestNotNull(TEXT("The replay inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("The replay item exists"), Item))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!GetEntryView(
			Inventory,
			Item->GetItemId(),
			SourceEntry))
	{
		AddError(TEXT("Could not resolve the replay source snapshot."));
		return false;
	}

	const FRpgInventoryMoveIntent Intent = MakeMoveIntent(
		SourceEntry,
		MakePlacement(Root, 2, 0));
	const FRpgInventoryMutationResult FirstCommit =
		Inventory->MoveItem(Intent);
	TestEqual(
		TEXT("The first immutable move request succeeds"),
		FirstCommit.Code,
		ERpgInventoryMutationResultCode::Success);
	const FString AfterFirstCommit =
		MakeInventorySignature(Inventory);

	const FRpgInventoryMutationResult Replay =
		Inventory->MoveItem(Intent);
	TestEqual(
		TEXT("An identical request id and payload replays success"),
		Replay.Code,
		FirstCommit.Code);
	TestEqual(
		TEXT("The replay preserves the original applied quantity"),
		Replay.AppliedQuantity,
		FirstCommit.AppliedQuantity);
	TestEqual(
		TEXT("The replay preserves the original delta count"),
		Replay.Deltas.Num(),
		FirstCommit.Deltas.Num());
	TestEqual(
		TEXT("The replay echoes the immutable correlation id"),
		Replay.RequestId,
		Intent.RequestId);
	TestEqual(
		TEXT("An identical local retry mutates nothing twice"),
		MakeInventorySignature(Inventory),
		AfterFirstCommit);

	FRpgInventoryMoveIntent ChangedPayload = Intent;
	ChangedPayload.TargetPlacement.X += 1;
	const FRpgInventoryMutationResult Collision =
		Inventory->MoveItem(ChangedPayload);
	TestEqual(
		TEXT("The same request id with a changed payload is rejected"),
		Collision.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("A local request-id collision applies no quantity"),
		Collision.AppliedQuantity,
		0);
	TestEqual(
		TEXT("A rejected local collision preserves committed state"),
		MakeInventorySignature(Inventory),
		AfterFirstCommit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryRestoreMutationEpochTest,
	"SurvivalRpg.Inventory.Intent.RestoreStartsNewMutationEpoch",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryRestoreMutationEpochTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("RestoreMutationEpochInventory"));
	if (!TestNotNull(TEXT("The restore-epoch inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	const uint64 InitialMutationEpoch =
		Inventory->GetMutationEpoch();
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("The restore-epoch item exists"), Item))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!GetEntryView(
			Inventory,
			Item->GetItemId(),
			SourceEntry))
	{
		AddError(TEXT("Could not resolve the restore-epoch source."));
		return false;
	}

	const FRpgInventoryGraphSaveData BeforeMove =
		Inventory->ExportInventoryGraph();
	const FRpgInventoryMoveIntent MoveIntent =
		MakeMoveIntent(
			SourceEntry,
			MakePlacement(Root, 2, 0));
	TestEqual(
		TEXT("The pre-restore command succeeds"),
		Inventory->MoveItem(MoveIntent).Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("Ordinary mutations stay inside the current command epoch"),
		Inventory->GetMutationEpoch(),
		InitialMutationEpoch);

	FRpgInventoryMutationResult RuntimeCheckpointResult;
	TestTrue(
		TEXT("A runtime checkpoint restore succeeds inside the current epoch"),
		Inventory->RestoreRuntimeCheckpoint(
			Inventory->ExportInventoryGraph(),
			RuntimeCheckpointResult));
	TestEqual(
		TEXT("A runtime checkpoint reports restore semantics"),
		RuntimeCheckpointResult.Operation,
		ERpgInventoryMutationOperation::Restore);
	TestEqual(
		TEXT("A runtime checkpoint restore does not invalidate request replay"),
		Inventory->GetMutationEpoch(),
		InitialMutationEpoch);

	FRpgInventoryMutationResult RestoreResult;
	TestTrue(
		TEXT("The older graph restores successfully"),
		Inventory->RestoreInventoryGraph(
			BeforeMove,
			RestoreResult));
	TestEqual(
		TEXT("A profile restore reports Restore semantics"),
		RestoreResult.Operation,
		ERpgInventoryMutationOperation::Restore);
	TestEqual(
		TEXT("A successful profile restore advances the command epoch exactly once"),
		Inventory->GetMutationEpoch(),
		InitialMutationEpoch + 1);

	FRpgInventoryEntryView RestoredEntry;
	if (!GetEntryView(
			Inventory,
			Item->GetItemId(),
			RestoredEntry))
	{
		AddError(TEXT("Could not resolve the restored item."));
		return false;
	}
	TestEqual(
		TEXT("The older graph returns the item to its source cell"),
		RestoredEntry.Placement.X,
		0);

	const FRpgInventoryMutationResult RetriedAfterRestore =
		Inventory->MoveItem(MoveIntent);
	TestEqual(
		TEXT("The same command id executes again in the new restore epoch"),
		RetriedAfterRestore.Code,
		ERpgInventoryMutationResultCode::Success);
	FRpgInventoryEntryView MovedAfterRestore;
	if (!GetEntryView(
			Inventory,
			Item->GetItemId(),
			MovedAfterRestore))
	{
		AddError(TEXT("Could not resolve the post-restore moved item."));
		return false;
	}
	TestEqual(
		TEXT("The post-restore retry reapplies the reverted mutation"),
		MovedAfterRestore.Placement.X,
		2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTypedTransferReplayFingerprintTest,
	"SurvivalRpg.Inventory.Intent.Transfer.RequestTargetAndPolicyAreImmutable",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTypedTransferReplayFingerprintTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("TypedTransferSource"));
	URpgInventoryManagerComponent* FirstTarget =
		TestWorld.CreateInventory(TEXT("TypedTransferFirstTarget"));
	URpgInventoryManagerComponent* OtherTarget =
		TestWorld.CreateInventory(TEXT("TypedTransferOtherTarget"));
	URpgInventoryManagerComponent* PolicyTarget =
		TestWorld.CreateInventory(TEXT("TypedTransferPolicyTarget"));
	if (!TestNotNull(
			TEXT("The transfer source exists"),
			SourceInventory) ||
		!TestNotNull(
			TEXT("The first transfer target exists"),
			FirstTarget) ||
		!TestNotNull(
			TEXT("The alternate transfer target exists"),
			OtherTarget) ||
		!TestNotNull(
			TEXT("The partial-policy target exists"),
			PolicyTarget))
	{
		return false;
	}

	const FRpgInventoryContainerHandle SourceRoot =
		MakeRoot(SourceInventory);
	URpgInventoryItemInstance* TransferStack =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			3,
			MakePlacement(SourceRoot, 0, 0));
	if (!TestNotNull(
			TEXT("The transfer stack exists"),
			TransferStack))
	{
		return false;
	}

	FRpgInventoryEntryView TransferEntry;
	if (!GetEntryView(
			SourceInventory,
			TransferStack->GetItemId(),
			TransferEntry))
	{
		AddError(TEXT("Could not resolve the transfer source snapshot."));
		return false;
	}

	FRpgInventoryTransferIntent StaleTransfer =
		MakeTransferIntent(TransferEntry, FirstTarget, 1);
	StaleTransfer.ExpectedSourcePlacement.X += 1;
	const FString SourceBeforeStale =
		MakeInventorySignature(SourceInventory);
	const FString TargetBeforeStale =
		MakeInventorySignature(FirstTarget);
	const FRpgInventoryMutationResult StaleResult =
		SourceInventory->TransferItem(
			FirstTarget,
			StaleTransfer);
	TestEqual(
		TEXT("A stale cross-inventory source snapshot is rejected"),
		StaleResult.Code,
		ERpgInventoryMutationResultCode::SourceMismatch);
	TestEqual(
		TEXT("A stale cross-inventory request preserves its source"),
		MakeInventorySignature(SourceInventory),
		SourceBeforeStale);
	TestEqual(
		TEXT("A stale cross-inventory request preserves its target"),
		MakeInventorySignature(FirstTarget),
		TargetBeforeStale);

	FRpgInventoryTransferIntent StaleQuantityTransfer =
		MakeTransferIntent(TransferEntry, FirstTarget, 1);
	++StaleQuantityTransfer.ExpectedSourceQuantity;
	const FRpgInventoryMutationResult StaleQuantityResult =
		SourceInventory->TransferItem(
			FirstTarget,
			StaleQuantityTransfer);
	TestEqual(
		TEXT("A changed complete source stack count rejects a partial transfer"),
		StaleQuantityResult.Code,
		ERpgInventoryMutationResultCode::SourceMismatch);
	TestEqual(
		TEXT("A stale source quantity preserves the complete source graph"),
		MakeInventorySignature(SourceInventory),
		SourceBeforeStale);
	TestEqual(
		TEXT("A stale source quantity preserves the complete target graph"),
		MakeInventorySignature(FirstTarget),
		TargetBeforeStale);

	const FRpgInventoryTransferIntent TransferIntent =
		MakeTransferIntent(TransferEntry, FirstTarget, 1);
	const FRpgInventoryMutationResult FirstTransfer =
		SourceInventory->TransferItem(
			FirstTarget,
			TransferIntent);
	TestEqual(
		TEXT("The first typed transfer succeeds"),
		FirstTransfer.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The typed transfer applies its exact quantity"),
		FirstTransfer.AppliedQuantity,
		1);

	const FString SourceAfterTransfer =
		MakeInventorySignature(SourceInventory);
	const FString FirstTargetAfterTransfer =
		MakeInventorySignature(FirstTarget);
	const FString OtherTargetBeforeCollision =
		MakeInventorySignature(OtherTarget);
	const FRpgInventoryMutationResult TransferReplay =
		SourceInventory->TransferItem(
			FirstTarget,
			TransferIntent);
	TestEqual(
		TEXT("An identical cross-inventory request replays success"),
		TransferReplay.Code,
		FirstTransfer.Code);
	TestEqual(
		TEXT("The cross replay preserves its applied quantity"),
		TransferReplay.AppliedQuantity,
		FirstTransfer.AppliedQuantity);
	TestEqual(
		TEXT("An identical cross retry does not mutate the source"),
		MakeInventorySignature(SourceInventory),
		SourceAfterTransfer);
	TestEqual(
		TEXT("An identical cross retry does not mutate the target"),
		MakeInventorySignature(FirstTarget),
		FirstTargetAfterTransfer);

	const FRpgInventoryMutationResult TargetCollision =
		SourceInventory->TransferItem(
			OtherTarget,
			TransferIntent);
	TestEqual(
		TEXT("The same request id cannot select another target inventory"),
		TargetCollision.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("A target collision leaves the source unchanged"),
		MakeInventorySignature(SourceInventory),
		SourceAfterTransfer);
	TestEqual(
		TEXT("A target collision leaves the first target unchanged"),
		MakeInventorySignature(FirstTarget),
		FirstTargetAfterTransfer);
	TestEqual(
		TEXT("A target collision leaves the alternate target unchanged"),
		MakeInventorySignature(OtherTarget),
		OtherTargetBeforeCollision);

	FRpgInventoryTransferIntent QuantityCollision = TransferIntent;
	QuantityCollision.Quantity = 2;
	const FRpgInventoryMutationResult PayloadCollision =
		SourceInventory->TransferItem(
			FirstTarget,
			QuantityCollision);
	TestEqual(
		TEXT("The same request id cannot change transfer quantity"),
		PayloadCollision.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("A cross payload collision preserves the source"),
		MakeInventorySignature(SourceInventory),
		SourceAfterTransfer);
	TestEqual(
		TEXT("A cross payload collision preserves the target"),
		MakeInventorySignature(FirstTarget),
		FirstTargetAfterTransfer);

	FRpgInventoryTransferIntent SourceQuantityCollision = TransferIntent;
	++SourceQuantityCollision.ExpectedSourceQuantity;
	const FRpgInventoryMutationResult SourceQuantityCollisionResult =
		SourceInventory->TransferItem(
			FirstTarget,
			SourceQuantityCollision);
	TestEqual(
		TEXT("The same request id cannot change its complete source quantity snapshot"),
		SourceQuantityCollisionResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("A source-quantity fingerprint collision preserves the source"),
		MakeInventorySignature(SourceInventory),
		SourceAfterTransfer);
	TestEqual(
		TEXT("A source-quantity fingerprint collision preserves the target"),
		MakeInventorySignature(FirstTarget),
		FirstTargetAfterTransfer);

	URpgInventoryItemInstance* PolicyItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(SourceRoot, 1, 0));
	if (!TestNotNull(
			TEXT("The partial-policy item exists"),
			PolicyItem))
	{
		return false;
	}

	FRpgInventoryEntryView PolicyEntry;
	if (!GetEntryView(
			SourceInventory,
			PolicyItem->GetItemId(),
			PolicyEntry))
	{
		AddError(TEXT("Could not resolve the partial-policy snapshot."));
		return false;
	}

	const FRpgInventoryTransferIntent PolicyIntent =
		MakeTransferIntent(PolicyEntry, PolicyTarget, 1);
	const FRpgInventoryMutationResult FirstPickup =
		SourceInventory->PickupItem(
			PolicyTarget,
			PolicyIntent,
			false);
	TestEqual(
		TEXT("The initial exact pickup succeeds"),
		FirstPickup.Code,
		ERpgInventoryMutationResultCode::Success);
	const FString SourceAfterPickup =
		MakeInventorySignature(SourceInventory);
	const FString PolicyTargetAfterPickup =
		MakeInventorySignature(PolicyTarget);

	const FRpgInventoryMutationResult PickupReplay =
		SourceInventory->PickupItem(
			PolicyTarget,
			PolicyIntent,
			false);
	TestEqual(
		TEXT("An identical pickup policy replays success"),
		PickupReplay.Code,
		FirstPickup.Code);
	TestEqual(
		TEXT("An identical pickup retry preserves the source"),
		MakeInventorySignature(SourceInventory),
		SourceAfterPickup);
	TestEqual(
		TEXT("An identical pickup retry preserves the target"),
		MakeInventorySignature(PolicyTarget),
		PolicyTargetAfterPickup);

	const FRpgInventoryMutationResult PolicyCollision =
		SourceInventory->PickupItem(
			PolicyTarget,
			PolicyIntent,
			true);
	TestEqual(
		TEXT("The same request id cannot change partial-stack policy"),
		PolicyCollision.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("A partial-policy collision applies no quantity"),
		PolicyCollision.AppliedQuantity,
		0);
	TestEqual(
		TEXT("A partial-policy collision preserves the source"),
		MakeInventorySignature(SourceInventory),
		SourceAfterPickup);
	TestEqual(
		TEXT("A partial-policy collision preserves the target"),
		MakeInventorySignature(PolicyTarget),
		PolicyTargetAfterPickup);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryAuthorityAndLegacyFacadeRevisionContractTest,
	"SurvivalRpg.Inventory.Transaction.AuthorityAndLegacyFacadeRevisionContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryAuthorityAndLegacyFacadeRevisionContractTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("AuthorityLegacyFacadeInventory"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("AuthorityLegacyFacadeTarget"));
	if (!TestNotNull(TEXT("The authority-test inventory exists"), Inventory) ||
		!TestNotNull(
			TEXT("The authority-test transfer target exists"),
			TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* FirstItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 7, 0));
	URpgInventoryItemInstance* SecondItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 8, 0));
	if (!TestNotNull(TEXT("The first legacy-wrapper item exists"), FirstItem) ||
		!TestNotNull(TEXT("The second legacy-wrapper item exists"), SecondItem))
	{
		return false;
	}

	const uint64 MutationEpochBeforeLegacyOperations =
		Inventory->GetMutationEpoch();
	int32 ExpectedRevision = Inventory->GetInventoryRevision();
	const FString BeforeSort = MakeInventorySignature(Inventory);
	TestTrue(
		TEXT("The authority-only legacy sort repacks a displaced inventory"),
		Inventory->ApplyInventorySort(ERpgInventorySortMode::Name));
	++ExpectedRevision;
	TestEqual(
		TEXT("A changed legacy sort advances revision exactly once"),
		Inventory->GetInventoryRevision(),
		ExpectedRevision);
	TestEqual(
		TEXT("A runtime legacy sort preserves the mutation epoch"),
		Inventory->GetMutationEpoch(),
		MutationEpochBeforeLegacyOperations);
	TestNotEqual(
		TEXT("The changed legacy sort rewrites placement state"),
		MakeInventorySignature(Inventory),
		BeforeSort);

	const FString StableSortedGraph = MakeInventorySignature(Inventory);
	TestFalse(
		TEXT("Repeating the same legacy sort reports no mutation"),
		Inventory->ApplyInventorySort(ERpgInventorySortMode::Name));
	TestEqual(
		TEXT("A no-op legacy sort does not advance revision"),
		Inventory->GetInventoryRevision(),
		ExpectedRevision);
	TestEqual(
		TEXT("A no-op legacy sort preserves the graph"),
		MakeInventorySignature(Inventory),
		StableSortedGraph);

	const TArray<FRpgInventoryEntryView> SortedEntries =
		Inventory->GetAllEntries();
	if (!TestEqual(
			TEXT("The legacy-order fixture retains both entries"),
			SortedEntries.Num(),
			2) ||
		!SortedEntries.IsValidIndex(0))
	{
		return false;
	}

	const FGuid ReorderedEntryId = SortedEntries[0].EntryId;
	const FRpgInventoryItemId ReorderedItemId = SortedEntries[0].ItemId;
	const FString BeforeIndexMove = MakeInventorySignature(Inventory);
	TestTrue(
		TEXT("The authority-only legacy index move changes shared order"),
		Inventory->MoveInventoryEntry(
			ReorderedEntryId,
			SortedEntries.Num() - 1));
	++ExpectedRevision;
	TestEqual(
		TEXT("A changed legacy index move advances revision exactly once"),
		Inventory->GetInventoryRevision(),
		ExpectedRevision);
	TestEqual(
		TEXT("A legacy index move preserves the mutation epoch"),
		Inventory->GetMutationEpoch(),
		MutationEpochBeforeLegacyOperations);
	TestNotEqual(
		TEXT("The changed legacy index move rewrites placement order"),
		MakeInventorySignature(Inventory),
		BeforeIndexMove);

	const FString StableIndexGraph = MakeInventorySignature(Inventory);
	TestFalse(
		TEXT("Moving the same entry to its current final index is a no-op"),
		Inventory->MoveInventoryEntry(
			ReorderedEntryId,
			SortedEntries.Num() - 1));
	TestEqual(
		TEXT("A no-op legacy index move does not advance revision"),
		Inventory->GetInventoryRevision(),
		ExpectedRevision);
	TestEqual(
		TEXT("A no-op legacy index move preserves the graph"),
		MakeInventorySignature(Inventory),
		StableIndexGraph);

	const FRpgInventoryGridPlacement ExactLegacyPlacement =
		MakePlacement(Root, 5, 2);
	TestTrue(
		TEXT("The authority-only legacy placement wrapper commits"),
		Inventory->MoveInventoryEntryToPlacement(
			ReorderedEntryId,
			ExactLegacyPlacement));
	++ExpectedRevision;
	TestEqual(
		TEXT("A changed legacy placement advances revision exactly once"),
		Inventory->GetInventoryRevision(),
		ExpectedRevision);
	TestEqual(
		TEXT("A legacy placement move preserves the mutation epoch"),
		Inventory->GetMutationEpoch(),
		MutationEpochBeforeLegacyOperations);

	FRpgInventoryEntryView AuthorityEntry;
	if (!TestTrue(
			TEXT("The legacy-moved entry remains addressable"),
			GetEntryView(
				Inventory,
				ReorderedItemId,
				AuthorityEntry)))
	{
		return false;
	}
	TestEqual(
		TEXT("The legacy placement wrapper reaches the requested X coordinate"),
		AuthorityEntry.Placement.X,
		ExactLegacyPlacement.X);
	TestEqual(
		TEXT("The legacy placement wrapper reaches the requested Y coordinate"),
		AuthorityEntry.Placement.Y,
		ExactLegacyPlacement.Y);

	const FString StablePlacementGraph =
		MakeInventorySignature(Inventory);
	TestTrue(
		TEXT("Repeating an exact legacy placement remains a successful no-op"),
		Inventory->MoveInventoryEntryToPlacement(
			ReorderedEntryId,
			ExactLegacyPlacement));
	TestEqual(
		TEXT("A no-op legacy placement does not advance revision"),
		Inventory->GetInventoryRevision(),
		ExpectedRevision);
	TestEqual(
		TEXT("A no-op legacy placement preserves the graph"),
		MakeInventorySignature(Inventory),
		StablePlacementGraph);

	AActor* InventoryOwner = Inventory->GetOwner();
	if (!TestNotNull(
			TEXT("The authority-test inventory owns an actor"),
			InventoryOwner))
	{
		return false;
	}

	const FString SourceBeforeAuthorityRejections =
		MakeInventorySignature(Inventory);
	const FString TargetBeforeAuthorityRejections =
		MakeInventorySignature(TargetInventory);
	const int32 SourceRevisionBeforeAuthorityRejections =
		Inventory->GetInventoryRevision();
	const int32 TargetRevisionBeforeAuthorityRejections =
		TargetInventory->GetInventoryRevision();
	const uint64 SourceEpochBeforeAuthorityRejections =
		Inventory->GetMutationEpoch();
	const uint64 TargetEpochBeforeAuthorityRejections =
		TargetInventory->GetMutationEpoch();

	InventoryOwner->SetRole(ROLE_SimulatedProxy);
	TestFalse(
		TEXT("The mutation owner no longer has authority"),
		InventoryOwner->HasAuthority());
	TestEqual(
		TEXT("The mutation owner now behaves as a simulated proxy"),
		static_cast<int32>(InventoryOwner->GetLocalRole()),
		static_cast<int32>(ROLE_SimulatedProxy));

	const FRpgInventoryMutationResult MoveResult =
		Inventory->MoveItem(MakeMoveIntent(
			AuthorityEntry,
			MakePlacement(Root, 6, 2)));
	TestEqual(
		TEXT("A simulated proxy cannot commit a typed move"),
		MoveResult.Code,
		ERpgInventoryMutationResultCode::AuthorityRequired);

	const FRpgInventoryMutationResult EquipmentMoveResult =
		Inventory->MoveEquipmentItem(MakeMoveIntent(
			AuthorityEntry,
			MakePlacement(Root, 7, 2)));
	TestEqual(
		TEXT("A simulated proxy cannot commit a trusted equipment move"),
		EquipmentMoveResult.Code,
		ERpgInventoryMutationResultCode::AuthorityRequired);

	FRpgInventoryMutationRequest GenericMoveRequest;
	GenericMoveRequest.RequestId = FGuid::NewGuid();
	GenericMoveRequest.Operation = ERpgInventoryMutationOperation::Move;
	GenericMoveRequest.ItemId = AuthorityEntry.ItemId;
	GenericMoveRequest.ExpectedEntryId = AuthorityEntry.EntryId;
	GenericMoveRequest.Source =
		AuthorityEntry.Placement.GetContainerHandle();
	GenericMoveRequest.ExpectedSourcePlacement =
		AuthorityEntry.Placement;
	GenericMoveRequest.ExpectedSourceQuantity =
		AuthorityEntry.StackCount;
	GenericMoveRequest.Target = Root;
	GenericMoveRequest.TargetPlacement =
		MakePlacement(Root, 8, 2);
	GenericMoveRequest.Quantity = AuthorityEntry.StackCount;
	const FRpgInventoryMutationResult GenericMoveResult =
		Inventory->ExecuteInventoryMutation(GenericMoveRequest);
	TestEqual(
		TEXT("A simulated proxy cannot execute the generic mutation kernel"),
		GenericMoveResult.Code,
		ERpgInventoryMutationResultCode::AuthorityRequired);

	const FRpgInventoryMutationResult TransferResult =
		Inventory->TransferItem(
			TargetInventory,
			MakeTransferIntent(
				AuthorityEntry,
				TargetInventory,
				AuthorityEntry.StackCount));
	TestEqual(
		TEXT("A simulated source cannot execute a typed transfer"),
		TransferResult.Code,
		ERpgInventoryMutationResultCode::AuthorityRequired);

	TestFalse(
		TEXT("A simulated proxy cannot invoke the legacy sort wrapper"),
		Inventory->ApplyInventorySort(ERpgInventorySortMode::Name));
	TestFalse(
		TEXT("A simulated proxy cannot invoke the legacy index wrapper"),
		Inventory->MoveInventoryEntry(ReorderedEntryId, 0));
	TestFalse(
		TEXT("A simulated proxy cannot invoke the legacy placement wrapper"),
		Inventory->MoveInventoryEntryToPlacement(
			ReorderedEntryId,
			MakePlacement(Root, 9, 2)));

	TestEqual(
		TEXT("Every authority rejection preserves the complete source graph"),
		MakeInventorySignature(Inventory),
		SourceBeforeAuthorityRejections);
	TestEqual(
		TEXT("Every authority rejection preserves the complete target graph"),
		MakeInventorySignature(TargetInventory),
		TargetBeforeAuthorityRejections);
	TestEqual(
		TEXT("Authority rejection does not advance source revision"),
		Inventory->GetInventoryRevision(),
		SourceRevisionBeforeAuthorityRejections);
	TestEqual(
		TEXT("Authority rejection does not advance target revision"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBeforeAuthorityRejections);
	TestEqual(
		TEXT("Authority rejection preserves the source command epoch"),
		Inventory->GetMutationEpoch(),
		SourceEpochBeforeAuthorityRejections);
	TestEqual(
		TEXT("Authority rejection preserves the target command epoch"),
		TargetInventory->GetMutationEpoch(),
		TargetEpochBeforeAuthorityRejections);

	InventoryOwner->SetRole(ROLE_Authority);
	TestTrue(
		TEXT("The test restores authority before world teardown"),
		InventoryOwner->HasAuthority());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryForeignWorldTargetAtomicityTest,
	"SurvivalRpg.Inventory.Intent.Transfer.ForeignWorldTargetIsRejectedAtomically",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryForeignWorldTargetAtomicityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryIntentTransactionTests;
	FScopedInventoryWorld SourceWorld;
	FScopedInventoryWorld TargetWorld;
	if (!InitializeTest(*this, SourceWorld) ||
		!InitializeTest(*this, TargetWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		SourceWorld.CreateInventory(TEXT("ForeignWorldTransferSource"));
	URpgInventoryManagerComponent* TargetInventory =
		TargetWorld.CreateInventory(TEXT("ForeignWorldTransferTarget"));
	if (!TestNotNull(
			TEXT("The foreign-world source inventory exists"),
			SourceInventory) ||
		!TestNotNull(
			TEXT("The foreign-world target inventory exists"),
			TargetInventory))
	{
		return false;
	}

	TestNotEqual(
		TEXT("The transfer fixtures belong to different worlds"),
		SourceInventory->GetWorld(),
		TargetInventory->GetWorld());

	const FRpgInventoryContainerHandle SourceRoot =
		MakeRoot(SourceInventory);
	URpgInventoryItemInstance* SourceItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(SourceRoot, 0, 0));
	if (!TestNotNull(
		TEXT("The foreign-world transfer source item exists"),
		SourceItem))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntry;
	if (!TestTrue(
			TEXT("The foreign-world source snapshot resolves"),
			GetEntryView(
				SourceInventory,
				SourceItem->GetItemId(),
				SourceEntry)))
	{
		return false;
	}

	const FRpgInventoryTransferIntent Intent =
		MakeTransferIntent(
			SourceEntry,
			TargetInventory,
			SourceEntry.StackCount);
	const FString SourceBefore =
		MakeInventorySignature(SourceInventory);
	const FString TargetBefore =
		MakeInventorySignature(TargetInventory);
	const int32 SourceRevisionBefore =
		SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore =
		TargetInventory->GetInventoryRevision();
	const uint64 SourceEpochBefore =
		SourceInventory->GetMutationEpoch();
	const uint64 TargetEpochBefore =
		TargetInventory->GetMutationEpoch();

	const FRpgInventoryMutationResult Result =
		SourceInventory->TransferItem(
			TargetInventory,
			Intent);
	TestEqual(
		TEXT("A foreign-world transfer target is rejected"),
		Result.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("The foreign-world rejection preserves request correlation"),
		Result.RequestId,
		Intent.RequestId);

	const FRpgInventoryMutationResult Replay =
		SourceInventory->TransferItem(
			TargetInventory,
			Intent);
	TestEqual(
		TEXT("An identical foreign-world retry replays its rejection"),
		Replay.Code,
		Result.Code);
	TestEqual(
		TEXT("The foreign-world rejection and replay preserve the source graph"),
		MakeInventorySignature(SourceInventory),
		SourceBefore);
	TestEqual(
		TEXT("The foreign-world rejection and replay preserve the target graph"),
		MakeInventorySignature(TargetInventory),
		TargetBefore);
	TestEqual(
		TEXT("A foreign-world rejection does not advance source revision"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore);
	TestEqual(
		TEXT("A foreign-world rejection does not advance target revision"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBefore);
	TestEqual(
		TEXT("A foreign-world rejection preserves the source command epoch"),
		SourceInventory->GetMutationEpoch(),
		SourceEpochBefore);
	TestEqual(
		TEXT("A foreign-world rejection preserves the target command epoch"),
		TargetInventory->GetMutationEpoch(),
		TargetEpochBefore);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
