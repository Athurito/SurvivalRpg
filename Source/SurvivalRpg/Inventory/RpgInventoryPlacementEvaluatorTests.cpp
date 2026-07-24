#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "RpgPlayerInventoryLayoutDefinition.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace RpgInventoryPlacementEvaluatorTests
{
	class FScopedInventoryWorld
	{
	public:
		FScopedInventoryWorld()
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

		URpgInventoryManagerComponent* CreateInventory(const TCHAR* DebugName)
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
			AActor* Owner = World->SpawnActor<AActor>(SpawnParameters);
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
						TEXT("Inventory")),
					RF_Transient);
			Owner->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

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
		int32 Y,
		bool bRotated = false)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(Container);
		Placement.X = X;
		Placement.Y = Y;
		Placement.bRotated = bRotated;
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

	FString MakeInventorySignature(
		const URpgInventoryManagerComponent* Inventory)
	{
		TArray<FString> Rows;
		if (!Inventory)
		{
			return FString();
		}

		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			Rows.Add(FString::Printf(
				TEXT("%s|%s|%d|%s|%d|%d|%d"),
				*Entry.EntryId.ToString(),
				*Entry.ItemId.ToString(),
				Entry.StackCount,
				*Entry.Placement.GetContainerHandle().ToString(),
				Entry.Placement.X,
				Entry.Placement.Y,
				Entry.Placement.bRotated ? 1 : 0));
		}
		Rows.Sort();
		return FString::Join(Rows, TEXT(";"));
	}

	FString MakePlacementPlanSignature(
		const FRpgInventoryPlacementPlan& Plan)
	{
		TArray<FString> Steps;
		Steps.Reserve(Plan.Steps.Num());
		for (const FRpgInventoryPlacementStep& Step : Plan.Steps)
		{
			const FRpgInventoryGridPlacement& Placement = Step.Placement;
			const FRpgInventoryGridPlacement& Displaced =
				Step.DisplacedPlacement;
			Steps.Add(FString::Printf(
				TEXT("%d|%s|%d|%d|%d|%d|%d|%d|%s|%s|%s|%s|%s|%d|%d|%d|%d|%d"),
				static_cast<int32>(Step.Resolution),
				*Placement.GetContainerHandle().ToString(),
				Placement.X,
				Placement.Y,
				Placement.Width,
				Placement.Height,
				Placement.bRotated ? 1 : 0,
				Step.Quantity,
				*Step.TargetItemId.ToString(),
				*Step.TargetEntryId.ToString(),
				*Step.DisplacedItemId.ToString(),
				*Step.DisplacedEntryId.ToString(),
				*Displaced.GetContainerHandle().ToString(),
				Displaced.X,
				Displaced.Y,
				Displaced.Width,
				Displaced.Height,
				Displaced.bRotated ? 1 : 0));
		}

		return FString::Printf(
			TEXT("%d|%d|%d|%d|%d|%s"),
			static_cast<int32>(Plan.Code),
			Plan.SourceRevision,
			Plan.TargetRevision,
			Plan.RequestedQuantity,
			Plan.AppliedQuantity,
			*FString::Join(Steps, TEXT(";")));
	}

	FRpgInventoryPlacementQuery MakeExactQuery(
		ERpgInventoryPlacementPurpose Purpose,
		const FRpgInventoryPlacementSubject& Subject,
		const FRpgInventoryGridPlacement& Placement)
	{
		FRpgInventoryPlacementQuery Query;
		Query.Purpose = Purpose;
		Query.Search = ERpgInventoryPlacementSearch::Exact;
		Query.Subject = Subject;
		Query.TargetContainer = Placement.GetContainerHandle();
		Query.ExactPlacement = Placement;
		return Query;
	}

	FRpgInventoryPlacementQuery MakeFirstFitQuery(
		ERpgInventoryPlacementPurpose Purpose,
		const FRpgInventoryPlacementSubject& Subject,
		const FRpgInventoryContainerHandle& Container)
	{
		FRpgInventoryPlacementQuery Query;
		Query.Purpose = Purpose;
		Query.Search = ERpgInventoryPlacementSearch::FirstFit;
		Query.Subject = Subject;
		Query.TargetContainer = Container;
		return Query;
	}

	bool InitializeTest(
		FAutomationTestBase& Test,
		FScopedInventoryWorld& TestWorld)
	{
		if (!TestWorld.IsValid())
		{
			Test.AddError(
				TEXT("Could not create an isolated placement-evaluator world."));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPlacementSubjectFactoryContractTest,
	"SurvivalRpg.Inventory.PlacementSubject.FactoryContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPlacementSubjectFactoryContractTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPlacementEvaluatorTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("PlacementSubjectFactoryInventory"));
	if (!TestNotNull(TEXT("The subject factory inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* OwnedItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4,
			MakePlacement(Root, 0, 0));
	FRpgInventoryEntryView OwnedEntry;
	if (!TestNotNull(TEXT("The owned subject item exists"), OwnedItem) ||
		!TestTrue(
			TEXT("The owned subject exposes a complete entry snapshot"),
			OwnedItem &&
				GetEntryView(
					Inventory,
					OwnedItem->GetItemId(),
					OwnedEntry)))
	{
		return false;
	}

	auto HasExactSourceSnapshot =
		[Inventory, &OwnedEntry](
			const FRpgInventoryPlacementSubject& Subject,
			ERpgInventoryPlacementSubjectKind ExpectedKind,
			int32 ExpectedQuantity)
		{
			return Subject.Kind == ExpectedKind &&
				Subject.SourceInventory == Inventory &&
				Subject.ItemInstance == OwnedEntry.Instance.Get() &&
				Subject.ItemDefinition ==
					OwnedEntry.Instance->GetItemDef() &&
				Subject.ItemId == OwnedEntry.ItemId &&
				Subject.ExpectedEntryId == OwnedEntry.EntryId &&
				Subject.ExpectedSourcePlacement == OwnedEntry.Placement &&
				Subject.ExpectedSourceQuantity == OwnedEntry.StackCount &&
				Subject.Quantity == ExpectedQuantity;
		};

	const FRpgInventoryPlacementSubject OwnedDefault =
		FRpgInventoryPlacementSubject::FromOwnedEntry(
			Inventory,
			OwnedEntry);
	TestTrue(
		TEXT("OwnedEntry records every source field and defaults to the full stack"),
		HasExactSourceSnapshot(
			OwnedDefault,
			ERpgInventoryPlacementSubjectKind::OwnedEntry,
			OwnedEntry.StackCount));

	constexpr int32 ExplicitOwnedQuantity = 2;
	const FRpgInventoryPlacementSubject OwnedExplicit =
		FRpgInventoryPlacementSubject::FromOwnedEntry(
			Inventory,
			OwnedEntry,
			ExplicitOwnedQuantity);
	TestTrue(
		TEXT("OwnedEntry preserves its full snapshot with an explicit quantity"),
		HasExactSourceSnapshot(
			OwnedExplicit,
			ERpgInventoryPlacementSubjectKind::OwnedEntry,
			ExplicitOwnedQuantity));

	constexpr int32 IncomingQuantity = 3;
	const FRpgInventoryPlacementSubject Incoming =
		FRpgInventoryPlacementSubject::FromIncomingInstance(
			Inventory,
			OwnedEntry,
			IncomingQuantity);
	TestTrue(
		TEXT("IncomingEntry changes only provenance and evaluated quantity"),
		HasExactSourceSnapshot(
			Incoming,
			ERpgInventoryPlacementSubjectKind::IncomingEntry,
			IncomingQuantity));

	constexpr int32 DefinitionQuantity = 5;
	const FRpgInventoryPlacementSubject Definition =
		FRpgInventoryPlacementSubject::FromDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			DefinitionQuantity);
	TestTrue(
		TEXT("DefinitionGrant records only definition provenance and quantity"),
		Definition.Kind ==
				ERpgInventoryPlacementSubjectKind::DefinitionGrant &&
			Definition.SourceInventory == nullptr &&
			Definition.ItemInstance == nullptr &&
			Definition.ItemDefinition ==
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass() &&
			!Definition.ItemId.IsValid() &&
			!Definition.ExpectedEntryId.IsValid() &&
			!Definition.ExpectedSourcePlacement.IsValid() &&
			Definition.ExpectedSourceQuantity == 0 &&
			Definition.Quantity == DefinitionQuantity);

	URpgInventoryItemInstance* DetachedItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 1, 0));
	if (!TestNotNull(TEXT("The detached subject item exists"), DetachedItem))
	{
		return false;
	}
	Inventory->RemoveItemInstance(DetachedItem);
	if (!TestFalse(
			TEXT("The subject item is detached before factory evaluation"),
			Inventory->ContainsItemInstance(DetachedItem)))
	{
		return false;
	}

	auto HasExactDetachedFields =
		[DetachedItem](
			const FRpgInventoryPlacementSubject& Subject,
			ERpgInventoryPlacementSubjectKind ExpectedKind,
			FRpgInventoryItemId ExpectedItemId,
			int32 ExpectedQuantity)
		{
			return Subject.Kind == ExpectedKind &&
				Subject.SourceInventory == nullptr &&
				Subject.ItemInstance == DetachedItem &&
				Subject.ItemDefinition == DetachedItem->GetItemDef() &&
				Subject.ItemId == ExpectedItemId &&
				!Subject.ExpectedEntryId.IsValid() &&
				!Subject.ExpectedSourcePlacement.IsValid() &&
				Subject.ExpectedSourceQuantity == 0 &&
				Subject.Quantity == ExpectedQuantity;
		};

	constexpr int32 DetachedQuantity = 2;
	const FRpgInventoryPlacementSubject Detached =
		FRpgInventoryPlacementSubject::FromDetachedInstance(
			DetachedItem,
			DetachedQuantity);
	TestTrue(
		TEXT("DetachedInstance records concrete identity without a source snapshot"),
		HasExactDetachedFields(
			Detached,
			ERpgInventoryPlacementSubjectKind::DetachedInstance,
			DetachedItem->GetItemId(),
			DetachedQuantity));

	constexpr int32 GeneratedQuantity = 3;
	const FRpgInventoryPlacementSubject Generated =
		FRpgInventoryPlacementSubject::FromGeneratedGrant(
			DetachedItem,
			GeneratedQuantity);
	TestTrue(
		TEXT("GeneratedGrant changes only detached provenance and quantity"),
		HasExactDetachedFields(
			Generated,
			ERpgInventoryPlacementSubjectKind::GeneratedGrant,
			DetachedItem->GetItemId(),
			GeneratedQuantity) &&
			Detached.Kind != Generated.Kind);

	const FRpgInventoryItemId StagedItemId =
		FRpgInventoryItemId::NewId();
	constexpr int32 StagedQuantity = 4;
	const FRpgInventoryPlacementSubject Staged =
		FRpgInventoryPlacementSubject::FromStagedRestore(
			DetachedItem,
			StagedItemId,
			StagedQuantity);
	TestTrue(
		TEXT("StagedRestore overrides the concrete instance item id"),
		HasExactDetachedFields(
			Staged,
			ERpgInventoryPlacementSubjectKind::StagedRestore,
			StagedItemId,
			StagedQuantity) &&
			Staged.ItemId != DetachedItem->GetItemId());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPlacementOperationMatrixTest,
	"SurvivalRpg.Inventory.PlacementEvaluator.OperationMatrix",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPlacementOperationMatrixTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPlacementEvaluatorTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("PlacementMatrixTarget"));
	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("PlacementMatrixSource"));
	if (!TestNotNull(TEXT("The target inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The source inventory exists"), SourceInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	const FRpgInventoryContainerHandle SourceRoot = MakeRoot(SourceInventory);
	URpgInventoryItemInstance* MergeTarget =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			8,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* MovingStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2,
			MakePlacement(Root, 1, 0));
	URpgInventoryItemInstance* SwapTarget =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	URpgInventoryItemInstance* IncomingStack =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			3,
			MakePlacement(SourceRoot, 0, 0));
	if (!TestNotNull(TEXT("The merge target exists"), MergeTarget) ||
		!TestNotNull(TEXT("The moving stack exists"), MovingStack) ||
		!TestNotNull(TEXT("The swap target exists"), SwapTarget) ||
		!TestNotNull(TEXT("The incoming stack exists"), IncomingStack))
	{
		return false;
	}

	FRpgInventoryEntryView MovingEntry;
	FRpgInventoryEntryView IncomingEntry;
	if (!TestTrue(
			TEXT("The moving stack exposes a complete snapshot"),
			GetEntryView(Inventory, MovingStack->GetItemId(), MovingEntry)) ||
		!TestTrue(
			TEXT("The incoming stack exposes a complete snapshot"),
			GetEntryView(
				SourceInventory,
				IncomingStack->GetItemId(),
				IncomingEntry)))
	{
		return false;
	}

	const FString TargetBefore = MakeInventorySignature(Inventory);
	const FString SourceBefore = MakeInventorySignature(SourceInventory);
	const int32 TargetRevisionBefore = Inventory->GetInventoryRevision();
	const int32 SourceRevisionBefore =
		SourceInventory->GetInventoryRevision();

	const FRpgInventoryPlacementSubject OwnedSubject =
		FRpgInventoryPlacementSubject::FromOwnedEntry(
			Inventory,
			MovingEntry);
	const FRpgInventoryPlacementPlan MoveMerge =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Move,
			OwnedSubject,
			MakePlacement(Root, 0, 0)));
	TestEqual(
		TEXT("Move selects compatible stack merging"),
		MoveMerge.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Move merge has one explicit step"), MoveMerge.Steps.Num(), 1);
	if (MoveMerge.Steps.IsValidIndex(0))
	{
		TestEqual(
			TEXT("Move reports Merge instead of leaving UI to infer it"),
			MoveMerge.Steps[0].Resolution,
			ERpgInventoryPlacementResolution::Merge);
		TestTrue(
			TEXT("Move merge identifies the concrete receiver"),
			MoveMerge.Steps[0].TargetItemId == MergeTarget->GetItemId());
	}

	const FRpgInventoryPlacementPlan EquipSwap =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Equip,
			OwnedSubject,
			MakePlacement(Root, 0, 0)));
	TestEqual(
		TEXT("Equip preserves concrete identities through a swap"),
		EquipSwap.Code,
		ERpgInventoryMutationResultCode::Success);
	if (EquipSwap.Steps.IsValidIndex(0))
	{
		TestEqual(
			TEXT("Equip never merges compatible equipment stacks"),
			EquipSwap.Steps[0].Resolution,
			ERpgInventoryPlacementResolution::Swap);
		TestTrue(
			TEXT("Equip identifies the displaced concrete item"),
			EquipSwap.Steps[0].DisplacedItemId == MergeTarget->GetItemId());
	}

	const FRpgInventoryPlacementPlan MoveSwap =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Move,
			OwnedSubject,
			MakePlacement(Root, 2, 0)));
	TestEqual(
		TEXT("Move accepts one unlike overlap as an atomic swap"),
		MoveSwap.Code,
		ERpgInventoryMutationResultCode::Success);
	if (MoveSwap.Steps.IsValidIndex(0))
	{
		TestEqual(
			TEXT("The unlike overlap resolves explicitly as Swap"),
			MoveSwap.Steps[0].Resolution,
			ERpgInventoryPlacementResolution::Swap);
	}

	const FRpgInventoryPlacementPlan SplitOccupied =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Split,
			FRpgInventoryPlacementSubject::FromOwnedEntry(
				Inventory,
				MovingEntry,
				1),
			MakePlacement(Root, 0, 0)));
	TestEqual(
		TEXT("Split requires a genuinely empty destination"),
		SplitOccupied.Code,
		ERpgInventoryMutationResultCode::Occupied);
	TestEqual(
		TEXT("Rejected split exposes no speculative step"),
		SplitOccupied.Steps.Num(),
		0);

	const FRpgInventoryPlacementPlan AddPlace =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Add,
			FRpgInventoryPlacementSubject::FromDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				1),
			MakePlacement(Root, 3, 0)));
	TestEqual(
		TEXT("Definition preflight can place into an empty exact cell"),
		AddPlace.Code,
		ERpgInventoryMutationResultCode::Success);
	if (AddPlace.Steps.IsValidIndex(0))
	{
		TestEqual(
			TEXT("Definition preflight reports Place"),
			AddPlace.Steps[0].Resolution,
			ERpgInventoryPlacementResolution::Place);
	}

	const FRpgInventoryPlacementSubject IncomingSubject =
		FRpgInventoryPlacementSubject::FromIncomingInstance(
			SourceInventory,
			IncomingEntry,
			1);
	const FRpgInventoryPlacementPlan TransferMerge =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Transfer,
			IncomingSubject,
			MakePlacement(Root, 0, 0)));
	TestEqual(
		TEXT("Transfer selects a compatible target stack"),
		TransferMerge.Code,
		ERpgInventoryMutationResultCode::Success);
	if (TransferMerge.Steps.IsValidIndex(0))
	{
		TestEqual(
			TEXT("Transfer exposes its merge resolution"),
			TransferMerge.Steps[0].Resolution,
			ERpgInventoryPlacementResolution::Merge);
	}

	const FRpgInventoryPlacementPlan TransferOccupied =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Transfer,
			IncomingSubject,
			MakePlacement(Root, 2, 0)));
	TestFalse(
		TEXT("Cross-inventory transfer never infers a reciprocal swap"),
		TransferOccupied.IsSuccess());
	TestEqual(
		TEXT("Rejected transfer exposes no swap step"),
		TransferOccupied.Steps.Num(),
		0);

	const FRpgInventoryPlacementSubject RestoreSubject =
		FRpgInventoryPlacementSubject::FromStagedRestore(
			IncomingStack,
			IncomingStack->GetItemId(),
			1);
	const FRpgInventoryPlacementPlan RestorePlace =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Restore,
			RestoreSubject,
			MakePlacement(Root, 4, 0)));
	TestEqual(
		TEXT("Restore accepts one exact empty placement"),
		RestorePlace.Code,
		ERpgInventoryMutationResultCode::Success);
	if (RestorePlace.Steps.IsValidIndex(0))
	{
		TestEqual(
			TEXT("Restore uses Place and never merge or swap"),
			RestorePlace.Steps[0].Resolution,
			ERpgInventoryPlacementResolution::Place);
	}
	const FRpgInventoryPlacementPlan RestoreOccupied =
		Inventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Restore,
			RestoreSubject,
			MakePlacement(Root, 0, 0)));
	TestEqual(
		TEXT("Restore rejects an occupied exact placement"),
		RestoreOccupied.Code,
		ERpgInventoryMutationResultCode::Occupied);

	TestEqual(
		TEXT("Every placement evaluation leaves the target graph unchanged"),
		MakeInventorySignature(Inventory),
		TargetBefore);
	TestEqual(
		TEXT("Every placement evaluation leaves the source graph unchanged"),
		MakeInventorySignature(SourceInventory),
		SourceBefore);
	TestEqual(
		TEXT("Read-only evaluation does not advance the target revision"),
		Inventory->GetInventoryRevision(),
		TargetRevisionBefore);
	TestEqual(
		TEXT("Read-only evaluation does not advance the source revision"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPlacementPublicPreflightParityAndPurityTest,
	"SurvivalRpg.Inventory.PlacementEvaluator.PublicPreflightParityAndPurity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPlacementPublicPreflightParityAndPurityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPlacementEvaluatorTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("PlacementPublicPreflightTarget"));
	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("PlacementPublicPreflightSource"));
	if (!TestNotNull(TEXT("The preflight target exists"), TargetInventory) ||
		!TestNotNull(TEXT("The preflight source exists"), SourceInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle TargetRoot = MakeRoot(TargetInventory);
	const FRpgInventoryContainerHandle SourceRoot = MakeRoot(SourceInventory);
	URpgInventoryItemInstance* MergeTarget =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			8,
			MakePlacement(TargetRoot, 0, 0));
	URpgInventoryItemInstance* IncomingItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakePlacement(SourceRoot, 0, 0));
	URpgInventoryItemInstance* GeneratedAddProbe =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakePlacement(SourceRoot, 1, 0));
	if (!TestNotNull(TEXT("The compatible merge target exists"), MergeTarget) ||
		!TestNotNull(TEXT("The incoming source item exists"), IncomingItem) ||
		!TestNotNull(TEXT("The generated add probe exists"), GeneratedAddProbe))
	{
		return false;
	}

	SourceInventory->RemoveItemInstance(GeneratedAddProbe);
	if (!TestFalse(
			TEXT("The generated add probe is detached before read-only planning"),
			SourceInventory->ContainsItemInstance(GeneratedAddProbe)))
	{
		return false;
	}

	FRpgInventoryEntryView IncomingEntry;
	if (!TestTrue(
			TEXT("The incoming item exposes a complete source snapshot"),
			GetEntryView(
				SourceInventory,
				IncomingItem->GetItemId(),
				IncomingEntry)))
	{
		return false;
	}

	FRpgInventoryStackKey MergeTargetKeyBefore;
	FRpgInventoryStackKey IncomingKeyBefore;
	FRpgInventoryStackKey AddProbeKeyBefore;
	if (!TestTrue(
			TEXT("The merge target exposes canonical item state"),
			MergeTarget->TryBuildStackKey(MergeTargetKeyBefore)) ||
		!TestTrue(
			TEXT("The incoming item exposes canonical item state"),
			IncomingItem->TryBuildStackKey(IncomingKeyBefore)) ||
		!TestTrue(
			TEXT("The detached add probe exposes canonical item state"),
			GeneratedAddProbe->TryBuildStackKey(AddProbeKeyBefore)))
	{
		return false;
	}

	const FString TargetGraphBefore =
		MakeInventorySignature(TargetInventory);
	const FString SourceGraphBefore =
		MakeInventorySignature(SourceInventory);
	const int32 TargetRevisionBefore =
		TargetInventory->GetInventoryRevision();
	const int32 SourceRevisionBefore =
		SourceInventory->GetInventoryRevision();
	const uint64 TargetEpochBefore = TargetInventory->GetMutationEpoch();
	const uint64 SourceEpochBefore = SourceInventory->GetMutationEpoch();
	UObject* const MergeTargetOuterBefore = MergeTarget->GetOuter();
	UObject* const IncomingOuterBefore = IncomingItem->GetOuter();
	UObject* const AddProbeOuterBefore = GeneratedAddProbe->GetOuter();
	const FRpgInventoryItemId MergeTargetIdBefore =
		MergeTarget->GetItemId();
	const FRpgInventoryItemId IncomingIdBefore = IncomingItem->GetItemId();
	const FRpgInventoryItemId AddProbeIdBefore =
		GeneratedAddProbe->GetItemId();
	const TSubclassOf<URpgInventoryItemDefinition> MergeTargetDefBefore =
		MergeTarget->GetItemDef();
	const TSubclassOf<URpgInventoryItemDefinition> IncomingDefBefore =
		IncomingItem->GetItemDef();
	const TSubclassOf<URpgInventoryItemDefinition> AddProbeDefBefore =
		GeneratedAddProbe->GetItemDef();
	const int32 MergeTargetCountBefore =
		TargetInventory->GetItemStackCount(MergeTarget);
	const int32 IncomingCountBefore =
		SourceInventory->GetItemStackCount(IncomingItem);

	constexpr int32 AddQuantity = 3;
	const FRpgInventoryPlacementSubject AddSubject =
		FRpgInventoryPlacementSubject::FromGeneratedGrant(
			GeneratedAddProbe,
			AddQuantity);
	const FRpgInventoryPlacementQuery AddFirstFitQuery =
		MakeFirstFitQuery(
			ERpgInventoryPlacementPurpose::Add,
			AddSubject,
			FRpgInventoryContainerHandle());
	const FRpgInventoryPlacementPlan AddFirstFitPlan =
		TargetInventory->EvaluatePlacement(AddFirstFitQuery);
	const FRpgInventoryPlacementPlan AddFirstFitRepeat =
		TargetInventory->EvaluatePlacement(AddFirstFitQuery);
	TestTrue(
		TEXT("FirstFit add planning accepts the complete definition quantity"),
		AddFirstFitPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("CanAddItemDefinition matches the shared FirstFit evaluator"),
		TargetInventory->CanAddItemDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			AddQuantity),
		AddFirstFitPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("Repeated FirstFit add planning is deterministic"),
		MakePlacementPlanSignature(AddFirstFitRepeat),
		MakePlacementPlanSignature(AddFirstFitPlan));
	TestEqual(
		TEXT("FirstFit add merges then creates one remaining entry"),
		AddFirstFitPlan.Steps.Num(),
		2);
	if (AddFirstFitPlan.Steps.Num() == 2)
	{
		TestEqual(
			TEXT("FirstFit add fills the compatible stack first"),
			AddFirstFitPlan.Steps[0].Resolution,
			ERpgInventoryPlacementResolution::Merge);
		TestEqual(
			TEXT("FirstFit add merges exactly the free stack capacity"),
			AddFirstFitPlan.Steps[0].Quantity,
			2);
		TestEqual(
			TEXT("FirstFit add places the remaining quantity"),
			AddFirstFitPlan.Steps[1].Resolution,
			ERpgInventoryPlacementResolution::Place);
		TestEqual(
			TEXT("FirstFit add needs one placed unit after merging"),
			AddFirstFitPlan.Steps[1].Quantity,
			1);
	}

	const FRpgInventoryGridPlacement ExactAddPlacement =
		MakePlacement(TargetRoot, 3, 0);
	const FRpgInventoryPlacementQuery ExactAddQuery =
		MakeExactQuery(
			ERpgInventoryPlacementPurpose::Add,
			FRpgInventoryPlacementSubject::FromGeneratedGrant(
				GeneratedAddProbe,
				1),
			ExactAddPlacement);
	const FRpgInventoryPlacementPlan ExactAddPlan =
		TargetInventory->EvaluatePlacement(ExactAddQuery);
	const FRpgInventoryPlacementPlan ExactAddRepeat =
		TargetInventory->EvaluatePlacement(ExactAddQuery);
	TestTrue(
		TEXT("Exact add planning accepts the empty requested cell"),
		ExactAddPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("CanAddItemDefinitionToPlacement matches the shared Exact evaluator"),
		TargetInventory->CanAddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			ExactAddPlacement),
		ExactAddPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("Repeated Exact add planning is deterministic"),
		MakePlacementPlanSignature(ExactAddRepeat),
		MakePlacementPlanSignature(ExactAddPlan));

	const int32 RequiredDefinitionEntries =
		TargetInventory->GetRequiredNewEntryCountForItemDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			AddQuantity);
	TestEqual(
		TEXT("Definition entry preflight accounts for the two-unit merge"),
		RequiredDefinitionEntries,
		1);
	TestEqual(
		TEXT("Repeated definition entry preflight is deterministic"),
		TargetInventory->GetRequiredNewEntryCountForItemDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			AddQuantity),
		RequiredDefinitionEntries);
	const int32 RequiredInstanceEntries =
		TargetInventory->GetRequiredNewEntryCountForItemInstance(
			GeneratedAddProbe,
			1);
	TestEqual(
		TEXT("A concrete detached instance requires one identity-preserving entry"),
		RequiredInstanceEntries,
		1);
	TestEqual(
		TEXT("Repeated instance entry preflight is deterministic"),
		TargetInventory->GetRequiredNewEntryCountForItemInstance(
			GeneratedAddProbe,
			1),
		RequiredInstanceEntries);

	const FRpgInventoryPlacementSubject IncomingSubject =
		FRpgInventoryPlacementSubject::FromIncomingInstance(
			SourceInventory,
			IncomingEntry,
			1);
	const FRpgInventoryPlacementQuery ReceiveFirstFitQuery =
		MakeFirstFitQuery(
			ERpgInventoryPlacementPurpose::Transfer,
			IncomingSubject,
			FRpgInventoryContainerHandle());
	const FRpgInventoryPlacementPlan ReceiveFirstFitPlan =
		TargetInventory->EvaluatePlacement(ReceiveFirstFitQuery);
	const FRpgInventoryPlacementPlan ReceiveFirstFitRepeat =
		TargetInventory->EvaluatePlacement(ReceiveFirstFitQuery);
	TestTrue(
		TEXT("FirstFit receive planning accepts the complete incoming quantity"),
		ReceiveFirstFitPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("CanReceiveTransferredItemInstance matches the shared FirstFit evaluator"),
		TargetInventory->CanReceiveTransferredItemInstance(IncomingItem, 1),
		ReceiveFirstFitPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("Repeated FirstFit receive planning is deterministic"),
		MakePlacementPlanSignature(ReceiveFirstFitRepeat),
		MakePlacementPlanSignature(ReceiveFirstFitPlan));
	if (ReceiveFirstFitPlan.Steps.IsValidIndex(0))
	{
		TestEqual(
			TEXT("FirstFit receive selects the compatible merge target"),
			ReceiveFirstFitPlan.Steps[0].Resolution,
			ERpgInventoryPlacementResolution::Merge);
	}

	const FRpgInventoryGridPlacement ExactReceivePlacement =
		MakePlacement(TargetRoot, 4, 0);
	const FRpgInventoryPlacementQuery ExactReceiveQuery =
		MakeExactQuery(
			ERpgInventoryPlacementPurpose::Transfer,
			IncomingSubject,
			ExactReceivePlacement);
	const FRpgInventoryPlacementPlan ExactReceivePlan =
		TargetInventory->EvaluatePlacement(ExactReceiveQuery);
	const FRpgInventoryPlacementPlan ExactReceiveRepeat =
		TargetInventory->EvaluatePlacement(ExactReceiveQuery);
	const bool bExactReceiveIsOnePlace =
		ExactReceivePlan.IsCompleteSuccess() &&
		ExactReceivePlan.Steps.Num() == 1 &&
		ExactReceivePlan.Steps[0].Resolution ==
			ERpgInventoryPlacementResolution::Place;
	TestTrue(
		TEXT("Exact receive planning selects one empty placement"),
		bExactReceiveIsOnePlace);
	TestEqual(
		TEXT("CanReceiveTransferredItemInstanceToPlacement matches the shared Exact evaluator"),
		TargetInventory->CanReceiveTransferredItemInstanceToPlacement(
			IncomingItem,
			1,
			ExactReceivePlacement),
		bExactReceiveIsOnePlace);
	TestEqual(
		TEXT("Repeated Exact receive planning is deterministic"),
		MakePlacementPlanSignature(ExactReceiveRepeat),
		MakePlacementPlanSignature(ExactReceivePlan));

	TestEqual(
		TEXT("Public preflights preserve the complete target graph"),
		MakeInventorySignature(TargetInventory),
		TargetGraphBefore);
	TestEqual(
		TEXT("Public preflights preserve the complete source graph"),
		MakeInventorySignature(SourceInventory),
		SourceGraphBefore);
	TestTrue(
		TEXT("Public preflights do not advance either graph revision"),
		TargetInventory->GetInventoryRevision() == TargetRevisionBefore &&
			SourceInventory->GetInventoryRevision() == SourceRevisionBefore);
	TestTrue(
		TEXT("Public preflights do not advance either mutation epoch"),
		TargetInventory->GetMutationEpoch() == TargetEpochBefore &&
			SourceInventory->GetMutationEpoch() == SourceEpochBefore);
	TestTrue(
		TEXT("Every concrete item keeps its original UObject outer"),
		MergeTarget->GetOuter() == MergeTargetOuterBefore &&
			IncomingItem->GetOuter() == IncomingOuterBefore &&
			GeneratedAddProbe->GetOuter() == AddProbeOuterBefore);
	TestTrue(
		TEXT("Every concrete item keeps its persistent identity"),
		MergeTarget->GetItemId() == MergeTargetIdBefore &&
			IncomingItem->GetItemId() == IncomingIdBefore &&
			GeneratedAddProbe->GetItemId() == AddProbeIdBefore);
	TestTrue(
		TEXT("Every concrete item keeps its static definition"),
		MergeTarget->GetItemDef() == MergeTargetDefBefore &&
			IncomingItem->GetItemDef() == IncomingDefBefore &&
			GeneratedAddProbe->GetItemDef() == AddProbeDefBefore);
	TestTrue(
		TEXT("Both managed stack quantities remain unchanged"),
		TargetInventory->GetItemStackCount(MergeTarget) ==
				MergeTargetCountBefore &&
			SourceInventory->GetItemStackCount(IncomingItem) ==
				IncomingCountBefore);

	FRpgInventoryStackKey MergeTargetKeyAfter;
	FRpgInventoryStackKey IncomingKeyAfter;
	FRpgInventoryStackKey AddProbeKeyAfter;
	TestTrue(
		TEXT("Every item still exposes canonical runtime state"),
		MergeTarget->TryBuildStackKey(MergeTargetKeyAfter) &&
			IncomingItem->TryBuildStackKey(IncomingKeyAfter) &&
			GeneratedAddProbe->TryBuildStackKey(AddProbeKeyAfter));
	TestTrue(
		TEXT("Every item's canonical runtime state remains unchanged"),
		MergeTargetKeyAfter == MergeTargetKeyBefore &&
			IncomingKeyAfter == IncomingKeyBefore &&
			AddProbeKeyAfter == AddProbeKeyBefore);
	TestTrue(
		TEXT("Target, source, and detached ownership remain unchanged"),
		TargetInventory->ContainsItemInstance(MergeTarget) &&
			!SourceInventory->ContainsItemInstance(MergeTarget) &&
			SourceInventory->ContainsItemInstance(IncomingItem) &&
			!TargetInventory->ContainsItemInstance(IncomingItem) &&
			!SourceInventory->ContainsItemInstance(GeneratedAddProbe) &&
			!TargetInventory->ContainsItemInstance(GeneratedAddProbe));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPlacementIgnoredNonOverlapCapacityTest,
	"SurvivalRpg.Inventory.PlacementEvaluator.IgnoredNonOverlapDoesNotBypassCapacity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPlacementIgnoredNonOverlapCapacityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPlacementEvaluatorTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("PlacementIgnoredCapacityTarget"));
	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("PlacementIgnoredCapacitySource"));
	if (!TestNotNull(TEXT("The capacity target exists"), TargetInventory) ||
		!TestNotNull(TEXT("The capacity source exists"), SourceInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle TargetRoot = MakeRoot(TargetInventory);
	URpgInventoryItemInstance* IgnoredItem =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(TargetRoot, 0, 0));
	URpgInventoryItemInstance* IncomingItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(MakeRoot(SourceInventory), 0, 0));
	URpgInventoryItemInstance* DetachedIncomingItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(MakeRoot(SourceInventory), 1, 0));
	if (!TestNotNull(TEXT("The ignored target item exists"), IgnoredItem) ||
		!TestNotNull(TEXT("The incoming source item exists"), IncomingItem) ||
		!TestNotNull(
			TEXT("The detached incoming probe exists"),
			DetachedIncomingItem))
	{
		return false;
	}
	SourceInventory->RemoveItemInstance(DetachedIncomingItem);
	TestFalse(
		TEXT("The detached incoming probe is no longer source-managed"),
		SourceInventory->ContainsItemInstance(DetachedIncomingItem));

	TargetInventory->SetFixedMaxEntries(1);
	TargetInventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	const FRpgInventoryGridPlacement EmptyPlacement =
		MakePlacement(TargetRoot, 2, 0);
	TestEqual(
		TEXT("The target entry budget is full"),
		TargetInventory->GetFreeEntryCount(),
		0);
	TestNull(
		TEXT("The requested cell is empty and does not replace the ignored item"),
		TargetInventory->GetItemAtContainerCell(TargetRoot, 2, 0));

	const FString TargetGraphBefore =
		MakeInventorySignature(TargetInventory);
	const FString SourceGraphBefore =
		MakeInventorySignature(SourceInventory);
	TestTrue(
		TEXT("The overlapping ignored item can be replaced at full capacity"),
		TargetInventory
			->CanReceiveTransferredItemInstanceToPlacementIgnoringItem(
				IncomingItem,
				1,
				MakePlacement(TargetRoot, 0, 0),
				IgnoredItem));
	TestFalse(
		TEXT("A detached item cannot masquerade as an incoming transfer"),
		TargetInventory
			->CanReceiveTransferredItemInstanceToPlacementIgnoringItem(
				DetachedIncomingItem,
				1,
				MakePlacement(TargetRoot, 0, 0),
				IgnoredItem));
	TestFalse(
		TEXT("A non-overlapping ignored item cannot bypass full target capacity"),
		TargetInventory
			->CanReceiveTransferredItemInstanceToPlacementIgnoringItem(
				IncomingItem,
				1,
				EmptyPlacement,
				IgnoredItem));
	TestEqual(
		TEXT("Rejected capacity preflight preserves the target graph"),
		MakeInventorySignature(TargetInventory),
		TargetGraphBefore);
	TestEqual(
		TEXT("Rejected capacity preflight preserves the source graph"),
		MakeInventorySignature(SourceInventory),
		SourceGraphBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPlacementRotationAndRuntimeIdentityTest,
	"SurvivalRpg.Inventory.PlacementEvaluator.RotationAndRuntimeIdentity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPlacementRotationAndRuntimeIdentityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPlacementEvaluatorTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("PlacementRotationInventory"));
	URpgInventoryManagerComponent* GrantInventory =
		TestWorld.CreateInventory(TEXT("PlacementGrantInventory"));
	if (!TestNotNull(TEXT("The rotation inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The grant inventory exists"), GrantInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* Bag =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("The rotation bag exists"), Bag))
	{
		return false;
	}

	const FRpgInventoryContainerHandle BagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			Bag->GetItemId(),
			TEXT("Main"),
			1);
	bool bFilled = true;
	for (int32 Y = 0; Y < 4; ++Y)
	{
		for (int32 X = 0; X < 4; ++X)
		{
			if (X == 3 && (Y == 2 || Y == 3))
			{
				continue;
			}
			bFilled &= Inventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				1,
				MakePlacement(BagContents, X, Y)) != nullptr;
		}
	}
	if (!TestTrue(TEXT("Only one vertical two-cell gap remains"), bFilled))
	{
		return false;
	}

	const FString BeforeRotationPlan = MakeInventorySignature(Inventory);
	const FRpgInventoryPlacementPlan RotationPlan =
		Inventory->EvaluatePlacement(MakeFirstFitQuery(
			ERpgInventoryPlacementPurpose::Add,
			FRpgInventoryPlacementSubject::FromDefinition(
				URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
				1),
			BagContents));
	TestEqual(
		TEXT("FirstFit rotates when no unrotated footprint fits"),
		RotationPlan.Code,
		ERpgInventoryMutationResultCode::Success);
	if (RotationPlan.Steps.IsValidIndex(0))
	{
		const FRpgInventoryGridPlacement& Placement =
			RotationPlan.Steps[0].Placement;
		TestEqual(TEXT("The deterministic gap starts at X three"), Placement.X, 3);
		TestEqual(TEXT("The deterministic gap starts at Y two"), Placement.Y, 2);
		TestTrue(TEXT("The 2x1 footprint is rotated into the vertical gap"), Placement.bRotated);
		TestEqual(
			TEXT("Rotation preserves the authored width"),
			Placement.Width,
			2);
		TestEqual(
			TEXT("Rotation preserves the authored height"),
			Placement.Height,
			1);
	}
	TestEqual(
		TEXT("FirstFit planning is side-effect-free"),
		MakeInventorySignature(Inventory),
		BeforeRotationPlan);

	const FRpgInventoryContainerHandle GrantRoot = MakeRoot(GrantInventory);
	URpgInventoryItemInstance* RuntimeDistinctStack =
		GrantInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			5,
			MakePlacement(GrantRoot, 0, 0));
	if (!TestNotNull(
		TEXT("The runtime-distinct target stack exists"),
		RuntimeDistinctStack))
	{
		return false;
	}
	RuntimeDistinctStack->AddStatTagStack(
		RpgGameplayTags::Ability_Attack_Basic,
		1);

	URpgInventoryItemInstance* FreshDefaultStack =
		GrantInventory->GrantItemDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2);
	if (!TestNotNull(
		TEXT("A default-state grant creates a concrete stack"),
		FreshDefaultStack))
	{
		return false;
	}
	TestTrue(
		TEXT("Same definition with different runtime state does not merge"),
		FreshDefaultStack != RuntimeDistinctStack);
	TestEqual(
		TEXT("The runtime-distinct stack retains its quantity"),
		GrantInventory->GetItemStackCount(RuntimeDistinctStack),
		5);
	TestEqual(
		TEXT("The fresh default-state stack receives the grant"),
		GrantInventory->GetItemStackCount(FreshDefaultStack),
		2);

	URpgInventoryItemInstance* CompatibleMergeResult =
		GrantInventory->GrantItemDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2);
	TestEqual(
		TEXT("A later compatible grant returns the concrete merge receiver"),
		CompatibleMergeResult,
		FreshDefaultStack);
	TestEqual(
		TEXT("Compatible default-state grants still merge"),
		GrantInventory->GetItemStackCount(FreshDefaultStack),
		4);
	TestEqual(
		TEXT("The incompatible stack remains isolated after later grants"),
		GrantInventory->GetItemStackCount(RuntimeDistinctStack),
		5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPlacementHandleAndSubtreeCapacityTest,
	"SurvivalRpg.Inventory.PlacementEvaluator.HandleAndSubtreeCapacity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPlacementHandleAndSubtreeCapacityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPlacementEvaluatorTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* CollisionInventory =
		TestWorld.CreateInventory(TEXT("PlacementHandleCollision"));
	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("PlacementSubtreeSource"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("PlacementSubtreeTarget"));
	if (!TestNotNull(TEXT("The handle-collision inventory exists"), CollisionInventory) ||
		!TestNotNull(TEXT("The subtree source exists"), SourceInventory) ||
		!TestNotNull(TEXT("The subtree target exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle CollisionRoot =
		MakeRoot(CollisionInventory);
	URpgInventoryItemInstance* CollisionBag =
		CollisionInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestGearNameCollisionBagItemDefinition::StaticClass(),
			1,
			MakePlacement(CollisionRoot, 0, 0));
	if (!TestNotNull(
		TEXT("The local-name collision provider exists"),
		CollisionBag))
	{
		return false;
	}

	const FRpgInventoryContainerHandle CollidingContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			CollisionBag->GetItemId(),
			TEXT("Gear.Head"),
			1);
	const FRpgInventoryContainerHandle LegacyNamedGearHead =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::GearHeadGroupId);

	FActorSpawnParameters LayoutControllerParameters;
	LayoutControllerParameters.Name = MakeUniqueObjectName(
		TestWorld.GetWorld(),
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("PlacementHandleLayoutController"));
	LayoutControllerParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* LayoutController =
		TestWorld.GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			LayoutControllerParameters);

	FActorSpawnParameters LayoutPlayerStateParameters;
	LayoutPlayerStateParameters.Name = MakeUniqueObjectName(
		TestWorld.GetWorld(),
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("PlacementHandleLayoutPlayerState"));
	LayoutPlayerStateParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* LayoutPlayerState =
		TestWorld.GetWorld()->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			LayoutPlayerStateParameters);
	if (!TestNotNull(TEXT("The layout controller fixture exists"), LayoutController) ||
		!TestNotNull(TEXT("The layout PlayerState fixture exists"), LayoutPlayerState))
	{
		return false;
	}

	LayoutController->SetPlayerState(LayoutPlayerState);
	LayoutPlayerState->SetOwner(LayoutController);
	const URpgPawnData* LayoutPawnData =
		LayoutPlayerState->GetPawnData<URpgPawnData>();
	URpgPlayerInventoryLayoutDefinition* LayoutDefinition =
		LayoutPlayerState->GetMutableTestInventoryLayoutDefinition();
	const URpgPlayerInventoryLayoutComponent* DefaultLayout =
		LayoutController->GetPlayerInventoryLayoutComponent();
	if (!TestNotNull(TEXT("The fixture PawnData exists"), LayoutPawnData) ||
		!TestNotNull(TEXT("The fixture layout definition exists"), LayoutDefinition) ||
		!TestNotNull(TEXT("The default player inventory layout exists"), DefaultLayout))
	{
		return false;
	}

	FRpgInventorySlotGroupDefinition* HeadGroup =
		LayoutDefinition->StaticSlotGroups.FindByPredicate(
			[](const FRpgInventorySlotGroupDefinition& Group)
			{
				return Group.GroupKind ==
						ERpgInventorySlotGroupKind::Gear &&
					Group.EquipmentSlotRole ==
						ERpgEquipmentSlot::Head;
			});
	if (!TestNotNull(
			TEXT("The fixture layout owns an explicitly typed Head group"),
			HeadGroup))
	{
		return false;
	}

	const FName RenamedHeadContainerId(TEXT("Designer.HeadSlot"));
	HeadGroup->ContainerId = RenamedHeadContainerId;
	const FRpgInventoryContainerHandle RootGearHead =
		FRpgInventoryContainerHandle::MakeRoot(
			RenamedHeadContainerId);
	TestTrue(
		TEXT("The renamed Head root is classified from its typed role"),
		DefaultLayout->IsGearContainer(RootGearHead));
	TestFalse(
		TEXT("The old Gear.Head root no longer inherits Gear semantics from its name"),
		DefaultLayout->IsGearContainer(LegacyNamedGearHead));
	TestFalse(
		TEXT("An item-owned Gear.Head handle does not inherit Gear semantics from its local id"),
		DefaultLayout->IsGearContainer(CollidingContents));
	TestTrue(
		TEXT("The authored fixture starts with an unambiguous static equipment contract"),
		DefaultLayout->HasValidStaticEquipmentRoleContract());

	ERpgEquipmentSlot RootEquipmentSlot = ERpgEquipmentSlot::None;
	TestTrue(
		TEXT("The renamed Head root resolves to an equipment slot"),
		DefaultLayout->TryGetEquipmentSlotForGearContainer(
			RootGearHead,
			RootEquipmentSlot));
	TestEqual(
		TEXT("The renamed root retains its explicit Head role"),
		RootEquipmentSlot,
		ERpgEquipmentSlot::Head);

	ERpgEquipmentSlot CollidingEquipmentSlot =
		ERpgEquipmentSlot::Head;
	TestFalse(
		TEXT("The item-owned Gear.Head handle cannot resolve as equipment"),
		DefaultLayout->TryGetEquipmentSlotForGearContainer(
			CollidingContents,
			CollidingEquipmentSlot));
	TestEqual(
		TEXT("Rejected item-owned aliases leave no equipment role"),
		CollidingEquipmentSlot,
		ERpgEquipmentSlot::None);

	FRpgInventorySlotAddress RootGearAddress;
	FRpgInventoryGridPlacement RootGearPlacement;
	if (!TestEqual(
			TEXT("The fixture layout definition contains all static groups"),
			LayoutDefinition->StaticSlotGroups.Num(),
			13) ||
		!TestEqual(
			TEXT("The component resolves all fixture layout groups"),
			DefaultLayout->GetSlotGroups().Num(),
			13) ||
		!TestTrue(
			TEXT("The Head equipment role produces the renamed root address"),
			DefaultLayout->TryMakeGearSlotAddress(
				ERpgEquipmentSlot::Head,
				RootGearAddress)) ||
		!TestEqual(
			TEXT("The canonical Head address keeps the complete root handle"),
			RootGearAddress.GetContainerHandle(),
			RootGearHead) ||
		!TestTrue(
			TEXT("The renamed Head address normalizes to a layout placement"),
			DefaultLayout->ResolveSlotAddress(
				RootGearAddress,
				RootGearPlacement)))
	{
		return false;
	}
	TestEqual(
		TEXT("Renamed Head normalization keeps the exact root handle"),
		RootGearPlacement.GetContainerHandle(),
		RootGearHead);
	TestEqual(
		TEXT("Root Gear.Head normalization uses single-cell width"),
		RootGearPlacement.Width,
		1);
	TestEqual(
		TEXT("Root Gear.Head normalization uses single-cell height"),
		RootGearPlacement.Height,
		1);
	TestFalse(
		TEXT("Renamed Head single-cell normalization disables rotation"),
		RootGearPlacement.bRotated);

	FRpgInventorySlotGroupDefinition DuplicateHeadGroup = *HeadGroup;
	DuplicateHeadGroup.ContainerId = TEXT("Designer.HeadSlot.Duplicate");
	LayoutDefinition->StaticSlotGroups.Add(DuplicateHeadGroup);
	TestFalse(
		TEXT("A duplicate typed Gear role invalidates the static equipment contract"),
		DefaultLayout->HasValidStaticEquipmentRoleContract());
	AddExpectedError(
		TEXT("Inventory Gear role"),
		EAutomationExpectedErrorFlags::Contains,
		2);

	FRpgInventorySlotAddress AmbiguousHeadAddress;
	TestFalse(
		TEXT("A duplicate Head role prevents slot-to-address resolution"),
		DefaultLayout->TryMakeGearSlotAddress(
			ERpgEquipmentSlot::Head,
			AmbiguousHeadAddress));
	ERpgEquipmentSlot AmbiguousHeadRole =
		ERpgEquipmentSlot::None;
	TestFalse(
		TEXT("A duplicate Head role prevents address-to-slot resolution"),
		DefaultLayout->TryGetEquipmentSlotRoleForAddress(
			RootGearAddress,
			AmbiguousHeadRole));
	TestEqual(
		TEXT("Ambiguous Gear lookup fails without leaking a role"),
		AmbiguousHeadRole,
		ERpgEquipmentSlot::None);
	LayoutDefinition->StaticSlotGroups.RemoveAt(
		LayoutDefinition->StaticSlotGroups.Num() - 1);
	TestTrue(
		TEXT("Removing the duplicate Gear role restores the static equipment contract"),
		DefaultLayout->HasValidStaticEquipmentRoleContract());

	const FRpgInventorySlotGroupDefinition* RestoredHeadGroup =
		LayoutDefinition->StaticSlotGroups.FindByPredicate(
			[](const FRpgInventorySlotGroupDefinition& Group)
			{
				return Group.GroupKind ==
						ERpgInventorySlotGroupKind::Gear &&
					Group.EquipmentSlotRole ==
						ERpgEquipmentSlot::Head;
			});
	if (!TestNotNull(
			TEXT("The renamed Head definition remains available for duplicate-id coverage"),
			RestoredHeadGroup))
	{
		return false;
	}

	FRpgInventorySlotGroupDefinition DuplicateContainerGroup =
		*RestoredHeadGroup;
	LayoutDefinition->StaticSlotGroups.Add(DuplicateContainerGroup);
	TestFalse(
		TEXT("A duplicate static ContainerId invalidates the equipment contract before view filtering"),
		DefaultLayout->HasValidStaticEquipmentRoleContract());
	AddExpectedError(
		TEXT("Duplicate inventory container id"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	const TArray<FRpgInventorySlotGroupView> DuplicateIdGroups =
		DefaultLayout->GetSlotGroups();
	TestFalse(
		TEXT("Both definitions with a duplicate ContainerId are rejected instead of keeping the first"),
		DuplicateIdGroups.ContainsByPredicate(
			[RootGearHead](const FRpgInventorySlotGroupView& Group)
			{
				return Group.ContainerHandle == RootGearHead;
			}));
	LayoutDefinition->StaticSlotGroups.RemoveAt(
		LayoutDefinition->StaticSlotGroups.Num() - 1);
	TestTrue(
		TEXT("Removing the duplicate ContainerId restores the equipment contract"),
		DefaultLayout->HasValidStaticEquipmentRoleContract());

	FRpgInventorySlotGroupDefinition* MalformedHeadGroup =
		LayoutDefinition->StaticSlotGroups.FindByPredicate(
			[](const FRpgInventorySlotGroupDefinition& Group)
			{
				return Group.GroupKind ==
						ERpgInventorySlotGroupKind::Gear &&
					Group.EquipmentSlotRole ==
						ERpgEquipmentSlot::Head;
			});
	if (!TestNotNull(
			TEXT("The Head definition remains available for malformed-contract coverage"),
			MalformedHeadGroup))
	{
		return false;
	}
	MalformedHeadGroup->GridSize.Width = 2;
	TestFalse(
		TEXT("A multi-cell Gear definition invalidates destructive-operation preflight"),
		DefaultLayout->HasValidStaticEquipmentRoleContract());
	MalformedHeadGroup->GridSize.Width = 1;
	TestTrue(
		TEXT("Restoring the single-cell Gear contract passes preflight again"),
		DefaultLayout->HasValidStaticEquipmentRoleContract());

	FRpgInventoryGridSize CollidingGridSize;
	if (!TestTrue(
		TEXT("The exact item-owned Gear.Head handle resolves its provider grid"),
		CollisionInventory->GetGridSizeForContainerHandle(
			CollidingContents,
			CollidingGridSize)))
	{
		return false;
	}
	TestEqual(
		TEXT("The item-owned Gear.Head grid retains its real width"),
		CollidingGridSize.Width,
		4);
	TestEqual(
		TEXT("The item-owned Gear.Head grid retains its real height"),
		CollidingGridSize.Height,
		4);

	const FRpgInventoryPlacementSubject WideSubject =
		FRpgInventoryPlacementSubject::FromDefinition(
			URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
			1);
	const FRpgInventoryPlacementPlan OutOfBoundsPlan =
		CollisionInventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Add,
			WideSubject,
			MakePlacement(CollidingContents, 3, 0)));
	TestEqual(
		TEXT("An item-owned Gear.Head grid keeps its real 2x1 footprint"),
		OutOfBoundsPlan.Code,
		ERpgInventoryMutationResultCode::OutOfBounds);

	const FRpgInventoryGridPlacement ValidWidePlacement =
		MakePlacement(CollidingContents, 2, 0);
	const FRpgInventoryPlacementPlan ValidWidePlan =
		CollisionInventory->EvaluatePlacement(MakeExactQuery(
			ERpgInventoryPlacementPurpose::Add,
			WideSubject,
			ValidWidePlacement));
	TestEqual(
		TEXT("The same item-owned grid accepts the full footprint in bounds"),
		ValidWidePlan.Code,
		ERpgInventoryMutationResultCode::Success);
	if (ValidWidePlan.Steps.IsValidIndex(0))
	{
		TestEqual(
			TEXT("The item-owned placement is not collapsed to one cell"),
			ValidWidePlan.Steps[0].Placement.Width,
			2);
	}
	URpgInventoryItemInstance* WideItem =
		CollisionInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
			1,
			ValidWidePlacement);
	if (!TestNotNull(TEXT("The valid wide item commits"), WideItem))
	{
		return false;
	}
	TestEqual(
		TEXT("The second occupied cell resolves the committed wide item"),
		CollisionInventory->GetItemAtContainerCell(
			CollidingContents,
			3,
			0),
		WideItem);

	const FRpgInventoryContainerHandle SourceRoot = MakeRoot(SourceInventory);
	const FRpgInventoryContainerHandle TargetRoot = MakeRoot(TargetInventory);
	URpgInventoryItemInstance* Bag =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(SourceRoot, 0, 0));
	if (!TestNotNull(TEXT("The transferred provider exists"), Bag))
	{
		return false;
	}
	const FRpgInventoryContainerHandle BagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			Bag->GetItemId(),
			TEXT("Main"),
			1);
	URpgInventoryItemInstance* Child =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(BagContents, 0, 0));
	if (!TestNotNull(TEXT("The transferred provider has one child"), Child))
	{
		return false;
	}

	TargetInventory->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	TargetInventory->SetFixedMaxEntries(1);
	FRpgInventoryEntryView BagEntry;
	if (!TestTrue(
		TEXT("The provider exposes a complete source snapshot"),
		GetEntryView(SourceInventory, Bag->GetItemId(), BagEntry)))
	{
		return false;
	}

	const FString SourceBefore = MakeInventorySignature(SourceInventory);
	const FString TargetBefore = MakeInventorySignature(TargetInventory);
	const FRpgInventoryPlacementPlan CapacityPlan =
		TargetInventory->EvaluatePlacement(MakeFirstFitQuery(
			ERpgInventoryPlacementPurpose::Transfer,
			FRpgInventoryPlacementSubject::FromIncomingInstance(
				SourceInventory,
				BagEntry,
				1),
			TargetRoot));
	TestEqual(
		TEXT("Transfer capacity counts the provider and every descendant"),
		CapacityPlan.Code,
		ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(
		TEXT("Rejected subtree capacity produces no partial placement steps"),
		CapacityPlan.Steps.Num(),
		0);

	FRpgInventoryTransferIntent TransferIntent;
	TransferIntent.EnsureRequestId();
	TransferIntent.ItemId = BagEntry.ItemId;
	TransferIntent.ExpectedEntryId = BagEntry.EntryId;
	TransferIntent.ExpectedSourcePlacement = BagEntry.Placement;
	TransferIntent.ExpectedSourceQuantity = BagEntry.StackCount;
	TransferIntent.TargetContainer = TargetRoot;
	TransferIntent.Quantity = 1;
	const FRpgInventoryMutationResult TransferResult =
		SourceInventory->TransferItem(TargetInventory, TransferIntent);
	TestEqual(
		TEXT("The authoritative transfer re-evaluates the same subtree capacity"),
		TransferResult.Code,
		ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(
		TEXT("Rejected subtree transfer preserves the source graph"),
		MakeInventorySignature(SourceInventory),
		SourceBefore);
	TestEqual(
		TEXT("Rejected subtree transfer preserves the target graph"),
		MakeInventorySignature(TargetInventory),
		TargetBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPlacementRestoreScratchTest,
	"SurvivalRpg.Inventory.PlacementEvaluator.RestoreScratchRejectsOverlap",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPlacementRestoreScratchTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPlacementEvaluatorTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("PlacementRestoreScratch"));
	if (!TestNotNull(TEXT("The restore inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeRoot(Inventory);
	URpgInventoryItemInstance* First =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* Second =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 1, 0));
	if (!TestNotNull(TEXT("The first saved item exists"), First) ||
		!TestNotNull(TEXT("The second saved item exists"), Second))
	{
		return false;
	}

	FRpgInventoryGraphSaveData SaveData = Inventory->ExportInventoryGraph();
	if (!TestEqual(
			TEXT("The restore fixture exports two rows"),
			SaveData.Items.Num(),
			2) ||
		!SaveData.Items.IsValidIndex(1))
	{
		return false;
	}
	SaveData.Items[1].Container = SaveData.Items[0].Container;
	SaveData.Items[1].Placement = SaveData.Items[0].Placement;

	const FString BeforeRestore = MakeInventorySignature(Inventory);
	const int32 RevisionBeforeRestore = Inventory->GetInventoryRevision();
	FRpgInventoryMutationResult RestoreResult;
	TestFalse(
		TEXT("Restore rejects two staged rows that overlap each other"),
		Inventory->RestoreInventoryGraph(SaveData, RestoreResult));
	TestEqual(
		TEXT("Restore overlap uses the shared Occupied reason"),
		RestoreResult.Code,
		ERpgInventoryMutationResultCode::Occupied);
	TestEqual(
		TEXT("Rejected restore applies no staged quantity"),
		RestoreResult.AppliedQuantity,
		0);
	TestEqual(
		TEXT("Rejected restore preserves the complete live graph"),
		MakeInventorySignature(Inventory),
		BeforeRestore);
	TestEqual(
		TEXT("Rejected restore does not advance the replicated revision"),
		Inventory->GetInventoryRevision(),
		RevisionBeforeRestore);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
