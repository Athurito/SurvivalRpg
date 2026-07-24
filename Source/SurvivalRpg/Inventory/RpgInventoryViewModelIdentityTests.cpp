#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectHash.h"

namespace RpgInventoryViewModelIdentityTests
{
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

		URpgInventoryManagerComponent* CreateInventory(
			const TCHAR* DebugName) const
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
			AActor* OwnerActor = World->SpawnActor<AActor>(SpawnParameters);
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

	bool MoveToEquipmentPlacement(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		const FRpgInventoryGridPlacement& TargetPlacement)
	{
		FRpgInventoryEntryView Entry;
		if (!Inventory || !Item ||
			!FindEntry(Inventory, Item->GetItemId(), Entry))
		{
			return false;
		}

		FRpgInventoryMoveIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = Entry.ItemId;
		Intent.ExpectedEntryId = Entry.EntryId;
		Intent.ExpectedSourcePlacement = Entry.Placement;
		Intent.ExpectedQuantity = Entry.StackCount;
		Intent.TargetPlacement = TargetPlacement;
		return Inventory->MoveEquipmentItem(Intent).IsSuccess();
	}

	FRpgInventoryTransferIntent MakeTransferIntent(
		const FRpgInventoryEntryView& SourceEntry,
		const FRpgInventoryContainerHandle& TargetContainer,
		const FRpgInventoryGridPlacement& TargetPlacement)
	{
		FRpgInventoryTransferIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = SourceEntry.ItemId;
		Intent.ExpectedEntryId = SourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = SourceEntry.Placement;
		Intent.ExpectedSourceQuantity = SourceEntry.StackCount;
		Intent.TargetContainer = TargetContainer;
		Intent.TargetPlacement = TargetPlacement;
		Intent.Quantity = SourceEntry.StackCount;
		return Intent;
	}

	URpgInventoryEntryViewModel* FindEntryViewModel(
		const URpgInventoryPanelViewModel* PanelViewModel,
		const FRpgInventoryItemId& ItemId)
	{
		if (!PanelViewModel)
		{
			return nullptr;
		}

		for (URpgInventoryEntryViewModel* EntryViewModel :
			 PanelViewModel->GetEntries())
		{
			if (EntryViewModel &&
				EntryViewModel->GetItemId() == ItemId)
			{
				return EntryViewModel;
			}
		}
		return nullptr;
	}

	int32 CountDirectObjectsOfClass(
		const UObject* Outer,
		const UClass* ObjectClass)
	{
		TArray<UObject*> DirectObjects;
		if (Outer)
		{
			GetObjectsWithOuter(
				Outer,
				DirectObjects,
				EGetObjectsFlags::None);
		}

		int32 Count = 0;
		for (const UObject* Object : DirectObjects)
		{
			if (Object && ObjectClass &&
				Object->IsA(ObjectClass))
			{
				++Count;
			}
		}
		return Count;
	}

	int32 CountGroupSlots(
		const TArray<URpgInventorySlotGroupViewModel*>& Groups)
	{
		int32 SlotCount = 0;
		for (const URpgInventorySlotGroupViewModel* Group : Groups)
		{
			if (Group)
			{
				SlotCount += Group->GetSlots().Num();
			}
		}
		return SlotCount;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryEntryViewModelStableItemIdentityTest,
	"SurvivalRpg.Inventory.ViewModel.ChildIdentity.ItemIdSurvivesEntryReconstruction",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryEntryViewModelStableItemIdentityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelIdentityTests;

	FScopedInventoryWorld TestWorld;
	if (!TestTrue(
			TEXT("The item-identity test world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("EntryViewModelIdentityInventory"));
	URpgInventoryManagerComponent* TransferInventory =
		TestWorld.CreateInventory(TEXT("EntryViewModelIdentityTransferInventory"));
	if (!TestNotNull(TEXT("The source inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The transfer inventory exists"), TransferInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle StorageHandle =
		FRpgInventoryContainerHandle::MakeRoot(TEXT("Storage"));
	const FRpgInventoryGridPlacement InitialPlacement =
		MakePlacement(StorageHandle, 0, 0);
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			InitialPlacement);
	URpgInventoryItemInstance* Sentinel =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(StorageHandle, 2, 0));
	if (!TestNotNull(TEXT("The fixture item exists"), Item) ||
		!TestNotNull(TEXT("The unaffected sentinel exists"), Sentinel))
	{
		return false;
	}
	const FRpgInventoryItemId ItemId = Item->GetItemId();
	const FRpgInventoryItemId SentinelItemId = Sentinel->GetItemId();

	FRpgInventoryEntryView InitialEntry;
	if (!TestTrue(
			TEXT("The initial entry resolves by persistent item identity"),
			FindEntry(Inventory, ItemId, InitialEntry)))
	{
		return false;
	}

	URpgInventoryPanelViewModel* PanelViewModel =
		NewObject<URpgInventoryPanelViewModel>(Inventory);
	PanelViewModel->BindInventory(Inventory);
	URpgInventoryEntryViewModel* InitialEntryViewModel =
		FindEntryViewModel(PanelViewModel, ItemId);
	URpgInventoryEntryViewModel* InitialSentinelViewModel =
		FindEntryViewModel(PanelViewModel, SentinelItemId);
	if (!TestNotNull(
			TEXT("The panel creates one child VM for the item"),
			InitialEntryViewModel) ||
		!TestNotNull(
			TEXT("The panel creates one child VM for the unaffected sentinel"),
			InitialSentinelViewModel))
	{
		PanelViewModel->UnbindInventory();
		return false;
	}
	TestEqual(
		TEXT("The child VM exposes the persistent item id"),
		InitialEntryViewModel->GetItemId(),
		ItemId);
	TestEqual(
		TEXT("The child VM initially exposes the inventory-local entry id"),
		InitialEntryViewModel->GetEntryId(),
		InitialEntry.EntryId);

	const FRpgInventoryMutationResult OutboundTransfer =
		Inventory->TransferItem(
			TransferInventory,
			MakeTransferIntent(
				InitialEntry,
				StorageHandle,
				MakePlacement(StorageHandle, 0, 0)));
	if (!TestTrue(
			TEXT("The typed outbound transfer succeeds"),
			OutboundTransfer.IsSuccess()))
	{
		PanelViewModel->UnbindInventory();
		return false;
	}

	FRpgInventoryEntryView TransferredEntry;
	if (!TestTrue(
			TEXT("The target resolves the transferred persistent item id"),
			FindEntry(
				TransferInventory,
				ItemId,
				TransferredEntry)))
	{
		PanelViewModel->UnbindInventory();
		return false;
	}
	TestTrue(
		TEXT("The target assigns a fresh inventory-local entry id"),
		TransferredEntry.EntryId != InitialEntry.EntryId);

	const FRpgInventoryGridPlacement ReconstructedPlacement =
		MakePlacement(StorageHandle, 3, 0);
	const FRpgInventoryMutationResult ReturnTransfer =
		TransferInventory->TransferItem(
			Inventory,
			MakeTransferIntent(
				TransferredEntry,
				StorageHandle,
				ReconstructedPlacement));
	if (!TestTrue(
			TEXT("The typed return transfer succeeds"),
			ReturnTransfer.IsSuccess()))
	{
		PanelViewModel->UnbindInventory();
		return false;
	}

	FRpgInventoryEntryView ReconstructedEntry;
	if (!TestTrue(
			TEXT("The reconstructed entry resolves by the same item id"),
			FindEntry(
				Inventory,
				ItemId,
				ReconstructedEntry)))
	{
		PanelViewModel->UnbindInventory();
		return false;
	}
	TestTrue(
		TEXT("Reconstruction assigns a fresh inventory-local entry id"),
		ReconstructedEntry.EntryId != InitialEntry.EntryId);

	PanelViewModel->RefreshEntries();
	URpgInventoryEntryViewModel* ReconciledEntryViewModel =
		FindEntryViewModel(PanelViewModel, ItemId);
	TestEqual(
		TEXT("The panel retains the exact child VM across entry reconstruction"),
		ReconciledEntryViewModel,
		InitialEntryViewModel);
	if (ReconciledEntryViewModel)
	{
		TestEqual(
			TEXT("The retained child VM adopts the fresh entry id"),
			ReconciledEntryViewModel->GetEntryId(),
			ReconstructedEntry.EntryId);
		TestEqual(
			TEXT("The retained child VM adopts the reconstructed item instance"),
			ReconciledEntryViewModel->GetItemInstance(),
			ReconstructedEntry.Instance.Get());
		TestEqual(
			TEXT("The retained child VM adopts the changed placement"),
			ReconciledEntryViewModel->GetPlacement(),
			ReconstructedPlacement);
	}
	TestEqual(
		TEXT("The unrelated item keeps the exact same child VM"),
		FindEntryViewModel(PanelViewModel, SentinelItemId),
		InitialSentinelViewModel);
	const TArray<URpgInventoryEntryViewModel*> ReorderedEntries =
		PanelViewModel->GetEntries();
	if (TestEqual(
			TEXT("Both child VMs remain in the panel after the reorder"),
			ReorderedEntries.Num(),
			2))
	{
		TestEqual(
			TEXT("The unaffected sentinel moves to the first visual row"),
			ReorderedEntries[0],
			InitialSentinelViewModel);
		TestEqual(
			TEXT("The reconstructed item moves to the second visual row without replacing its VM"),
			ReorderedEntries[1],
			InitialEntryViewModel);
	}

	PanelViewModel->UnbindInventory();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventorySlotGroupViewModelStableContainerIdentityTest,
	"SurvivalRpg.Inventory.ViewModel.ChildIdentity.FullContainerHandlePreventsAlias",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventorySlotGroupViewModelStableContainerIdentityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelIdentityTests;

	FScopedInventoryWorld TestWorld;
	if (!TestTrue(
			TEXT("The container-identity test world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	UWorld* World = TestWorld.GetWorld();
	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("ContainerIdentityController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("ContainerIdentityPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The player-state fixture exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		Controller->GetPlayerInventoryLayoutComponent();
	if (!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The inventory layout exists"), InventoryLayout))
	{
		return false;
	}

	const FRpgInventoryContainerHandle PocketsHandle =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* Backpack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(PocketsHandle, 0, 0));
	URpgInventoryItemInstance* Belt =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(PocketsHandle, 1, 0));
	if (!TestNotNull(TEXT("The backpack provider exists"), Backpack) ||
		!TestNotNull(TEXT("The belt provider exists"), Belt))
	{
		return false;
	}

	FRpgInventorySlotAddress BackpackAddress;
	FRpgInventorySlotAddress BeltAddress;
	FRpgInventoryGridPlacement BackpackPlacement;
	FRpgInventoryGridPlacement BeltPlacement;
	if (!TestTrue(
			TEXT("The layout resolves the Backpack gear address"),
			InventoryLayout->TryMakeGearSlotAddress(
				ERpgEquipmentSlot::Backpack,
				BackpackAddress)) ||
		!TestTrue(
			TEXT("The layout resolves the Belt gear address"),
			InventoryLayout->TryMakeGearSlotAddress(
				ERpgEquipmentSlot::Belt,
				BeltAddress)) ||
		!TestTrue(
			TEXT("The Backpack address resolves to a placement"),
			InventoryLayout->ResolveSlotAddress(
				BackpackAddress,
				BackpackPlacement)) ||
		!TestTrue(
			TEXT("The Belt address resolves to a placement"),
			InventoryLayout->ResolveSlotAddress(
				BeltAddress,
				BeltPlacement)))
	{
		return false;
	}

	if (!TestTrue(
			TEXT("The first provider moves into Backpack"),
			MoveToEquipmentPlacement(
				Inventory,
				Backpack,
				BackpackPlacement)) ||
		!TestTrue(
			TEXT("The second provider moves into Belt"),
			MoveToEquipmentPlacement(
				Inventory,
				Belt,
				BeltPlacement)))
	{
		return false;
	}

	const FRpgInventoryContainerHandle BackpackContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			Backpack->GetItemId(),
			TEXT("Main"),
			1);
	const FRpgInventoryContainerHandle BeltContents =
		FRpgInventoryContainerHandle::MakeItemOwned(
			Belt->GetItemId(),
			TEXT("Main"),
			1);
	TestEqual(
		TEXT("Both item-owned containers intentionally share the local id"),
		BackpackContents.ContainerId,
		BeltContents.ContainerId);
	TestTrue(
		TEXT("Their complete graph handles remain distinct"),
		BackpackContents != BeltContents);

	URpgPlayerInventoryViewModel* PlayerViewModel =
		NewObject<URpgPlayerInventoryViewModel>(Controller);
	PlayerViewModel->BindPlayerController(Controller);
	const TArray<URpgInventorySlotGroupViewModel*> InitialCarryGroups =
		PlayerViewModel->GetCarryGroups();
	const TArray<URpgInventorySlotGroupViewModel*> InitialInventoryGroups =
		PlayerViewModel->GetInventoryGroups();
	const int32 ExposedGroupCount =
		InitialCarryGroups.Num() + InitialInventoryGroups.Num();
	const int32 ExposedAddressSlotCount =
		CountGroupSlots(InitialCarryGroups) +
		CountGroupSlots(InitialInventoryGroups);
	TestEqual(
		TEXT("The aggregate owns no discarded Gear group VMs"),
		CountDirectObjectsOfClass(
			PlayerViewModel,
			URpgInventorySlotGroupViewModel::StaticClass()),
		ExposedGroupCount);
	TestEqual(
		TEXT("The aggregate owns no discarded Gear address VMs"),
		CountDirectObjectsOfClass(
			PlayerViewModel,
			URpgInventoryAddressSlotViewModel::StaticClass()),
		ExposedAddressSlotCount);

	URpgInventorySlotGroupViewModel* BackpackGroup =
		PlayerViewModel->GetSlotGroupByHandle(BackpackContents);
	URpgInventorySlotGroupViewModel* BeltGroup =
		PlayerViewModel->GetSlotGroupByHandle(BeltContents);
	if (!TestNotNull(
			TEXT("The Backpack content child VM resolves by full handle"),
			BackpackGroup) ||
		!TestNotNull(
			TEXT("The Belt content child VM resolves by full handle"),
			BeltGroup))
	{
		PlayerViewModel->UnbindPlayerInventory();
		return false;
	}
	TestNotEqual(
		TEXT("Same-name item compartments never alias one child VM"),
		BackpackGroup,
		BeltGroup);
	TestEqual(
		TEXT("The Backpack child VM retains its complete handle"),
		BackpackGroup->GetContainerHandle(),
		BackpackContents);
	TestEqual(
		TEXT("The Belt child VM retains its complete handle"),
		BeltGroup->GetContainerHandle(),
		BeltContents);

	PlayerViewModel->BindPlayerController(Controller);
	TestEqual(
		TEXT("A presenter rebind reuses the exact Backpack child VM"),
		PlayerViewModel->GetSlotGroupByHandle(BackpackContents),
		BackpackGroup);
	TestEqual(
		TEXT("A presenter rebind reuses the exact Belt child VM"),
		PlayerViewModel->GetSlotGroupByHandle(BeltContents),
		BeltGroup);
	TestEqual(
		TEXT("A presenter rebind does not accumulate stale group VMs"),
		CountDirectObjectsOfClass(
			PlayerViewModel,
			URpgInventorySlotGroupViewModel::StaticClass()),
		ExposedGroupCount);
	TestEqual(
		TEXT("A presenter rebind does not accumulate stale address VMs"),
		CountDirectObjectsOfClass(
			PlayerViewModel,
			URpgInventoryAddressSlotViewModel::StaticClass()),
		ExposedAddressSlotCount);

	PlayerViewModel->UnbindPlayerInventory();
	return true;
}

#endif
