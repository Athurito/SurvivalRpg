#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IPickupable.h"
#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Misc/AutomationTest.h"

namespace RpgInventoryPickupBatchTests
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

	FInventoryPickup MakeUnitTemplatePickup(int32 RowCount)
	{
		FInventoryPickup Pickup;
		for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
		{
			FPickupTemplate& Row = Pickup.Templates.AddDefaulted_GetRef();
			Row.ItemDef =
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
			Row.StackCount = 1;
		}
		return Pickup;
	}

	FInventoryPickup MakeStackTemplatePickup(int32 RowCount)
	{
		FInventoryPickup Pickup;
		for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
		{
			FPickupTemplate& Row = Pickup.Templates.AddDefaulted_GetRef();
			Row.ItemDef =
				URpgInventoryAutomationTestStackItemDefinition::StaticClass();
			Row.StackCount = 1;
		}
		return Pickup;
	}

	FRpgInventoryGridPlacement MakeStoragePlacement(int32 X, int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(
			FRpgInventoryContainerHandle::MakeRoot(StorageContainerId));
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}

	FRpgInventoryGridPlacement MakePlacement(
		const FRpgInventoryContainerHandle& ContainerHandle,
		int32 X,
		int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(ContainerHandle);
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}

	FString MakeStrictSignature(
		const URpgInventoryManagerComponent* Inventory)
	{
		if (!Inventory)
		{
			return TEXT("InvalidInventory");
		}

		TArray<FString> Rows;
		for (const FRpgInventoryEntryView& Entry :
			 Inventory->GetAllEntries())
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
		return FString::Printf(
			TEXT("Revision=%d;%s"),
			Inventory->GetInventoryRevision(),
			*FString::Join(Rows, TEXT(";")));
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

	bool AreMutationDeltasEqual(
		const FRpgInventoryMutationDelta& A,
		const FRpgInventoryMutationDelta& B)
	{
		return A.Kind == B.Kind &&
			A.ItemId == B.ItemId &&
			A.BeforeContainer == B.BeforeContainer &&
			A.AfterContainer == B.AfterContainer &&
			A.BeforePlacement == B.BeforePlacement &&
			A.AfterPlacement == B.AfterPlacement &&
			A.PreviousQuantity == B.PreviousQuantity &&
			A.NewQuantity == B.NewQuantity;
	}

	ARpgDroppedInventoryActor* SpawnCanonicalDrop(
		FScopedInventoryWorld& TestWorld,
		const TCHAR* DebugName)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(
			TestWorld.GetWorld(),
			ARpgDroppedInventoryActor::StaticClass(),
			FName(DebugName));
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARpgDroppedInventoryActor* DropActor =
			TestWorld.GetWorld()->SpawnActor<ARpgDroppedInventoryActor>(
				SpawnParameters);
		if (!DropActor)
		{
			return nullptr;
		}

		// An empty static pickup intentionally remains a legacy fallback on authority.
		// Seed and consume one root so collect tests exercise the canonical runtime graph
		// while still starting with an empty source inventory.
		DropActor->SetPickupInventory(MakeStackTemplatePickup(1));
		if (URpgInventoryManagerComponent* Inventory =
				DropActor->GetLootInventoryManager())
		{
			for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
			{
				if (Entry.Placement.GetContainerHandle().IsRoot())
				{
					Inventory->ConsumeItemById(Entry.ItemId, Entry.StackCount);
					break;
				}
			}
		}
		return DropActor;
	}

	FGameplayTag GetInventoryChangedChannel()
	{
		return FGameplayTag::RequestGameplayTag(
			TEXT("Rpg.Inventory.Message.StackChanged"));
	}

	bool InitializeTest(
		FAutomationTestBase& Test,
		FScopedInventoryWorld& TestWorld)
	{
		if (!TestWorld.IsValid())
		{
			Test.AddError(
				TEXT("Could not create an isolated pickup-batch test world."));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPickupBatchSharedScratchRejectTest,
	"SurvivalRpg.Inventory.PickupBatch.SharedScratchRejectsWithoutMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPickupBatchSharedScratchRejectTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("PickupBatchScratchReject"));
	if (!TestNotNull(TEXT("The target inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryGridSize GridSize = Inventory->GetDefaultGridSize();
	const int32 GridCellCount = GridSize.Width * GridSize.Height;
	if (!TestTrue(
			TEXT("The fixture exposes at least two spatial cells"),
			GridCellCount >= 2) ||
		!TestNotNull(
			TEXT("All but one target cell can be occupied"),
			Inventory->GrantItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				GridCellCount - 1)))
	{
		return false;
	}

	const FInventoryPickup SingleRowPickup = MakeUnitTemplatePickup(1);
	const FInventoryPickup TwoRowPickup = MakeUnitTemplatePickup(2);
	const FString SignatureBeforeBatch = MakeStrictSignature(Inventory);
	const int32 RevisionBeforeBatch = Inventory->GetInventoryRevision();
	int32 MessageCount = 0;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner == Inventory)
				{
					++MessageCount;
				}
			});

	TestTrue(
		TEXT("The first payload row independently fits the final cell"),
		Inventory->CanAddPickupBatch(SingleRowPickup));
	TestTrue(
		TEXT("The second identical payload row independently fits that cell"),
		Inventory->CanAddPickupBatch(SingleRowPickup));
	TestFalse(
		TEXT("The shared batch scratch prevents both rows claiming one cell"),
		Inventory->CanAddPickupBatch(TwoRowPickup));
	TestEqual(
		TEXT("All pickup preflights preserve the exact inventory graph"),
		MakeStrictSignature(Inventory),
		SignatureBeforeBatch);

	TArray<FRpgInventoryItemId> AffectedItemIds;
	const FRpgInventoryMutationResult Result =
		Inventory->AddPickupBatch(TwoRowPickup, AffectedItemIds);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestFalse(TEXT("The overcommitted batch is rejected"), Result.IsSuccess());
	TestEqual(
		TEXT("A rejected batch applies no quantity"),
		Result.AppliedQuantity,
		0);
	TestTrue(
		TEXT("A rejected batch exposes no authoritative deltas"),
		Result.Deltas.IsEmpty());
	TestTrue(
		TEXT("A rejected batch exposes no affected item ids"),
		AffectedItemIds.IsEmpty());
	TestEqual(
		TEXT("The rejected batch emits no inventory notifications"),
		MessageCount,
		0);
	TestEqual(
		TEXT("The rejected batch does not advance inventory revision"),
		Inventory->GetInventoryRevision(),
		RevisionBeforeBatch);
	TestEqual(
		TEXT("The rejected batch preserves the exact target graph"),
		MakeStrictSignature(Inventory),
		SignatureBeforeBatch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPickupBatchAtomicCommitTest,
	"SurvivalRpg.Inventory.PickupBatch.AtomicCommitUsesDeterministicScratchOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPickupBatchAtomicCommitTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("PickupBatchAtomicCommit"));
	if (!TestNotNull(TEXT("The target inventory exists"), Inventory))
	{
		return false;
	}

	const FInventoryPickup Pickup = MakeUnitTemplatePickup(2);
	const int32 RevisionBeforePreflight = Inventory->GetInventoryRevision();
	const FString SignatureBeforePreflight = MakeStrictSignature(Inventory);
	int32 MessageCount = 0;
	bool bEveryCallbackSawFinalState = true;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner != Inventory)
				{
					return;
				}

				++MessageCount;
				bEveryCallbackSawFinalState &=
					Inventory->GetInventoryRevision() ==
						RevisionBeforePreflight + 1 &&
					Inventory->GetUsedEntryCount() == 2;
			});

	TestTrue(
		TEXT("The complete two-row payload passes shared-scratch preflight"),
		Inventory->CanAddPickupBatch(Pickup));
	TestEqual(
		TEXT("Pickup preflight is graph-read-only"),
		MakeStrictSignature(Inventory),
		SignatureBeforePreflight);

	TArray<FRpgInventoryItemId> AffectedItemIds;
	const FRpgInventoryMutationResult Result =
		Inventory->AddPickupBatch(Pickup, AffectedItemIds);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestEqual(
		TEXT("The complete batch commits successfully"),
		Result.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The result reports the pickup operation"),
		Result.Operation,
		ERpgInventoryMutationOperation::Pickup);
	TestEqual(
		TEXT("Both requested units are committed"),
		Result.AppliedQuantity,
		2);
	TestEqual(
		TEXT("Both payload rows create one entry delta"),
		Result.Deltas.Num(),
		2);
	TestEqual(
		TEXT("The batch returns one representative id per payload row"),
		AffectedItemIds.Num(),
		2);
	TestEqual(
		TEXT("Both changed rows emit one notification"),
		MessageCount,
		2);
	TestTrue(
		TEXT("Every notification observes the complete final batch graph"),
		bEveryCallbackSawFinalState);
	TestEqual(
		TEXT("The complete batch advances inventory revision exactly once"),
		Inventory->GetInventoryRevision(),
		RevisionBeforePreflight + 1);
	TestEqual(
		TEXT("Shared scratch creates two non-overlapping entries"),
		Inventory->GetUsedEntryCount(),
		2);

	if (AffectedItemIds.Num() == 2)
	{
		TestTrue(
			TEXT("Each payload row maps to a distinct concrete item"),
			AffectedItemIds[0] != AffectedItemIds[1]);

		FRpgInventoryEntryView FirstEntry;
		FRpgInventoryEntryView SecondEntry;
		const bool bFoundFirst = FindEntry(
			Inventory,
			AffectedItemIds[0],
			FirstEntry);
		const bool bFoundSecond = FindEntry(
			Inventory,
			AffectedItemIds[1],
			SecondEntry);
		TestTrue(TEXT("The first affected id resolves"), bFoundFirst);
		TestTrue(TEXT("The second affected id resolves"), bFoundSecond);
		if (bFoundFirst && bFoundSecond)
		{
			TestEqual(
				TEXT("Payload row zero claims the deterministic first cell"),
				FirstEntry.Placement.X,
				0);
			TestEqual(
				TEXT("Payload row zero remains on the first row"),
				FirstEntry.Placement.Y,
				0);
			TestEqual(
				TEXT("Payload row one advances through shared scratch"),
				SecondEntry.Placement.X,
				1);
			TestEqual(
				TEXT("Payload row one remains on the first row"),
				SecondEntry.Placement.Y,
				0);
			TestEqual(
				TEXT("Both entries use the default storage root"),
				FirstEntry.Placement.GetContainerHandle(),
				FRpgInventoryContainerHandle::MakeRoot(StorageContainerId));
			TestEqual(
				TEXT("Both entries use the same default storage root"),
				SecondEntry.Placement.GetContainerHandle(),
				FirstEntry.Placement.GetContainerHandle());
		}

		if (Result.Deltas.Num() == 2)
		{
			TestEqual(
				TEXT("The first delta follows payload order"),
				Result.Deltas[0].ItemId,
				AffectedItemIds[0]);
			TestEqual(
				TEXT("The second delta follows payload order"),
				Result.Deltas[1].ItemId,
				AffectedItemIds[1]);
			TestEqual(
				TEXT("The first row is reported as added"),
				Result.Deltas[0].Kind,
				ERpgInventoryMutationDeltaKind::Added);
			TestEqual(
				TEXT("The second row is reported as added"),
				Result.Deltas[1].Kind,
				ERpgInventoryMutationDeltaKind::Added);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPickupBatchMixedMergeDetachedCapacityTest,
	"SurvivalRpg.Inventory.PickupBatch.MixedMergeDetachedCapacityAndReentrancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPickupBatchMixedMergeDetachedCapacityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("PickupBatchMixedTarget"));
	URpgInventoryManagerComponent* ForeignSetupInventory =
		TestWorld.CreateInventory(TEXT("PickupBatchForeignSetup"));
	if (!TestNotNull(TEXT("The mixed target inventory exists"), Inventory) ||
		!TestNotNull(
			TEXT("The foreign detached-instance setup inventory exists"),
			ForeignSetupInventory))
	{
		return false;
	}

	URpgInventoryItemInstance* TemplateMergeTarget =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			8,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* ConcreteMergeTrap =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			5,
			MakeStoragePlacement(4, 0));
	URpgInventoryItemInstance* SameOwnerDetached =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(1, 0));
	if (!TestNotNull(
			TEXT("The template merge target exists"),
			TemplateMergeTarget) ||
		!TestNotNull(
			TEXT("The runtime-compatible concrete merge trap exists"),
			ConcreteMergeTrap) ||
		!TestNotNull(
			TEXT("The same-owner detached fixture exists"),
			SameOwnerDetached))
	{
		return false;
	}

	SameOwnerDetached->AddStatTagStack(
		RpgGameplayTags::Ability_Attack_Basic,
		1);
	TestTrue(
		TEXT("Canonical consume detaches the same-owner concrete fixture"),
		Inventory->ConsumeItemById(
			SameOwnerDetached->GetItemId(),
			Inventory->GetItemStackCount(SameOwnerDetached)).IsSuccess());
	TestFalse(
		TEXT("The same-owner concrete fixture is detached before pickup"),
		Inventory->ContainsItemInstance(SameOwnerDetached));

	URpgInventoryItemInstance* ForeignTaggedDetached =
		ForeignSetupInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* ForeignCompatibleDetached =
		ForeignSetupInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(1, 0));
	if (!TestNotNull(
			TEXT("The tagged foreign detached fixture exists"),
			ForeignTaggedDetached) ||
		!TestNotNull(
			TEXT("The runtime-compatible foreign fixture exists"),
			ForeignCompatibleDetached))
	{
		return false;
	}

	ForeignTaggedDetached->AddStatTagStack(
		RpgGameplayTags::Ability_Support_Heal,
		2);
	TestTrue(
		TEXT("Canonical consume detaches the tagged foreign fixture"),
		ForeignSetupInventory->ConsumeItemById(
			ForeignTaggedDetached->GetItemId(),
			ForeignSetupInventory->GetItemStackCount(ForeignTaggedDetached)).IsSuccess());
	TestTrue(
		TEXT("Canonical consume detaches the compatible foreign fixture"),
		ForeignSetupInventory->ConsumeItemById(
			ForeignCompatibleDetached->GetItemId(),
			ForeignSetupInventory->GetItemStackCount(ForeignCompatibleDetached)).IsSuccess());
	TestEqual(
		TEXT("Both foreign concrete fixtures are detached"),
		ForeignSetupInventory->GetUsedEntryCount(),
		0);
	TestTrue(
		TEXT("The untagged concrete fixture is stack-compatible with the live trap"),
		ForeignCompatibleDetached->IsStackCompatibleWith(
			ConcreteMergeTrap));

	Inventory->SetFixedMaxEntries(5);
	Inventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	TestEqual(
		TEXT("The fixture permits exactly the two live and three concrete rows"),
		Inventory->GetMaxEntries(),
		5);

	FInventoryPickup Pickup = MakeStackTemplatePickup(2);
	Pickup.Instances.AddDefaulted_GetRef().Item = SameOwnerDetached;
	Pickup.Instances.AddDefaulted_GetRef().Item = ForeignTaggedDetached;
	Pickup.Instances.AddDefaulted_GetRef().Item =
		ForeignCompatibleDetached;
	const FInventoryPickup ReentrantPickup = MakeStackTemplatePickup(1);
	const int32 RevisionBeforeBatch = Inventory->GetInventoryRevision();
	const FString SignatureBeforeBatch = MakeStrictSignature(Inventory);

	int32 MessageCount = 0;
	bool bEveryCallbackSawFinalState = true;
	bool bConcreteMergeTrapWasNotified = false;
	bool bAttemptedReentrantPickup = false;
	bool bReentrantCanAdd = true;
	FRpgInventoryMutationResult ReentrantResult;
	TArray<FRpgInventoryItemId> ReentrantAffectedItemIds;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner != Inventory)
				{
					return;
				}

				++MessageCount;
				bEveryCallbackSawFinalState &=
					Inventory->GetInventoryRevision() ==
						RevisionBeforeBatch + 1 &&
					Inventory->GetUsedEntryCount() == 5 &&
					Inventory->GetItemStackCount(TemplateMergeTarget) == 10 &&
					Inventory->GetItemStackCount(ConcreteMergeTrap) == 5 &&
					Inventory->ContainsItemInstance(SameOwnerDetached);
				bConcreteMergeTrapWasNotified |=
					Message.Instance == ConcreteMergeTrap;
				if (!bAttemptedReentrantPickup)
				{
					bAttemptedReentrantPickup = true;
					bReentrantCanAdd =
						Inventory->CanAddPickupBatch(ReentrantPickup);
					ReentrantResult = Inventory->AddPickupBatch(
						ReentrantPickup,
						ReentrantAffectedItemIds);
				}
			});

	TestTrue(
		TEXT("The mixed payload passes shared-scratch preflight"),
		Inventory->CanAddPickupBatch(Pickup));
	TestEqual(
		TEXT("Mixed preflight preserves the complete target graph"),
		MakeStrictSignature(Inventory),
		SignatureBeforeBatch);

	TArray<FRpgInventoryItemId> AffectedItemIds;
	const FRpgInventoryMutationResult Result =
		Inventory->AddPickupBatch(Pickup, AffectedItemIds);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestEqual(
		TEXT("The mixed pickup commits successfully"),
		Result.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("All template units and concrete instances are applied"),
		Result.AppliedQuantity,
		5);
	TestEqual(
		TEXT("One coalesced merge and three concrete rows produce four deltas"),
		Result.Deltas.Num(),
		4);
	int32 StackChangeDeltaCount = 0;
	int32 AddedDeltaCount = 0;
	for (const FRpgInventoryMutationDelta& Delta : Result.Deltas)
	{
		StackChangeDeltaCount +=
			Delta.Kind == ERpgInventoryMutationDeltaKind::StackChanged;
		AddedDeltaCount +=
			Delta.Kind == ERpgInventoryMutationDeltaKind::Added;
	}
	TestEqual(
		TEXT("Both template rows coalesce into one stack-change delta"),
		StackChangeDeltaCount,
		1);
	TestEqual(
		TEXT("Every concrete instance produces its own added-entry delta"),
		AddedDeltaCount,
		3);
	TestEqual(
		TEXT("The mixed batch returns one representative per payload row"),
		AffectedItemIds.Num(),
		5);
	TestEqual(
		TEXT("Only the coalesced merge and three new rows notify"),
		MessageCount,
		4);
	TestTrue(
		TEXT("Every mixed-batch notification observes the complete final graph"),
		bEveryCallbackSawFinalState);
	TestFalse(
		TEXT("The untouched compatible concrete merge trap does not notify"),
		bConcreteMergeTrapWasNotified);
	TestEqual(
		TEXT("The mixed batch advances inventory revision exactly once"),
		Inventory->GetInventoryRevision(),
		RevisionBeforeBatch + 1);
	TestEqual(
		TEXT("The two template rows share one live stack"),
		Inventory->GetItemStackCount(TemplateMergeTarget),
		10);
	TestEqual(
		TEXT("A compatible concrete instance never merges into a live stack"),
		Inventory->GetItemStackCount(ConcreteMergeTrap),
		5);
	TestEqual(
		TEXT("The exact entry budget is fully occupied"),
		Inventory->GetUsedEntryCount(),
		5);
	const FRpgInventoryMutationDelta* TemplateMergeDelta =
		Result.Deltas.FindByPredicate(
			[TemplateMergeTarget](
				const FRpgInventoryMutationDelta& Delta)
			{
				return Delta.ItemId ==
					TemplateMergeTarget->GetItemId();
			});
	if (TestNotNull(
			TEXT("The coalesced template merge exposes one target delta"),
			TemplateMergeDelta))
	{
		TestEqual(
			TEXT("The template target reports one stack change"),
			TemplateMergeDelta->Kind,
			ERpgInventoryMutationDeltaKind::StackChanged);
		TestEqual(
			TEXT("The coalesced template merge captures the original count"),
			TemplateMergeDelta->PreviousQuantity,
			8);
		TestEqual(
			TEXT("The coalesced template merge captures both rows"),
			TemplateMergeDelta->NewQuantity,
			10);
	}

	TestTrue(
		TEXT("A notification attempted one reentrant pickup"),
		bAttemptedReentrantPickup);
	TestFalse(
		TEXT("Pickup preflight is guarded during batch notifications"),
		bReentrantCanAdd);
	TestEqual(
		TEXT("A reentrant pickup commit fails closed"),
		ReentrantResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestTrue(
		TEXT("A reentrant pickup exposes no deltas"),
		ReentrantResult.Deltas.IsEmpty());
	TestTrue(
		TEXT("A reentrant pickup exposes no affected ids"),
		ReentrantAffectedItemIds.IsEmpty());

	if (AffectedItemIds.Num() == 5)
	{
		TestEqual(
			TEXT("Template row zero resolves to the existing merge target"),
			AffectedItemIds[0],
			TemplateMergeTarget->GetItemId());
		TestEqual(
			TEXT("Template row one resolves to the same coalesced target"),
			AffectedItemIds[1],
			TemplateMergeTarget->GetItemId());
		TestEqual(
			TEXT("The same-owner concrete row keeps its persistent id"),
			AffectedItemIds[2],
			SameOwnerDetached->GetItemId());
		TestTrue(
			TEXT("The tagged foreign concrete row receives a fresh id"),
			AffectedItemIds[3] != ForeignTaggedDetached->GetItemId());
		TestTrue(
			TEXT("The compatible foreign concrete row receives a fresh id"),
			AffectedItemIds[4] != ForeignCompatibleDetached->GetItemId());

		URpgInventoryItemInstance* TaggedClone =
			Inventory->FindItemById(AffectedItemIds[3]);
		URpgInventoryItemInstance* CompatibleClone =
			Inventory->FindItemById(AffectedItemIds[4]);
		TestEqual(
			TEXT("Same-owner pickup reuses the exact concrete UObject"),
			Inventory->FindItemById(AffectedItemIds[2]),
			SameOwnerDetached);
		TestEqual(
			TEXT("The same-owner concrete row retains one unit"),
			Inventory->GetItemStackCount(SameOwnerDetached),
			1);
		TestEqual(
			TEXT("The same-owner concrete row preserves mutable state"),
			SameOwnerDetached->GetStatTagStackCount(
				RpgGameplayTags::Ability_Attack_Basic),
			1);
		TestNotNull(
			TEXT("The tagged foreign concrete clone resolves"),
			TaggedClone);
		TestNotNull(
			TEXT("The compatible foreign concrete clone resolves"),
			CompatibleClone);
		if (TaggedClone && CompatibleClone)
		{
			TestTrue(
				TEXT("The tagged foreign row is reconstructed as a new UObject"),
				TaggedClone != ForeignTaggedDetached);
			TestEqual(
				TEXT("The tagged clone is owned by the target actor"),
				TaggedClone->GetOuter(),
				static_cast<UObject*>(Inventory->GetOwner()));
			TestEqual(
				TEXT("The tagged clone preserves mutable runtime state"),
				TaggedClone->GetStatTagStackCount(
					RpgGameplayTags::Ability_Support_Heal),
				2);
			TestTrue(
				TEXT("The default-state clone remains compatible with the live trap"),
				CompatibleClone->IsStackCompatibleWith(
					ConcreteMergeTrap));
			TestTrue(
				TEXT("The compatible foreign row is reconstructed as a new UObject"),
				CompatibleClone != ForeignCompatibleDetached);
			TestEqual(
				TEXT("The runtime-compatible concrete clone retains its own entry"),
				Inventory->GetItemStackCount(CompatibleClone),
				1);
			TestEqual(
				TEXT("The compatible clone is owned by the target actor"),
				CompatibleClone->GetOuter(),
				static_cast<UObject*>(Inventory->GetOwner()));
		}
	}

	const FString SignatureBeforeReleasedGuardPreflight =
		MakeStrictSignature(Inventory);
	TestTrue(
		TEXT("The pickup guard is released after all notifications return"),
		Inventory->CanAddPickupBatch(ReentrantPickup));
	TestEqual(
		TEXT("Post-notification preflight remains read-only"),
		MakeStrictSignature(Inventory),
		SignatureBeforeReleasedGuardPreflight);

	const FInventoryPickup CapacityOverflowPickup =
		MakeUnitTemplatePickup(1);
	const FString SignatureBeforeCapacityReject =
		MakeStrictSignature(Inventory);
	const int32 RevisionBeforeCapacityReject =
		Inventory->GetInventoryRevision();
	int32 CapacityRejectMessageCount = 0;
	const FGameplayMessageListenerHandle CapacityListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner == Inventory)
				{
					++CapacityRejectMessageCount;
				}
			});
	TestFalse(
		TEXT("A new concrete row cannot exceed the exact entry budget"),
		Inventory->CanAddPickupBatch(CapacityOverflowPickup));
	TArray<FRpgInventoryItemId> CapacityAffectedItemIds;
	const FRpgInventoryMutationResult CapacityResult =
		Inventory->AddPickupBatch(
			CapacityOverflowPickup,
			CapacityAffectedItemIds);
	MessageSubsystem.UnregisterListener(CapacityListenerHandle);
	TestEqual(
		TEXT("Entry-budget overflow reports NoSpace"),
		CapacityResult.Code,
		ERpgInventoryMutationResultCode::NoSpace);
	TestTrue(
		TEXT("Capacity rejection exposes no deltas"),
		CapacityResult.Deltas.IsEmpty());
	TestTrue(
		TEXT("Capacity rejection exposes no affected ids"),
		CapacityAffectedItemIds.IsEmpty());
	TestEqual(
		TEXT("Capacity rejection emits no notifications"),
		CapacityRejectMessageCount,
		0);
	TestEqual(
		TEXT("Capacity rejection does not advance revision"),
		Inventory->GetInventoryRevision(),
		RevisionBeforeCapacityReject);
	TestEqual(
		TEXT("Capacity rejection preserves the exact committed graph"),
		MakeStrictSignature(Inventory),
		SignatureBeforeCapacityReject);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPickupBatchActorWideIdentityTest,
	"SurvivalRpg.Inventory.PickupBatch.ActorWideIdentityAndManagedInstanceReject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPickupBatchActorWideIdentityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	AActor* SharedOwner =
		TestWorld.CreateOwner(TEXT("PickupBatchActorWideOwner"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventoryOnOwner(
			SharedOwner,
			TEXT("TargetInventory"));
	URpgInventoryManagerComponent* SiblingInventory =
		TestWorld.CreateInventoryOnOwner(
			SharedOwner,
			TEXT("SiblingInventory"));
	if (!TestNotNull(TEXT("The shared inventory actor exists"), SharedOwner) ||
		!TestNotNull(TEXT("The target inventory exists"), TargetInventory) ||
		!TestNotNull(TEXT("The sibling inventory exists"), SiblingInventory))
	{
		return false;
	}

	URpgInventoryItemInstance* SiblingIdentityOwner =
		SiblingInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* DuplicateIdCandidate =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(0, 0));
	if (!TestNotNull(
			TEXT("The sibling-managed identity owner exists"),
			SiblingIdentityOwner) ||
		!TestNotNull(
			TEXT("The target-owned collision candidate exists"),
			DuplicateIdCandidate))
	{
		return false;
	}

	TestTrue(
		TEXT("Canonical consume detaches the duplicate-id candidate"),
		TargetInventory->ConsumeItemById(
			DuplicateIdCandidate->GetItemId(),
			TargetInventory->GetItemStackCount(DuplicateIdCandidate)).IsSuccess());
	TestTrue(
		TEXT("The detached target-owned candidate accepts the sibling id"),
		DuplicateIdCandidate->RestoreItemId(
			SiblingIdentityOwner->GetItemId()));
	TestFalse(
		TEXT("The collision candidate remains detached"),
		TargetInventory->ContainsItemInstance(DuplicateIdCandidate));

	const FString TargetSignatureBefore =
		MakeStrictSignature(TargetInventory);
	const FString SiblingSignatureBefore =
		MakeStrictSignature(SiblingInventory);
	int32 MessageCount = 0;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner == TargetInventory ||
					Message.InventoryOwner == SiblingInventory)
				{
					++MessageCount;
				}
			});

	FInventoryPickup ManagedPickup;
	ManagedPickup.Instances.AddDefaulted_GetRef().Item =
		SiblingIdentityOwner;
	TestFalse(
		TEXT("A sibling-managed concrete instance cannot be copied as pickup data"),
		TargetInventory->CanAddPickupBatch(ManagedPickup));
	TArray<FRpgInventoryItemId> ManagedAffectedItemIds;
	const FRpgInventoryMutationResult ManagedResult =
		TargetInventory->AddPickupBatch(
			ManagedPickup,
			ManagedAffectedItemIds);
	TestEqual(
		TEXT("A managed concrete source reports InvalidRequest"),
		ManagedResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestTrue(
		TEXT("Managed-source rejection exposes no deltas"),
		ManagedResult.Deltas.IsEmpty());
	TestTrue(
		TEXT("Managed-source rejection exposes no affected ids"),
		ManagedAffectedItemIds.IsEmpty());

	FInventoryPickup DuplicateIdPickup;
	DuplicateIdPickup.Instances.AddDefaulted_GetRef().Item =
		DuplicateIdCandidate;
	TestFalse(
		TEXT("Actor-wide preflight rejects a sibling-owned persistent id"),
		TargetInventory->CanAddPickupBatch(DuplicateIdPickup));
	TArray<FRpgInventoryItemId> DuplicateAffectedItemIds;
	const FRpgInventoryMutationResult DuplicateResult =
		TargetInventory->AddPickupBatch(
			DuplicateIdPickup,
			DuplicateAffectedItemIds);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestEqual(
		TEXT("Actor-wide identity collision reports DuplicateItemId"),
		DuplicateResult.Code,
		ERpgInventoryMutationResultCode::DuplicateItemId);
	TestTrue(
		TEXT("Identity rejection exposes no deltas"),
		DuplicateResult.Deltas.IsEmpty());
	TestTrue(
		TEXT("Identity rejection exposes no affected ids"),
		DuplicateAffectedItemIds.IsEmpty());
	TestEqual(
		TEXT("Both actor-wide rejection paths emit no notifications"),
		MessageCount,
		0);
	TestEqual(
		TEXT("Actor-wide rejection preserves the exact target graph"),
		MakeStrictSignature(TargetInventory),
		TargetSignatureBefore);
	TestEqual(
		TEXT("Actor-wide rejection preserves the exact sibling graph"),
		MakeStrictSignature(SiblingInventory),
		SiblingSignatureBefore);
	TestEqual(
		TEXT("The sibling remains the unique actor-wide identity owner"),
		SiblingInventory->FindItemById(
			SiblingIdentityOwner->GetItemId()),
		SiblingIdentityOwner);
	TestNull(
		TEXT("The target never publishes the colliding identity"),
		TargetInventory->FindItemById(
			SiblingIdentityOwner->GetItemId()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCanonicalDropPickupHelperTest,
	"SurvivalRpg.Inventory.PickupBatch.CanonicalDropHelperFailsClosedWithoutDuplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCanonicalDropPickupHelperTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("CanonicalDropHelperTarget"));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(
		TestWorld.GetWorld(),
		ARpgDroppedInventoryActor::StaticClass(),
		TEXT("CanonicalDropHelperSource"));
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgDroppedInventoryActor* DropActor =
		TestWorld.GetWorld()->SpawnActor<ARpgDroppedInventoryActor>(
			SpawnParameters);
	if (!TestNotNull(TEXT("The pickup target inventory exists"), TargetInventory) ||
		!TestNotNull(TEXT("The canonical dropped actor exists"), DropActor))
	{
		return false;
	}

	DropActor->SetPickupInventory(MakeStackTemplatePickup(4));
	URpgInventoryManagerComponent* SourceInventory =
		DropActor->GetLootInventoryManager();
	if (!TestNotNull(TEXT("The canonical source inventory exists"), SourceInventory))
	{
		return false;
	}
	TestTrue(
		TEXT("The dropped actor exposes its runtime inventory as canonical"),
		DropActor->IsLootInventoryCanonical());
	TestEqual(
		TEXT("The canonical source owns the complete stack before the helper call"),
		SourceInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		4);

	const FString SourceSignatureBefore = MakeStrictSignature(SourceInventory);
	const FString TargetSignatureBefore = MakeStrictSignature(TargetInventory);
	const int32 SourceRevisionBefore = SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore = TargetInventory->GetInventoryRevision();
	int32 MessageCount = 0;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner == SourceInventory ||
					Message.InventoryOwner == TargetInventory)
				{
					++MessageCount;
				}
			});

	TScriptInterface<IPickupable> PickupInterface(DropActor);
	const bool bAdded = UPickupableStatics::AddPickupToInventory(
		TargetInventory,
		PickupInterface);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestFalse(
		TEXT("The detached-payload helper rejects a canonical source inventory"),
		bAdded);
	TestEqual(
		TEXT("Fail-closed routing emits no source or target notifications"),
		MessageCount,
		0);
	TestEqual(
		TEXT("Fail-closed routing preserves the source revision"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore);
	TestEqual(
		TEXT("Fail-closed routing preserves the target revision"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBefore);
	TestEqual(
		TEXT("Fail-closed routing preserves the exact canonical source graph"),
		MakeStrictSignature(SourceInventory),
		SourceSignatureBefore);
	TestEqual(
		TEXT("Fail-closed routing preserves the exact target graph"),
		MakeStrictSignature(TargetInventory),
		TargetSignatureBefore);
	TestEqual(
		TEXT("The source stack remains available to the source-owned collect path"),
		SourceInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		4);
	TestEqual(
		TEXT("The target receives no duplicated stack units"),
		TargetInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgDroppedInventoryTrySetRollbackTest,
	"SurvivalRpg.Inventory.PickupBatch.DropTrySetRollsBackPartialPopulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgDroppedInventoryTrySetRollbackTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(
		TestWorld.GetWorld(),
		ARpgDroppedInventoryActor::StaticClass(),
		TEXT("TrySetRollbackDrop"));
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgDroppedInventoryActor* DropActor =
		TestWorld.GetWorld()->SpawnActor<ARpgDroppedInventoryActor>(
			SpawnParameters);
	if (!TestNotNull(TEXT("The rollback drop exists"), DropActor) ||
		!TestTrue(
			TEXT("The initial complete pickup payload commits"),
			DropActor->TrySetPickupInventory(MakeStackTemplatePickup(3))))
	{
		return false;
	}

	URpgInventoryManagerComponent* DropInventory =
		DropActor->GetLootInventoryManager();
	const TArray<FRpgInventoryEntryView> BeforeEntries =
		DropInventory ? DropInventory->GetAllEntries()
					  : TArray<FRpgInventoryEntryView>();
	if (!TestNotNull(TEXT("The rollback drop owns an inventory"), DropInventory) ||
		!TestEqual(TEXT("The fixture starts with one merged stack"), BeforeEntries.Num(), 1))
	{
		return false;
	}

	FInventoryPickup PartialFailurePayload;
	FPickupTemplate& ValidFirstRow =
		PartialFailurePayload.Templates.AddDefaulted_GetRef();
	ValidFirstRow.ItemDef =
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	ValidFirstRow.StackCount = 1;
	FPickupTemplate& InvalidSecondRow =
		PartialFailurePayload.Templates.AddDefaulted_GetRef();
	InvalidSecondRow.ItemDef = nullptr;
	InvalidSecondRow.StackCount = 1;

	AddExpectedError(
		TEXT("Cannot replace pickup inventory"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(
		TEXT("TrySet reports the incomplete replacement as failed"),
		DropActor->TrySetPickupInventory(PartialFailurePayload));
	TestTrue(
		TEXT("A failed replacement keeps the restored runtime graph canonical"),
		DropActor->IsLootInventoryCanonical());
	TestEqual(
		TEXT("Rollback removes the partially populated replacement row"),
		DropInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass()),
		0);
	TestEqual(
		TEXT("Rollback restores every unit from the previous payload"),
		DropInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		3);

	const TArray<FRpgInventoryEntryView> AfterEntries =
		DropInventory->GetAllEntries();
	if (!TestEqual(TEXT("Rollback restores one complete stack"), AfterEntries.Num(), 1))
	{
		return false;
	}
	TestTrue(
		TEXT("Rollback preserves the concrete item identity"),
		AfterEntries[0].ItemId == BeforeEntries[0].ItemId);
	TestEqual(
		TEXT("Rollback preserves the prior stack count"),
		AfterEntries[0].StackCount,
		BeforeEntries[0].StackCount);
	TestTrue(
		TEXT("Rollback preserves the prior placement"),
		AfterEntries[0].Placement == BeforeEntries[0].Placement);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCollectBatchMixedRootsTest,
	"SurvivalRpg.Inventory.CollectBatch.MixedRootsUseSharedScratchAndSingleCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCollectBatchMixedRootsTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	ARpgDroppedInventoryActor* SourceActor = SpawnCanonicalDrop(
		TestWorld,
		TEXT("CollectBatchMixedSource"));
	URpgInventoryManagerComponent* SourceInventory = SourceActor
		? SourceActor->GetLootInventoryManager()
		: nullptr;
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("CollectBatchMixedTarget"));
	if (!TestNotNull(TEXT("The canonical mixed source actor exists"), SourceActor) ||
		!TestNotNull(TEXT("The canonical mixed source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("The mixed target inventory exists"), TargetInventory))
	{
		return false;
	}
	TestTrue(
		TEXT("The mixed source exposes its runtime inventory as canonical"),
		SourceActor->IsLootInventoryCanonical());

	const FRpgInventoryContainerHandle Root =
		FRpgInventoryContainerHandle::MakeRoot(StorageContainerId);
	URpgInventoryItemInstance* TargetStack =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			8,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* TargetSentinel = nullptr;
	bool bFilledTargetScratch = TargetStack != nullptr;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			if ((X == 0 && Y == 0) ||
				((X == 1 || X == 3) && Y == 0))
			{
				continue;
			}

			URpgInventoryItemInstance* Filler =
				TargetInventory->AddItemDefinitionToPlacement(
					URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
					1,
					MakeStoragePlacement(X, Y));
			bFilledTargetScratch &= Filler != nullptr;
			if (X == 9 && Y == 5)
			{
				TargetSentinel = Filler;
			}
		}
	}
	if (!TestTrue(
			TEXT("The target leaves exactly two isolated one-cell holes"),
			bFilledTargetScratch) ||
		!TestNotNull(TEXT("The target sentinel exists"), TargetSentinel) ||
		!TestEqual(
			TEXT("The target starts with fifty-eight root entries"),
			TargetInventory->GetUsedEntryCount(),
			58))
	{
		return false;
	}

	URpgInventoryItemInstance* SourceWide =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* SourceBag =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(2, 0));
	if (!TestNotNull(TEXT("The skipped wide source root exists"), SourceWide) ||
		!TestNotNull(TEXT("The provider source root exists"), SourceBag))
	{
		return false;
	}

	const FRpgInventoryContainerHandle BagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			SourceBag->GetItemId(),
			BagContainerId,
			1);
	URpgInventoryItemInstance* SourceChild =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			7,
			MakePlacement(BagContents, 1, 1));
	URpgInventoryItemInstance* SourceWeapon =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(3, 0));
	URpgInventoryItemInstance* SourceStack =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4,
			MakeStoragePlacement(4, 0));
	if (!TestNotNull(TEXT("The provider child exists"), SourceChild) ||
		!TestNotNull(TEXT("The later weapon root exists"), SourceWeapon) ||
		!TestNotNull(TEXT("The final partial-stack root exists"), SourceStack))
	{
		return false;
	}
	SourceChild->AddStatTagStack(
		RpgGameplayTags::Ability_Attack_Basic,
		3);

	const FRpgInventoryItemId WideItemId = SourceWide->GetItemId();
	const FRpgInventoryItemId BagItemId = SourceBag->GetItemId();
	const FRpgInventoryItemId ChildItemId = SourceChild->GetItemId();
	const FRpgInventoryItemId WeaponItemId = SourceWeapon->GetItemId();
	const FRpgInventoryItemId SourceStackItemId = SourceStack->GetItemId();
	const FRpgInventoryItemId TargetStackItemId = TargetStack->GetItemId();
	FRpgInventoryEntryView WideBefore;
	FRpgInventoryEntryView BagBefore;
	FRpgInventoryEntryView ChildBefore;
	FRpgInventoryEntryView WeaponBefore;
	FRpgInventoryEntryView SourceStackBefore;
	FRpgInventoryEntryView TargetStackBefore;
	FRpgInventoryEntryView TargetSentinelBefore;
	if (!TestTrue(TEXT("The wide root has a complete source snapshot"),
			FindEntry(SourceInventory, WideItemId, WideBefore)) ||
		!TestTrue(TEXT("The bag has a complete source snapshot"),
			FindEntry(SourceInventory, BagItemId, BagBefore)) ||
		!TestTrue(TEXT("The child has a complete source snapshot"),
			FindEntry(SourceInventory, ChildItemId, ChildBefore)) ||
		!TestTrue(TEXT("The weapon has a complete source snapshot"),
			FindEntry(SourceInventory, WeaponItemId, WeaponBefore)) ||
		!TestTrue(TEXT("The partial stack has a complete source snapshot"),
			FindEntry(SourceInventory, SourceStackItemId, SourceStackBefore)) ||
		!TestTrue(TEXT("The merge target has a complete snapshot"),
			FindEntry(TargetInventory, TargetStackItemId, TargetStackBefore)) ||
		!TestTrue(TEXT("The target sentinel has a complete snapshot"),
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
	int32 MessageCount = 0;
	bool bEveryCallbackSawFinalGraphs = true;
	bool bUnchangedItemWasNotified = false;
	auto IsFinalStateVisible = [&]()
	{
		FRpgInventoryEntryView RemainingStackEntry;
		FRpgInventoryEntryView FilledTargetStackEntry;
		URpgInventoryItemInstance* RemainingStack =
			SourceInventory->FindItemById(SourceStackItemId);
		URpgInventoryItemInstance* FilledTargetStack =
			TargetInventory->FindItemById(TargetStackItemId);
		URpgInventoryItemInstance* VisibleTargetBag =
			TargetInventory->FindItemById(BagItemId);
		URpgInventoryItemInstance* VisibleTargetChild =
			TargetInventory->FindItemById(ChildItemId);
		URpgInventoryItemInstance* VisibleTargetWeapon =
			TargetInventory->FindItemById(WeaponItemId);
		return SourceInventory->FindItemById(WideItemId) == SourceWide &&
			HasStableIdentity(SourceInventory, WideBefore) &&
			HasStableIdentity(TargetInventory, TargetSentinelBefore) &&
			SourceInventory->FindItemById(BagItemId) == nullptr &&
			SourceInventory->FindItemById(ChildItemId) == nullptr &&
			SourceInventory->FindItemById(WeaponItemId) == nullptr &&
			RemainingStack == SourceStack &&
			FindEntry(
				SourceInventory,
				SourceStackItemId,
				RemainingStackEntry) &&
			RemainingStackEntry.EntryId == SourceStackBefore.EntryId &&
			RemainingStackEntry.Instance == SourceStackBefore.Instance &&
			SourceInventory->GetItemStackCount(RemainingStack) == 2 &&
			SourceInventory->GetUsedEntryCount() == 2 &&
			VisibleTargetBag && VisibleTargetBag != SourceBag &&
			VisibleTargetChild && VisibleTargetChild != SourceChild &&
			VisibleTargetWeapon && VisibleTargetWeapon != SourceWeapon &&
			FilledTargetStack == TargetStack &&
			FindEntry(
				TargetInventory,
				TargetStackItemId,
				FilledTargetStackEntry) &&
			FilledTargetStackEntry.EntryId == TargetStackBefore.EntryId &&
			FilledTargetStackEntry.Instance == TargetStackBefore.Instance &&
			TargetInventory->GetItemStackCount(FilledTargetStack) == 10 &&
			TargetInventory->GetUsedEntryCount() == 61 &&
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
				if (Message.InventoryOwner != SourceInventory &&
					Message.InventoryOwner != TargetInventory)
				{
					return;
				}

				++MessageCount;
				bEveryCallbackSawFinalGraphs &= IsFinalStateVisible();
				bUnchangedItemWasNotified |=
					Message.Instance == SourceWide ||
					Message.Instance == TargetSentinel;
			});

	TArray<FRpgInventoryContainerHandle> TargetContainers;
	TargetContainers.Add(Root);
	const FGuid RequestId = FGuid::NewGuid();
	TArray<FRpgInventoryItemId> AffectedTargetItemIds;
	const FRpgInventoryMutationResult Result =
		SourceInventory->CollectRootItemsBatch(
			TargetInventory,
			TargetContainers,
			RequestId,
			AffectedTargetItemIds);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestEqual(
		TEXT("The mixed collect reports an explicitly partial batch"),
		Result.Code,
		ERpgInventoryMutationResultCode::PartiallyApplied);
	TestEqual(TEXT("The mixed collect preserves its request id"),
		Result.RequestId, RequestId);
	TestEqual(TEXT("The mixed collect reports the pickup operation"),
		Result.Operation, ERpgInventoryMutationOperation::Pickup);
	TestEqual(TEXT("The mixed collect requests all root quantities"),
		Result.RequestedQuantity, 7);
	TestEqual(TEXT("The mixed collect applies both roots and two stack units"),
		Result.AppliedQuantity, 4);
	TestEqual(TEXT("The mixed collect exposes five authoritative deltas"),
		Result.Deltas.Num(), 5);
	TestEqual(TEXT("Every changed source or target row notifies once"),
		MessageCount, 8);
	TestTrue(TEXT("Every callback observes both complete final graphs"),
		bEveryCallbackSawFinalGraphs);
	TestFalse(TEXT("Skipped roots and target sentinels do not notify"),
		bUnchangedItemWasNotified);
	TestEqual(TEXT("The source revision advances exactly once"),
		SourceInventory->GetInventoryRevision(), SourceRevisionBefore + 1);
	TestEqual(TEXT("The target revision advances exactly once"),
		TargetInventory->GetInventoryRevision(), TargetRevisionBefore + 1);

	TestTrue(TEXT("The skipped wide root keeps its complete identity"),
		HasStableIdentity(SourceInventory, WideBefore));
	TestTrue(TEXT("The unrelated target sentinel keeps its complete identity"),
		HasStableIdentity(TargetInventory, TargetSentinelBefore));
	FRpgInventoryEntryView RemainingSourceStack;
	if (TestTrue(TEXT("The partial source stack remains addressable"),
			FindEntry(SourceInventory, SourceStackItemId, RemainingSourceStack)))
	{
		TestEqual(TEXT("The partial source stack keeps its UObject"),
			RemainingSourceStack.Instance.Get(), SourceStack);
		TestEqual(TEXT("The partial source stack keeps its EntryId"),
			RemainingSourceStack.EntryId, SourceStackBefore.EntryId);
		TestTrue(TEXT("The partial source stack keeps its placement"),
			RemainingSourceStack.Placement == SourceStackBefore.Placement);
		TestEqual(TEXT("Exactly two source stack units remain"),
			RemainingSourceStack.StackCount, 2);
	}
	FRpgInventoryEntryView FilledTargetStack;
	if (TestTrue(TEXT("The merge target remains addressable"),
			FindEntry(TargetInventory, TargetStackItemId, FilledTargetStack)))
	{
		TestEqual(TEXT("The merge target keeps its UObject"),
			FilledTargetStack.Instance.Get(), TargetStack);
		TestEqual(TEXT("The merge target keeps its EntryId"),
			FilledTargetStack.EntryId, TargetStackBefore.EntryId);
		TestTrue(TEXT("The merge target keeps its placement"),
			FilledTargetStack.Placement == TargetStackBefore.Placement);
		TestEqual(TEXT("The merge target reaches its exact stack limit"),
			FilledTargetStack.StackCount, 10);
	}

	URpgInventoryItemInstance* TargetBag =
		TargetInventory->FindItemById(BagItemId);
	URpgInventoryItemInstance* TargetChild =
		TargetInventory->FindItemById(ChildItemId);
	URpgInventoryItemInstance* TargetWeapon =
		TargetInventory->FindItemById(WeaponItemId);
	TestNotNull(TEXT("The complete bag root reaches the target"), TargetBag);
	TestNotNull(TEXT("The complete bag child reaches the target"), TargetChild);
	TestNotNull(TEXT("The later-fitting weapon reaches the target"), TargetWeapon);
	if (TargetBag && TargetChild && TargetWeapon)
	{
		TestTrue(TEXT("The bag is reconstructed for the target actor"),
			TargetBag != SourceBag);
		TestTrue(TEXT("The child is reconstructed for the target actor"),
			TargetChild != SourceChild);
		TestTrue(TEXT("The weapon is reconstructed for the target actor"),
			TargetWeapon != SourceWeapon);
		TestEqual(TEXT("The bag has the durable target actor as outer"),
			TargetBag->GetOuter(), static_cast<UObject*>(TargetInventory->GetOwner()));
		TestEqual(TEXT("The child has the durable target actor as outer"),
			TargetChild->GetOuter(), static_cast<UObject*>(TargetInventory->GetOwner()));
		TestEqual(TEXT("The weapon has the durable target actor as outer"),
			TargetWeapon->GetOuter(), static_cast<UObject*>(TargetInventory->GetOwner()));
		TestEqual(TEXT("The child's mutable runtime state survives"),
			TargetChild->GetStatTagStackCount(
				RpgGameplayTags::Ability_Attack_Basic),
			3);
	}

	FRpgInventoryEntryView TargetBagEntry;
	FRpgInventoryEntryView TargetChildEntry;
	FRpgInventoryEntryView TargetWeaponEntry;
	if (TestTrue(TEXT("The target bag has a concrete entry"),
			FindEntry(TargetInventory, BagItemId, TargetBagEntry)) &&
		TestTrue(TEXT("The target child has a concrete entry"),
			FindEntry(TargetInventory, ChildItemId, TargetChildEntry)) &&
		TestTrue(TEXT("The target weapon has a concrete entry"),
			FindEntry(TargetInventory, WeaponItemId, TargetWeaponEntry)))
	{
		TestTrue(TEXT("The bag receives a fresh target EntryId"),
			TargetBagEntry.EntryId != BagBefore.EntryId);
		TestTrue(TEXT("The child receives a fresh target EntryId"),
			TargetChildEntry.EntryId != ChildBefore.EntryId);
		TestTrue(TEXT("The weapon receives a fresh target EntryId"),
			TargetWeaponEntry.EntryId != WeaponBefore.EntryId);
		TestTrue(TEXT("The bag claims the first shared-scratch hole"),
			TargetBagEntry.Placement == MakeStoragePlacement(1, 0));
		TestTrue(TEXT("The weapon claims the second shared-scratch hole"),
			TargetWeaponEntry.Placement == MakeStoragePlacement(3, 0));
		TestEqual(TEXT("The child remains owned by the transferred bag"),
			TargetChildEntry.Placement.GetContainerHandle().ItemOwnerId,
			BagItemId);
		TestTrue(TEXT("The child keeps its inner-grid placement"),
			TargetChildEntry.Placement ==
				MakePlacement(
					FRpgInventoryContainerHandle::MakeItemOwned(
						BagItemId,
						BagContainerId,
						1),
					1,
					1));
	}

	TestEqual(TEXT("Only accepted roots produce affected target ids"),
		AffectedTargetItemIds.Num(), 3);
	if (AffectedTargetItemIds.Num() == 3)
	{
		TestEqual(TEXT("The provider root is the first affected target"),
			AffectedTargetItemIds[0], BagItemId);
		TestEqual(TEXT("The weapon root is the second affected target"),
			AffectedTargetItemIds[1], WeaponItemId);
		TestEqual(TEXT("A partial merge reports the existing target identity"),
			AffectedTargetItemIds[2], TargetStackItemId);
		for (const FRpgInventoryItemId& AffectedId : AffectedTargetItemIds)
		{
			TestNotNull(
				TEXT("Every autoequip candidate id resolves after the complete commit"),
				TargetInventory->FindItemById(AffectedId));
		}
	}
	TestFalse(TEXT("The descendant is not exposed as an autoequip root"),
		AffectedTargetItemIds.Contains(ChildItemId));
	TestFalse(TEXT("The skipped root is not exposed as affected"),
		AffectedTargetItemIds.Contains(WideItemId));
	TestFalse(TEXT("The partial source identity is not exposed as a target id"),
		AffectedTargetItemIds.Contains(SourceStackItemId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCollectBatchCompleteReplayTest,
	"SurvivalRpg.Inventory.CollectBatch.CompleteSourceBecomesEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCollectBatchCompleteReplayTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	ARpgDroppedInventoryActor* SourceActor = SpawnCanonicalDrop(
		TestWorld,
		TEXT("CollectBatchCompleteSource"));
	URpgInventoryManagerComponent* SourceInventory = SourceActor
		? SourceActor->GetLootInventoryManager()
		: nullptr;
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("CollectBatchCompleteTarget"));
	if (!TestNotNull(TEXT("The complete source actor exists"), SourceActor) ||
		!TestNotNull(TEXT("The complete source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("The complete target inventory exists"), TargetInventory))
	{
		return false;
	}

	URpgInventoryItemInstance* SourceWeapon =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* SourceUnit =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(1, 0));
	if (!TestNotNull(TEXT("The complete source weapon exists"), SourceWeapon) ||
		!TestNotNull(TEXT("The complete source unit exists"), SourceUnit))
	{
		return false;
	}
	const FRpgInventoryItemId WeaponItemId = SourceWeapon->GetItemId();
	const FRpgInventoryItemId UnitItemId = SourceUnit->GetItemId();
	const int32 SourceRevisionBefore = SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore = TargetInventory->GetInventoryRevision();
	TArray<FRpgInventoryContainerHandle> TargetContainers;
	TargetContainers.Add(
		FRpgInventoryContainerHandle::MakeRoot(StorageContainerId));
	const FGuid RequestId = FGuid::NewGuid();
	const FGuid FreshCallbackRequestId = FGuid::NewGuid();
	int32 MessageCount = 0;
	bool bExercisedCallbackReentrancy = false;
	FRpgInventoryMutationResult CallbackReplayResult;
	TArray<FRpgInventoryItemId> CallbackReplayAffectedTargetItemIds;
	FRpgInventoryMutationResult CallbackFreshCollectResult;
	TArray<FRpgInventoryItemId> CallbackFreshAffectedTargetItemIds;
	FRpgInventoryMutationResult CallbackConsumeResult;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner == SourceInventory ||
					Message.InventoryOwner == TargetInventory)
				{
					++MessageCount;
					if (!bExercisedCallbackReentrancy)
					{
						bExercisedCallbackReentrancy = true;
						CallbackReplayResult =
							SourceInventory->CollectRootItemsBatch(
								TargetInventory,
								TargetContainers,
								RequestId,
								CallbackReplayAffectedTargetItemIds);
						CallbackConsumeResult =
							TargetInventory->ConsumeItemById(
								WeaponItemId,
								1);
						CallbackFreshCollectResult =
							TargetInventory->CollectRootItemsBatch(
								SourceInventory,
								TargetContainers,
								FreshCallbackRequestId,
								CallbackFreshAffectedTargetItemIds);
					}
				}
			});

	TArray<FRpgInventoryItemId> AffectedTargetItemIds;
	const FRpgInventoryMutationResult Result =
		SourceInventory->CollectRootItemsBatch(
			TargetInventory,
			TargetContainers,
			RequestId,
			AffectedTargetItemIds);
	const int32 MessageCountAfterCommit = MessageCount;
	const FString SourceSignatureAfterCommit =
		MakeStrictSignature(SourceInventory);
	const FString TargetSignatureAfterCommit =
		MakeStrictSignature(TargetInventory);

	TArray<FRpgInventoryItemId> ReplayAffectedTargetItemIds;
	const FRpgInventoryMutationResult ReplayResult =
		SourceInventory->CollectRootItemsBatch(
			TargetInventory,
			TargetContainers,
			RequestId,
			ReplayAffectedTargetItemIds);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestEqual(TEXT("The complete collect succeeds"),
		Result.Code, ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The complete collect reports both requested roots"),
		Result.RequestedQuantity, 2);
	TestEqual(TEXT("The complete collect applies both roots"),
		Result.AppliedQuantity, 2);
	TestEqual(TEXT("Both complete roots produce one move delta"),
		Result.Deltas.Num(), 2);
	TestEqual(TEXT("The complete source becomes empty"),
		SourceInventory->GetUsedEntryCount(), 0);
	TestEqual(TEXT("The target receives exactly both roots"),
		TargetInventory->GetUsedEntryCount(), 2);
	TestEqual(TEXT("The source revision advances exactly once"),
		SourceInventory->GetInventoryRevision(), SourceRevisionBefore + 1);
	TestEqual(TEXT("The target revision advances exactly once"),
		TargetInventory->GetInventoryRevision(), TargetRevisionBefore + 1);
	TestEqual(TEXT("Both roots emit source and target notifications"),
		MessageCountAfterCommit, 4);
	TestTrue(TEXT("The first batch callback exercises all reentrant seams"),
		bExercisedCallbackReentrancy);
	TestEqual(TEXT("An exact collect replay succeeds inside its callback"),
		CallbackReplayResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The callback replay keeps the original request id"),
		CallbackReplayResult.RequestId,
		RequestId);
	TestEqual(TEXT("The callback replay keeps the pickup operation"),
		CallbackReplayResult.Operation,
		ERpgInventoryMutationOperation::Pickup);
	TestEqual(TEXT("The callback replay keeps the requested quantity"),
		CallbackReplayResult.RequestedQuantity,
		Result.RequestedQuantity);
	TestEqual(TEXT("The callback replay keeps the applied quantity"),
		CallbackReplayResult.AppliedQuantity,
		Result.AppliedQuantity);
	bool bCallbackReplayDeltasMatch =
		CallbackReplayResult.Deltas.Num() == Result.Deltas.Num();
	for (int32 DeltaIndex = 0;
		 bCallbackReplayDeltasMatch && DeltaIndex < Result.Deltas.Num();
		 ++DeltaIndex)
	{
		bCallbackReplayDeltasMatch = AreMutationDeltasEqual(
			CallbackReplayResult.Deltas[DeltaIndex],
			Result.Deltas[DeltaIndex]);
	}
	TestTrue(TEXT("The callback replay returns the cached ordered deltas"),
		bCallbackReplayDeltasMatch);
	TestEqual(TEXT("The callback replay restores both affected target ids"),
		CallbackReplayAffectedTargetItemIds.Num(),
		AffectedTargetItemIds.Num());
	if (CallbackReplayAffectedTargetItemIds.Num() == 2 &&
		AffectedTargetItemIds.Num() == 2)
	{
		TestEqual(TEXT("The callback replay keeps the first affected id"),
			CallbackReplayAffectedTargetItemIds[0],
			AffectedTargetItemIds[0]);
		TestEqual(TEXT("The callback replay keeps the second affected id"),
			CallbackReplayAffectedTargetItemIds[1],
			AffectedTargetItemIds[1]);
	}
	TestEqual(TEXT("A fresh collect fails closed inside a batch callback"),
		CallbackFreshCollectResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(TEXT("The rejected fresh collect preserves its request id"),
		CallbackFreshCollectResult.RequestId,
		FreshCallbackRequestId);
	TestEqual(TEXT("The rejected fresh collect reports pickup"),
		CallbackFreshCollectResult.Operation,
		ERpgInventoryMutationOperation::Pickup);
	TestEqual(TEXT("The guarded fresh collect is rejected before planning"),
		CallbackFreshCollectResult.RequestedQuantity,
		0);
	TestEqual(TEXT("The rejected fresh collect applies nothing"),
		CallbackFreshCollectResult.AppliedQuantity,
		0);
	TestEqual(TEXT("The rejected fresh collect exposes no deltas"),
		CallbackFreshCollectResult.Deltas.Num(),
		0);
	TestEqual(TEXT("The rejected fresh collect exposes no affected ids"),
		CallbackFreshAffectedTargetItemIds.Num(),
		0);
	TestEqual(TEXT("A local consume fails closed inside a batch callback"),
		CallbackConsumeResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestTrue(TEXT("The rejected local consume receives a request id"),
		CallbackConsumeResult.RequestId.IsValid());
	TestEqual(TEXT("The rejected local mutation reports consume"),
		CallbackConsumeResult.Operation,
		ERpgInventoryMutationOperation::Consume);
	TestEqual(TEXT("The rejected local consume reports its requested unit"),
		CallbackConsumeResult.RequestedQuantity,
		1);
	TestEqual(TEXT("The rejected local consume applies nothing"),
		CallbackConsumeResult.AppliedQuantity,
		0);
	TestEqual(TEXT("The rejected local consume exposes no deltas"),
		CallbackConsumeResult.Deltas.Num(),
		0);

	TestEqual(TEXT("The complete collect returns one id per source root"),
		AffectedTargetItemIds.Num(), 2);
	if (AffectedTargetItemIds.Num() == 2)
	{
		TestEqual(TEXT("The weapon id remains first"),
			AffectedTargetItemIds[0], WeaponItemId);
		TestEqual(TEXT("The unit id remains second"),
			AffectedTargetItemIds[1], UnitItemId);
	}
	URpgInventoryItemInstance* TargetWeapon =
		TargetInventory->FindItemById(WeaponItemId);
	URpgInventoryItemInstance* TargetUnit =
		TargetInventory->FindItemById(UnitItemId);
	TestNotNull(TEXT("The transferred weapon resolves by persistent id"),
		TargetWeapon);
	TestNotNull(TEXT("The transferred unit resolves by persistent id"),
		TargetUnit);
	if (TargetWeapon && TargetUnit)
	{
		TestTrue(TEXT("The target reconstructs the weapon UObject"),
			TargetWeapon != SourceWeapon);
		TestTrue(TEXT("The target reconstructs the unit UObject"),
			TargetUnit != SourceUnit);
	}

	TestEqual(TEXT("Exact replay returns the cached success code"),
		ReplayResult.Code, Result.Code);
	TestEqual(TEXT("Exact replay keeps the original request id"),
		ReplayResult.RequestId, RequestId);
	TestEqual(TEXT("Exact replay keeps the requested quantity"),
		ReplayResult.RequestedQuantity, Result.RequestedQuantity);
	TestEqual(TEXT("Exact replay keeps the applied quantity"),
		ReplayResult.AppliedQuantity, Result.AppliedQuantity);
	TestEqual(TEXT("Exact replay keeps the authoritative delta count"),
		ReplayResult.Deltas.Num(), Result.Deltas.Num());
	bool bReplayDeltasMatch =
		ReplayResult.Deltas.Num() == Result.Deltas.Num();
	for (int32 DeltaIndex = 0;
		 bReplayDeltasMatch && DeltaIndex < Result.Deltas.Num();
		 ++DeltaIndex)
	{
		bReplayDeltasMatch = AreMutationDeltasEqual(
			ReplayResult.Deltas[DeltaIndex],
			Result.Deltas[DeltaIndex]);
	}
	TestTrue(TEXT("Exact replay returns the identical ordered deltas"),
		bReplayDeltasMatch);
	TestEqual(TEXT("Exact replay restores both affected target ids"),
		ReplayAffectedTargetItemIds.Num(), AffectedTargetItemIds.Num());
	if (ReplayAffectedTargetItemIds.Num() == 2 &&
		AffectedTargetItemIds.Num() == 2)
	{
		TestEqual(TEXT("Replay keeps the first affected id"),
			ReplayAffectedTargetItemIds[0], AffectedTargetItemIds[0]);
		TestEqual(TEXT("Replay keeps the second affected id"),
			ReplayAffectedTargetItemIds[1], AffectedTargetItemIds[1]);
	}
	TestEqual(TEXT("Exact replay emits no second notification batch"),
		MessageCount, MessageCountAfterCommit);
	TestEqual(TEXT("Exact replay preserves the empty source graph"),
		MakeStrictSignature(SourceInventory), SourceSignatureAfterCommit);
	TestEqual(TEXT("Exact replay preserves the complete target graph"),
		MakeStrictSignature(TargetInventory), TargetSignatureAfterCommit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCollectBatchEmptyAndNoFitTest,
	"SurvivalRpg.Inventory.CollectBatch.EmptyAndNoFitAreSideEffectFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCollectBatchEmptyAndNoFitTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root =
		FRpgInventoryContainerHandle::MakeRoot(StorageContainerId);
	TArray<FRpgInventoryContainerHandle> TargetContainers;
	TargetContainers.Add(Root);
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetWorld());

	ARpgDroppedInventoryActor* EmptySourceActor = SpawnCanonicalDrop(
		TestWorld,
		TEXT("CollectBatchEmptySource"));
	URpgInventoryManagerComponent* EmptySourceInventory = EmptySourceActor
		? EmptySourceActor->GetLootInventoryManager()
		: nullptr;
	URpgInventoryManagerComponent* EmptyTargetInventory =
		TestWorld.CreateInventory(TEXT("CollectBatchEmptyTarget"));
	if (!TestNotNull(TEXT("The empty canonical source actor exists"), EmptySourceActor) ||
		!TestNotNull(TEXT("The empty canonical source inventory exists"), EmptySourceInventory) ||
		!TestNotNull(TEXT("The empty target inventory exists"), EmptyTargetInventory))
	{
		return false;
	}
	const FString EmptySourceSignatureBefore =
		MakeStrictSignature(EmptySourceInventory);
	const FString EmptyTargetSignatureBefore =
		MakeStrictSignature(EmptyTargetInventory);
	const int32 EmptySourceRevisionBefore =
		EmptySourceInventory->GetInventoryRevision();
	const int32 EmptyTargetRevisionBefore =
		EmptyTargetInventory->GetInventoryRevision();
	int32 EmptyMessageCount = 0;
	const FGameplayMessageListenerHandle EmptyListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner == EmptySourceInventory ||
					Message.InventoryOwner == EmptyTargetInventory)
				{
					++EmptyMessageCount;
				}
			});
	TArray<FRpgInventoryItemId> EmptyAffectedTargetItemIds;
	const FRpgInventoryMutationResult EmptyResult =
		EmptySourceInventory->CollectRootItemsBatch(
			EmptyTargetInventory,
			TargetContainers,
			FGuid::NewGuid(),
			EmptyAffectedTargetItemIds);
	MessageSubsystem.UnregisterListener(EmptyListenerHandle);

	TestEqual(TEXT("An empty source reports InvalidRequest"),
		EmptyResult.Code, ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(TEXT("An empty source requests no quantity"),
		EmptyResult.RequestedQuantity, 0);
	TestEqual(TEXT("An empty source applies no quantity"),
		EmptyResult.AppliedQuantity, 0);
	TestTrue(TEXT("An empty source exposes no deltas"),
		EmptyResult.Deltas.IsEmpty());
	TestTrue(TEXT("An empty source exposes no affected target ids"),
		EmptyAffectedTargetItemIds.IsEmpty());
	TestEqual(TEXT("An empty source emits no inventory messages"),
		EmptyMessageCount, 0);
	TestEqual(TEXT("An empty source preserves its exact graph"),
		MakeStrictSignature(EmptySourceInventory), EmptySourceSignatureBefore);
	TestEqual(TEXT("An empty source preserves the exact target graph"),
		MakeStrictSignature(EmptyTargetInventory), EmptyTargetSignatureBefore);
	TestEqual(TEXT("An empty source does not advance its revision"),
		EmptySourceInventory->GetInventoryRevision(), EmptySourceRevisionBefore);
	TestEqual(TEXT("An empty source does not advance the target revision"),
		EmptyTargetInventory->GetInventoryRevision(), EmptyTargetRevisionBefore);

	ARpgDroppedInventoryActor* NoFitSourceActor = SpawnCanonicalDrop(
		TestWorld,
		TEXT("CollectBatchNoFitSource"));
	URpgInventoryManagerComponent* NoFitSourceInventory = NoFitSourceActor
		? NoFitSourceActor->GetLootInventoryManager()
		: nullptr;
	URpgInventoryManagerComponent* FullTargetInventory =
		TestWorld.CreateInventory(TEXT("CollectBatchNoFitTarget"));
	if (!TestNotNull(TEXT("The no-fit canonical source actor exists"), NoFitSourceActor) ||
		!TestNotNull(TEXT("The no-fit source inventory exists"), NoFitSourceInventory) ||
		!TestNotNull(TEXT("The full target inventory exists"), FullTargetInventory))
	{
		return false;
	}
	URpgInventoryItemInstance* BlockedSourceItem =
		NoFitSourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(0, 0));
	bool bFilledTarget = true;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			bFilledTarget &=
				FullTargetInventory->AddItemDefinitionToPlacement(
					URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
					1,
					MakeStoragePlacement(X, Y)) != nullptr;
		}
	}
	if (!TestNotNull(TEXT("The blocked source root exists"), BlockedSourceItem) ||
		!TestTrue(TEXT("Every target grid cell is occupied"), bFilledTarget))
	{
		return false;
	}
	FRpgInventoryEntryView BlockedSourceBefore;
	if (!TestTrue(TEXT("The blocked root has a complete source snapshot"),
			FindEntry(
				NoFitSourceInventory,
				BlockedSourceItem->GetItemId(),
				BlockedSourceBefore)))
	{
		return false;
	}
	const FString NoFitSourceSignatureBefore =
		MakeStrictSignature(NoFitSourceInventory);
	const FString FullTargetSignatureBefore =
		MakeStrictSignature(FullTargetInventory);
	const int32 NoFitSourceRevisionBefore =
		NoFitSourceInventory->GetInventoryRevision();
	const int32 FullTargetRevisionBefore =
		FullTargetInventory->GetInventoryRevision();
	int32 NoFitMessageCount = 0;
	const FGameplayMessageListenerHandle NoFitListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			GetInventoryChangedChannel(),
			[&](
				FGameplayTag Channel,
				const FRpgInventoryChangeMessage& Message)
			{
				if (Message.InventoryOwner == NoFitSourceInventory ||
					Message.InventoryOwner == FullTargetInventory)
				{
					++NoFitMessageCount;
				}
			});
	TArray<FRpgInventoryItemId> NoFitAffectedTargetItemIds;
	const FRpgInventoryMutationResult NoFitResult =
		NoFitSourceInventory->CollectRootItemsBatch(
			FullTargetInventory,
			TargetContainers,
			FGuid::NewGuid(),
			NoFitAffectedTargetItemIds);
	MessageSubsystem.UnregisterListener(NoFitListenerHandle);

	TestEqual(TEXT("A zero-fit collect reports NoSpace"),
		NoFitResult.Code, ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(TEXT("A zero-fit collect requests the blocked root"),
		NoFitResult.RequestedQuantity, 1);
	TestEqual(TEXT("A zero-fit collect applies no quantity"),
		NoFitResult.AppliedQuantity, 0);
	TestTrue(TEXT("A zero-fit collect exposes no deltas"),
		NoFitResult.Deltas.IsEmpty());
	TestTrue(TEXT("A zero-fit collect exposes no affected target ids"),
		NoFitAffectedTargetItemIds.IsEmpty());
	TestEqual(TEXT("A zero-fit collect emits no inventory messages"),
		NoFitMessageCount, 0);
	TestEqual(TEXT("A zero-fit collect preserves the exact source graph"),
		MakeStrictSignature(NoFitSourceInventory), NoFitSourceSignatureBefore);
	TestEqual(TEXT("A zero-fit collect preserves the exact target graph"),
		MakeStrictSignature(FullTargetInventory), FullTargetSignatureBefore);
	TestEqual(TEXT("A zero-fit collect does not advance the source revision"),
		NoFitSourceInventory->GetInventoryRevision(), NoFitSourceRevisionBefore);
	TestEqual(TEXT("A zero-fit collect does not advance the target revision"),
		FullTargetInventory->GetInventoryRevision(), FullTargetRevisionBefore);
	TestTrue(TEXT("The blocked source root keeps its complete identity"),
		HasStableIdentity(NoFitSourceInventory, BlockedSourceBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCollectBatchItemOwnedTargetTest,
	"SurvivalRpg.Inventory.CollectBatch.MixedRootAndItemOwnedContainersRebaseSubtree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCollectBatchItemOwnedTargetTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	ARpgDroppedInventoryActor* SourceActor = SpawnCanonicalDrop(
		TestWorld,
		TEXT("CollectBatchItemOwnedSource"));
	URpgInventoryManagerComponent* SourceInventory = SourceActor
		? SourceActor->GetLootInventoryManager()
		: nullptr;
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("CollectBatchItemOwnedTarget"));
	if (!TestNotNull(TEXT("The item-owned source actor exists"), SourceActor) ||
		!TestNotNull(TEXT("The item-owned source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("The item-owned target inventory exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root =
		FRpgInventoryContainerHandle::MakeRoot(StorageContainerId);
	URpgInventoryItemInstance* HostBag =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* ExistingTargetStack =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			8,
			MakeStoragePlacement(1, 0));
	if (!TestNotNull(TEXT("The target host bag exists"), HostBag) ||
		!TestNotNull(TEXT("The root merge target exists"), ExistingTargetStack))
	{
		return false;
	}
	const FRpgInventoryContainerHandle HostBagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			HostBag->GetItemId(),
			BagContainerId,
			1);
	bool bFilledTargetRoot = true;
	URpgInventoryItemInstance* TargetSentinel = nullptr;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			if (Y == 0 && (X == 0 || X == 1))
			{
				continue;
			}

			URpgInventoryItemInstance* Filler =
				TargetInventory->AddItemDefinitionToPlacement(
					URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
					1,
					MakeStoragePlacement(X, Y));
			bFilledTargetRoot &= Filler != nullptr;
			if (X == 9 && Y == 5)
			{
				TargetSentinel = Filler;
			}
		}
	}
	if (!TestTrue(TEXT("The target root is completely occupied"),
			bFilledTargetRoot) ||
		!TestNotNull(TEXT("The target sentinel exists"), TargetSentinel) ||
		!TestEqual(TEXT("The full root contains sixty entries"),
			TargetInventory->GetUsedEntryCount(), 60))
	{
		return false;
	}

	URpgInventoryItemInstance* SourceStack =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* SourceBag =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakeStoragePlacement(1, 0));
	if (!TestNotNull(TEXT("The multi-container source stack exists"), SourceStack) ||
		!TestNotNull(TEXT("The nested provider source root exists"), SourceBag))
	{
		return false;
	}
	const FRpgInventoryContainerHandle SourceBagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			SourceBag->GetItemId(),
			BagContainerId,
			1);
	URpgInventoryItemInstance* SourceChild =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			7,
			MakePlacement(SourceBagContents, 1, 1));
	if (!TestNotNull(TEXT("The nested provider child exists"), SourceChild))
	{
		return false;
	}
	SourceChild->AddStatTagStack(
		RpgGameplayTags::Ability_Attack_Basic,
		3);

	const FRpgInventoryItemId SourceStackItemId = SourceStack->GetItemId();
	const FRpgInventoryItemId SourceBagItemId = SourceBag->GetItemId();
	const FRpgInventoryItemId SourceChildItemId = SourceChild->GetItemId();
	const FRpgInventoryItemId ExistingTargetStackItemId =
		ExistingTargetStack->GetItemId();
	FRpgInventoryEntryView HostBagBefore;
	FRpgInventoryEntryView ExistingTargetStackBefore;
	FRpgInventoryEntryView TargetSentinelBefore;
	FRpgInventoryEntryView SourceBagBefore;
	FRpgInventoryEntryView SourceChildBefore;
	if (!TestTrue(TEXT("The host bag has a stable entry"),
			FindEntry(TargetInventory, HostBag->GetItemId(), HostBagBefore)) ||
		!TestTrue(TEXT("The root merge target has a stable entry"),
			FindEntry(
				TargetInventory,
				ExistingTargetStackItemId,
				ExistingTargetStackBefore)) ||
		!TestTrue(TEXT("The target sentinel has a stable entry"),
			FindEntry(
				TargetInventory,
				TargetSentinel->GetItemId(),
				TargetSentinelBefore)) ||
		!TestTrue(TEXT("The source bag has a stable entry"),
			FindEntry(SourceInventory, SourceBagItemId, SourceBagBefore)) ||
		!TestTrue(TEXT("The source child has a stable entry"),
			FindEntry(SourceInventory, SourceChildItemId, SourceChildBefore)))
	{
		return false;
	}

	const int32 SourceRevisionBefore =
		SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore =
		TargetInventory->GetInventoryRevision();
	TArray<FRpgInventoryContainerHandle> TargetContainers;
	TargetContainers.Add(Root);
	TargetContainers.Add(HostBagContents);
	TArray<FRpgInventoryItemId> AffectedTargetItemIds;
	const FRpgInventoryMutationResult Result =
		SourceInventory->CollectRootItemsBatch(
			TargetInventory,
			TargetContainers,
			FGuid::NewGuid(),
			AffectedTargetItemIds);

	TestEqual(TEXT("Both source roots are collected completely"),
		Result.Code, ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The batch requests the two root quantities"),
		Result.RequestedQuantity, 5);
	TestEqual(TEXT("The batch applies the two root quantities"),
		Result.AppliedQuantity, 5);
	TestEqual(TEXT("Merge, fresh remainder, provider, child, and source removal produce five deltas"),
		Result.Deltas.Num(), 5);
	TestEqual(TEXT("The source is empty after complete collection"),
		SourceInventory->GetUsedEntryCount(), 0);
	TestEqual(TEXT("The target owns the full root plus three new rows"),
		TargetInventory->GetUsedEntryCount(), 63);
	TestEqual(TEXT("The source revision advances exactly once"),
		SourceInventory->GetInventoryRevision(), SourceRevisionBefore + 1);
	TestEqual(TEXT("The target revision advances exactly once"),
		TargetInventory->GetInventoryRevision(), TargetRevisionBefore + 1);
	TestTrue(TEXT("The host bag keeps its complete identity"),
		HasStableIdentity(TargetInventory, HostBagBefore));
	TestTrue(TEXT("The unrelated target sentinel keeps its complete identity"),
		HasStableIdentity(TargetInventory, TargetSentinelBefore));
	FRpgInventoryEntryView ExistingTargetStackAfter;
	if (TestTrue(TEXT("The root merge target remains addressable"),
			FindEntry(
				TargetInventory,
				ExistingTargetStackItemId,
				ExistingTargetStackAfter)))
	{
		TestEqual(TEXT("The root merge target keeps its UObject"),
			ExistingTargetStackAfter.Instance.Get(),
			ExistingTargetStack);
		TestEqual(TEXT("The root merge target keeps its EntryId"),
			ExistingTargetStackAfter.EntryId,
			ExistingTargetStackBefore.EntryId);
		TestTrue(TEXT("The root merge target keeps its placement"),
			ExistingTargetStackAfter.Placement ==
				ExistingTargetStackBefore.Placement);
		TestEqual(TEXT("The root merge target reaches ten"),
			ExistingTargetStackAfter.StackCount,
			10);
	}

	TestEqual(TEXT("The changed target rows retain deterministic order"),
		AffectedTargetItemIds.Num(), 3);
	FRpgInventoryItemId FreshRemainderItemId;
	if (AffectedTargetItemIds.Num() == 3)
	{
		TestEqual(TEXT("The root merge target is affected first"),
			AffectedTargetItemIds[0], ExistingTargetStackItemId);
		FreshRemainderItemId = AffectedTargetItemIds[1];
		TestTrue(TEXT("Merge fanout gives the placed remainder a fresh identity"),
			FreshRemainderItemId.IsValid() &&
			FreshRemainderItemId != SourceStackItemId);
		TestEqual(TEXT("The nested provider root is affected last"),
			AffectedTargetItemIds[2], SourceBagItemId);
	}

	FRpgInventoryEntryView FreshRemainderEntry;
	if (TestTrue(TEXT("The fresh stack remainder resolves in the target"),
			FindEntry(
				TargetInventory,
				FreshRemainderItemId,
				FreshRemainderEntry)))
	{
		TestTrue(TEXT("The fresh remainder is reconstructed for the target actor"),
			FreshRemainderEntry.Instance.Get() != SourceStack);
		TestEqual(TEXT("The fresh remainder keeps exactly two units"),
			FreshRemainderEntry.StackCount, 2);
		TestEqual(TEXT("The fresh remainder is placed inside the host bag"),
			FreshRemainderEntry.Placement.GetContainerHandle(),
			HostBagContents);
		TestEqual(TEXT("The fresh remainder claims the first inner cell"),
			FreshRemainderEntry.Placement.X, 0);
		TestEqual(TEXT("The fresh remainder remains on the first inner row"),
			FreshRemainderEntry.Placement.Y, 0);
	}
	TestNull(TEXT("The fully consumed source stack id is not retained in the target"),
		TargetInventory->FindItemById(SourceStackItemId));

	URpgInventoryItemInstance* TargetSourceBag =
		TargetInventory->FindItemById(SourceBagItemId);
	URpgInventoryItemInstance* TargetSourceChild =
		TargetInventory->FindItemById(SourceChildItemId);
	TestNotNull(TEXT("The nested provider root reaches the target"), TargetSourceBag);
	TestNotNull(TEXT("The nested provider child reaches the target"), TargetSourceChild);
	if (TargetSourceBag && TargetSourceChild)
	{
		TestTrue(TEXT("The nested provider is reconstructed for the target actor"),
			TargetSourceBag != SourceBag);
		TestTrue(TEXT("The nested child is reconstructed for the target actor"),
			TargetSourceChild != SourceChild);
		TestEqual(TEXT("The child's mutable runtime state survives rebasing"),
			TargetSourceChild->GetStatTagStackCount(
				RpgGameplayTags::Ability_Attack_Basic),
			3);
	}
	FRpgInventoryEntryView TargetSourceBagEntry;
	FRpgInventoryEntryView TargetSourceChildEntry;
	if (TestTrue(TEXT("The nested provider has a target entry"),
			FindEntry(
				TargetInventory,
				SourceBagItemId,
				TargetSourceBagEntry)) &&
		TestTrue(TEXT("The rebased child has a target entry"),
			FindEntry(
				TargetInventory,
				SourceChildItemId,
				TargetSourceChildEntry)))
	{
		TestTrue(TEXT("The provider receives a fresh target EntryId"),
			TargetSourceBagEntry.EntryId != SourceBagBefore.EntryId);
		TestTrue(TEXT("The child receives a fresh target EntryId"),
			TargetSourceChildEntry.EntryId != SourceChildBefore.EntryId);
		TestEqual(TEXT("The provider is nested inside the host bag"),
			TargetSourceBagEntry.Placement.GetContainerHandle(),
			HostBagContents);
		TestEqual(TEXT("The provider follows the fresh remainder in scratch order"),
			TargetSourceBagEntry.Placement.X, 1);
		TestEqual(TEXT("The provider remains on the first inner row"),
			TargetSourceBagEntry.Placement.Y, 0);
		TestEqual(TEXT("The child remains owned by the moved provider"),
			TargetSourceChildEntry.Placement.GetContainerHandle().ItemOwnerId,
			SourceBagItemId);
		TestEqual(TEXT("The child depth rebases from one to two"),
			TargetSourceChildEntry.Placement.GetContainerHandle().Depth,
			static_cast<uint8>(2));
		TestEqual(TEXT("The child keeps its inner X coordinate"),
			TargetSourceChildEntry.Placement.X, 1);
		TestEqual(TEXT("The child keeps its inner Y coordinate"),
			TargetSourceChildEntry.Placement.Y, 1);
	}

	TSet<FRpgInventoryItemId> DeltaItemIds;
	bool bEveryDeltaItemIdIsUnique = true;
	for (const FRpgInventoryMutationDelta& Delta : Result.Deltas)
	{
		bEveryDeltaItemIdIsUnique &= Delta.ItemId.IsValid() &&
			!DeltaItemIds.Contains(Delta.ItemId);
		DeltaItemIds.Add(Delta.ItemId);
	}
	TestTrue(TEXT("Merge fanout and nested moves expose unique delta item ids"),
		bEveryDeltaItemIdIsUnique);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCollectBatchPlannedRowMergeTest,
	"SurvivalRpg.Inventory.CollectBatch.LaterRootMergesIntoPlannedMovedRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCollectBatchPlannedRowMergeTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPickupBatchTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	ARpgDroppedInventoryActor* SourceActor = SpawnCanonicalDrop(
		TestWorld,
		TEXT("CollectBatchPlannedMergeSource"));
	URpgInventoryManagerComponent* SourceInventory = SourceActor
		? SourceActor->GetLootInventoryManager()
		: nullptr;
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("CollectBatchPlannedMergeTarget"));
	if (!TestNotNull(TEXT("The planned-merge source actor exists"), SourceActor) ||
		!TestNotNull(TEXT("The planned-merge source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("The planned-merge target inventory exists"), TargetInventory))
	{
		return false;
	}

	bool bFilledTarget = true;
	URpgInventoryItemInstance* TargetSentinel = nullptr;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			if (X == 0 && Y == 0)
			{
				continue;
			}
			URpgInventoryItemInstance* Filler =
				TargetInventory->AddItemDefinitionToPlacement(
					URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
					1,
					MakeStoragePlacement(X, Y));
			bFilledTarget &= Filler != nullptr;
			if (X == 9 && Y == 5)
			{
				TargetSentinel = Filler;
			}
		}
	}
	if (!TestTrue(TEXT("The target leaves exactly one free cell"), bFilledTarget) ||
		!TestNotNull(TEXT("The planned-merge target sentinel exists"), TargetSentinel) ||
		!TestEqual(TEXT("The target begins with fifty-nine rows"),
			TargetInventory->GetUsedEntryCount(), 59))
	{
		return false;
	}

	URpgInventoryItemInstance* FirstSourceStack =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			8,
			MakeStoragePlacement(0, 0));
	URpgInventoryItemInstance* LaterSourceStack =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			5,
			MakeStoragePlacement(1, 0));
	if (!TestNotNull(TEXT("The first source stack exists"), FirstSourceStack) ||
		!TestNotNull(TEXT("The later source stack exists"), LaterSourceStack))
	{
		return false;
	}
	const FRpgInventoryItemId FirstSourceItemId =
		FirstSourceStack->GetItemId();
	const FRpgInventoryItemId LaterSourceItemId =
		LaterSourceStack->GetItemId();
	FRpgInventoryEntryView FirstSourceBefore;
	FRpgInventoryEntryView LaterSourceBefore;
	FRpgInventoryEntryView TargetSentinelBefore;
	if (!TestTrue(TEXT("The first stack has a stable source entry"),
			FindEntry(SourceInventory, FirstSourceItemId, FirstSourceBefore)) ||
		!TestTrue(TEXT("The later stack has a stable source entry"),
			FindEntry(SourceInventory, LaterSourceItemId, LaterSourceBefore)) ||
		!TestTrue(TEXT("The target sentinel has a stable entry"),
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
	TArray<FRpgInventoryContainerHandle> TargetContainers;
	TargetContainers.Add(
		FRpgInventoryContainerHandle::MakeRoot(StorageContainerId));
	TArray<FRpgInventoryItemId> AffectedTargetItemIds;
	const FRpgInventoryMutationResult Result =
		SourceInventory->CollectRootItemsBatch(
			TargetInventory,
			TargetContainers,
			FGuid::NewGuid(),
			AffectedTargetItemIds);

	TestEqual(TEXT("The later root is explicitly partial"),
		Result.Code, ERpgInventoryMutationResultCode::PartiallyApplied);
	TestEqual(TEXT("Both complete source quantities are requested"),
		Result.RequestedQuantity, 13);
	TestEqual(TEXT("The first eight and two later units are applied"),
		Result.AppliedQuantity, 10);
	TestEqual(TEXT("The moved target row and surviving source row produce two deltas"),
		Result.Deltas.Num(), 2);
	TestNull(TEXT("The first source identity leaves the source"),
		SourceInventory->FindItemById(FirstSourceItemId));
	TestEqual(TEXT("Only the partial later source row remains"),
		SourceInventory->GetUsedEntryCount(), 1);
	TestEqual(TEXT("The planned target row fills the final target cell"),
		TargetInventory->GetUsedEntryCount(), 60);
	TestEqual(TEXT("The source revision advances exactly once"),
		SourceInventory->GetInventoryRevision(), SourceRevisionBefore + 1);
	TestEqual(TEXT("The target revision advances exactly once"),
		TargetInventory->GetInventoryRevision(), TargetRevisionBefore + 1);
	TestTrue(TEXT("The unrelated target sentinel keeps its complete identity"),
		HasStableIdentity(TargetInventory, TargetSentinelBefore));

	FRpgInventoryEntryView RemainingLaterSource;
	if (TestTrue(TEXT("The later partial source row remains addressable"),
			FindEntry(
				SourceInventory,
				LaterSourceItemId,
				RemainingLaterSource)))
	{
		TestEqual(TEXT("The later partial source keeps its UObject"),
			RemainingLaterSource.Instance.Get(), LaterSourceStack);
		TestEqual(TEXT("The later partial source keeps its EntryId"),
			RemainingLaterSource.EntryId, LaterSourceBefore.EntryId);
		TestTrue(TEXT("The later partial source keeps its placement"),
			RemainingLaterSource.Placement == LaterSourceBefore.Placement);
		TestEqual(TEXT("Three later source units remain"),
			RemainingLaterSource.StackCount, 3);
	}

	URpgInventoryItemInstance* FilledPlannedTarget =
		TargetInventory->FindItemById(FirstSourceItemId);
	TestNotNull(TEXT("The first identity resolves as the planned target row"),
		FilledPlannedTarget);
	if (FilledPlannedTarget)
	{
		TestTrue(TEXT("Cross-actor planning reconstructs the first stack"),
			FilledPlannedTarget != FirstSourceStack);
		TestEqual(TEXT("The later merge fills the planned target row to ten"),
			TargetInventory->GetItemStackCount(FilledPlannedTarget), 10);
	}
	FRpgInventoryEntryView FilledPlannedTargetEntry;
	if (TestTrue(TEXT("The planned target row has a stable target entry"),
			FindEntry(
				TargetInventory,
				FirstSourceItemId,
				FilledPlannedTargetEntry)))
	{
		TestTrue(TEXT("The moved row receives a fresh target EntryId"),
			FilledPlannedTargetEntry.EntryId != FirstSourceBefore.EntryId);
		TestEqual(TEXT("The moved row claims the only free cell"),
			FilledPlannedTargetEntry.Placement.X, 0);
		TestEqual(TEXT("The moved row remains on the first target row"),
			FilledPlannedTargetEntry.Placement.Y, 0);
	}

	TestEqual(TEXT("The scratch-planned row is the sole affected target id"),
		AffectedTargetItemIds.Num(), 1);
	if (AffectedTargetItemIds.Num() == 1)
	{
		TestEqual(TEXT("The affected target keeps the first source identity"),
			AffectedTargetItemIds[0], FirstSourceItemId);
	}

	TSet<FRpgInventoryItemId> DeltaItemIds;
	bool bEveryDeltaItemIdIsUnique = true;
	for (const FRpgInventoryMutationDelta& Delta : Result.Deltas)
	{
		bEveryDeltaItemIdIsUnique &= Delta.ItemId.IsValid() &&
			!DeltaItemIds.Contains(Delta.ItemId);
		DeltaItemIds.Add(Delta.ItemId);
	}
	TestTrue(TEXT("No item id is emitted as both Added and Removed"),
		bEveryDeltaItemIdIsUnique);
	const FRpgInventoryMutationDelta* MovedDelta =
		Result.Deltas.FindByPredicate(
			[FirstSourceItemId](const FRpgInventoryMutationDelta& Delta)
			{
				return Delta.ItemId == FirstSourceItemId;
			});
	const FRpgInventoryMutationDelta* PartialSourceDelta =
		Result.Deltas.FindByPredicate(
			[LaterSourceItemId](const FRpgInventoryMutationDelta& Delta)
			{
				return Delta.ItemId == LaterSourceItemId;
			});
	if (TestNotNull(TEXT("The first row exposes one moved delta"), MovedDelta))
	{
		TestEqual(TEXT("The scratch-created row is reported as Moved"),
			MovedDelta->Kind, ERpgInventoryMutationDeltaKind::Moved);
		TestEqual(TEXT("The moved row starts with the first source quantity"),
			MovedDelta->PreviousQuantity, 8);
		TestEqual(TEXT("The moved row reports its final merged quantity"),
			MovedDelta->NewQuantity, 10);
	}
	if (TestNotNull(TEXT("The later root exposes one source delta"),
			PartialSourceDelta))
	{
		TestEqual(TEXT("The later root reports one stack change"),
			PartialSourceDelta->Kind,
			ERpgInventoryMutationDeltaKind::StackChanged);
		TestEqual(TEXT("The later root starts with five units"),
			PartialSourceDelta->PreviousQuantity, 5);
		TestEqual(TEXT("The later root reports its three-unit remainder"),
			PartialSourceDelta->NewQuantity, 3);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
