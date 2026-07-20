#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgPlayerInventoryLayoutComponent.h"
#include "RpgInventoryDragDrop.h"
#include "RpgInventoryEquipmentPlacementPolicy.h"
#include "RpgInventoryInteractionSession.h"
#include "RpgInventoryContainerActor.h"
#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryItemUseContext.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/Systems/GameplayTagStack.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
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

	URpgInventoryEntryViewModel* MakeEntryViewModel(
		UObject* Outer,
		URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId)
	{
		FRpgInventoryEntryView Entry;
		if (!Outer || !GetEntryView(Inventory, ItemId, Entry))
		{
			return nullptr;
		}

		URpgInventoryEntryViewModel* ViewModel =
			NewObject<URpgInventoryEntryViewModel>(Outer);
		TMap<
			TSubclassOf<URpgInventoryItemFragment>,
			TSubclassOf<URpgInventoryFragmentViewModel>> FragmentViewModels;
		ViewModel->InitializeFromEntry(Entry, FragmentViewModels);
		return ViewModel;
	}

	URpgInventoryAddressSlotViewModel* MakeAddressViewModel(
		UObject* Outer,
		URpgInventoryManagerComponent* Inventory,
		URpgPlayerInventoryLayoutComponent* InventoryLayout,
		const FRpgInventoryContainerHandle& ContainerHandle,
		int32 X,
		int32 Y)
	{
		if (!Outer || !Inventory || !InventoryLayout)
		{
			return nullptr;
		}

		const TArray<FRpgInventorySlotGroupView> Groups =
			InventoryLayout->GetSlotGroups();
		const FRpgInventorySlotGroupView* Group = Groups.FindByPredicate(
			[&ContainerHandle](const FRpgInventorySlotGroupView& Candidate)
			{
				return Candidate.ContainerHandle == ContainerHandle;
			});
		if (!Group || !Group->ContainsCell(X, Y))
		{
			return nullptr;
		}

		URpgInventoryAddressSlotViewModel* ViewModel =
			NewObject<URpgInventoryAddressSlotViewModel>(Outer);
		ViewModel->InitializeSlot(
			Inventory,
			InventoryLayout,
			*Group,
			X,
			Y);
		return ViewModel;
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
	FRpgInventoryContextActionPolicyTest,
	"SurvivalRpg.Inventory.ContextActions.SourceSemanticsAndStaleState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryContextActionPolicyTest::RunTest(
	const FString& Parameters)
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
		TEXT("ContextActionPolicyController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("ContextActionPolicyPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The context-policy controller exists"), Controller) ||
		!TestNotNull(TEXT("The context-policy player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	FActorSpawnParameters ControllerPawnSpawnParameters;
	ControllerPawnSpawnParameters.Name = MakeUniqueObjectName(
		World,
		APawn::StaticClass(),
		TEXT("ContextActionPolicyPawn"));
	ControllerPawnSpawnParameters.ObjectFlags = RF_Transient;
	ControllerPawnSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* ControllerPawn =
		World->SpawnActor<APawn>(ControllerPawnSpawnParameters);
	if (!TestNotNull(
			TEXT("The context-policy controller pawn exists"),
			ControllerPawn))
	{
		return false;
	}
	Controller->Possess(ControllerPawn);

	URpgInventoryManagerComponent* PlayerInventory =
		PlayerState->GetInventoryManagerComponent();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		Controller->GetPlayerInventoryLayoutComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		Controller->GetEquipmentLoadoutComponent();
	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(
			Controller,
			Controller);
	if (!TestNotNull(TEXT("The player inventory exists"), PlayerInventory) ||
		!TestNotNull(TEXT("The player layout exists"), InventoryLayout) ||
		!TestNotNull(TEXT("The inventory action gateway exists"), UiActions) ||
		!TestNotNull(TEXT("The equipment loadout projection exists"), EquipmentLoadout) ||
		!TestNotNull(TEXT("The screen-local context coordinator exists"), Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);
	TestTrue(
		TEXT("The authoritative gateway recognizes the owned player inventory"),
		UiActions->CanAccessInventory(PlayerInventory));

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle WeaponSlot1 =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	const FRpgInventoryContainerHandle WeaponSlot2 =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::WeaponSlot2GroupId);
	URpgInventoryItemInstance* StackItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4,
			MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* UsableItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUsableItemDefinition::StaticClass(),
			2,
			MakePlacement(Pockets, 1, 0));
	URpgInventoryItemInstance* NoDropItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestNoDropItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 2, 0));
	URpgInventoryItemInstance* Weapon =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 3, 0));
	URpgInventoryItemInstance* Bag =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 1));
	if (!TestNotNull(TEXT("A player stack fixture exists"), StackItem) ||
		!TestNotNull(TEXT("A usable player item exists"), UsableItem) ||
		!TestNotNull(TEXT("A no-drop player item exists"), NoDropItem) ||
		!TestNotNull(TEXT("A player weapon fixture exists"), Weapon) ||
		!TestNotNull(TEXT("A player bag fixture exists"), Bag))
	{
		return false;
	}

	URpgInventoryEntryViewModel* StackViewModel =
		MakeEntryViewModel(
			Coordinator,
			PlayerInventory,
			StackItem->GetItemId());
	URpgInventoryEntryViewModel* UsableViewModel =
		MakeEntryViewModel(
			Coordinator,
			PlayerInventory,
			UsableItem->GetItemId());
	URpgInventoryEntryViewModel* NoDropViewModel =
		MakeEntryViewModel(
			Coordinator,
			PlayerInventory,
			NoDropItem->GetItemId());
	URpgInventoryEntryViewModel* WeaponViewModel =
		MakeEntryViewModel(
			Coordinator,
			PlayerInventory,
			Weapon->GetItemId());
	URpgInventoryEntryViewModel* BagViewModel =
		MakeEntryViewModel(
			Coordinator,
			PlayerInventory,
			Bag->GetItemId());
	URpgInventoryAddressSlotViewModel* ContentAddress =
		MakeAddressViewModel(
			Coordinator,
			PlayerInventory,
			InventoryLayout,
			Pockets,
			0,
			0);
	URpgInventoryAddressSlotViewModel* UsableAddress =
		MakeAddressViewModel(
			Coordinator,
			PlayerInventory,
			InventoryLayout,
			Pockets,
			1,
			0);
	if (!TestNotNull(TEXT("The stack entry projection exists"), StackViewModel) ||
		!TestNotNull(TEXT("The usable entry projection exists"), UsableViewModel) ||
		!TestNotNull(TEXT("The no-drop entry projection exists"), NoDropViewModel) ||
		!TestNotNull(TEXT("The weapon entry projection exists"), WeaponViewModel) ||
		!TestNotNull(TEXT("The bag entry projection exists"), BagViewModel) ||
		!TestNotNull(TEXT("The content address projection exists"), ContentAddress) ||
		!TestNotNull(TEXT("The usable content-address projection exists"), UsableAddress))
	{
		return false;
	}

	const ERpgInventoryContextAction AllActions[] =
	{
		ERpgInventoryContextAction::OpenContainer,
		ERpgInventoryContextAction::Inspect,
		ERpgInventoryContextAction::Use,
		ERpgInventoryContextAction::EquipAndActivate,
		ERpgInventoryContextAction::MoveToCarry,
		ERpgInventoryContextAction::Split,
		ERpgInventoryContextAction::Rotate,
		ERpgInventoryContextAction::QuickAccessBind,
		ERpgInventoryContextAction::QuickAccessUnbind,
		ERpgInventoryContextAction::Transfer,
		ERpgInventoryContextAction::Drop,
		ERpgInventoryContextAction::Unequip
	};
	auto TestEntryContract =
		[this, Coordinator, &AllActions](
			const TCHAR* SourceLabel,
			URpgInventoryEntryViewModel* ViewModel,
			bool bSupportsSpatialRotation)
		{
			const TArray<ERpgInventoryContextAction> Available =
				Coordinator->GetAvailableContextActions(
					ViewModel,
					bSupportsSpatialRotation);
			TSet<ERpgInventoryContextAction> UniqueActions;
			for (const ERpgInventoryContextAction Action : Available)
			{
				UniqueActions.Add(Action);
			}
			TestEqual(
				*FString::Printf(TEXT("%s actions contain no duplicates"), SourceLabel),
				UniqueActions.Num(),
				Available.Num());
			for (const ERpgInventoryContextAction Action : AllActions)
			{
				TestEqual(
					*FString::Printf(
						TEXT("%s availability equals CanExecute for action %d"),
						SourceLabel,
						static_cast<int32>(Action)),
					Available.Contains(Action),
					Coordinator->CanExecuteContextAction(
						ViewModel,
						Action,
						bSupportsSpatialRotation));
			}
		};
	auto TestAddressContract =
		[this, Coordinator, &AllActions](
			const TCHAR* SourceLabel,
			URpgInventoryAddressSlotViewModel* ViewModel,
			bool bSupportsSpatialRotation)
		{
			const TArray<ERpgInventoryContextAction> Available =
				Coordinator->GetAvailableContextActions(
					ViewModel,
					bSupportsSpatialRotation);
			TSet<ERpgInventoryContextAction> UniqueActions;
			for (const ERpgInventoryContextAction Action : Available)
			{
				UniqueActions.Add(Action);
			}
			TestEqual(
				*FString::Printf(TEXT("%s actions contain no duplicates"), SourceLabel),
				UniqueActions.Num(),
				Available.Num());
			for (const ERpgInventoryContextAction Action : AllActions)
			{
				TestEqual(
					*FString::Printf(
						TEXT("%s availability equals CanExecute for action %d"),
						SourceLabel,
						static_cast<int32>(Action)),
					Available.Contains(Action),
					Coordinator->CanExecuteContextAction(
						ViewModel,
						Action,
						bSupportsSpatialRotation));
			}
		};
	auto TestExactActionOrder =
		[this](
			const TCHAR* SourceLabel,
			const TArray<ERpgInventoryContextAction>& Actual,
			const TArray<ERpgInventoryContextAction>& Expected)
		{
			TestEqual(
				*FString::Printf(TEXT("%s exposes the exact action count"), SourceLabel),
				Actual.Num(),
				Expected.Num());
			const int32 ComparedActionCount =
				FMath::Min(Actual.Num(), Expected.Num());
			for (int32 ActionIndex = 0;
				ActionIndex < ComparedActionCount;
				++ActionIndex)
			{
				TestEqual(
					*FString::Printf(
						TEXT("%s action %d keeps the canonical display order"),
						SourceLabel,
						ActionIndex),
					Actual[ActionIndex],
					Expected[ActionIndex]);
			}
		};
	TestEntryContract(TEXT("Player stack"), StackViewModel, true);
	TestEntryContract(TEXT("Player usable"), UsableViewModel, true);
	TestEntryContract(TEXT("Player weapon"), WeaponViewModel, true);
	TestEntryContract(TEXT("Player bag"), BagViewModel, true);
	TestAddressContract(TEXT("Player content stack"), ContentAddress, true);
	int32 ResolvedFixtureSplitCount = 0;
	FRpgInventoryGridPlacement ResolvedFixtureSplitPlacement;
	TestTrue(
		TEXT("The authoritative split preflight finds real player-content space"),
		UiActions->CanSplitItemStack(
			PlayerInventory,
			StackItem,
			0,
			FRpgInventoryGridPlacement(),
			ResolvedFixtureSplitCount,
			ResolvedFixtureSplitPlacement));

	const TArray<ERpgInventoryContextAction> ExpectedPlayerStackActions =
	{
		ERpgInventoryContextAction::Inspect,
		ERpgInventoryContextAction::Split,
		ERpgInventoryContextAction::Rotate,
		ERpgInventoryContextAction::Drop
	};
	const TArray<ERpgInventoryContextAction> PlayerStackEntryActions =
		Coordinator->GetAvailableContextActions(StackViewModel, true);
	const TArray<ERpgInventoryContextAction> PlayerStackAddressActions =
		Coordinator->GetAvailableContextActions(ContentAddress, true);
	TestExactActionOrder(
		TEXT("Player stack entry"),
		PlayerStackEntryActions,
		ExpectedPlayerStackActions);
	TestExactActionOrder(
		TEXT("Player stack address"),
		PlayerStackAddressActions,
		ExpectedPlayerStackActions);
	TestFalse(
		TEXT("A non-usable stack entry does not invent Quick Access semantics"),
		PlayerStackEntryActions.Contains(
			ERpgInventoryContextAction::QuickAccessBind));
	TestFalse(
		TEXT("A non-usable content address remains ineligible for Quick Access"),
		PlayerStackAddressActions.Contains(
			ERpgInventoryContextAction::QuickAccessBind));

	TestTrue(
		TEXT("A genuine stack action uses the shared split predictor"),
		Coordinator->CanExecuteContextAction(
			StackViewModel,
			ERpgInventoryContextAction::Split,
			true));
	TestTrue(
		TEXT("A rotatable spatial item exposes Rotate only to a spatial presenter"),
		Coordinator->CanExecuteContextAction(
			StackViewModel,
			ERpgInventoryContextAction::Rotate,
			true));
	TestFalse(
		TEXT("The same item does not expose Rotate to a non-spatial presenter"),
		Coordinator->CanExecuteContextAction(
			StackViewModel,
			ERpgInventoryContextAction::Rotate));
	TestTrue(
		TEXT("A configured usable item exposes Use in player inventory"),
		Coordinator->CanExecuteContextAction(
			UsableViewModel,
			ERpgInventoryContextAction::Use));
	TestTrue(
		TEXT("A player weapon exposes EquipAndActivate"),
		Coordinator->CanExecuteContextAction(
			WeaponViewModel,
			ERpgInventoryContextAction::EquipAndActivate));
	TestTrue(
		TEXT("A player weapon exposes MoveToCarry"),
		Coordinator->CanExecuteContextAction(
			WeaponViewModel,
			ERpgInventoryContextAction::MoveToCarry));
	TestFalse(
		TEXT("ManualDropPolicy Disabled removes Drop from the shared policy"),
		Coordinator->CanExecuteContextAction(
			NoDropViewModel,
			ERpgInventoryContextAction::Drop));
	TestTrue(
		TEXT("A bag exposes its nested-container presentation action"),
		Coordinator->CanExecuteContextAction(
			BagViewModel,
			ERpgInventoryContextAction::OpenContainer,
			true));
	TestFalse(
		TEXT("A Backpack/Belt bag does not advertise the hand-only MoveToCarry action"),
		Coordinator->CanExecuteContextAction(
			BagViewModel,
			ERpgInventoryContextAction::MoveToCarry,
			true));
	TestTrue(
		TEXT("A content address uses the same real split predictor"),
		Coordinator->CanExecuteContextAction(
			ContentAddress,
			ERpgInventoryContextAction::Split,
			true));
	TestAddressContract(
		TEXT("Unbound usable content address"),
		UsableAddress,
		true);
	const FRpgInventoryDragPayload UsableAddressPayload =
		URpgInventoryDragDropCoordinator::MakeInventoryPayloadFromAddressSlot(
			UsableAddress);
	TestTrue(
		TEXT("An eligible usable content address exposes Quick Access Bind"),
		Coordinator->CanExecuteContextAction(
			UsableAddress,
			ERpgInventoryContextAction::QuickAccessBind,
			true));
	TestFalse(
		TEXT("An unbound usable content address does not expose Quick Access Unbind"),
		Coordinator->CanExecuteContextAction(
			UsableAddress,
			ERpgInventoryContextAction::QuickAccessUnbind,
			true));
	TestTrue(
		TEXT("The eligible content address binds through the authoritative Quick Access gateway"),
		Coordinator->BindPayloadToQuickAccessSlot(
			UsableAddressPayload,
			0));
	TestAddressContract(
		TEXT("Bound usable content address"),
		UsableAddress,
		true);
	TestTrue(
		TEXT("The bound content address keeps Bind available for reassignment"),
		Coordinator->CanExecuteContextAction(
			UsableAddress,
			ERpgInventoryContextAction::QuickAccessBind,
			true));
	TestTrue(
		TEXT("The bound content address exposes Quick Access Unbind"),
		Coordinator->CanExecuteContextAction(
			UsableAddress,
			ERpgInventoryContextAction::QuickAccessUnbind,
			true));
	TestTrue(
		TEXT("The same stable content payload clears its Quick Access binding"),
		Coordinator->ClearQuickAccessBindingForPayload(
			UsableAddressPayload));
	TestFalse(
		TEXT("Quick Access Unbind disappears after the binding is cleared"),
		Coordinator->CanExecuteContextAction(
			UsableAddress,
			ERpgInventoryContextAction::QuickAccessUnbind,
			true));

	FActorSpawnParameters ExternalContainerSpawnParameters;
	ExternalContainerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryContainerActor::StaticClass(),
		TEXT("ContextActionExternalContainer"));
	ExternalContainerSpawnParameters.ObjectFlags = RF_Transient;
	ExternalContainerSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryContainerActor* ExternalContainer =
		World->SpawnActor<ARpgInventoryContainerActor>(
			ExternalContainerSpawnParameters);
	URpgInventoryManagerComponent* ExternalInventory = ExternalContainer
		? ExternalContainer->GetInventoryManager()
		: nullptr;
	const FRpgInventoryContainerHandle ExternalRoot = ExternalInventory
		? FRpgInventoryContainerHandle::MakeRoot(
			ExternalInventory->GetDefaultContainerId())
		: FRpgInventoryContainerHandle();
	URpgInventoryItemInstance* ExternalUsable =
		ExternalInventory
			? ExternalInventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestUsableItemDefinition::StaticClass(),
				2,
				MakePlacement(ExternalRoot, 0, 0))
			: nullptr;
	URpgInventoryItemInstance* ExternalWeapon =
		ExternalInventory
			? ExternalInventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
				1,
				MakePlacement(ExternalRoot, 1, 0))
			: nullptr;
	URpgInventoryEntryViewModel* ExternalUsableViewModel =
		ExternalUsable
			? MakeEntryViewModel(
				Coordinator,
				ExternalInventory,
				ExternalUsable->GetItemId())
			: nullptr;
	URpgInventoryEntryViewModel* ExternalWeaponViewModel =
		ExternalWeapon
			? MakeEntryViewModel(
				Coordinator,
				ExternalInventory,
				ExternalWeapon->GetItemId())
			: nullptr;
	if (!TestNotNull(TEXT("An accessible external container exists"), ExternalContainer) ||
		!TestNotNull(TEXT("An external inventory exists"), ExternalInventory) ||
		!TestNotNull(TEXT("An external usable item exists"), ExternalUsable) ||
		!TestNotNull(TEXT("An external weapon exists"), ExternalWeapon) ||
		!TestNotNull(TEXT("The external usable projection exists"), ExternalUsableViewModel) ||
		!TestNotNull(TEXT("The external weapon projection exists"), ExternalWeaponViewModel))
	{
		return false;
	}
	TestTrue(
		TEXT("The real nearby container authorizes the controller pawn"),
		UiActions->CanAccessInventory(ExternalInventory));

	Coordinator->SetQuickTransferTarget(
		ExternalInventory,
		PlayerInventory);
	TestEntryContract(TEXT("External usable"), ExternalUsableViewModel, true);
	TestEntryContract(TEXT("External weapon"), ExternalWeaponViewModel, true);
	TestTrue(
		TEXT("An accessible external entry exposes Transfer into player inventory"),
		Coordinator->CanExecuteContextAction(
			ExternalUsableViewModel,
			ERpgInventoryContextAction::Transfer,
			true));
	TestTrue(
		TEXT("External-to-player Transfer appears in the shared ordered action list"),
		Coordinator->GetAvailableContextActions(
				ExternalUsableViewModel,
				true)
			.Contains(ERpgInventoryContextAction::Transfer));
	TestFalse(
		TEXT("OnlyFromPlayerInventory suppresses Use for storage items"),
		Coordinator->CanExecuteContextAction(
			ExternalUsableViewModel,
			ERpgInventoryContextAction::Use));
	TestFalse(
		TEXT("External weapons do not advertise an intent the server rejects as WrongInventory"),
		Coordinator->CanExecuteContextAction(
			ExternalWeaponViewModel,
			ERpgInventoryContextAction::EquipAndActivate));
	TestFalse(
		TEXT("External weapons do not advertise MoveToCarry"),
		Coordinator->CanExecuteContextAction(
			ExternalWeaponViewModel,
			ERpgInventoryContextAction::MoveToCarry));
	const FString ExternalSignatureBeforeRejectedIntents =
		MakeInventorySignature(ExternalInventory);
	TestFalse(
		TEXT("The Blueprint-callable explicit Equip dispatcher reuses the shared policy"),
		Coordinator->ExecuteEntryItemAction(
			ExternalWeaponViewModel,
			ERpgInventoryItemActionIntent::EquipAndActivate));
	TestFalse(
		TEXT("The Blueprint-callable explicit Use dispatcher reuses the shared source policy"),
		Coordinator->ExecuteEntryItemAction(
			ExternalUsableViewModel,
			ERpgInventoryItemActionIntent::Use));
	TestEqual(
		TEXT("Locally rejected external intents do not mutate inventory state"),
		MakeInventorySignature(ExternalInventory),
		ExternalSignatureBeforeRejectedIntents);

	UiActions->RequestEquipInventoryItem(Weapon);
	FRpgInventoryEntryView EquippedWeaponEntry;
	if (!TestTrue(
			TEXT("The real authoritative equip path keeps the weapon addressable"),
			GetEntryView(
				PlayerInventory,
				Weapon->GetItemId(),
				EquippedWeaponEntry)) ||
		!TestEqual(
			TEXT("The equipped weapon reaches its MainHand Carry role"),
			EquippedWeaponEntry.Placement.GetContainerHandle(),
			WeaponSlot1))
	{
		return false;
	}
	TestTrue(
		TEXT("The old Pockets weapon projection fails closed after its placement changes"),
		Coordinator->GetAvailableContextActions(
				WeaponViewModel,
				true)
			.IsEmpty());
	TestFalse(
		TEXT("The direct dispatcher rejects the same placement-stale weapon projection"),
		Coordinator->ExecuteEntryItemAction(
			WeaponViewModel,
			ERpgInventoryItemActionIntent::MoveToCarry));

	URpgInventoryAddressSlotViewModel* CarryAddress =
		MakeAddressViewModel(
			Coordinator,
			PlayerInventory,
			InventoryLayout,
			WeaponSlot1,
			0,
			0);
	if (!TestNotNull(TEXT("The Carry address projection exists"), CarryAddress))
	{
		return false;
	}
	TestAddressContract(
		TEXT("Player Carry weapon"),
		CarryAddress,
		true);
	TestFalse(
		TEXT("Carry addresses never offer MoveToCarry again"),
		Coordinator->CanExecuteContextAction(
			CarryAddress,
			ERpgInventoryContextAction::MoveToCarry,
			true));
	TestFalse(
		TEXT("Carry addresses never offer Split"),
		Coordinator->CanExecuteContextAction(
			CarryAddress,
			ERpgInventoryContextAction::Split,
			true));

	const TArray<ERpgInventoryContextAction> EquipmentActions =
		Coordinator->GetAvailableContextActions(
			ERpgEquipmentSlot::MainHand,
			Weapon->GetItemId());
	TestTrue(
		TEXT("Equipment context exposes Inspect"),
		EquipmentActions.Contains(ERpgInventoryContextAction::Inspect));
	TestTrue(
		TEXT("Equipment context exposes the same physical Unequip action used by controller shortcuts"),
		EquipmentActions.Contains(ERpgInventoryContextAction::Unequip));
	TestTrue(
		TEXT("Equipment context exposes Drop when manual-drop policy permits it"),
		EquipmentActions.Contains(ERpgInventoryContextAction::Drop));
	for (const ERpgInventoryContextAction Action : AllActions)
	{
		TestEqual(
			*FString::Printf(
				TEXT("Equipment availability equals CanExecute for action %d"),
				static_cast<int32>(Action)),
			EquipmentActions.Contains(Action),
			Coordinator->CanExecuteContextAction(
				ERpgEquipmentSlot::MainHand,
				Weapon->GetItemId(),
				Action));
	}

	TestEqual(
		TEXT("The MainHand loadout projects the actively equipped weapon"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand),
		Weapon);
	URpgInventoryItemInstance* HolsteredWeapon =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 3, 0));
	URpgInventoryEntryViewModel* HolsteredWeaponViewModel =
		HolsteredWeapon
			? MakeEntryViewModel(
				Coordinator,
				PlayerInventory,
				HolsteredWeapon->GetItemId())
			: nullptr;
	if (!TestNotNull(TEXT("A second weapon exists for the holstered-slot regression"), HolsteredWeapon) ||
		!TestNotNull(TEXT("The second weapon has a current Pockets projection"), HolsteredWeaponViewModel))
	{
		return false;
	}
	const FRpgInventoryMutationRequest HolsterRequest =
		MakePlacementRequest(
			ERpgInventoryMutationOperation::Equip,
			HolsteredWeapon,
			Pockets,
			WeaponSlot2,
			0,
			0);
	TestTrue(
		TEXT("A current rotatable weapon payload can be held before its source moves"),
		Coordinator->BeginHoldFromEntry(HolsteredWeaponViewModel));
	TestTrue(
		TEXT("The current held source initially permits a rotation toggle"),
		Coordinator->CanToggleInteractionRotation());
	TestTrue(
		TEXT("The second weapon can occupy WeaponSlot2 without changing active MainHand"),
		PlayerInventory->ExecuteInventoryMutation(HolsterRequest).IsSuccess());
	TestTrue(
		TEXT("The regression retains the held payload after an out-of-band authoritative move"),
		Coordinator->HasHeldPayload());
	TestFalse(
		TEXT("A held payload with a placement-stale source cannot rotate"),
		Coordinator->CanToggleInteractionRotation());
	TestFalse(
		TEXT("The rotation dispatcher revalidates and rejects the stale held source"),
		Coordinator->ToggleInteractionRotation());
	Coordinator->ForceCancelInteraction();
	TestEqual(
		TEXT("The active MainHand assignment still points at the first weapon"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand),
		Weapon);
	TestFalse(
		TEXT("MainHand context rejects a holstered WeaponSlot2 item id"),
		Coordinator->CanExecuteContextAction(
			ERpgEquipmentSlot::MainHand,
			HolsteredWeapon->GetItemId(),
			ERpgInventoryContextAction::Inspect));
	const FString PlayerSignatureBeforeHolsteredDispatches =
		MakeInventorySignature(PlayerInventory);
	TestFalse(
		TEXT("Direct MainHand Unequip rejects the holstered WeaponSlot2 item id"),
		Coordinator->UnequipEquipmentItem(
			ERpgEquipmentSlot::MainHand,
			HolsteredWeapon->GetItemId()));
	TestFalse(
		TEXT("Direct MainHand Drop rejects the holstered WeaponSlot2 item id"),
		Coordinator->DropEquipmentItem(
			ERpgEquipmentSlot::MainHand,
			HolsteredWeapon->GetItemId()));
	TestEqual(
		TEXT("Rejected holstered MainHand dispatches leave inventory state unchanged"),
		MakeInventorySignature(PlayerInventory),
		PlayerSignatureBeforeHolsteredDispatches);

	const FString PlayerSignatureBeforeNoDrop =
		MakeInventorySignature(PlayerInventory);
	TestFalse(
		TEXT("The direct drop dispatcher also rejects ManualDropPolicy Disabled"),
		Coordinator->DropEntry(NoDropViewModel));
	TestEqual(
		TEXT("A locally rejected no-drop request leaves the player inventory unchanged"),
		MakeInventorySignature(PlayerInventory),
		PlayerSignatureBeforeNoDrop);

	TestTrue(
		TEXT("The authoritative fixture reduces the represented stack"),
		PlayerInventory->RemoveItemInstanceStack(StackItem, 3));
	TestTrue(
		TEXT("The stale view model retains the same item identity for the regression"),
		StackViewModel->GetItemInstance() == StackItem &&
			StackViewModel->GetItemInstance()->GetItemId() == StackItem->GetItemId());
	TestTrue(
		TEXT("A stale stack projection fails closed instead of executing a cached menu row"),
		Coordinator->GetAvailableContextActions(StackViewModel, true).IsEmpty());
	TestTrue(
		TEXT("The address projection for the changed stack also fails closed"),
		Coordinator->GetAvailableContextActions(ContentAddress, true).IsEmpty());
	TestFalse(
		TEXT("The direct split dispatcher also rejects the stale projection"),
		Coordinator->QuickSplitEntry(
			StackViewModel,
			FRpgInventoryGridPlacement(),
			1));
	TestFalse(
		TEXT("The direct address split dispatcher rejects the same stale source"),
		Coordinator->QuickSplitAddressSlot(
			ContentAddress,
			FRpgInventoryGridPlacement(),
			1));

	FRpgInventoryEntryView RefreshedStackEntry;
	if (!TestTrue(
			TEXT("The reduced stack remains addressable"),
			GetEntryView(
				PlayerInventory,
				StackItem->GetItemId(),
				RefreshedStackEntry)))
	{
		return false;
	}
	TMap<
		TSubclassOf<URpgInventoryItemFragment>,
		TSubclassOf<URpgInventoryFragmentViewModel>> FragmentViewModels;
	StackViewModel->InitializeFromEntry(
		RefreshedStackEntry,
		FragmentViewModels);
	TestTrue(
		TEXT("A refreshed one-unit projection exposes Inspect again"),
		Coordinator->CanExecuteContextAction(
			StackViewModel,
			ERpgInventoryContextAction::Inspect,
			true));
	TestFalse(
		TEXT("A refreshed one-unit projection no longer exposes Split"),
		Coordinator->CanExecuteContextAction(
			StackViewModel,
			ERpgInventoryContextAction::Split,
			true));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryLocalPlayerFeedbackRoutingTest,
	"SurvivalRpg.Inventory.Feedback.LocalPlayerRecipientRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryLocalPlayerFeedbackRoutingTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	FActorSpawnParameters FirstControllerParameters;
	FirstControllerParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("FeedbackRecipientController"));
	FirstControllerParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* RecipientController =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			FirstControllerParameters);

	FActorSpawnParameters OtherControllerParameters;
	OtherControllerParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("FeedbackOtherController"));
	OtherControllerParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* OtherController =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			OtherControllerParameters);
	URpgInventoryUiActionComponent* UiActions = RecipientController
		? RecipientController->GetInventoryUiActionComponent()
		: nullptr;
	if (!TestNotNull(
			TEXT("The feedback recipient controller exists"),
			RecipientController) ||
		!TestNotNull(
			TEXT("The second local controller exists"),
			OtherController) ||
		!TestNotNull(
			TEXT("The recipient owns its inventory action component"),
			UiActions))
	{
		return false;
	}

	int32 BroadcastCount = 0;
	FRpgInventoryActionFeedbackMessage CapturedMessage;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryActionFeedbackMessage>(
			RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
			[&BroadcastCount, &CapturedMessage](
				FGameplayTag Channel,
				const FRpgInventoryActionFeedbackMessage& Message)
			{
				++BroadcastCount;
				CapturedMessage = Message;
			});

	UiActions->RequestTransferItemStack(nullptr, nullptr, nullptr, 1);
	MessageSubsystem.UnregisterListener(ListenerHandle);

	TestEqual(
		TEXT("Exactly one owner-local feedback message is broadcast"),
		BroadcastCount,
		1);
	TestEqual(
		TEXT("Feedback is addressed to the action component owner"),
		CapturedMessage.Recipient.Get(),
		static_cast<APlayerController*>(RecipientController));
	TestTrue(
		TEXT("The owning local player accepts the feedback"),
		CapturedMessage.IsAddressedTo(RecipientController));
	TestFalse(
		TEXT("Another local player rejects the feedback"),
		CapturedMessage.IsAddressedTo(OtherController));

	FRpgInventoryActionFeedbackMessage LegacyMessage;
	TestTrue(
		TEXT("Legacy unaddressed feedback remains compatible"),
		LegacyMessage.IsAddressedTo(RecipientController));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryManualDropConfirmationAuthorityTest,
	"SurvivalRpg.Inventory.Drop.ConfirmationAuthorityAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryManualDropConfirmationAuthorityTest::RunTest(
	const FString& Parameters)
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
		TEXT("ManualDropConfirmationController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("ManualDropConfirmationPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The manual-drop controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The manual-drop player-state fixture exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	if (!TestTrue(TEXT("The manual-drop fixture executes on server authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The manual-drop player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The manual-drop action gateway exists"), UiActions))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass(),
			9,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(
			TEXT("A confirm-protected stackable item exists"),
			Item))
	{
		return false;
	}

	const FRpgInventoryItemId ItemId = Item->GetItemId();
	FRpgInventoryEntryView InitialEntry;
	if (!TestTrue(
			TEXT("The confirm-protected item has a stable replicated entry"),
			GetEntryView(Inventory, ItemId, InitialEntry)))
	{
		return false;
	}

	auto CountDroppedActors = [World]()
	{
		int32 Count = 0;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			if (*It && !It->IsPendingKillPending())
			{
				++Count;
			}
		}
		return Count;
	};
	auto CountDroppedUnits = [World]()
	{
		int32 Count = 0;
		for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
		{
			const ARpgDroppedInventoryActor* DropActor = *It;
			const URpgInventoryManagerComponent* DropInventory =
				DropActor && !DropActor->IsPendingKillPending()
					? DropActor->GetLootInventoryManager()
					: nullptr;
			if (!DropInventory)
			{
				continue;
			}
			for (const FRpgInventoryEntryView& Entry :
				DropInventory->GetAllEntries())
			{
				Count += Entry.StackCount;
			}
		}
		return Count;
	};

	TArray<FRpgInventoryActionFeedbackMessage> FeedbackMessages;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryActionFeedbackMessage>(
			RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
			[&FeedbackMessages](
				FGameplayTag Channel,
				const FRpgInventoryActionFeedbackMessage& Message)
			{
				FeedbackMessages.Add(Message);
			});

	FRpgInventoryManualDropRequest UnconfirmedRequest;
	UnconfirmedRequest.RequestId = FGuid::NewGuid();
	UnconfirmedRequest.EntryId = InitialEntry.EntryId;
	UnconfirmedRequest.ItemId = InitialEntry.ItemId;
	UnconfirmedRequest.ExpectedSourcePlacement = InitialEntry.Placement;
	UnconfirmedRequest.StackCount = 3;
	const int32 UnconfirmedFeedbackIndex = FeedbackMessages.Num();
	UiActions->RequestDropInventoryItemById(
		Inventory,
		UnconfirmedRequest);

	TestEqual(
		TEXT("An unconfirmed request emits exactly one owner-local result"),
		FeedbackMessages.Num(),
		UnconfirmedFeedbackIndex + 1);
	const FRpgInventoryActionFeedbackMessage* UnconfirmedFeedback =
		FeedbackMessages.IsValidIndex(UnconfirmedFeedbackIndex)
			? &FeedbackMessages[UnconfirmedFeedbackIndex]
			: nullptr;
	if (TestNotNull(
		TEXT("The unconfirmed request produced feedback"),
		UnconfirmedFeedback))
	{
		TestEqual(
			TEXT("The server requires confirmation for the weapon stack"),
			UnconfirmedFeedback->Result,
			ERpgInventoryActionFeedbackResult::RequiresConfirmation);
		TestEqual(
			TEXT("Confirmation feedback retains the caller's request id"),
			UnconfirmedFeedback->RequestId,
			UnconfirmedRequest.RequestId);
		TestTrue(
			TEXT("Confirmation feedback retains the persistent item id"),
			UnconfirmedFeedback->ItemId == ItemId);
		TestEqual(
			TEXT("Confirmation feedback retains the exact source inventory"),
			UnconfirmedFeedback->InventoryOwner.Get(),
			static_cast<UActorComponent*>(Inventory));
		TestEqual(
			TEXT("Confirmation feedback retains the exact requested quantity"),
			UnconfirmedFeedback->StackCount,
			UnconfirmedRequest.StackCount);
		TestTrue(
			TEXT("Confirmation feedback uses the semantic Drop action"),
			UnconfirmedFeedback->ActionTag ==
				RpgGameplayTags::Rpg_Inventory_Action_Drop);
		TestEqual(
			TEXT("Confirmation feedback is addressed to the requesting controller"),
			UnconfirmedFeedback->Recipient.Get(),
			static_cast<APlayerController*>(Controller));
	}
	TestEqual(
		TEXT("An unconfirmed request leaves the source stack unchanged"),
		Inventory->GetItemStackCount(Item),
		9);
	TestEqual(
		TEXT("An unconfirmed request spawns no dropped inventory actor"),
		CountDroppedActors(),
		0);

	FRpgInventoryManualDropRequest StaleSourceRequest =
		UnconfirmedRequest;
	StaleSourceRequest.RequestId = FGuid::NewGuid();
	StaleSourceRequest.ExpectedSourcePlacement.X += 1;
	StaleSourceRequest.bConfirmed = true;
	const int32 StaleFeedbackIndex = FeedbackMessages.Num();
	UiActions->RequestDropInventoryItemById(
		Inventory,
		StaleSourceRequest);
	TestEqual(
		TEXT("A stale source request emits exactly one rejection"),
		FeedbackMessages.Num(),
		StaleFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(StaleFeedbackIndex))
	{
		TestEqual(
			TEXT("A stale source placement is rejected"),
			FeedbackMessages[StaleFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::InvalidRequest);
	}
	TestEqual(
		TEXT("A stale source request cannot mutate the stack"),
		Inventory->GetItemStackCount(Item),
		9);
	TestEqual(
		TEXT("A stale source request cannot spawn a drop actor"),
		CountDroppedActors(),
		0);

	FRpgInventoryManualDropRequest OversizedRequest =
		UnconfirmedRequest;
	OversizedRequest.RequestId = FGuid::NewGuid();
	OversizedRequest.StackCount = 10;
	OversizedRequest.bConfirmed = true;
	const int32 OversizedFeedbackIndex = FeedbackMessages.Num();
	UiActions->RequestDropInventoryItemById(
		Inventory,
		OversizedRequest);
	TestEqual(
		TEXT("An oversized request emits exactly one rejection"),
		FeedbackMessages.Num(),
		OversizedFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(OversizedFeedbackIndex))
	{
		TestEqual(
			TEXT("A no-longer-available exact quantity is rejected"),
			FeedbackMessages[OversizedFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::InvalidRequest);
	}
	TestEqual(
		TEXT("An oversized request cannot mutate the stack"),
		Inventory->GetItemStackCount(Item),
		9);
	TestEqual(
		TEXT("An oversized request cannot spawn a drop actor"),
		CountDroppedActors(),
		0);

	FRpgInventoryManualDropRequest ConfirmedRequest =
		UnconfirmedRequest;
	ConfirmedRequest.RequestId = FGuid::NewGuid();
	ConfirmedRequest.bConfirmed = true;
	const int32 ConfirmedFeedbackIndex = FeedbackMessages.Num();
	UiActions->RequestDropInventoryItemById(
		Inventory,
		ConfirmedRequest);
	TestEqual(
		TEXT("A valid confirmed request emits exactly one result"),
		FeedbackMessages.Num(),
		ConfirmedFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(ConfirmedFeedbackIndex))
	{
		TestEqual(
			TEXT("The valid confirmed request succeeds"),
			FeedbackMessages[ConfirmedFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::Success);
		TestEqual(
			TEXT("Success feedback retains the fresh confirmed request id"),
			FeedbackMessages[ConfirmedFeedbackIndex].RequestId,
			ConfirmedRequest.RequestId);
	}
	TestEqual(
		TEXT("A valid confirmed request removes exactly three source units"),
		Inventory->GetItemStackCount(Item),
		6);
	TestEqual(
		TEXT("A valid confirmed request creates exactly one drop actor"),
		CountDroppedActors(),
		1);
	TestEqual(
		TEXT("The world drop contains exactly the confirmed quantity"),
		CountDroppedUnits(),
		3);

	const int32 ReplayFeedbackIndex = FeedbackMessages.Num();
	UiActions->RequestDropInventoryItemById(
		Inventory,
		ConfirmedRequest);
	TestEqual(
		TEXT("Replaying the exact confirmed request emits one cached result"),
		FeedbackMessages.Num(),
		ReplayFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(ReplayFeedbackIndex))
	{
		TestEqual(
			TEXT("The identical confirmed request replays its cached success"),
			FeedbackMessages[ReplayFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::Success);
		TestEqual(
			TEXT("The replay retains the original confirmed request id"),
			FeedbackMessages[ReplayFeedbackIndex].RequestId,
			ConfirmedRequest.RequestId);
	}
	TestEqual(
		TEXT("A confirmed request replay cannot remove another source quantity"),
		Inventory->GetItemStackCount(Item),
		6);
	TestEqual(
		TEXT("A confirmed request replay cannot spawn another drop actor"),
		CountDroppedActors(),
		1);
	TestEqual(
		TEXT("A confirmed request replay cannot add another dropped quantity"),
		CountDroppedUnits(),
		3);

	FRpgInventoryManualDropRequest CollidingRequest = ConfirmedRequest;
	CollidingRequest.StackCount = 1;
	const int32 CollisionFeedbackIndex = FeedbackMessages.Num();
	AddExpectedError(
		TEXT("Rejected manual-drop RequestId collision"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	UiActions->RequestDropInventoryItemById(
		Inventory,
		CollidingRequest);
	TestEqual(
		TEXT("A different payload under the confirmed request id emits one rejection"),
		FeedbackMessages.Num(),
		CollisionFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(CollisionFeedbackIndex))
	{
		TestEqual(
			TEXT("A request-id collision is rejected instead of replayed"),
			FeedbackMessages[CollisionFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::InvalidRequest);
	}
	TestEqual(
		TEXT("A request-id collision cannot remove another source quantity"),
		Inventory->GetItemStackCount(Item),
		6);
	TestEqual(
		TEXT("A request-id collision cannot spawn another drop actor"),
		CountDroppedActors(),
		1);
	TestEqual(
		TEXT("A request-id collision cannot add another dropped quantity"),
		CountDroppedUnits(),
		3);

	URpgInventoryItemInstance* ProviderBag =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 1));
	if (!TestNotNull(
			TEXT("A direct-drop provider bag exists"),
			ProviderBag))
	{
		MessageSubsystem.UnregisterListener(ListenerHandle);
		return false;
	}
	const FRpgInventoryContainerHandle ProviderContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			ProviderBag->GetItemId(),
			BagContainerId,
			1);
	URpgInventoryItemInstance* ProtectedChild =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestNoDropItemDefinition::StaticClass(),
			1,
			MakePlacement(ProviderContents, 0, 0));
	FRpgInventoryEntryView ProviderEntry;
	if (!TestNotNull(
			TEXT("The provider's disabled child exists"),
			ProtectedChild) ||
		!TestTrue(
			TEXT("The provider has a stable replicated entry"),
			GetEntryView(
				Inventory,
				ProviderBag->GetItemId(),
				ProviderEntry)))
	{
		MessageSubsystem.UnregisterListener(ListenerHandle);
		return false;
	}

	const FString BeforeProtectedSubtreeDrop =
		MakeInventorySignature(Inventory);
	FRpgInventoryManualDropRequest ProtectedSubtreeRequest;
	ProtectedSubtreeRequest.RequestId = FGuid::NewGuid();
	ProtectedSubtreeRequest.EntryId = ProviderEntry.EntryId;
	ProtectedSubtreeRequest.ItemId = ProviderEntry.ItemId;
	ProtectedSubtreeRequest.ExpectedSourcePlacement =
		ProviderEntry.Placement;
	ProtectedSubtreeRequest.StackCount = 1;
	ProtectedSubtreeRequest.bConfirmed = true;
	const int32 ProtectedFeedbackIndex = FeedbackMessages.Num();
	UiActions->RequestDropInventoryItemById(
		Inventory,
		ProtectedSubtreeRequest);
	TestEqual(
		TEXT("A protected-descendant request emits one rejection"),
		FeedbackMessages.Num(),
		ProtectedFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(ProtectedFeedbackIndex))
	{
		TestEqual(
			TEXT("A disabled descendant blocks its provider's physical drop"),
			FeedbackMessages[ProtectedFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::CannotDrop);
	}
	TestEqual(
		TEXT("Protected-descendant rejection preserves the complete source graph"),
		MakeInventorySignature(Inventory),
		BeforeProtectedSubtreeDrop);
	TestEqual(
		TEXT("Protected-descendant rejection creates no additional drop actor"),
		CountDroppedActors(),
		1);
	TestEqual(
		TEXT("Protected-descendant rejection adds no world-drop units"),
		CountDroppedUnits(),
		3);

	MessageSubsystem.UnregisterListener(ListenerHandle);
	return true;
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
	URpgPlayerInventoryLayoutComponent* Layout = Controller->GetPlayerInventoryLayoutComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout = Controller->GetEquipmentLoadoutComponent();
	URpgInventoryUiActionComponent* UiActions = Controller->GetInventoryUiActionComponent();
	if (!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The player inventory layout exists"), Layout) ||
		!TestNotNull(TEXT("The equipment reconciliation component exists"), EquipmentLoadout) ||
		!TestNotNull(TEXT("The authoritative inventory action component exists"), UiActions))
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
	const FRpgInventoryDropTarget PouchTarget =
		URpgInventoryDragDropCoordinator::MakeEquipmentTarget(ERpgEquipmentSlot::Pouch);

	FRpgInventorySlotAddress BackpackAddress;
	FRpgInventorySlotAddress PouchAddress;
	if (!TestTrue(
			TEXT("The layout resolves the physical Backpack address"),
			URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(
				ERpgEquipmentSlot::Backpack,
				BackpackAddress)) ||
		!TestTrue(
			TEXT("The layout resolves the physical Pouch address"),
			URpgPlayerInventoryLayoutComponent::TryMakeGearSlotAddress(
				ERpgEquipmentSlot::Pouch,
				PouchAddress)))
	{
		return false;
	}

	TestTrue(
		TEXT("The shared policy permits the authored Backpack provider slot"),
		FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
			Backpack,
			ERpgEquipmentSlot::Backpack));
	TestFalse(
		TEXT("The shared policy rejects the same provider from its unauthored Pouch slot"),
		FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
			Backpack,
			ERpgEquipmentSlot::Pouch));
	TestTrue(
		TEXT("The layout consumes the shared policy for Gear.Backpack"),
		Layout->CanItemUseSlotAddress(Backpack, BackpackAddress));
	TestFalse(
		TEXT("The layout consumes the shared policy for Gear.Pouch"),
		Layout->CanItemUseSlotAddress(Backpack, PouchAddress));
	TestTrue(
		TEXT("The loadout reconciliation accepts the same authored Backpack placement"),
		EquipmentLoadout->CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::Backpack, Backpack));
	TestFalse(
		TEXT("The loadout reconciliation rejects the same unauthored Pouch placement"),
		EquipmentLoadout->CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::Pouch, Backpack));

	const FRpgInventoryContainerHandle PouchSlot =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::GearPouchGroupId);
	const FRpgInventoryMutationRequest RejectedPouchRequest = MakePlacementRequest(
		ERpgInventoryMutationOperation::Equip,
		Backpack,
		Pockets,
		PouchSlot,
		0,
		0);
	const FRpgInventoryMutationResult RejectedPouchPlan =
		Inventory->PlanInventoryMutation(RejectedPouchRequest);
	TestFalse(
		TEXT("The authoritative planner rejects the same unauthored Pouch placement"),
		RejectedPouchPlan.IsSuccess());
	TestEqual(
		TEXT("The planner reports the provider policy failure instead of inventory full"),
		RejectedPouchPlan.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	const FRpgInventoryMutationResult RejectedPouchExecute =
		Inventory->ExecuteInventoryMutation(RejectedPouchRequest);
	TestFalse(
		TEXT("The authoritative commit rejects the same unauthored Pouch placement"),
		RejectedPouchExecute.IsSuccess());
	TestEqual(
		TEXT("The commit preserves the provider policy failure code"),
		RejectedPouchExecute.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	FRpgInventoryEntryView RejectedPouchEntry;
	TestTrue(
		TEXT("The rejected provider remains addressable"),
		GetEntryView(Inventory, Backpack->GetItemId(), RejectedPouchEntry));
	TestEqual(
		TEXT("The rejected provider remains physically in Pockets"),
		RejectedPouchEntry.Placement.GetContainerHandle(),
		Pockets);

	URpgInventoryInteractionSession* Session = Coordinator->GetInteractionSession();
	TestFalse(TEXT("The session starts without a held payload"), Session->HasPayload());
	TestFalse(
		TEXT("The UI preview rejects the same unauthored Pouch placement"),
		Coordinator->PreviewPayloadDrop(Payload, PouchTarget));
	TestEqual(
		TEXT("The rejected Pouch target presents as blocked"),
		Coordinator->ResolveInteractionPreview(Payload, PouchTarget),
		ERpgInventoryInteractionPreviewState::Blocked);
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

	UiActions->RequestEquipSlotContainerItem(ERpgEquipmentSlot::Backpack, Backpack);
	FRpgInventoryEntryView ReEquippedBackpackEntry;
	TestTrue(
		TEXT("The slot-container action physically equips the backpack again"),
		GetEntryView(Inventory, Backpack->GetItemId(), ReEquippedBackpackEntry));
	TestEqual(
		TEXT("The re-equipped provider occupies Gear.Backpack"),
		ReEquippedBackpackEntry.Placement.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::GearBackpackGroupId));
	TestEqual(
		TEXT("Physical equip reconciles the Backpack loadout mirror before unequip"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::Backpack),
		Backpack);

	const FRpgInventoryContainerHandle BackpackContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			Backpack->GetItemId(),
			BagContainerId,
			1);
	URpgInventoryItemInstance* PackedItem = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(BackpackContents, 0, 0));
	TestNotNull(TEXT("The equipped provider owns one packed child item"), PackedItem);

	UiActions->RequestUnequipSlotContainerItem(
		ERpgEquipmentSlot::Backpack,
		FRpgInventoryItemId::NewId());
	FRpgInventoryEntryView StaleUnequipBackpackEntry;
	TestTrue(
		TEXT("The provider remains addressable after a stale unequip request"),
		GetEntryView(Inventory, Backpack->GetItemId(), StaleUnequipBackpackEntry));
	TestEqual(
		TEXT("A stale expected item id cannot remove the current provider"),
		StaleUnequipBackpackEntry.Placement.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::GearBackpackGroupId));

	UiActions->RequestUnequipSlotContainerItem(
		ERpgEquipmentSlot::Backpack,
		Backpack->GetItemId());
	FRpgInventoryEntryView ActionUnequippedBackpackEntry;
	TestTrue(
		TEXT("The unequipped provider remains addressable"),
		GetEntryView(Inventory, Backpack->GetItemId(), ActionUnequippedBackpackEntry));
	TestEqual(
		TEXT("Slot-container unequip moves the physical provider into content"),
		ActionUnequippedBackpackEntry.Placement.GetContainerHandle(),
		Pockets);
	TestNull(
		TEXT("Slot-container unequip reconciles the Backpack loadout mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::Backpack));
	FRpgInventoryEntryView PackedItemEntry;
	TestTrue(
		TEXT("The provider's packed child remains addressable after unequip"),
		PackedItem && GetEntryView(Inventory, PackedItem->GetItemId(), PackedItemEntry));
	TestEqual(
		TEXT("Unequip preserves the item-owned child placement handle"),
		PackedItemEntry.Placement.GetContainerHandle(),
		BackpackContents);
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
	URpgPlayerInventoryLayoutComponent* Layout = Controller->GetPlayerInventoryLayoutComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout = Controller->GetEquipmentLoadoutComponent();
	if (!TestTrue(TEXT("The fixture executes on server authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The controller actionbar exists"), ActionBar) ||
		!TestNotNull(TEXT("The server inventory action component exists"), UiActions) ||
		!TestNotNull(TEXT("The player inventory layout exists"), Layout) ||
		!TestNotNull(TEXT("The equipment reconciliation component exists"), EquipmentLoadout))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle WeaponSlot1 =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	const FRpgInventoryContainerHandle ShieldSlot =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId);
	URpgInventoryItemInstance* Weapon = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* MainHandOnlyShield = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestMainHandShieldItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 1, 0));
	if (!TestNotNull(TEXT("A concrete weapon starts in Pockets"), Weapon) ||
		!TestNotNull(TEXT("A category-compatible but role-incompatible shield starts in Pockets"), MainHandOnlyShield))
	{
		return false;
	}

	FRpgInventorySlotAddress WeaponSlot1Address;
	WeaponSlot1Address.SetContainerHandle(WeaponSlot1);
	WeaponSlot1Address.X = 0;
	WeaponSlot1Address.Y = 0;
	FRpgInventorySlotAddress ShieldSlotAddress;
	ShieldSlotAddress.SetContainerHandle(ShieldSlot);
	ShieldSlotAddress.X = 0;
	ShieldSlotAddress.Y = 0;
	TestTrue(
		TEXT("The shared policy permits the authored MainHand role"),
		FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
			Weapon,
			ERpgEquipmentSlot::MainHand));
	TestFalse(
		TEXT("The shared policy rejects a MainHand-only item from the OffHand role"),
		FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
			MainHandOnlyShield,
			ERpgEquipmentSlot::OffHand));
	const TArray<FRpgInventorySlotGroupView> SlotGroups = Layout->GetSlotGroups();
	const bool bShieldGroupAllowsMismatchItem = SlotGroups.ContainsByPredicate(
		[&ShieldSlot, MainHandOnlyShield](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerHandle == ShieldSlot && Group.Rule.AllowsItem(MainHandOnlyShield);
		});
	TestTrue(
		TEXT("The mismatch fixture passes the Shield category filter before role validation"),
		bShieldGroupAllowsMismatchItem);
	TestTrue(
		TEXT("The Carry layout accepts the weapon in a MainHand role"),
		Layout->CanItemUseSlotAddress(Weapon, WeaponSlot1Address));
	TestFalse(
		TEXT("The Carry layout rejects the category-compatible item from an unauthored OffHand role"),
		Layout->CanItemUseSlotAddress(MainHandOnlyShield, ShieldSlotAddress));
	TestTrue(
		TEXT("The loadout accepts the same authored MainHand role"),
		EquipmentLoadout->CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::MainHand, Weapon));
	TestFalse(
		TEXT("The loadout rejects the category-compatible item from the unauthored OffHand role"),
		EquipmentLoadout->CanAssignItemToEquipmentSlot(ERpgEquipmentSlot::OffHand, MainHandOnlyShield));
	const FRpgInventoryMutationRequest RejectedOffHandRequest = MakePlacementRequest(
		ERpgInventoryMutationOperation::Equip,
		MainHandOnlyShield,
		Pockets,
		ShieldSlot,
		0,
		0);
	const FRpgInventoryMutationResult RejectedOffHandPlan =
		Inventory->PlanInventoryMutation(RejectedOffHandRequest);
	TestFalse(
		TEXT("The authoritative planner rejects the unauthored OffHand Carry placement"),
		RejectedOffHandPlan.IsSuccess());
	TestEqual(
		TEXT("The Carry planner reports the role policy failure instead of inventory full"),
		RejectedOffHandPlan.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	const FRpgInventoryMutationResult RejectedOffHandExecute =
		Inventory->ExecuteInventoryMutation(RejectedOffHandRequest);
	TestFalse(
		TEXT("The authoritative commit rejects the unauthored OffHand Carry placement"),
		RejectedOffHandExecute.IsSuccess());
	TestEqual(
		TEXT("The Carry commit preserves the role policy failure code"),
		RejectedOffHandExecute.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	FRpgInventoryEntryView RejectedOffHandEntry;
	TestTrue(
		TEXT("The rejected Carry item remains addressable"),
		GetEntryView(Inventory, MainHandOnlyShield->GetItemId(), RejectedOffHandEntry));
	TestEqual(
		TEXT("The rejected Carry item remains physically in Pockets"),
		RejectedOffHandEntry.Placement.GetContainerHandle(),
		Pockets);
	UiActions->RequestEquipInventoryItem(Weapon);

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
	TestEqual(
		TEXT("Default equipment activation references the item only after it reaches a compatible Carry role"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand),
		Weapon);

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

	const FRpgInventoryDropTarget OffHandTarget =
		URpgInventoryDragDropCoordinator::MakeEquipmentTarget(ERpgEquipmentSlot::OffHand);
	TestFalse(
		TEXT("The UI preview rejects the same unauthored OffHand role"),
		Coordinator->PreviewPayloadDrop(Payload, OffHandTarget));
	TestEqual(
		TEXT("The rejected OffHand target presents as blocked"),
		Coordinator->ResolveInteractionPreview(Payload, OffHandTarget),
		ERpgInventoryInteractionPreviewState::Blocked);

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
	FRpgCraftingOutputWithdrawalOnlyTransferTest,
	"SurvivalRpg.Inventory.Transfer.CraftingOutputWithdrawalOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingOutputWithdrawalOnlyTransferTest::RunTest(const FString& Parameters)
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
		TEXT("CraftingOutputTransferController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ControllerSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

	FActorSpawnParameters PawnSpawnParameters;
	PawnSpawnParameters.Name = MakeUniqueObjectName(
		World,
		APawn::StaticClass(),
		TEXT("CraftingOutputTransferPawn"));
	PawnSpawnParameters.ObjectFlags = RF_Transient;
	PawnSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* ControllerPawn =
		World->SpawnActor<APawn>(PawnSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("CraftingOutputTransferPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	PlayerStateSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(PlayerStateSpawnParameters);

	FActorSpawnParameters CraftingStationSpawnParameters;
	CraftingStationSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgCraftingStationActor::StaticClass(),
		TEXT("CraftingOutputTransferStation"));
	CraftingStationSpawnParameters.ObjectFlags = RF_Transient;
	CraftingStationSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgCraftingStationActor* CraftingStationActor =
		World->SpawnActor<ARpgCraftingStationActor>(CraftingStationSpawnParameters);

	FActorSpawnParameters ContainerSpawnParameters;
	ContainerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryContainerActor::StaticClass(),
		TEXT("CraftingOutputTransferRegularContainer"));
	ContainerSpawnParameters.ObjectFlags = RF_Transient;
	ContainerSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryContainerActor* RegularContainer =
		World->SpawnActor<ARpgInventoryContainerActor>(ContainerSpawnParameters);

	if (!TestNotNull(TEXT("The output-policy controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The output-policy pawn fixture exists"), ControllerPawn) ||
		!TestNotNull(TEXT("The output-policy player-state fixture exists"), PlayerState) ||
		!TestNotNull(TEXT("The real crafting-station fixture exists"), CraftingStationActor) ||
		!TestNotNull(TEXT("The regular storage fixture exists"), RegularContainer))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	Controller->Possess(ControllerPawn);
	URpgInventoryManagerComponent* PlayerInventory = PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions = Controller->GetInventoryUiActionComponent();
	URpgCraftingStationComponent* CraftingStation = CraftingStationActor->GetCraftingStationComponent();
	URpgInventoryManagerComponent* OutputInventory = CraftingStationActor->GetOutputInventoryComponent();
	URpgInventoryManagerComponent* RegularInventory = RegularContainer->GetInventoryManager();
	if (!TestTrue(TEXT("The output-policy fixture executes on server authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The real player inventory exists"), PlayerInventory) ||
		!TestNotNull(TEXT("The real UI action component exists"), UiActions) ||
		!TestNotNull(TEXT("The real crafting component exists"), CraftingStation) ||
		!TestNotNull(TEXT("The real crafting output inventory exists"), OutputInventory) ||
		!TestNotNull(TEXT("The regular container inventory exists"), RegularInventory))
	{
		return false;
	}

	TestTrue(TEXT("The player may access the nearby crafting output"), UiActions->CanAccessInventory(OutputInventory));
	TestTrue(TEXT("The player may access the nearby regular container"), UiActions->CanAccessInventory(RegularInventory));

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle OutputRoot =
		FRpgInventoryContainerHandle::MakeRoot(OutputInventory->GetDefaultContainerId());
	const FRpgInventoryContainerHandle RegularRoot =
		FRpgInventoryContainerHandle::MakeRoot(RegularInventory->GetDefaultContainerId());

	URpgInventoryItemInstance* RejectedDepositItem = PlayerInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("The player owns one candidate deposit item"), RejectedDepositItem))
	{
		return false;
	}

	FRpgCraftingOutputItem CraftedOutput;
	CraftedOutput.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	CraftedOutput.Count = 1;
	TArray<FRpgCraftingOutputItem> CraftedOutputs;
	CraftedOutputs.Add(CraftedOutput);
	TestTrue(
		TEXT("The authoritative crafting producer can populate its output"),
		CraftingStation->AddCraftingOutputs(CraftedOutputs));
	if (!TestEqual(TEXT("The producer creates one output entry"), OutputInventory->GetUsedEntryCount(), 1))
	{
		return false;
	}

	const TArray<FRpgInventoryEntryView> InitialOutputEntries = OutputInventory->GetAllEntries();
	if (!TestEqual(TEXT("Exactly one concrete crafted output exists"), InitialOutputEntries.Num(), 1) ||
		!InitialOutputEntries.IsValidIndex(0) ||
		!TestNotNull(
			TEXT("The crafted output owns a concrete item instance"),
			InitialOutputEntries[0].Instance.Get()))
	{
		return false;
	}
	const FRpgInventoryItemId CraftedOutputId = InitialOutputEntries[0].ItemId;

	FRpgInventoryQuickTransferRequest RejectedQuickTransfer;
	RejectedQuickTransfer.RequestId = FGuid::NewGuid();
	RejectedQuickTransfer.ItemId = RejectedDepositItem->GetItemId();
	RejectedQuickTransfer.StackCount = 1;
	FRpgInventoryContainerHandle PredictedContainer;
	FRpgInventoryGridPlacement PredictedPlacement;
	TestFalse(
		TEXT("Prediction rejects a player quick-transfer into a crafting output"),
		UiActions->FindQuickTransferDestination(
			PlayerInventory,
			OutputInventory,
			RejectedQuickTransfer,
			PredictedContainer,
			PredictedPlacement));
	TestFalse(
		TEXT("The shared auto-transfer contract rejects crafting-output deposits"),
		UiActions->CanTransferItemStack(
			PlayerInventory,
			OutputInventory,
			RejectedDepositItem,
			1));

	const FString PlayerBeforeRejectedTransfers = MakeInventorySignature(PlayerInventory);
	const FString OutputBeforeRejectedTransfers = MakeInventorySignature(OutputInventory);
	UiActions->RequestQuickTransferItem(PlayerInventory, OutputInventory, RejectedQuickTransfer);
	TestEqual(
		TEXT("The server rejects a forged quick-transfer without mutating the player"),
		MakeInventorySignature(PlayerInventory),
		PlayerBeforeRejectedTransfers);
	TestEqual(
		TEXT("The server rejects a forged quick-transfer without mutating the output"),
		MakeInventorySignature(OutputInventory),
		OutputBeforeRejectedTransfers);

	const FRpgInventoryGridPlacement RejectedExactPlacement = MakePlacement(OutputRoot, 4, 0);
	TestFalse(
		TEXT("The shared exact-placement contract rejects crafting-output deposits"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			OutputInventory,
			RejectedDepositItem,
			1,
			RejectedExactPlacement));
	UiActions->RequestTransferItemStackToPlacement(
		PlayerInventory,
		OutputInventory,
		RejectedDepositItem,
		1,
		RejectedExactPlacement);
	TestEqual(
		TEXT("The exact server RPC preserves the player after a forbidden deposit"),
		MakeInventorySignature(PlayerInventory),
		PlayerBeforeRejectedTransfers);
	TestEqual(
		TEXT("The exact server RPC preserves the crafting output after a forbidden deposit"),
		MakeInventorySignature(OutputInventory),
		OutputBeforeRejectedTransfers);

	FRpgInventoryEntryView CraftedEntryBeforeMove;
	if (!TestTrue(
		TEXT("The crafted output remains addressable before internal reordering"),
		GetEntryView(OutputInventory, CraftedOutputId, CraftedEntryBeforeMove)))
	{
		return false;
	}
	FRpgInventoryMutationRequest InternalMove = MakePlacementRequest(
		ERpgInventoryMutationOperation::Move,
		CraftedEntryBeforeMove.Instance,
		CraftedEntryBeforeMove.Placement.GetContainerHandle(),
		CraftedEntryBeforeMove.Placement.GetContainerHandle(),
		3,
		0);
	UiActions->RequestInventoryMutation(OutputInventory, InternalMove);
	FRpgInventoryEntryView CraftedEntryAfterMove;
	TestTrue(
		TEXT("The output entry remains addressable after an internal move"),
		GetEntryView(OutputInventory, CraftedOutputId, CraftedEntryAfterMove));
	TestEqual(TEXT("The withdrawal-only policy preserves internal output reordering"), CraftedEntryAfterMove.Placement.X, 3);
	TestEqual(TEXT("The internal output move preserves its row"), CraftedEntryAfterMove.Placement.Y, 0);

	FRpgInventoryQuickTransferRequest AllowedWithdrawalPrediction;
	AllowedWithdrawalPrediction.ItemId = CraftedOutputId;
	AllowedWithdrawalPrediction.StackCount = 1;
	TestTrue(
		TEXT("Quick-transfer prediction permits output withdrawal into player content"),
		UiActions->FindQuickTransferDestination(
			OutputInventory,
			PlayerInventory,
			AllowedWithdrawalPrediction,
			PredictedContainer,
			PredictedPlacement));

	const FRpgInventoryGridPlacement PlayerWithdrawalPlacement = MakePlacement(Pockets, 1, 0);
	TestTrue(
		TEXT("The shared exact-placement contract permits output withdrawal"),
		UiActions->CanTransferItemStackToPlacement(
			OutputInventory,
			PlayerInventory,
			CraftedEntryAfterMove.Instance,
			1,
			PlayerWithdrawalPlacement));
	UiActions->RequestTransferItemStackToPlacement(
		OutputInventory,
		PlayerInventory,
		CraftedEntryAfterMove.Instance,
		1,
		PlayerWithdrawalPlacement);
	TestNull(
		TEXT("The withdrawn item leaves the crafting output"),
		OutputInventory->FindItemById(CraftedOutputId));
	TestNotNull(
		TEXT("The withdrawn item arrives in the player inventory with stable identity"),
		PlayerInventory->FindItemById(CraftedOutputId));

	TestTrue(
		TEXT("Crafting production remains able to add a later output after UI-policy checks"),
		CraftingStation->AddCraftingOutputs(CraftedOutputs));
	TestEqual(TEXT("The later production creates one new output entry"), OutputInventory->GetUsedEntryCount(), 1);

	URpgInventoryItemInstance* RegularTransferItem = PlayerInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 2, 0));
	if (!TestNotNull(TEXT("The player owns one normal storage-transfer item"), RegularTransferItem))
	{
		return false;
	}
	const FRpgInventoryItemId RegularTransferItemId = RegularTransferItem->GetItemId();
	const FRpgInventoryGridPlacement RegularTargetPlacement = MakePlacement(RegularRoot, 3, 0);
	TestTrue(
		TEXT("The shared transfer contract still permits ordinary storage deposits"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			RegularInventory,
			RegularTransferItem,
			1,
			RegularTargetPlacement));
	UiActions->RequestTransferItemStackToPlacement(
		PlayerInventory,
		RegularInventory,
		RegularTransferItem,
		1,
		RegularTargetPlacement);
	TestNull(
		TEXT("The ordinary transfer removes the item from the player"),
		PlayerInventory->FindItemById(RegularTransferItemId));
	TestNotNull(
		TEXT("The ordinary transfer still reaches a regular container"),
		RegularInventory->FindItemById(RegularTransferItemId));

	URpgInventoryItemInstance* SwapSource = PlayerInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 0, 1));
	URpgInventoryItemInstance* SwapTarget = RegularInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(RegularRoot, 1, 0));
	if (!TestNotNull(TEXT("The cross-inventory swap source exists"), SwapSource) ||
		!TestNotNull(TEXT("The occupied unlike-item target exists"), SwapTarget))
	{
		return false;
	}
	TestFalse(
		TEXT("Prediction no longer advertises an unsupported cross-inventory swap"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			RegularInventory,
			SwapSource,
			1,
			MakePlacement(RegularRoot, 1, 0)));

	URpgInventoryManagerComponent* AlternateOutputInventory =
		NewObject<URpgInventoryManagerComponent>(
			CraftingStationActor,
			MakeUniqueObjectName(
				CraftingStationActor,
				URpgInventoryManagerComponent::StaticClass(),
				TEXT("AlternateOutputInventory")),
			RF_Transient);
	URpgCraftingStationComponent* SecondaryCraftingStation =
		NewObject<URpgCraftingStationComponent>(
			CraftingStationActor,
			MakeUniqueObjectName(
				CraftingStationActor,
				URpgCraftingStationComponent::StaticClass(),
				TEXT("SecondaryCraftingStation")),
			RF_Transient);
	if (!TestNotNull(TEXT("The multiple-component fixture owns an alternate output inventory"), AlternateOutputInventory) ||
		!TestNotNull(TEXT("The multiple-component fixture owns a second crafting component"), SecondaryCraftingStation))
	{
		return false;
	}
	CraftingStationActor->AddInstanceComponent(AlternateOutputInventory);
	AlternateOutputInventory->RegisterComponent();
	CraftingStation->SetOutputInventoryManager(AlternateOutputInventory);
	CraftingStationActor->AddInstanceComponent(SecondaryCraftingStation);
	SecondaryCraftingStation->RegisterComponent();
	SecondaryCraftingStation->SetOutputInventoryManager(OutputInventory);
	TestTrue(
		TEXT("Output access resolves the matching crafting component even when it is not the owner's first component"),
		UiActions->CanAccessInventory(OutputInventory));

	URpgInventoryItemInstance* MultiComponentDepositItem = PlayerInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 2, 0));
	if (!TestNotNull(TEXT("The player owns a candidate for the multiple-component output"), MultiComponentDepositItem))
	{
		return false;
	}
	TestFalse(
		TEXT("Output recognition checks every crafting component instead of only the owner's first component"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			OutputInventory,
			MultiComponentDepositItem,
			1,
			MakePlacement(OutputRoot, 5, 0)));

	SecondaryCraftingStation->SetOutputInventoryManager(RegularInventory);
	TestTrue(
		TEXT("The externally assigned output remains accessible through its crafting station"),
		UiActions->CanAccessInventory(RegularInventory));
	TestFalse(
		TEXT("An externally assigned crafting output remains withdrawal-only even when its owner is ordinary storage"),
		UiActions->CanTransferItemStack(
			PlayerInventory,
			RegularInventory,
			MultiComponentDepositItem,
			1));
	TestFalse(
		TEXT("Exact-placement prediction also recognizes an externally assigned crafting output"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			RegularInventory,
			MultiComponentDepositItem,
			1,
			MakePlacement(RegularRoot, 5, 0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgExactPlacementStackTransferPolicyTest,
	"SurvivalRpg.Inventory.Transfer.ExactPlacement.StackCapacityAndPartialAssignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgExactPlacementStackTransferPolicyTest::RunTest(const FString& Parameters)
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
		TEXT("ExactPlacementStackController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ControllerSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

	FActorSpawnParameters PawnSpawnParameters;
	PawnSpawnParameters.Name = MakeUniqueObjectName(
		World,
		APawn::StaticClass(),
		TEXT("ExactPlacementStackPawn"));
	PawnSpawnParameters.ObjectFlags = RF_Transient;
	PawnSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* ControllerPawn = World->SpawnActor<APawn>(PawnSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("ExactPlacementStackPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	PlayerStateSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(PlayerStateSpawnParameters);

	FActorSpawnParameters ContainerSpawnParameters;
	ContainerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryContainerActor::StaticClass(),
		TEXT("ExactPlacementStackContainer"));
	ContainerSpawnParameters.ObjectFlags = RF_Transient;
	ContainerSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryContainerActor* Container =
		World->SpawnActor<ARpgInventoryContainerActor>(ContainerSpawnParameters);

	if (!TestNotNull(TEXT("The exact-placement controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The exact-placement pawn fixture exists"), ControllerPawn) ||
		!TestNotNull(TEXT("The exact-placement player-state fixture exists"), PlayerState) ||
		!TestNotNull(TEXT("The exact-placement target container exists"), Container))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	Controller->Possess(ControllerPawn);
	URpgInventoryManagerComponent* PlayerInventory = PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions = Controller->GetInventoryUiActionComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout = Controller->GetEquipmentLoadoutComponent();
	URpgInventoryManagerComponent* TargetInventory = Container->GetInventoryManager();
	if (!TestTrue(TEXT("The exact-placement fixture executes on server authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The exact-placement player inventory exists"), PlayerInventory) ||
		!TestNotNull(TEXT("The exact-placement action component exists"), UiActions) ||
		!TestNotNull(TEXT("The exact-placement equipment loadout exists"), EquipmentLoadout) ||
		!TestNotNull(TEXT("The exact-placement target inventory exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle TargetRoot =
		FRpgInventoryContainerHandle::MakeRoot(TargetInventory->GetDefaultContainerId());
	URpgInventoryItemInstance* SourceStack = PlayerInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass(),
		3,
		MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* NearlyFullTargetStack = TargetInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass(),
		9,
		MakePlacement(TargetRoot, 1, 0));
	if (!TestNotNull(TEXT("The player owns a three-item source stack"), SourceStack) ||
		!TestNotNull(TEXT("The target owns a compatible stack with one free unit"), NearlyFullTargetStack))
	{
		return false;
	}

	const FRpgInventoryGridPlacement TargetStackPlacement = MakePlacement(TargetRoot, 1, 0);
	TestEqual(
		TEXT("The compatible target stack exposes exactly one free unit"),
		TargetInventory->GetFreeStackCapacity(NearlyFullTargetStack),
		1);
	TestFalse(
		TEXT("Exact-placement preview rejects a request larger than the compatible stack's free capacity"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			TargetInventory,
			SourceStack,
			2,
			TargetStackPlacement));
	TestTrue(
		TEXT("Exact-placement preview accepts a request that exactly fits the compatible stack"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			TargetInventory,
			SourceStack,
			1,
			TargetStackPlacement));
	NearlyFullTargetStack->AddStatTagStack(RpgGameplayTags::Rpg_Inventory_Action_Transfer, 1);
	TestFalse(
		TEXT("Exact-placement preview rejects a same-definition stack with incompatible runtime state"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			TargetInventory,
			SourceStack,
			1,
			TargetStackPlacement));

	const FRpgInventoryItemId SourceItemId = SourceStack->GetItemId();
	const FRpgInventoryItemId ExistingTargetItemId = NearlyFullTargetStack->GetItemId();
	UiActions->RequestAssignItemToEquipmentSlot(ERpgEquipmentSlot::MainHand, SourceStack);
	if (!TestEqual(
		TEXT("The source stack is active in MainHand before the partial transfer"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand),
		SourceStack))
	{
		return false;
	}

	FRpgInventoryEntryView SourceEntryBeforeTransfer;
	FRpgInventoryEntryView ExistingTargetEntryBeforeTransfer;
	if (!TestTrue(
			TEXT("The assigned source entry is addressable before the partial transfer"),
			GetEntryView(PlayerInventory, SourceItemId, SourceEntryBeforeTransfer)) ||
		!TestTrue(
			TEXT("The unrelated target entry is addressable before the partial transfer"),
			GetEntryView(TargetInventory, ExistingTargetItemId, ExistingTargetEntryBeforeTransfer)))
	{
		return false;
	}

	const FRpgInventoryGridPlacement EmptyTargetPlacement = MakePlacement(TargetRoot, 0, 0);
	TestTrue(
		TEXT("Exact-placement preview accepts one unit into an empty target cell"),
		UiActions->CanTransferItemStackToPlacement(
			PlayerInventory,
			TargetInventory,
			SourceStack,
			1,
			EmptyTargetPlacement));
	UiActions->RequestTransferItemStackToPlacement(
		PlayerInventory,
		TargetInventory,
		SourceStack,
		1,
		EmptyTargetPlacement);

	URpgInventoryItemInstance* RemainingSourceStack = PlayerInventory->FindItemById(SourceItemId);
	if (!TestNotNull(TEXT("A partial exact transfer preserves the source item identity"), RemainingSourceStack))
	{
		return false;
	}
	TestEqual(
		TEXT("A partial exact transfer preserves the source runtime instance"),
		RemainingSourceStack,
		SourceStack);
	TestEqual(
		TEXT("The partial exact transfer removes only the requested unit"),
		PlayerInventory->GetItemStackCount(RemainingSourceStack),
		2);
	TestEqual(
		TEXT("The surviving assigned source stack remains active in MainHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand),
		RemainingSourceStack);

	FRpgInventoryEntryView SourceEntryAfterTransfer;
	FRpgInventoryEntryView ExistingTargetEntryAfterTransfer;
	if (!TestTrue(
			TEXT("The surviving source entry remains addressable after the partial transfer"),
			GetEntryView(PlayerInventory, SourceItemId, SourceEntryAfterTransfer)) ||
		!TestTrue(
			TEXT("The unrelated target entry remains addressable after the partial transfer"),
			GetEntryView(TargetInventory, ExistingTargetItemId, ExistingTargetEntryAfterTransfer)))
	{
		return false;
	}
	TestEqual(
		TEXT("A partial exact transfer preserves the source entry identity"),
		SourceEntryAfterTransfer.EntryId,
		SourceEntryBeforeTransfer.EntryId);
	TestEqual(
		TEXT("A cross-inventory commit preserves unrelated target runtime instances"),
		TargetInventory->FindItemById(ExistingTargetItemId),
		NearlyFullTargetStack);
	TestEqual(
		TEXT("A cross-inventory commit preserves unrelated target entry identities"),
		ExistingTargetEntryAfterTransfer.EntryId,
		ExistingTargetEntryBeforeTransfer.EntryId);

	URpgInventoryItemInstance* TransferredUnit =
		TargetInventory->GetItemAtContainerCell(TargetRoot, EmptyTargetPlacement.X, EmptyTargetPlacement.Y);
	if (!TestNotNull(TEXT("The requested unit reaches the exact empty target cell"), TransferredUnit))
	{
		return false;
	}
	TestEqual(
		TEXT("The exact target receives one unit"),
		TargetInventory->GetItemStackCount(TransferredUnit),
		1);

	const int32 TargetCountBeforeWholeTransfer =
		TargetInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass());
	UiActions->RequestTransferItemStack(
		PlayerInventory,
		TargetInventory,
		RemainingSourceStack,
		2);

	TestNull(
		TEXT("A complete quick transfer removes the surviving source stack from the player graph"),
		PlayerInventory->FindItemById(SourceItemId));
	TestNull(
		TEXT("A complete quick transfer clears the active MainHand mirror before the physical commit"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand));
	TestEqual(
		TEXT("The complete quick transfer adds the full remaining quantity to the target"),
		TargetInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass()),
		TargetCountBeforeWholeTransfer + 2);
	TestEqual(
		TEXT("The complete quick transfer deterministically merges into the compatible target stack"),
		TargetInventory->GetItemStackCount(TransferredUnit),
		3);
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
	FRpgInventoryGenericMutationRpcSafetyTest,
	"SurvivalRpg.Inventory.Transaction.GenericUiRpcRejectsDedicatedOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryGenericMutationRpcSafetyTest::RunTest(const FString& Parameters)
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
		TEXT("GenericMutationSafetyController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("GenericMutationSafetyPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The generic-mutation controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The generic-mutation player-state fixture exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions = Controller->GetInventoryUiActionComponent();
	if (!TestTrue(TEXT("The fixture executes on server authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The inventory action gateway exists"), UiActions))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* Item = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("A concrete item exists before the rejected generic drop"), Item))
	{
		return false;
	}

	FRpgInventoryEntryView Entry;
	if (!TestTrue(TEXT("The test item has a replicated entry identity"), GetEntryView(Inventory, Item->GetItemId(), Entry)))
	{
		return false;
	}

	URpgInventoryInteractionSession* Session = NewObject<URpgInventoryInteractionSession>(Controller);
	Session->Initialize(Controller, Controller);
	FRpgInventoryDragPayload Payload;
	Payload.SourceType = ERpgInventoryDragSourceType::InventoryEntry;
	Payload.SourceInventory = Inventory;
	Payload.ItemInstance = Item;
	Payload.EntryId = Entry.EntryId;
	Payload.StackCount = Entry.StackCount;
	Payload.SourcePlacement = Entry.Placement;
	if (!TestTrue(
		TEXT("The interaction session accepts the item payload"),
		Session->BeginInteraction(Payload, ERpgInventoryInteractionInputMode::Mouse)))
	{
		return false;
	}

	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::InventoryPanel;
	Target.TargetInventory = Inventory;
	Session->MarkRequestPending(Target, RpgGameplayTags::Rpg_Inventory_Action_Drop);

	FRpgInventoryMutationRequest DropRequest;
	DropRequest.Operation = ERpgInventoryMutationOperation::Drop;
	DropRequest.ItemId = Item->GetItemId();
	DropRequest.Source = Pockets;
	DropRequest.Quantity = 1;
	DropRequest.RequestId = Session->GetRequestId();
	UiActions->RequestInventoryMutation(Inventory, DropRequest);

	TestNotNull(
		TEXT("The generic mutation RPC cannot delete an item through Drop"),
		Inventory->FindItemById(Item->GetItemId()));
	TestEqual(TEXT("The rejected generic Drop preserves the stack"), Inventory->GetItemStackCount(Item), 1);
	TestFalse(TEXT("InvalidRequest feedback resolves the pending request"), Session->IsRequestPending());
	TestTrue(TEXT("Rejected feedback retains the payload for another target"), Session->HasPayload());
	TestEqual(
		TEXT("Rejected feedback exposes the rejected interaction state"),
		Session->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryMutationRejectionCacheBoundTest,
	"SurvivalRpg.Inventory.Transaction.RejectedRequestCacheIsBounded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryMutationRejectionCacheBoundTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory = TestWorld.CreateInventory(TEXT("MutationCacheInventory"));
	if (!TestNotNull(TEXT("Mutation-cache inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* Item = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1,
		MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("A concrete item exists for the cache eviction request"), Item))
	{
		return false;
	}

	FRpgInventoryMutationRequest ReusedRequest = MakePlacementRequest(
		ERpgInventoryMutationOperation::Move,
		Item,
		FRpgInventoryContainerHandle::MakeRoot(FName(TEXT("WrongSource"))),
		Root,
		1,
		0);
	const FRpgInventoryMutationResult InitialRejection = Inventory->ExecuteInventoryMutation(ReusedRequest);
	TestEqual(
		TEXT("The first payload is cached as a source mismatch"),
		InitialRejection.Code,
		ERpgInventoryMutationResultCode::SourceMismatch);

	bool bAllFillerRequestsRejected = true;
	for (int32 RequestIndex = 0; RequestIndex < 65; ++RequestIndex)
	{
		FRpgInventoryMutationRequest FillerRequest = MakePlacementRequest(
			ERpgInventoryMutationOperation::Move,
			nullptr,
			Root,
			Root,
			1,
			0);
		bAllFillerRequestsRejected &=
			Inventory->ExecuteInventoryMutation(FillerRequest).Code == ERpgInventoryMutationResultCode::ItemNotFound;
	}
	TestTrue(TEXT("Every unique filler request is rejected deterministically"), bAllFillerRequestsRejected);

	ReusedRequest.Source = Root;
	const FRpgInventoryMutationResult ReusedCommit = Inventory->ExecuteInventoryMutation(ReusedRequest);
	TestEqual(
		TEXT("The oldest rejected request is evicted and its corrected payload can execute"),
		ReusedCommit.Code,
		ERpgInventoryMutationResultCode::Success);

	FRpgInventoryEntryView MovedEntry;
	TestTrue(TEXT("The item remains addressable after the corrected request"), GetEntryView(Inventory, Item->GetItemId(), MovedEntry));
	TestEqual(TEXT("The corrected request reaches its new X coordinate"), MovedEntry.Placement.X, 1);
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

	FRpgInventoryMutationRequest SortRequest;
	SortRequest.Operation = ERpgInventoryMutationOperation::Sort;
	SortRequest.Source = Root;
	SortRequest.Quantity = static_cast<int32>(ERpgInventorySortMode::Name);
	SortRequest.EnsureRequestId();
	const FRpgInventoryMutationResult SortCommit = Inventory->ExecuteInventoryMutation(SortRequest);
	TestEqual(
		TEXT("Transactional sort can repack a completely full grid without consulting stale live occupancy"),
		SortCommit.Code,
		ERpgInventoryMutationResultCode::Success);
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

	SortRequest.RequestId = FGuid::NewGuid();
	const FRpgInventoryMutationResult NoOpSortCommit = Inventory->ExecuteInventoryMutation(SortRequest);
	TestEqual(
		TEXT("A valid already-sorted transaction remains a successful no-op"),
		NoOpSortCommit.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The successful no-op transaction preserves placement state"),
		MakeInventorySignature(Inventory),
		StableSignature);
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
	FRpgInventorySplitEquipmentPolicyTest,
	"SurvivalRpg.Inventory.Transaction.SplitCannotBypassEquipmentPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventorySplitEquipmentPolicyTest::RunTest(const FString& Parameters)
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
		TEXT("SplitEquipmentPolicyController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("SplitEquipmentPolicyPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The split-policy controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The split-policy player-state fixture exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	if (!TestNotNull(TEXT("The split-policy player inventory exists"), Inventory) ||
		!TestNotNull(
			TEXT("The split-policy layout exists"),
			Controller->GetPlayerInventoryLayoutComponent()))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle GearHead =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::GearHeadGroupId);
	const FRpgInventoryContainerHandle WeaponSlot1 =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	URpgInventoryItemInstance* SourceStack = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		6,
		MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("A six-unit consumable stack starts in Pockets"), SourceStack))
	{
		return false;
	}

	const FString BeforeRejectedSplits = MakeInventorySignature(Inventory);
	FRpgInventoryMutationRequest GearSplit = MakePlacementRequest(
		ERpgInventoryMutationOperation::Split,
		SourceStack,
		Pockets,
		GearHead,
		0,
		0);
	GearSplit.Quantity = 2;
	TestEqual(
		TEXT("A forged split into Gear.Head is rejected by the shared placement policy"),
		Inventory->PlanInventoryMutation(GearSplit).Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	TestEqual(
		TEXT("Executing the forged Gear.Head split remains rejected"),
		Inventory->ExecuteInventoryMutation(GearSplit).Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);

	FRpgInventoryMutationRequest CarrySplit = MakePlacementRequest(
		ERpgInventoryMutationOperation::Split,
		SourceStack,
		Pockets,
		WeaponSlot1,
		0,
		0);
	CarrySplit.Quantity = 2;
	TestEqual(
		TEXT("A forged split into WeaponSlot1 is rejected by the shared placement policy"),
		Inventory->PlanInventoryMutation(CarrySplit).Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	TestEqual(
		TEXT("Executing the forged WeaponSlot1 split remains rejected"),
		Inventory->ExecuteInventoryMutation(CarrySplit).Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	TestEqual(
		TEXT("Rejected equipment splits leave the complete inventory graph unchanged"),
		MakeInventorySignature(Inventory),
		BeforeRejectedSplits);
	TestEqual(TEXT("Rejected equipment splits preserve the source quantity"), Inventory->GetItemStackCount(SourceStack), 6);
	TestEqual(TEXT("Rejected equipment splits create no new entries"), Inventory->GetUsedEntryCount(), 1);
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
	FRpgInventoryRawAddOwnershipGuardTest,
	"SurvivalRpg.Inventory.Transaction.RawAddOwnershipAndIdentityGuards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryRawAddOwnershipGuardTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("RawAddTarget"));
	URpgInventoryManagerComponent* ForeignInventory =
		TestWorld.CreateInventory(TEXT("RawAddForeign"));
	if (!TestNotNull(TEXT("Target inventory exists"), TargetInventory) ||
		!TestNotNull(TEXT("Foreign inventory exists"), ForeignInventory))
	{
		return false;
	}

	URpgInventoryItemInstance* ExistingItem = TargetInventory->GrantItemDefinition(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		1);
	if (!TestNotNull(TEXT("Canonical grant creates an actor-owned fixture"), ExistingItem))
	{
		return false;
	}
	TestEqual(
		TEXT("Granted instances use the inventory actor as exact durable Outer"),
		ExistingItem->GetOuter(),
		static_cast<UObject*>(TargetInventory->GetOwner()));

	const FString InitialSignature = MakeInventorySignature(TargetInventory);
	const int32 InitialEntryCount = TargetInventory->GetUsedEntryCount();
	TestFalse(
		TEXT("Raw-add preflight rejects an instance already contained by this inventory"),
		TargetInventory->CanAddItemInstance(ExistingItem));
	TargetInventory->AddItemInstanceWithStack(ExistingItem, 1);
	TargetInventory->AddItemInstanceWithStackToPlacement(
		ExistingItem,
		1,
		MakePlacement(MakeStorageHandle(), 5, 0));
	TestEqual(
		TEXT("Repeated raw auto/exact adds preserve the complete target graph"),
		MakeInventorySignature(TargetInventory),
		InitialSignature);
	TestEqual(
		TEXT("Repeated raw adds create no second entry"),
		TargetInventory->GetUsedEntryCount(),
		InitialEntryCount);

	URpgInventoryItemInstance* DuplicateIdCandidate =
		TargetInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	if (!TestNotNull(TEXT("A second actor-owned fixture exists"), DuplicateIdCandidate))
	{
		return false;
	}
	TargetInventory->RemoveItemInstance(DuplicateIdCandidate);
	TestTrue(
		TEXT("The detached same-owner fixture can be assigned the occupied persistent id"),
		DuplicateIdCandidate->RestoreItemId(ExistingItem->GetItemId()));
	const FString BeforeDuplicateIdAdd = MakeInventorySignature(TargetInventory);
	TestFalse(
		TEXT("Raw-add preflight rejects a different UObject with an occupied persistent id"),
		TargetInventory->CanAddItemInstance(DuplicateIdCandidate));
	TargetInventory->AddItemInstanceWithStack(DuplicateIdCandidate, 1);
	TargetInventory->AddItemInstanceWithStackToPlacement(
		DuplicateIdCandidate,
		1,
		MakePlacement(MakeStorageHandle(), 4, 0));
	TestEqual(
		TEXT("Duplicate-id raw adds preserve the authoritative graph"),
		MakeInventorySignature(TargetInventory),
		BeforeDuplicateIdAdd);
	TestEqual(
		TEXT("The original item remains the unique resolver result"),
		TargetInventory->FindItemById(ExistingItem->GetItemId()),
		ExistingItem);

	URpgInventoryItemInstance* ForeignDetachedItem =
		ForeignInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	if (!TestNotNull(TEXT("A foreign actor-owned fixture exists"), ForeignDetachedItem))
	{
		return false;
	}
	ForeignDetachedItem->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 3);
	ForeignInventory->RemoveItemInstance(ForeignDetachedItem);
	const FString BeforeForeignOuterAdd = MakeInventorySignature(TargetInventory);
	TestFalse(
		TEXT("Raw-add preflight rejects a detached instance with a foreign actor Outer"),
		TargetInventory->CanAddItemInstance(ForeignDetachedItem));
	TargetInventory->AddItemInstanceWithStack(ForeignDetachedItem, 1);
	TestEqual(
		TEXT("Foreign-Outer raw add leaves the target unchanged"),
		MakeInventorySignature(TargetInventory),
		BeforeForeignOuterAdd);

	URpgInventoryItemInstance* BootstrappedItem =
		TargetInventory->BootstrapItemInstance(ForeignDetachedItem);
	if (!TestNotNull(TEXT("Explicit bootstrap accepts detached foreign setup data"), BootstrappedItem))
	{
		return false;
	}
	TestTrue(
		TEXT("Bootstrap reconstructs a distinct runtime UObject"),
		BootstrappedItem != ForeignDetachedItem);
	TestEqual(
		TEXT("Bootstrap owns the reconstructed instance under the target actor"),
		BootstrappedItem->GetOuter(),
		static_cast<UObject*>(TargetInventory->GetOwner()));
	TestTrue(
		TEXT("Bootstrap starts a fresh persistent identity instead of duplicating the setup object"),
		BootstrappedItem->GetItemId() != ForeignDetachedItem->GetItemId());
	TestEqual(
		TEXT("Bootstrap preserves mutable instance state"),
		BootstrappedItem->GetStatTagStackCount(RpgGameplayTags::Ability_Attack_Basic),
		3);

	URpgInventoryManagerComponent* SiblingInventory =
		NewObject<URpgInventoryManagerComponent>(
			TargetInventory->GetOwner(),
			MakeUniqueObjectName(
				TargetInventory->GetOwner(),
				URpgInventoryManagerComponent::StaticClass(),
				TEXT("SiblingInventory")),
			RF_Transient);
	TargetInventory->GetOwner()->AddInstanceComponent(SiblingInventory);
	SiblingInventory->RegisterComponent();
	if (!TestNotNull(TEXT("A sibling inventory on the same actor exists"), SiblingInventory))
	{
		return false;
	}
	TestFalse(
		TEXT("Raw-add preflight rejects an item managed by a sibling inventory despite its matching Outer"),
		SiblingInventory->CanAddItemInstance(ExistingItem));
	SiblingInventory->AddItemInstanceWithStack(ExistingItem, 1);
	TestEqual(
		TEXT("Sibling inventory cannot acquire a second reference to the same concrete item"),
		SiblingInventory->GetUsedEntryCount(),
		0);

	URpgInventoryItemInstance* SiblingIdentityOwner =
		SiblingInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	URpgInventoryItemInstance* SiblingDuplicateIdCandidate =
		TargetInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	if (!TestNotNull(
			TEXT("A sibling-owned identity fixture exists"),
			SiblingIdentityOwner) ||
		!TestNotNull(
			TEXT("A detached target-owned duplicate-id candidate exists"),
			SiblingDuplicateIdCandidate))
	{
		return false;
	}
	TargetInventory->RemoveItemInstance(SiblingDuplicateIdCandidate);
	TestTrue(
		TEXT("The detached target-owned candidate can copy the sibling's occupied id"),
		SiblingDuplicateIdCandidate->RestoreItemId(
			SiblingIdentityOwner->GetItemId()));
	const FString BeforeSiblingIdCollision =
		MakeInventorySignature(TargetInventory);
	TestFalse(
		TEXT("Raw-add preflight rejects an ItemId occupied by another UObject in a sibling inventory"),
		TargetInventory->CanAddItemInstance(
			SiblingDuplicateIdCandidate));
	TargetInventory->AddItemInstanceWithStack(
		SiblingDuplicateIdCandidate,
		1);
	TestEqual(
		TEXT("A sibling ItemId collision leaves the target graph unchanged"),
		MakeInventorySignature(TargetInventory),
		BeforeSiblingIdCollision);

	URpgInventoryItemInstance* StillManagedForeignItem =
		ForeignInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	TestFalse(
		TEXT("Bootstrap refuses to copy an item that is still authoritative in another inventory"),
		TargetInventory->CanBootstrapItemInstance(StillManagedForeignItem));
	TestNull(
		TEXT("Managed items require cross-inventory transfer instead of bootstrap duplication"),
		TargetInventory->BootstrapItemInstance(StillManagedForeignItem));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPickupBatchRollbackTest,
	"SurvivalRpg.Inventory.Pickup.BatchRollbackOnSpatialFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPickupBatchRollbackTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("PickupRollbackTarget"));
	URpgInventoryManagerComponent* SetupInventory =
		TestWorld.CreateInventory(TEXT("PickupRollbackSetup"));
	if (!TestNotNull(
			TEXT("The pickup rollback target inventory exists"),
			TargetInventory) ||
		!TestNotNull(
			TEXT("The detached pickup setup inventory exists"),
			SetupInventory))
	{
		return false;
	}

	const FRpgInventoryGridSize GridSize =
		TargetInventory->GetDefaultGridSize();
	const int32 GridCellCount = GridSize.Width * GridSize.Height;
	if (!TestTrue(
			TEXT("The rollback fixture has room for at least two cells"),
			GridCellCount >= 2) ||
		!TestNotNull(
			TEXT("All but one target cell can be filled"),
			TargetInventory->GrantItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				GridCellCount - 1)))
	{
		return false;
	}
	TestEqual(
		TEXT("Exactly one target cell remains before the pickup batch"),
		TargetInventory->GetUsedEntryCount(),
		GridCellCount - 1);

	URpgInventoryItemInstance* FirstSetupItem =
		SetupInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	URpgInventoryItemInstance* SecondSetupItem =
		SetupInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	if (!TestNotNull(
			TEXT("The first detached pickup setup item exists"),
			FirstSetupItem) ||
		!TestNotNull(
			TEXT("The second detached pickup setup item exists"),
			SecondSetupItem))
	{
		return false;
	}
	SetupInventory->RemoveItemInstance(FirstSetupItem);
	SetupInventory->RemoveItemInstance(SecondSetupItem);
	TestTrue(
		TEXT("The first pickup item independently fits the last free cell"),
		TargetInventory->CanBootstrapItemInstance(FirstSetupItem));
	TestTrue(
		TEXT("The second pickup item independently fits the same last free cell"),
		TargetInventory->CanBootstrapItemInstance(SecondSetupItem));

	FActorSpawnParameters PickupSpawnParameters;
	PickupSpawnParameters.Name = MakeUniqueObjectName(
		TestWorld.GetTestWorld(),
		ARpgInventoryAutomationTestPickupActor::StaticClass(),
		TEXT("PickupRollbackActor"));
	PickupSpawnParameters.ObjectFlags = RF_Transient;
	PickupSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPickupActor* PickupActor =
		TestWorld.GetTestWorld()->SpawnActor<
			ARpgInventoryAutomationTestPickupActor>(
			PickupSpawnParameters);
	if (!TestNotNull(
			TEXT("The concrete pickup batch fixture exists"),
			PickupActor))
	{
		return false;
	}

	FInventoryPickup PickupInventory;
	PickupInventory.Instances.AddDefaulted_GetRef().Item =
		FirstSetupItem;
	PickupInventory.Instances.AddDefaulted_GetRef().Item =
		SecondSetupItem;
	PickupActor->SetTestPickupInventory(PickupInventory);

	const FString TargetBeforePickup =
		MakeInventorySignature(TargetInventory);
	TScriptInterface<IPickupable> PickupInterface(PickupActor);
	TestFalse(
		TEXT("A two-item pickup cannot commit into one remaining cell"),
		UPickupableStatics::AddPickupToInventory(
			TargetInventory,
			PickupInterface));
	TestEqual(
		TEXT("A failed pickup batch rolls the complete target graph back"),
		MakeInventorySignature(TargetInventory),
		TargetBeforePickup);
	TestEqual(
		TEXT("Rollback restores the exact pre-pickup entry count"),
		TargetInventory->GetUsedEntryCount(),
		GridCellCount - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryLowLevelBlueprintDeprecationTest,
	"SurvivalRpg.Inventory.Transaction.LowLevelBlueprintMutationSurfaceDeprecated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryLowLevelBlueprintDeprecationTest::RunTest(const FString& Parameters)
{
	static const FName DeprecatedFunctionNames[] = {
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, CanAddItemInstance),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, CanAddItemInstanceToPlacement),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, AddItemDefinition),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, AddItemDefinitionToPlacement),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, AddItemInstance),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, AddItemInstanceWithStack),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, AddItemInstanceWithStackToPlacement),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, AddStackToExistingItem),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, RemoveItemInstance),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, RemoveItemInstanceStack),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, ApplyInventorySort),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, MoveInventoryEntry),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, MoveInventoryEntryToPlacement),
	};

	for (const FName FunctionName : DeprecatedFunctionNames)
	{
		const UFunction* Function =
			URpgInventoryManagerComponent::StaticClass()->FindFunctionByName(FunctionName);
		if (!TestNotNull(
				*FString::Printf(TEXT("%s remains reflected for Blueprint migration"), *FunctionName.ToString()),
				Function))
		{
			continue;
		}
		TestTrue(
			*FString::Printf(TEXT("%s is marked DeprecatedFunction"), *FunctionName.ToString()),
			Function->HasMetaData(TEXT("DeprecatedFunction")));
		TestFalse(
			*FString::Printf(TEXT("%s explains its replacement"), *FunctionName.ToString()),
			Function->GetMetaData(TEXT("DeprecationMessage")).IsEmpty());
	}

	static const FName CanonicalFunctionNames[] = {
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, GrantItemDefinition),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, BootstrapItemInstance),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, CanBootstrapItemInstance),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, CanConsumeItemById),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, ConsumeItemById),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryManagerComponent, ConsumeItemsByDefinition),
	};
	for (const FName FunctionName : CanonicalFunctionNames)
	{
		const UFunction* Function =
			URpgInventoryManagerComponent::StaticClass()->FindFunctionByName(FunctionName);
		if (TestNotNull(
				*FString::Printf(TEXT("%s is reflected as a canonical intent"), *FunctionName.ToString()),
				Function))
		{
			TestFalse(
				*FString::Printf(TEXT("%s is not deprecated"), *FunctionName.ToString()),
				Function->HasMetaData(TEXT("DeprecatedFunction")));
		}
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryConsumeNestedSubtreeTest,
	"SurvivalRpg.Inventory.Transaction.ConsumeNestedSubtreeIsAtomic",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryConsumeNestedSubtreeTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("NestedConsumeInventory"));
	if (!TestNotNull(TEXT("Nested-consume inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* RootBag =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* IndependentSibling =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	if (!TestNotNull(TEXT("The consumed root bag exists"), RootBag) ||
		!TestNotNull(
			TEXT("An independent root sibling exists"),
			IndependentSibling))
	{
		return false;
	}

	const FRpgInventoryItemId RootBagId = RootBag->GetItemId();
	const FRpgInventoryItemId SiblingId =
		IndependentSibling->GetItemId();
	const FRpgInventoryContainerHandle RootBagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			RootBagId,
			BagContainerId,
			1);
	URpgInventoryItemInstance* NestedBag =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(RootBagContents, 0, 0));
	if (!TestNotNull(TEXT("The nested bag exists"), NestedBag))
	{
		return false;
	}

	const FRpgInventoryItemId NestedBagId = NestedBag->GetItemId();
	const FRpgInventoryContainerHandle NestedBagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			NestedBagId,
			BagContainerId,
			2);
	URpgInventoryItemInstance* NestedLeaf =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4,
			MakePlacement(NestedBagContents, 1, 2));
	if (!TestNotNull(TEXT("The depth-two leaf stack exists"), NestedLeaf))
	{
		return false;
	}
	const FRpgInventoryItemId NestedLeafId =
		NestedLeaf->GetItemId();

	FRpgInventoryMutationRequest ConsumeRequest;
	ConsumeRequest.Operation =
		ERpgInventoryMutationOperation::Consume;
	ConsumeRequest.ItemId = RootBagId;
	ConsumeRequest.Source = Root;
	ConsumeRequest.Quantity = 1;
	ConsumeRequest.RequestId = FGuid::NewGuid();
	const FRpgInventoryMutationResult ConsumePlan =
		Inventory->PlanInventoryMutation(ConsumeRequest);
	TestEqual(
		TEXT("A full root consume plans successfully"),
		ConsumePlan.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The consume plan contains root and both descendant deltas"),
		ConsumePlan.Deltas.Num(),
		3);
	if (ConsumePlan.Deltas.Num() == 3)
	{
		TestEqual(
			TEXT("The deepest descendant is planned before its owner"),
			ConsumePlan.Deltas[0].BeforeContainer.Depth,
			static_cast<uint8>(2));
		TestEqual(
			TEXT("The physical root is planned last"),
			ConsumePlan.Deltas.Last().ItemId,
			RootBagId);
	}

	const FRpgInventoryMutationResult ConsumeResult =
		Inventory->ExecuteInventoryMutation(ConsumeRequest);
	TestEqual(
		TEXT("The complete nested consume commits"),
		ConsumeResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("Applied quantity counts only the requested physical root"),
		ConsumeResult.AppliedQuantity,
		1);
	TestEqual(
		TEXT("Exactly one independent entry remains"),
		Inventory->GetUsedEntryCount(),
		1);
	TestNull(
		TEXT("The root bag identity is removed"),
		Inventory->FindItemById(RootBagId));
	TestNull(
		TEXT("The nested bag identity is removed"),
		Inventory->FindItemById(NestedBagId));
	TestNull(
		TEXT("The nested leaf identity is removed"),
		Inventory->FindItemById(NestedLeafId));
	TestNotNull(
		TEXT("The unrelated root sibling survives"),
		Inventory->FindItemById(SiblingId));

	const FString AfterConsumeSignature =
		MakeInventorySignature(Inventory);
	const FRpgInventoryMutationResult RetriedResult =
		Inventory->ExecuteInventoryMutation(ConsumeRequest);
	TestEqual(
		TEXT("A reliable consume retry returns the cached success"),
		RetriedResult.Code,
		ConsumeResult.Code);
	TestEqual(
		TEXT("A consume retry cannot remove unrelated inventory"),
		MakeInventorySignature(Inventory),
		AfterConsumeSignature);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryLegacyProviderPartialRemovalTest,
	"SurvivalRpg.Inventory.Transaction.LegacyContainerPartialRemovalFailsClosed",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryLegacyProviderPartialRemovalTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("LegacyContainerRemovalInventory"));
	if (!TestNotNull(
			TEXT("Legacy-container inventory exists"),
			Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	const FRpgInventoryItemId BagId =
		FRpgInventoryItemId::NewId();
	FRpgInventorySnapshot LegacySnapshot;
	LegacySnapshot.ContainerId = StorageContainerId;

	FRpgInventorySnapshotEntry& BagEntry =
		LegacySnapshot.Entries.AddDefaulted_GetRef();
	BagEntry.EntryId = FGuid::NewGuid();
	BagEntry.ItemId = BagId;
	BagEntry.ItemDefinition =
		URpgInventoryAutomationTestLegacyStackableBagItemDefinition::
			StaticClass();
	BagEntry.StackCount = 2;
	BagEntry.Placement = MakePlacement(Root, 0, 0);
	Inventory->ImportInventorySnapshot(LegacySnapshot);

	URpgInventoryItemInstance* LegacyBag =
		Inventory->FindItemById(BagId);
	if (!TestNotNull(
			TEXT("The legacy snapshot reconstructed its provider"),
			LegacyBag))
	{
		return false;
	}
	TestEqual(
		TEXT("The fixture contains a legacy two-unit provider stack"),
		Inventory->GetItemStackCount(LegacyBag),
		2);
	const URpgInventoryFragment_ItemTraits* LegacyTraits =
		LegacyBag->FindFragmentByClass<
			URpgInventoryFragment_ItemTraits>();
	TestTrue(
		TEXT("The malformed legacy fixture advertises a raw stack size above one"),
		LegacyTraits && LegacyTraits->GetMaxStackSize() > 1);
	TestEqual(
		TEXT("The authoritative effective stack rule clamps providers to one"),
		URpgInventoryManagerComponent::
			GetEffectiveMaxStackSizeForDefinition(
				LegacyBag->GetItemDef()),
		1);

	const FString BeforeRejectedMutations =
		MakeInventorySignature(Inventory);
	TestFalse(
		TEXT("Exact consume preflight rejects a partial empty provider"),
		Inventory->CanConsumeItemById(BagId, 1));
	FRpgInventoryMutationRequest ConsumeRequest;
	ConsumeRequest.Operation =
		ERpgInventoryMutationOperation::Consume;
	ConsumeRequest.ItemId = BagId;
	ConsumeRequest.Source = Root;
	ConsumeRequest.Quantity = 1;
	ConsumeRequest.RequestId = FGuid::NewGuid();
	const FRpgInventoryMutationResult ConsumePlan =
		Inventory->PlanInventoryMutation(ConsumeRequest);
	TestEqual(
		TEXT("A partial concrete container consume is rejected"),
		ConsumePlan.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestTrue(
		TEXT("A rejected partial consume exposes no deltas"),
		ConsumePlan.Deltas.IsEmpty());

	FRpgInventoryMutationRequest DropRequest = ConsumeRequest;
	DropRequest.Operation = ERpgInventoryMutationOperation::Drop;
	DropRequest.RequestId = FGuid::NewGuid();
	const FRpgInventoryMutationResult DropPlan =
		Inventory->PlanInventoryMutation(DropRequest);
	TestEqual(
		TEXT("A partial concrete container drop is rejected"),
		DropPlan.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestTrue(
		TEXT("A rejected partial drop exposes no deltas"),
		DropPlan.Deltas.IsEmpty());

	FRpgInventoryMutationRequest SplitRequest =
		MakePlacementRequest(
			ERpgInventoryMutationOperation::Split,
			LegacyBag,
			Root,
			Root,
			3,
			0);
	SplitRequest.Quantity = 1;
	const FRpgInventoryMutationResult SplitPlan =
		Inventory->PlanInventoryMutation(SplitRequest);
	TestEqual(
		TEXT("A container provider can never be split"),
		SplitPlan.Code,
		ERpgInventoryMutationResultCode::StackLimitReached);
	TestFalse(
		TEXT("The deprecated stack-removal adapter also rejects the partial provider"),
		Inventory->RemoveItemInstanceStack(LegacyBag, 1));

	UWorld* World = TestWorld.GetTestWorld();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgDroppedInventoryActor::StaticClass(),
		TEXT("LegacyPartialProviderDrop"));
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgDroppedInventoryActor* DropActor =
		World->SpawnActor<ARpgDroppedInventoryActor>(
			ARpgDroppedInventoryActor::StaticClass(),
			FTransform::Identity,
			SpawnParameters);
	if (!TestNotNull(
			TEXT("The physical partial-provider target exists"),
			DropActor))
	{
		return false;
	}
	const FRpgInventoryMutationResult PhysicalDropResult =
		DropActor->TransferItemFromInventory(
			Inventory,
			BagId,
			1,
			FGuid::NewGuid());
	TestEqual(
		TEXT("The physical gateway also rejects a partial empty provider"),
		PhysicalDropResult.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("Rejected physical drop leaves its target empty"),
		DropActor->GetLootInventoryManager()->GetUsedEntryCount(),
		0);
	TestFalse(
		TEXT("Broad definition consume never selects a concrete provider"),
		Inventory->ConsumeItemsByDefinition(
			URpgInventoryAutomationTestLegacyStackableBagItemDefinition::
				StaticClass(),
			1));
	TestEqual(
		TEXT("Every rejected path preserves the complete legacy graph"),
		MakeInventorySignature(Inventory),
		BeforeRejectedMutations);

	Inventory->RemoveItemInstance(LegacyBag);
	TestEqual(
		TEXT("The full legacy remove adapter deletes the complete provider stack"),
		Inventory->GetUsedEntryCount(),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryBatchConsumeAtomicityTest,
	"SurvivalRpg.Inventory.Transaction.DefinitionConsumeIsAtomic",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryBatchConsumeAtomicityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("BatchConsumeInventory"));
	if (!TestNotNull(TEXT("Batch-consume inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* FirstStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			3,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* SecondStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2,
			MakePlacement(Root, 1, 0));
	if (!TestNotNull(TEXT("The first resource stack exists"), FirstStack) ||
		!TestNotNull(TEXT("The second resource stack exists"), SecondStack))
	{
		return false;
	}

	const FString BeforeInsufficientConsume =
		MakeInventorySignature(Inventory);
	TestFalse(
		TEXT("An insufficient multi-stack consume is rejected"),
		Inventory->ConsumeItemsByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			6));
	TestEqual(
		TEXT("Insufficient batch consume mutates no earlier stack"),
		MakeInventorySignature(Inventory),
		BeforeInsufficientConsume);

	TestTrue(
		TEXT("A satisfiable multi-stack consume commits"),
		Inventory->ConsumeItemsByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4));
	TestEqual(
		TEXT("The exact unconsumed quantity remains"),
		Inventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass()),
		1);

	URpgInventoryItemInstance* Bag =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	if (!TestNotNull(TEXT("A concrete provider exists"), Bag))
	{
		return false;
	}
	const FString BeforeProviderConsume =
		MakeInventorySignature(Inventory);
	TestFalse(
		TEXT("Definition consume refuses a provider even when count is sufficient"),
		Inventory->ConsumeItemsByDefinition(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1));
	TestEqual(
		TEXT("Rejected provider consume preserves the graph"),
		MakeInventorySignature(Inventory),
		BeforeProviderConsume);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPhysicalDropSubtreeTest,
	"SurvivalRpg.Inventory.Drop.PhysicalSubtreePreservesStateAndCapacity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPhysicalDropSubtreeTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("PhysicalDropSource"));
	URpgInventoryManagerComponent* CapacityTarget =
		TestWorld.CreateInventory(TEXT("PhysicalDropCapacityTarget"));
	if (!TestNotNull(TEXT("Physical-drop source exists"), SourceInventory) ||
		!TestNotNull(TEXT("Capacity-limited target exists"), CapacityTarget))
	{
		return false;
	}

	CapacityTarget->SetCapacityMode(
		ERpgInventoryCapacityMode::FixedEntries);
	CapacityTarget->SetFixedMaxEntries(1);
	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* SourceBag =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("The source drop bag exists"), SourceBag))
	{
		return false;
	}

	const FRpgInventoryItemId BagId = SourceBag->GetItemId();
	const FRpgInventoryContainerHandle BagContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			BagId,
			BagContainerId,
			1);
	URpgInventoryItemInstance* SourceChild =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			5,
			MakePlacement(BagContents, 2, 1));
	if (!TestNotNull(TEXT("The source drop child exists"), SourceChild))
	{
		return false;
	}
	SourceChild->AddStatTagStack(
		RpgGameplayTags::Ability_Attack_Basic,
		3);
	const FRpgInventoryItemId ChildId = SourceChild->GetItemId();
	URpgInventoryItemInstance* RemainingSiblingA =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 1, 0));
	URpgInventoryItemInstance* RemainingSiblingB =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	if (!TestNotNull(
			TEXT("The first independent over-capacity sibling exists"),
			RemainingSiblingA) ||
		!TestNotNull(
			TEXT("The second independent over-capacity sibling exists"),
			RemainingSiblingB))
	{
		return false;
	}

	FRpgInventoryMutationRequest CapacityProbe;
	CapacityProbe.Operation = ERpgInventoryMutationOperation::Drop;
	CapacityProbe.ItemId = BagId;
	CapacityProbe.Source = Root;
	CapacityProbe.Target = Root;
	CapacityProbe.Quantity = 1;
	CapacityProbe.RequestId = FGuid::NewGuid();
	const FString SourceBeforeCapacityRejection =
		MakeInventorySignature(SourceInventory);
	const FRpgInventoryMutationResult CapacityResult =
		SourceInventory->ExecuteCrossInventoryTransfer(
			CapacityTarget,
			CapacityProbe,
			false);
	TestEqual(
		TEXT("Entry capacity counts every member of a moved subtree"),
		CapacityResult.Code,
		ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(
		TEXT("Capacity rejection applies no root quantity"),
		CapacityResult.AppliedQuantity,
		0);
	TestEqual(
		TEXT("Capacity rejection preserves the complete source graph"),
		MakeInventorySignature(SourceInventory),
		SourceBeforeCapacityRejection);
	TestEqual(
		TEXT("Capacity rejection leaves the target empty"),
		CapacityTarget->GetUsedEntryCount(),
		0);
	SourceInventory->SetCapacityMode(
		ERpgInventoryCapacityMode::FixedEntries);
	SourceInventory->SetFixedMaxEntries(1);
	TestTrue(
		TEXT("The source fixture is now over its shrunken capacity"),
		SourceInventory->GetUsedEntryCount() >
			SourceInventory->GetMaxEntries());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgDroppedInventoryActor::StaticClass(),
		TEXT("PhysicalDropActor"));
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgDroppedInventoryActor* DropActor =
		World->SpawnActor<ARpgDroppedInventoryActor>(
			ARpgDroppedInventoryActor::StaticClass(),
			FTransform::Identity,
			SpawnParameters);
	if (!TestNotNull(TEXT("A durable dropped actor exists"), DropActor))
	{
		return false;
	}

	const FRpgInventoryMutationResult DropResult =
		DropActor->TransferItemFromInventory(
			SourceInventory,
			BagId,
			1,
			FGuid::NewGuid());
	TestEqual(
		TEXT("The dedicated physical-drop gateway succeeds"),
		DropResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The physical drop retains semantic Drop operation"),
		DropResult.Operation,
		ERpgInventoryMutationOperation::Drop);
	TestEqual(
		TEXT("The physical drop reports one delta per subtree entry"),
		DropResult.Deltas.Num(),
		2);
	TestEqual(
		TEXT("An over-capacity source can shrink even while it remains over capacity"),
		SourceInventory->GetUsedEntryCount(),
		2);
	TestTrue(
		TEXT("The successful egress leaves the source above its shrunken cap"),
		SourceInventory->GetUsedEntryCount() >
			SourceInventory->GetMaxEntries());

	URpgInventoryManagerComponent* DropInventory =
		DropActor->GetLootInventoryManager();
	if (!TestNotNull(
			TEXT("The drop exposes an authoritative loot inventory"),
			DropInventory))
	{
		return false;
	}
	TestEqual(
		TEXT("The world inventory contains root and child"),
		DropInventory->GetUsedEntryCount(),
		2);
	URpgInventoryItemInstance* DroppedBag =
		DropInventory->FindItemById(BagId);
	URpgInventoryItemInstance* DroppedChild =
		DropInventory->FindItemById(ChildId);
	TestNotNull(
		TEXT("The dropped bag preserves its persistent identity"),
		DroppedBag);
	TestNotNull(
		TEXT("The dropped child preserves its persistent identity"),
		DroppedChild);
	if (DroppedBag && DroppedChild)
	{
		TestTrue(
			TEXT("The durable drop reconstructs the bag under its actor"),
			DroppedBag != SourceBag);
		TestTrue(
			TEXT("The durable drop reconstructs the child under its actor"),
			DroppedChild != SourceChild);
		TestEqual(
			TEXT("The child runtime state survives physical drop"),
			DroppedChild->GetStatTagStackCount(
				RpgGameplayTags::Ability_Attack_Basic),
			3);
		FRpgInventoryEntryView DroppedChildView;
		TestTrue(
			TEXT("The dropped child placement resolves"),
			GetEntryView(DropInventory, ChildId, DroppedChildView));
		TestEqual(
			TEXT("The child remains owned by the dropped bag"),
			DroppedChildView.Placement.GetContainerHandle().ItemOwnerId,
			BagId);
		TestEqual(
			TEXT("The inner X placement survives physical drop"),
			DroppedChildView.Placement.X,
			2);
		TestEqual(
			TEXT("The inner Y placement survives physical drop"),
			DroppedChildView.Placement.Y,
			1);
	}

	URpgInventoryManagerComponent* IdentitySource =
		TestWorld.CreateInventory(TEXT("PhysicalDropIdentitySource"));
	if (!TestNotNull(
			TEXT("The identity-preserving drop source exists"),
			IdentitySource))
	{
		return false;
	}
	URpgInventoryItemInstance* FirstConcreteStack =
		IdentitySource->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			3,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* SecondConcreteStack =
		IdentitySource->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2,
			MakePlacement(Root, 1, 0));
	if (!TestNotNull(
			TEXT("The first concrete death-drop stack exists"),
			FirstConcreteStack) ||
		!TestNotNull(
			TEXT("The second concrete death-drop stack exists"),
			SecondConcreteStack))
	{
		return false;
	}
	const FRpgInventoryItemId FirstConcreteStackId =
		FirstConcreteStack->GetItemId();
	const FRpgInventoryItemId SecondConcreteStackId =
		SecondConcreteStack->GetItemId();
	const FRpgInventoryMutationResult FirstIdentityDrop =
		DropActor->TransferItemFromInventory(
			IdentitySource,
			FirstConcreteStackId,
			3,
			FGuid::NewGuid(),
			true);
	const FRpgInventoryMutationResult SecondIdentityDrop =
		DropActor->TransferItemFromInventory(
			IdentitySource,
			SecondConcreteStackId,
			2,
			FGuid::NewGuid(),
			true);
	TestEqual(
		TEXT("The first identity-preserving stack drop succeeds"),
		FirstIdentityDrop.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The second identity-preserving stack drop succeeds"),
		SecondIdentityDrop.Code,
		ERpgInventoryMutationResultCode::Success);
	TestNotNull(
		TEXT("The first compatible stack keeps its own item id"),
		DropInventory->FindItemById(FirstConcreteStackId));
	TestNotNull(
		TEXT("The second compatible stack is not merged away"),
		DropInventory->FindItemById(SecondConcreteStackId));
	TestEqual(
		TEXT("Both identity-preserving source rows moved completely"),
		IdentitySource->GetUsedEntryCount(),
		0);

	const FRpgInventoryContainerHandle DropRoot =
		FRpgInventoryContainerHandle::MakeRoot(
			DropInventory->GetDefaultContainerId());
	const FRpgInventoryGridSize InitialDropGrid =
		DropInventory->GetDefaultGridSize();
	for (int32 Y = 0; Y < InitialDropGrid.Height; ++Y)
	{
		for (int32 X = 0; X < InitialDropGrid.Width; ++X)
		{
			if (DropInventory->GetItemAtContainerCell(
					DropRoot,
					X,
					Y))
			{
				continue;
			}

			URpgInventoryItemInstance* Filler =
				DropInventory->AddItemDefinitionToPlacement(
					URpgInventoryAutomationTestUnitItemDefinition::
						StaticClass(),
					1,
					MakePlacement(DropRoot, X, Y));
			if (!TestNotNull(
					TEXT("The overflow fixture fills every free corpse cell"),
					Filler))
			{
				return false;
			}
		}
	}

	URpgInventoryManagerComponent* OverflowSource =
		TestWorld.CreateInventory(TEXT("PhysicalDropOverflowSource"));
	URpgInventoryItemInstance* OverflowItem =
		OverflowSource
			? OverflowSource->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass(),
				1,
				MakePlacement(Root, 0, 0))
			: nullptr;
	if (!TestNotNull(
			TEXT("The corpse-overflow source exists"),
			OverflowSource) ||
		!TestNotNull(
			TEXT("The corpse-overflow item exists"),
			OverflowItem))
	{
		return false;
	}
	const FRpgInventoryItemId OverflowItemId =
		OverflowItem->GetItemId();

	const FRpgInventoryMutationResult OverflowDropResult =
		DropActor->TransferItemFromInventory(
			OverflowSource,
			OverflowItemId,
			1,
			FGuid::NewGuid(),
			true);
	TestEqual(
		TEXT("A full corpse grid expands instead of retaining the item on the player"),
		OverflowDropResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestTrue(
		TEXT("The durable corpse root grows only when its authored grid is full"),
		DropInventory->GetDefaultGridSize().Height >
			InitialDropGrid.Height);
	TestNotNull(
		TEXT("The overflow item keeps its persistent identity in the expanded corpse"),
		DropInventory->FindItemById(OverflowItemId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryItemUseContextConsumeCallbackTest,
	"SurvivalRpg.Inventory.UseContext.ConsumeCompletionIsExactlyOnce",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryItemUseContextConsumeCallbackTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("ItemUseContextInventory"));
	if (!TestNotNull(TEXT("The item-use inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* ConsumedStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2,
			MakePlacement(Root, 0, 0));
	if (!TestNotNull(
			TEXT("The fully consumed source stack exists"),
			ConsumedStack))
	{
		return false;
	}
	const FRpgInventoryItemId ConsumedStackId =
		ConsumedStack->GetItemId();

	int32 SuccessfulCompletionCount = 0;
	URpgInventoryItemUseContext* SuccessfulContext =
		NewObject<URpgInventoryItemUseContext>(Inventory);
	SuccessfulContext->Initialize(
		Inventory,
		ConsumedStack,
		1,
		2);
	SuccessfulContext->SetConsumeSucceededCallback(
		FSimpleDelegate::CreateLambda(
			[&SuccessfulCompletionCount]()
			{
				++SuccessfulCompletionCount;
			}));

	TestTrue(
		TEXT("A full-stack item use consumes successfully"),
		SuccessfulContext->TryConsume());
	TestTrue(
		TEXT("The successful context records its committed consume"),
		SuccessfulContext->bConsumed);
	TestNull(
		TEXT("The fully consumed stack is removed from inventory"),
		Inventory->FindItemById(ConsumedStackId));
	TestEqual(
		TEXT("A committed consume runs completion exactly once"),
		SuccessfulCompletionCount,
		1);

	TestTrue(
		TEXT("Retrying an already committed context is idempotent"),
		SuccessfulContext->TryConsume());
	TestEqual(
		TEXT("An idempotent retry does not run completion again"),
		SuccessfulCompletionCount,
		1);

	URpgInventoryItemInstance* RejectedStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 1, 0));
	if (!TestNotNull(
			TEXT("The insufficient source stack exists"),
			RejectedStack))
	{
		return false;
	}
	const FRpgInventoryItemId RejectedStackId =
		RejectedStack->GetItemId();

	int32 RejectedCompletionCount = 0;
	URpgInventoryItemUseContext* RejectedContext =
		NewObject<URpgInventoryItemUseContext>(Inventory);
	RejectedContext->Initialize(
		Inventory,
		RejectedStack,
		1,
		2);
	RejectedContext->SetConsumeSucceededCallback(
		FSimpleDelegate::CreateLambda(
			[&RejectedCompletionCount]()
			{
				++RejectedCompletionCount;
			}));

	TestFalse(
		TEXT("A consume larger than the available stack is rejected"),
		RejectedContext->TryConsume());
	TestFalse(
		TEXT("A rejected context remains unconsumed"),
		RejectedContext->bConsumed);
	TestEqual(
		TEXT("A rejected consume never runs completion"),
		RejectedCompletionCount,
		0);
	TestEqual(
		TEXT("A rejected consume preserves the source stack"),
		Inventory->GetItemStackCount(
			Inventory->FindItemById(RejectedStackId)),
		1);

	TestFalse(
		TEXT("Retrying the still-invalid consume remains rejected"),
		RejectedContext->TryConsume());
	TestEqual(
		TEXT("Repeated rejected consumes still never run completion"),
		RejectedCompletionCount,
		0);

	int32 PreflightCompletionCount = 0;
	int32 PreflightCallCount = 0;
	URpgInventoryItemUseContext* PreflightRejectedContext =
		NewObject<URpgInventoryItemUseContext>(Inventory);
	PreflightRejectedContext->Initialize(
		Inventory,
		RejectedStack,
		1,
		1);
	PreflightRejectedContext->SetConsumePreflightCallback(
		FRpgInventoryUseConsumePreflight::CreateLambda(
			[&PreflightCallCount]()
			{
				++PreflightCallCount;
				return false;
			}));
	PreflightRejectedContext->SetConsumeSucceededCallback(
		FSimpleDelegate::CreateLambda(
			[&PreflightCompletionCount]()
			{
				++PreflightCompletionCount;
			}));
	TestFalse(
		TEXT("A final delayed-use preflight can reject an otherwise valid consume"),
		PreflightRejectedContext->TryConsume());
	TestEqual(
		TEXT("The final preflight runs exactly once per attempted commit"),
		PreflightCallCount,
		1);
	TestEqual(
		TEXT("Preflight rejection cannot run completion"),
		PreflightCompletionCount,
		0);
	TestEqual(
		TEXT("Preflight rejection preserves the valid source quantity"),
		Inventory->GetItemStackCount(RejectedStack),
		1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
