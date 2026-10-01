#include "RpgInventoryManagerComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "RpgInventoryAutomationTestTypes.h"
#include "RpgInventoryContainerActor.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryFragment_StorageProfile.h"
#include "RpgInventoryItemInstance.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace RpgPhysicalBatchTests
{
	class FWorld
	{
	public:
		FWorld()
		{
			Instance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
		}
		~FWorld()
		{
			Instance->Shutdown();
			if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
			Instance->RemoveFromRoot();
		}
		ARpgInventoryContainerActor* Chest()
		{
			ARpgInventoryContainerActor* Actor = World ? World->SpawnActor<ARpgInventoryContainerActor>() : nullptr;
			if (Actor) { Actor->GetContainerComponent()->EnsurePersistentContainerId(); }
			return Actor;
		}
	private:
		UGameInstance* Instance = nullptr;
		UWorld* World = nullptr;
	};

	FRpgInventoryBatchOperation Consume(URpgInventoryManagerComponent* Inventory, URpgInventoryItemInstance* Item, int32 Quantity)
	{
		FRpgInventoryBatchOperation Operation;
		Operation.SourceInventory = Inventory;
		Operation.ItemId = Item->GetItemId();
		Operation.Quantity = Quantity;
		Operation.ExpectedSourceRevision = Inventory->GetInventoryRevision();
		return Operation;
	}

	FRpgInventoryBatchOperation Grant(URpgInventoryManagerComponent* Inventory, TSubclassOf<URpgInventoryItemDefinition> Definition, int32 Quantity)
	{
		FRpgInventoryBatchOperation Operation;
		Operation.TargetInventory = Inventory;
		Operation.ItemDefinition = Definition;
		Operation.Quantity = Quantity;
		return Operation;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalBatchAtomicTest, "SurvivalRpg.Inventory.PhysicalStorage.BatchAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalBatchAtomicTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalBatchTests;
	FWorld Scope;
	ARpgInventoryContainerActor* ActorA = Scope.Chest();
	ARpgInventoryContainerActor* ActorB = Scope.Chest();
	ARpgInventoryContainerActor* ActorOutput = Scope.Chest();
	if (!TestNotNull(TEXT("A"), ActorA) || !TestNotNull(TEXT("B"), ActorB) || !TestNotNull(TEXT("Output"), ActorOutput)) { return false; }
	URpgInventoryManagerComponent* A = ActorA->GetInventoryManager();
	URpgInventoryManagerComponent* B = ActorB->GetInventoryManager();
	URpgInventoryManagerComponent* Output = ActorOutput->GetInventoryManager();
	const auto Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	URpgInventoryItemInstance* ItemA = A->GrantItemDefinition(Material, 3);
	URpgInventoryItemInstance* ItemB = B->GrantItemDefinition(Material, 4);
	if (!TestNotNull(TEXT("Item A"), ItemA) || !TestNotNull(TEXT("Item B"), ItemB)) { return false; }
	FRpgInventoryGridSize OneCell;
	TestTrue(TEXT("Constrain output"), Output->SetDefaultGridSize(OneCell));
	TArray<FRpgInventoryBatchOperation> Operations = { Consume(A, ItemA, 3), Consume(B, ItemB, 2),
		Grant(Output, URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 2) };
	const int32 RevisionA = A->GetInventoryRevision();
	const int32 RevisionB = B->GetInventoryRevision();
	const FGuid Request = FGuid::NewGuid();
	TestEqual(TEXT("Late output overflow rejects whole batch"), A->ApplyInventoryBatch(Operations, Request).Code, ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(TEXT("A unchanged after rejected output"), A->GetItemStackCount(ItemA), 3);
	TestEqual(TEXT("B unchanged after rejected output"), B->GetItemStackCount(ItemB), 4);
	TestEqual(TEXT("No partial first output"), Output->GetAllItems().Num(), 0);
	TestEqual(TEXT("A revision unchanged"), A->GetInventoryRevision(), RevisionA);
	TestEqual(TEXT("B revision unchanged"), B->GetInventoryRevision(), RevisionB);
	Operations.Last().Quantity = 1;
	TestEqual(TEXT("Caller context changed during staging rejects atomically"),
		A->ApplyInventoryBatch(Operations, FGuid::NewGuid(), {}, {}, [] { return false; }).Code,
		ERpgInventoryMutationResultCode::SourceMismatch);
	TestEqual(TEXT("Context rejection retains both sources"), A->GetItemStackCount(ItemA) + B->GetItemStackCount(ItemB), 7);
	TestEqual(TEXT("Context rejection publishes no output"), Output->GetAllItems().Num(), 0);
	int32 Notifications = 0;
	bool bSawFinalState = true;
	bool bRejectedReentrantMutation = false;
	const FDelegateHandle Handle = A->OnInventoryPostCommit.AddLambda([&](URpgInventoryManagerComponent*)
	{
		++Notifications;
		bSawFinalState &= A->GetTotalItemCountByDefinition(Material) == 0 && B->GetItemStackCount(ItemB) == 2 && Output->GetAllItems().Num() == 1;
		bRejectedReentrantMutation = B->GrantItemDefinition(Material, 1) == nullptr;
	});
	const FGuid SuccessfulRequest = FGuid::NewGuid();
	TestEqual(TEXT("Three-inventory commit"), A->ApplyInventoryBatch(Operations, SuccessfulRequest).Code, ERpgInventoryMutationResultCode::Success);
	TestTrue(TEXT("First listener sees all final graphs"), bSawFinalState);
	TestTrue(TEXT("Notification cannot nest another participant mutation"), bRejectedReentrantMutation);
	TestEqual(TEXT("One notification"), Notifications, 1);
	TestEqual(TEXT("Exact replay succeeds"), A->ApplyInventoryBatch(Operations, SuccessfulRequest).Code, ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Replay does not republish"), Notifications, 1);
	Operations.Last().Quantity = 2;
	TestEqual(TEXT("Changed replay rejected"), A->ApplyInventoryBatch(Operations, SuccessfulRequest).Code, ERpgInventoryMutationResultCode::InvalidRequest);
	A->OnInventoryPostCommit.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalBatchIdentityTest, "SurvivalRpg.Inventory.PhysicalStorage.InstanceIdentityAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalBatchIdentityTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalBatchTests;
	FWorld Scope;
	ARpgInventoryContainerActor* ActorA = Scope.Chest();
	ARpgInventoryContainerActor* ActorB = Scope.Chest();
	if (!ActorA || !ActorB) { return false; }
	URpgInventoryManagerComponent* A = ActorA->GetInventoryManager();
	URpgInventoryManagerComponent* B = ActorB->GetInventoryManager();
	URpgInventoryItemInstance* Source = A->GrantItemDefinition(URpgInventoryAutomationTestStatefulMaterialDefinition::StaticClass(), 4);
	if (!TestNotNull(TEXT("Stateful source"), Source)) { return false; }
	const auto* Fragment = Source->FindFragmentByClass<URpgInventoryAutomationTestStatefulFragment>();
	if (!TestNotNull(TEXT("State fragment"), Fragment)) { return false; }
	Fragment->SetTestValue(Source, 73);
	const FRpgInventoryItemId Id = Source->GetItemId();
	FRpgInventoryBatchOperation Transfer = Consume(A, Source, 4);
	Transfer.TargetInventory = B;
	ERpgInventoryMutationResultCode Code;
	TestTrue(TEXT("Read-only preflight"), A->CanApplyInventoryBatch({ Transfer }, Code));
	TestEqual(TEXT("Preflight retains source"), A->GetItemStackCount(Source), 4);
	TestEqual(TEXT("Transfer commit"), A->ApplyInventoryBatch({ Transfer }, FGuid::NewGuid()).Code, ERpgInventoryMutationResultCode::Success);
	URpgInventoryItemInstance* Arrived = B->FindItemById(Id);
	TestNotNull(TEXT("Whole transfer retains item identity"), Arrived);
	if (Arrived) { TestEqual(TEXT("Opaque runtime payload retained"), static_cast<int32>(Fragment->GetTestValue(Arrived)), 73); }
	FRpgInventoryBatchCapacityChange Capacity;
	Capacity.Inventory = B;
	Capacity.NewGridSize = B->GetDefaultGridSize();
	Capacity.NewGridSize.Height += 2;
	Capacity.ExpectedRevision = B->GetInventoryRevision();
	bool bCallbackSawFinalCapacity = false;
	TestEqual(TEXT("Capacity and tier share transaction"), A->ApplyInventoryBatch({}, FGuid::NewGuid(), { Capacity }, [&]
	{
		bCallbackSawFinalCapacity = B->GetDefaultGridSize() == Capacity.NewGridSize;
		ActorB->GetContainerComponent()->SetUpgradeTier(1);
	}).Code, ERpgInventoryMutationResultCode::Success);
	TestTrue(TEXT("Metadata callback sees final grid"), bCallbackSawFinalCapacity);
	TestEqual(TEXT("Tier updated"), ActorB->GetContainerComponent()->ExportPhysicalStorageMetadata().UpgradeTier, 1);
	TestEqual(TEXT("Stale grid request rejected"), A->ApplyInventoryBatch({}, FGuid::NewGuid(), { Capacity }).Code, ERpgInventoryMutationResultCode::InvalidRequest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageAssignmentTest, "SurvivalRpg.Inventory.PhysicalStorage.AssignmentPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageAssignmentTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalBatchTests;
	FWorld Scope;
	ARpgInventoryContainerActor* ActorA = Scope.Chest();
	ARpgInventoryContainerActor* ActorB = Scope.Chest();
	if (!ActorA || !ActorB) { return false; }
	URpgInventoryContainerComponent* A = ActorA->GetContainerComponent();
	URpgInventoryContainerComponent* B = ActorB->GetContainerComponent();
	FRpgStorageAssignment Rule;
	Rule.ItemDefinition = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	Rule.AssignmentOrder = 999999; // Client-authored priority must never be accepted.
	if (!TestTrue(TEXT("Assign first chest"), A->SetAssignments({ Rule })) ||
		!TestTrue(TEXT("Assign second chest"), B->SetAssignments({ Rule })) ||
		!TestEqual(TEXT("First chest has one rule"), A->GetAssignments().Num(), 1) ||
		!TestEqual(TEXT("Second chest has one rule"), B->GetAssignments().Num(), 1)) { return false; }
	const int64 FirstOrder = A->GetAssignments()[0].AssignmentOrder;
	const int64 SecondOrder = B->GetAssignments()[0].AssignmentOrder;
	TestTrue(TEXT("Cross-chest order is monotonic"), FirstOrder > 0 && SecondOrder > FirstOrder);
	TestTrue(TEXT("Untrusted requested priority ignored"), FirstOrder != Rule.AssignmentOrder);
	int64 Order = 0;
	TestEqual(TEXT("Empty assigned chest remains target"), A->GetAssignmentRank(Rule.ItemDefinition, Order), 0);
	const FRpgPhysicalStorageMetadata Saved = A->ExportPhysicalStorageMetadata();
	TestTrue(TEXT("Remove all assignments"), A->SetAssignments({}));
	TestEqual(TEXT("Empty general chest has no destination"), A->GetAssignmentRank(Rule.ItemDefinition, Order), INDEX_NONE);
	if (!TestTrue(TEXT("Restore assignment state"), A->RestorePhysicalStorageMetadata(Saved)) ||
		!TestEqual(TEXT("Restored chest has one rule"), A->GetAssignments().Num(), 1)) { return false; }
	TestEqual(TEXT("Restored creation priority"), A->GetAssignments()[0].AssignmentOrder, FirstOrder);
	TestTrue(TEXT("Delete before reassign"), A->SetAssignments({}));
	if (!TestTrue(TEXT("Reassign"), A->SetAssignments({ Rule })) ||
		!TestEqual(TEXT("Reassigned chest has one rule"), A->GetAssignments().Num(), 1)) { return false; }
	TestTrue(TEXT("Reassignment follows later chest"), A->GetAssignments()[0].AssignmentOrder > SecondOrder);
	URpgInventoryFragment_StorageProfile* Profile = NewObject<URpgInventoryFragment_StorageProfile>();
	Profile->StorageMode = ERpgInventoryStorageMode::GridItem;
	Profile->StorageDomainTag = RpgGameplayTags::Storage_Domain_Materials;
	Profile->bCanAutoDeposit = true;
	Profile->bCanCraftFromNetwork = true;
	TestTrue(TEXT("Physical grid materials support automation"), Profile->CanAutoDepositPhysical());
	TestTrue(TEXT("Physical grid materials support crafting"), Profile->CanCraftFromPhysicalStorage());
	TestFalse(TEXT("Physical materials never opt into legacy ledger"), Profile->CanDepositAsBulk());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageRestoreBoundsTest, "SurvivalRpg.Inventory.PhysicalStorage.SavedGridPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageRestoreBoundsTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalBatchTests;
	FWorld Scope;
	ARpgInventoryContainerActor* Actor = Scope.Chest();
	if (!TestNotNull(TEXT("Chest"), Actor)) { return false; }
	URpgInventoryManagerComponent* Inventory = Actor->GetInventoryManager();
	URpgInventoryItemInstance* Original = Inventory->GrantItemDefinition(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1);
	if (!TestNotNull(TEXT("Original item"), Original)) { return false; }
	const FRpgInventoryGridSize LiveGrid = Inventory->GetDefaultGridSize();
	const int32 Revision = Inventory->GetInventoryRevision();
	const uint64 Epoch = Inventory->GetMutationEpoch();
	FRpgInventoryGraphSaveData Saved = Inventory->ExportInventoryGraph();
	if (!TestEqual(TEXT("One saved item"), Saved.Items.Num(), 1)) { return false; }
	Saved.Items[0].Placement.X = LiveGrid.Width;
	FRpgInventoryGridSize SavedGrid = LiveGrid;
	++SavedGrid.Width;
	int32 Notifications = 0;
	const FDelegateHandle Handle = Inventory->OnInventoryPostCommit.AddLambda([&](URpgInventoryManagerComponent*) { ++Notifications; });
	FRpgInventoryMutationResult Result;
	TestFalse(TEXT("Current grid rejects saved item beyond its bounds"), Inventory->ValidateInventoryGraphForRestore(Saved, Result));
	TestTrue(TEXT("Saved larger grid validates without live expansion"), Inventory->ValidateInventoryGraphForRestore(Saved, Result, &SavedGrid));
	SavedGrid.Width = 1;
	SavedGrid.Height = 1;
	Saved.Items[0].Placement.X = 1;
	TestFalse(TEXT("Saved smaller grid rejects a row that fits the live grid"), Inventory->ValidateInventoryGraphForRestore(Saved, Result, &SavedGrid));
	TestTrue(TEXT("Preflight retains live grid"), Inventory->GetDefaultGridSize() == LiveGrid);
	TestEqual(TEXT("Preflight retains revision"), Inventory->GetInventoryRevision(), Revision);
	TestEqual(TEXT("Preflight retains request epoch"), Inventory->GetMutationEpoch(), Epoch);
	TestEqual(TEXT("Preflight preserves original instance"), Inventory->FindItemById(Original->GetItemId()), Original);
	FRpgInventoryGridPlacement Placement;
	TestTrue(TEXT("Original placement remains available"), Inventory->GetItemPlacement(Original, Placement));
	TestEqual(TEXT("Preflight never moves the live item"), Placement.X, 0);
	TestEqual(TEXT("Preflight emits no commit notifications"), Notifications, 0);
	Inventory->OnInventoryPostCommit.Remove(Handle);
	return true;
}

#endif
