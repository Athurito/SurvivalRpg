#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgPlayerInventoryLayoutComponent.h"
#include "RpgInventoryDragDrop.h"
#include "RpgInventoryInteractionSession.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Systems/GameplayTagStack.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"

namespace RpgInventoryTransactionTests
{
	const FName StorageContainerId(TEXT("Storage"));
	const FName BagContainerId(TEXT("Main"));

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

		UWorld* GetTestWorld() const
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
			SpawnParameters.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			AActor* OwnerActor = World->SpawnActor<AActor>(SpawnParameters);
			if (!OwnerActor)
			{
				return nullptr;
			}

			URpgInventoryManagerComponent* Inventory = NewObject<URpgInventoryManagerComponent>(
				OwnerActor,
				MakeUniqueObjectName(OwnerActor, URpgInventoryManagerComponent::StaticClass(), TEXT("Inventory")),
				RF_Transient);
			OwnerActor->AddInstanceComponent(Inventory);
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

	FRpgInventoryMutationRequest MakePlacementRequest(
		ERpgInventoryMutationOperation Operation,
		const URpgInventoryItemInstance* Item,
		const FRpgInventoryContainerHandle& Source,
		const FRpgInventoryContainerHandle& Target,
		int32 X,
		int32 Y,
		bool bRotated = false)
	{
		FRpgInventoryMutationRequest Request;
		Request.Operation = Operation;
		Request.ItemId = Item ? Item->GetItemId() : FRpgInventoryItemId();
		Request.Source = Source;
		Request.Target = Target;
		Request.TargetPlacement = MakePlacement(Target, X, Y, bRotated);
		Request.RequestId = FGuid::NewGuid();
		return Request;
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

	FString MakeInventorySignature(const URpgInventoryManagerComponent* Inventory)
	{
		TArray<FString> Rows;
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			Rows.Add(FString::Printf(
				TEXT("%s|%d|%s|%d|%d|%d|%d|%d"),
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

	FRpgInventorySavedItem* FindSavedItem(
		FRpgInventoryGraphSaveData& SaveData,
		const FRpgInventoryItemId& ItemId)
	{
		return SaveData.Items.FindByPredicate(
			[&ItemId](const FRpgInventorySavedItem& SavedItem)
			{
				return SavedItem.ItemId == ItemId;
			});
	}

	const FRpgInventorySavedItem* FindSavedItem(
		const FRpgInventoryGraphSaveData& SaveData,
		const FRpgInventoryItemId& ItemId)
	{
		return SaveData.Items.FindByPredicate(
			[&ItemId](const FRpgInventorySavedItem& SavedItem)
			{
				return SavedItem.ItemId == ItemId;
			});
	}

	bool SetSavedPlacement(
		FRpgInventoryGraphSaveData& SaveData,
		const FRpgInventoryItemId& ItemId,
		const FRpgInventoryContainerHandle& Container,
		int32 X = 0,
		int32 Y = 0)
	{
		FRpgInventorySavedItem* SavedItem = FindSavedItem(SaveData, ItemId);
		if (!SavedItem)
		{
			return false;
		}

		SavedItem->Container = Container;
		SavedItem->Placement.SetContainerHandle(Container);
		SavedItem->Placement.X = X;
		SavedItem->Placement.Y = Y;
		SavedItem->Placement.bRotated = false;
		return true;
	}

	bool InitializeTest(FAutomationTestBase& Test, FScopedInventoryWorld& TestWorld)
	{
		if (!TestWorld.IsValid())
		{
			Test.AddError(TEXT("Could not create an isolated standalone inventory test world."));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryFragmentedGridAndRotationTest,
	"SurvivalRpg.Inventory.Transaction.FragmentedGridAndEdgeRotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryFragmentedGridAndRotationTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryManagerComponent* RotationInventory = TestWorld.CreateInventory(TEXT("RotationInventory"));
	if (!TestNotNull(TEXT("Rotation inventory exists"), RotationInventory))
	{
		return false;
	}

	URpgInventoryItemInstance* EdgeItem = RotationInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 8, 5));
	TestNotNull(TEXT("A 2x1 item fits along the lower-right edge before rotation"), EdgeItem);
	if (!EdgeItem)
	{
		return false;
	}

	FRpgInventoryMutationRequest EdgeRotation = MakePlacementRequest(
		ERpgInventoryMutationOperation::Rotate,
		EdgeItem,
		Root,
		Root,
		8,
		5,
		true);
	const FString BeforeRejectedRotation = MakeInventorySignature(RotationInventory);
	const FRpgInventoryMutationResult EdgePlan = RotationInventory->PlanInventoryMutation(EdgeRotation);
	TestEqual(TEXT("Rotating at the lower edge is rejected as out of bounds"), EdgePlan.Code, ERpgInventoryMutationResultCode::OutOfBounds);
	const FRpgInventoryMutationResult EdgeCommit = RotationInventory->ExecuteInventoryMutation(EdgeRotation);
	TestEqual(TEXT("The authoritative edge rotation is rejected with the same reason"), EdgeCommit.Code, ERpgInventoryMutationResultCode::OutOfBounds);
	TestEqual(TEXT("Rejected edge rotation leaves the graph unchanged"), MakeInventorySignature(RotationInventory), BeforeRejectedRotation);

	URpgInventoryItemInstance* InPlaceItem = RotationInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 0, 0));
	TestNotNull(TEXT("A second wide item is added for an in-place rotation"), InPlaceItem);
	if (!InPlaceItem)
	{
		return false;
	}

	FRpgInventoryMutationRequest InPlaceRotation = MakePlacementRequest(
		ERpgInventoryMutationOperation::Rotate,
		InPlaceItem,
		Root,
		Root,
		0,
		0,
		true);
	const FRpgInventoryMutationResult InPlaceResult = RotationInventory->ExecuteInventoryMutation(InPlaceRotation);
	TestEqual(TEXT("A clear in-place rotation commits"), InPlaceResult.Code, ERpgInventoryMutationResultCode::Success);
	FRpgInventoryEntryView RotatedView;
	TestTrue(TEXT("The rotated item remains addressable by persistent id"), GetEntryView(RotationInventory, InPlaceItem->GetItemId(), RotatedView));
	TestTrue(TEXT("The committed placement records rotation"), RotatedView.Placement.bRotated);
	TestEqual(TEXT("The rotated 2x1 footprint occupies one cell horizontally"), RotatedView.Placement.GetOccupiedSize().Width, 1);
	TestEqual(TEXT("The rotated 2x1 footprint occupies two cells vertically"), RotatedView.Placement.GetOccupiedSize().Height, 2);

	URpgInventoryManagerComponent* FragmentedInventory = TestWorld.CreateInventory(TEXT("FragmentedInventory"));
	if (!TestNotNull(TEXT("Fragmented-grid inventory exists"), FragmentedInventory))
	{
		return false;
	}

	bool bFilledCheckerboard = true;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			if ((X + Y) % 2 == 0)
			{
				bFilledCheckerboard &= FragmentedInventory->AddItemDefinitionToPlacement(
					URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
					1,
					MakePlacement(Root, X, Y)) != nullptr;
			}
		}
	}
	TestTrue(TEXT("Checkerboard fragmentation was constructed"), bFilledCheckerboard);
	TestEqual(TEXT("Checkerboard uses thirty isolated cells"), FragmentedInventory->GetUsedEntryCount(), 30);
	TestFalse(
		TEXT("A rotatable 2x1 item cannot fit when every free cell is orthogonally isolated"),
		FragmentedInventory->CanAddItemDefinition(URpgInventoryAutomationTestWideItemDefinition::StaticClass(), 1));
	const FString BeforeFailedAdd = MakeInventorySignature(FragmentedInventory);
	TestNull(
		TEXT("Atomic definition add rejects the fragmented grid"),
		FragmentedInventory->AddItemDefinition(URpgInventoryAutomationTestWideItemDefinition::StaticClass(), 1));
	TestEqual(TEXT("Failed fragmented-grid add does not partially mutate entries"), MakeInventorySignature(FragmentedInventory), BeforeFailedAdd);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryContainerGearDragDropTest,
	"SurvivalRpg.Inventory.DragDrop.ItemContainerBackpackPreviewAndCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryContainerGearDragDropTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("ContainerGearDragDropController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("ContainerGearDragDropPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The drag/drop controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The drag/drop player-state fixture exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	if (!TestNotNull(TEXT("The player inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets = FRpgInventoryContainerHandle::MakeRoot(
		URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* Backpack = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("The ItemContainer backpack starts in Pockets"), Backpack))
	{
		return false;
	}

	FRpgInventoryEntryView BackpackEntry;
	if (!TestTrue(TEXT("The backpack entry is addressable"), GetEntryView(Inventory, Backpack->GetItemId(), BackpackEntry)))
	{
		return false;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(Controller, Controller);
	if (!TestNotNull(TEXT("The screen-local coordinator exists"), Coordinator) ||
		!TestNotNull(TEXT("The coordinator owns an interaction session"), Coordinator ? Coordinator->GetInteractionSession() : nullptr))
	{
		return false;
	}

	FRpgInventoryDragPayload Payload;
	Payload.SourceType = ERpgInventoryDragSourceType::InventoryEntry;
	Payload.SourceInventory = Inventory;
	Payload.ItemInstance = Backpack;
	Payload.EntryId = BackpackEntry.EntryId;
	Payload.StackCount = BackpackEntry.StackCount;
	Payload.SourcePlacement = BackpackEntry.Placement;
	Payload.ItemFootprint.Width = BackpackEntry.Placement.Width;
	Payload.ItemFootprint.Height = BackpackEntry.Placement.Height;
	const FRpgInventoryDropTarget BackpackTarget =
		URpgInventoryDragDropCoordinator::MakeEquipmentTarget(ERpgEquipmentSlot::Backpack);

	URpgInventoryInteractionSession* Session = Coordinator->GetInteractionSession();
	TestFalse(TEXT("The session starts without a held payload"), Session->HasPayload());
	bool bEveryPurePreviewAccepted = true;
	for (int32 PreviewIteration = 0; PreviewIteration < 256; ++PreviewIteration)
	{
		bEveryPurePreviewAccepted &= Coordinator->PreviewPayloadDrop(Payload, BackpackTarget);
	}
	TestTrue(
		TEXT("Repeated pure Gear.Backpack previews are accepted without recursive delegate broadcasts"),
		bEveryPurePreviewAccepted);
	TestEqual(
		TEXT("The resolved indicator is Equip"),
		Coordinator->ResolveInteractionPreview(Payload, BackpackTarget),
		ERpgInventoryInteractionPreviewState::Equip);
	TestFalse(TEXT("Pure preview evaluation does not start or broadcast an interaction"), Session->HasPayload());

	TestTrue(TEXT("An actual hover updates the shared interaction"), Coordinator->UpdateInteractionPreview(Payload, BackpackTarget));
	TestTrue(TEXT("Actual hover owns the payload until acknowledgement"), Session->HasPayload());
	TestEqual(TEXT("Actual hover publishes the Equip indicator"), Session->GetPreviewState(), ERpgInventoryInteractionPreviewState::Equip);
	TestTrue(TEXT("Dropping the backpack dispatches the authoritative equip request"), Coordinator->CommitPayloadToTarget(Payload, BackpackTarget));

	FRpgInventoryEntryView EquippedBackpackEntry;
	TestTrue(TEXT("The equipped backpack keeps its persistent identity"), GetEntryView(Inventory, Backpack->GetItemId(), EquippedBackpackEntry));
	TestEqual(
		TEXT("The backpack is physically located in Gear.Backpack"),
		EquippedBackpackEntry.Placement.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::GearBackpackGroupId));

	FRpgInventoryDragPayload EquippedPayload =
		URpgInventoryDragDropCoordinator::MakeEquipmentPayload(Backpack, ERpgEquipmentSlot::Backpack);
	FRpgInventoryGridPlacement ExactPocketsPlacement = MakePlacement(Pockets, 2, 0);
	FRpgInventoryDropTarget ExactPocketsTarget;
	ExactPocketsTarget.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	ExactPocketsTarget.TargetInventory = Inventory;
	ExactPocketsTarget.TargetPlacement = ExactPocketsPlacement;
	TestTrue(
		TEXT("Gear-to-grid preview validates the exact visible target instead of treating it as a generic unequip"),
		Coordinator->PreviewPayloadDrop(EquippedPayload, ExactPocketsTarget));
	TestTrue(
		TEXT("Gear-to-grid commit dispatches the exact spatial address"),
		Coordinator->CommitPayloadToTarget(EquippedPayload, ExactPocketsTarget));

	FRpgInventoryEntryView ReturnedBackpackEntry;
	TestTrue(TEXT("The returned backpack remains addressable"), GetEntryView(Inventory, Backpack->GetItemId(), ReturnedBackpackEntry));
	TestEqual(TEXT("Gear-to-grid preserves the requested X coordinate"), ReturnedBackpackEntry.Placement.X, ExactPocketsPlacement.X);
	TestEqual(TEXT("Gear-to-grid preserves the requested Y coordinate"), ReturnedBackpackEntry.Placement.Y, ExactPocketsPlacement.Y);
	TestEqual(
		TEXT("Gear-to-grid moves the physical item back into Pockets"),
		ReturnedBackpackEntry.Placement.GetContainerHandle(),
		Pockets);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCarryQuickAccessBindingTest,
	"SurvivalRpg.Inventory.QuickAccess.WeaponSlot1DragCommitsAuthorityBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCarryQuickAccessBindingTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("CarryQuickAccessController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("CarryQuickAccessPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The Carry binding controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The Carry binding player-state fixture exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	URpgActionBarComponent* ActionBar = Controller->GetActionBarComponent();
	URpgInventoryUiActionComponent* UiActions = Controller->GetInventoryUiActionComponent();
	if (!TestTrue(TEXT("The fixture executes on server authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The controller actionbar exists"), ActionBar) ||
		!TestNotNull(TEXT("The server inventory action component exists"), UiActions))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle WeaponSlot1 =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	URpgInventoryItemInstance* Weapon = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("A concrete weapon starts in Pockets"), Weapon))
	{
		return false;
	}

	FRpgInventorySlotAddress WeaponSlot1Address;
	WeaponSlot1Address.SetContainerHandle(WeaponSlot1);
	WeaponSlot1Address.X = 0;
	WeaponSlot1Address.Y = 0;
	UiActions->RequestMoveItemToInventorySlotAddress(Weapon, WeaponSlot1Address);

	FRpgInventoryEntryView WeaponEntry;
	if (!TestTrue(
			TEXT("The WeaponSlot1 entry is addressable after the real layout mutation"),
			GetEntryView(Inventory, Weapon->GetItemId(), WeaponEntry)) ||
		!TestEqual(
			TEXT("The weapon is physically located in WeaponSlot1"),
			WeaponEntry.Placement.GetContainerHandle(),
			WeaponSlot1))
	{
		return false;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(Controller, Controller);
	if (!TestNotNull(TEXT("The player drag/drop coordinator exists"), Coordinator))
	{
		return false;
	}

	FRpgInventoryDragPayload Payload;
	Payload.SourceType = ERpgInventoryDragSourceType::InventoryEntry;
	Payload.SourceInventory = Inventory;
	Payload.ItemInstance = Weapon;
	Payload.EntryId = WeaponEntry.EntryId;
	Payload.StackCount = WeaponEntry.StackCount;
	Payload.SourcePlacement = WeaponEntry.Placement;
	Payload.SourceSlotAddress = WeaponSlot1Address;
	Payload.ItemFootprint.Width = 1;
	Payload.ItemFootprint.Height = 1;

	const FRpgInventoryDropTarget ActionBarTarget =
		URpgInventoryDragDropCoordinator::MakeActionBarSlotTarget(0);
	TestTrue(
		TEXT("WeaponSlot1 previews as a valid Quick Access binding"),
		Coordinator->UpdateInteractionPreview(Payload, ActionBarTarget));
	TestTrue(
		TEXT("Dropping WeaponSlot1 dispatches and applies the authoritative Quick Access mutation"),
		Coordinator->CommitPayloadToTarget(Payload, ActionBarTarget));

	const FRpgActionBarSlot AppliedSlot = ActionBar->GetSlot(0);
	TestEqual(
		TEXT("The first Quick Access slot stores Carry semantics"),
		AppliedSlot.SlotType,
		ERpgActionBarSlotType::CarrySlot);
	TestEqual(
		TEXT("The binding follows the WeaponSlot1 role"),
		AppliedSlot.CarryRole,
		URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	TestEqual(
		TEXT("The canonical Carry address is retained for presentation and validation"),
		AppliedSlot.SlotAddress.GetContainerHandle(),
		WeaponSlot1);
	TestFalse(
		TEXT("The acknowledged binding releases the held drag ghost"),
		Coordinator->GetInteractionSession()->HasPayload());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryQuickTransferSkipsFullPreferredContainerTest,
	"SurvivalRpg.Inventory.QuickTransfer.SkipsFullPreferredContainer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryQuickTransferSkipsFullPreferredContainerTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("QuickTransferController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("QuickTransferPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The quick-transfer controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The quick-transfer player-state fixture exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	URpgPlayerInventoryLayoutComponent* Layout = Controller->GetPlayerInventoryLayoutComponent();
	URpgInventoryUiActionComponent* UiActions = Controller->GetInventoryUiActionComponent();
	if (!TestTrue(TEXT("The fixture runs the quick transfer on server authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The fixture uses the real player inventory"), Inventory) ||
		!TestNotNull(TEXT("The fixture uses the real player inventory layout"), Layout) ||
		!TestNotNull(TEXT("The fixture uses the real inventory UI action component"), UiActions))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets = FRpgInventoryContainerHandle::MakeRoot(
		URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle BackpackSlot = FRpgInventoryContainerHandle::MakeRoot(
		URpgPlayerInventoryLayoutComponent::GearBackpackGroupId);
	const FRpgInventoryContainerHandle BeltSlot = FRpgInventoryContainerHandle::MakeRoot(
		URpgPlayerInventoryLayoutComponent::GearBeltGroupId);

	URpgInventoryItemInstance* BackpackProvider = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* BeltProvider = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 1, 0));
	if (!TestNotNull(TEXT("The backpack provider bag was created"), BackpackProvider) ||
		!TestNotNull(TEXT("The belt provider bag was created"), BeltProvider))
	{
		return false;
	}

	const FRpgInventoryMutationResult EquipBackpackResult = Inventory->ExecuteInventoryMutation(
		MakePlacementRequest(
			ERpgInventoryMutationOperation::Move,
			BackpackProvider,
			Pockets,
			BackpackSlot,
			0,
			0));
	const FRpgInventoryMutationResult EquipBeltResult = Inventory->ExecuteInventoryMutation(
		MakePlacementRequest(
			ERpgInventoryMutationOperation::Move,
			BeltProvider,
			Pockets,
			BeltSlot,
			0,
			0));
	if (!TestEqual(
			TEXT("The first automation bag becomes the Backpack provider"),
			EquipBackpackResult.Code,
			ERpgInventoryMutationResultCode::Success) ||
		!TestEqual(
			TEXT("The second automation bag becomes the Belt provider"),
			EquipBeltResult.Code,
			ERpgInventoryMutationResultCode::Success))
	{
		return false;
	}

	const FRpgInventoryContainerHandle BackpackContents = FRpgInventoryContainerHandle::MakeItemOwned(
		BackpackProvider->GetItemId(),
		BagContainerId,
		1);
	const FRpgInventoryContainerHandle BeltContents = FRpgInventoryContainerHandle::MakeItemOwned(
		BeltProvider->GetItemId(),
		BagContainerId,
		1);
	const bool bLayoutExposesBothProviders = Layout->GetSlotGroups().ContainsByPredicate(
		[&BackpackContents](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerHandle == BackpackContents && Group.SourceEquipmentSlotName == FName(TEXT("Backpack"));
		}) && Layout->GetSlotGroups().ContainsByPredicate(
		[&BeltContents](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerHandle == BeltContents && Group.SourceEquipmentSlotName == FName(TEXT("Belt"));
		});
	if (!TestTrue(TEXT("The real layout exposes the equipped Backpack and Belt content grids"), bLayoutExposesBothProviders))
	{
		return false;
	}

	URpgInventoryItemInstance* SourceItem = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(BackpackContents, 0, 0));
	if (!TestNotNull(TEXT("The transfer source starts in Backpack content"), SourceItem))
	{
		return false;
	}

	bool bFilledPockets = true;
	for (int32 Y = 0; Y < 2; ++Y)
	{
		for (int32 X = 0; X < 4; ++X)
		{
			bFilledPockets &= Inventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				1,
				MakePlacement(Pockets, X, Y)) != nullptr;
		}
	}
	if (!TestTrue(TEXT("The first preferred Pockets target is completely full"), bFilledPockets))
	{
		return false;
	}

	FRpgInventoryQuickTransferRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ItemId = SourceItem->GetItemId();
	Request.StackCount = 1;
	Request.PreferredTargetContainers = { Pockets, BeltContents };

	FRpgInventoryContainerHandle ResolvedContainer;
	FRpgInventoryGridPlacement ResolvedPlacement;
	TestTrue(
		TEXT("Destination scan continues after the full first preferred container"),
		UiActions->FindQuickTransferDestination(Inventory, Inventory, Request, ResolvedContainer, ResolvedPlacement));
	TestEqual(TEXT("The later free Belt grid is selected"), ResolvedContainer, BeltContents);
	TestEqual(TEXT("The deterministic Belt placement starts at X zero"), ResolvedPlacement.X, 0);
	TestEqual(TEXT("The deterministic Belt placement starts at Y zero"), ResolvedPlacement.Y, 0);

	UiActions->RequestQuickTransferItem(Inventory, Inventory, Request);
	FRpgInventoryEntryView MovedEntry;
	if (!TestTrue(TEXT("The committed item remains addressable by persistent id"), GetEntryView(Inventory, SourceItem->GetItemId(), MovedEntry)))
	{
		return false;
	}
	TestEqual(TEXT("The server commit uses the resolved Belt content container"), MovedEntry.Placement.GetContainerHandle(), BeltContents);
	TestEqual(TEXT("The server commit preserves the resolved X coordinate"), MovedEntry.Placement.X, ResolvedPlacement.X);
	TestEqual(TEXT("The server commit preserves the resolved Y coordinate"), MovedEntry.Placement.Y, ResolvedPlacement.Y);
	TestNull(TEXT("The source cell in Backpack content is empty after the atomic move"), Inventory->GetItemAtContainerCell(BackpackContents, 0, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentLoadIgnoresNestedContentsTest,
	"SurvivalRpg.Equipment.Load.NestedContainerContentsAreWeightless",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentLoadIgnoresNestedContentsTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("EquipmentLoadTestController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("EquipmentLoadTestPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("Concrete player-controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("Player-state inventory fixture exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout = Controller->GetEquipmentLoadoutComponent();
	if (!TestNotNull(TEXT("The fixture uses the real player inventory manager"), Inventory) ||
		!TestNotNull(TEXT("The fixture uses the real equipment-load component"), EquipmentLoadout))
	{
		return false;
	}

	const FRpgInventoryContainerHandle BackpackSlot = FRpgInventoryContainerHandle::MakeRoot(
		URpgPlayerInventoryLayoutComponent::GearBackpackGroupId);
	const FRpgInventoryContainerHandle Pockets = FRpgInventoryContainerHandle::MakeRoot(
		URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* Backpack = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("The weighted backpack fixture starts in a normal content grid"), Backpack))
	{
		return false;
	}

	const FRpgInventoryMutationResult EquipBackpackResult = Inventory->ExecuteInventoryMutation(
		MakePlacementRequest(
			ERpgInventoryMutationOperation::Move,
			Backpack,
			Pockets,
			BackpackSlot,
			0,
			0));
	if (!TestEqual(
			TEXT("The authoritative transaction equips the backpack into Gear.Backpack"),
			EquipBackpackResult.Code,
			ERpgInventoryMutationResultCode::Success))
	{
		return false;
	}

	EquipmentLoadout->RefreshEquipmentLoadState();
	TestEqual(TEXT("Only the equipped backpack contributes its authored load"), EquipmentLoadout->GetEquipmentLoadWeight(), 7.5f);
	TestEqual(TEXT("A 7.5 kg backpack remains in the Light tier"), EquipmentLoadout->GetEquipmentLoadTier(), ERpgEquipmentLoadTier::Light);

	const FRpgInventoryContainerHandle BackpackContents = FRpgInventoryContainerHandle::MakeItemOwned(
		Backpack->GetItemId(),
		BagContainerId,
		1);
	TArray<URpgInventoryItemInstance*> HeavyContents;
	bool bFilledBackpack = true;
	for (int32 Y = 0; Y < 4; ++Y)
	{
		for (int32 X = 0; X < 4; ++X)
		{
			URpgInventoryItemInstance* HeavyItem = Inventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestHeavyItemDefinition::StaticClass(),
				1,
				MakePlacement(BackpackContents, X, Y));
			bFilledBackpack &= HeavyItem != nullptr;
			HeavyContents.Add(HeavyItem);
		}
	}
	TestTrue(TEXT("Sixteen 30 kg test items fill the backpack's complete 4x4 content grid"), bFilledBackpack);

	EquipmentLoadout->RefreshEquipmentLoadState();
	TestEqual(
		TEXT("All 480 kg of nested backpack contents are excluded from equipment load"),
		EquipmentLoadout->GetEquipmentLoadWeight(),
		7.5f);
	TestEqual(
		TEXT("Nested contents cannot change the dodge load tier"),
		EquipmentLoadout->GetEquipmentLoadTier(),
		ERpgEquipmentLoadTier::Light);

	if (!HeavyContents.IsValidIndex(0) || !HeavyContents[0])
	{
		AddError(TEXT("The heavy-content fixture did not produce a movable item."));
		return false;
	}

	const FRpgInventoryContainerHandle ChestSlot = FRpgInventoryContainerHandle::MakeRoot(
		URpgPlayerInventoryLayoutComponent::GearChestGroupId);
	FRpgInventoryMutationRequest MoveToGear = MakePlacementRequest(
		ERpgInventoryMutationOperation::Move,
		HeavyContents[0],
		BackpackContents,
		ChestSlot,
		0,
		0);
	const FRpgInventoryMutationResult MoveResult = Inventory->ExecuteInventoryMutation(MoveToGear);
	TestEqual(TEXT("The same heavy armor item can move from backpack contents into Gear.Chest"), MoveResult.Code, ERpgInventoryMutationResultCode::Success);

	EquipmentLoadout->RefreshEquipmentLoadState();
	TestEqual(
		TEXT("Once physically in Gear, that exact item contributes its 30 kg load"),
		EquipmentLoadout->GetEquipmentLoadWeight(),
		37.5f);
	TestEqual(
		TEXT("Gear plus backpack now crosses the Heavy threshold"),
		EquipmentLoadout->GetEquipmentLoadTier(),
		ERpgEquipmentLoadTier::Heavy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryFullGridSortTest,
	"SurvivalRpg.Inventory.Transaction.FullGridSortUsesScratchOccupancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryFullGridSortTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = TestWorld.CreateInventory(TEXT("FullGridSortInventory"));
	if (!TestNotNull(TEXT("Sort inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	bool bFilledGrid = true;
	for (int32 Y = 0; Y < 6; Y += 2)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			bFilledGrid &= Inventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
				1,
				MakePlacement(Root, X, Y, true)) != nullptr;
		}
	}
	TestTrue(TEXT("Thirty vertical 1x2 placements fill the complete 10x6 grid"), bFilledGrid);
	TestEqual(TEXT("The full grid contains thirty entries"), Inventory->GetUsedEntryCount(), 30);

	TestTrue(TEXT("Sort can repack a completely full grid without consulting stale live occupancy"), Inventory->ApplyInventorySort(ERpgInventorySortMode::Name));
	const TArray<FRpgInventoryEntryView> SortedEntries = Inventory->GetAllEntries();
	TestEqual(TEXT("Sort preserves every entry"), SortedEntries.Num(), 30);

	TArray<bool> OccupiedCells;
	OccupiedCells.Init(false, 60);
	bool bAllUnrotated = true;
	bool bNoOverlapOrBoundsFailure = true;
	int32 OccupiedCellCount = 0;
	for (const FRpgInventoryEntryView& Entry : SortedEntries)
	{
		bAllUnrotated &= !Entry.Placement.bRotated;
		const FRpgInventoryGridSize OccupiedSize = Entry.Placement.GetOccupiedSize();
		for (int32 LocalY = 0; LocalY < OccupiedSize.Height; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX < OccupiedSize.Width; ++LocalX)
			{
				const int32 X = Entry.Placement.X + LocalX;
				const int32 Y = Entry.Placement.Y + LocalY;
				if (X < 0 || X >= 10 || Y < 0 || Y >= 6)
				{
					bNoOverlapOrBoundsFailure = false;
					continue;
				}

				const int32 CellIndex = Y * 10 + X;
				if (OccupiedCells[CellIndex])
				{
					bNoOverlapOrBoundsFailure = false;
				}
				OccupiedCells[CellIndex] = true;
				++OccupiedCellCount;
			}
		}
	}
	TestTrue(TEXT("Deterministic first-fit sort prefers the unrotated orientation"), bAllUnrotated);
	TestTrue(TEXT("Sorted placements are non-overlapping and inside the grid"), bNoOverlapOrBoundsFailure);
	TestEqual(TEXT("Sorted footprints still occupy all sixty cells"), OccupiedCellCount, 60);

	const FString StableSignature = MakeInventorySignature(Inventory);
	TestFalse(TEXT("Repeating the same deterministic sort reports no further mutation"), Inventory->ApplyInventorySort(ERpgInventorySortMode::Name));
	TestEqual(TEXT("Repeated sort produces byte-for-byte equivalent placement state"), MakeInventorySignature(Inventory), StableSignature);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryMergeCompatibilityTest,
	"SurvivalRpg.Inventory.Transaction.MergeCompatibilityAndPartialMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryMergeCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = TestWorld.CreateInventory(TEXT("MergeInventory"));
	if (!TestNotNull(TEXT("Merge inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* TargetStack = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		8,
		MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* MovingStack = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		5,
		MakePlacement(Root, 1, 0));
	TestNotNull(TEXT("Target stack exists"), TargetStack);
	TestNotNull(TEXT("Moving stack exists"), MovingStack);
	if (!TargetStack || !MovingStack)
	{
		return false;
	}

	MovingStack->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 1);
	FRpgInventoryMutationRequest IncompatibleMerge = MakePlacementRequest(
		ERpgInventoryMutationOperation::Merge,
		MovingStack,
		Root,
		Root,
		0,
		0);
	const FString BeforeIncompatibleMerge = MakeInventorySignature(Inventory);
	const FRpgInventoryMutationResult IncompatiblePlan = Inventory->PlanInventoryMutation(IncompatibleMerge);
	TestEqual(TEXT("Same definition with different stack-relevant runtime state is incompatible"), IncompatiblePlan.Code, ERpgInventoryMutationResultCode::StackIncompatible);
	TestEqual(TEXT("Incompatible merge planning is read-only"), MakeInventorySignature(Inventory), BeforeIncompatibleMerge);

	MovingStack->RemoveStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 1);
	FRpgInventoryMutationRequest PartialMerge = MakePlacementRequest(
		ERpgInventoryMutationOperation::Merge,
		MovingStack,
		Root,
		Root,
		0,
		0);
	const FRpgInventoryMutationResult PartialPlan = Inventory->PlanInventoryMutation(PartialMerge);
	TestEqual(TEXT("Only available stack capacity is planned"), PartialPlan.Code, ERpgInventoryMutationResultCode::PartiallyApplied);
	TestEqual(TEXT("A target at eight accepts exactly two units"), PartialPlan.AppliedQuantity, 2);
	TestEqual(TEXT("Partial merge exposes source and target deltas"), PartialPlan.Deltas.Num(), 2);

	const FRpgInventoryMutationResult PartialCommit = Inventory->ExecuteInventoryMutation(PartialMerge);
	TestEqual(TEXT("Partial merge commits with an explicit partial result"), PartialCommit.Code, ERpgInventoryMutationResultCode::PartiallyApplied);
	TestEqual(TEXT("Target reaches its max stack size"), Inventory->GetItemStackCount(TargetStack), 10);
	TestEqual(TEXT("Unmerged remainder stays in the source stack"), Inventory->GetItemStackCount(MovingStack), 3);
	TestEqual(TEXT("Partial merge preserves both concrete item identities"), Inventory->GetUsedEntryCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventorySplitIdempotencyTest,
	"SurvivalRpg.Inventory.Transaction.ExactHalfSplitAndIdempotency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventorySplitIdempotencyTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = TestWorld.CreateInventory(TEXT("SplitInventory"));
	if (!TestNotNull(TEXT("Split inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* SourceStack = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		9,
		MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("Nine-unit source stack exists"), SourceStack))
	{
		return false;
	}
	SourceStack->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 3);

	const int32 HalfQuantity = Inventory->GetItemStackCount(SourceStack) / 2;
	FRpgInventoryMutationRequest SplitRequest = MakePlacementRequest(
		ERpgInventoryMutationOperation::Split,
		SourceStack,
		Root,
		Root,
		1,
		0);
	SplitRequest.Quantity = HalfQuantity;
	const FRpgInventoryMutationResult SplitPlan = Inventory->PlanInventoryMutation(SplitRequest);
	TestEqual(TEXT("Explicit floor-half split plans successfully"), SplitPlan.Code, ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Nine units default to an exact floor half of four"), SplitPlan.AppliedQuantity, 4);
	TestEqual(TEXT("Split plan has source-change and new-item deltas"), SplitPlan.Deltas.Num(), 2);

	const FRpgInventoryMutationResult FirstCommit = Inventory->ExecuteInventoryMutation(SplitRequest);
	TestEqual(TEXT("Exact half split commits"), FirstCommit.Code, ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Original stack retains five units"), Inventory->GetItemStackCount(SourceStack), 5);
	TestEqual(TEXT("Split creates exactly one additional entry"), Inventory->GetUsedEntryCount(), 2);

	URpgInventoryItemInstance* SplitStack = nullptr;
	int32 TotalQuantity = 0;
	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		TotalQuantity += Entry.StackCount;
		if (Entry.ItemId != SourceStack->GetItemId())
		{
			SplitStack = Entry.Instance;
			TestEqual(TEXT("New stack has the exact requested quantity"), Entry.StackCount, HalfQuantity);
		}
	}
	TestNotNull(TEXT("Split result has a distinct concrete item"), SplitStack);
	TestEqual(TEXT("Split conserves total quantity"), TotalQuantity, 9);
	if (SplitStack)
	{
		TestTrue(TEXT("Split item receives a new persistent id"), SplitStack->GetItemId() != SourceStack->GetItemId());
		TestEqual(TEXT("Split copies stack-relevant StatTags"), SplitStack->GetStatTagStackCount(RpgGameplayTags::Ability_Attack_Basic), 3);
		TestTrue(TEXT("Copied runtime state remains stack-compatible"), SplitStack->IsStackCompatibleWith(SourceStack));
	}

	const FRpgInventoryMutationResult RetriedCommit = Inventory->ExecuteInventoryMutation(SplitRequest);
	TestEqual(TEXT("A reliable retry returns the cached success"), RetriedCommit.Code, FirstCommit.Code);
	TestEqual(TEXT("A repeated request id does not split twice"), Inventory->GetUsedEntryCount(), 2);
	TotalQuantity = 0;
	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		TotalQuantity += Entry.StackCount;
	}
	TestEqual(TEXT("Idempotent retry still conserves total quantity"), TotalQuantity, 9);

	FRpgInventoryMutationRequest ZeroSplit = SplitRequest;
	ZeroSplit.RequestId = FGuid::NewGuid();
	ZeroSplit.Quantity = 0;
	TestEqual(
		TEXT("Zero is outside the exact split range"),
		Inventory->PlanInventoryMutation(ZeroSplit).Code,
		ERpgInventoryMutationResultCode::StackLimitReached);
	FRpgInventoryMutationRequest WholeStackSplit = SplitRequest;
	WholeStackSplit.RequestId = FGuid::NewGuid();
	WholeStackSplit.Quantity = Inventory->GetItemStackCount(SourceStack);
	TestEqual(
		TEXT("The full current stack is outside the exact split range"),
		Inventory->PlanInventoryMutation(WholeStackSplit).Code,
		ERpgInventoryMutationResultCode::StackLimitReached);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventorySwapAndRollbackTest,
	"SurvivalRpg.Inventory.Transaction.SwapAndAtomicRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventorySwapAndRollbackTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = TestWorld.CreateInventory(TEXT("SwapInventory"));
	if (!TestNotNull(TEXT("Swap inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* WideItem = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* UnitItem = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 3, 0));
	TestNotNull(TEXT("Wide swap source exists"), WideItem);
	TestNotNull(TEXT("Unit swap target exists"), UnitItem);
	if (!WideItem || !UnitItem)
	{
		return false;
	}

	FRpgInventoryMutationRequest SwapRequest = MakePlacementRequest(
		ERpgInventoryMutationOperation::Swap,
		WideItem,
		Root,
		Root,
		3,
		0);
	const FRpgInventoryMutationResult SwapPlan = Inventory->PlanInventoryMutation(SwapRequest);
	TestEqual(TEXT("A size-asymmetric but spatially valid swap plans successfully"), SwapPlan.Code, ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Swap exposes one delta per concrete item"), SwapPlan.Deltas.Num(), 2);
	const FRpgInventoryMutationResult SwapCommit = Inventory->ExecuteInventoryMutation(SwapRequest);
	TestEqual(TEXT("Swap commits atomically"), SwapCommit.Code, ERpgInventoryMutationResultCode::Success);

	FRpgInventoryEntryView WideView;
	FRpgInventoryEntryView UnitView;
	TestTrue(TEXT("Wide item remains addressable after swap"), GetEntryView(Inventory, WideItem->GetItemId(), WideView));
	TestTrue(TEXT("Unit item remains addressable after swap"), GetEntryView(Inventory, UnitItem->GetItemId(), UnitView));
	TestEqual(TEXT("Wide item moves to target X"), WideView.Placement.X, 3);
	TestEqual(TEXT("Unit item returns to source X"), UnitView.Placement.X, 0);

	FRpgInventoryMutationRequest RejectedMove = MakePlacementRequest(
		ERpgInventoryMutationOperation::Move,
		WideItem,
		Root,
		Root,
		9,
		5);
	const FString BeforeRejectedMove = MakeInventorySignature(Inventory);
	const FRpgInventoryMutationResult RejectedResult = Inventory->ExecuteInventoryMutation(RejectedMove);
	TestEqual(TEXT("A 2x1 move beyond the right edge is rejected"), RejectedResult.Code, ERpgInventoryMutationResultCode::OutOfBounds);
	TestTrue(TEXT("Rejected mutation contains no authoritative deltas"), RejectedResult.Deltas.IsEmpty());
	TestEqual(TEXT("Rejected mutation rolls back every item and placement"), MakeInventorySignature(Inventory), BeforeRejectedMove);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryAsymmetricDisplacementTest,
	"SurvivalRpg.Inventory.Transaction.AsymmetricSwapUsesReleasedSpace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryAsymmetricDisplacementTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = TestWorld.CreateInventory(TEXT("AsymmetricDisplacementInventory"));
	if (!TestNotNull(TEXT("Displacement inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* UnitItem = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* LargeItem = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestLargeItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 1, 0));
	TestNotNull(TEXT("The displaced 1x1 item exists"), UnitItem);
	TestNotNull(TEXT("The moving 3x2 item exists"), LargeItem);
	if (!UnitItem || !LargeItem)
	{
		return false;
	}

	FRpgInventoryMutationRequest SwapRequest = MakePlacementRequest(
		ERpgInventoryMutationOperation::Swap,
		LargeItem,
		Root,
		Root,
		0,
		0);
	const FRpgInventoryMutationResult SwapPlan = Inventory->PlanInventoryMutation(SwapRequest);
	TestEqual(TEXT("The asymmetric displacement plans successfully"), SwapPlan.Code, ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The plan exposes both concrete moves"), SwapPlan.Deltas.Num(), 2);
	if (SwapPlan.Deltas.Num() == 2)
	{
		const FRpgInventoryMutationDelta* UnitDelta = SwapPlan.Deltas.FindByPredicate(
			[UnitItem](const FRpgInventoryMutationDelta& Delta)
			{
				return Delta.ItemId == UnitItem->GetItemId();
			});
		TestNotNull(TEXT("The displaced item has a planned delta"), UnitDelta);
		if (UnitDelta)
		{
			TestEqual(TEXT("The displaced item uses the first fully released column"), UnitDelta->AfterPlacement.X, 3);
			TestFalse(TEXT("Planned final footprints never overlap"), UnitDelta->AfterPlacement.Overlaps(SwapPlan.Deltas[0].ItemId == UnitItem->GetItemId()
				? SwapPlan.Deltas[1].AfterPlacement
				: SwapPlan.Deltas[0].AfterPlacement));
		}
	}

	const FRpgInventoryMutationResult SwapCommit = Inventory->ExecuteInventoryMutation(SwapRequest);
	TestEqual(TEXT("The asymmetric displacement commits atomically"), SwapCommit.Code, ERpgInventoryMutationResultCode::Success);

	FRpgInventoryEntryView UnitView;
	FRpgInventoryEntryView LargeView;
	TestTrue(TEXT("The displaced item remains addressable"), GetEntryView(Inventory, UnitItem->GetItemId(), UnitView));
	TestTrue(TEXT("The moving item remains addressable"), GetEntryView(Inventory, LargeItem->GetItemId(), LargeView));
	TestEqual(TEXT("The large item reaches the requested origin"), LargeView.Placement.X, 0);
	TestEqual(TEXT("The 1x1 item lands in the released far-right cell"), UnitView.Placement.X, 3);
	TestFalse(TEXT("Committed final footprints never overlap"), UnitView.Placement.Overlaps(LargeView.Placement));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryNestedGraphValidationTest,
	"SurvivalRpg.Inventory.Graph.NestedDepthCycleAndDuplicateValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryNestedGraphValidationTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = TestWorld.CreateInventory(TEXT("NestedSource"));
	if (!TestNotNull(TEXT("Nested source inventory exists"), SourceInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	TArray<URpgInventoryItemInstance*> Bags;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Bags.Add(SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, Index, 0)));
	}
	URpgInventoryItemInstance* DepthFourItem = SourceInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 4, 0));
	URpgInventoryItemInstance* DepthFiveProbe = SourceInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 5, 0));
	bool bSeeded = DepthFourItem != nullptr && DepthFiveProbe != nullptr;
	for (URpgInventoryItemInstance* Bag : Bags)
	{
		bSeeded &= Bag != nullptr;
	}
	TestTrue(TEXT("Nested graph seed items were created"), bSeeded);
	if (!bSeeded)
	{
		return false;
	}

	const FRpgInventoryGraphSaveData RootGraph = SourceInventory->ExportInventoryGraph();
	FRpgInventoryGraphSaveData DepthFourGraph = RootGraph;
	bool bBuiltDepthFourGraph = SetSavedPlacement(DepthFourGraph, Bags[0]->GetItemId(), Root, 0, 0);
	bBuiltDepthFourGraph &= SetSavedPlacement(
		DepthFourGraph,
		Bags[1]->GetItemId(),
		FRpgInventoryContainerHandle::MakeItemOwned(Bags[0]->GetItemId(), BagContainerId, 1));
	bBuiltDepthFourGraph &= SetSavedPlacement(
		DepthFourGraph,
		Bags[2]->GetItemId(),
		FRpgInventoryContainerHandle::MakeItemOwned(Bags[1]->GetItemId(), BagContainerId, 2));
	bBuiltDepthFourGraph &= SetSavedPlacement(
		DepthFourGraph,
		Bags[3]->GetItemId(),
		FRpgInventoryContainerHandle::MakeItemOwned(Bags[2]->GetItemId(), BagContainerId, 3));
	bBuiltDepthFourGraph &= SetSavedPlacement(
		DepthFourGraph,
		DepthFourItem->GetItemId(),
		FRpgInventoryContainerHandle::MakeItemOwned(Bags[3]->GetItemId(), BagContainerId, 4));
	TestTrue(TEXT("Depth-four graph DTO was constructed"), bBuiltDepthFourGraph);

	URpgInventoryManagerComponent* DepthTarget = TestWorld.CreateInventory(TEXT("DepthTarget"));
	FRpgInventoryMutationResult DepthImportResult;
	TestTrue(TEXT("Four item-owned levels are accepted"), DepthTarget->ImportInventoryGraph(DepthFourGraph, DepthImportResult));
	TestEqual(TEXT("Accepted depth-four import reports success"), DepthImportResult.Code, ERpgInventoryMutationResultCode::Success);
	FRpgInventoryEntryView DepthFourView;
	TestTrue(TEXT("Depth-four item keeps its persistent identity"), GetEntryView(DepthTarget, DepthFourItem->GetItemId(), DepthFourView));
	TestEqual(TEXT("Imported item remains at depth four"), DepthFourView.Placement.GetContainerHandle().Depth, static_cast<uint8>(4));

	FRpgInventoryGraphSaveData DepthFiveGraph = DepthFourGraph;
	TestTrue(
		TEXT("Depth-five probe DTO was constructed"),
		SetSavedPlacement(
			DepthFiveGraph,
			DepthFiveProbe->GetItemId(),
			FRpgInventoryContainerHandle::MakeItemOwned(DepthFourItem->GetItemId(), BagContainerId, 5)));
	const FString BeforeDepthFiveImport = MakeInventorySignature(DepthTarget);
	FRpgInventoryMutationResult DepthFiveResult;
	TestFalse(TEXT("A fifth item-owned level is rejected"), DepthTarget->ImportInventoryGraph(DepthFiveGraph, DepthFiveResult));
	TestTrue(
		TEXT("Depth-five rejection uses a stable validation error"),
		DepthFiveResult.Code == ERpgInventoryMutationResultCode::InvalidRequest ||
		DepthFiveResult.Code == ERpgInventoryMutationResultCode::MaxDepthExceeded);
	TestEqual(TEXT("Rejected depth-five import leaves the last valid graph intact"), MakeInventorySignature(DepthTarget), BeforeDepthFiveImport);

	FRpgInventoryGraphSaveData CycleGraph = RootGraph;
	TestTrue(
		TEXT("First half of cycle DTO was constructed"),
		SetSavedPlacement(
			CycleGraph,
			Bags[0]->GetItemId(),
			FRpgInventoryContainerHandle::MakeItemOwned(Bags[1]->GetItemId(), BagContainerId, 1)));
	TestTrue(
		TEXT("Second half of cycle DTO was constructed"),
		SetSavedPlacement(
			CycleGraph,
			Bags[1]->GetItemId(),
			FRpgInventoryContainerHandle::MakeItemOwned(Bags[0]->GetItemId(), BagContainerId, 1)));
	FRpgInventoryMutationResult CycleResult;
	TestFalse(TEXT("Mutually owning containers are rejected"), DepthTarget->ImportInventoryGraph(CycleGraph, CycleResult));
	TestEqual(TEXT("Cycle rejection is distinguishable for UI/save diagnostics"), CycleResult.Code, ERpgInventoryMutationResultCode::CycleDetected);
	TestEqual(TEXT("Cycle rejection is atomic"), MakeInventorySignature(DepthTarget), BeforeDepthFiveImport);

	FRpgInventoryGraphSaveData DuplicateGraph = RootGraph;
	FRpgInventorySavedItem DuplicateRow = DuplicateGraph.Items[0];
	DuplicateRow.Placement.X = 9;
	DuplicateGraph.Items.Add(DuplicateRow);
	FRpgInventoryMutationResult DuplicateResult;
	TestFalse(TEXT("Duplicate persistent item identities are rejected"), DepthTarget->ImportInventoryGraph(DuplicateGraph, DuplicateResult));
	TestEqual(TEXT("Duplicate id has an explicit result code"), DuplicateResult.Code, ERpgInventoryMutationResultCode::DuplicateItemId);
	TestEqual(TEXT("Duplicate-id rejection preserves the valid graph"), MakeInventorySignature(DepthTarget), BeforeDepthFiveImport);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCrossInventoryMultiStackPickupTest,
	"SurvivalRpg.Inventory.Transaction.CrossInventoryMultiStackPickupIsExplicitlyPartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCrossInventoryMultiStackPickupTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = TestWorld.CreateInventory(TEXT("MultiStackPickupSource"));
	URpgInventoryManagerComponent* TargetInventory = TestWorld.CreateInventory(TEXT("MultiStackPickupTarget"));
	if (!TestNotNull(TEXT("Multi-stack source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("Multi-stack target inventory exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* SourceStack = SourceInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		4,
		MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* FirstTargetStack = TargetInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		8,
		MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* SecondTargetStack = TargetInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		9,
		MakePlacement(Root, 1, 0));
	if (!TestNotNull(TEXT("Four-unit pickup source stack exists"), SourceStack) ||
		!TestNotNull(TEXT("First compatible target stack exists"), FirstTargetStack) ||
		!TestNotNull(TEXT("Second compatible target stack exists"), SecondTargetStack))
	{
		return false;
	}

	const FRpgInventoryItemId SourceItemId = SourceStack->GetItemId();
	const FRpgInventoryItemId FirstTargetItemId = FirstTargetStack->GetItemId();
	const FRpgInventoryItemId SecondTargetItemId = SecondTargetStack->GetItemId();
	bool bFilledRemainingCells = true;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			if (Y == 0 && (X == 0 || X == 1))
			{
				continue;
			}

			bFilledRemainingCells &= TargetInventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				1,
				MakePlacement(Root, X, Y)) != nullptr;
		}
	}
	TestTrue(TEXT("Every non-stack cell in the target grid was occupied"), bFilledRemainingCells);
	TestEqual(TEXT("The target grid has no free placement cells"), TargetInventory->GetUsedEntryCount(), 60);

	FRpgInventoryMutationRequest PickupRequest;
	PickupRequest.Operation = ERpgInventoryMutationOperation::Pickup;
	PickupRequest.ItemId = SourceItemId;
	PickupRequest.Source = Root;
	PickupRequest.Target = Root;
	PickupRequest.Quantity = 4;
	PickupRequest.RequestId = FGuid::NewGuid();

	const FString SourceBeforeAtomicAttempt = MakeInventorySignature(SourceInventory);
	const FString TargetBeforeAtomicAttempt = MakeInventorySignature(TargetInventory);
	const FRpgInventoryMutationResult AtomicResult = SourceInventory->ExecuteCrossInventoryTransfer(
		TargetInventory,
		PickupRequest,
		false);
	TestEqual(TEXT("A pickup that cannot apply its exact quantity rejects without explicit partial permission"), AtomicResult.Code, ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(TEXT("Rejected exact pickup applies no quantity"), AtomicResult.AppliedQuantity, 0);
	TestTrue(TEXT("Rejected exact pickup exposes no authoritative deltas"), AtomicResult.Deltas.IsEmpty());
	TestEqual(TEXT("Rejected exact pickup leaves the source graph unchanged"), MakeInventorySignature(SourceInventory), SourceBeforeAtomicAttempt);
	TestEqual(TEXT("Rejected exact pickup leaves every target stack unchanged"), MakeInventorySignature(TargetInventory), TargetBeforeAtomicAttempt);

	PickupRequest.RequestId = FGuid::NewGuid();
	const FRpgInventoryMutationResult PartialResult = SourceInventory->ExecuteCrossInventoryTransfer(
		TargetInventory,
		PickupRequest,
		true);
	TestEqual(TEXT("Explicit partial pickup reports PartiallyApplied"), PartialResult.Code, ERpgInventoryMutationResultCode::PartiallyApplied);
	TestEqual(TEXT("Two compatible stacks accept their exact combined free capacity"), PartialResult.AppliedQuantity, 3);

	URpgInventoryItemInstance* RemainingSourceStack = SourceInventory->FindItemById(SourceItemId);
	URpgInventoryItemInstance* FilledFirstTargetStack = TargetInventory->FindItemById(FirstTargetItemId);
	URpgInventoryItemInstance* FilledSecondTargetStack = TargetInventory->FindItemById(SecondTargetItemId);
	TestNotNull(TEXT("The exact world-loot remainder keeps its source identity"), RemainingSourceStack);
	TestNotNull(TEXT("The first merged stack keeps its identity"), FilledFirstTargetStack);
	TestNotNull(TEXT("The second merged stack keeps its identity"), FilledSecondTargetStack);
	if (RemainingSourceStack && FilledFirstTargetStack && FilledSecondTargetStack)
	{
		TestEqual(TEXT("One unit remains in world loot"), SourceInventory->GetItemStackCount(RemainingSourceStack), 1);
		TestEqual(TEXT("The first target stack reaches ten"), TargetInventory->GetItemStackCount(FilledFirstTargetStack), 10);
		TestEqual(TEXT("The second target stack reaches ten"), TargetInventory->GetItemStackCount(FilledSecondTargetStack), 10);
	}
	TestEqual(TEXT("Partial merging does not require or create a target placement"), TargetInventory->GetUsedEntryCount(), 60);

	const FString SourceAfterPartialPickup = MakeInventorySignature(SourceInventory);
	const FString TargetAfterPartialPickup = MakeInventorySignature(TargetInventory);
	const FRpgInventoryMutationResult RetriedResult = SourceInventory->ExecuteCrossInventoryTransfer(
		TargetInventory,
		PickupRequest,
		true);
	TestEqual(TEXT("A reliable pickup retry returns the cached partial result"), RetriedResult.Code, PartialResult.Code);
	TestEqual(TEXT("A pickup retry does not consume the exact remainder twice"), MakeInventorySignature(SourceInventory), SourceAfterPartialPickup);
	TestEqual(TEXT("A pickup retry does not merge into target stacks twice"), MakeInventorySignature(TargetInventory), TargetAfterPartialPickup);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryCrossInventoryNestedSubtreeTest,
	"SurvivalRpg.Inventory.Transaction.CrossInventoryNestedSubtreeIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCrossInventoryNestedSubtreeTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = TestWorld.CreateInventory(TEXT("NestedTransferSource"));
	URpgInventoryManagerComponent* BlockedTarget = TestWorld.CreateInventory(TEXT("NestedTransferBlockedTarget"));
	URpgInventoryManagerComponent* OpenTarget = TestWorld.CreateInventory(TEXT("NestedTransferOpenTarget"));
	if (!TestNotNull(TEXT("Nested-transfer source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("Blocked nested-transfer target exists"), BlockedTarget) ||
		!TestNotNull(TEXT("Open nested-transfer target exists"), OpenTarget))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* SourceBag = SourceInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("Source bag exists"), SourceBag))
	{
		return false;
	}

	const FRpgInventoryItemId BagItemId = SourceBag->GetItemId();
	const FRpgInventoryContainerHandle BagContents = FRpgInventoryContainerHandle::MakeItemOwned(
		BagItemId,
		BagContainerId,
		1);
	URpgInventoryItemInstance* SourceChild = SourceInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		7,
		MakePlacement(BagContents, 2, 1));
	if (!TestNotNull(TEXT("A concrete child stack exists inside the bag"), SourceChild))
	{
		return false;
	}
	SourceChild->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 3);
	const FRpgInventoryItemId ChildItemId = SourceChild->GetItemId();

	bool bFilledBlockedTarget = true;
	for (int32 Y = 0; Y < 6; ++Y)
	{
		for (int32 X = 0; X < 10; ++X)
		{
			bFilledBlockedTarget &= BlockedTarget->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				1,
				MakePlacement(Root, X, Y)) != nullptr;
		}
	}
	TestTrue(TEXT("Blocked transfer target was filled completely"), bFilledBlockedTarget);

	FRpgInventoryMutationRequest TransferRequest;
	TransferRequest.Operation = ERpgInventoryMutationOperation::Transfer;
	TransferRequest.ItemId = BagItemId;
	TransferRequest.Source = Root;
	TransferRequest.Target = Root;
	TransferRequest.Quantity = 1;
	TransferRequest.RequestId = FGuid::NewGuid();

	const FString SourceBeforeBlockedTransfer = MakeInventorySignature(SourceInventory);
	const FString BlockedTargetBeforeTransfer = MakeInventorySignature(BlockedTarget);
	const FRpgInventoryMutationResult BlockedResult = SourceInventory->ExecuteCrossInventoryTransfer(
		BlockedTarget,
		TransferRequest,
		true);
	TestEqual(TEXT("A filled target rejects the complete container subtree"), BlockedResult.Code, ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(TEXT("A blocked subtree transfer applies no quantity"), BlockedResult.AppliedQuantity, 0);
	TestTrue(TEXT("A blocked subtree transfer exposes no misleading deltas"), BlockedResult.Deltas.IsEmpty());
	TestEqual(TEXT("The bag and every descendant remain in the source on rejection"), MakeInventorySignature(SourceInventory), SourceBeforeBlockedTransfer);
	TestEqual(TEXT("The blocked target remains byte-for-byte unchanged"), MakeInventorySignature(BlockedTarget), BlockedTargetBeforeTransfer);

	TransferRequest.RequestId = FGuid::NewGuid();
	const FRpgInventoryMutationResult TransferResult = SourceInventory->ExecuteCrossInventoryTransfer(
		OpenTarget,
		TransferRequest,
		false);
	TestEqual(TEXT("A complete nested subtree transfers successfully when the root fits"), TransferResult.Code, ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The whole bag quantity is applied"), TransferResult.AppliedQuantity, 1);
	TestEqual(TEXT("The source no longer owns the bag or its child"), SourceInventory->GetUsedEntryCount(), 0);
	TestEqual(TEXT("The target reconstructs the root and its descendant"), OpenTarget->GetUsedEntryCount(), 2);
	TestNull(TEXT("The source cannot resolve the transferred bag id"), SourceInventory->FindItemById(BagItemId));
	TestNull(TEXT("The source cannot resolve the transferred child id"), SourceInventory->FindItemById(ChildItemId));

	URpgInventoryItemInstance* TransferredBag = OpenTarget->FindItemById(BagItemId);
	URpgInventoryItemInstance* TransferredChild = OpenTarget->FindItemById(ChildItemId);
	TestNotNull(TEXT("The target preserves the bag's persistent id"), TransferredBag);
	TestNotNull(TEXT("The target preserves the child's persistent id"), TransferredChild);
	if (TransferredBag && TransferredChild)
	{
		TestTrue(TEXT("Cross-inventory transfer reconstructs the bag UObject"), TransferredBag != SourceBag);
		TestTrue(TEXT("Cross-inventory transfer reconstructs the child UObject"), TransferredChild != SourceChild);
		TestEqual(TEXT("The reconstructed bag is outered to the durable target actor"), TransferredBag->GetOuter(), static_cast<UObject*>(OpenTarget->GetOwner()));
		TestEqual(TEXT("The reconstructed child is outered to the durable target actor"), TransferredChild->GetOuter(), static_cast<UObject*>(OpenTarget->GetOwner()));
		TestEqual(TEXT("The child stack quantity survives transfer"), OpenTarget->GetItemStackCount(TransferredChild), 7);
		TestEqual(TEXT("The child's runtime StatTags survive transfer"), TransferredChild->GetStatTagStackCount(RpgGameplayTags::Ability_Attack_Basic), 3);

		FRpgInventoryEntryView BagView;
		FRpgInventoryEntryView ChildView;
		TestTrue(TEXT("Transferred bag placement resolves by id"), GetEntryView(OpenTarget, BagItemId, BagView));
		TestTrue(TEXT("Transferred child placement resolves by id"), GetEntryView(OpenTarget, ChildItemId, ChildView));
		TestEqual(TEXT("Transferred bag is placed in the target root"), BagView.Placement.GetContainerHandle(), Root);
		TestEqual(TEXT("The child remains owned by the transferred bag"), ChildView.Placement.GetContainerHandle().ItemOwnerId, BagItemId);
		TestEqual(TEXT("The child remains one item-owned level below the root"), ChildView.Placement.GetContainerHandle().Depth, static_cast<uint8>(1));
		TestEqual(TEXT("The child's inner-grid X coordinate survives transfer"), ChildView.Placement.X, 2);
		TestEqual(TEXT("The child's inner-grid Y coordinate survives transfer"), ChildView.Placement.Y, 1);
	}

	const FString TargetAfterTransfer = MakeInventorySignature(OpenTarget);
	const FRpgInventoryMutationResult RetriedResult = SourceInventory->ExecuteCrossInventoryTransfer(
		OpenTarget,
		TransferRequest,
		false);
	TestEqual(TEXT("A reliable nested-transfer retry returns the cached success"), RetriedResult.Code, TransferResult.Code);
	TestEqual(TEXT("A nested-transfer retry cannot duplicate the subtree"), MakeInventorySignature(OpenTarget), TargetAfterTransfer);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryGraphPersistenceRoundTripTest,
	"SurvivalRpg.Inventory.Graph.IdentityAndRuntimeStateRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryGraphPersistenceRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory = TestWorld.CreateInventory(TEXT("PersistenceSource"));
	URpgInventoryManagerComponent* RestoredInventory = TestWorld.CreateInventory(TEXT("PersistenceTarget"));
	if (!TestNotNull(TEXT("Persistence source exists"), SourceInventory) ||
		!TestNotNull(TEXT("Persistence target exists"), RestoredInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* SourceItem = SourceInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		7,
		MakePlacement(Root, 4, 2));
	if (!TestNotNull(TEXT("Persistent source item exists"), SourceItem))
	{
		return false;
	}
	// Insert in non-lexical order so v2 must canonicalize tag rows before writing the payload.
	SourceItem->AddStatTagStack(RpgGameplayTags::Ability_Support_Heal, 2);
	SourceItem->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 3);

	const FRpgInventoryItemId PersistentId = SourceItem->GetItemId();
	const FRpgInventoryGraphSaveData ExportedGraph = SourceInventory->ExportInventoryGraph();
	TestEqual(TEXT("One saved graph row is exported"), ExportedGraph.Items.Num(), 1);
	const FRpgInventorySavedItem* ExportedItem = FindSavedItem(ExportedGraph, PersistentId);
	TestNotNull(TEXT("Export resolves the item by persistent id"), ExportedItem);
	if (!ExportedItem)
	{
		return false;
	}
	TestTrue(TEXT("Core runtime state payload is exported"), !ExportedItem->RuntimeState.IsEmpty());
	const FRpgInventoryFragmentStatePayload* ExportedCorePayload = ExportedItem->RuntimeState.FindByPredicate(
		[](const FRpgInventoryFragmentStatePayload& Payload)
		{
			return Payload.FragmentId == FName(TEXT("Inventory.Core.StatTags"));
		});
	TestNotNull(TEXT("Core StatTags payload has a stable fragment id"), ExportedCorePayload);
	if (!ExportedCorePayload)
	{
		return false;
	}
	TestEqual(TEXT("New exports use deterministic core payload v2"), ExportedCorePayload->Version, 2);

	FRpgInventoryMutationResult ImportResult;
	TestTrue(TEXT("A fully validated graph imports atomically"), RestoredInventory->ImportInventoryGraph(ExportedGraph, ImportResult));
	TestEqual(TEXT("Round-trip import reports success"), ImportResult.Code, ERpgInventoryMutationResultCode::Success);
	URpgInventoryItemInstance* RestoredItem = RestoredInventory->FindItemById(PersistentId);
	TestNotNull(TEXT("Restored inventory resolves the original persistent id"), RestoredItem);
	if (!RestoredItem)
	{
		return false;
	}
	TestTrue(TEXT("Restore reconstructs a new UObject under the target inventory owner"), RestoredItem != SourceItem);
	TestEqual(TEXT("Restore preserves the static item definition"), RestoredItem->GetItemDef(), SourceItem->GetItemDef());
	TestEqual(TEXT("Restore preserves stack quantity"), RestoredInventory->GetItemStackCount(RestoredItem), 7);
	TestEqual(TEXT("Restore preserves core StatTag runtime state"), RestoredItem->GetStatTagStackCount(RpgGameplayTags::Ability_Attack_Basic), 3);
	TestEqual(TEXT("Restore preserves every core StatTag row"), RestoredItem->GetStatTagStackCount(RpgGameplayTags::Ability_Support_Heal), 2);
	FRpgInventoryEntryView RestoredView;
	TestTrue(TEXT("Restored placement is available by item id"), GetEntryView(RestoredInventory, PersistentId, RestoredView));
	TestEqual(TEXT("Restore preserves X"), RestoredView.Placement.X, 4);
	TestEqual(TEXT("Restore preserves Y"), RestoredView.Placement.Y, 2);

	const FRpgInventoryGraphSaveData ReExportedGraph = RestoredInventory->ExportInventoryGraph();
	const FRpgInventorySavedItem* ReExportedItem = FindSavedItem(ReExportedGraph, PersistentId);
	TestNotNull(TEXT("Re-export still uses the same persistent identity"), ReExportedItem);
	if (ReExportedItem)
	{
		TestEqual(TEXT("Runtime payload count survives a complete round trip"), ReExportedItem->RuntimeState.Num(), ExportedItem->RuntimeState.Num());
		bool bPayloadsEqual = ReExportedItem->RuntimeState.Num() == ExportedItem->RuntimeState.Num();
		for (int32 Index = 0; bPayloadsEqual && Index < ExportedItem->RuntimeState.Num(); ++Index)
		{
			const FRpgInventoryFragmentStatePayload& Before = ExportedItem->RuntimeState[Index];
			const FRpgInventoryFragmentStatePayload& After = ReExportedItem->RuntimeState[Index];
			bPayloadsEqual = Before.FragmentId == After.FragmentId &&
				Before.Version == After.Version &&
				Before.Payload == After.Payload;
		}
		TestTrue(TEXT("Deterministic v2 runtime payload bytes survive export/import/export"), bPayloadsEqual);

		URpgInventoryManagerComponent* SecondRestoredInventory = TestWorld.CreateInventory(TEXT("PersistenceSecondTarget"));
		FRpgInventoryMutationResult SecondImportResult;
		TestTrue(
			TEXT("Re-exported runtime payload remains importable"),
			SecondRestoredInventory->ImportInventoryGraph(ReExportedGraph, SecondImportResult));
		URpgInventoryItemInstance* SecondRestoredItem = SecondRestoredInventory->FindItemById(PersistentId);
		TestNotNull(TEXT("Second restore still resolves the persistent id"), SecondRestoredItem);
		if (SecondRestoredItem)
		{
			TestEqual(
				TEXT("Repeated round trips preserve semantic StatTag state"),
				SecondRestoredItem->GetStatTagStackCount(RpgGameplayTags::Ability_Attack_Basic),
				3);
			TestEqual(
				TEXT("Repeated round trips preserve the second semantic StatTag"),
				SecondRestoredItem->GetStatTagStackCount(RpgGameplayTags::Ability_Support_Heal),
				2);
		}
	}

	FRpgInventoryGraphSaveData LegacyV1Graph = ExportedGraph;
	FRpgInventorySavedItem* LegacyV1Item = FindSavedItem(LegacyV1Graph, PersistentId);
	FRpgInventoryFragmentStatePayload* LegacyV1CorePayload = LegacyV1Item
		? LegacyV1Item->RuntimeState.FindByPredicate(
			[](const FRpgInventoryFragmentStatePayload& Payload)
			{
				return Payload.FragmentId == FName(TEXT("Inventory.Core.StatTags"));
			})
		: nullptr;
	TestNotNull(TEXT("Legacy migration fixture resolves the core payload"), LegacyV1CorePayload);
	if (LegacyV1CorePayload)
	{
		LegacyV1CorePayload->Version = 1;
		LegacyV1CorePayload->Payload.Reset();
		FGameplayTagStackContainer LegacyStatTags;
		LegacyStatTags.AddStack(RpgGameplayTags::Ability_Support_Heal, 2);
		LegacyStatTags.AddStack(RpgGameplayTags::Ability_Attack_Basic, 3);
		FMemoryWriter LegacyWriter(LegacyV1CorePayload->Payload, true);
		FGameplayTagStackContainer::StaticStruct()->SerializeItem(LegacyWriter, &LegacyStatTags, nullptr);
		TestFalse(TEXT("Legacy v1 fixture serializes without archive errors"), LegacyWriter.IsError());

		URpgInventoryManagerComponent* LegacyRestoredInventory = TestWorld.CreateInventory(TEXT("PersistenceLegacyTarget"));
		FRpgInventoryMutationResult LegacyImportResult;
		TestTrue(
			TEXT("Legacy FastArray-based core payload v1 remains importable"),
			LegacyRestoredInventory->ImportInventoryGraph(LegacyV1Graph, LegacyImportResult));
		URpgInventoryItemInstance* LegacyRestoredItem = LegacyRestoredInventory->FindItemById(PersistentId);
		TestNotNull(TEXT("Legacy v1 import preserves persistent identity"), LegacyRestoredItem);
		if (LegacyRestoredItem)
		{
			TestEqual(
				TEXT("Legacy v1 migration preserves semantic StatTag state"),
				LegacyRestoredItem->GetStatTagStackCount(RpgGameplayTags::Ability_Attack_Basic),
				3);
			TestEqual(
				TEXT("Legacy v1 migration preserves every StatTag row"),
				LegacyRestoredItem->GetStatTagStackCount(RpgGameplayTags::Ability_Support_Heal),
				2);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
