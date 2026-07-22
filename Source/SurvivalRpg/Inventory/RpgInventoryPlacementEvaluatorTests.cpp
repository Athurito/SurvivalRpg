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
	const FRpgInventoryContainerHandle RootGearHead =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::GearHeadGroupId);
	TestTrue(
		TEXT("The exact built-in root Gear.Head handle is classified as gear"),
		URpgPlayerInventoryLayoutComponent::IsBuiltInGearContainer(
			RootGearHead));
	TestFalse(
		TEXT("An item-owned Gear.Head handle is not classified as built-in gear"),
		URpgPlayerInventoryLayoutComponent::IsBuiltInGearContainer(
			CollidingContents));

	ERpgEquipmentSlot RootEquipmentSlot = ERpgEquipmentSlot::None;
	TestTrue(
		TEXT("The exact root Gear.Head handle resolves to an equipment slot"),
		URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearContainer(
			RootGearHead,
			RootEquipmentSlot));
	TestEqual(
		TEXT("The built-in root Gear.Head handle maps to the Head slot"),
		RootEquipmentSlot,
		ERpgEquipmentSlot::Head);

	ERpgEquipmentSlot CollidingEquipmentSlot = ERpgEquipmentSlot::Head;
	TestFalse(
		TEXT("The item-owned Gear.Head handle cannot resolve as equipment"),
		URpgPlayerInventoryLayoutComponent::TryGetEquipmentSlotForGearContainer(
			CollidingContents,
			CollidingEquipmentSlot));
	TestEqual(
		TEXT("Rejected item-owned gear-name aliases leave no equipment slot"),
		CollidingEquipmentSlot,
		ERpgEquipmentSlot::None);

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
	const URpgPlayerInventoryLayoutDefinition* LayoutDefinition =
		LayoutPawnData ? LayoutPawnData->InventoryLayoutDefinition : nullptr;
	const URpgPlayerInventoryLayoutComponent* DefaultLayout =
		LayoutController->GetPlayerInventoryLayoutComponent();
	FRpgInventorySlotAddress RootGearAddress;
	FRpgInventoryGridPlacement RootGearPlacement;
	if (!TestNotNull(TEXT("The fixture PawnData exists"), LayoutPawnData) ||
		!TestNotNull(TEXT("The fixture layout definition exists"), LayoutDefinition) ||
		!TestEqual(
			TEXT("The fixture layout definition contains all static groups"),
			LayoutDefinition ? LayoutDefinition->StaticSlotGroups.Num() : 0,
			13) ||
		!TestNotNull(TEXT("The default player inventory layout exists"), DefaultLayout) ||
		!TestEqual(
			TEXT("The component resolves all fixture layout groups"),
			DefaultLayout ? DefaultLayout->GetSlotGroups().Num() : 0,
			13) ||
		!TestTrue(
			TEXT("The Head equipment slot produces a canonical root address"),
			URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(
				ERpgEquipmentSlot::Head,
				RootGearAddress)) ||
		!TestEqual(
			TEXT("The canonical Head address keeps the complete root handle"),
			RootGearAddress.GetContainerHandle(),
			RootGearHead) ||
		!TestTrue(
			TEXT("The root Gear.Head address normalizes to a layout placement"),
			DefaultLayout->ResolveSlotAddress(
				RootGearAddress,
				RootGearPlacement)))
	{
		return false;
	}
	TestEqual(
		TEXT("Root Gear.Head normalization keeps the exact root handle"),
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
		TEXT("Root Gear.Head single-cell normalization disables rotation"),
		RootGearPlacement.bRotated);

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
