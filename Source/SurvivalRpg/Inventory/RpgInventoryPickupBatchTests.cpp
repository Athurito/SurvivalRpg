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
	Inventory->RemoveItemInstance(SameOwnerDetached);
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
	ForeignSetupInventory->RemoveItemInstance(ForeignTaggedDetached);
	ForeignSetupInventory->RemoveItemInstance(ForeignCompatibleDetached);
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

	TargetInventory->RemoveItemInstance(DuplicateIdCandidate);
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

#endif // WITH_DEV_AUTOMATION_TESTS
