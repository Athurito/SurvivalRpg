#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Misc/AutomationTest.h"

namespace RpgInventoryTransferDeltaTests
{
	const FName StorageContainerId(TEXT("Storage"));
	const FName BagContainerId(TEXT("Main"));

	class FScopedInventoryWorld
	{
	public:
		FScopedInventoryWorld()
		{
			GameInstance = NewObject<UGameInstance>(
				GEngine,
				NAME_None,
				RF_Transient);
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

		UWorld* GetWorld() const
		{
			return World;
		}

		AActor* CreateOwner(const TCHAR* DebugName)
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
			return World->SpawnActor<AActor>(SpawnParameters);
		}

		URpgInventoryManagerComponent* CreateInventory(
			const TCHAR* DebugName)
		{
			return CreateInventoryOnOwner(
				CreateOwner(DebugName),
				TEXT("Inventory"));
		}

		URpgInventoryManagerComponent* CreateInventoryOnOwner(
			AActor* Owner,
			const TCHAR* ComponentName)
		{
			if (!Owner)
			{
				return nullptr;
			}

			URpgInventoryManagerComponent* Inventory =
				NewObject<URpgInventoryManagerComponent>(
					Owner,
					MakeUniqueObjectName(
						Owner,
						URpgInventoryManagerComponent::StaticClass(),
						FName(ComponentName)),
					RF_Transient);
			Owner->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	FRpgInventoryContainerHandle MakeStorageHandle()
	{
		return FRpgInventoryContainerHandle::MakeRoot(StorageContainerId);
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

	bool FindEntry(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId,
		FRpgInventoryEntryView& OutEntry)
	{
		if (!Inventory)
		{
			return false;
		}
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.ItemId == ItemId)
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	FRpgInventoryMutationRequest MakeTransferRequest(
		const URpgInventoryManagerComponent* SourceInventory,
		const URpgInventoryItemInstance* Item,
		ERpgInventoryMutationOperation Operation,
		const FRpgInventoryContainerHandle& TargetContainer,
		const FRpgInventoryGridPlacement* ExactPlacement = nullptr)
	{
		FRpgInventoryMutationRequest Request;
		FRpgInventoryEntryView SourceEntry;
		if (!SourceInventory || !Item ||
			!FindEntry(SourceInventory, Item->GetItemId(), SourceEntry))
		{
			return Request;
		}

		Request.Operation = Operation;
		Request.ItemId = SourceEntry.ItemId;
		Request.ExpectedEntryId = SourceEntry.EntryId;
		Request.Source = SourceEntry.Placement.GetContainerHandle();
		Request.ExpectedSourcePlacement = SourceEntry.Placement;
		Request.ExpectedSourceQuantity = SourceEntry.StackCount;
		Request.Target = TargetContainer;
		Request.Quantity = SourceEntry.StackCount;
		Request.RequestId = FGuid::NewGuid();
		if (ExactPlacement)
		{
			Request.TargetPlacement = *ExactPlacement;
		}
		return Request;
	}

	int32 CountDefinitionUnits(
		const URpgInventoryManagerComponent* Inventory,
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		int32 Total = 0;
		if (!Inventory)
		{
			return Total;
		}
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.Instance &&
				Entry.Instance->GetItemDef() == ItemDefinition)
			{
				Total += Entry.StackCount;
			}
		}
		return Total;
	}

	FString MakeStrictSignature(
		const URpgInventoryManagerComponent* Inventory)
	{
		TArray<FString> Rows;
		if (!Inventory)
		{
			return TEXT("InvalidInventory");
		}
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			Rows.Add(FString::Printf(
				TEXT("%s|%s|%p|%d|%s|%d|%d|%d|%d|%d"),
				*Entry.EntryId.ToString(),
				*Entry.ItemId.ToString(),
				static_cast<const void*>(Entry.Instance.Get()),
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

	bool HasStableIdentity(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryEntryView& Before)
	{
		FRpgInventoryEntryView After;
		return FindEntry(Inventory, Before.ItemId, After) &&
			After.Instance == Before.Instance &&
			After.EntryId == Before.EntryId &&
			After.StackCount == Before.StackCount &&
			After.Placement == Before.Placement;
	}

	bool InitializeTest(
		FAutomationTestBase& Test,
		FScopedInventoryWorld& TestWorld)
	{
		if (!TestWorld.IsValid())
		{
			Test.AddError(
				TEXT("Could not create an isolated inventory transfer test world."));
			return false;
		}
		return true;
	}

	FGameplayTag GetInventoryChangedChannel()
	{
		return FGameplayTag::RequestGameplayTag(
			TEXT("Rpg.Inventory.Message.StackChanged"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTransferDeltaAtomicCallbacksTest,
	"SurvivalRpg.Inventory.TransferDelta.AtomicMergePlaceCallbacksAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTransferDeltaAtomicCallbacksTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransferDeltaTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("TransferDeltaSource"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("TransferDeltaTarget"));
	if (!TestNotNull(TEXT("Source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("Target inventory exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* SourceStack =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			5,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* SourceSentinel =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	URpgInventoryItemInstance* TargetStack =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			8,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* TargetSentinel =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	if (!TestNotNull(TEXT("Source stack exists"), SourceStack) ||
		!TestNotNull(TEXT("Source sentinel exists"), SourceSentinel) ||
		!TestNotNull(TEXT("Target merge stack exists"), TargetStack) ||
		!TestNotNull(TEXT("Target sentinel exists"), TargetSentinel))
	{
		return false;
	}

	FRpgInventoryEntryView SourceSentinelBefore;
	FRpgInventoryEntryView TargetSentinelBefore;
	if (!TestTrue(
			TEXT("Source sentinel has a stable entry"),
			FindEntry(
				SourceInventory,
				SourceSentinel->GetItemId(),
				SourceSentinelBefore)) ||
		!TestTrue(
			TEXT("Target sentinel has a stable entry"),
			FindEntry(
				TargetInventory,
				TargetSentinel->GetItemId(),
				TargetSentinelBefore)))
	{
		return false;
	}

	const FRpgInventoryItemId SourceItemId = SourceStack->GetItemId();
	const int32 SourceRevisionBefore =
		SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore =
		TargetInventory->GetInventoryRevision();
	const uint64 SourceEpochBefore = SourceInventory->GetMutationEpoch();
	const uint64 TargetEpochBefore = TargetInventory->GetMutationEpoch();
	const FRpgInventoryMutationRequest Request = MakeTransferRequest(
		SourceInventory,
		SourceStack,
		ERpgInventoryMutationOperation::Transfer,
		Root);

	int32 MessageCount = 0;
	bool bEveryCallbackSawFinalGraphs = true;
	bool bSentinelWasNotified = false;
	bool bIssuedReentrantRetry = false;
	FRpgInventoryMutationResult ReentrantResult;
	auto IsFinalStateVisible = [&]()
	{
		return SourceInventory->FindItemById(SourceItemId) == nullptr &&
			SourceInventory->GetUsedEntryCount() == 1 &&
			TargetInventory->GetItemStackCount(TargetStack) == 10 &&
			CountDefinitionUnits(
				TargetInventory,
				URpgInventoryAutomationTestStackItemDefinition::StaticClass()) ==
				13 &&
			TargetInventory->GetUsedEntryCount() == 3 &&
			HasStableIdentity(SourceInventory, SourceSentinelBefore) &&
			HasStableIdentity(TargetInventory, TargetSentinelBefore) &&
			SourceInventory->GetInventoryRevision() ==
				SourceRevisionBefore + 1 &&
			TargetInventory->GetInventoryRevision() ==
				TargetRevisionBefore + 1;
	};

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				++MessageCount;
				bEveryCallbackSawFinalGraphs &= IsFinalStateVisible();
				bSentinelWasNotified |=
					Message.Instance == SourceSentinel ||
					Message.Instance == TargetSentinel;
				if (!bIssuedReentrantRetry)
				{
					bIssuedReentrantRetry = true;
					ReentrantResult =
						SourceInventory->ExecuteCrossInventoryTransfer(
							TargetInventory,
							Request,
							false);
				}
			});

	const FRpgInventoryMutationResult TransferResult =
		SourceInventory->ExecuteCrossInventoryTransfer(
			TargetInventory,
			Request,
			false);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestEqual(
		TEXT("Merge plus placement succeeds"),
		TransferResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The full source stack is applied"),
		TransferResult.AppliedQuantity,
		5);
	TestEqual(
		TEXT("Merge, placement, and source removal produce three deltas"),
		TransferResult.Deltas.Num(),
		3);
	TestEqual(
		TEXT("Only the three changed rows emit messages"),
		MessageCount,
		3);
	TestTrue(
		TEXT("Every callback observes both final inventory graphs"),
		bEveryCallbackSawFinalGraphs);
	TestFalse(
		TEXT("Unrelated sentinel rows emit no transfer message"),
		bSentinelWasNotified);
	TestTrue(
		TEXT("The first callback issued a reentrant retry"),
		bIssuedReentrantRetry);
	TestEqual(
		TEXT("The reentrant retry replays cached success"),
		ReentrantResult.Code,
		TransferResult.Code);
	TestEqual(
		TEXT("The reentrant retry replays the same applied quantity"),
		ReentrantResult.AppliedQuantity,
		TransferResult.AppliedQuantity);
	TestEqual(
		TEXT("The source revision advances exactly once"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore + 1);
	TestEqual(
		TEXT("The target revision advances exactly once"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBefore + 1);
	TestEqual(
		TEXT("Runtime transfer does not change the source command epoch"),
		SourceInventory->GetMutationEpoch(),
		SourceEpochBefore);
	TestEqual(
		TEXT("Runtime transfer does not change the target command epoch"),
		TargetInventory->GetMutationEpoch(),
		TargetEpochBefore);
	TestTrue(
		TEXT("The source sentinel keeps UObject and EntryId identity"),
		HasStableIdentity(SourceInventory, SourceSentinelBefore));
	TestTrue(
		TEXT("The target sentinel keeps UObject and EntryId identity"),
		HasStableIdentity(TargetInventory, TargetSentinelBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTransferDeltaRejectedPartialTest,
	"SurvivalRpg.Inventory.TransferDelta.RejectedLegacyPartialTransferIsSideEffectFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTransferDeltaRejectedPartialTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransferDeltaTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("RejectedTransferSource"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("RejectedTransferTarget"));
	if (!TestNotNull(TEXT("Source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("Target inventory exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* SourceStack =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			5,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* SourceSentinel =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	URpgInventoryItemInstance* TargetStack =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			8,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("Source stack exists"), SourceStack) ||
		!TestNotNull(TEXT("Source sentinel exists"), SourceSentinel) ||
		!TestNotNull(TEXT("Target merge stack exists"), TargetStack))
	{
		return false;
	}

	bool bFilledTarget = true;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			if (X == 0 && Y == 0)
			{
				continue;
			}
			bFilledTarget &=
				TargetInventory->AddItemDefinitionToPlacement(
					URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
					1,
					MakePlacement(Root, X, Y)) != nullptr;
		}
	}
	if (!TestTrue(TEXT("The target grid is completely occupied"), bFilledTarget))
	{
		return false;
	}

	const FString SourceBefore = MakeStrictSignature(SourceInventory);
	const FString TargetBefore = MakeStrictSignature(TargetInventory);
	const int32 SourceRevisionBefore =
		SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore =
		TargetInventory->GetInventoryRevision();
	const uint64 SourceEpochBefore = SourceInventory->GetMutationEpoch();
	const uint64 TargetEpochBefore = TargetInventory->GetMutationEpoch();
	const FRpgInventoryMutationRequest Request = MakeTransferRequest(
		SourceInventory,
		SourceStack,
		ERpgInventoryMutationOperation::Transfer,
		Root);

	int32 MessageCount = 0;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&MessageCount](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				++MessageCount;
			});

	// The legacy bool must never enable partial Transfer or Drop semantics.
	const FRpgInventoryMutationResult Rejected =
		SourceInventory->ExecuteCrossInventoryTransfer(
			TargetInventory,
			Request,
			true);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestEqual(
		TEXT("A partial-fit Transfer rejects even when the legacy bool is true"),
		Rejected.Code,
		ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(TEXT("The rejected transfer applies nothing"), Rejected.AppliedQuantity, 0);
	TestTrue(TEXT("The rejected transfer exposes no deltas"), Rejected.Deltas.IsEmpty());
	TestEqual(TEXT("The rejected transfer emits no messages"), MessageCount, 0);
	TestEqual(
		TEXT("The complete source graph remains byte-for-byte stable"),
		MakeStrictSignature(SourceInventory),
		SourceBefore);
	TestEqual(
		TEXT("The complete target graph remains byte-for-byte stable"),
		MakeStrictSignature(TargetInventory),
		TargetBefore);
	TestEqual(
		TEXT("The source revision does not advance on rejection"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore);
	TestEqual(
		TEXT("The target revision does not advance on rejection"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBefore);
	TestEqual(
		TEXT("The source epoch does not change on rejection"),
		SourceInventory->GetMutationEpoch(),
		SourceEpochBefore);
	TestEqual(
		TEXT("The target epoch does not change on rejection"),
		TargetInventory->GetMutationEpoch(),
		TargetEpochBefore);
	TestEqual(
		TEXT("The compatible target stack remains unmodified"),
		TargetInventory->GetItemStackCount(TargetStack),
		8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTransferDeltaSameActorIdentityTest,
	"SurvivalRpg.Inventory.TransferDelta.SameActorFullTransferReusesInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTransferDeltaSameActorIdentityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransferDeltaTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	AActor* SharedOwner = TestWorld.CreateOwner(TEXT("SharedTransferOwner"));
	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventoryOnOwner(SharedOwner, TEXT("SourceInventory"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventoryOnOwner(SharedOwner, TEXT("TargetInventory"));
	if (!TestNotNull(TEXT("Shared owner exists"), SharedOwner) ||
		!TestNotNull(TEXT("Source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("Target inventory exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* MovingItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* SourceSentinel =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	URpgInventoryItemInstance* TargetSentinel =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	if (!TestNotNull(TEXT("Moving item exists"), MovingItem) ||
		!TestNotNull(TEXT("Source sentinel exists"), SourceSentinel) ||
		!TestNotNull(TEXT("Target sentinel exists"), TargetSentinel))
	{
		return false;
	}

	FRpgInventoryEntryView MovingBefore;
	FRpgInventoryEntryView SourceSentinelBefore;
	FRpgInventoryEntryView TargetSentinelBefore;
	if (!TestTrue(
			TEXT("Moving item has a stable entry"),
			FindEntry(SourceInventory, MovingItem->GetItemId(), MovingBefore)) ||
		!TestTrue(
			TEXT("Source sentinel has a stable entry"),
			FindEntry(
				SourceInventory,
				SourceSentinel->GetItemId(),
				SourceSentinelBefore)) ||
		!TestTrue(
			TEXT("Target sentinel has a stable entry"),
			FindEntry(
				TargetInventory,
				TargetSentinel->GetItemId(),
				TargetSentinelBefore)))
	{
		return false;
	}

	const int32 SourceRevisionBefore =
		SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore =
		TargetInventory->GetInventoryRevision();
	const FRpgInventoryGridPlacement ExactPlacement =
		MakePlacement(Root, 0, 0);
	const FRpgInventoryMutationRequest Request = MakeTransferRequest(
		SourceInventory,
		MovingItem,
		ERpgInventoryMutationOperation::Transfer,
		Root,
		&ExactPlacement);

	int32 MessageCount = 0;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&MessageCount](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				++MessageCount;
			});

	const FRpgInventoryMutationResult TransferResult =
		SourceInventory->ExecuteCrossInventoryTransfer(
			TargetInventory,
			Request,
			false);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	FRpgInventoryEntryView TargetMovingEntry;
	TestEqual(
		TEXT("The same-actor transfer succeeds"),
		TransferResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestNull(
		TEXT("The source releases the transferred identity"),
		SourceInventory->FindItemById(MovingBefore.ItemId));
	TestEqual(
		TEXT("The target reuses the exact actor-owned UObject"),
		TargetInventory->FindItemById(MovingBefore.ItemId),
		MovingItem);
	if (TestTrue(
			TEXT("The target exposes the transferred entry"),
			FindEntry(TargetInventory, MovingBefore.ItemId, TargetMovingEntry)))
	{
		TestTrue(
			TEXT("The target assigns a fresh inventory-local EntryId"),
			TargetMovingEntry.EntryId != MovingBefore.EntryId);
		TestEqual(
			TEXT("The reused UObject keeps the shared actor as exact Outer"),
			TargetMovingEntry.Instance->GetOuter(),
			static_cast<UObject*>(SharedOwner));
	}
	TestEqual(
		TEXT("Only source removal and target addition are notified"),
		MessageCount,
		2);
	TestEqual(
		TEXT("The source revision advances exactly once"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore + 1);
	TestEqual(
		TEXT("The target revision advances exactly once"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBefore + 1);
	TestTrue(
		TEXT("The source sentinel keeps UObject and EntryId identity"),
		HasStableIdentity(SourceInventory, SourceSentinelBefore));
	TestTrue(
		TEXT("The target sentinel keeps UObject and EntryId identity"),
		HasStableIdentity(TargetInventory, TargetSentinelBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryTransferDeltaSubtreeReconstructionTest,
	"SurvivalRpg.Inventory.TransferDelta.CrossActorSubtreeReconstructsOnlyMovedItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryTransferDeltaSubtreeReconstructionTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransferDeltaTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("SubtreeDeltaSource"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("SubtreeDeltaTarget"));
	if (!TestNotNull(TEXT("Source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("Target inventory exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* SourceBag =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("Source bag exists"), SourceBag))
	{
		return false;
	}
	const FRpgInventoryItemId BagItemId = SourceBag->GetItemId();
	const FRpgInventoryContainerHandle BagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			BagItemId,
			BagContainerId,
			1);
	URpgInventoryItemInstance* SourceChild =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			7,
			MakePlacement(BagContents, 1, 1));
	URpgInventoryItemInstance* SourceSentinel =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 9, 5));
	URpgInventoryItemInstance* TargetSentinel =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 9, 5));
	if (!TestNotNull(TEXT("Nested source child exists"), SourceChild) ||
		!TestNotNull(TEXT("Source sentinel exists"), SourceSentinel) ||
		!TestNotNull(TEXT("Target sentinel exists"), TargetSentinel))
	{
		return false;
	}
	SourceChild->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 3);
	const FRpgInventoryItemId ChildItemId = SourceChild->GetItemId();

	FRpgInventoryEntryView SourceSentinelBefore;
	FRpgInventoryEntryView TargetSentinelBefore;
	if (!TestTrue(
			TEXT("Source sentinel has a stable entry"),
			FindEntry(
				SourceInventory,
				SourceSentinel->GetItemId(),
				SourceSentinelBefore)) ||
		!TestTrue(
			TEXT("Target sentinel has a stable entry"),
			FindEntry(
				TargetInventory,
				TargetSentinel->GetItemId(),
				TargetSentinelBefore)))
	{
		return false;
	}

	const int32 SourceRevisionBefore =
		SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore =
		TargetInventory->GetInventoryRevision();
	const uint64 SourceEpochBefore = SourceInventory->GetMutationEpoch();
	const uint64 TargetEpochBefore = TargetInventory->GetMutationEpoch();
	const FRpgInventoryGridPlacement TargetPlacement =
		MakePlacement(Root, 0, 0);
	const FRpgInventoryMutationRequest Request = MakeTransferRequest(
		SourceInventory,
		SourceBag,
		ERpgInventoryMutationOperation::Transfer,
		Root,
		&TargetPlacement);

	int32 MessageCount = 0;
	bool bEveryCallbackSawFinalGraphs = true;
	bool bSentinelWasNotified = false;
	auto IsFinalStateVisible = [&]()
	{
		URpgInventoryItemInstance* TargetBag =
			TargetInventory->FindItemById(BagItemId);
		URpgInventoryItemInstance* TargetChild =
			TargetInventory->FindItemById(ChildItemId);
		return SourceInventory->FindItemById(BagItemId) == nullptr &&
			SourceInventory->FindItemById(ChildItemId) == nullptr &&
			SourceInventory->GetUsedEntryCount() == 1 &&
			TargetBag && TargetChild &&
			TargetBag != SourceBag && TargetChild != SourceChild &&
			TargetBag->GetOuter() == TargetInventory->GetOwner() &&
			TargetChild->GetOuter() == TargetInventory->GetOwner() &&
			TargetChild->GetStatTagStackCount(
				RpgGameplayTags::Ability_Attack_Basic) == 3 &&
			TargetInventory->GetUsedEntryCount() == 3 &&
			HasStableIdentity(SourceInventory, SourceSentinelBefore) &&
			HasStableIdentity(TargetInventory, TargetSentinelBefore) &&
			SourceInventory->GetInventoryRevision() ==
				SourceRevisionBefore + 1 &&
			TargetInventory->GetInventoryRevision() ==
				TargetRevisionBefore + 1;
	};

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				++MessageCount;
				bEveryCallbackSawFinalGraphs &= IsFinalStateVisible();
				bSentinelWasNotified |=
					Message.Instance == SourceSentinel ||
					Message.Instance == TargetSentinel;
			});

	const FRpgInventoryMutationResult TransferResult =
		SourceInventory->ExecuteCrossInventoryTransfer(
			TargetInventory,
			Request,
			false);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	URpgInventoryItemInstance* TransferredBag =
		TargetInventory->FindItemById(BagItemId);
	URpgInventoryItemInstance* TransferredChild =
		TargetInventory->FindItemById(ChildItemId);
	FRpgInventoryEntryView TransferredChildEntry;
	TestEqual(
		TEXT("The complete cross-actor subtree transfer succeeds"),
		TransferResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("Only bag and child are represented by move deltas"),
		TransferResult.Deltas.Num(),
		2);
	TestNotNull(TEXT("The target reconstructs the bag"), TransferredBag);
	TestNotNull(TEXT("The target reconstructs the child"), TransferredChild);
	if (TransferredBag && TransferredChild)
	{
		TestTrue(
			TEXT("The target bag is a new target-owned UObject"),
			TransferredBag != SourceBag &&
				TransferredBag->GetOuter() == TargetInventory->GetOwner());
		TestTrue(
			TEXT("The target child is a new target-owned UObject"),
			TransferredChild != SourceChild &&
				TransferredChild->GetOuter() == TargetInventory->GetOwner());
		TestEqual(
			TEXT("The child runtime state survives reconstruction"),
			TransferredChild->GetStatTagStackCount(
				RpgGameplayTags::Ability_Attack_Basic),
			3);
	}
	if (TestTrue(
			TEXT("The transferred child has a target entry"),
			FindEntry(TargetInventory, ChildItemId, TransferredChildEntry)))
	{
		TestEqual(
			TEXT("The child remains owned by the transferred bag"),
			TransferredChildEntry.Placement.GetContainerHandle().ItemOwnerId,
			BagItemId);
		TestEqual(
			TEXT("The child keeps its inner-grid X coordinate"),
			TransferredChildEntry.Placement.X,
			1);
		TestEqual(
			TEXT("The child keeps its inner-grid Y coordinate"),
			TransferredChildEntry.Placement.Y,
			1);
	}
	TestEqual(
		TEXT("Only bag and child emit source and target messages"),
		MessageCount,
		4);
	TestTrue(
		TEXT("Every subtree callback observes both final graphs"),
		bEveryCallbackSawFinalGraphs);
	TestFalse(
		TEXT("Unrelated sentinels emit no subtree transfer message"),
		bSentinelWasNotified);
	TestTrue(
		TEXT("The source sentinel keeps UObject and EntryId identity"),
		HasStableIdentity(SourceInventory, SourceSentinelBefore));
	TestTrue(
		TEXT("The target sentinel keeps UObject and EntryId identity"),
		HasStableIdentity(TargetInventory, TargetSentinelBefore));
	TestEqual(
		TEXT("The source revision advances exactly once"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore + 1);
	TestEqual(
		TEXT("The target revision advances exactly once"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBefore + 1);
	TestEqual(
		TEXT("Subtree transfer does not change the source epoch"),
		SourceInventory->GetMutationEpoch(),
		SourceEpochBefore);
	TestEqual(
		TEXT("Subtree transfer does not change the target epoch"),
		TargetInventory->GetMutationEpoch(),
		TargetEpochBefore);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
