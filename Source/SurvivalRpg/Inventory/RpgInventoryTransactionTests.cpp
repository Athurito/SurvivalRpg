#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgPlayerInventoryLayoutComponent.h"
#include "RpgPlayerInventoryLayoutDefinition.h"
#include "RpgInventoryDragDropCoordinator.h"
#include "RpgInventoryDragDropTypes.h"
#include "RpgInventoryEquipmentPlacementPolicy.h"
#include "RpgInventoryInteractionSession.h"
#include "RpgInventoryContainerActor.h"
#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemCapabilities.h"
#include "RpgInventoryItemDefinition.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryItemUseContext.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentAutomationTestTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarSlotViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryEntryViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryFragmentViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryAddressSlotViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModel.h"
#include "SurvivalRpg/Systems/GameplayTagStack.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/Formatters/BinaryArchiveFormatter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/StructuredArchive.h"
#include "UObject/UnrealType.h"

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
		if (Item)
		{
			if (const AActor* ItemOwner = Cast<AActor>(Item->GetOuter()))
			{
				if (const URpgInventoryManagerComponent* Inventory =
					ItemOwner->FindComponentByClass<URpgInventoryManagerComponent>())
				{
					for (const FRpgInventoryEntryView& Entry :
						Inventory->GetAllEntries())
					{
						if (Entry.ItemId == Request.ItemId)
						{
							Request.ExpectedEntryId = Entry.EntryId;
							Request.ExpectedSourcePlacement = Entry.Placement;
							Request.ExpectedSourceQuantity = Entry.StackCount;
							Request.Quantity = Entry.StackCount;
							break;
						}
					}
				}
			}
		}
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

	bool ConsumeWholeItem(
		URpgInventoryManagerComponent* Inventory,
		const URpgInventoryItemInstance* Item)
	{
		if (!Inventory || !Item)
		{
			return false;
		}

		const int32 Quantity = Inventory->GetItemStackCount(
			const_cast<URpgInventoryItemInstance*>(Item));
		const FRpgInventoryMutationResult Result =
			Inventory->ConsumeItemById(Item->GetItemId(), Quantity);
		return Quantity > 0 && Result.IsSuccess() &&
			Result.AppliedQuantity == Quantity;
	}

	FRpgInventoryTransferIntent MakeTransferIntent(
		const URpgInventoryManagerComponent* SourceInventory,
		const FRpgInventoryItemId& ItemId,
		int32 Quantity,
		FGuid RequestId = FGuid::NewGuid())
	{
		FRpgInventoryTransferIntent Intent;
		Intent.ItemId = ItemId;
		Intent.Quantity = Quantity;
		Intent.RequestId = RequestId;
		Intent.EnsureRequestId();
		FRpgInventoryEntryView Entry;
		if (GetEntryView(SourceInventory, ItemId, Entry))
		{
			Intent.ExpectedEntryId = Entry.EntryId;
			Intent.ExpectedSourcePlacement = Entry.Placement;
			Intent.ExpectedSourceQuantity = Entry.StackCount;
		}
		return Intent;
	}

	FRpgInventoryTransferIntent MakeExactTransferIntent(
		const URpgInventoryManagerComponent* SourceInventory,
		const FRpgInventoryItemId& ItemId,
		int32 Quantity,
		const FRpgInventoryGridPlacement& TargetPlacement,
		FGuid RequestId = FGuid::NewGuid())
	{
		FRpgInventoryTransferIntent Intent = MakeTransferIntent(
			SourceInventory,
			ItemId,
			Quantity,
			RequestId);
		Intent.TargetContainer = TargetPlacement.GetContainerHandle();
		Intent.TargetPlacement = TargetPlacement;
		return Intent;
	}

	FRpgInventoryQuickTransferRequest MakeQuickTransferRequest(
		const URpgInventoryManagerComponent* SourceInventory,
		const FRpgInventoryItemId& ItemId,
		int32 Quantity)
	{
		FRpgInventoryQuickTransferRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.ItemId = ItemId;
		Request.StackCount = Quantity;
		FRpgInventoryEntryView Entry;
		if (GetEntryView(SourceInventory, ItemId, Entry))
		{
			Request.ExpectedEntryId = Entry.EntryId;
			Request.ExpectedSourcePlacement = Entry.Placement;
			Request.ExpectedSourceQuantity = Entry.StackCount;
		}
		return Request;
	}

	FRpgInventoryMoveIntent MakeMoveIntent(
		const URpgInventoryManagerComponent* Inventory,
		const FRpgInventoryItemId& ItemId,
		const FRpgInventoryGridPlacement& TargetPlacement)
	{
		FRpgInventoryMoveIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = ItemId;
		Intent.TargetPlacement = TargetPlacement;
		FRpgInventoryEntryView Entry;
		if (GetEntryView(Inventory, ItemId, Entry))
		{
			Intent.ExpectedEntryId = Entry.EntryId;
			Intent.ExpectedSourcePlacement = Entry.Placement;
			Intent.ExpectedQuantity = Entry.StackCount;
		}
		return Intent;
	}

	bool MoveWholeEntryToEquipmentPlacement(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item,
		const FRpgInventoryGridPlacement& TargetPlacement)
	{
		FRpgInventoryEntryView Entry;
		if (!Inventory || !Item ||
			!GetEntryView(Inventory, Item->GetItemId(), Entry))
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

	FRpgInventoryEquipmentIntent MakeEquipmentIntent(
		const URpgInventoryManagerComponent* Inventory,
		const URpgInventoryItemInstance* Item,
		ERpgInventoryEquipmentIntentOperation Operation,
		ERpgEquipmentSlot TargetEquipmentSlot =
			ERpgEquipmentSlot::None)
	{
		FRpgInventoryEquipmentIntent Intent;
		FRpgInventoryEntryView Entry;
		if (!Inventory || !Item ||
			!GetEntryView(Inventory, Item->GetItemId(), Entry))
		{
			return Intent;
		}

		Intent.EnsureRequestId();
		Intent.ItemId = Entry.ItemId;
		Intent.ExpectedEntryId = Entry.EntryId;
		Intent.ExpectedSourcePlacement = Entry.Placement;
		Intent.ExpectedQuantity = Entry.StackCount;
		Intent.Operation = Operation;
		Intent.TargetEquipmentSlot = TargetEquipmentSlot;
		return Intent;
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

	bool ArePlacementSnapshotsExactlyEqual(
		const FRpgInventoryGridPlacement& A,
		const FRpgInventoryGridPlacement& B)
	{
		return A == B;
	}

	FString MakeStrictInventorySignature(
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
				*Entry.Placement.ContainerHandle.ToString(),
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

	FRpgInventoryDragPayload MakeInventoryEntryPayload(
		URpgInventoryManagerComponent* Inventory,
		URpgInventoryItemInstance* Item)
	{
		FRpgInventoryDragPayload Payload;
		FRpgInventoryEntryView Entry;
		if (!Inventory || !Item ||
			!GetEntryView(Inventory, Item->GetItemId(), Entry))
		{
			return Payload;
		}

		Payload.SourceType =
			ERpgInventoryDragSourceType::InventoryEntry;
		Payload.SourceInventory = Inventory;
		Payload.ItemInstance = Item;
		Payload.EntryId = Entry.EntryId;
		Payload.StackCount = Entry.StackCount;
		Payload.SourcePlacement = Entry.Placement;
		Payload.ItemFootprint.Width = 0;
		Payload.ItemFootprint.Height = 0;
		if (const URpgInventoryFragment_SpatialItem* SpatialFragment =
				URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
					Item->GetItemDef()))
		{
			Payload.ItemFootprint = SpatialFragment->Footprint;
		}
		return Payload;
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

	bool CopyInventoryEntryIdForTest(
		URpgInventoryManagerComponent* Inventory,
		int32 SourceEntryIndex,
		int32 TargetEntryIndex)
	{
		if (!Inventory)
		{
			return false;
		}

		const FStructProperty* InventoryListProperty =
			FindFProperty<FStructProperty>(
				URpgInventoryManagerComponent::StaticClass(),
				TEXT("InventoryList"));
		if (!InventoryListProperty ||
			InventoryListProperty->Struct != FRpgInventoryList::StaticStruct())
		{
			return false;
		}

		void* InventoryListMemory =
			InventoryListProperty->ContainerPtrToValuePtr<void>(Inventory);
		const FArrayProperty* EntriesProperty = FindFProperty<FArrayProperty>(
			FRpgInventoryList::StaticStruct(),
			TEXT("Entries"));
		const FStructProperty* EntryProperty = EntriesProperty
			? CastField<FStructProperty>(EntriesProperty->Inner)
			: nullptr;
		if (!InventoryListMemory || !EntriesProperty || !EntryProperty ||
			EntryProperty->Struct != FRpgInventoryEntry::StaticStruct())
		{
			return false;
		}

		void* EntriesMemory =
			EntriesProperty->ContainerPtrToValuePtr<void>(InventoryListMemory);
		FScriptArrayHelper Entries(EntriesProperty, EntriesMemory);
		if (!Entries.IsValidIndex(SourceEntryIndex) ||
			!Entries.IsValidIndex(TargetEntryIndex))
		{
			return false;
		}

		const FStructProperty* EntryIdProperty =
			FindFProperty<FStructProperty>(
				FRpgInventoryEntry::StaticStruct(),
				TEXT("EntryId"));
		if (!EntryIdProperty ||
			EntryIdProperty->Struct != TBaseStructure<FGuid>::Get())
		{
			return false;
		}

		const FGuid* SourceEntryId =
			EntryIdProperty->ContainerPtrToValuePtr<FGuid>(
				Entries.GetRawPtr(SourceEntryIndex));
		FGuid* TargetEntryId =
			EntryIdProperty->ContainerPtrToValuePtr<FGuid>(
				Entries.GetRawPtr(TargetEntryIndex));
		if (!SourceEntryId || !SourceEntryId->IsValid() || !TargetEntryId)
		{
			return false;
		}

		EntryIdProperty->CopyCompleteValue(TargetEntryId, SourceEntryId);
		return *TargetEntryId == *SourceEntryId;
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
	FRpgInventoryItemCapabilitiesTest,
	"SurvivalRpg.Inventory.ItemCapabilities.DefinitionSemanticsAndUseParity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryItemCapabilitiesTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* PlayerInventory =
		TestWorld.CreateInventory(TEXT("CapabilityPlayerInventory"));
	URpgInventoryManagerComponent* ExternalInventory =
		TestWorld.CreateInventory(TEXT("CapabilityExternalInventory"));
	if (!TestNotNull(
			TEXT("The capability player inventory exists"),
			PlayerInventory) ||
		!TestNotNull(
			TEXT("The capability external inventory exists"),
			ExternalInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* NoTraitsItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestNoTraitsItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* UsableItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUsableItemDefinition::
				StaticClass(),
			2,
			MakePlacement(Root, 1, 0));
	URpgInventoryItemInstance* MalformedUsableItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestMalformedUsableItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	URpgInventoryItemInstance* FixedWideItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestFixedWideItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Root, 3, 0));
	URpgInventoryItemInstance* BagItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestGearNameCollisionBagItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Root, 5, 0));
	URpgInventoryItemInstance* NoDropItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestNoDropItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Root, 6, 0));
	URpgInventoryItemInstance* HybridItem =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestHybridWeaponItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Root, 7, 0));
	if (!TestNotNull(
			TEXT("The no-traits capability fixture exists"),
			NoTraitsItem) ||
		!TestNotNull(
			TEXT("The usable capability fixture exists"),
			UsableItem) ||
		!TestNotNull(
			TEXT("The malformed usable capability fixture exists"),
			MalformedUsableItem) ||
		!TestNotNull(
			TEXT("The fixed-orientation capability fixture exists"),
			FixedWideItem) ||
		!TestNotNull(
			TEXT("The item-container capability fixture exists"),
			BagItem) ||
		!TestNotNull(
			TEXT("The no-drop capability fixture exists"),
			NoDropItem) ||
		!TestNotNull(
			TEXT("The hybrid capability fixture exists"),
			HybridItem))
	{
		return false;
	}

	TestEqual(
		TEXT("Missing ItemTraits preserves the historical direct-drop fallback"),
		FRpgInventoryItemCapabilities::ResolveManualDropPolicy(
			NoTraitsItem),
		ERpgInventoryManualDropPolicy::Direct);
	TestEqual(
		TEXT("An explicit disabled manual-drop policy remains disabled"),
		FRpgInventoryItemCapabilities::ResolveManualDropPolicy(
			NoDropItem),
		ERpgInventoryManualDropPolicy::Disabled);
	TestEqual(
		TEXT("A default weapon policy still resolves to confirmation"),
		FRpgInventoryItemCapabilities::ResolveManualDropPolicy(
			HybridItem),
		ERpgInventoryManualDropPolicy::Confirm);

	TestFalse(
		TEXT("A normal spatial item does not invent an item-container contract"),
		FRpgInventoryItemCapabilities::HasItemContainerContract(
			NoTraitsItem));
	TestTrue(
		TEXT("A non-equippable portable container retains its valid grid capability"),
		FRpgInventoryItemCapabilities::HasItemContainerContract(
			BagItem));

	const FRpgInventorySpatialCapability FixedSpatial =
		FRpgInventoryItemCapabilities::ResolveSpatial(FixedWideItem);
	TestTrue(
		TEXT("The fixed-orientation item exposes a valid spatial contract"),
		FixedSpatial.IsValid());
	TestEqual(
		TEXT("The spatial capability preserves the unrotated width"),
		FixedSpatial.Footprint.Width,
		2);
	TestEqual(
		TEXT("The spatial capability preserves the unrotated height"),
		FixedSpatial.Footprint.Height,
		1);
	TestFalse(
		TEXT("The fixed-orientation item does not advertise rotation"),
		FixedSpatial.bAllowRotation);

	const FRpgInventoryUseCapabilityEvaluation PlayerUse =
		FRpgInventoryItemCapabilities::EvaluateUse(
			UsableItem,
			PlayerInventory,
			PlayerInventory,
			2,
			1);
	TestTrue(
		TEXT("A configured owned usable item is locally available"),
		PlayerUse.Result ==
			ERpgInventoryUseCapabilityResult::Available);
	TestEqual(
		TEXT("The shared use contract resolves the exact consume count"),
		PlayerUse.RequiredConsumeCount,
		1);
	TestNotNull(
		TEXT("Available use retains its immutable execution contract"),
		PlayerUse.UseContract);

	const FRpgInventoryUseCapabilityEvaluation ExternalUse =
		FRpgInventoryItemCapabilities::EvaluateUse(
			UsableItem,
			ExternalInventory,
			PlayerInventory,
			2,
			1);
	TestTrue(
		TEXT("OnlyFromPlayerInventory uses the same wrong-inventory result as authority"),
		ExternalUse.Result ==
			ERpgInventoryUseCapabilityResult::WrongInventory);

	const FRpgInventoryUseCapabilityEvaluation InsufficientUse =
		FRpgInventoryItemCapabilities::EvaluateUse(
			UsableItem,
			PlayerInventory,
			PlayerInventory,
			2,
			3);
	TestTrue(
		TEXT("An oversized use request reports insufficient represented quantity"),
		InsufficientUse.Result ==
			ERpgInventoryUseCapabilityResult::InsufficientQuantity);
	TestEqual(
		TEXT("Insufficient use retains the exact required consume count"),
		InsufficientUse.RequiredConsumeCount,
		3);

	const FRpgInventoryUseCapabilityEvaluation InvalidUse =
		FRpgInventoryItemCapabilities::EvaluateUse(
			UsableItem,
			PlayerInventory,
			PlayerInventory,
			2,
			0);
	TestTrue(
		TEXT("A non-positive use count remains an invalid request"),
		InvalidUse.Result ==
			ERpgInventoryUseCapabilityResult::InvalidRequest);

	TestTrue(
		TEXT("A zero-cost usable item cannot amplify multiple activations in one request"),
		FRpgInventoryItemCapabilities::EvaluateUse(
			HybridItem,
			PlayerInventory,
			PlayerInventory,
			1,
			2)
			.Result ==
			ERpgInventoryUseCapabilityResult::InvalidRequest);

	TestTrue(
		TEXT("A malformed UsableItem remains recognizable for legacy binding semantics"),
		FRpgInventoryItemCapabilities::HasUsableContract(
			MalformedUsableItem));
	TestTrue(
		TEXT("A malformed UsableItem without an ability fails closed for execution"),
		FRpgInventoryItemCapabilities::EvaluateUse(
			MalformedUsableItem,
			PlayerInventory,
			PlayerInventory,
			1,
			1)
			.Result ==
			ERpgInventoryUseCapabilityResult::NotConfigured);

	TestTrue(
		TEXT("The default hybrid preference keeps Use when a usable contract is present"),
		FRpgInventoryItemCapabilities::ShouldUseAsPrimaryAction(
			UsableItem,
			true));
	TestFalse(
		TEXT("An authored EquipAndActivate hybrid preference is preserved"),
		FRpgInventoryItemCapabilities::ShouldUseAsPrimaryAction(
			HybridItem,
			true));
	TestTrue(
		TEXT("Without an equipment destination every primary action still falls back to Use"),
		FRpgInventoryItemCapabilities::ShouldUseAsPrimaryAction(
			HybridItem,
			false));

	return true;
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
	URpgInventoryItemInstance* DefinitionlessContainer =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestGearNameCollisionBagItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Pockets, 1, 1));
	if (!TestNotNull(TEXT("A player stack fixture exists"), StackItem) ||
		!TestNotNull(TEXT("A usable player item exists"), UsableItem) ||
		!TestNotNull(TEXT("A no-drop player item exists"), NoDropItem) ||
		!TestNotNull(TEXT("A player weapon fixture exists"), Weapon) ||
		!TestNotNull(TEXT("A player bag fixture exists"), Bag) ||
		!TestNotNull(
			TEXT("A portable definitionless container fixture exists"),
			DefinitionlessContainer))
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
	URpgInventoryEntryViewModel* DefinitionlessContainerViewModel =
		MakeEntryViewModel(
			Coordinator,
			PlayerInventory,
			DefinitionlessContainer->GetItemId());
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
	URpgInventoryAddressSlotViewModel*
		DefinitionlessContainerAddress =
			MakeAddressViewModel(
				Coordinator,
				PlayerInventory,
				InventoryLayout,
				Pockets,
				1,
				1);
	if (!TestNotNull(TEXT("The stack entry projection exists"), StackViewModel) ||
		!TestNotNull(TEXT("The usable entry projection exists"), UsableViewModel) ||
		!TestNotNull(TEXT("The no-drop entry projection exists"), NoDropViewModel) ||
		!TestNotNull(TEXT("The weapon entry projection exists"), WeaponViewModel) ||
		!TestNotNull(TEXT("The bag entry projection exists"), BagViewModel) ||
		!TestNotNull(
			TEXT("The definitionless container entry projection exists"),
			DefinitionlessContainerViewModel) ||
		!TestNotNull(TEXT("The content address projection exists"), ContentAddress) ||
		!TestNotNull(TEXT("The usable content-address projection exists"), UsableAddress) ||
		!TestNotNull(
			TEXT("The definitionless container address projection exists"),
			DefinitionlessContainerAddress))
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
	TestEntryContract(
		TEXT("Player definitionless container"),
		DefinitionlessContainerViewModel,
		true);
	TestAddressContract(TEXT("Player content stack"), ContentAddress, true);
	TestAddressContract(
		TEXT("Player definitionless container address"),
		DefinitionlessContainerAddress,
		true);
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
	TestTrue(
		TEXT("An explicit bag provider exposes EquipAndActivate"),
		Coordinator->CanExecuteContextAction(
			BagViewModel,
			ERpgInventoryContextAction::EquipAndActivate,
			true));
	TestFalse(
		TEXT("A Backpack/Belt bag does not advertise the hand-only MoveToCarry action"),
		Coordinator->CanExecuteContextAction(
			BagViewModel,
			ERpgInventoryContextAction::MoveToCarry,
			true));
	TestTrue(
		TEXT("A definitionless container still exposes its presentation action"),
		Coordinator->CanExecuteContextAction(
			DefinitionlessContainerViewModel,
			ERpgInventoryContextAction::OpenContainer,
			true));
	TestFalse(
		TEXT("A definitionless container entry does not expose EquipAndActivate"),
		Coordinator->CanExecuteContextAction(
			DefinitionlessContainerViewModel,
			ERpgInventoryContextAction::EquipAndActivate,
			true));
	TestFalse(
		TEXT("A definitionless container address does not expose EquipAndActivate"),
		Coordinator->CanExecuteContextAction(
			DefinitionlessContainerAddress,
			ERpgInventoryContextAction::EquipAndActivate,
			true));
	const FString DefinitionlessQuickActionSignature =
		MakeInventorySignature(PlayerInventory);
	TestFalse(
		TEXT("Entry quick action does not dispatch Equip for a definitionless container"),
		Coordinator->UseOrEquipEntry(
			DefinitionlessContainerViewModel,
			1));
	TestFalse(
		TEXT("Address quick action does not dispatch Equip for a definitionless container"),
		Coordinator->UseOrEquipAddressSlot(
			DefinitionlessContainerAddress,
			1));
	TestEqual(
		TEXT("Rejected definitionless quick actions preserve inventory state"),
		MakeInventorySignature(PlayerInventory),
		DefinitionlessQuickActionSignature);
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
			ERpgInventoryContextAction::EquipAndActivate));
	TestFalse(
		TEXT("The Blueprint-callable explicit Use dispatcher reuses the shared source policy"),
		Coordinator->ExecuteEntryItemAction(
			ExternalUsableViewModel,
			ERpgInventoryContextAction::Use));
	TestEqual(
		TEXT("Locally rejected external intents do not mutate inventory state"),
		MakeInventorySignature(ExternalInventory),
		ExternalSignatureBeforeRejectedIntents);

	UiActions->RequestApplyInventoryEquipmentIntent(
		PlayerInventory,
		MakeEquipmentIntent(
			PlayerInventory,
			Weapon,
			ERpgInventoryEquipmentIntentOperation::
				EquipDefaultAndActivate));
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
			ERpgInventoryContextAction::MoveToCarry));

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

	URpgPlayerInventoryLayoutDefinition* ContextLayoutDefinition =
		PlayerState->GetMutableTestInventoryLayoutDefinition();
	FRpgInventorySlotGroupDefinition* PrimaryCarryDefinition =
		ContextLayoutDefinition
			? ContextLayoutDefinition->StaticSlotGroups.FindByPredicate(
				[](const FRpgInventorySlotGroupDefinition& Group)
				{
					return Group.ContainerId ==
						URpgPlayerInventoryLayoutComponent::
							WeaponSlot1GroupId;
				})
			: nullptr;
	if (!TestNotNull(
			TEXT("The context fixture exposes the primary Carry definition"),
			PrimaryCarryDefinition))
	{
		return false;
	}
	const ERpgEquipmentSlot AuthoredPrimaryCarryRole =
		PrimaryCarryDefinition->EquipmentSlotRole;
	PrimaryCarryDefinition->EquipmentSlotRole =
		ERpgEquipmentSlot::OffHand;
	TestFalse(
		TEXT("MainHand context fails closed when the physical Carry group is reauthored as OffHand"),
		Coordinator->CanExecuteContextAction(
			ERpgEquipmentSlot::MainHand,
			Weapon->GetItemId(),
			ERpgInventoryContextAction::Inspect));
	TestTrue(
		TEXT("The cross-role regression still has the same active MainHand mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand) == Weapon);
	PrimaryCarryDefinition->EquipmentSlotRole =
		AuthoredPrimaryCarryRole;
	TestTrue(
		TEXT("Restoring the exact MainHand Carry role restores equipment context"),
		Coordinator->CanExecuteContextAction(
			ERpgEquipmentSlot::MainHand,
			Weapon->GetItemId(),
			ERpgInventoryContextAction::Inspect));

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

	const FRpgInventoryMutationResult ReducedStackResult =
		PlayerInventory->ConsumeItemById(StackItem->GetItemId(), 3);
	TestTrue(
		TEXT("The authoritative fixture reduces the represented stack"),
		ReducedStackResult.IsSuccess() && ReducedStackResult.AppliedQuantity == 3);
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
	URpgInventoryManagerComponent* PreparedDropInventory = nullptr;
	FRpgInventoryManualDropRequest PreparedDropRequest;
	TestFalse(
		TEXT("The entry drop presenter rejects a stale source quantity snapshot"),
		Coordinator->PrepareDropEntryRequest(
			StackViewModel,
			1,
			PreparedDropInventory,
			PreparedDropRequest));
	TestFalse(
		TEXT("The address drop presenter rejects the same stale source quantity snapshot"),
		Coordinator->PrepareDropAddressSlotRequest(
			ContentAddress,
			1,
			PreparedDropInventory,
			PreparedDropRequest));

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

	FRpgInventoryQuickTransferRequest InvalidTransferRequest;
	InvalidTransferRequest.RequestId = FGuid::NewGuid();
	InvalidTransferRequest.StackCount = 1;
	UiActions->RequestQuickTransferItem(
		nullptr,
		nullptr,
		InvalidTransferRequest);
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
	UnconfirmedRequest.ExpectedSourceQuantity = InitialEntry.StackCount;
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

	ARpgDroppedInventoryActor* PhysicalDropActor = nullptr;
	for (TActorIterator<ARpgDroppedInventoryActor> It(World); It; ++It)
	{
		if (*It && !It->IsPendingKillPending())
		{
			PhysicalDropActor = *It;
			break;
		}
	}
	if (TestNotNull(
			TEXT("The confirmed command owns one durable physical drop target"),
			PhysicalDropActor))
	{
		FRpgInventoryTransferIntent PhysicalReplayIntent;
		PhysicalReplayIntent.RequestId = ConfirmedRequest.RequestId;
		PhysicalReplayIntent.ItemId = ConfirmedRequest.ItemId;
		PhysicalReplayIntent.ExpectedEntryId = ConfirmedRequest.EntryId;
		PhysicalReplayIntent.ExpectedSourcePlacement =
			ConfirmedRequest.ExpectedSourcePlacement;
		PhysicalReplayIntent.ExpectedSourceQuantity =
			ConfirmedRequest.ExpectedSourceQuantity;
		PhysicalReplayIntent.Quantity = ConfirmedRequest.StackCount;
		const FRpgInventoryMutationResult PhysicalReplay =
			PhysicalDropActor->TransferItemFromInventoryByIntent(
				Inventory,
				PhysicalReplayIntent);
		TestEqual(
			TEXT("The physical drop kernel replays the caller's exact request id"),
			PhysicalReplay.RequestId,
			ConfirmedRequest.RequestId);
		TestTrue(
			TEXT("The identical physical retry replays success"),
			PhysicalReplay.IsSuccess());
		TestEqual(
			TEXT("The physical retry cannot consume the source twice"),
			Inventory->GetItemStackCount(Item),
			6);
		TestEqual(
			TEXT("The physical retry cannot add the dropped quantity twice"),
			CountDroppedUnits(),
			3);
	}

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
	ProtectedSubtreeRequest.ExpectedSourceQuantity =
		ProviderEntry.StackCount;
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
	FRpgInventoryEquipmentIntentRetryBoundaryTest,
	"SurvivalRpg.Inventory.Intent.Equip.StaleSnapshotAndRetryAreAtomic",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryEquipmentIntentRetryBoundaryTest::RunTest(
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
		TEXT("EquipmentIntentRetryController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<
			ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("EquipmentIntentRetryPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<
			ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The equipment-intent controller exists"), Controller) ||
		!TestNotNull(TEXT("The equipment-intent player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		Controller->GetEquipmentLoadoutComponent();
	if (!TestTrue(TEXT("The fixture executes on authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The typed UI action component exists"), UiActions) ||
		!TestNotNull(TEXT("The equipment selection mirror exists"), EquipmentLoadout))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle WeaponSlot1 =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::
				WeaponSlot1GroupId);
	const FRpgInventoryContainerHandle ShieldSlot =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::
				ShieldSlotGroupId);
	URpgInventoryItemInstance* Weapon =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWeaponItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("A weapon starts in Pockets"), Weapon))
	{
		return false;
	}

	TArray<FRpgInventoryActionFeedbackMessage> FeedbackMessages;
	TArray<FRpgEquipmentLoadoutSlotsChangedMessage>
		EquipmentMessages;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	FGameplayMessageListenerHandle FeedbackHandle =
		MessageSubsystem.RegisterListener<
			FRpgInventoryActionFeedbackMessage>(
			RpgGameplayTags::
				Rpg_Inventory_Message_ActionFeedback,
			[&FeedbackMessages](
				FGameplayTag,
				const FRpgInventoryActionFeedbackMessage& Message)
			{
				FeedbackMessages.Add(Message);
			});
	FGameplayMessageListenerHandle EquipmentHandle =
		MessageSubsystem.RegisterListener<
			FRpgEquipmentLoadoutSlotsChangedMessage>(
			RpgGameplayTags::
				Rpg_EquipmentLoadout_Message_SlotsChanged,
			[&EquipmentMessages](
				FGameplayTag,
				const FRpgEquipmentLoadoutSlotsChangedMessage&
					Message)
			{
				EquipmentMessages.Add(Message);
			});

	FRpgInventoryEntryView PendingEntry;
	if (!TestTrue(
			TEXT("The initial weapon snapshot is available"),
			GetEntryView(
				Inventory,
				Weapon->GetItemId(),
				PendingEntry)))
	{
		FeedbackHandle.Unregister();
		EquipmentHandle.Unregister();
		return false;
	}

	URpgInventoryInteractionSession* PendingSession =
		NewObject<URpgInventoryInteractionSession>(Controller);
	PendingSession->Initialize(Controller, Controller);
	FRpgInventoryDragPayload PendingPayload;
	PendingPayload.SourceType =
		ERpgInventoryDragSourceType::InventoryEntry;
	PendingPayload.SourceInventory = Inventory;
	PendingPayload.ItemInstance = Weapon;
	PendingPayload.EntryId = PendingEntry.EntryId;
	PendingPayload.StackCount = PendingEntry.StackCount;
	PendingPayload.SourcePlacement = PendingEntry.Placement;
	const URpgInventoryFragment_SpatialItem* PendingSpatialFragment =
		URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(
			Weapon->GetItemDef());
	if (!TestNotNull(
			TEXT("The pending weapon owns a canonical spatial contract"),
			PendingSpatialFragment))
	{
		return false;
	}
	PendingPayload.ItemFootprint = PendingSpatialFragment->Footprint;
	FRpgInventoryDropTarget PendingEquipmentTarget;
	PendingEquipmentTarget.TargetType =
		ERpgInventoryDropTargetType::EquipmentSlot;
	PendingEquipmentTarget.EquipmentSlot =
		ERpgEquipmentSlot::MainHand;
	TestTrue(
		TEXT("The equipment interaction accepts an exact source payload"),
		PendingSession->BeginInteraction(
			PendingPayload,
			ERpgInventoryInteractionInputMode::Mouse));
	PendingSession->MarkRequestPending(
		PendingEquipmentTarget,
		RpgGameplayTags::Rpg_Inventory_Action_Equip);

	FRpgInventoryMoveIntent PrematureMove;
	PrematureMove.EnsureRequestId();
	PrematureMove.ItemId = PendingEntry.ItemId;
	PrematureMove.ExpectedEntryId = PendingEntry.EntryId;
	PrematureMove.ExpectedSourcePlacement =
		PendingEntry.Placement;
	PrematureMove.ExpectedQuantity = PendingEntry.StackCount;
	PrematureMove.TargetPlacement =
		MakePlacement(Pockets, 1, 0);
	TestTrue(
		TEXT("The fixture emits a matching physical inventory delta"),
		Inventory->MoveItem(PrematureMove).IsSuccess());
	TestTrue(
		TEXT("An intermediate inventory delta cannot acknowledge a pending equipment command"),
		PendingSession->IsRequestPending());
	PendingSession->RejectRequestLocally();
	PendingSession->CancelInteraction();

	FRpgInventoryEquipmentIntent ValidIntent =
		MakeEquipmentIntent(
			Inventory,
			Weapon,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::MainHand);
	const FString InitialSignature =
		MakeInventorySignature(Inventory);

	FRpgInventoryEquipmentIntent StaleEntryIntent = ValidIntent;
	StaleEntryIntent.RequestId = FGuid::NewGuid();
	StaleEntryIntent.ExpectedEntryId = FGuid::NewGuid();
	const int32 StaleEntryFeedbackIndex =
		FeedbackMessages.Num();
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		StaleEntryIntent);
	TestEqual(
		TEXT("A stale EntryId emits one rejection"),
		FeedbackMessages.Num(),
		StaleEntryFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(
			StaleEntryFeedbackIndex))
	{
		TestEqual(
			TEXT("A stale EntryId is rejected as an invalid snapshot"),
			FeedbackMessages[StaleEntryFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::InvalidRequest);
	}
	TestEqual(
		TEXT("A stale EntryId leaves the inventory byte-for-byte unchanged"),
		MakeInventorySignature(Inventory),
		InitialSignature);
	TestNull(
		TEXT("A stale EntryId cannot activate MainHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand));

	FRpgInventoryEquipmentIntent StalePlacementIntent =
		ValidIntent;
	StalePlacementIntent.RequestId = FGuid::NewGuid();
	++StalePlacementIntent.ExpectedSourcePlacement.X;
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		StalePlacementIntent);
	TestEqual(
		TEXT("A stale source placement leaves the inventory unchanged"),
		MakeInventorySignature(Inventory),
		InitialSignature);
	TestNull(
		TEXT("A stale source placement cannot activate MainHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand));

	FRpgInventoryEquipmentIntent StaleQuantityIntent =
		ValidIntent;
	StaleQuantityIntent.RequestId = FGuid::NewGuid();
	++StaleQuantityIntent.ExpectedQuantity;
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		StaleQuantityIntent);
	TestEqual(
		TEXT("A partial/stale equipment quantity remains atomic"),
		MakeInventorySignature(Inventory),
		InitialSignature);

	const int32 SuccessFeedbackIndex =
		FeedbackMessages.Num();
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		ValidIntent);
	TestEqual(
		TEXT("The valid intent emits one result"),
		FeedbackMessages.Num(),
		SuccessFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(SuccessFeedbackIndex))
	{
		TestEqual(
			TEXT("The valid intent succeeds"),
			FeedbackMessages[SuccessFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::Success);
		TestEqual(
			TEXT("Equipment feedback preserves the caller RequestId"),
			FeedbackMessages[SuccessFeedbackIndex].RequestId,
			ValidIntent.RequestId);
	}

	FRpgInventoryEntryView EquippedEntry;
	TestTrue(
		TEXT("The equipped weapon remains addressable"),
		GetEntryView(
			Inventory,
			Weapon->GetItemId(),
			EquippedEntry));
	TestEqual(
		TEXT("The physical transaction moves the weapon to WeaponSlot1"),
		EquippedEntry.Placement.GetContainerHandle(),
		WeaponSlot1);
	TestEqual(
		TEXT("The post-commit selection activates MainHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand),
		Weapon);

	const FString SignatureBeforeNoOpMove =
		MakeInventorySignature(Inventory);
	const int32 EquipmentMessageCountBeforeNoOpMove =
		EquipmentMessages.Num();
	FRpgInventoryMoveIntent NoOpMove;
	NoOpMove.EnsureRequestId();
	NoOpMove.ItemId = EquippedEntry.ItemId;
	NoOpMove.ExpectedEntryId = EquippedEntry.EntryId;
	NoOpMove.ExpectedSourcePlacement =
		EquippedEntry.Placement;
	NoOpMove.ExpectedQuantity = EquippedEntry.StackCount;
	NoOpMove.TargetPlacement = EquippedEntry.Placement;
	UiActions->RequestMoveInventoryItem(Inventory, NoOpMove);
	UiActions->RequestMoveInventoryItem(Inventory, NoOpMove);
	TestEqual(
		TEXT("A no-op Gear move and its replay leave physical state unchanged"),
		MakeInventorySignature(Inventory),
		SignatureBeforeNoOpMove);
	TestEqual(
		TEXT("A no-op Gear move and its replay do not rebuild equipment state"),
		EquipmentMessages.Num(),
		EquipmentMessageCountBeforeNoOpMove);

	URpgInventoryItemInstance* UnrelatedItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	FRpgInventoryEntryView UnrelatedEntry;
	if (!TestNotNull(
			TEXT("An unrelated Pockets item exists"),
			UnrelatedItem) ||
		!TestTrue(
			TEXT("The unrelated item has an exact source snapshot"),
			GetEntryView(
				Inventory,
				UnrelatedItem
					? UnrelatedItem->GetItemId()
					: FRpgInventoryItemId(),
				UnrelatedEntry)))
	{
		FeedbackHandle.Unregister();
		EquipmentHandle.Unregister();
		return false;
	}

	const int32 EquipmentMessageCountBeforeUnrelatedMove =
		EquipmentMessages.Num();
	FRpgInventoryMoveIntent UnrelatedMove;
	UnrelatedMove.EnsureRequestId();
	UnrelatedMove.ItemId = UnrelatedEntry.ItemId;
	UnrelatedMove.ExpectedEntryId = UnrelatedEntry.EntryId;
	UnrelatedMove.ExpectedSourcePlacement =
		UnrelatedEntry.Placement;
	UnrelatedMove.ExpectedQuantity = UnrelatedEntry.StackCount;
	UnrelatedMove.TargetPlacement =
		MakePlacement(Pockets, 2, 0);
	UiActions->RequestMoveInventoryItem(
		Inventory,
		UnrelatedMove);
	TestEqual(
		TEXT("A Content-only move does not rebuild equipment actors or grants"),
		EquipmentMessages.Num(),
		EquipmentMessageCountBeforeUnrelatedMove);

	const FString SignatureBeforeReplay =
		MakeInventorySignature(Inventory);
	const int32 EquipmentMessageCountBeforeReplay =
		EquipmentMessages.Num();
	const int32 ReplayFeedbackIndex =
		FeedbackMessages.Num();
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		ValidIntent);
	TestEqual(
		TEXT("An identical retry replays exactly one feedback"),
		FeedbackMessages.Num(),
		ReplayFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(ReplayFeedbackIndex))
	{
		TestEqual(
			TEXT("An identical retry replays success"),
			FeedbackMessages[ReplayFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::Success);
	}
	TestEqual(
		TEXT("An identical retry performs no second physical mutation"),
		MakeInventorySignature(Inventory),
		SignatureBeforeReplay);
	TestEqual(
		TEXT("An identical retry performs no second loadout reconciliation"),
		EquipmentMessages.Num(),
		EquipmentMessageCountBeforeReplay);
	TestEqual(
		TEXT("An identical retry cannot toggle MainHand off"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand),
		Weapon);

	FRpgInventoryEquipmentIntent CollisionIntent =
		ValidIntent;
	CollisionIntent.TargetEquipmentSlot =
		ERpgEquipmentSlot::OffHand;
	const int32 CollisionFeedbackIndex =
		FeedbackMessages.Num();
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		CollisionIntent);
	TestEqual(
		TEXT("A RequestId collision emits one rejection"),
		FeedbackMessages.Num(),
		CollisionFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(CollisionFeedbackIndex))
	{
		TestEqual(
			TEXT("Changing the target under one RequestId is rejected"),
			FeedbackMessages[CollisionFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::InvalidRequest);
	}
	TestEqual(
		TEXT("The collision cannot mutate the committed placement"),
		MakeInventorySignature(Inventory),
		SignatureBeforeReplay);
	TestEqual(
		TEXT("The collision cannot change active MainHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand),
		Weapon);

	URpgInventoryItemInstance* OffHandItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackableOffHandItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("An OffHand item starts in Pockets"), OffHandItem))
	{
		FeedbackHandle.Unregister();
		EquipmentHandle.Unregister();
		return false;
	}
	FRpgInventoryEquipmentIntent EquipOffHandIntent =
		MakeEquipmentIntent(
			Inventory,
			OffHandItem,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::OffHand);
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		EquipOffHandIntent);
	TestEqual(
		TEXT("The typed OffHand intent activates only OffHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::OffHand),
		OffHandItem);
	FRpgInventoryEntryView EquippedOffHandEntry;
	TestTrue(
		TEXT("The OffHand item remains addressable after equip"),
		GetEntryView(
			Inventory,
			OffHandItem->GetItemId(),
			EquippedOffHandEntry));
	TestEqual(
		TEXT("The physical OffHand item occupies ShieldSlot"),
		EquippedOffHandEntry.Placement.GetContainerHandle(),
		ShieldSlot);

	FRpgInventoryEquipmentIntent ClearHandIntent =
		MakeEquipmentIntent(
			Inventory,
			Weapon,
			ERpgInventoryEquipmentIntentOperation::
				ClearActiveSelection,
			ERpgEquipmentSlot::MainHand);
	const FString SignatureBeforeClear =
		MakeInventorySignature(Inventory);
	const int32 ClearFeedbackIndex = FeedbackMessages.Num();
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		ClearHandIntent);
	TestEqual(
		TEXT("The activation-only clear emits one result"),
		FeedbackMessages.Num(),
		ClearFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(ClearFeedbackIndex))
	{
		TestEqual(
			TEXT("The activation-only clear succeeds"),
			FeedbackMessages[ClearFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::Success);
		TestEqual(
			TEXT("The activation-only clear preserves request correlation"),
			FeedbackMessages[ClearFeedbackIndex].RequestId,
			ClearHandIntent.RequestId);
	}
	TestNull(
		TEXT("The activation-only clear holsters MainHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand));
	TestEqual(
		TEXT("Clearing MainHand preserves the independent OffHand selection"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::OffHand),
		OffHandItem);
	TestEqual(
		TEXT("Holstering never changes the physical Carry placement"),
		MakeInventorySignature(Inventory),
		SignatureBeforeClear);

	const int32 EquipmentMessageCountBeforeClearReplay =
		EquipmentMessages.Num();
	const int32 ClearReplayFeedbackIndex =
		FeedbackMessages.Num();
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		ClearHandIntent);
	TestEqual(
		TEXT("A clear retry replays exactly one result"),
		FeedbackMessages.Num(),
		ClearReplayFeedbackIndex + 1);
	if (FeedbackMessages.IsValidIndex(ClearReplayFeedbackIndex))
	{
		TestEqual(
			TEXT("A clear retry replays success"),
			FeedbackMessages[ClearReplayFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::Success);
	}
	TestEqual(
		TEXT("A clear retry does not repeat loadout side effects"),
		EquipmentMessages.Num(),
		EquipmentMessageCountBeforeClearReplay);
	TestEqual(
		TEXT("A clear retry cannot mutate the physical inventory"),
		MakeInventorySignature(Inventory),
		SignatureBeforeClear);
	TestEqual(
		TEXT("A clear retry also preserves OffHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::OffHand),
		OffHandItem);

	FRpgInventoryEquipmentIntent ClearCollisionIntent =
		ClearHandIntent;
	ClearCollisionIntent.TargetEquipmentSlot =
		ERpgEquipmentSlot::OffHand;
	const int32 ClearCollisionFeedbackIndex =
		FeedbackMessages.Num();
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		ClearCollisionIntent);
	if (FeedbackMessages.IsValidIndex(
			ClearCollisionFeedbackIndex))
	{
		TestEqual(
			TEXT("A clear RequestId collision is rejected"),
			FeedbackMessages[ClearCollisionFeedbackIndex].Result,
			ERpgInventoryActionFeedbackResult::InvalidRequest);
	}
	TestEqual(
		TEXT("The clear RequestId collision cannot clear OffHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::OffHand),
		OffHandItem);

	FeedbackHandle.Unregister();
	EquipmentHandle.Unregister();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryEquipmentRuntimeReconcileLifecycleTest,
	"SurvivalRpg.Inventory.Intent.Equip.RuntimeReconcileIsTwoPhase",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryEquipmentRuntimeReconcileLifecycleTest::RunTest(
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
		TEXT("EquipmentRuntimeReconcileController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("EquipmentRuntimeReconcilePlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);

	FActorSpawnParameters PawnSpawnParameters;
	PawnSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgEquipmentAutomationTestPawn::StaticClass(),
		TEXT("EquipmentRuntimeReconcilePawn"));
	PawnSpawnParameters.ObjectFlags = RF_Transient;
	PawnSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgEquipmentAutomationTestPawn* Pawn =
		World->SpawnActor<ARpgEquipmentAutomationTestPawn>(
			PawnSpawnParameters);
	if (!TestNotNull(TEXT("The runtime-reconcile controller exists"), Controller) ||
		!TestNotNull(TEXT("The runtime-reconcile player state exists"), PlayerState) ||
		!TestNotNull(TEXT("The authoritative GAS equipment pawn exists"), Pawn))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	Controller->Possess(Pawn);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		Controller->GetEquipmentLoadoutComponent();
	URpgEquipmentManagerComponent* EquipmentManager =
		Pawn->GetEquipmentManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	URpgAbilitySystemComponent* AbilitySystemComponent =
		Pawn->GetRpgAbilitySystemComponent();
	URpgHealthSet* HealthSet = Pawn->GetHealthSet();
	if (!TestNotNull(TEXT("The canonical player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The controller loadout mirror exists"), EquipmentLoadout) ||
		!TestNotNull(TEXT("The pawn equipment manager exists"), EquipmentManager) ||
		!TestNotNull(TEXT("The typed inventory gateway exists"), UiActions) ||
		!TestNotNull(TEXT("The pawn ability system exists"), AbilitySystemComponent) ||
		!TestNotNull(TEXT("The pawn health set exists"), HealthSet))
	{
		return false;
	}
	if (!AbilitySystemComponent->GetSet<URpgHealthSet>())
	{
		AbilitySystemComponent->AddAttributeSetSubobject(HealthSet);
	}
	AbilitySystemComponent->InitAbilityActorInfo(Pawn, Pawn);

	const FRpgInventoryContainerHandle WeaponSlot1 =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	const FRpgInventoryContainerHandle WeaponSlot2 =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::WeaponSlot2GroupId);
	const FRpgInventoryContainerHandle ShieldSlot =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId);
	URpgInventoryItemInstance* OneHandItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
			1,
			MakePlacement(
				FRpgInventoryContainerHandle::MakeRoot(
					URpgPlayerInventoryLayoutComponent::PocketsGroupId),
				0,
				0));
	URpgInventoryItemInstance* OffHandItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackableOffHandItemDefinition::StaticClass(),
			1,
			MakePlacement(
				FRpgInventoryContainerHandle::MakeRoot(
					URpgPlayerInventoryLayoutComponent::PocketsGroupId),
				1,
				0));
	URpgInventoryItemInstance* TwoHandItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestTwoHandItemDefinition::StaticClass(),
			1,
			MakePlacement(
				FRpgInventoryContainerHandle::MakeRoot(
					URpgPlayerInventoryLayoutComponent::PocketsGroupId),
				2,
				0));
	if (!TestNotNull(TEXT("A one-handed Carry item exists"), OneHandItem) ||
		!TestNotNull(TEXT("An OffHand Carry item exists"), OffHandItem) ||
		!TestNotNull(TEXT("A two-handed Carry item exists"), TwoHandItem))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("The one-handed item moves through the trusted Carry seam"),
			MoveWholeEntryToEquipmentPlacement(
				Inventory,
				OneHandItem,
				MakePlacement(WeaponSlot1, 0, 0))) ||
		!TestTrue(
			TEXT("The OffHand item moves through the trusted Carry seam"),
			MoveWholeEntryToEquipmentPlacement(
				Inventory,
				OffHandItem,
				MakePlacement(ShieldSlot, 0, 0))) ||
		!TestTrue(
			TEXT("The two-handed item moves through the trusted Carry seam"),
			MoveWholeEntryToEquipmentPlacement(
				Inventory,
				TwoHandItem,
				MakePlacement(WeaponSlot2, 0, 0))))
	{
		return false;
	}

	FRpgEquipmentSelectionSaveData InitialSelection;
	InitialSelection.ActiveMainHandItemId = OneHandItem->GetItemId();
	InitialSelection.ActiveOffHandItemId = OffHandItem->GetItemId();
	EquipmentLoadout->RestoreEquipmentSelection(InitialSelection);

	URpgInventoryAutomationTestCountingEquipmentInstance* InitialMainRuntime =
		Cast<URpgInventoryAutomationTestCountingEquipmentInstance>(
			EquipmentManager->GetEquipmentInstanceInSlot(
				ERpgEquipmentSlot::MainHand));
	URpgInventoryAutomationTestCountingEquipmentInstance* InitialOffRuntime =
		Cast<URpgInventoryAutomationTestCountingEquipmentInstance>(
			EquipmentManager->GetEquipmentInstanceInSlot(
				ERpgEquipmentSlot::OffHand));
	if (!TestNotNull(TEXT("The initial MainHand runtime instance exists"), InitialMainRuntime) ||
		!TestNotNull(TEXT("The initial OffHand runtime instance exists"), InitialOffRuntime))
	{
		return false;
	}
	TestEqual(TEXT("Initial MainHand receives one equip callback"), InitialMainRuntime->GetEquippedCount(), 1);
	TestEqual(TEXT("Initial OffHand receives one equip callback"), InitialOffRuntime->GetEquippedCount(), 1);

	const FString PhysicalCarryStateBeforeHandActivation =
		MakeStrictInventorySignature(Inventory);
	TestTrue(
		TEXT("The native hand-selection seam activates the ready two-handed item"),
		EquipmentLoadout->SetMainHandItemActive(TwoHandItem));

	TestEqual(TEXT("Replaced MainHand receives exactly one unequip callback"), InitialMainRuntime->GetUnequippedCount(), 1);
	TestEqual(TEXT("The two-hand conflict removes OffHand exactly once"), InitialOffRuntime->GetUnequippedCount(), 1);
	TestEqual(
		TEXT("The loadout mirror selects the two-handed item"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::MainHand),
		TwoHandItem);
	TestNull(
		TEXT("The two-handed selection leaves no active OffHand mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(ERpgEquipmentSlot::OffHand));

	URpgInventoryAutomationTestCountingEquipmentInstance* TwoHandRuntime =
		Cast<URpgInventoryAutomationTestCountingEquipmentInstance>(
			EquipmentManager->GetEquipmentInstanceInSlot(
				ERpgEquipmentSlot::MainHand));
	if (!TestNotNull(TEXT("The two-handed runtime instance exists"), TwoHandRuntime))
	{
		return false;
	}
	TestEqual(TEXT("The replacement receives one equip callback"), TwoHandRuntime->GetEquippedCount(), 1);
	TestEqual(TEXT("The replacement remains equipped"), TwoHandRuntime->GetUnequippedCount(), 0);
	TestFalse(
		TEXT("The native hand-selection seam rejects OffHand while a two-handed MainHand is active"),
		EquipmentLoadout->SetOffHandItemActive(OffHandItem));
	TestEqual(
		TEXT("A rejected OffHand activation preserves the two-handed MainHand runtime"),
		EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand),
		static_cast<URpgEquipmentInstance*>(TwoHandRuntime));
	TestNull(
		TEXT("A rejected OffHand activation creates no OffHand runtime"),
		EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::OffHand));
	TestTrue(
		TEXT("Selecting the already-active two-handed item is idempotent"),
		EquipmentLoadout->SetMainHandItemActive(TwoHandItem));
	TestEqual(
		TEXT("Idempotent hand selection preserves the runtime instance"),
		EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand),
		static_cast<URpgEquipmentInstance*>(TwoHandRuntime));
	TestEqual(
		TEXT("Hand activation never mutates physical Carry placement"),
		MakeStrictInventorySignature(Inventory),
		PhysicalCarryStateBeforeHandActivation);
	TestTrue(
		TEXT("The two-handed runtime blocks OffHand input"),
		EquipmentManager->IsEquipmentSlotBlocked(ERpgEquipmentSlot::OffHand));
	TestEqual(
		TEXT("Only the two-handed runtime entry survives reconciliation"),
		EquipmentManager->GetEquipmentInstancesOfType(
			URpgInventoryAutomationTestCountingEquipmentInstance::StaticClass()).Num(),
		1);

	TestTrue(
		TEXT("An idempotent reconcile reports a complete target state"),
		EquipmentLoadout->ReconcileRuntimeEquipmentOnCurrentPawn());
	TestEqual(
		TEXT("Idempotent reconcile preserves the runtime instance"),
		EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand),
		static_cast<URpgEquipmentInstance*>(TwoHandRuntime));
	TestEqual(TEXT("Idempotent reconcile emits no second equip callback"), TwoHandRuntime->GetEquippedCount(), 1);
	TestEqual(TEXT("Idempotent reconcile emits no unequip callback"), TwoHandRuntime->GetUnequippedCount(), 0);

	FRpgInventoryEntryView BlockedOffHandEntry;
	if (!TestTrue(
			TEXT("The inactive OffHand candidate remains physically addressable"),
			GetEntryView(
				Inventory,
				OffHandItem->GetItemId(),
				BlockedOffHandEntry)))
	{
		return false;
	}
	FRpgInventoryEquipmentIntent BlockedOffHandIntent =
		MakeEquipmentIntent(
			Inventory,
			OffHandItem,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::OffHand);
	FRpgInventoryGridPlacement BlockedOffHandPlacement;
	const FRpgInventoryPlacementPlan BlockedOffHandPlan =
		UiActions->PlanEquipmentIntentPlacement(
			Inventory,
			BlockedOffHandIntent,
			BlockedOffHandPlacement);
	TestEqual(
		TEXT("An active two-handed MainHand rejects the dynamic OffHand plan"),
		BlockedOffHandPlan.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	TestFalse(
		TEXT("The dynamic hand conflict has no complete physical placement"),
		BlockedOffHandPlan.IsCompleteSuccess());

	URpgInventoryDragDropCoordinator* HandConflictCoordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(
				Controller,
				Controller);
	if (!TestNotNull(
			TEXT("The runtime fixture owns a hand-conflict preview coordinator"),
			HandConflictCoordinator))
	{
		return false;
	}
	HandConflictCoordinator->SetUiActionComponent(UiActions);
	FRpgInventoryDragPayload BlockedOffHandPayload;
	BlockedOffHandPayload.SourceType =
		ERpgInventoryDragSourceType::InventoryEntry;
	BlockedOffHandPayload.SourceInventory = Inventory;
	BlockedOffHandPayload.ItemInstance = OffHandItem;
	BlockedOffHandPayload.EntryId = BlockedOffHandEntry.EntryId;
	BlockedOffHandPayload.StackCount = BlockedOffHandEntry.StackCount;
	BlockedOffHandPayload.SourcePlacement =
		BlockedOffHandEntry.Placement;
	BlockedOffHandPayload.ItemFootprint =
		BlockedOffHandEntry.Placement.GetUnrotatedSize();
	const FRpgInventoryDropTarget BlockedOffHandTarget =
		URpgInventoryDragDropCoordinator::MakeEquipmentTarget(
			ERpgEquipmentSlot::OffHand);
	const FRpgInventoryInteractionPreviewPlan BlockedOffHandPreview =
		HandConflictCoordinator->PlanInteractionPreview(
			BlockedOffHandPayload,
			BlockedOffHandTarget);
	TestTrue(
		TEXT("The OffHand UI consumes the same rejected domain plan"),
		BlockedOffHandPreview.bUsesPlacementPlan);
	TestEqual(
		TEXT("The active two-hand conflict previews as Blocked"),
		BlockedOffHandPreview.State,
		ERpgInventoryInteractionPreviewState::Blocked);
	TestFalse(
		TEXT("A blocked OffHand preview cannot dispatch a UI commit"),
		HandConflictCoordinator->CommitPayloadToTarget(
			BlockedOffHandPayload,
			BlockedOffHandTarget));

	const FString InventoryBeforeBlockedOffHand =
		MakeInventorySignature(Inventory);
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		BlockedOffHandIntent);
	TestEqual(
		TEXT("The authoritative dynamic-hand rejection leaves physical inventory unchanged"),
		MakeInventorySignature(Inventory),
		InventoryBeforeBlockedOffHand);
	TestEqual(
		TEXT("The rejected OffHand request preserves the active two-handed MainHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand),
		TwoHandItem);
	TestNull(
		TEXT("The rejected OffHand request leaves OffHand empty"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::OffHand));

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* MovableGrantArmor =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestMovableGrantItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(
			TEXT("Grant-bearing armor starts in Content"),
			MovableGrantArmor))
	{
		return false;
	}

	FRpgInventoryEquipmentIntent EquipChestIntent =
		MakeEquipmentIntent(
			Inventory,
			MovableGrantArmor,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::Chest);
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		EquipChestIntent);
	TestEqual(
		TEXT("One physical armor runtime raises MaxHealth once"),
		HealthSet->GetMaxHealth(),
		600.0f);
	URpgInventoryAutomationTestCountingEquipmentInstance* ChestRuntime =
		Cast<URpgInventoryAutomationTestCountingEquipmentInstance>(
			EquipmentManager->GetEquipmentInstanceInSlot(
				ERpgEquipmentSlot::Chest));
	if (!TestNotNull(
			TEXT("The Chest runtime for grant-bearing armor exists"),
			ChestRuntime))
	{
		return false;
	}

	float PeakObservedMaxHealth = HealthSet->GetMaxHealth();
	const FDelegateHandle MaxHealthHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			URpgHealthSet::GetMaxHealthAttribute()).AddLambda(
			[&PeakObservedMaxHealth](const FOnAttributeChangeData& ChangeData)
			{
				PeakObservedMaxHealth = FMath::Max(
					PeakObservedMaxHealth,
					ChangeData.NewValue);
			});

	FRpgInventoryEquipmentIntent MoveArmorToHeadIntent =
		MakeEquipmentIntent(
			Inventory,
			MovableGrantArmor,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::Head);
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		MoveArmorToHeadIntent);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		URpgHealthSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);

	TestEqual(
		TEXT("Moving one armor instance preserves one final MaxHealth grant"),
		HealthSet->GetMaxHealth(),
		600.0f);
	TestEqual(
		TEXT("Two-phase reconcile never exposes a duplicate MaxHealth grant"),
		PeakObservedMaxHealth,
		600.0f);
	TestEqual(
		TEXT("The replaced Chest runtime is unequipped exactly once"),
		ChestRuntime->GetUnequippedCount(),
		1);
	TestNull(
		TEXT("The old Chest runtime slot is empty"),
		EquipmentManager->GetEquipmentInstanceInSlot(
			ERpgEquipmentSlot::Chest));
	URpgInventoryAutomationTestCountingEquipmentInstance* HeadRuntime =
		Cast<URpgInventoryAutomationTestCountingEquipmentInstance>(
			EquipmentManager->GetEquipmentInstanceInSlot(
				ERpgEquipmentSlot::Head));
	if (!TestNotNull(TEXT("The replacement Head runtime exists"), HeadRuntime))
	{
		return false;
	}
	TestEqual(
		TEXT("The replacement runtime keeps the same concrete item instigator"),
		HeadRuntime->GetInstigator(),
		static_cast<UObject*>(MovableGrantArmor));

	const FString InventoryBeforeRepeatedPhysicalReconcile =
		MakeStrictInventorySignature(Inventory);
	TestTrue(
		TEXT("The first repeated physical reconcile succeeds"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	TestTrue(
		TEXT("The second repeated physical reconcile succeeds"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	TestEqual(
		TEXT("Repeated physical reconcile preserves the armor runtime instance"),
		EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::Head),
		static_cast<URpgEquipmentInstance*>(HeadRuntime));
	TestEqual(
		TEXT("Repeated physical reconcile preserves the active hand runtime instance"),
		EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand),
		static_cast<URpgEquipmentInstance*>(TwoHandRuntime));
	TestEqual(
		TEXT("Repeated physical reconcile emits no duplicate armor equip callback"),
		HeadRuntime->GetEquippedCount(),
		1);
	TestEqual(
		TEXT("Repeated physical reconcile does not unequip unchanged armor"),
		HeadRuntime->GetUnequippedCount(),
		0);
	TestEqual(
		TEXT("Repeated physical reconcile cannot duplicate the persistent GAS grant"),
		HealthSet->GetMaxHealth(),
		600.0f);
	TestEqual(
		TEXT("Repeated physical reconcile cannot mutate physical inventory state"),
		MakeStrictInventorySignature(Inventory),
		InventoryBeforeRepeatedPhysicalReconcile);

	int32 RuntimeCountForArmor = 0;
	for (URpgEquipmentInstance* RuntimeInstance :
		EquipmentManager->GetEquipmentInstancesOfType(
			URpgEquipmentInstance::StaticClass()))
	{
		RuntimeCountForArmor += RuntimeInstance &&
			RuntimeInstance->GetInstigator() == MovableGrantArmor
			? 1
			: 0;
	}
	TestEqual(
		TEXT("Exactly one runtime entry represents the moved armor"),
		RuntimeCountForArmor,
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventorySpatialEquipmentIdentityMoveTest,
	"SurvivalRpg.Inventory.Intent.Equip.SpatialDropPreservesStackIdentity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventorySpatialEquipmentIdentityMoveTest::RunTest(
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
		TEXT("SpatialEquipmentIdentityController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("SpatialEquipmentIdentityPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The spatial-equipment controller exists"), Controller) ||
		!TestNotNull(TEXT("The spatial-equipment player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(
			Controller,
			Controller);
	if (!TestNotNull(TEXT("The canonical player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The typed inventory gateway exists"), UiActions) ||
		!TestNotNull(TEXT("The screen-local drag coordinator exists"), Coordinator) ||
		!TestNotNull(
			TEXT("The drag coordinator owns an interaction session"),
			Coordinator ? Coordinator->GetInteractionSession() : nullptr))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle WeaponSlot1 =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	URpgInventoryItemInstance* MovingStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass(),
			4,
			MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* OccupyingStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass(),
			9,
			MakePlacement(Pockets, 1, 0));
	if (!TestNotNull(TEXT("A four-item source stack exists"), MovingStack) ||
		!TestNotNull(TEXT("A nine-item Carry stack exists"), OccupyingStack))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("The target stack enters Carry through the trusted setup seam"),
			MoveWholeEntryToEquipmentPlacement(
				Inventory,
				OccupyingStack,
				MakePlacement(WeaponSlot1, 0, 0))))
	{
		return false;
	}

	FRpgInventoryEntryView MovingEntry;
	FRpgInventoryEntryView OccupyingEntry;
	if (!TestTrue(
			TEXT("The moving stack has an exact source snapshot"),
			GetEntryView(Inventory, MovingStack->GetItemId(), MovingEntry)) ||
		!TestTrue(
			TEXT("The occupying stack has an exact target snapshot"),
			GetEntryView(Inventory, OccupyingStack->GetItemId(), OccupyingEntry)))
	{
		return false;
	}

	FRpgInventoryDragPayload Payload;
	Payload.SourceType = ERpgInventoryDragSourceType::InventoryEntry;
	Payload.SourceInventory = Inventory;
	Payload.ItemInstance = MovingStack;
	Payload.EntryId = MovingEntry.EntryId;
	Payload.StackCount = MovingEntry.StackCount;
	Payload.SourcePlacement = MovingEntry.Placement;
	Payload.ItemFootprint = MovingEntry.Placement.GetUnrotatedSize();
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	Target.TargetInventory = Inventory;
	Target.TargetPlacement = OccupyingEntry.Placement;
	const FRpgInventoryInteractionPreviewPlan PlacementPreview =
		Coordinator->PlanInteractionPreview(Payload, Target);
	TestTrue(
		TEXT("The spatial preview is projected from a complete domain placement plan"),
		PlacementPreview.bUsesPlacementPlan &&
			PlacementPreview.PlacementPlan.IsCompleteSuccess());
	if (!TestEqual(
			TEXT("The equipment preview contains one atomic placement step"),
			PlacementPreview.PlacementPlan.Steps.Num(),
			1))
	{
		return false;
	}
	const FRpgInventoryPlacementStep& SwapStep =
		PlacementPreview.PlacementPlan.Steps[0];
	TestEqual(
		TEXT("The domain plan selects Swap even for compatible equipment stacks"),
		SwapStep.Resolution,
		ERpgInventoryPlacementResolution::Swap);
	TestEqual(
		TEXT("The preview names the concrete displaced item"),
		SwapStep.DisplacedItemId,
		OccupyingEntry.ItemId);
	TestEqual(
		TEXT("The preview names the concrete displaced entry"),
		SwapStep.DisplacedEntryId,
		OccupyingEntry.EntryId);
	TestEqual(
		TEXT("The preview resolves the displaced entry to the exact source placement"),
		SwapStep.DisplacedPlacement,
		MovingEntry.Placement);

	TestTrue(
		TEXT("The exact spatial equipment move has a valid preview"),
		Coordinator->UpdateInteractionPreview(Payload, Target));
	TestEqual(
		TEXT("A compatible Equipment target previews an identity-preserving swap, not a merge"),
		Coordinator->GetInteractionSession()->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::Swap);
	TestTrue(
		TEXT("The exact spatial equipment drop dispatches through the typed gateway"),
		Coordinator->CommitPayloadToTarget(Payload, Target));

	FRpgInventoryEntryView MovedEntry;
	FRpgInventoryEntryView DisplacedEntry;
	TestTrue(
		TEXT("The moving ItemId survives the equipment drop"),
		GetEntryView(Inventory, MovingStack->GetItemId(), MovedEntry));
	TestTrue(
		TEXT("The displaced ItemId survives the equipment drop"),
		GetEntryView(Inventory, OccupyingStack->GetItemId(), DisplacedEntry));
	TestEqual(
		TEXT("The complete moving stack owns the Carry placement"),
		MovedEntry.Placement.GetContainerHandle(),
		WeaponSlot1);
	TestEqual(
		TEXT("The displaced stack moves atomically to the exact source placement"),
		DisplacedEntry.Placement,
		MovingEntry.Placement);
	TestEqual(TEXT("The moving stack quantity is unchanged"), MovedEntry.StackCount, 4);
	TestEqual(TEXT("The displaced stack quantity is unchanged"), DisplacedEntry.StackCount, 9);
	TestEqual(
		TEXT("The moving runtime instance is preserved"),
		Inventory->FindItemById(MovingStack->GetItemId()),
		MovingStack);
	TestEqual(
		TEXT("The displaced runtime instance is preserved"),
		Inventory->FindItemById(OccupyingStack->GetItemId()),
		OccupyingStack);
	TestFalse(
		TEXT("The pre-commit payload is stale after the atomic swap"),
		Coordinator->PreviewPayloadDrop(Payload, Target));
	TestFalse(
		TEXT("The stale payload cannot dispatch a second mutation"),
		Coordinator->CommitPayloadToTarget(Payload, Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryEquipmentSplitDerivedSyncTest,
	"SurvivalRpg.Inventory.Intent.Equip.SplitIntoCarryRefreshesDerivedState",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryEquipmentSplitDerivedSyncTest::RunTest(
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
		TEXT("EquipmentSplitSyncController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("EquipmentSplitSyncPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);

	FActorSpawnParameters PawnSpawnParameters;
	PawnSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgEquipmentAutomationTestPawn::StaticClass(),
		TEXT("EquipmentSplitSyncPawn"));
	PawnSpawnParameters.ObjectFlags = RF_Transient;
	PawnSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgEquipmentAutomationTestPawn* Pawn =
		World->SpawnActor<ARpgEquipmentAutomationTestPawn>(
			PawnSpawnParameters);
	if (!TestNotNull(TEXT("The split-sync controller exists"), Controller) ||
		!TestNotNull(TEXT("The split-sync player state exists"), PlayerState) ||
		!TestNotNull(TEXT("The split-sync equipment pawn exists"), Pawn))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	Controller->Possess(Pawn);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		Controller->GetEquipmentLoadoutComponent();
	if (!TestNotNull(TEXT("The canonical player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The typed inventory gateway exists"), UiActions) ||
		!TestNotNull(TEXT("The equipment derived-state mirror exists"), EquipmentLoadout))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle ShieldSlot =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId);
	URpgInventoryItemInstance* SourceStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackableOffHandItemDefinition::StaticClass(),
			4,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("A four-item OffHand stack starts in Content"), SourceStack))
	{
		return false;
	}
	TestEqual(
		TEXT("Content-only equipment data contributes no carried load"),
		EquipmentLoadout->GetEquipmentLoadWeight(),
		0.0f);

	TArray<FRpgInventoryActionFeedbackMessage> FeedbackMessages;
	TArray<FRpgEquipmentLoadoutSlotsChangedMessage> EquipmentMessages;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	FGameplayMessageListenerHandle FeedbackHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryActionFeedbackMessage>(
			RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
			[&FeedbackMessages](
				FGameplayTag,
				const FRpgInventoryActionFeedbackMessage& Message)
			{
				FeedbackMessages.Add(Message);
			});
	FGameplayMessageListenerHandle EquipmentHandle =
		MessageSubsystem.RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
			RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged,
			[&EquipmentMessages](
				FGameplayTag,
				const FRpgEquipmentLoadoutSlotsChangedMessage& Message)
			{
				EquipmentMessages.Add(Message);
			});

	const FGuid SplitRequestId = FGuid::NewGuid();
	const FRpgInventoryGridPlacement ShieldPlacement =
		MakePlacement(ShieldSlot, 0, 0);
	FRpgInventoryEntryView SourceBeforeSplit;
	if (!TestTrue(
			TEXT("The split source has an exact entry snapshot"),
			GetEntryView(
				Inventory,
				SourceStack->GetItemId(),
				SourceBeforeSplit)))
	{
		FeedbackHandle.Unregister();
		EquipmentHandle.Unregister();
		return false;
	}
	FRpgInventorySplitRequest SplitRequest;
	SplitRequest.RequestId = SplitRequestId;
	SplitRequest.ItemId = SourceBeforeSplit.ItemId;
	SplitRequest.ExpectedEntryId = SourceBeforeSplit.EntryId;
	SplitRequest.ExpectedSourcePlacement =
		SourceBeforeSplit.Placement;
	SplitRequest.ExpectedSourceQuantity =
		SourceBeforeSplit.StackCount;
	SplitRequest.TargetPlacement = ShieldPlacement;
	SplitRequest.SplitCount = 1;
	UiActions->RequestSplitItemStackById(
		Inventory,
		SplitRequest);
	if (!TestTrue(TEXT("The split emits action feedback"), FeedbackMessages.Num() > 0))
	{
		FeedbackHandle.Unregister();
		EquipmentHandle.Unregister();
		return false;
	}
	TestEqual(
		TEXT("Split feedback reports success"),
		FeedbackMessages.Last().Result,
		ERpgInventoryActionFeedbackResult::Success);
	TestEqual(
		TEXT("Split feedback preserves request correlation"),
		FeedbackMessages.Last().RequestId,
		SplitRequestId);
	const FRpgInventoryActionFeedbackMessage OriginalSplitFeedback =
		FeedbackMessages.Last();
	TestEqual(
		TEXT("The source stack retains the unsplit quantity"),
		Inventory->GetItemStackCount(SourceStack),
		3);

	URpgInventoryItemInstance* CarryStack =
		Inventory->GetItemAtContainerCell(ShieldSlot, 0, 0);
	if (!TestNotNull(TEXT("The split creates a concrete Carry stack"), CarryStack))
	{
		FeedbackHandle.Unregister();
		EquipmentHandle.Unregister();
		return false;
	}
	TestNotEqual(
		TEXT("The Carry split owns a distinct persistent item identity"),
		CarryStack->GetItemId(),
		SourceStack->GetItemId());
	TestEqual(
		TEXT("The Carry split contains exactly one item"),
		Inventory->GetItemStackCount(CarryStack),
		1);
	TestEqual(
		TEXT("A successful split into Carry refreshes derived equipment load"),
		EquipmentLoadout->GetEquipmentLoadWeight(),
		4.0f);
	TestNull(
		TEXT("Physical Carry placement does not implicitly activate OffHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::OffHand));
	TestTrue(
		TEXT("The committed Carry delta emits equipment-derived refresh"),
		EquipmentMessages.Num() > 0);

	const FString SignatureBeforeReplay = MakeInventorySignature(Inventory);
	const int32 EquipmentMessageCountBeforeReplay = EquipmentMessages.Num();
	const int32 FeedbackCountBeforeReplay = FeedbackMessages.Num();
	UiActions->RequestSplitItemStackById(
		Inventory,
		SplitRequest);
	TestEqual(
		TEXT("An identical split retry inside the bounded replay window emits one feedback"),
		FeedbackMessages.Num(),
		FeedbackCountBeforeReplay + 1);
	TestEqual(
		TEXT("The split retry replays success"),
		FeedbackMessages.Last().Result,
		ERpgInventoryActionFeedbackResult::Success);
	TestEqual(
		TEXT("The split retry replays the exact affected count"),
		FeedbackMessages.Last().StackCount,
		OriginalSplitFeedback.StackCount);
	TestEqual(
		TEXT("The split retry replays the exact action tag"),
		FeedbackMessages.Last().ActionTag,
		OriginalSplitFeedback.ActionTag);
	TestTrue(
		TEXT("The split retry replays the exact stable item identity"),
		FeedbackMessages.Last().ItemId ==
			OriginalSplitFeedback.ItemId);
	TestEqual(
		TEXT("The split retry replays the exact inventory context"),
		FeedbackMessages.Last().InventoryOwner.Get(),
		OriginalSplitFeedback.InventoryOwner.Get());
	TestEqual(
		TEXT("The split retry replays the exact authorized item context"),
		FeedbackMessages.Last().Item.Get(),
		OriginalSplitFeedback.Item.Get());
	TestEqual(
		TEXT("The split retry cannot mutate inventory state twice"),
		MakeInventorySignature(Inventory),
		SignatureBeforeReplay);
	TestEqual(
		TEXT("The split retry cannot repeat equipment-derived side effects"),
		EquipmentMessages.Num(),
		EquipmentMessageCountBeforeReplay);
	TestEqual(
		TEXT("The split retry preserves the derived load"),
		EquipmentLoadout->GetEquipmentLoadWeight(),
		4.0f);

	FRpgInventorySplitRequest PayloadCollision = SplitRequest;
	PayloadCollision.SplitCount = 2;
	const int32 FeedbackCountBeforeCollision = FeedbackMessages.Num();
	UiActions->RequestSplitItemStackById(
		Inventory,
		PayloadCollision);
	TestEqual(
		TEXT("A split RequestId payload collision emits one rejection"),
		FeedbackMessages.Num(),
		FeedbackCountBeforeCollision + 1);
	TestEqual(
		TEXT("A split RequestId payload collision is InvalidRequest"),
		FeedbackMessages.Last().Result,
		ERpgInventoryActionFeedbackResult::InvalidRequest);
	TestEqual(
		TEXT("A split RequestId payload collision cannot mutate inventory"),
		MakeInventorySignature(Inventory),
		SignatureBeforeReplay);
	TestEqual(
		TEXT("A split RequestId payload collision cannot repeat equipment side effects"),
		EquipmentMessages.Num(),
		EquipmentMessageCountBeforeReplay);

	FRpgInventorySplitRequest StaleSnapshotRequest = SplitRequest;
	StaleSnapshotRequest.RequestId = FGuid::NewGuid();
	const int32 FeedbackCountBeforeStaleSnapshot =
		FeedbackMessages.Num();
	UiActions->RequestSplitItemStackById(
		Inventory,
		StaleSnapshotRequest);
	TestEqual(
		TEXT("A stale split snapshot emits one rejection"),
		FeedbackMessages.Num(),
		FeedbackCountBeforeStaleSnapshot + 1);
	TestEqual(
		TEXT("A stale split snapshot is InvalidRequest"),
		FeedbackMessages.Last().Result,
		ERpgInventoryActionFeedbackResult::InvalidRequest);
	TestEqual(
		TEXT("A stale split snapshot cannot mutate inventory"),
		MakeInventorySignature(Inventory),
		SignatureBeforeReplay);
	TestEqual(
		TEXT("A stale split snapshot cannot repeat equipment side effects"),
		EquipmentMessages.Num(),
		EquipmentMessageCountBeforeReplay);

	FeedbackHandle.Unregister();
	EquipmentHandle.Unregister();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryEquipmentMirrorCannotMovePhysicalItemTest,
	"SurvivalRpg.Inventory.Intent.Equip.LoadoutMirrorCannotMovePhysicalItem",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryEquipmentMirrorCannotMovePhysicalItemTest::RunTest(
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
		TEXT("EquipmentMirrorBoundaryController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<
			ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);
	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("EquipmentMirrorBoundaryPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<
			ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The mirror-boundary controller exists"), Controller) ||
		!TestNotNull(TEXT("The mirror-boundary player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		Controller->GetEquipmentLoadoutComponent();
	if (!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The typed UI gateway exists"), UiActions) ||
		!TestNotNull(TEXT("The loadout mirror exists"), EquipmentLoadout))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle BackpackSlot =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::
				GearBackpackGroupId);
	URpgInventoryItemInstance* Backpack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("A backpack starts in Pockets"), Backpack))
	{
		return false;
	}

	TestTrue(
		TEXT("Reconciling while the backpack remains in Pockets succeeds"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	FRpgInventoryEntryView InitialEntry;
	TestTrue(
		TEXT("Pockets reconciliation keeps the item addressable"),
		GetEntryView(
			Inventory,
			Backpack->GetItemId(),
			InitialEntry));
	TestEqual(
		TEXT("Reconciliation never moves a Pockets item into Gear"),
		InitialEntry.Placement.GetContainerHandle(),
		Pockets);
	TestNull(
		TEXT("Only physical Gear truth may create a Backpack mirror entry"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Backpack));

	FRpgInventoryEquipmentIntent EquipIntent =
		MakeEquipmentIntent(
			Inventory,
			Backpack,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::Backpack);
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		EquipIntent);
	FRpgInventoryEntryView EquippedEntry;
	TestTrue(
		TEXT("The typed equip keeps the backpack addressable"),
		GetEntryView(
			Inventory,
			Backpack->GetItemId(),
			EquippedEntry));
	TestEqual(
		TEXT("The typed inventory transaction owns Gear.Backpack placement"),
		EquippedEntry.Placement.GetContainerHandle(),
		BackpackSlot);
	TestEqual(
		TEXT("One reconciliation mirrors the physical backpack"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Backpack),
		Backpack);

	const FRpgInventoryDragPayload EquipmentPayload =
		URpgInventoryDragDropCoordinator::MakeEquipmentPayload(
			Backpack,
			ERpgEquipmentSlot::Backpack);
	FRpgInventoryDragPayload MissingSnapshotPayload;
	MissingSnapshotPayload.SourceType =
		ERpgInventoryDragSourceType::EquipmentSlot;
	MissingSnapshotPayload.ItemInstance = Backpack;
	MissingSnapshotPayload.EquipmentSlot =
		ERpgEquipmentSlot::Backpack;
	TestFalse(
		TEXT("An equipment drag without an exact entry snapshot is invalid"),
		URpgInventoryDragDropCoordinator::IsPayloadValid(
			MissingSnapshotPayload));
	TestEqual(
		TEXT("An equipment-origin drag captures its owning inventory"),
		EquipmentPayload.SourceInventory.Get(),
		Inventory);
	TestEqual(
		TEXT("An equipment-origin drag captures the exact entry identity"),
		EquipmentPayload.EntryId,
		EquippedEntry.EntryId);
	TestEqual(
		TEXT("An equipment-origin drag captures the exact physical placement"),
		EquipmentPayload.SourcePlacement,
		EquippedEntry.Placement);
	TestEqual(
		TEXT("An equipment-origin drag captures the complete quantity"),
		EquipmentPayload.StackCount,
		EquippedEntry.StackCount);

	TestTrue(
		TEXT("Repeated reconciliation accepts the unchanged physical backpack"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	TestEqual(
		TEXT("Repeated reconciliation preserves the physical Backpack mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Backpack),
		Backpack);

	FRpgInventoryEquipmentIntent UnequipIntent =
		MakeEquipmentIntent(
			Inventory,
			Backpack,
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent);
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		UnequipIntent);
	FRpgInventoryEntryView UnequippedEntry;
	TestTrue(
		TEXT("The typed unequip keeps the backpack addressable"),
		GetEntryView(
			Inventory,
			Backpack->GetItemId(),
			UnequippedEntry));
	TestEqual(
		TEXT("The typed unequip returns the backpack to Content"),
		UnequippedEntry.Placement.GetContainerHandle(),
		Pockets);
	TestNull(
		TEXT("Post-commit reconciliation clears the stale mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Backpack));

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(
			Controller,
			Controller);
	if (!TestNotNull(
			TEXT("A coordinator exists for stale equipment-payload validation"),
			Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);
	FRpgInventoryDropTarget StalePayloadTarget;
	StalePayloadTarget.TargetType =
		ERpgInventoryDropTargetType::InventorySlot;
	StalePayloadTarget.TargetInventory = Inventory;
	StalePayloadTarget.TargetPlacement = MakePlacement(Pockets, 2, 0);
	TestFalse(
		TEXT("An equipment payload becomes stale after the physical item moves"),
		Coordinator->PreviewPayloadDrop(
			EquipmentPayload,
			StalePayloadTarget));
	TestFalse(
		TEXT("A stale equipment payload cannot dispatch a mutation"),
		Coordinator->CommitPayloadToTarget(
			EquipmentPayload,
			StalePayloadTarget));

	URpgInventoryItemInstance* Armor =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestHeavyItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Pockets, 1, 0));
	if (!TestNotNull(
			TEXT("Armor starts in normal Content"),
			Armor))
	{
		return false;
	}

	TestTrue(
		TEXT("Reconciling Content armor succeeds without treating it as equipped"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	TestNull(
		TEXT("Content armor creates no Chest mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest));

	const FRpgInventoryContainerHandle GearChest =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::GearChestGroupId);
	if (!TestTrue(
			TEXT("The trusted inventory seam moves armor into physical Gear.Chest"),
			MoveWholeEntryToEquipmentPlacement(
				Inventory,
				Armor,
				MakePlacement(GearChest, 0, 0))))
	{
		return false;
	}
	FRpgInventoryEntryView EquippedArmorEntry;
	TestTrue(
		TEXT("The physical armor move keeps the instance addressable"),
		GetEntryView(
			Inventory,
			Armor->GetItemId(),
			EquippedArmorEntry));
	TestEqual(
		TEXT("The inventory graph owns Gear.Chest placement"),
		EquippedArmorEntry.Placement.GetContainerHandle(),
		GearChest);
	TestNull(
		TEXT("A physical manager move cannot write the loadout mirror as a side effect"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest));
	TestTrue(
		TEXT("Reconciliation imports the complete physical Gear snapshot"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	TestEqual(
		TEXT("Reconciliation mirrors the physical Chest item"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest),
		Armor);

	TestTrue(
		TEXT("Repeated reconciliation accepts unchanged Gear.Chest truth"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	TestEqual(
		TEXT("Repeated reconciliation preserves the Chest mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest),
		Armor);

	URpgPlayerInventoryLayoutDefinition* MutableLayoutDefinition =
		PlayerState->GetMutableTestInventoryLayoutDefinition();
	if (!TestNotNull(
			TEXT("The mirror regression owns a mutable transient layout definition"),
			MutableLayoutDefinition))
	{
		return false;
	}
	const int32 ChestDefinitionIndex =
		MutableLayoutDefinition->StaticSlotGroups.IndexOfByPredicate(
			[](const FRpgInventorySlotGroupDefinition& Group)
			{
				return Group.GroupKind ==
						ERpgInventorySlotGroupKind::Gear &&
					Group.EquipmentSlotRole ==
						ERpgEquipmentSlot::Chest;
			});
	if (!TestTrue(
			TEXT("The fixture exposes its required typed Chest definition"),
			MutableLayoutDefinition->StaticSlotGroups.IsValidIndex(
				ChestDefinitionIndex)))
	{
		return false;
	}
	const FRpgInventorySlotGroupDefinition SavedChestDefinition =
		MutableLayoutDefinition->StaticSlotGroups[ChestDefinitionIndex];
	MutableLayoutDefinition->StaticSlotGroups.RemoveAt(
		ChestDefinitionIndex);
	TestFalse(
		TEXT("Physical reconciliation fails when a required typed Gear address is missing"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	TestEqual(
		TEXT("Failed structural preflight preserves the existing Chest mirror atomically"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest),
		Armor);
	MutableLayoutDefinition->StaticSlotGroups.Insert(
		SavedChestDefinition,
		ChestDefinitionIndex);
	TestTrue(
		TEXT("Restoring the required Chest definition makes reconciliation valid again"),
		EquipmentLoadout->ReconcilePhysicalEquipmentFromInventory());
	TestEqual(
		TEXT("Successful reconciliation still preserves the physical Chest mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest),
		Armor);

	FRpgInventoryEquipmentIntent UnequipArmorIntent =
		MakeEquipmentIntent(
			Inventory,
			Armor,
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent);
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		UnequipArmorIntent);
	TestNull(
		TEXT("Typed Chest unequip clears the mirror after the physical move"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest));
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
	URpgInventoryItemInstance* DefinitionlessContainer =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestGearNameCollisionBagItemDefinition::
				StaticClass(),
			1,
			MakePlacement(Pockets, 1, 0));
	if (!TestNotNull(TEXT("The ItemContainer backpack starts in Pockets"), Backpack) ||
		!TestNotNull(
			TEXT("The portable definitionless container starts in Pockets"),
			DefinitionlessContainer))
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
			Layout->TryMakeGearSlotAddress(
				ERpgEquipmentSlot::Backpack,
				BackpackAddress)) ||
		!TestTrue(
			TEXT("The layout resolves the physical Pouch address"),
			Layout->TryMakeGearSlotAddress(
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
	TestFalse(
		TEXT("A definitionless ItemContainer is not an implicit Backpack provider"),
		FRpgInventoryEquipmentPlacementPolicy::CanItemUseEquipmentSlot(
			DefinitionlessContainer,
			ERpgEquipmentSlot::Backpack));
	TestFalse(
		TEXT("The layout rejects the same definitionless Backpack provider"),
		Layout->CanItemUseSlotAddress(
			DefinitionlessContainer,
			BackpackAddress));
	const FRpgInventoryContainerHandle BackpackSlot =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::
				GearBackpackGroupId);
	const FRpgInventoryMutationRequest
		DefinitionlessBackpackRequest = MakePlacementRequest(
			ERpgInventoryMutationOperation::Equip,
			DefinitionlessContainer,
			Pockets,
			BackpackSlot,
			0,
			0);
	const FRpgInventoryMutationResult
		DefinitionlessBackpackPlan =
			Inventory->PlanInventoryMutation(
				DefinitionlessBackpackRequest);
	TestFalse(
		TEXT("The authoritative planner rejects a definitionless Backpack provider"),
		DefinitionlessBackpackPlan.IsSuccess());
	TestEqual(
		TEXT("The definitionless provider reports the shared policy failure"),
		DefinitionlessBackpackPlan.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	const FRpgInventoryMutationResult
		DefinitionlessBackpackExecute =
			Inventory->ExecuteInventoryMutation(
				DefinitionlessBackpackRequest);
	TestFalse(
		TEXT("The authoritative commit rejects a definitionless Backpack provider"),
		DefinitionlessBackpackExecute.IsSuccess());
	TestEqual(
		TEXT("The rejected definitionless provider preserves the policy result"),
		DefinitionlessBackpackExecute.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		MakeEquipmentIntent(
			Inventory,
			DefinitionlessContainer,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::Backpack));
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		MakeEquipmentIntent(
			Inventory,
			DefinitionlessContainer,
			ERpgInventoryEquipmentIntentOperation::
				EquipDefaultAndActivate));
	FRpgInventoryEntryView DefinitionlessContainerEntry;
	TestTrue(
		TEXT("The rejected definitionless container remains addressable"),
		GetEntryView(
			Inventory,
			DefinitionlessContainer->GetItemId(),
			DefinitionlessContainerEntry));
	TestEqual(
		TEXT("Planner, commit, explicit-slot, and default-equip actions leave the definitionless container in Pockets"),
		DefinitionlessContainerEntry.Placement.GetContainerHandle(),
		Pockets);
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
	FRpgInventoryEquipmentIntent BackpackIntent =
		MakeEquipmentIntent(
			Inventory,
			Backpack,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::Backpack);
	FRpgInventoryGridPlacement PlannedBackpackPlacement;
	const FRpgInventoryPlacementPlan BackpackPlacementPlan =
		UiActions->PlanEquipmentIntentPlacement(
			Inventory,
			BackpackIntent,
			PlannedBackpackPlacement);
	TestTrue(
		TEXT("The abstract Backpack target resolves to a complete concrete plan"),
		BackpackPlacementPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("The Backpack plan resolves the authored physical Gear container"),
		PlannedBackpackPlacement.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::GearBackpackGroupId));
	FRpgInventoryEquipmentIntent PouchIntent = BackpackIntent;
	PouchIntent.TargetEquipmentSlot = ERpgEquipmentSlot::Pouch;
	FRpgInventoryGridPlacement RejectedPouchPlacement;
	const FRpgInventoryPlacementPlan RejectedPouchPlacementPlan =
		UiActions->PlanEquipmentIntentPlacement(
			Inventory,
			PouchIntent,
			RejectedPouchPlacement);
	TestEqual(
		TEXT("The public equipment planner preserves the unauthored-slot reason"),
		RejectedPouchPlacementPlan.Code,
		ERpgInventoryMutationResultCode::ItemNotAllowed);
	const FRpgInventoryInteractionPreviewPlan BackpackInteractionPlan =
		Coordinator->PlanInteractionPreview(Payload, BackpackTarget);
	TestTrue(
		TEXT("The Gear hover consumes the complete equipment placement plan"),
		BackpackInteractionPlan.bUsesPlacementPlan &&
			BackpackInteractionPlan.PlacementPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("The normalized Gear target shown by the UI matches the equipment plan"),
		BackpackInteractionPlan.ResolvedTargetPlacement,
		PlannedBackpackPlacement);
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

	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		MakeEquipmentIntent(
			Inventory,
			Backpack,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::Backpack));
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

	FRpgInventoryEquipmentIntent StaleUnequipIntent =
		MakeEquipmentIntent(
			Inventory,
			Backpack,
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent);
	StaleUnequipIntent.ItemId = FRpgInventoryItemId::NewId();
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		StaleUnequipIntent);
	FRpgInventoryEntryView StaleUnequipBackpackEntry;
	TestTrue(
		TEXT("The provider remains addressable after a stale unequip request"),
		GetEntryView(Inventory, Backpack->GetItemId(), StaleUnequipBackpackEntry));
	TestEqual(
		TEXT("A stale expected item id cannot remove the current provider"),
		StaleUnequipBackpackEntry.Placement.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::GearBackpackGroupId));

	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		MakeEquipmentIntent(
			Inventory,
			Backpack,
			ERpgInventoryEquipmentIntentOperation::
				UnequipToContent));
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

	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		MakeEquipmentIntent(
			Inventory,
			Backpack,
			ERpgInventoryEquipmentIntentOperation::
				EquipDefaultAndActivate));
	FRpgInventoryEntryView DefaultReEquippedBackpackEntry;
	TestTrue(
		TEXT("The default-equip provider remains addressable"),
		GetEntryView(
			Inventory,
			Backpack->GetItemId(),
			DefaultReEquippedBackpackEntry));
	TestEqual(
		TEXT("Default equipment destination uses the authored Backpack slot"),
		DefaultReEquippedBackpackEntry.Placement.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::
				GearBackpackGroupId));
	TestEqual(
		TEXT("Default provider equip reconciles the Backpack loadout mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Backpack),
		Backpack);
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
	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		MakeEquipmentIntent(
			Inventory,
			Weapon,
			ERpgInventoryEquipmentIntentOperation::
				EquipDefaultAndActivate));

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
	TestTrue(
		TEXT("The binding follows the primary Carry semantic role"),
		AppliedSlot.CarrySemanticRole ==
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);
	TestEqual(
		TEXT("The canonical Carry address is retained for presentation and validation"),
		AppliedSlot.SlotAddress.GetContainerHandle(),
		WeaponSlot1);
	TestFalse(
		TEXT("The acknowledged binding releases the held drag ghost"),
		Coordinator->GetInteractionSession()->HasPayload());

	URpgPlayerInventoryLayoutDefinition* TestLayoutDefinition =
		PlayerState->GetMutableTestInventoryLayoutDefinition();
	if (!TestNotNull(
		TEXT("The fixture owns a mutable transient layout definition"),
		TestLayoutDefinition))
	{
		return false;
	}

	const FName CustomCarryContainerId(TEXT("DesignerCarrySlot"));
	FRpgInventorySlotGroupDefinition& CustomCarryGroup =
		TestLayoutDefinition->StaticSlotGroups.AddDefaulted_GetRef();
	CustomCarryGroup.ContainerId = CustomCarryContainerId;
	CustomCarryGroup.SemanticRole =
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Utility;
	CustomCarryGroup.DisplayName = FText::FromString(TEXT("Utility Carry"));
	CustomCarryGroup.GroupKind = ERpgInventorySlotGroupKind::Carry;
	CustomCarryGroup.GridSize.Width = 1;
	CustomCarryGroup.GridSize.Height = 1;
	CustomCarryGroup.Rule.bActionbarBindable = true;
	CustomCarryGroup.EquipmentSlotRole = ERpgEquipmentSlot::MainHand;

	const FRpgInventoryContainerHandle CustomCarryHandle =
		FRpgInventoryContainerHandle::MakeRoot(CustomCarryContainerId);
	FRpgInventorySlotAddress CustomCarryAddress;
	CustomCarryAddress.SetContainerHandle(CustomCarryHandle);
	CustomCarryAddress.X = 0;
	CustomCarryAddress.Y = 0;
	TestTrue(
		TEXT("The custom Carry root remains independent from built-in physical ids"),
		CustomCarryContainerId != URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId &&
		CustomCarryContainerId != URpgPlayerInventoryLayoutComponent::WeaponSlot2GroupId &&
		CustomCarryContainerId != URpgPlayerInventoryLayoutComponent::ShieldSlotGroupId);
	TestTrue(
		TEXT("The custom root address remains classified as Carry from authored data"),
		Layout->IsCarrySlotAddress(CustomCarryAddress));
	TestTrue(
		TEXT("The custom root address remains actionbar-bindable"),
		Layout->IsSlotAddressActionbarBindable(CustomCarryAddress));
	TestTrue(
		TEXT("The custom Carry structure and item pass the detailed actionbar predicate"),
		Layout->CanBindSlotAddressToActionbar(
			CustomCarryAddress,
			Weapon));

	CustomCarryGroup.SemanticRole = FGameplayTag();
	TestFalse(
		TEXT("A Carry group without a stable semantic role is not advertised as actionbar-bindable"),
		Layout->IsSlotAddressActionbarBindable(CustomCarryAddress));
	TestFalse(
		TEXT("The detailed Carry binding predicate rejects the same missing semantic role"),
		Layout->CanBindSlotAddressToActionbar(
			CustomCarryAddress,
			Weapon));
	const FGameplayTag ForeignSemanticRole =
		FGameplayTag::RequestGameplayTag(TEXT("UI.Screen.Inventory"));
	CustomCarryGroup.SemanticRole = ForeignSemanticRole;
	TestFalse(
		TEXT("A Carry role outside the layout namespace is not advertised as actionbar-bindable"),
		Layout->IsSlotAddressActionbarBindable(CustomCarryAddress));
	TestFalse(
		TEXT("The detailed Carry binding predicate rejects the same foreign semantic role"),
		Layout->CanBindSlotAddressToActionbar(
			CustomCarryAddress,
			Weapon));
	FRpgInventorySlotGroupView ForeignSemanticGroup;
	TestFalse(
		TEXT("The runtime semantic resolver fails closed for roles outside the layout namespace"),
		Layout->TryGetSlotGroupBySemanticRole(
			ForeignSemanticRole,
			ForeignSemanticGroup));
	CustomCarryGroup.SemanticRole =
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Utility;
	TestTrue(
		TEXT("The authority path binds a custom canonical Carry address"),
		ActionBar->TryBindCarrySlotToSlotAuthority(1, CustomCarryAddress));

	const FRpgActionBarSlot CustomAppliedSlot = ActionBar->GetSlot(1);
	TestEqual(
		TEXT("The custom binding retains Carry semantics"),
		CustomAppliedSlot.SlotType,
		ERpgActionBarSlotType::CarrySlot);
	TestTrue(
		TEXT("The custom binding follows the designer-defined role"),
		CustomAppliedSlot.CarrySemanticRole ==
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Utility);
	TestEqual(
		TEXT("The custom binding retains its complete canonical root handle"),
		CustomAppliedSlot.SlotAddress.GetContainerHandle(),
		CustomCarryHandle);

	URpgPlayerInventoryViewModel* AggregatePlayerViewModel =
		NewObject<URpgPlayerInventoryViewModel>(Controller);
	AggregatePlayerViewModel->BindPlayerController(Controller);
	const TArray<URpgActionBarSlotViewModel*> AggregateActionBarSlots =
		AggregatePlayerViewModel->GetActionBarSlots();
	if (TestTrue(
		TEXT("The aggregate PlayerInventory VM exposes the custom Carry binding"),
		AggregateActionBarSlots.IsValidIndex(1) &&
			AggregateActionBarSlots[1] != nullptr))
	{
		TestEqual(
			TEXT("The aggregate PlayerInventory VM preserves the authored Carry display name"),
			AggregateActionBarSlots[1]->GetShortDisplayName().ToString(),
			FString(TEXT("Utility Carry")));
	}

	CustomCarryGroup.GridSize.Width = 2;
	AddExpectedError(
		TEXT("has an invalid GroupKind, EquipmentSlotRole, or grid-size contract"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(
		TEXT("A semantic Carry role fails closed when a designer authors a multi-cell group"),
		ActionBar->TryBindCarrySlotToSlotAuthority(2, CustomCarryAddress));
	CustomCarryGroup.GridSize.Width = 1;

	const FName RenamedCustomCarryContainerId(TEXT("RenamedDesignerCarrySlot"));
	CustomCarryGroup.ContainerId = RenamedCustomCarryContainerId;
	ActionBar->RefreshBindings();
	const FRpgActionBarSlot RenamedCustomAppliedSlot = ActionBar->GetSlot(1);
	TestTrue(
		TEXT("Renaming a physical Carry container does not change the saved semantic binding"),
		RenamedCustomAppliedSlot.CarrySemanticRole ==
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Utility);
	TestEqual(
		TEXT("Role revalidation follows the renamed Carry container's current address"),
		RenamedCustomAppliedSlot.SlotAddress.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(RenamedCustomCarryContainerId));

	CustomCarryGroup.SemanticRole =
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary;
	TestFalse(
		TEXT("A duplicate Carry semantic role is not advertised as actionbar-bindable"),
		Layout->IsSlotAddressActionbarBindable(
			CustomCarryAddress));
	TestFalse(
		TEXT("The detailed binding predicate rejects the same duplicate Carry role"),
		Layout->CanBindSlotAddressToActionbar(
			CustomCarryAddress,
			Weapon));
	AddExpectedError(
		TEXT("is not a unique static root"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	FRpgInventorySlotGroupView DuplicateRoleGroup;
	TestFalse(
		TEXT("Duplicate semantic roles fail closed instead of selecting the first group"),
		Layout->TryGetSlotGroupBySemanticRole(
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary,
			DuplicateRoleGroup));
	CustomCarryGroup.SemanticRole =
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Utility;
	FRpgInventorySlotGroupDefinition* GearGroup =
		TestLayoutDefinition->StaticSlotGroups.FindByPredicate(
			[](const FRpgInventorySlotGroupDefinition& Group)
			{
				return Group.GroupKind ==
					ERpgInventorySlotGroupKind::Gear;
			});
	if (!TestNotNull(
		TEXT("The fixture exposes a Gear definition for global role-uniqueness coverage"),
		GearGroup))
	{
		return false;
	}
	GearGroup->SemanticRole =
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary;
	AddExpectedError(
		TEXT("is not a unique static root"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestNull(
		TEXT("The aggregate VM cannot hide a Gear/Carry role collision by omitting Gear groups"),
		AggregatePlayerViewModel->GetSlotGroupBySemanticRole(
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary));
	GearGroup->SemanticRole = FGameplayTag();

	FNameProperty* LegacyCarryRoleProperty = FindFProperty<FNameProperty>(
		FRpgQuickAccessBinding::StaticStruct(),
		TEXT("CarryRole"));
	if (!TestNotNull(
		TEXT("The version-one CarryRole field remains reflected for save migration"),
		LegacyCarryRoleProperty))
	{
		return false;
	}
	TestTrue(
		TEXT("The historical CarryRole name remains available to tagged SaveGame loading"),
		LegacyCarryRoleProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(
		TEXT("The legacy CarryRole shadow is marked deprecated"),
		LegacyCarryRoleProperty->HasAnyPropertyFlags(CPF_Deprecated));
	TestTrue(
		TEXT("The legacy CarryRole shadow is excluded from owner replication"),
		LegacyCarryRoleProperty->HasAnyPropertyFlags(CPF_RepSkip));

	FRpgQuickAccessBindingV1TaggedFixture HistoricalBinding;
	HistoricalBinding.SlotType = ERpgActionBarSlotType::CarrySlot;
	HistoricalBinding.SlotAddress = WeaponSlot1Address;
	HistoricalBinding.CarryRole =
		URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId;
	TArray<uint8> HistoricalTaggedBytes;
	{
		FMemoryWriter Writer(HistoricalTaggedBytes, true);
		FBinaryArchiveFormatter Formatter(Writer);
		FStructuredArchive Archive(Formatter);
		FRpgQuickAccessBindingV1TaggedFixture::StaticStruct()->
			SerializeTaggedProperties(
				Archive.Open(),
				reinterpret_cast<uint8*>(&HistoricalBinding),
				nullptr,
				nullptr);
	}
	FRpgQuickAccessBinding LoadedHistoricalBinding;
	{
		FMemoryReader Reader(HistoricalTaggedBytes, true);
		FBinaryArchiveFormatter Formatter(Reader);
		FStructuredArchive Archive(Formatter);
		FRpgQuickAccessBinding::StaticStruct()->SerializeTaggedProperties(
			Archive.Open(),
			reinterpret_cast<uint8*>(&LoadedHistoricalBinding),
			nullptr,
			nullptr);
	}
	TestEqual(
		TEXT("Tagged version-one CarryRole bytes load into the reflected migration shadow"),
		LegacyCarryRoleProperty->GetPropertyValue_InContainer(
			&LoadedHistoricalBinding),
		URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);

	TArray<FRpgQuickAccessBinding> LegacyBindings;
	LegacyBindings.SetNum(8);
	LegacyBindings[0] = LoadedHistoricalBinding;
	ActionBar->RestoreQuickAccessBindings(LegacyBindings, true);
	const FRpgQuickAccessBinding MigratedCarryBinding =
		ActionBar->GetQuickAccessBindings()[0];
	TestTrue(
		TEXT("A version-one Carry root migrates through the active layout's authored role"),
		MigratedCarryBinding.CarrySemanticRole ==
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);
	TestTrue(
		TEXT("Successful Carry-role promotion clears the legacy save shadow"),
		LegacyCarryRoleProperty->GetPropertyValue_InContainer(
			&MigratedCarryBinding).IsNone());

	FRpgInventorySlotGroupDefinition* PrimaryCarryGroup =
		TestLayoutDefinition->StaticSlotGroups.FindByPredicate(
			[](const FRpgInventorySlotGroupDefinition& Group)
			{
				return Group.SemanticRole ==
					RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary;
			});
	if (!TestNotNull(
		TEXT("The fixture exposes its primary Carry definition for migration regression coverage"),
		PrimaryCarryGroup))
	{
		return false;
	}
	const FName OriginalPrimaryContainerId = PrimaryCarryGroup->ContainerId;
	const FName RenamedPrimaryContainerId(TEXT("RenamedPrimaryCarry"));
	PrimaryCarryGroup->ContainerId = RenamedPrimaryContainerId;
	LegacyBindings[0] = LoadedHistoricalBinding;
	ActionBar->RestoreQuickAccessBindings(LegacyBindings, true);
	const FRpgQuickAccessBinding RenamedLegacyBinding =
		ActionBar->GetQuickAccessBindings()[0];
	TestTrue(
		TEXT("A skipped-version v1 binding migrates through the historical id alias after a physical rename"),
		RenamedLegacyBinding.CarrySemanticRole ==
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);
	TestEqual(
		TEXT("The migrated v1 binding immediately adopts the renamed role address"),
		RenamedLegacyBinding.SlotAddress.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(RenamedPrimaryContainerId));
	PrimaryCarryGroup->ContainerId = OriginalPrimaryContainerId;
	ActionBar->RefreshBindings();
	TestTrue(
		TEXT("The restored primary Carry binding is available before policy is disabled"),
		ActionBar->GetSlot(0).bAvailable);
	TestTrue(
		TEXT("The activation regression starts with empty runtime hands"),
		EquipmentLoadout->ClearActiveHands());
	PrimaryCarryGroup->Rule.bActionbarBindable = false;
	ActionBar->RefreshBindings();
	TestFalse(
		TEXT("Disabling the authored actionbar rule blocks the bound Carry slot"),
		ActionBar->GetSlot(0).bAvailable);
	UiActions->RequestActivateCarrySlot(
		0,
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);
	TestNull(
		TEXT("The server gateway cannot activate an authored non-bindable Carry role"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand));
	PrimaryCarryGroup->Rule.bActionbarBindable = true;
	ActionBar->RefreshBindings();
	UiActions->RequestActivateCarrySlot(
		0,
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Secondary);
	TestNull(
		TEXT("A forged semantic role cannot activate another bound Carry slot"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand));
	UiActions->RequestActivateCarrySlot(
		0,
		RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);
	TestEqual(
		TEXT("The exact available server binding still activates through the canonical gateway"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand),
		Weapon);
	EquipmentLoadout->ClearActiveHands();

	TArray<FRpgQuickAccessBinding> IncompleteCurrentBindings;
	IncompleteCurrentBindings.SetNum(8);
	IncompleteCurrentBindings[0].SlotType =
		ERpgActionBarSlotType::CarrySlot;
	IncompleteCurrentBindings[0].SlotAddress = WeaponSlot1Address;
	ActionBar->RestoreQuickAccessBindings(
		IncompleteCurrentBindings,
		false);
	TestTrue(
		TEXT("Schema v2 cannot infer a missing semantic role from its physical address"),
		ActionBar->GetQuickAccessBindings()[0].IsEmpty());

	TArray<FRpgQuickAccessBinding> UnknownLegacyBindings;
	UnknownLegacyBindings.SetNum(8);
	FRpgQuickAccessBinding& UnknownLegacyBinding =
		UnknownLegacyBindings[0];
	UnknownLegacyBinding.SlotType = ERpgActionBarSlotType::CarrySlot;
	UnknownLegacyBinding.SlotAddress.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeRoot(TEXT("UnknownLegacyCarry")));
	UnknownLegacyBinding.SlotAddress.X = 0;
	UnknownLegacyBinding.SlotAddress.Y = 0;
	LegacyCarryRoleProperty->SetPropertyValue_InContainer(
		&UnknownLegacyBinding,
		FName(TEXT("UnknownLegacyCarry")));
	ActionBar->RestoreQuickAccessBindings(
		UnknownLegacyBindings,
		true);
	TestTrue(
		TEXT("An unmappable v1 Carry root is reset immediately instead of leaking an unsavable shadow"),
		ActionBar->GetQuickAccessBindings()[0].IsEmpty());
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
	const TArray<FRpgInventorySlotGroupView> SlotGroups = Layout->GetSlotGroups();
	const FRpgInventorySlotGroupView* StaticBackpackGroup = SlotGroups.FindByPredicate(
		[&BackpackSlot](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerHandle == BackpackSlot;
		});
	if (!TestNotNull(TEXT("The layout exposes the static Gear.Backpack group"), StaticBackpackGroup))
	{
		return false;
	}
	TestEqual(
		TEXT("The static Gear.Backpack group owns the Backpack equipment-slot role"),
		StaticBackpackGroup->EquipmentSlotRole,
		ERpgEquipmentSlot::Backpack);
	TestEqual(
		TEXT("The static Gear.Backpack group has no provider provenance"),
		StaticBackpackGroup->SourceEquipmentSlot,
		ERpgEquipmentSlot::None);

	const FRpgInventorySlotGroupView* BackpackProviderGroup = SlotGroups.FindByPredicate(
		[&BackpackContents](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerHandle == BackpackContents;
		});
	if (!TestNotNull(TEXT("The layout exposes the item-owned Backpack content group"), BackpackProviderGroup))
	{
		return false;
	}
	TestEqual(
		TEXT("The item-owned Backpack content group has no equipment-slot role"),
		BackpackProviderGroup->EquipmentSlotRole,
		ERpgEquipmentSlot::None);
	TestEqual(
		TEXT("The item-owned Backpack content group preserves Backpack provider provenance"),
		BackpackProviderGroup->SourceEquipmentSlot,
		ERpgEquipmentSlot::Backpack);

	const bool bLayoutExposesBothProviders = SlotGroups.ContainsByPredicate(
		[&BackpackContents](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerHandle == BackpackContents &&
				Group.SourceEquipmentSlot == ERpgEquipmentSlot::Backpack;
		}) && SlotGroups.ContainsByPredicate(
		[&BeltContents](const FRpgInventorySlotGroupView& Group)
		{
			return Group.ContainerHandle == BeltContents &&
				Group.SourceEquipmentSlot == ERpgEquipmentSlot::Belt;
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
	FRpgInventoryEntryView SourceEntry;
	if (!TestTrue(
		TEXT("The quick-transfer source has a complete replicated snapshot"),
		GetEntryView(Inventory, SourceItem->GetItemId(), SourceEntry)))
	{
		return false;
	}

	FRpgInventorySlotAddress BackpackAddress;
	BackpackAddress.SetContainerHandle(BackpackContents);
	BackpackAddress.X = 0;
	BackpackAddress.Y = 0;
	FRpgInventorySlotAddress BeltAddress;
	BeltAddress.SetContainerHandle(BeltContents);
	BeltAddress.X = BackpackAddress.X;
	BeltAddress.Y = BackpackAddress.Y;
	TestFalse(
		TEXT("Same-cell addresses in Backpack Main and Belt Main keep distinct provider identities"),
		BackpackAddress == BeltAddress);

	FRpgInventoryGridPlacement ResolvedBackpackPlacement;
	FRpgInventoryGridPlacement ResolvedBeltPlacement;
	if (!TestTrue(
			TEXT("The layout resolves the exact Backpack Main address"),
			Layout->ResolveSlotAddress(BackpackAddress, ResolvedBackpackPlacement)) ||
		!TestTrue(
			TEXT("The layout resolves the exact Belt Main address"),
			Layout->ResolveSlotAddress(BeltAddress, ResolvedBeltPlacement)))
	{
		return false;
	}
	TestEqual(
		TEXT("Backpack address resolution preserves its provider-owned handle"),
		ResolvedBackpackPlacement.GetContainerHandle(),
		BackpackContents);
	TestEqual(
		TEXT("Belt address resolution preserves its provider-owned handle"),
		ResolvedBeltPlacement.GetContainerHandle(),
		BeltContents);

	FRpgInventorySlotAddress RoundTrippedBackpackAddress;
	FRpgInventorySlotAddress RoundTrippedBeltAddress;
	if (!TestTrue(
			TEXT("The resolved Backpack placement converts back to a visible address"),
			Layout->TryMakeSlotAddressFromPlacement(
				ResolvedBackpackPlacement,
				RoundTrippedBackpackAddress)) ||
		!TestTrue(
			TEXT("The resolved Belt placement converts back to a visible address"),
			Layout->TryMakeSlotAddressFromPlacement(
				ResolvedBeltPlacement,
				RoundTrippedBeltAddress)))
	{
		return false;
	}
	TestTrue(
		TEXT("Backpack placement round-trip preserves the complete slot address"),
		RoundTrippedBackpackAddress == BackpackAddress);
	TestTrue(
		TEXT("Belt placement round-trip preserves the complete slot address"),
		RoundTrippedBeltAddress == BeltAddress);

	FRpgInventoryGridSize BackpackGridSize;
	FRpgInventoryGridSize BeltGridSize;
	if (!TestTrue(
			TEXT("Grid-size lookup resolves the exact Backpack Main handle"),
			Layout->GetGridSizeForContainerHandle(
				BackpackContents,
				BackpackGridSize)) ||
		!TestTrue(
			TEXT("Grid-size lookup resolves the exact Belt Main handle"),
			Layout->GetGridSizeForContainerHandle(
				BeltContents,
				BeltGridSize)))
	{
		return false;
	}
	TestEqual(TEXT("Backpack Main keeps its authored width"), BackpackGridSize.Width, 4);
	TestEqual(TEXT("Backpack Main keeps its authored height"), BackpackGridSize.Height, 4);
	TestEqual(TEXT("Belt Main keeps its authored width"), BeltGridSize.Width, 4);
	TestEqual(TEXT("Belt Main keeps its authored height"), BeltGridSize.Height, 4);
	const FRpgInventoryContainerHandle UnknownMain = FRpgInventoryContainerHandle::MakeItemOwned(
		FRpgInventoryItemId::NewId(),
		BagContainerId,
		1);
	FRpgInventoryGridSize UnknownGridSize;
	TestFalse(
		TEXT("A matching local Main id cannot resolve without the exact provider identity"),
		Layout->GetGridSizeForContainerHandle(UnknownMain, UnknownGridSize));

	TestEqual(
		TEXT("Backpack Main address lookup returns the item in that exact provider grid"),
		Layout->GetItemInSlotAddress(BackpackAddress),
		SourceItem);
	TestNull(
		TEXT("The same Belt Main coordinates do not alias the occupied Backpack Main cell"),
		Layout->GetItemInSlotAddress(BeltAddress));

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
	Request.ExpectedEntryId = SourceEntry.EntryId;
	Request.ExpectedSourcePlacement = SourceEntry.Placement;
	Request.ExpectedSourceQuantity = SourceEntry.StackCount;
	Request.StackCount = 1;
	Request.PreferredTargetContainers = { Pockets, BeltContents };

	FRpgInventoryContainerHandle ResolvedContainer;
	FRpgInventoryGridPlacement ResolvedPlacement;
	const FRpgInventoryPlacementPlan QuickTransferPlan =
		UiActions->PlanQuickTransferDestination(
			Inventory,
			Inventory,
			Request,
			ResolvedContainer,
			ResolvedPlacement);
	TestTrue(
		TEXT("Quick transfer exposes one complete deterministic placement plan"),
		QuickTransferPlan.IsCompleteSuccess());
	if (!TestEqual(
			TEXT("The one-unit quick transfer needs exactly one placement step"),
			QuickTransferPlan.Steps.Num(),
			1))
	{
		return false;
	}
	TestEqual(
		TEXT("The selected quick-transfer step is a concrete placement"),
		QuickTransferPlan.Steps[0].Resolution,
		ERpgInventoryPlacementResolution::Place);
	TestEqual(
		TEXT("The public plan and resolved output expose the same target"),
		QuickTransferPlan.Steps[0].Placement,
		ResolvedPlacement);
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
	FRpgInventoryEntryView RejectedDepositEntry;
	if (!TestTrue(
		TEXT("The rejected deposit still exposes a complete source snapshot"),
		GetEntryView(
			PlayerInventory,
			RejectedDepositItem->GetItemId(),
			RejectedDepositEntry)))
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
	RejectedQuickTransfer.ExpectedEntryId = RejectedDepositEntry.EntryId;
	RejectedQuickTransfer.ExpectedSourcePlacement =
		RejectedDepositEntry.Placement;
	RejectedQuickTransfer.ExpectedSourceQuantity =
		RejectedDepositEntry.StackCount;
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
		UiActions->FindQuickTransferDestination(
			PlayerInventory,
			OutputInventory,
			RejectedQuickTransfer,
			PredictedContainer,
			PredictedPlacement));

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
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			OutputInventory,
			MakeExactTransferIntent(
				PlayerInventory,
				RejectedDepositItem->GetItemId(),
				1,
				RejectedExactPlacement)).IsCompleteSuccess());
	UiActions->RequestTransferInventoryItem(
		PlayerInventory,
		OutputInventory,
		MakeExactTransferIntent(
			PlayerInventory,
			RejectedDepositItem->GetItemId(),
			1,
			RejectedExactPlacement));
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
	FRpgInventoryMoveIntent InternalMove;
	InternalMove.EnsureRequestId();
	InternalMove.ItemId = CraftedEntryBeforeMove.ItemId;
	InternalMove.ExpectedEntryId = CraftedEntryBeforeMove.EntryId;
	InternalMove.ExpectedSourcePlacement =
		CraftedEntryBeforeMove.Placement;
	InternalMove.ExpectedQuantity =
		CraftedEntryBeforeMove.StackCount;
	InternalMove.TargetPlacement =
		MakePlacement(
			CraftedEntryBeforeMove.Placement.GetContainerHandle(),
			3,
			0);
	UiActions->RequestMoveInventoryItem(
		OutputInventory,
		InternalMove);
	FRpgInventoryEntryView CraftedEntryAfterMove;
	TestTrue(
		TEXT("The output entry remains addressable after an internal move"),
		GetEntryView(OutputInventory, CraftedOutputId, CraftedEntryAfterMove));
	TestEqual(TEXT("The withdrawal-only policy preserves internal output reordering"), CraftedEntryAfterMove.Placement.X, 3);
	TestEqual(TEXT("The internal output move preserves its row"), CraftedEntryAfterMove.Placement.Y, 0);

	FRpgInventoryQuickTransferRequest AllowedWithdrawalPrediction;
	AllowedWithdrawalPrediction.RequestId = FGuid::NewGuid();
	AllowedWithdrawalPrediction.ItemId = CraftedOutputId;
	AllowedWithdrawalPrediction.ExpectedEntryId =
		CraftedEntryAfterMove.EntryId;
	AllowedWithdrawalPrediction.ExpectedSourcePlacement =
		CraftedEntryAfterMove.Placement;
	AllowedWithdrawalPrediction.ExpectedSourceQuantity =
		CraftedEntryAfterMove.StackCount;
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
	FRpgInventoryTransferIntent WithdrawalIntent = MakeExactTransferIntent(
		OutputInventory,
		CraftedEntryAfterMove.ItemId,
		1,
		PlayerWithdrawalPlacement);
	TestTrue(
		TEXT("The shared exact-placement contract permits output withdrawal"),
		UiActions->PlanExactTransferPlacement(
			OutputInventory,
			PlayerInventory,
			WithdrawalIntent).IsCompleteSuccess());
	UiActions->RequestTransferInventoryItem(
		OutputInventory,
		PlayerInventory,
		WithdrawalIntent);
	TestNull(
		TEXT("The withdrawn item leaves the crafting output"),
		OutputInventory->FindItemById(CraftedOutputId));
	TestNotNull(
		TEXT("The withdrawn item arrives in the player inventory with stable identity"),
		PlayerInventory->FindItemById(CraftedOutputId));
	const FString OutputAfterWithdrawal =
		MakeInventorySignature(OutputInventory);
	const FString PlayerAfterWithdrawal =
		MakeInventorySignature(PlayerInventory);
	UiActions->RequestTransferInventoryItem(
		OutputInventory,
		PlayerInventory,
		WithdrawalIntent);
	TestEqual(
		TEXT("An exact UI transfer retry preserves the empty source"),
		MakeInventorySignature(OutputInventory),
		OutputAfterWithdrawal);
	TestEqual(
		TEXT("An exact UI transfer retry cannot add the withdrawn item twice"),
		MakeInventorySignature(PlayerInventory),
		PlayerAfterWithdrawal);

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
	FRpgInventoryEntryView RegularTransferEntry;
	if (!TestTrue(
			TEXT("The normal storage-transfer source snapshot resolves"),
			GetEntryView(
				PlayerInventory,
				RegularTransferItemId,
				RegularTransferEntry)))
	{
		return false;
	}
	const FRpgInventoryGridPlacement RegularTargetPlacement = MakePlacement(RegularRoot, 3, 0);
	TestTrue(
		TEXT("The shared transfer contract still permits ordinary storage deposits"),
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			RegularInventory,
			MakeExactTransferIntent(
				PlayerInventory,
				RegularTransferItem->GetItemId(),
				1,
				RegularTargetPlacement)).IsCompleteSuccess());
	FRpgInventoryQuickTransferRequest RegularQuickTransfer;
	RegularQuickTransfer.RequestId = FGuid::NewGuid();
	RegularQuickTransfer.ItemId = RegularTransferItemId;
	RegularQuickTransfer.ExpectedEntryId =
		RegularTransferEntry.EntryId;
	RegularQuickTransfer.ExpectedSourcePlacement =
		RegularTransferEntry.Placement;
	RegularQuickTransfer.ExpectedSourceQuantity =
		RegularTransferEntry.StackCount;
	RegularQuickTransfer.StackCount = 1;
	RegularQuickTransfer.PreferredTargetContainers.Add(
		RegularRoot);
	UiActions->RequestQuickTransferItem(
		PlayerInventory,
		RegularInventory,
		RegularQuickTransfer);
	TestNull(
		TEXT("The ordinary transfer removes the item from the player"),
		PlayerInventory->FindItemById(RegularTransferItemId));
	TestNotNull(
		TEXT("The ordinary transfer still reaches a regular container"),
		RegularInventory->FindItemById(RegularTransferItemId));
	const FString PlayerAfterRegularTransfer =
		MakeInventorySignature(PlayerInventory);
	const FString RegularAfterTransfer =
		MakeInventorySignature(RegularInventory);
	UiActions->RequestQuickTransferItem(
		PlayerInventory,
		RegularInventory,
		RegularQuickTransfer);
	TestEqual(
		TEXT("A quick-transfer retry preserves the source after full removal"),
		MakeInventorySignature(PlayerInventory),
		PlayerAfterRegularTransfer);
	TestEqual(
		TEXT("A quick-transfer retry cannot derive and apply a second destination"),
		MakeInventorySignature(RegularInventory),
		RegularAfterTransfer);

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
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			RegularInventory,
			MakeExactTransferIntent(
				PlayerInventory,
				SwapSource->GetItemId(),
				1,
				MakePlacement(RegularRoot, 1, 0))).IsCompleteSuccess());

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
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			OutputInventory,
			MakeExactTransferIntent(
				PlayerInventory,
				MultiComponentDepositItem->GetItemId(),
				1,
				MakePlacement(OutputRoot, 5, 0))).IsCompleteSuccess());

	SecondaryCraftingStation->SetOutputInventoryManager(RegularInventory);
	TestTrue(
		TEXT("The externally assigned output remains accessible through its crafting station"),
		UiActions->CanAccessInventory(RegularInventory));
	TestFalse(
		TEXT("An externally assigned crafting output remains withdrawal-only even when its owner is ordinary storage"),
		UiActions->FindQuickTransferDestination(
			PlayerInventory,
			RegularInventory,
			MakeQuickTransferRequest(
				PlayerInventory,
				MultiComponentDepositItem->GetItemId(),
				1),
			PredictedContainer,
			PredictedPlacement));
	TestFalse(
		TEXT("Exact-placement prediction also recognizes an externally assigned crafting output"),
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			RegularInventory,
			MakeExactTransferIntent(
				PlayerInventory,
				MultiComponentDepositItem->GetItemId(),
				1,
				MakePlacement(RegularRoot, 5, 0))).IsCompleteSuccess());
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
		4,
		MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* NearlyFullTargetStack = TargetInventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass(),
		9,
		MakePlacement(TargetRoot, 1, 0));
	if (!TestNotNull(TEXT("The player owns a four-item source stack"), SourceStack) ||
		!TestNotNull(TEXT("The target owns a compatible stack with one free unit"), NearlyFullTargetStack))
	{
		return false;
	}

	const FRpgInventoryGridPlacement TargetStackPlacement = MakePlacement(TargetRoot, 1, 0);
	TestEqual(
		TEXT("The compatible target stack exposes exactly one free unit"),
		TargetInventory->GetFreeStackCapacity(NearlyFullTargetStack),
		1);
	FRpgInventoryEntryView InitialSourceEntry;
	FRpgInventoryEntryView InitialTargetEntry;
	if (!TestTrue(
			TEXT("The initial exact-transfer source snapshot is addressable"),
			GetEntryView(
				PlayerInventory,
				SourceStack->GetItemId(),
				InitialSourceEntry)) ||
		!TestTrue(
			TEXT("The initial exact-transfer target snapshot is addressable"),
			GetEntryView(
				TargetInventory,
				NearlyFullTargetStack->GetItemId(),
				InitialTargetEntry)))
	{
		return false;
	}
	auto MakeExactTransferIntent =
		[&InitialSourceEntry, &TargetStackPlacement](int32 Quantity)
	{
		FRpgInventoryTransferIntent Intent;
		Intent.ItemId = InitialSourceEntry.ItemId;
		Intent.ExpectedEntryId = InitialSourceEntry.EntryId;
		Intent.ExpectedSourcePlacement = InitialSourceEntry.Placement;
		Intent.ExpectedSourceQuantity = InitialSourceEntry.StackCount;
		Intent.TargetContainer =
			TargetStackPlacement.GetContainerHandle();
		Intent.TargetPlacement = TargetStackPlacement;
		Intent.Quantity = Quantity;
		return Intent;
	};

	const FRpgInventoryPlacementPlan PartialCapacityPlan =
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			MakeExactTransferIntent(2));
	TestEqual(
		TEXT("The exact evaluator reports the one-unit fit as partial"),
		PartialCapacityPlan.Code,
		ERpgInventoryMutationResultCode::PartiallyApplied);
	TestEqual(
		TEXT("The partial plan covers only the target stack's free unit"),
		PartialCapacityPlan.AppliedQuantity,
		1);
	TestFalse(
		TEXT("A partial placement plan cannot authorize a full drag commit"),
		PartialCapacityPlan.IsCompleteSuccess());

	const FRpgInventoryPlacementPlan CompatibleMergePlan =
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			MakeExactTransferIntent(1));
	TestTrue(
		TEXT("A quantity matching the free capacity yields a complete plan"),
		CompatibleMergePlan.IsCompleteSuccess());
	if (!TestEqual(
			TEXT("The compatible exact transfer has one merge step"),
			CompatibleMergePlan.Steps.Num(),
			1))
	{
		return false;
	}
	TestEqual(
		TEXT("Runtime-compatible exact transfer resolves to Merge"),
		CompatibleMergePlan.Steps[0].Resolution,
		ERpgInventoryPlacementResolution::Merge);
	TestEqual(
		TEXT("The merge plan names the concrete target entry"),
		CompatibleMergePlan.Steps[0].TargetEntryId,
		InitialTargetEntry.EntryId);

	FRpgInventoryTransferIntent MatchingCanonicalHandleIntent =
		MakeExactTransferIntent(1);
	MatchingCanonicalHandleIntent.ExpectedSourcePlacement.SetContainerHandle(
		InitialSourceEntry.Placement.GetContainerHandle());
	const FRpgInventoryPlacementPlan MatchingCanonicalHandlePlan =
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			MatchingCanonicalHandleIntent);
	TestTrue(
		TEXT("An exact matching canonical handle preserves the source snapshot"),
		MatchingCanonicalHandlePlan.IsCompleteSuccess());

	FRpgInventoryTransferIntent StaleCanonicalIntent =
		MakeExactTransferIntent(1);
	StaleCanonicalIntent.ExpectedSourcePlacement.SetContainerHandle(
		FRpgInventoryContainerHandle::MakeRoot(TEXT("StaleCanonicalRoot")));
	const FRpgInventoryPlacementPlan StaleCanonicalPlan =
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			StaleCanonicalIntent);
	TestEqual(
		TEXT("A stale canonical handle is rejected as a source mismatch"),
		StaleCanonicalPlan.Code,
		ERpgInventoryMutationResultCode::SourceMismatch);
	TestFalse(
		TEXT("A stale canonical source snapshot never becomes a complete preview plan"),
		StaleCanonicalPlan.IsCompleteSuccess());

	TestFalse(
		TEXT("Exact-placement preview rejects a request larger than the compatible stack's free capacity"),
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			MakeExactTransferIntent(2)).IsCompleteSuccess());
	TestTrue(
		TEXT("Exact-placement preview accepts a request that exactly fits the compatible stack"),
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			MakeExactTransferIntent(1)).IsCompleteSuccess());
	NearlyFullTargetStack->AddStatTagStack(RpgGameplayTags::Rpg_Inventory_Action_Transfer, 1);
	const FRpgInventoryPlacementPlan RuntimeIncompatiblePlan =
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			MakeExactTransferIntent(1));
	TestEqual(
		TEXT("Same-definition stacks with different runtime state are explicitly incompatible"),
		RuntimeIncompatiblePlan.Code,
		ERpgInventoryMutationResultCode::StackIncompatible);
	TestFalse(
		TEXT("Runtime-incompatible stacks produce no accepted placement plan"),
		RuntimeIncompatiblePlan.IsCompleteSuccess());
	TestFalse(
		TEXT("Exact-placement preview rejects a same-definition stack with incompatible runtime state"),
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			MakeExactTransferIntent(1)).IsCompleteSuccess());

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(
				Controller,
				Controller);
	if (!TestNotNull(
			TEXT("The exact-transfer fixture owns a screen-local coordinator"),
			Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);
	FRpgInventoryDragPayload RuntimeIncompatiblePayload;
	RuntimeIncompatiblePayload.SourceType =
		ERpgInventoryDragSourceType::InventoryEntry;
	RuntimeIncompatiblePayload.SourceInventory = PlayerInventory;
	RuntimeIncompatiblePayload.ItemInstance = SourceStack;
	RuntimeIncompatiblePayload.EntryId = InitialSourceEntry.EntryId;
	RuntimeIncompatiblePayload.StackCount = InitialSourceEntry.StackCount;
	RuntimeIncompatiblePayload.SourcePlacement =
		InitialSourceEntry.Placement;
	RuntimeIncompatiblePayload.ItemFootprint =
		InitialSourceEntry.Placement.GetUnrotatedSize();
	FRpgInventoryDropTarget RuntimeIncompatibleTarget;
	RuntimeIncompatibleTarget.TargetType =
		ERpgInventoryDropTargetType::InventorySlot;
	RuntimeIncompatibleTarget.TargetInventory = TargetInventory;
	RuntimeIncompatibleTarget.TargetPlacement = TargetStackPlacement;
	const FRpgInventoryInteractionPreviewPlan RuntimeIncompatiblePreview =
		Coordinator->PlanInteractionPreview(
			RuntimeIncompatiblePayload,
			RuntimeIncompatibleTarget);
	TestTrue(
		TEXT("The UI semantic is derived from the rejected domain plan"),
		RuntimeIncompatiblePreview.bUsesPlacementPlan);
	TestEqual(
		TEXT("Runtime-incompatible cross-inventory stacks preview as Blocked, never Merge"),
		RuntimeIncompatiblePreview.State,
		ERpgInventoryInteractionPreviewState::Blocked);
	TestEqual(
		TEXT("The UI retains the exact StackIncompatible reason"),
		RuntimeIncompatiblePreview.PlacementPlan.Code,
		ERpgInventoryMutationResultCode::StackIncompatible);

	URpgInventoryItemInstance* LocalRuntimeIncompatibleStack =
		PlayerInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackableWeaponItemDefinition::
				StaticClass(),
			3,
			MakePlacement(Pockets, 1, 0));
	if (!TestNotNull(
			TEXT("A same-inventory runtime-incompatible target stack exists"),
			LocalRuntimeIncompatibleStack))
	{
		return false;
	}
	LocalRuntimeIncompatibleStack->AddStatTagStack(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		1);
	FRpgInventoryEntryView LocalIncompatibleEntry;
	if (!TestTrue(
			TEXT("The local runtime-incompatible target has an exact snapshot"),
			GetEntryView(
				PlayerInventory,
				LocalRuntimeIncompatibleStack->GetItemId(),
				LocalIncompatibleEntry)))
	{
		return false;
	}
	FRpgInventoryDropTarget LocalIncompatibleTarget;
	LocalIncompatibleTarget.TargetType =
		ERpgInventoryDropTargetType::InventorySlot;
	LocalIncompatibleTarget.TargetInventory = PlayerInventory;
	LocalIncompatibleTarget.TargetPlacement =
		LocalIncompatibleEntry.Placement;
	const FRpgInventoryInteractionPreviewPlan LocalIncompatiblePreview =
		Coordinator->PlanInteractionPreview(
			RuntimeIncompatiblePayload,
			LocalIncompatibleTarget);
	TestEqual(
		TEXT("Same-definition runtime-incompatible local stacks preview as Swap, never Merge"),
		LocalIncompatiblePreview.State,
		ERpgInventoryInteractionPreviewState::Swap);
	if (!TestTrue(
			TEXT("The local incompatible preview contains a complete swap plan"),
			LocalIncompatiblePreview.PlacementPlan.IsCompleteSuccess()) ||
		!TestEqual(
			TEXT("The local incompatible swap contains one atomic step"),
			LocalIncompatiblePreview.PlacementPlan.Steps.Num(),
			1))
	{
		return false;
	}
	TestEqual(
		TEXT("The local incompatible plan names the displaced placement"),
		LocalIncompatiblePreview.PlacementPlan.Steps[0].
			DisplacedPlacement,
		InitialSourceEntry.Placement);
	TestTrue(
		TEXT("The same-inventory commit applies the previewed runtime-incompatible swap"),
		Coordinator->CommitPayloadToTarget(
			RuntimeIncompatiblePayload,
			LocalIncompatibleTarget));
	FRpgInventoryEntryView SourceAfterLocalSwap;
	FRpgInventoryEntryView DisplacedAfterLocalSwap;
	TestTrue(
		TEXT("The local swap preserves the moving source identity"),
		GetEntryView(
			PlayerInventory,
			SourceStack->GetItemId(),
			SourceAfterLocalSwap));
	TestTrue(
		TEXT("The local swap preserves the displaced target identity"),
		GetEntryView(
			PlayerInventory,
			LocalRuntimeIncompatibleStack->GetItemId(),
			DisplacedAfterLocalSwap));
	TestEqual(
		TEXT("The moving source commits to the previewed target placement"),
		SourceAfterLocalSwap.Placement,
		LocalIncompatibleEntry.Placement);
	TestEqual(
		TEXT("The displaced local target commits to the previewed source placement"),
		DisplacedAfterLocalSwap.Placement,
		InitialSourceEntry.Placement);

	const FRpgInventoryItemId SourceItemId = SourceStack->GetItemId();
	const FRpgInventoryItemId ExistingTargetItemId = NearlyFullTargetStack->GetItemId();
	UiActions->RequestApplyInventoryEquipmentIntent(
		PlayerInventory,
		MakeEquipmentIntent(
			PlayerInventory,
			SourceStack,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::MainHand));
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
		TEXT("Exact-placement preview accepts half the stack into an empty target cell"),
		UiActions->PlanExactTransferPlacement(
			PlayerInventory,
			TargetInventory,
			RpgInventoryTransactionTests::MakeExactTransferIntent(
				PlayerInventory,
				SourceStack->GetItemId(),
				2,
				EmptyTargetPlacement)).IsCompleteSuccess());
	FRpgInventoryTransferIntent PartialTransferIntent;
	PartialTransferIntent.EnsureRequestId();
	PartialTransferIntent.ItemId = SourceEntryBeforeTransfer.ItemId;
	PartialTransferIntent.ExpectedEntryId =
		SourceEntryBeforeTransfer.EntryId;
	PartialTransferIntent.ExpectedSourcePlacement =
		SourceEntryBeforeTransfer.Placement;
	PartialTransferIntent.ExpectedSourceQuantity =
		SourceEntryBeforeTransfer.StackCount;
	PartialTransferIntent.TargetContainer =
		EmptyTargetPlacement.GetContainerHandle();
	PartialTransferIntent.TargetPlacement =
		EmptyTargetPlacement;
	PartialTransferIntent.Quantity = 2;
	UiActions->RequestTransferInventoryItem(
		PlayerInventory,
		TargetInventory,
		PartialTransferIntent);

	const FString PlayerAfterPartialTransfer =
		MakeInventorySignature(PlayerInventory);
	const FString TargetAfterPartialTransfer =
		MakeInventorySignature(TargetInventory);
	UiActions->RequestTransferInventoryItem(
		PlayerInventory,
		TargetInventory,
		PartialTransferIntent);
	TestEqual(
		TEXT("An exact half-stack retry applies no second source mutation"),
		MakeInventorySignature(PlayerInventory),
		PlayerAfterPartialTransfer);
	TestEqual(
		TEXT("An exact half-stack retry applies no second target mutation"),
		MakeInventorySignature(TargetInventory),
		TargetAfterPartialTransfer);

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
		TEXT("The exact target receives the requested two units"),
		TargetInventory->GetItemStackCount(TransferredUnit),
		2);

	const int32 TargetCountBeforeWholeTransfer =
		TargetInventory->GetTotalItemCountByDefinition(
			URpgInventoryAutomationTestStackableWeaponItemDefinition::StaticClass());
	UiActions->RequestQuickTransferItem(
		PlayerInventory,
		TargetInventory,
		MakeQuickTransferRequest(
			PlayerInventory,
			RemainingSourceStack->GetItemId(),
			2));

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
		4);
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

	EquipmentLoadout->ReconcileEquipmentLoadFromInventory();
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

	EquipmentLoadout->ReconcileEquipmentLoadFromInventory();
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

	EquipmentLoadout->ReconcileEquipmentLoadFromInventory();
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
	FRpgInventoryLegacyOrderingSurfaceRemovedTest,
	"SurvivalRpg.Inventory.Transaction.LegacyOrderingSurfaceRemoved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryLegacyOrderingSurfaceRemovedTest::RunTest(const FString& Parameters)
{
	const UEnum* MutationOperationEnum =
		StaticEnum<ERpgInventoryMutationOperation>();
	if (!TestNotNull(
			TEXT("The inventory mutation operation enum remains reflected"),
			MutationOperationEnum))
	{
		return false;
	}

	TestEqual(
		TEXT("The retired Sort enumerator is absent from reflection"),
		MutationOperationEnum->GetValueByNameString(TEXT("Sort")),
		static_cast<int64>(INDEX_NONE));

	struct FOperationOrdinalContract
	{
		ERpgInventoryMutationOperation Operation;
		int32 ExpectedOrdinal;
		const TCHAR* Label;
	};
	static const FOperationOrdinalContract OrdinalContracts[] = {
		{ERpgInventoryMutationOperation::Equip, 7, TEXT("Equip")},
		{ERpgInventoryMutationOperation::Pickup, 8, TEXT("Pickup")},
		{ERpgInventoryMutationOperation::Transfer, 9, TEXT("Transfer")},
		{ERpgInventoryMutationOperation::Drop, 10, TEXT("Drop")},
		{ERpgInventoryMutationOperation::Consume, 11, TEXT("Consume")},
		{ERpgInventoryMutationOperation::Restore, 12, TEXT("Restore")},
	};
	for (const FOperationOrdinalContract& Contract : OrdinalContracts)
	{
		TestEqual(
			*FString::Printf(
				TEXT("%s keeps its serialized mutation ordinal"),
				Contract.Label),
			static_cast<int32>(Contract.Operation),
			Contract.ExpectedOrdinal);
	}

	static const FName RemovedManagerFunctions[] = {
		TEXT("ApplyInventorySort"),
		TEXT("MoveInventoryEntry"),
	};
	for (const FName FunctionName : RemovedManagerFunctions)
	{
		TestNull(
			*FString::Printf(
				TEXT("%s is absent from the inventory manager reflection surface"),
				*FunctionName.ToString()),
			URpgInventoryManagerComponent::StaticClass()->FindFunctionByName(
				FunctionName));
	}

	static const FName RemovedUiFunctions[] = {
		TEXT("RequestApplyInventorySort"),
		TEXT("RequestMoveInventoryEntry"),
	};
	for (const FName FunctionName : RemovedUiFunctions)
	{
		TestNull(
			*FString::Printf(
				TEXT("%s is absent from the inventory UI action reflection surface"),
				*FunctionName.ToString()),
			URpgInventoryUiActionComponent::StaticClass()->FindFunctionByName(
				FunctionName));
	}

	TestNotNull(
		TEXT("BaseStorage keeps the shared resource-sort mode enum"),
		StaticEnum<ERpgInventorySortMode>());
	static const FName PreservedBaseStorageRequests[] = {
		TEXT("RequestApplyBaseResourceSort"),
		TEXT("RequestMoveBaseResourceEntry"),
	};
	for (const FName FunctionName : PreservedBaseStorageRequests)
	{
		const UFunction* Function =
			URpgInventoryUiActionComponent::StaticClass()->FindFunctionByName(
				FunctionName);
		if (!TestNotNull(
				*FString::Printf(
					TEXT("BaseStorage keeps the reflected %s request"),
					*FunctionName.ToString()),
				Function))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s remains a server RPC"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(FUNC_NetServer));
		TestTrue(
			*FString::Printf(
				TEXT("%s remains reliable"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(FUNC_NetReliable));
		TestTrue(
			*FString::Printf(
				TEXT("%s remains BlueprintCallable"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
		TestFalse(
			*FString::Printf(
				TEXT("%s is not a client RPC"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(FUNC_NetClient));
		TestFalse(
			*FString::Printf(
				TEXT("%s is not a multicast RPC"),
				*FunctionName.ToString()),
			Function->HasAnyFunctionFlags(FUNC_NetMulticast));
	}

	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(
		World,
		AActor::StaticClass(),
		TEXT("LegacyOrderingBaseStorageOwner"));
	SpawnParameters.ObjectFlags = RF_Transient;
	AActor* BaseStorageOwner = World->SpawnActor<AActor>(SpawnParameters);
	URpgBaseStorageComponent* BaseStorage = BaseStorageOwner
		? NewObject<URpgBaseStorageComponent>(
			BaseStorageOwner,
			TEXT("BaseStorage"),
			RF_Transient)
		: nullptr;
	if (!TestNotNull(
			TEXT("The BaseStorage ordering fixture owns an actor"),
			BaseStorageOwner) ||
		!TestNotNull(
			TEXT("The BaseStorage ordering fixture owns a storage component"),
			BaseStorage))
	{
		return false;
	}
	BaseStorageOwner->AddInstanceComponent(BaseStorage);
	BaseStorage->RegisterComponent();
	TestTrue(
		TEXT("The BaseStorage ordering fixture is server-authoritative"),
		BaseStorageOwner->HasAuthority());

	const TSubclassOf<URpgInventoryItemDefinition> AlphabeticalFirst =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	const TSubclassOf<URpgInventoryItemDefinition> AlphabeticalSecond =
		URpgInventoryAutomationTestStatefulMaterialDefinition::StaticClass();
	constexpr int32 FirstCount = 3;
	constexpr int32 SecondCount = 7;
	constexpr int32 ResourceCapacity = 20;

	BaseStorage->AddResourceCapacity(
		AlphabeticalSecond,
		ResourceCapacity);
	TestTrue(
		TEXT("The later-alphabetical BaseStorage resource receives a count"),
		BaseStorage->StoreDefinitionResource(
			AlphabeticalSecond,
			SecondCount));
	BaseStorage->AddResourceCapacity(
		AlphabeticalFirst,
		ResourceCapacity);
	TestTrue(
		TEXT("The earlier-alphabetical BaseStorage resource receives a count"),
		BaseStorage->StoreDefinitionResource(
			AlphabeticalFirst,
			FirstCount));
	TestEqual(
		TEXT("The earlier-alphabetical resource keeps its configured capacity"),
		BaseStorage->GetResourceCapacity(AlphabeticalFirst),
		ResourceCapacity);
	TestEqual(
		TEXT("The later-alphabetical resource keeps its configured capacity"),
		BaseStorage->GetResourceCapacity(AlphabeticalSecond),
		ResourceCapacity);
	TestEqual(
		TEXT("The earlier-alphabetical resource keeps its stored count"),
		BaseStorage->GetResourceCount(AlphabeticalFirst),
		FirstCount);
	TestEqual(
		TEXT("The later-alphabetical resource keeps its stored count"),
		BaseStorage->GetResourceCount(AlphabeticalSecond),
		SecondCount);

	const TArray<FRpgBaseResourceEntryView> InitialRows =
		BaseStorage->GetAllResources();
	if (!TestEqual(
			TEXT("The BaseStorage ordering fixture contains two rows"),
			InitialRows.Num(),
			2))
	{
		return false;
	}
	TestTrue(
		TEXT("The fixture starts in insertion order rather than name order"),
		InitialRows[0].ItemDefinition == AlphabeticalSecond);
	TestTrue(
		TEXT("Insertion order keeps a strictly increasing BaseStorage SortIndex"),
		InitialRows[0].SortIndex < InitialRows[1].SortIndex);

	TestTrue(
		TEXT("BaseStorage name sort rewrites the resource-row order"),
		BaseStorage->ApplyResourceSort(ERpgInventorySortMode::Name));
	const TArray<FRpgBaseResourceEntryView> NameSortedRows =
		BaseStorage->GetAllResources();
	if (!TestEqual(
			TEXT("Name sort preserves both BaseStorage rows"),
			NameSortedRows.Num(),
			2))
	{
		return false;
	}
	TestTrue(
		TEXT("Name sort places the alphabetically earlier resource first"),
		NameSortedRows[0].ItemDefinition == AlphabeticalFirst);
	TestTrue(
		TEXT("Name sort places the alphabetically later resource second"),
		NameSortedRows[1].ItemDefinition == AlphabeticalSecond);
	TestEqual(
		TEXT("Name sort assigns contiguous SortIndex zero"),
		NameSortedRows[0].SortIndex,
		0);
	TestEqual(
		TEXT("Name sort assigns contiguous SortIndex one"),
		NameSortedRows[1].SortIndex,
		1);

	TestTrue(
		TEXT("BaseStorage manual move returns the later resource to index zero"),
		BaseStorage->MoveResourceEntry(AlphabeticalSecond, 0));
	const TArray<FRpgBaseResourceEntryView> ManuallyMovedRows =
		BaseStorage->GetAllResources();
	if (!TestEqual(
			TEXT("Manual move preserves both BaseStorage rows"),
			ManuallyMovedRows.Num(),
			2))
	{
		return false;
	}
	TestTrue(
		TEXT("Manual move places the requested resource first"),
		ManuallyMovedRows[0].ItemDefinition == AlphabeticalSecond);
	TestTrue(
		TEXT("Manual move places the other resource second"),
		ManuallyMovedRows[1].ItemDefinition == AlphabeticalFirst);
	TestEqual(
		TEXT("Manual move reassigns contiguous SortIndex zero"),
		ManuallyMovedRows[0].SortIndex,
		0);
	TestEqual(
		TEXT("Manual move reassigns contiguous SortIndex one"),
		ManuallyMovedRows[1].SortIndex,
		1);
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
	FRpgInventoryCanonicalStackKeyContractTest,
	"SurvivalRpg.Inventory.Transaction.CanonicalStackKeyContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryCanonicalStackKeyContractTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("CanonicalStackKeyInventory"));
	if (!TestNotNull(TEXT("Canonical-key inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* First =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStatefulMaterialDefinition::
				StaticClass(),
			4,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* Second =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStatefulMaterialDefinition::
				StaticClass(),
			3,
			MakePlacement(Root, 1, 0));
	if (!TestNotNull(TEXT("First stateful stack exists"), First) ||
		!TestNotNull(TEXT("Second stateful stack exists"), Second))
	{
		return false;
	}

	FRpgInventoryStackKey FirstDefaultKey;
	FRpgInventoryStackKey SecondDefaultKey;
	TestTrue(
		TEXT("First default stack builds a valid canonical key"),
		First->TryBuildStackKey(FirstDefaultKey) &&
			FirstDefaultKey.IsValid());
	TestTrue(
		TEXT("Second default stack builds a valid canonical key"),
		Second->TryBuildStackKey(SecondDefaultKey) &&
			SecondDefaultKey.IsValid());
	TestTrue(
		TEXT("Persistent identity and placement are excluded from the key"),
		First->GetItemId() != Second->GetItemId() &&
			FirstDefaultKey == SecondDefaultKey);
	TestFalse(
		TEXT("Even default fragment state stays concrete without an explicit rehydration contract"),
		First->CanCollapseIntoDefinitionCount());

	const TArray<FRpgInventoryFragmentStatePayload>& CanonicalPayloads =
		FirstDefaultKey.GetRuntimeState();
	TestEqual(
		TEXT("The key contains core and stateful-fragment payloads"),
		CanonicalPayloads.Num(),
		2);
	if (CanonicalPayloads.Num() == 2)
	{
		TestEqual(
			TEXT("Fragment payloads are ordered lexically rather than by definition order"),
			CanonicalPayloads[0].FragmentId,
			FName(TEXT("Automation.Stateful")));
		TestEqual(
			TEXT("The canonical core payload follows the lexical fragment id"),
			CanonicalPayloads[1].FragmentId,
			FName(TEXT("Inventory.Core.StatTags")));
	}

	First->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 2);
	First->AddStatTagStack(RpgGameplayTags::Ability_Support_Heal, 1);
	Second->AddStatTagStack(RpgGameplayTags::Ability_Support_Heal, 1);
	Second->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 2);
	FRpgInventoryStackKey FirstTaggedKey;
	FRpgInventoryStackKey SecondTaggedKey;
	TestTrue(
		TEXT("Semantically equal tags build keys regardless of insertion order"),
		First->TryBuildStackKey(FirstTaggedKey) &&
			Second->TryBuildStackKey(SecondTaggedKey) &&
			FirstTaggedKey == SecondTaggedKey);
	TestFalse(
		TEXT("A tagged runtime variant cannot collapse into a default definition credit"),
		First->CanCollapseIntoDefinitionCount());

	const URpgInventoryAutomationTestStatefulFragment* StatefulFragment =
		First->FindFragmentByClass<
			URpgInventoryAutomationTestStatefulFragment>();
	if (!TestNotNull(
		TEXT("The test definition exposes its stateful fragment"),
		StatefulFragment))
	{
		return false;
	}

	StatefulFragment->SetTestValue(Second, 37);
	FRpgInventoryStackKey SecondVariantKey;
	TestTrue(
		TEXT("The fragment variant still builds a valid key"),
		Second->TryBuildStackKey(SecondVariantKey));
	TestTrue(
		TEXT("Fragment-owned bytes participate in exact key equality"),
		SecondVariantKey != FirstTaggedKey);
	TestFalse(
		TEXT("A fragment-only variant cannot merge with the otherwise equal stack"),
		Second->IsStackCompatibleWith(First));

	FRpgInventoryMutationRequest VariantMerge = MakePlacementRequest(
		ERpgInventoryMutationOperation::Merge,
		Second,
		Root,
		Root,
		0,
		0);
	const FString BeforeVariantMerge = MakeInventorySignature(Inventory);
	const int32 RevisionBeforeVariantMerge =
		Inventory->GetInventoryRevision();
	const FRpgInventoryMutationResult VariantPlan =
		Inventory->PlanInventoryMutation(VariantMerge);
	TestEqual(
		TEXT("The canonical evaluator rejects a fragment-state mismatch"),
		VariantPlan.Code,
		ERpgInventoryMutationResultCode::StackIncompatible);
	TestEqual(
		TEXT("Fragment-state rejection leaves the graph unchanged"),
		MakeInventorySignature(Inventory),
		BeforeVariantMerge);
	TestEqual(
		TEXT("Fragment-state rejection does not advance revision"),
		Inventory->GetInventoryRevision(),
		RevisionBeforeVariantMerge);

	URpgInventoryItemInstance* DifferentDefinition =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
			1,
			MakePlacement(Root, 2, 0));
	FRpgInventoryStackKey DifferentDefinitionKey;
	TestTrue(
		TEXT("A different definition builds its own key"),
		DifferentDefinition &&
			DifferentDefinition->TryBuildStackKey(
				DifferentDefinitionKey));
	TestTrue(
		TEXT("Definition identity remains part of the canonical key"),
		DifferentDefinitionKey != FirstDefaultKey);
	TestTrue(
		TEXT("A stateless untagged material can collapse into a definition credit"),
		DifferentDefinition &&
			DifferentDefinition->CanCollapseIntoDefinitionCount());

	URpgInventoryItemInstance* MissingDefinition =
		NewObject<URpgInventoryItemInstance>(GetTransientPackage());
	FRpgInventoryStackKey InvalidKey = FirstDefaultKey;
	TestFalse(
		TEXT("Missing definitions fail key construction closed"),
		MissingDefinition->TryBuildStackKey(InvalidKey));
	TestFalse(
		TEXT("Failed key construction clears stale output"),
		InvalidKey.IsValid());
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
		URpgInventoryAutomationTestStatefulMaterialDefinition::StaticClass(),
		9,
		MakePlacement(Root, 0, 0));
	if (!TestNotNull(TEXT("Nine-unit source stack exists"), SourceStack))
	{
		return false;
	}
	SourceStack->AddStatTagStack(RpgGameplayTags::Ability_Attack_Basic, 3);
	const URpgInventoryAutomationTestStatefulFragment* StatefulFragment =
		SourceStack->FindFragmentByClass<
			URpgInventoryAutomationTestStatefulFragment>();
	if (!TestNotNull(
		TEXT("Split source exposes stateful fragment"),
		StatefulFragment))
	{
		return false;
	}
	StatefulFragment->SetTestValue(SourceStack, 19);

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
		TestEqual(
			TEXT("Split copies fragment-owned runtime state"),
			StatefulFragment->GetTestValue(SplitStack),
			static_cast<uint8>(19));
		TestTrue(TEXT("Copied runtime state remains stack-compatible"), SplitStack->IsStackCompatibleWith(SourceStack));
		FRpgInventoryStackKey SourceKey;
		FRpgInventoryStackKey SplitKey;
		TestTrue(
			TEXT("Split receives the exact canonical key despite a new identity and placement"),
			SourceStack->TryBuildStackKey(SourceKey) &&
				SplitStack->TryBuildStackKey(SplitKey) &&
				SourceKey == SplitKey);
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

	FRpgInventoryMutationRequest ZeroSplit = MakePlacementRequest(
		ERpgInventoryMutationOperation::Split,
		SourceStack,
		Root,
		Root,
		2,
		0);
	ZeroSplit.Quantity = 0;
	TestEqual(
		TEXT("Zero normalizes to the full entry and remains outside the exact split range"),
		Inventory->PlanInventoryMutation(ZeroSplit).Code,
		ERpgInventoryMutationResultCode::StackLimitReached);
	FRpgInventoryMutationRequest WholeStackSplit = MakePlacementRequest(
		ERpgInventoryMutationOperation::Split,
		SourceStack,
		Root,
		Root,
		2,
		0);
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
	const FRpgInventoryItemId ExistingItemId = ExistingItem->GetItemId();
	TestFalse(
		TEXT("A live managed item cannot rewrite its persistent identity"),
		ExistingItem->RestoreItemId(FRpgInventoryItemId::NewId()));
	TestTrue(
		TEXT("Rejected live identity rewrite preserves the canonical ItemId"),
		ExistingItem->GetItemId() == ExistingItemId);

	const FString InitialSignature = MakeInventorySignature(TargetInventory);
	const int32 InitialEntryCount = TargetInventory->GetUsedEntryCount();
	TestFalse(
		TEXT("Bootstrap preflight rejects an instance already contained by this inventory"),
		TargetInventory->CanBootstrapItemInstance(ExistingItem));
	TestNull(
		TEXT("Bootstrap cannot duplicate an already managed instance"),
		TargetInventory->BootstrapItemInstance(ExistingItem));
	TestEqual(
		TEXT("Rejected managed-instance bootstrap preserves the complete target graph"),
		MakeInventorySignature(TargetInventory),
		InitialSignature);
	TestEqual(
		TEXT("Rejected managed-instance bootstrap creates no second entry"),
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
	TestTrue(
		TEXT("Canonical consume detaches the duplicate-id fixture"),
		ConsumeWholeItem(TargetInventory, DuplicateIdCandidate));
	TestTrue(
		TEXT("The detached same-owner fixture can be assigned the occupied persistent id"),
		DuplicateIdCandidate->RestoreItemId(ExistingItem->GetItemId()));
	const FString BeforeDuplicateIdAdd = MakeInventorySignature(TargetInventory);
	TestFalse(
		TEXT("Bootstrap preflight rejects a different UObject with an occupied persistent id"),
		TargetInventory->CanBootstrapItemInstance(DuplicateIdCandidate));
	TestNull(
		TEXT("Bootstrap rejects the occupied persistent id"),
		TargetInventory->BootstrapItemInstance(DuplicateIdCandidate));
	TestEqual(
		TEXT("Duplicate-id bootstrap rejection preserves the authoritative graph"),
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
	TestTrue(
		TEXT("Canonical consume detaches the foreign setup fixture"),
		ConsumeWholeItem(ForeignInventory, ForeignDetachedItem));
	const FString BeforeForeignOuterAdd = MakeInventorySignature(TargetInventory);
	TestTrue(
		TEXT("Bootstrap preflight accepts detached foreign setup data"),
		TargetInventory->CanBootstrapItemInstance(ForeignDetachedItem));
	TestEqual(
		TEXT("Read-only foreign bootstrap preflight leaves the target unchanged"),
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
		TEXT("Bootstrap preflight rejects an item managed by a sibling inventory despite its matching Outer"),
		SiblingInventory->CanBootstrapItemInstance(ExistingItem));
	TestNull(
		TEXT("Bootstrap cannot duplicate a sibling-managed concrete item"),
		SiblingInventory->BootstrapItemInstance(ExistingItem));
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
	TestTrue(
		TEXT("Canonical consume detaches the sibling-id fixture"),
		ConsumeWholeItem(TargetInventory, SiblingDuplicateIdCandidate));
	TestTrue(
		TEXT("The detached target-owned candidate can copy the sibling's occupied id"),
		SiblingDuplicateIdCandidate->RestoreItemId(
			SiblingIdentityOwner->GetItemId()));
	const FString BeforeSiblingIdCollision =
		MakeInventorySignature(TargetInventory);
	TestFalse(
		TEXT("Bootstrap preflight rejects an ItemId occupied by another UObject in a sibling inventory"),
		TargetInventory->CanBootstrapItemInstance(
			SiblingDuplicateIdCandidate));
	TestNull(
		TEXT("Bootstrap rejects the sibling ItemId collision"),
		TargetInventory->BootstrapItemInstance(
			SiblingDuplicateIdCandidate));
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
	FRpgInventoryBootstrapSameOwnerReuseAndPreflightPurityTest,
	"SurvivalRpg.Inventory.Transaction.BootstrapSameOwnerReuseAndPreflightPurity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryBootstrapSameOwnerReuseAndPreflightPurityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("SameOwnerBootstrapInventory"));
	if (!TestNotNull(TEXT("The same-owner bootstrap inventory exists"), Inventory))
	{
		return false;
	}

	URpgInventoryItemInstance* DetachedItem =
		Inventory->GrantItemDefinition(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1);
	if (!TestNotNull(TEXT("The same-owner bootstrap fixture exists"), DetachedItem))
	{
		return false;
	}

	DetachedItem->AddStatTagStack(
		RpgGameplayTags::Ability_Attack_Basic,
		3);
	const FRpgInventoryItemId OriginalItemId = DetachedItem->GetItemId();
	UObject* const OriginalOuter = DetachedItem->GetOuter();
	const TSubclassOf<URpgInventoryItemDefinition> OriginalDefinition =
		DetachedItem->GetItemDef();
	TestTrue(
		TEXT("Canonical consume detaches the bootstrap fixture"),
		ConsumeWholeItem(Inventory, DetachedItem));
	if (!TestFalse(
			TEXT("The same-owner fixture is detached before bootstrap"),
			Inventory->ContainsItemInstance(DetachedItem)))
	{
		return false;
	}

	const FString GraphBeforePreflight =
		MakeInventorySignature(Inventory);
	const int32 RevisionBeforePreflight =
		Inventory->GetInventoryRevision();
	const uint64 EpochBeforePreflight =
		Inventory->GetMutationEpoch();

	TestTrue(
		TEXT("A detached same-owner item passes bootstrap preflight"),
		Inventory->CanBootstrapItemInstance(DetachedItem, 2));
	TestTrue(
		TEXT("Repeated same-owner bootstrap preflight remains deterministic"),
		Inventory->CanBootstrapItemInstance(DetachedItem, 2));
	TestEqual(
		TEXT("Bootstrap preflight preserves the complete graph"),
		MakeInventorySignature(Inventory),
		GraphBeforePreflight);
	TestEqual(
		TEXT("Bootstrap preflight preserves inventory revision"),
		Inventory->GetInventoryRevision(),
		RevisionBeforePreflight);
	TestEqual(
		TEXT("Bootstrap preflight preserves mutation epoch"),
		Inventory->GetMutationEpoch(),
		EpochBeforePreflight);
	TestFalse(
		TEXT("Bootstrap preflight does not make the detached item authoritative"),
		Inventory->ContainsItemInstance(DetachedItem));
	TestTrue(
		TEXT("Bootstrap preflight preserves the detached item identity"),
		DetachedItem->GetItemId() == OriginalItemId);
	TestEqual(
		TEXT("Bootstrap preflight preserves the detached item outer"),
		DetachedItem->GetOuter(),
		OriginalOuter);
	TestTrue(
		TEXT("Bootstrap preflight preserves the detached item definition"),
		DetachedItem->GetItemDef() == OriginalDefinition);
	TestEqual(
		TEXT("Bootstrap preflight preserves mutable runtime state"),
		DetachedItem->GetStatTagStackCount(
			RpgGameplayTags::Ability_Attack_Basic),
		3);

	URpgInventoryItemInstance* ReusedItem =
		Inventory->BootstrapItemInstance(DetachedItem, 2);
	if (!TestNotNull(
			TEXT("Same-owner bootstrap commits the detached item"),
			ReusedItem))
	{
		return false;
	}

	TestEqual(
		TEXT("Same-owner bootstrap reuses the exact UObject"),
		ReusedItem,
		DetachedItem);
	TestEqual(
		TEXT("Same-owner bootstrap preserves the actor outer"),
		ReusedItem->GetOuter(),
		OriginalOuter);
	TestTrue(
		TEXT("Same-owner bootstrap preserves the persistent identity"),
		ReusedItem->GetItemId() == OriginalItemId);
	TestTrue(
		TEXT("Same-owner bootstrap preserves the item definition"),
		ReusedItem->GetItemDef() == OriginalDefinition);
	TestEqual(
		TEXT("Same-owner bootstrap preserves mutable runtime state"),
		ReusedItem->GetStatTagStackCount(
			RpgGameplayTags::Ability_Attack_Basic),
		3);
	TestTrue(
		TEXT("The reused item is authoritative in the inventory again"),
		Inventory->ContainsItemInstance(ReusedItem));
	TestEqual(
		TEXT("The reused item resolves through its original identity"),
		Inventory->FindItemById(OriginalItemId),
		ReusedItem);
	TestEqual(
		TEXT("Same-owner bootstrap applies the requested stack quantity"),
		Inventory->GetItemStackCount(ReusedItem),
		2);
	TestEqual(
		TEXT("Same-owner bootstrap creates exactly one entry"),
		Inventory->GetUsedEntryCount(),
		1);
	TestEqual(
		TEXT("Same-owner bootstrap advances inventory revision exactly once"),
		Inventory->GetInventoryRevision(),
		RevisionBeforePreflight + 1);
	TestEqual(
		TEXT("Same-owner bootstrap does not replace the mutation epoch"),
		Inventory->GetMutationEpoch(),
		EpochBeforePreflight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgMissingHandlePlacementIngressRejectedTest,
	"SurvivalRpg.Inventory.IntentBoundary.MissingHandlePlacementIngressRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgMissingHandlePlacementIngressRejectedTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("MissingHandlePlacementTarget"));
	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("MissingHandlePlacementSource"));
	if (!TestNotNull(TEXT("The missing-handle target exists"), TargetInventory) ||
		!TestNotNull(TEXT("The missing-handle source exists"), SourceInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Storage = MakeStorageHandle();
	URpgInventoryItemInstance* TargetItem =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Storage, 0, 0));
	URpgInventoryItemInstance* SourceItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Storage, 0, 0));
	URpgInventoryItemInstance* DetachedTargetItem =
		TargetInventory->GrantItemDefinition(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1);
	if (!TestNotNull(TEXT("The target fixture item exists"), TargetItem) ||
		!TestNotNull(TEXT("The incoming source fixture exists"), SourceItem) ||
		!TestNotNull(TEXT("The detachable target-owned fixture exists"), DetachedTargetItem))
	{
		return false;
	}
	TestTrue(
		TEXT("Canonical consume detaches the placement fixture"),
		ConsumeWholeItem(TargetInventory, DetachedTargetItem));

	const TArray<FRpgInventoryEntryView> TargetEntries =
		TargetInventory->GetAllEntries();
	if (!TestEqual(TEXT("The target retains one managed fixture"), TargetEntries.Num(), 1) ||
		!TargetEntries.IsValidIndex(0))
	{
		return false;
	}

	FRpgInventoryGridPlacement MissingHandlePlacement;
	MissingHandlePlacement.X = 4;
	MissingHandlePlacement.Y = 0;
	MissingHandlePlacement.Width = 1;
	MissingHandlePlacement.Height = 1;
	TestFalse(
		TEXT("The fixture is invalid without its required canonical handle"),
		MissingHandlePlacement.IsValid());

	const FString TargetBefore = MakeInventorySignature(TargetInventory);
	const FString SourceBefore = MakeInventorySignature(SourceInventory);
	const int32 TargetRevisionBefore = TargetInventory->GetInventoryRevision();
	const int32 SourceRevisionBefore = SourceInventory->GetInventoryRevision();
	const uint64 TargetEpochBefore = TargetInventory->GetMutationEpoch();
	const uint64 SourceEpochBefore = SourceInventory->GetMutationEpoch();

	TestFalse(
		TEXT("Definition preflight rejects a placement without a handle"),
		TargetInventory->CanAddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MissingHandlePlacement));
	TestNull(
		TEXT("Definition insertion rejects a placement without a handle"),
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MissingHandlePlacement));
	TestFalse(
		TEXT("Incoming-transfer preflight rejects a placement without a handle"),
		TargetInventory->CanReceiveTransferredItemInstanceToPlacement(
			SourceItem,
			1,
			MissingHandlePlacement));
	TestFalse(
		TEXT("Swap-style incoming preflight rejects a placement without a handle"),
		TargetInventory->CanReceiveTransferredItemInstanceToPlacementIgnoringItem(
			SourceItem,
			1,
			MissingHandlePlacement,
			TargetItem));
	const FRpgInventoryMoveIntent MissingHandleMoveIntent = MakeMoveIntent(
		TargetInventory,
		TargetEntries[0].ItemId,
		MissingHandlePlacement);
	TestFalse(
		TEXT("Typed move preflight rejects a target without a handle"),
		TargetInventory->PlanMoveItem(MissingHandleMoveIntent).IsSuccess());
	TestFalse(
		TEXT("Typed move commit rejects a target without a handle"),
		TargetInventory->MoveItem(MissingHandleMoveIntent).IsSuccess());

	TestEqual(
		TEXT("Rejected missing-handle ingress preserves the target graph"),
		MakeInventorySignature(TargetInventory),
		TargetBefore);
	TestEqual(
		TEXT("Rejected missing-handle ingress preserves the source graph"),
		MakeInventorySignature(SourceInventory),
		SourceBefore);
	TestEqual(
		TEXT("Rejected missing-handle ingress preserves target revision"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBefore);
	TestEqual(
		TEXT("Rejected missing-handle ingress preserves source revision"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore);
	TestEqual(
		TEXT("Rejected missing-handle ingress preserves target mutation epoch"),
		TargetInventory->GetMutationEpoch(),
		TargetEpochBefore);
	TestEqual(
		TEXT("Rejected missing-handle ingress preserves source mutation epoch"),
		SourceInventory->GetMutationEpoch(),
		SourceEpochBefore);
	TestFalse(
		TEXT("The detached instance never becomes managed through missing-handle ingress"),
		TargetInventory->ContainsItemInstance(DetachedTargetItem));
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
	TestTrue(
		TEXT("Canonical consume detaches the first pickup setup item"),
		ConsumeWholeItem(SetupInventory, FirstSetupItem));
	TestTrue(
		TEXT("Canonical consume detaches the second pickup setup item"),
		ConsumeWholeItem(SetupInventory, SecondSetupItem));
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
	FRpgInventoryBlueprintMutationSurfaceContractTest,
	"SurvivalRpg.Inventory.Transaction.BlueprintMutationSurfaceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryBlueprintMutationSurfaceContractTest::RunTest(
	const FString& Parameters)
{
	static const FName RetiredOrNativeOnlyFunctionNames[] = {
		TEXT("CanAddItemInstance"),
		TEXT("CanAddItemInstanceToPlacement"),
		TEXT("AddItemDefinition"),
		TEXT("AddItemDefinitionToPlacement"),
		TEXT("AddItemInstance"),
		TEXT("AddItemInstanceWithStack"),
		TEXT("AddItemInstanceWithStackToPlacement"),
		TEXT("AddStackToExistingItem"),
		TEXT("RemoveItemInstance"),
		TEXT("RemoveItemInstanceStack"),
		TEXT("MoveInventoryEntryToPlacement"),
		TEXT("CanMoveInventoryEntryToPlacement"),
		TEXT("PlanInventoryMutation"),
		TEXT("ExecuteInventoryMutation"),
		TEXT("ExecuteCrossInventoryTransfer"),
	};
	for (const FName FunctionName : RetiredOrNativeOnlyFunctionNames)
	{
		TestNull(
			*FString::Printf(TEXT("%s is absent from the Blueprint surface"), *FunctionName.ToString()),
			URpgInventoryManagerComponent::StaticClass()->FindFunctionByName(FunctionName));
	}

	static const FName RemovedPersistenceFunctionNames[] = {
		TEXT("ExportInventorySnapshot"),
		TEXT("ImportInventorySnapshot"),
		TEXT("ImportInventoryGraph"),
	};
	for (const FName FunctionName : RemovedPersistenceFunctionNames)
	{
		TestNull(
			*FString::Printf(TEXT("%s is no longer exposed to Blueprint"), *FunctionName.ToString()),
			URpgInventoryManagerComponent::StaticClass()->FindFunctionByName(FunctionName));
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

	static const FName RetiredUiFunctionNames[] = {
		TEXT("RequestInventoryMutation"),
		TEXT("RequestAssignItemToEquipmentSlot"),
		TEXT("RequestClearEquipmentSlot"),
		TEXT("RequestTransferItemStack"),
		TEXT("RequestTransferItemStackToPlacement"),
		TEXT("RequestMoveItemToInventorySlotAddress"),
		TEXT("RequestEquipSlotContainerItem"),
		TEXT("RequestUnequipSlotContainerItem"),
		TEXT("RequestBindActionBarToInventorySlot"),
		TEXT("RequestBindActionBarToCarrySlot"),
		TEXT("RequestClearActionBarCarryBinding"),
		TEXT("RequestClearActionBarConsumableBinding"),
		TEXT("RequestSplitItemStack"),
		TEXT("RequestEquipInventoryItem"),
		TEXT("RequestUnequipInventoryItemToContentSlot"),
		TEXT("RequestDropInventoryItem"),
		TEXT("RequestStoreItemInstanceInBase"),
		TEXT("RequestTakeItemInstanceFromBase"),
	};
	for (const FName FunctionName : RetiredUiFunctionNames)
	{
		TestNull(
			*FString::Printf(
				TEXT("%s is retired from the Blueprint UI facade"),
				*FunctionName.ToString()),
			URpgInventoryUiActionComponent::StaticClass()->
				FindFunctionByName(FunctionName));
	}
	TestNull(
		TEXT("RequestMoveInventoryEntryToPlacement is retired from the UI facade"),
		URpgInventoryUiActionComponent::StaticClass()->FindFunctionByName(
			TEXT("RequestMoveInventoryEntryToPlacement")));
	TestNull(
		TEXT("TransferItemFromInventory is retired from dropped actors"),
		ARpgDroppedInventoryActor::StaticClass()->FindFunctionByName(
			TEXT("TransferItemFromInventory")));

	static const FName CanonicalUiFunctionNames[] = {
		GET_FUNCTION_NAME_CHECKED(URpgInventoryUiActionComponent, RequestMoveInventoryItem),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryUiActionComponent, RequestTransferInventoryItem),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryUiActionComponent, RequestUseInventoryItemById),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryUiActionComponent, RequestApplyInventoryEquipmentIntent),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryUiActionComponent, RequestQuickTransferItem),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryUiActionComponent, RequestSplitItemStackById),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryUiActionComponent, RequestDropInventoryItemById),
		GET_FUNCTION_NAME_CHECKED(URpgInventoryUiActionComponent, RequestMutateQuickAccessBinding),
	};
	for (const FName FunctionName : CanonicalUiFunctionNames)
	{
		const UFunction* Function =
			URpgInventoryUiActionComponent::StaticClass()->FindFunctionByName(FunctionName);
		if (TestNotNull(
				*FString::Printf(TEXT("%s is reflected as the typed UI gateway"), *FunctionName.ToString()),
				Function))
		{
			TestFalse(
				*FString::Printf(TEXT("%s is not deprecated"), *FunctionName.ToString()),
				Function->HasMetaData(TEXT("DeprecatedFunction")));
			TestTrue(
				*FString::Printf(TEXT("%s is an owning-client Server RPC"), *FunctionName.ToString()),
				Function->HasAnyFunctionFlags(FUNC_NetServer));
		}
	}

	const UFunction* SplitFunction =
		URpgInventoryUiActionComponent::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				URpgInventoryUiActionComponent,
				RequestSplitItemStackById));
	if (TestNotNull(
			TEXT("The canonical split gateway remains reflected"),
			SplitFunction))
	{
		const FStructProperty* SplitRequestProperty =
			FindFProperty<FStructProperty>(
				SplitFunction,
				TEXT("Request"));
		if (TestNotNull(
				TEXT("The canonical split gateway accepts one typed request"),
				SplitRequestProperty))
		{
			TestTrue(
				TEXT("The split request parameter uses FRpgInventorySplitRequest"),
				SplitRequestProperty->Struct ==
					FRpgInventorySplitRequest::StaticStruct());
			TestTrue(
				TEXT("The typed split request is an RPC parameter"),
				SplitRequestProperty->HasAnyPropertyFlags(CPF_Parm));
		}
	}

	static const FName HiddenLoadoutFunctionNames[] = {
		FName(TEXT("CanAssignItemToEquipmentSlot")),
		FName(TEXT("AssignItemToEquipmentSlot")),
		FName(TEXT("ClearEquipmentSlot")),
		FName(TEXT("ClearItemFromAllEquipmentSlots")),
		FName(TEXT("CanRemoveItemFromLoadout")),
		FName(TEXT("GetLoadoutSlots")),
		FName(TEXT("CanActivateItemInEquipmentSlot")),
		FName(TEXT("ActivateMainHandItem")),
		FName(TEXT("ActivateOffHandItem")),
		FName(TEXT("SetMainHandItemActive")),
		FName(TEXT("SetOffHandItemActive")),
		FName(TEXT("ClearActiveMainHand")),
		FName(TEXT("ClearActiveHands")),
		FName(TEXT("ClearActiveOffHand")),
		FName(TEXT("UnequipLoadoutFromCurrentPawn")),
		FName(TEXT("RefreshEquipmentLoadoutOnCurrentPawn")),
		FName(TEXT("DetachRuntimeEquipmentFromCurrentPawn")),
		FName(TEXT("ReconcileRuntimeEquipmentOnCurrentPawn")),
		FName(TEXT("RefreshEquipmentLoadState")),
		FName(TEXT("ReconcileEquipmentLoadFromInventory")),
		FName(TEXT("ReconcilePhysicalEquipmentFromInventory")),
		FName(TEXT("ExportEquipmentSelection")),
		FName(TEXT("RestoreEquipmentSelection")),
	};
	for (const FName FunctionName : HiddenLoadoutFunctionNames)
	{
		TestNull(
			*FString::Printf(
				TEXT("%s is native-only and absent from the Blueprint mutation surface"),
				*FunctionName.ToString()),
			URpgEquipmentLoadoutComponent::StaticClass()->
				FindFunctionByName(FunctionName));
	}

	for (TFieldIterator<UFunction> FunctionIt(
			 URpgEquipmentLoadoutComponent::StaticClass(),
			 EFieldIteratorFlags::ExcludeSuper);
		 FunctionIt;
		 ++FunctionIt)
	{
		TestFalse(
			*FString::Printf(
				TEXT("Loadout function %s is never a player-facing Server RPC"),
				*FunctionIt->GetName()),
			FunctionIt->HasAnyFunctionFlags(FUNC_NetServer));
	}

	static const FName PureLoadoutGetterNames[] = {
		GET_FUNCTION_NAME_CHECKED(
			URpgEquipmentLoadoutComponent,
			GetItemInEquipmentSlot),
		GET_FUNCTION_NAME_CHECKED(
			URpgEquipmentLoadoutComponent,
			GetEquipmentLoadWeight),
		GET_FUNCTION_NAME_CHECKED(
			URpgEquipmentLoadoutComponent,
			GetEquipmentLoadTier),
		GET_FUNCTION_NAME_CHECKED(
			URpgEquipmentLoadoutComponent,
			GetEquipmentLoadTierTag),
		GET_FUNCTION_NAME_CHECKED(
			URpgEquipmentLoadoutComponent,
			GetDodgeProfileForCurrentLoad),
	};
	for (const FName FunctionName : PureLoadoutGetterNames)
	{
		const UFunction* Function =
			URpgEquipmentLoadoutComponent::StaticClass()->
				FindFunctionByName(FunctionName);
		if (TestNotNull(
				*FString::Printf(
					TEXT("%s remains reflected as read-only presentation data"),
					*FunctionName.ToString()),
				Function))
		{
			TestTrue(
				*FString::Printf(
					TEXT("%s is BlueprintPure"),
					*FunctionName.ToString()),
				Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
		}
	}

	static const FName CanonicalEquipmentGatewayNames[] = {
		GET_FUNCTION_NAME_CHECKED(
			URpgInventoryUiActionComponent,
			RequestApplyInventoryEquipmentIntent),
		GET_FUNCTION_NAME_CHECKED(
			URpgInventoryUiActionComponent,
			RequestActivateCarrySlot),
		GET_FUNCTION_NAME_CHECKED(
			URpgInventoryUiActionComponent,
			RequestClearActiveHands),
	};
	for (const FName FunctionName : CanonicalEquipmentGatewayNames)
	{
		const UFunction* Function =
			URpgInventoryUiActionComponent::StaticClass()->
				FindFunctionByName(FunctionName);
		if (TestNotNull(
				*FString::Printf(
					TEXT("%s remains the reflected equipment gateway"),
					*FunctionName.ToString()),
				Function))
		{
			TestTrue(
				*FString::Printf(
					TEXT("%s is an owning-client Server RPC"),
					*FunctionName.ToString()),
				Function->HasAnyFunctionFlags(FUNC_NetServer));
			TestFalse(
				*FString::Printf(
					TEXT("%s is canonical rather than deprecated"),
					*FunctionName.ToString()),
				Function->HasMetaData(TEXT("DeprecatedFunction")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentReplicationConditionContractTest,
	"SurvivalRpg.Inventory.Intent.Equip.ReplicationConditionsSeparatePrivateSelectionFromPawnRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentReplicationConditionContractTest::RunTest(
	const FString& Parameters)
{
	auto VerifyLifetimeCondition =
		[this](
			const UObject* Object,
			const TArray<FLifetimeProperty>& LifetimeProperties,
			FName PropertyName,
			ELifetimeCondition ExpectedCondition)
		{
			if (!Object)
			{
				AddError(TEXT("A replication-contract CDO is missing"));
				return false;
			}

			const FProperty* Property = FindFProperty<FProperty>(
				Object->GetClass(),
				PropertyName);
			if (!TestNotNull(
					*FString::Printf(
						TEXT("Replicated property %s.%s exists"),
						*Object->GetClass()->GetName(),
						*PropertyName.ToString()),
					Property))
			{
				return false;
			}
			TestTrue(
				*FString::Printf(
					TEXT("%s.%s carries CPF_Net"),
					*Object->GetClass()->GetName(),
					*PropertyName.ToString()),
				Property->HasAnyPropertyFlags(CPF_Net));

			const FLifetimeProperty* LifetimeProperty =
				LifetimeProperties.FindByPredicate(
					[Property](const FLifetimeProperty& Candidate)
					{
						return Candidate.RepIndex == Property->RepIndex;
					});
			if (!TestNotNull(
					*FString::Printf(
						TEXT("%s.%s has a lifetime replication record"),
						*Object->GetClass()->GetName(),
						*PropertyName.ToString()),
					LifetimeProperty))
			{
				return false;
			}

			TestEqual(
				*FString::Printf(
					TEXT("%s.%s uses the intended replication condition"),
					*Object->GetClass()->GetName(),
					*PropertyName.ToString()),
				static_cast<int32>(LifetimeProperty->Condition),
				static_cast<int32>(ExpectedCondition));
			return true;
		};

	const URpgEquipmentLoadoutComponent* LoadoutCDO =
		GetDefault<URpgEquipmentLoadoutComponent>();
	const URpgEquipmentManagerComponent* EquipmentManagerCDO =
		GetDefault<URpgEquipmentManagerComponent>();
	LoadoutCDO->GetClass()->SetUpRuntimeReplicationData();
	EquipmentManagerCDO->GetClass()->SetUpRuntimeReplicationData();
	TArray<FLifetimeProperty> LoadoutLifetimeProperties;
	TArray<FLifetimeProperty> EquipmentManagerLifetimeProperties;
	LoadoutCDO->GetLifetimeReplicatedProps(LoadoutLifetimeProperties);
	EquipmentManagerCDO->GetLifetimeReplicatedProps(
		EquipmentManagerLifetimeProperties);
	bool bContractResolved = true;
	bContractResolved &= VerifyLifetimeCondition(
		LoadoutCDO,
		LoadoutLifetimeProperties,
		FName(TEXT("Slots")),
		COND_OwnerOnly);
	bContractResolved &= VerifyLifetimeCondition(
		LoadoutCDO,
		LoadoutLifetimeProperties,
		FName(TEXT("RememberedOffhands")),
		COND_OwnerOnly);
	bContractResolved &= VerifyLifetimeCondition(
		LoadoutCDO,
		LoadoutLifetimeProperties,
		FName(TEXT("CurrentEquipmentLoadWeight")),
		COND_OwnerOnly);
	bContractResolved &= VerifyLifetimeCondition(
		LoadoutCDO,
		LoadoutLifetimeProperties,
		FName(TEXT("CurrentEquipmentLoadTier")),
		COND_OwnerOnly);
	bContractResolved &= VerifyLifetimeCondition(
		EquipmentManagerCDO,
		EquipmentManagerLifetimeProperties,
		FName(TEXT("EquipmentList")),
		COND_None);
	return bContractResolved;
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
	URpgInventoryManagerComponent* CapacityTarget =
		TestWorld.CreateInventory(TEXT("NestedCapacityTarget"));
	if (!TestNotNull(TEXT("Capacity validation target exists"), CapacityTarget))
	{
		return false;
	}

	CapacityTarget->SetCapacityMode(ERpgInventoryCapacityMode::FixedEntries);
	CapacityTarget->SetFixedMaxEntries(RootGraph.Items.Num());
	FRpgInventoryMutationResult ExactCapacityResult;
	TestTrue(
		TEXT("An import that exactly fills MaxEntries is accepted"),
		CapacityTarget->RestoreInventoryGraph(
			RootGraph,
			ExactCapacityResult));
	TestEqual(
		TEXT("The exact-capacity import reports success"),
		ExactCapacityResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The exact-capacity import restores every row"),
		CapacityTarget->GetUsedEntryCount(),
		RootGraph.Items.Num());

	URpgInventoryManagerComponent* CapacityOverflowTarget =
		TestWorld.CreateInventory(TEXT("NestedCapacityOverflowTarget"));
	if (!TestNotNull(
			TEXT("Capacity overflow validation target exists"),
			CapacityOverflowTarget))
	{
		return false;
	}

	CapacityOverflowTarget->SetCapacityMode(
		ERpgInventoryCapacityMode::FixedEntries);
	CapacityOverflowTarget->SetFixedMaxEntries(RootGraph.Items.Num() - 1);
	const FString BeforeCapacityOverflow =
		MakeStrictInventorySignature(CapacityOverflowTarget);
	const int32 RevisionBeforeCapacityOverflow =
		CapacityOverflowTarget->GetInventoryRevision();
	const uint64 EpochBeforeCapacityOverflow =
		CapacityOverflowTarget->GetMutationEpoch();
	FRpgInventoryMutationResult CapacityOverflowResult;
	TestFalse(
		TEXT("An import one entry above MaxEntries is rejected"),
		CapacityOverflowTarget->RestoreInventoryGraph(
			RootGraph,
			CapacityOverflowResult));
	TestEqual(
		TEXT("Capacity overflow reports NoSpace"),
		CapacityOverflowResult.Code,
		ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(
		TEXT("Capacity rejection preserves the complete empty target graph"),
		MakeStrictInventorySignature(CapacityOverflowTarget),
		BeforeCapacityOverflow);
	TestEqual(
		TEXT("Capacity rejection does not advance inventory revision"),
		CapacityOverflowTarget->GetInventoryRevision(),
		RevisionBeforeCapacityOverflow);
	TestEqual(
		TEXT("Capacity rejection does not advance mutation epoch"),
		CapacityOverflowTarget->GetMutationEpoch(),
		EpochBeforeCapacityOverflow);
	TestEqual(
		TEXT("Capacity rejection applies no rows"),
		CapacityOverflowResult.AppliedQuantity,
		0);
	TestTrue(
		TEXT("Capacity rejection exposes no deltas"),
		CapacityOverflowResult.Deltas.IsEmpty());

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

	URpgInventoryManagerComponent* DepthTarget =
		TestWorld.CreateInventory(TEXT("DepthTarget"));
	if (!TestNotNull(TEXT("Nested depth validation target exists"), DepthTarget))
	{
		return false;
	}
	FRpgInventoryMutationResult DepthImportResult;
	if (!TestTrue(
			TEXT("Four item-owned levels are accepted"),
			DepthTarget->RestoreInventoryGraph(
				DepthFourGraph,
				DepthImportResult)))
	{
		return false;
	}
	TestEqual(TEXT("Accepted depth-four import reports success"), DepthImportResult.Code, ERpgInventoryMutationResultCode::Success);
	FRpgInventoryEntryView DepthFourView;
	TestTrue(TEXT("Depth-four item keeps its persistent identity"), GetEntryView(DepthTarget, DepthFourItem->GetItemId(), DepthFourView));
	TestEqual(TEXT("Imported item remains at depth four"), DepthFourView.Placement.GetContainerHandle().Depth, static_cast<uint8>(4));
	URpgInventoryItemInstance* RuntimeStateSentinel =
		DepthTarget->FindItemById(DepthFiveProbe->GetItemId());
	if (!TestNotNull(
			TEXT("Rejected-restore runtime-state sentinel exists"),
			RuntimeStateSentinel))
	{
		return false;
	}
	RuntimeStateSentinel->AddStatTagStack(
		RpgGameplayTags::Ability_Attack_Basic,
		3);
	TestEqual(
		TEXT("Runtime-state sentinel starts with its non-default tag stack"),
		RuntimeStateSentinel->GetStatTagStackCount(
			RpgGameplayTags::Ability_Attack_Basic),
		3);
	const FRpgInventoryItemId RuntimeStateSentinelId =
		RuntimeStateSentinel->GetItemId();
	const FString BeforeRejectedImports =
		MakeStrictInventorySignature(DepthTarget);
	const int32 RevisionBeforeRejectedImports =
		DepthTarget->GetInventoryRevision();
	const uint64 EpochBeforeRejectedImports =
		DepthTarget->GetMutationEpoch();
	auto VerifyRejectedRestoreIsAtomic =
		[this,
		 DepthTarget,
		 RuntimeStateSentinel,
		 RuntimeStateSentinelId,
		 &BeforeRejectedImports,
		 RevisionBeforeRejectedImports,
		 EpochBeforeRejectedImports](
			const TCHAR* RejectionName,
			const FRpgInventoryMutationResult& RejectionResult)
		{
			TestEqual(
				*FString::Printf(
					TEXT("%s preserves the complete live graph"),
					RejectionName),
				MakeStrictInventorySignature(DepthTarget),
				BeforeRejectedImports);
			TestEqual(
				*FString::Printf(
					TEXT("%s preserves the inventory revision"),
					RejectionName),
				DepthTarget->GetInventoryRevision(),
				RevisionBeforeRejectedImports);
			TestEqual(
				*FString::Printf(
					TEXT("%s preserves the mutation epoch"),
					RejectionName),
				DepthTarget->GetMutationEpoch(),
				EpochBeforeRejectedImports);
			URpgInventoryItemInstance* CurrentRuntimeStateSentinel =
				DepthTarget->FindItemById(RuntimeStateSentinelId);
			TestEqual(
				*FString::Printf(
					TEXT("%s preserves the runtime-state sentinel UObject"),
					RejectionName),
				CurrentRuntimeStateSentinel,
				RuntimeStateSentinel);
			if (CurrentRuntimeStateSentinel)
			{
				TestEqual(
					*FString::Printf(
						TEXT("%s preserves the runtime-state sentinel payload"),
						RejectionName),
					CurrentRuntimeStateSentinel->GetStatTagStackCount(
						RpgGameplayTags::Ability_Attack_Basic),
					3);
			}
			TestEqual(
				*FString::Printf(
					TEXT("%s applies no rows"),
					RejectionName),
				RejectionResult.AppliedQuantity,
				0);
			TestTrue(
				*FString::Printf(
					TEXT("%s exposes no deltas"),
					RejectionName),
				RejectionResult.Deltas.IsEmpty());
		};

	FRpgInventoryGraphSaveData DepthFiveGraph = DepthFourGraph;
	TestTrue(
		TEXT("Depth-five probe DTO was constructed"),
		SetSavedPlacement(
			DepthFiveGraph,
			DepthFiveProbe->GetItemId(),
			FRpgInventoryContainerHandle::MakeItemOwned(DepthFourItem->GetItemId(), BagContainerId, 5)));
	FRpgInventoryMutationResult DepthFiveResult;
	TestFalse(TEXT("A fifth item-owned level is rejected"), DepthTarget->RestoreInventoryGraph(DepthFiveGraph, DepthFiveResult));
	TestEqual(
		TEXT("Depth-five rejection reports MaxDepthExceeded"),
		DepthFiveResult.Code,
		ERpgInventoryMutationResultCode::MaxDepthExceeded);
	VerifyRejectedRestoreIsAtomic(
		TEXT("Depth-five rejection"),
		DepthFiveResult);

	FRpgInventoryGraphSaveData OrphanGraph = RootGraph;
	TestTrue(
		TEXT("Missing-owner DTO was constructed"),
		SetSavedPlacement(
			OrphanGraph,
			DepthFiveProbe->GetItemId(),
			FRpgInventoryContainerHandle::MakeItemOwned(
				FRpgInventoryItemId::NewId(),
				BagContainerId,
				1)));
	FRpgInventoryMutationResult OrphanResult;
	TestFalse(
		TEXT("An item-owned container with a missing owner is rejected"),
		DepthTarget->RestoreInventoryGraph(OrphanGraph, OrphanResult));
	TestEqual(
		TEXT("A missing owner reports InvalidContainer"),
		OrphanResult.Code,
		ERpgInventoryMutationResultCode::InvalidContainer);
	VerifyRejectedRestoreIsAtomic(
		TEXT("Missing-owner rejection"),
		OrphanResult);

	FRpgInventoryGraphSaveData UnknownLocalContainerGraph = RootGraph;
	TestTrue(
		TEXT("Unknown local-container DTO was constructed"),
		SetSavedPlacement(
			UnknownLocalContainerGraph,
			DepthFourItem->GetItemId(),
			FRpgInventoryContainerHandle::MakeItemOwned(
				Bags[0]->GetItemId(),
				FName(TEXT("Missing")),
				1)));
	FRpgInventoryMutationResult UnknownLocalContainerResult;
	TestFalse(
		TEXT("An owner that does not expose the requested local container is rejected"),
		DepthTarget->RestoreInventoryGraph(
			UnknownLocalContainerGraph,
			UnknownLocalContainerResult));
	TestEqual(
		TEXT("An unknown local container reports InvalidContainer"),
		UnknownLocalContainerResult.Code,
		ERpgInventoryMutationResultCode::InvalidContainer);
	VerifyRejectedRestoreIsAtomic(
		TEXT("Unknown-container rejection"),
		UnknownLocalContainerResult);

	FRpgInventoryGraphSaveData DepthMismatchGraph = RootGraph;
	TestTrue(
		TEXT("Parent-child depth-mismatch DTO was constructed"),
		SetSavedPlacement(
			DepthMismatchGraph,
			Bags[1]->GetItemId(),
			FRpgInventoryContainerHandle::MakeItemOwned(
				Bags[0]->GetItemId(),
				BagContainerId,
				2)));
	FRpgInventoryMutationResult DepthMismatchResult;
	TestFalse(
		TEXT("A child whose depth is not parent depth plus one is rejected"),
		DepthTarget->RestoreInventoryGraph(
			DepthMismatchGraph,
			DepthMismatchResult));
	TestEqual(
		TEXT("A parent-child depth mismatch reports InvalidContainer"),
		DepthMismatchResult.Code,
		ERpgInventoryMutationResultCode::InvalidContainer);
	VerifyRejectedRestoreIsAtomic(
		TEXT("Depth-mismatch rejection"),
		DepthMismatchResult);

	FRpgInventoryGraphSaveData IntMaxXGraph = RootGraph;
	TestTrue(
		TEXT("INT_MAX-X DTO was constructed"),
		SetSavedPlacement(
			IntMaxXGraph,
			DepthFiveProbe->GetItemId(),
			Root,
			MAX_int32,
			0));
	FRpgInventoryMutationResult IntMaxXResult;
	TestFalse(
		TEXT("An INT_MAX X coordinate is rejected without overflow"),
		DepthTarget->RestoreInventoryGraph(
			IntMaxXGraph,
			IntMaxXResult));
	TestEqual(
		TEXT("An INT_MAX X coordinate reports OutOfBounds"),
		IntMaxXResult.Code,
		ERpgInventoryMutationResultCode::OutOfBounds);
	VerifyRejectedRestoreIsAtomic(
		TEXT("INT_MAX-X rejection"),
		IntMaxXResult);

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
	TestFalse(TEXT("Mutually owning containers are rejected"), DepthTarget->RestoreInventoryGraph(CycleGraph, CycleResult));
	TestEqual(TEXT("Cycle rejection is distinguishable for UI/save diagnostics"), CycleResult.Code, ERpgInventoryMutationResultCode::CycleDetected);
	VerifyRejectedRestoreIsAtomic(
		TEXT("Cycle rejection"),
		CycleResult);

	FRpgInventoryGraphSaveData DuplicateGraph = RootGraph;
	FRpgInventorySavedItem DuplicateRow = DuplicateGraph.Items[0];
	DuplicateRow.Placement.X = 9;
	DuplicateGraph.Items.Add(DuplicateRow);
	FRpgInventoryMutationResult DuplicateResult;
	TestFalse(TEXT("Duplicate persistent item identities are rejected"), DepthTarget->RestoreInventoryGraph(DuplicateGraph, DuplicateResult));
	TestEqual(TEXT("Duplicate id has an explicit result code"), DuplicateResult.Code, ERpgInventoryMutationResultCode::DuplicateItemId);
	VerifyRejectedRestoreIsAtomic(
		TEXT("Duplicate-id rejection"),
		DuplicateResult);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryDuplicateEntryIdTransferTest,
	"SurvivalRpg.Inventory.Graph.DuplicateEntryIdTransferIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryDuplicateEntryIdTransferTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("DuplicateEntryIdSource"));
	URpgInventoryManagerComponent* TargetInventory =
		TestWorld.CreateInventory(TEXT("DuplicateEntryIdTarget"));
	if (!TestNotNull(TEXT("Duplicate-entry source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("Duplicate-entry target inventory exists"), TargetInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	URpgInventoryItemInstance* FirstItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 0, 0));
	URpgInventoryItemInstance* SecondItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Root, 1, 0));
	if (!TestNotNull(TEXT("First valid source row exists"), FirstItem) ||
		!TestNotNull(TEXT("Second valid source row exists"), SecondItem))
	{
		return false;
	}

	FRpgInventoryEntryView FirstEntry;
	FRpgInventoryEntryView SecondEntry;
	if (!TestTrue(
			TEXT("First source row exposes a complete snapshot"),
			GetEntryView(SourceInventory, FirstItem->GetItemId(), FirstEntry)) ||
		!TestTrue(
			TEXT("Second source row exposes a complete snapshot"),
			GetEntryView(SourceInventory, SecondItem->GetItemId(), SecondEntry)))
	{
		return false;
	}
	TestNotEqual(
		TEXT("The valid fixture starts with distinct entry ids"),
		FirstEntry.EntryId,
		SecondEntry.EntryId);
	if (!TestTrue(
			TEXT("Reflection duplicates the second row's EntryId"),
			CopyInventoryEntryIdForTest(SourceInventory, 0, 1)))
	{
		return false;
	}

	const int32 SourceRevisionBefore = SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBefore = TargetInventory->GetInventoryRevision();
	const uint64 SourceEpochBefore = SourceInventory->GetMutationEpoch();
	const uint64 TargetEpochBefore = TargetInventory->GetMutationEpoch();
	const FString SourceBefore = MakeStrictInventorySignature(SourceInventory);
	const FString TargetBefore = MakeStrictInventorySignature(TargetInventory);
	const FRpgInventoryItemId FirstItemId = FirstItem->GetItemId();
	const FRpgInventoryItemId SecondItemId = SecondItem->GetItemId();

	FRpgInventoryMutationRequest TransferRequest;
	TransferRequest.Operation = ERpgInventoryMutationOperation::Transfer;
	TransferRequest.ItemId = FirstEntry.ItemId;
	TransferRequest.ExpectedEntryId = FirstEntry.EntryId;
	TransferRequest.Source = Root;
	TransferRequest.ExpectedSourcePlacement = FirstEntry.Placement;
	TransferRequest.ExpectedSourceQuantity = FirstEntry.StackCount;
	TransferRequest.Target = Root;
	TransferRequest.TargetPlacement = MakePlacement(Root, 0, 0);
	TransferRequest.Quantity = FirstEntry.StackCount;
	TransferRequest.RequestId = FGuid::NewGuid();

	int32 ChangeMessageCount = 0;
	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(TestWorld.GetTestWorld());
	const FGameplayMessageListenerHandle ListenerHandle =
		MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
			FGameplayTag::RequestGameplayTag(
				TEXT("Rpg.Inventory.Message.StackChanged")),
			[&ChangeMessageCount](
				FGameplayTag,
				const FRpgInventoryChangeMessage&)
			{
				++ChangeMessageCount;
			});
	const FRpgInventoryMutationResult Result =
		SourceInventory->ExecuteCrossInventoryTransfer(
			TargetInventory,
			TransferRequest,
			false);
	MessageSubsystem.UnregisterListener(ListenerHandle);
	TestEqual(
		TEXT("A corrupt source graph reports DuplicateEntryId exactly"),
		Result.Code,
		ERpgInventoryMutationResultCode::DuplicateEntryId);
	TestEqual(
		TEXT("Duplicate-entry rejection applies no quantity"),
		Result.AppliedQuantity,
		0);
	TestTrue(
		TEXT("Duplicate-entry rejection exposes no deltas"),
		Result.Deltas.IsEmpty());
	TestEqual(
		TEXT("Duplicate-entry rejection preserves the source revision"),
		SourceInventory->GetInventoryRevision(),
		SourceRevisionBefore);
	TestEqual(
		TEXT("Duplicate-entry rejection preserves the target revision"),
		TargetInventory->GetInventoryRevision(),
		TargetRevisionBefore);
	TestEqual(
		TEXT("Duplicate-entry rejection preserves the source mutation epoch"),
		SourceInventory->GetMutationEpoch(),
		SourceEpochBefore);
	TestEqual(
		TEXT("Duplicate-entry rejection preserves the target mutation epoch"),
		TargetInventory->GetMutationEpoch(),
		TargetEpochBefore);
	TestEqual(
		TEXT("Duplicate-entry rejection emits no inventory change messages"),
		ChangeMessageCount,
		0);
	TestEqual(
		TEXT("Duplicate-entry rejection preserves the complete corrupt source graph"),
		MakeStrictInventorySignature(SourceInventory),
		SourceBefore);
	TestEqual(
		TEXT("Duplicate-entry rejection preserves the complete target graph"),
		MakeStrictInventorySignature(TargetInventory),
		TargetBefore);
	TestEqual(
		TEXT("The first source item keeps its exact UObject"),
		SourceInventory->FindItemById(FirstItemId),
		FirstItem);
	TestEqual(
		TEXT("The second source item keeps its exact UObject"),
		SourceInventory->FindItemById(SecondItemId),
		SecondItem);
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
	FRpgInventoryEntryView SourceEntry;
	if (!TestTrue(
		TEXT("The pickup source exposes a complete snapshot"),
		GetEntryView(SourceInventory, SourceItemId, SourceEntry)))
	{
		return false;
	}

	FRpgInventoryMutationRequest PickupRequest;
	PickupRequest.Operation = ERpgInventoryMutationOperation::Pickup;
	PickupRequest.ItemId = SourceItemId;
	PickupRequest.ExpectedEntryId = SourceEntry.EntryId;
	PickupRequest.Source = Root;
	PickupRequest.ExpectedSourcePlacement = SourceEntry.Placement;
	PickupRequest.ExpectedSourceQuantity = SourceEntry.StackCount;
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
	FRpgInventoryEntryView SourceBagEntry;
	if (!TestTrue(
		TEXT("The nested provider exposes a complete source snapshot"),
		GetEntryView(SourceInventory, BagItemId, SourceBagEntry)))
	{
		return false;
	}

	FRpgInventoryMutationRequest TransferRequest;
	TransferRequest.Operation = ERpgInventoryMutationOperation::Transfer;
	TransferRequest.ItemId = BagItemId;
	TransferRequest.ExpectedEntryId = SourceBagEntry.EntryId;
	TransferRequest.Source = Root;
	TransferRequest.ExpectedSourcePlacement = SourceBagEntry.Placement;
	TransferRequest.ExpectedSourceQuantity = SourceBagEntry.StackCount;
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
	TestTrue(TEXT("A fully validated graph restores atomically"), RestoredInventory->RestoreInventoryGraph(ExportedGraph, ImportResult));
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
			SecondRestoredInventory->RestoreInventoryGraph(ReExportedGraph, SecondImportResult));
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
			LegacyRestoredInventory->RestoreInventoryGraph(LegacyV1Graph, LegacyImportResult));
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
	FRpgInventoryConsumeFacadePreflightCommitParityAndPurityTest,
	"SurvivalRpg.Inventory.Transaction.ConsumeFacadePreflightCommitParityAndPurity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryConsumeFacadePreflightCommitParityAndPurityTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(TEXT("ConsumeFacadeInventory"));
	if (!TestNotNull(TEXT("The consume-facade inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Root = MakeStorageHandle();
	const FRpgInventoryGridPlacement InitialPlacement =
		MakePlacement(Root, 0, 0);
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			3,
			InitialPlacement);
	if (!TestNotNull(TEXT("The consume-facade stack exists"), Item))
	{
		return false;
	}

	const FRpgInventoryItemId ItemId = Item->GetItemId();
	const FString GraphBeforePreflight =
		MakeInventorySignature(Inventory);
	const int32 RevisionBeforePreflight =
		Inventory->GetInventoryRevision();
	const uint64 EpochBeforePreflight =
		Inventory->GetMutationEpoch();

	TestTrue(
		TEXT("Consume preflight accepts an available partial quantity"),
		Inventory->CanConsumeItemById(ItemId, 2));
	TestTrue(
		TEXT("Repeated consume preflight remains deterministic"),
		Inventory->CanConsumeItemById(ItemId, 2));
	TestFalse(
		TEXT("Consume preflight rejects an oversized quantity"),
		Inventory->CanConsumeItemById(ItemId, 4));
	TestFalse(
		TEXT("Consume preflight rejects a zero quantity"),
		Inventory->CanConsumeItemById(ItemId, 0));
	TestFalse(
		TEXT("Consume preflight rejects an invalid item identity"),
		Inventory->CanConsumeItemById(FRpgInventoryItemId(), 1));
	TestEqual(
		TEXT("Consume preflights preserve the complete graph"),
		MakeInventorySignature(Inventory),
		GraphBeforePreflight);
	TestEqual(
		TEXT("Consume preflights preserve inventory revision"),
		Inventory->GetInventoryRevision(),
		RevisionBeforePreflight);
	TestEqual(
		TEXT("Consume preflights preserve mutation epoch"),
		Inventory->GetMutationEpoch(),
		EpochBeforePreflight);
	TestEqual(
		TEXT("Consume preflights preserve the concrete stack quantity"),
		Inventory->GetItemStackCount(Item),
		3);

	const FRpgInventoryMutationResult Result =
		Inventory->ConsumeItemById(ItemId, 2);
	TestEqual(
		TEXT("The direct consume facade commits successfully"),
		Result.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The direct consume facade reports Consume"),
		Result.Operation,
		ERpgInventoryMutationOperation::Consume);
	TestTrue(
		TEXT("The direct consume facade assigns a request id"),
		Result.RequestId.IsValid());
	TestEqual(
		TEXT("The direct consume facade preserves requested quantity"),
		Result.RequestedQuantity,
		2);
	TestEqual(
		TEXT("The direct consume facade applies the complete requested quantity"),
		Result.AppliedQuantity,
		2);
	TestEqual(
		TEXT("A partial direct consume emits one authoritative delta"),
		Result.Deltas.Num(),
		1);
	if (Result.Deltas.Num() == 1)
	{
		const FRpgInventoryMutationDelta& Delta = Result.Deltas[0];
		TestEqual(
			TEXT("The direct consume emits a stack-change delta"),
			Delta.Kind,
			ERpgInventoryMutationDeltaKind::StackChanged);
		TestTrue(
			TEXT("The stack-change delta retains the concrete item identity"),
			Delta.ItemId == ItemId);
		TestEqual(
			TEXT("The stack-change delta records the previous quantity"),
			Delta.PreviousQuantity,
			3);
		TestEqual(
			TEXT("The stack-change delta records the remaining quantity"),
			Delta.NewQuantity,
			1);
		TestTrue(
			TEXT("The stack-change delta preserves its source container"),
			Delta.BeforeContainer == Root &&
				Delta.AfterContainer == Root);
		TestTrue(
			TEXT("The stack-change delta preserves its placement"),
			Delta.BeforePlacement == InitialPlacement &&
				Delta.AfterPlacement == InitialPlacement);
	}

	TestEqual(
		TEXT("Partial consume preserves the concrete UObject"),
		Inventory->FindItemById(ItemId),
		Item);
	TestEqual(
		TEXT("Partial consume leaves the exact remaining quantity"),
		Inventory->GetItemStackCount(Item),
		1);
	TestEqual(
		TEXT("Partial consume preserves the single inventory entry"),
		Inventory->GetUsedEntryCount(),
		1);
	TestEqual(
		TEXT("Partial consume advances inventory revision exactly once"),
		Inventory->GetInventoryRevision(),
		RevisionBeforePreflight + 1);
	TestEqual(
		TEXT("Partial consume does not replace the mutation epoch"),
		Inventory->GetMutationEpoch(),
		EpochBeforePreflight);
	TestFalse(
		TEXT("The consumed quantity is no longer available"),
		Inventory->CanConsumeItemById(ItemId, 2));
	TestTrue(
		TEXT("The exact remaining quantity remains consumable"),
		Inventory->CanConsumeItemById(ItemId, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryDroppedActorStaticFallbackTest,
	"SurvivalRpg.Inventory.Drop.NonCanonicalStaticFallbackWinsPartialGraph",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryDroppedActorStaticFallbackTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryTransactionTests;
	FScopedInventoryWorld TestWorld;
	if (!InitializeTest(*this, TestWorld))
	{
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	ARpgInventoryAutomationTestDroppedInventoryActor* DropActor =
		World->SpawnActorDeferred<
			ARpgInventoryAutomationTestDroppedInventoryActor>(
			ARpgInventoryAutomationTestDroppedInventoryActor::
				StaticClass(),
			FTransform::Identity);
	if (!TestNotNull(
			TEXT("The deferred dropped-actor fallback fixture exists"),
			DropActor))
	{
		return false;
	}

	FInventoryPickup StaticFallback;
	FPickupTemplate& StaticTemplate =
		StaticFallback.Templates.AddDefaulted_GetRef();
	StaticTemplate.ItemDef =
		URpgInventoryAutomationTestStackItemDefinition::StaticClass();
	StaticTemplate.StackCount = 4;
	DropActor->SetTestStaticPickupFallback(StaticFallback);

	URpgInventoryManagerComponent* PartialRuntimeInventory =
		DropActor->GetLootInventoryManager();
	URpgInventoryItemInstance* PartialRuntimeItem =
		PartialRuntimeInventory
			? PartialRuntimeInventory->GrantItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass(),
				1)
			: nullptr;
	if (!TestNotNull(
			TEXT("The non-canonical manager contains a partial runtime row"),
			PartialRuntimeItem))
	{
		DropActor->FinishSpawning(FTransform::Identity);
		return false;
	}

	TestFalse(
		TEXT("A deferred manager with partial rows is not canonical"),
		DropActor->IsLootInventoryCanonical());
	const FInventoryPickup EffectivePickup =
		DropActor->GetPickupInventory();
	TestEqual(
		TEXT("A non-canonical partial graph cannot hide the static fallback"),
		EffectivePickup.Templates.Num(),
		1);
	TestEqual(
		TEXT("The effective fallback retains no partial runtime instances"),
		EffectivePickup.Instances.Num(),
		0);
	if (EffectivePickup.Templates.Num() == 1)
	{
		TestTrue(
			TEXT("The effective fallback retains its item definition"),
			EffectivePickup.Templates[0].ItemDef ==
				URpgInventoryAutomationTestStackItemDefinition::
					StaticClass());
		TestEqual(
			TEXT("The effective fallback retains its full quantity"),
			EffectivePickup.Templates[0].StackCount,
			4);
	}

	DropActor->FinishSpawning(FTransform::Identity);
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
	FRpgInventoryEntryView SourceBagEntry;
	if (!TestTrue(
		TEXT("The physical-drop provider exposes a complete source snapshot"),
		GetEntryView(SourceInventory, BagId, SourceBagEntry)))
	{
		return false;
	}

	FRpgInventoryMutationRequest CapacityProbe;
	CapacityProbe.Operation = ERpgInventoryMutationOperation::Drop;
	CapacityProbe.ItemId = BagId;
	CapacityProbe.ExpectedEntryId = SourceBagEntry.EntryId;
	CapacityProbe.Source = Root;
	CapacityProbe.ExpectedSourcePlacement = SourceBagEntry.Placement;
	CapacityProbe.ExpectedSourceQuantity = SourceBagEntry.StackCount;
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

	const FRpgInventoryTransferIntent BagDropIntent =
		MakeTransferIntent(SourceInventory, BagId, 1);
	const FRpgInventoryMutationResult DropResult =
		DropActor->TransferItemFromInventoryByIntent(
			SourceInventory,
			BagDropIntent);
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
	const FGuid FirstIdentityDropRequestId = FGuid::NewGuid();
	const FRpgInventoryTransferIntent FirstIdentityDropIntent =
		MakeTransferIntent(
			IdentitySource,
			FirstConcreteStackId,
			3,
			FirstIdentityDropRequestId);
	const FRpgInventoryMutationResult FirstIdentityDrop =
		DropActor->TransferItemFromInventoryByIntent(
			IdentitySource,
			FirstIdentityDropIntent,
			true);
	const FString IdentitySourceAfterFirstDrop =
		MakeInventorySignature(IdentitySource);
	const FString DropAfterFirstIdentityDrop =
		MakeInventorySignature(DropInventory);
	const FRpgInventoryMutationResult FirstIdentityDropReplay =
		DropActor->TransferItemFromInventoryByIntent(
			IdentitySource,
			FirstIdentityDropIntent,
			true);
	TestEqual(
		TEXT("The first identity-preserving stack drop succeeds"),
		FirstIdentityDrop.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("A full identity-preserving physical drop replays its original success"),
		FirstIdentityDropReplay.Code,
		FirstIdentityDrop.Code);
	TestEqual(
		TEXT("A full identity-preserving retry applies no second source mutation"),
		MakeInventorySignature(IdentitySource),
		IdentitySourceAfterFirstDrop);
	TestEqual(
		TEXT("A full identity-preserving retry derives no second target placement"),
		MakeInventorySignature(DropInventory),
		DropAfterFirstIdentityDrop);
	const FRpgInventoryMutationResult IdentityPolicyCollision =
		DropActor->TransferItemFromInventoryByIntent(
			IdentitySource,
			FirstIdentityDropIntent,
			false);
	TestEqual(
		TEXT("A physical drop request id cannot change its merge policy"),
		IdentityPolicyCollision.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);
	TestEqual(
		TEXT("A physical merge-policy collision preserves the source"),
		MakeInventorySignature(IdentitySource),
		IdentitySourceAfterFirstDrop);
	TestEqual(
		TEXT("A physical merge-policy collision preserves the target"),
		MakeInventorySignature(DropInventory),
		DropAfterFirstIdentityDrop);

	const FRpgInventoryGraphSaveData DropEpochSnapshot =
		DropInventory->ExportInventoryGraph();
	FRpgInventoryMutationResult DropRestoreResult;
	TestTrue(
		TEXT("A successful target restore establishes a fresh drop-command epoch"),
		DropInventory->RestoreInventoryGraph(
			DropEpochSnapshot,
			DropRestoreResult));
	TestEqual(
		TEXT("The target restore preserves the current drop graph"),
		MakeInventorySignature(DropInventory),
		DropAfterFirstIdentityDrop);
	const FRpgInventoryMutationResult RetryAfterTargetRestore =
		DropActor->TransferItemFromInventoryByIntent(
			IdentitySource,
			FirstIdentityDropIntent,
			true);
	TestEqual(
		TEXT("A physical-drop result from the previous target epoch is re-evaluated"),
		RetryAfterTargetRestore.Code,
		ERpgInventoryMutationResultCode::ItemNotFound);
	TestEqual(
		TEXT("Re-evaluating the stale target-epoch request preserves the source"),
		MakeInventorySignature(IdentitySource),
		IdentitySourceAfterFirstDrop);
	TestEqual(
		TEXT("Re-evaluating the stale target-epoch request preserves the target"),
		MakeInventorySignature(DropInventory),
		DropAfterFirstIdentityDrop);
	const FRpgInventoryMutationResult RetryInRestoredTargetEpoch =
		DropActor->TransferItemFromInventoryByIntent(
			IdentitySource,
			FirstIdentityDropIntent,
			true);
	TestEqual(
		TEXT("A retry in the restored target epoch replays its re-evaluated result"),
		RetryInRestoredTargetEpoch.Code,
		RetryAfterTargetRestore.Code);
	const FRpgInventoryMutationResult RestoredEpochPolicyCollision =
		DropActor->TransferItemFromInventoryByIntent(
			IdentitySource,
			FirstIdentityDropIntent,
			false);
	TestEqual(
		TEXT("The restored target epoch still rejects a merge-policy collision"),
		RestoredEpochPolicyCollision.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);

	const FRpgInventoryMutationResult SecondIdentityDrop =
		DropActor->TransferItemFromInventoryByIntent(
			IdentitySource,
			MakeTransferIntent(
				IdentitySource,
				SecondConcreteStackId,
				2),
			true);
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

	URpgInventoryManagerComponent* SourceEpochInventory =
		TestWorld.CreateInventory(TEXT("PhysicalDropSourceEpoch"));
	URpgInventoryItemInstance* SourceEpochItem =
		SourceEpochInventory
			? SourceEpochInventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass(),
				1,
				MakePlacement(Root, 0, 0))
			: nullptr;
	if (!TestNotNull(
			TEXT("The source-epoch drop fixture exists"),
			SourceEpochItem))
	{
		return false;
	}
	const FRpgInventoryItemId SourceEpochItemId =
		SourceEpochItem->GetItemId();
	const FGuid SourceEpochRequestId = FGuid::NewGuid();
	const FRpgInventoryGraphSaveData SourceEpochSnapshot =
		SourceEpochInventory->ExportInventoryGraph();
	const FRpgInventoryMutationResult RemoveSourceEpochItem =
		SourceEpochInventory->ConsumeItemById(
			SourceEpochItemId,
			1);
	TestTrue(
		TEXT("The source-epoch fixture starts with its item absent"),
		RemoveSourceEpochItem.IsSuccess());

	const FRpgInventoryMutationResult MissingBeforeSourceRestore =
		DropActor->TransferItemFromInventoryByIntent(
			SourceEpochInventory,
			MakeTransferIntent(
				SourceEpochInventory,
				SourceEpochItemId,
				1,
				SourceEpochRequestId),
			true);
	TestEqual(
		TEXT("The physical gateway caches the missing pre-restore source state"),
		MissingBeforeSourceRestore.Code,
		ERpgInventoryMutationResultCode::ItemNotFound);
	FRpgInventoryMutationResult SourceRestoreResult;
	TestTrue(
		TEXT("A successful source restore establishes a fresh drop-command epoch"),
		SourceEpochInventory->RestoreInventoryGraph(
			SourceEpochSnapshot,
			SourceRestoreResult));
	const FRpgInventoryTransferIntent RestoredSourceEpochIntent =
		MakeTransferIntent(
			SourceEpochInventory,
			SourceEpochItemId,
			1,
			SourceEpochRequestId);
	const FRpgInventoryMutationResult RetryAfterSourceRestore =
		DropActor->TransferItemFromInventoryByIntent(
			SourceEpochInventory,
			RestoredSourceEpochIntent,
			true);
	TestEqual(
		TEXT("A physical-drop result from the previous source epoch is re-evaluated"),
		RetryAfterSourceRestore.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The re-evaluated source-epoch request moves the restored item once"),
		SourceEpochInventory->GetUsedEntryCount(),
		0);
	const FString DropAfterSourceEpochRetry =
		MakeInventorySignature(DropInventory);
	const FRpgInventoryMutationResult ReplayInRestoredSourceEpoch =
		DropActor->TransferItemFromInventoryByIntent(
			SourceEpochInventory,
			RestoredSourceEpochIntent,
			true);
	TestEqual(
		TEXT("A retry in the restored source epoch replays success"),
		ReplayInRestoredSourceEpoch.Code,
		RetryAfterSourceRestore.Code);
	TestEqual(
		TEXT("The restored source-epoch replay cannot mutate the target twice"),
		MakeInventorySignature(DropInventory),
		DropAfterSourceEpochRetry);
	const FRpgInventoryMutationResult SourceEpochPolicyCollision =
		DropActor->TransferItemFromInventoryByIntent(
			SourceEpochInventory,
			RestoredSourceEpochIntent,
			false);
	TestEqual(
		TEXT("The restored source epoch still rejects a merge-policy collision"),
		SourceEpochPolicyCollision.Code,
		ERpgInventoryMutationResultCode::InvalidRequest);

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
		DropActor->TransferItemFromInventoryByIntent(
			OverflowSource,
			MakeTransferIntent(
				OverflowSource,
				OverflowItemId,
				1),
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryInteractionGlobalPendingSingleFlightTest,
	"SurvivalRpg.Inventory.Interaction.Pending.GlobalSingleFlight",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryInteractionGlobalPendingSingleFlightTest::RunTest(
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
		TEXT("GlobalPendingController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("GlobalPendingPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The pending-guard controller exists"), Controller) ||
		!TestNotNull(TEXT("The pending-guard player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	if (!TestNotNull(TEXT("The pending-guard inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The pending-guard action gateway exists"), UiActions))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* FirstItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* SecondItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 1, 0));
	if (!TestNotNull(TEXT("The first independent payload exists"), FirstItem) ||
		!TestNotNull(TEXT("The second independent payload exists"), SecondItem))
	{
		return false;
	}

	const FRpgInventoryDragPayload FirstPayload =
		MakeInventoryEntryPayload(Inventory, FirstItem);
	const FRpgInventoryDragPayload SecondPayload =
		MakeInventoryEntryPayload(Inventory, SecondItem);
	if (!TestTrue(
			TEXT("The first pending payload owns an exact source snapshot"),
			URpgInventoryDragDropCoordinator::IsPayloadValid(FirstPayload)) ||
		!TestTrue(
			TEXT("The competing payload owns an exact source snapshot"),
			URpgInventoryDragDropCoordinator::IsPayloadValid(SecondPayload)))
	{
		return false;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(Controller, Controller);
	if (!TestNotNull(TEXT("The pending-guard coordinator exists"), Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);
	URpgInventoryInteractionSession* Session =
		Coordinator->GetInteractionSession();
	if (!TestNotNull(TEXT("The pending-guard session exists"), Session) ||
		!TestTrue(
			TEXT("The first payload begins the interaction"),
			Session->BeginInteraction(
				FirstPayload,
				ERpgInventoryInteractionInputMode::Mouse)))
	{
		return false;
	}

	FRpgInventoryDropTarget FirstTarget;
	FirstTarget.TargetType =
		ERpgInventoryDropTargetType::InventorySlot;
	FirstTarget.TargetInventory = Inventory;
	FirstTarget.TargetPlacement = MakePlacement(Pockets, 2, 0);
	Session->MarkRequestPending(
		FirstTarget,
		RpgGameplayTags::Rpg_Inventory_Action_Transfer);
	const FGuid FirstRequestId = Session->GetRequestId();
	if (!TestTrue(TEXT("The first request is pending"), Session->IsRequestPending()) ||
		!TestTrue(TEXT("The pending request owns a correlation id"), FirstRequestId.IsValid()))
	{
		return false;
	}

	const FString InventoryBeforeCompetingRequest =
		MakeStrictInventorySignature(Inventory);
	FRpgInventoryDropTarget SecondTarget;
	SecondTarget.TargetType =
		ERpgInventoryDropTargetType::InventorySlot;
	SecondTarget.TargetInventory = Inventory;
	SecondTarget.TargetPlacement = MakePlacement(Pockets, 3, 0);
	const FRpgInventoryInteractionPreviewPlan CompetingPreview =
		Coordinator->PlanInteractionPreview(
			SecondPayload,
			SecondTarget);
	TestEqual(
		TEXT("Any competing payload previews as Pending while one request is in flight"),
		CompetingPreview.State,
		ERpgInventoryInteractionPreviewState::Pending);
	TestFalse(
		TEXT("The global Pending state is never accepted"),
		CompetingPreview.IsAccepted());
	TestFalse(
		TEXT("A second payload cannot dispatch while the first request is pending"),
		Coordinator->CommitPayloadToTarget(
			SecondPayload,
			SecondTarget));
	TestEqual(
		TEXT("The blocked competing commit performs no inventory mutation"),
		MakeStrictInventorySignature(Inventory),
		InventoryBeforeCompetingRequest);
	TestTrue(
		TEXT("The original request remains pending after the competing attempt"),
		Session->IsRequestPending());
	TestEqual(
		TEXT("The competing attempt cannot replace the original correlation id"),
		Session->GetRequestId(),
		FirstRequestId);
	TestEqual(
		TEXT("The competing attempt cannot replace the original payload entry"),
		Session->GetPayload().EntryId,
		FirstPayload.EntryId);
	TestTrue(
		TEXT("The competing attempt cannot replace the original exact target"),
		ArePlacementSnapshotsExactlyEqual(
			Session->GetTarget().TargetPlacement,
			FirstTarget.TargetPlacement));

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	FRpgInventoryActionFeedbackMessage MissingCorrelationFeedback;
	MissingCorrelationFeedback.Recipient = Controller;
	MissingCorrelationFeedback.ActionTag =
		RpgGameplayTags::Rpg_Inventory_Action_Transfer;
	MissingCorrelationFeedback.Result =
		ERpgInventoryActionFeedbackResult::ServerRejected;
	MessageSubsystem.BroadcastMessage(
		RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
		MissingCorrelationFeedback);
	TestTrue(
		TEXT("Feedback without a request id cannot release an InventorySlot request"),
		Session->IsRequestPending());
	TestEqual(
		TEXT("Uncorrelated feedback preserves the original correlation id"),
		Session->GetRequestId(),
		FirstRequestId);
	TestEqual(
		TEXT("Uncorrelated feedback preserves the original payload entry"),
		Session->GetPayload().EntryId,
		FirstPayload.EntryId);

	FRpgInventoryActionFeedbackMessage ForeignCorrelationFeedback =
		MissingCorrelationFeedback;
	ForeignCorrelationFeedback.RequestId = FGuid::NewGuid();
	ForeignCorrelationFeedback.ItemId = FirstItem->GetItemId();
	ForeignCorrelationFeedback.InventoryOwner = Inventory;
	ForeignCorrelationFeedback.Item = FirstItem;
	MessageSubsystem.BroadcastMessage(
		RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
		ForeignCorrelationFeedback);
	TestTrue(
		TEXT("Feedback with a foreign request id cannot release an InventorySlot request"),
		Session->IsRequestPending());
	TestEqual(
		TEXT("Foreign feedback preserves the original correlation id"),
		Session->GetRequestId(),
		FirstRequestId);

	FRpgInventoryActionFeedbackMessage CorrelatedFeedback =
		ForeignCorrelationFeedback;
	CorrelatedFeedback.RequestId = FirstRequestId;
	MessageSubsystem.BroadcastMessage(
		RpgGameplayTags::Rpg_Inventory_Message_ActionFeedback,
		CorrelatedFeedback);
	TestFalse(
		TEXT("Exact request-correlated feedback releases the global single-flight guard"),
		Session->IsRequestPending());
	TestEqual(
		TEXT("The correlated server rejection retains the original payload for retry"),
		Session->GetPayload().EntryId,
		FirstPayload.EntryId);
	const FRpgInventoryInteractionPreviewPlan ReleasedPreview =
		Coordinator->PlanInteractionPreview(
			SecondPayload,
			SecondTarget);
	TestTrue(
		TEXT("The competing payload becomes eligible after pending state resolves"),
		ReleasedPreview.IsAccepted());
	TestEqual(
		TEXT("The released empty-cell move has normal Move semantics"),
		ReleasedPreview.State,
		ERpgInventoryInteractionPreviewState::Move);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryWholeDragPartialMergeAtomicityTest,
	"SurvivalRpg.Inventory.DragDrop.WholeEntryPartialMergeBlocked",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryWholeDragPartialMergeAtomicityTest::RunTest(
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
		TEXT("WholeDragPartialMergeController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("WholeDragPartialMergePlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The partial-merge controller exists"), Controller) ||
		!TestNotNull(TEXT("The partial-merge player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	if (!TestNotNull(TEXT("The partial-merge inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The partial-merge action gateway exists"), UiActions))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* SourceStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			4,
			MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* TargetStack =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			9,
			MakePlacement(Pockets, 1, 0));
	if (!TestNotNull(TEXT("The whole-entry source stack exists"), SourceStack) ||
		!TestNotNull(TEXT("The nearly-full compatible target exists"), TargetStack))
	{
		return false;
	}

	FRpgInventoryEntryView SourceBefore;
	FRpgInventoryEntryView TargetBefore;
	if (!TestTrue(
			TEXT("The source owns an exact preflight snapshot"),
			GetEntryView(Inventory, SourceStack->GetItemId(), SourceBefore)) ||
		!TestTrue(
			TEXT("The target owns an exact preflight snapshot"),
			GetEntryView(Inventory, TargetStack->GetItemId(), TargetBefore)))
	{
		return false;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(Controller, Controller);
	if (!TestNotNull(TEXT("The partial-merge coordinator exists"), Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);
	const FRpgInventoryDragPayload Payload =
		MakeInventoryEntryPayload(Inventory, SourceStack);
	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	Target.TargetInventory = Inventory;
	Target.TargetPlacement = TargetBefore.Placement;
	const FString InventoryBefore =
		MakeStrictInventorySignature(Inventory);
	const FRpgInventoryInteractionPreviewPlan Preview =
		Coordinator->PlanInteractionPreview(Payload, Target);
	TestTrue(
		TEXT("The whole-entry hover consumes the domain placement evaluator"),
		Preview.bUsesPlacementPlan);
	TestEqual(
		TEXT("The evaluator exposes the one-unit compatible fit as partial"),
		Preview.PlacementPlan.Code,
		ERpgInventoryMutationResultCode::PartiallyApplied);
	TestEqual(
		TEXT("The partial plan retains the four-unit whole-entry request"),
		Preview.PlacementPlan.RequestedQuantity,
		4);
	TestEqual(
		TEXT("The target has capacity for only one unit"),
		Preview.PlacementPlan.AppliedQuantity,
		1);
	if (!TestEqual(
			TEXT("The partial whole-entry plan contains one merge step"),
			Preview.PlacementPlan.Steps.Num(),
			1))
	{
		return false;
	}
	TestEqual(
		TEXT("The partial whole-entry step resolves to Merge"),
		Preview.PlacementPlan.Steps[0].Resolution,
		ERpgInventoryPlacementResolution::Merge);
	TestEqual(
		TEXT("The partial whole-entry step names the exact receiver entry"),
		Preview.PlacementPlan.Steps[0].TargetEntryId,
		TargetBefore.EntryId);
	TestEqual(
		TEXT("The partial whole-entry step covers only one unit"),
		Preview.PlacementPlan.Steps[0].Quantity,
		1);
	TestFalse(
		TEXT("A partial merge is not a complete whole-entry plan"),
		Preview.PlacementPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("The UI blocks a partial whole-entry merge"),
		Preview.State,
		ERpgInventoryInteractionPreviewState::Blocked);
	TestFalse(
		TEXT("A partial whole-entry plan cannot dispatch"),
		Coordinator->CommitPayloadToTarget(Payload, Target));
	TestEqual(
		TEXT("Rejected partial merge preserves identities, raw placements, quantities, and revision"),
		MakeStrictInventorySignature(Inventory),
		InventoryBefore);
	TestEqual(
		TEXT("The source runtime instance remains the same object"),
		Inventory->FindItemById(SourceBefore.ItemId),
		SourceBefore.Instance.Get());
	TestEqual(
		TEXT("The target runtime instance remains the same object"),
		Inventory->FindItemById(TargetBefore.ItemId),
		TargetBefore.Instance.Get());
	TestFalse(
		TEXT("A locally blocked partial plan never enters Pending"),
		Coordinator->GetInteractionSession()->IsRequestPending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryGearClearFullContentAtomicityTest,
	"SurvivalRpg.Inventory.DragDrop.GearClearFullContentBlocked",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryGearClearFullContentAtomicityTest::RunTest(
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
		TEXT("GearClearFullContentController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("GearClearFullContentPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The full-content controller exists"), Controller) ||
		!TestNotNull(TEXT("The full-content player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgPlayerInventoryLayoutComponent* Layout =
		Controller->GetPlayerInventoryLayoutComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		Controller->GetEquipmentLoadoutComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	if (!TestNotNull(TEXT("The full-content inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The full-content layout exists"), Layout) ||
		!TestNotNull(TEXT("The full-content loadout mirror exists"), EquipmentLoadout) ||
		!TestNotNull(TEXT("The full-content action gateway exists"), UiActions))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* Armor =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestHeavyItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("The Chest armor starts in Content"), Armor))
	{
		return false;
	}

	UiActions->RequestApplyInventoryEquipmentIntent(
		Inventory,
		MakeEquipmentIntent(
			Inventory,
			Armor,
			ERpgInventoryEquipmentIntentOperation::EquipToSlot,
			ERpgEquipmentSlot::Chest));
	FRpgInventoryEntryView EquippedArmorBefore;
	if (!TestTrue(
			TEXT("The armor remains addressable after physical equip"),
			GetEntryView(
				Inventory,
				Armor->GetItemId(),
				EquippedArmorBefore)))
	{
		return false;
	}
	TestEqual(
		TEXT("The armor physically occupies Gear.Chest"),
		EquippedArmorBefore.Placement.GetContainerHandle(),
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::GearChestGroupId));
	TestEqual(
		TEXT("The Chest loadout mirror references the physical armor"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest),
		Armor);

	int32 ContentCellCount = 0;
	bool bFilledEveryContentCell = true;
	for (const FRpgInventorySlotGroupView& Group :
		 Layout->GetSlotGroups())
	{
		if (Group.GroupKind != ERpgInventorySlotGroupKind::Content)
		{
			continue;
		}

		for (int32 Y = 0; Y < Group.GridSize.Height; ++Y)
		{
			for (int32 X = 0; X < Group.GridSize.Width; ++X)
			{
				++ContentCellCount;
				if (!Inventory->GetItemAtContainerCell(
						Group.ContainerHandle,
						X,
						Y))
				{
					bFilledEveryContentCell &=
						Inventory->AddItemDefinitionToPlacement(
							URpgInventoryAutomationTestUnitItemDefinition::
								StaticClass(),
							1,
							MakePlacement(
								Group.ContainerHandle,
								X,
								Y)) != nullptr;
				}
			}
		}
	}
	if (!TestTrue(TEXT("The layout exposes at least one Content cell"), ContentCellCount > 0) ||
		!TestTrue(TEXT("Every compatible Content cell is occupied"), bFilledEveryContentCell))
	{
		return false;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(Controller, Controller);
	if (!TestNotNull(TEXT("The Gear-clear coordinator exists"), Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);
	const FRpgInventoryDragPayload Payload =
		URpgInventoryDragDropCoordinator::MakeEquipmentPayload(
			Armor,
			ERpgEquipmentSlot::Chest);
	const FRpgInventoryDropTarget ClearTarget =
		URpgInventoryDragDropCoordinator::MakeClearTarget();
	const FString InventoryBeforeClear =
		MakeStrictInventorySignature(Inventory);
	const FRpgInventoryInteractionPreviewPlan Preview =
		Coordinator->PlanInteractionPreview(Payload, ClearTarget);
	TestTrue(
		TEXT("Physical Gear clear consumes an UnequipToContent placement plan"),
		Preview.bUsesPlacementPlan);
	TestFalse(
		TEXT("A full Content layout has no complete unequip placement"),
		Preview.PlacementPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("The full Content placement plan reports NoSpace"),
		Preview.PlacementPlan.Code,
		ERpgInventoryMutationResultCode::NoSpace);
	TestEqual(
		TEXT("A full Content layout blocks the Gear clear preview"),
		Preview.State,
		ERpgInventoryInteractionPreviewState::Blocked);
	TestFalse(
		TEXT("A blocked Gear clear cannot dispatch"),
		Coordinator->CommitPayloadToTarget(Payload, ClearTarget));
	TestEqual(
		TEXT("Blocked Gear clear preserves identities, raw placements, quantities, and revision"),
		MakeStrictInventorySignature(Inventory),
		InventoryBeforeClear);
	TestEqual(
		TEXT("Blocked Gear clear preserves the Chest loadout mirror"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::Chest),
		Armor);
	FRpgInventoryEntryView EquippedArmorAfter;
	if (!TestTrue(
			TEXT("Blocked Gear clear preserves the armor entry"),
			GetEntryView(
				Inventory,
				EquippedArmorBefore.ItemId,
				EquippedArmorAfter)))
	{
		return false;
	}
	TestEqual(
		TEXT("Blocked Gear clear preserves the exact entry id"),
		EquippedArmorAfter.EntryId,
		EquippedArmorBefore.EntryId);
	TestEqual(
		TEXT("Blocked Gear clear preserves the exact runtime instance"),
		EquippedArmorAfter.Instance.Get(),
		EquippedArmorBefore.Instance.Get());
	TestTrue(
		TEXT("Blocked Gear clear preserves the raw Gear placement snapshot"),
		ArePlacementSnapshotsExactlyEqual(
			EquippedArmorAfter.Placement,
			EquippedArmorBefore.Placement));
	TestFalse(
		TEXT("A locally blocked Gear clear never enters Pending"),
		Coordinator->GetInteractionSession()->IsRequestPending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryExactCrossCoordinatorParityTest,
	"SurvivalRpg.Inventory.DragDrop.ExactCrossInventoryPlanCommitParity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryExactCrossCoordinatorParityTest::RunTest(
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
		TEXT("ExactCrossCoordinatorController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ControllerSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PawnSpawnParameters;
	PawnSpawnParameters.Name = MakeUniqueObjectName(
		World,
		APawn::StaticClass(),
		TEXT("ExactCrossCoordinatorPawn"));
	PawnSpawnParameters.ObjectFlags = RF_Transient;
	PawnSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* ControllerPawn = World->SpawnActor<APawn>(
		PawnSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("ExactCrossCoordinatorPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	PlayerStateSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);

	FActorSpawnParameters ContainerSpawnParameters;
	ContainerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryContainerActor::StaticClass(),
		TEXT("ExactCrossCoordinatorContainer"));
	ContainerSpawnParameters.ObjectFlags = RF_Transient;
	ContainerSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryContainerActor* Container =
		World->SpawnActor<ARpgInventoryContainerActor>(
			ContainerSpawnParameters);
	if (!TestNotNull(TEXT("The exact-cross controller exists"), Controller) ||
		!TestNotNull(TEXT("The exact-cross pawn exists"), ControllerPawn) ||
		!TestNotNull(TEXT("The exact-cross player state exists"), PlayerState) ||
		!TestNotNull(TEXT("The exact-cross container exists"), Container))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	Controller->Possess(ControllerPawn);
	URpgInventoryManagerComponent* SourceInventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryManagerComponent* TargetInventory =
		Container->GetInventoryManager();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	if (!TestTrue(TEXT("The exact-cross fixture runs on authority"), Controller->HasAuthority()) ||
		!TestNotNull(TEXT("The exact-cross source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("The exact-cross target inventory exists"), TargetInventory) ||
		!TestNotNull(TEXT("The exact-cross action gateway exists"), UiActions) ||
		!TestTrue(
			TEXT("The possessed pawn can access the nearby target inventory"),
			UiActions->CanAccessInventory(TargetInventory)))
	{
		return false;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(Controller, Controller);
	if (!TestNotNull(TEXT("The exact-cross coordinator exists"), Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle TargetRoot =
		FRpgInventoryContainerHandle::MakeRoot(
			TargetInventory->GetDefaultContainerId());

	URpgInventoryItemInstance* PartialSource =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			2,
			MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* PartialTarget =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			9,
			MakePlacement(TargetRoot, 0, 0));
	if (!TestNotNull(TEXT("The partial exact source exists"), PartialSource) ||
		!TestNotNull(TEXT("The partial exact target exists"), PartialTarget))
	{
		return false;
	}
	FRpgInventoryEntryView PartialTargetEntry;
	if (!TestTrue(
			TEXT("The partial target owns an exact snapshot"),
			GetEntryView(
				TargetInventory,
				PartialTarget->GetItemId(),
				PartialTargetEntry)))
	{
		return false;
	}
	const FRpgInventoryDragPayload PartialPayload =
		MakeInventoryEntryPayload(SourceInventory, PartialSource);
	FRpgInventoryDropTarget PartialDropTarget;
	PartialDropTarget.TargetType =
		ERpgInventoryDropTargetType::InventorySlot;
	PartialDropTarget.TargetInventory = TargetInventory;
	PartialDropTarget.TargetPlacement = PartialTargetEntry.Placement;
	const FString SourceBeforePartial =
		MakeStrictInventorySignature(SourceInventory);
	const FString TargetBeforePartial =
		MakeStrictInventorySignature(TargetInventory);
	const FRpgInventoryInteractionPreviewPlan PartialPreview =
		Coordinator->PlanInteractionPreview(
			PartialPayload,
			PartialDropTarget);
	TestTrue(
		TEXT("The cross-inventory exact hover consumes a domain plan"),
		PartialPreview.bUsesPlacementPlan);
	TestEqual(
		TEXT("The exact cross evaluator exposes insufficient stack capacity as partial"),
		PartialPreview.PlacementPlan.Code,
		ERpgInventoryMutationResultCode::PartiallyApplied);
	TestEqual(
		TEXT("The partial exact cross plan covers one of two requested units"),
		PartialPreview.PlacementPlan.AppliedQuantity,
		1);
	TestEqual(
		TEXT("The coordinator blocks the partial exact cross plan"),
		PartialPreview.State,
		ERpgInventoryInteractionPreviewState::Blocked);
	TestFalse(
		TEXT("The partial exact cross plan cannot dispatch"),
		Coordinator->CommitPayloadToTarget(
			PartialPayload,
			PartialDropTarget));
	TestEqual(
		TEXT("Rejected partial exact transfer preserves the complete source snapshot"),
		MakeStrictInventorySignature(SourceInventory),
		SourceBeforePartial);
	TestEqual(
		TEXT("Rejected partial exact transfer preserves the complete target snapshot"),
		MakeStrictInventorySignature(TargetInventory),
		TargetBeforePartial);

	URpgInventoryItemInstance* IncompatibleSource =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 1, 0));
	URpgInventoryItemInstance* IncompatibleTarget =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakePlacement(TargetRoot, 1, 0));
	if (!TestNotNull(TEXT("The incompatible exact source exists"), IncompatibleSource) ||
		!TestNotNull(TEXT("The incompatible exact target exists"), IncompatibleTarget))
	{
		return false;
	}
	IncompatibleTarget->AddStatTagStack(
		RpgGameplayTags::Rpg_Inventory_Action_Transfer,
		1);
	FRpgInventoryEntryView IncompatibleTargetEntry;
	if (!TestTrue(
			TEXT("The incompatible target owns an exact snapshot"),
			GetEntryView(
				TargetInventory,
				IncompatibleTarget->GetItemId(),
				IncompatibleTargetEntry)))
	{
		return false;
	}
	const FRpgInventoryDragPayload IncompatiblePayload =
		MakeInventoryEntryPayload(SourceInventory, IncompatibleSource);
	FRpgInventoryDropTarget IncompatibleDropTarget;
	IncompatibleDropTarget.TargetType =
		ERpgInventoryDropTargetType::InventorySlot;
	IncompatibleDropTarget.TargetInventory = TargetInventory;
	IncompatibleDropTarget.TargetPlacement =
		IncompatibleTargetEntry.Placement;
	const FString SourceBeforeIncompatible =
		MakeStrictInventorySignature(SourceInventory);
	const FString TargetBeforeIncompatible =
		MakeStrictInventorySignature(TargetInventory);
	const FRpgInventoryInteractionPreviewPlan IncompatiblePreview =
		Coordinator->PlanInteractionPreview(
			IncompatiblePayload,
			IncompatibleDropTarget);
	TestTrue(
		TEXT("The incompatible exact hover consumes a domain plan"),
		IncompatiblePreview.bUsesPlacementPlan);
	TestEqual(
		TEXT("Different runtime state produces StackIncompatible"),
		IncompatiblePreview.PlacementPlan.Code,
		ERpgInventoryMutationResultCode::StackIncompatible);
	TestEqual(
		TEXT("The coordinator blocks an incompatible exact cross merge"),
		IncompatiblePreview.State,
		ERpgInventoryInteractionPreviewState::Blocked);
	TestFalse(
		TEXT("The incompatible exact cross plan cannot dispatch"),
		Coordinator->CommitPayloadToTarget(
			IncompatiblePayload,
			IncompatibleDropTarget));
	TestEqual(
		TEXT("Rejected incompatible transfer preserves the complete source snapshot"),
		MakeStrictInventorySignature(SourceInventory),
		SourceBeforeIncompatible);
	TestEqual(
		TEXT("Rejected incompatible transfer preserves the complete target snapshot"),
		MakeStrictInventorySignature(TargetInventory),
		TargetBeforeIncompatible);
	TestEqual(
		TEXT("Rejected incompatible transfer preserves the target runtime state"),
		IncompatibleTarget->GetStatTagStackCount(
			RpgGameplayTags::Rpg_Inventory_Action_Transfer),
		1);

	URpgInventoryItemInstance* CompatibleSource =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 2, 0));
	URpgInventoryItemInstance* CompatibleTarget =
		TargetInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
			9,
			MakePlacement(TargetRoot, 2, 0));
	if (!TestNotNull(TEXT("The complete exact source exists"), CompatibleSource) ||
		!TestNotNull(TEXT("The compatible exact target exists"), CompatibleTarget))
	{
		return false;
	}
	FRpgInventoryEntryView CompatibleSourceBefore;
	FRpgInventoryEntryView CompatibleTargetBefore;
	if (!TestTrue(
			TEXT("The compatible source owns an exact snapshot"),
			GetEntryView(
				SourceInventory,
				CompatibleSource->GetItemId(),
				CompatibleSourceBefore)) ||
		!TestTrue(
			TEXT("The compatible target owns an exact snapshot"),
			GetEntryView(
				TargetInventory,
				CompatibleTarget->GetItemId(),
				CompatibleTargetBefore)))
	{
		return false;
	}
	const FRpgInventoryDragPayload CompatiblePayload =
		MakeInventoryEntryPayload(SourceInventory, CompatibleSource);
	FRpgInventoryDropTarget CompatibleDropTarget;
	CompatibleDropTarget.TargetType =
		ERpgInventoryDropTargetType::InventorySlot;
	CompatibleDropTarget.TargetInventory = TargetInventory;
	CompatibleDropTarget.TargetPlacement =
		CompatibleTargetBefore.Placement;
	const FString SourceBeforeCompatiblePreview =
		MakeStrictInventorySignature(SourceInventory);
	const FString TargetBeforeCompatiblePreview =
		MakeStrictInventorySignature(TargetInventory);
	const int32 SourceRevisionBeforeCommit =
		SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBeforeCommit =
		TargetInventory->GetInventoryRevision();
	const FRpgInventoryInteractionPreviewPlan CompatiblePreview =
		Coordinator->PlanInteractionPreview(
			CompatiblePayload,
			CompatibleDropTarget);
	TestTrue(
		TEXT("The compatible exact cross plan is complete"),
		CompatiblePreview.bUsesPlacementPlan &&
			CompatiblePreview.PlacementPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("The complete exact cross plan previews as Merge"),
		CompatiblePreview.State,
		ERpgInventoryInteractionPreviewState::Merge);
	if (!TestEqual(
			TEXT("The complete exact cross plan contains one atomic step"),
			CompatiblePreview.PlacementPlan.Steps.Num(),
			1))
	{
		return false;
	}
	TestEqual(
		TEXT("The complete exact cross step resolves to Merge"),
		CompatiblePreview.PlacementPlan.Steps[0].Resolution,
		ERpgInventoryPlacementResolution::Merge);
	TestEqual(
		TEXT("The merge step names the concrete target entry"),
		CompatiblePreview.PlacementPlan.Steps[0].TargetEntryId,
		CompatibleTargetBefore.EntryId);
	TestEqual(
		TEXT("Compatible preview is pure for the source graph"),
		MakeStrictInventorySignature(SourceInventory),
		SourceBeforeCompatiblePreview);
	TestEqual(
		TEXT("Compatible preview is pure for the target graph"),
		MakeStrictInventorySignature(TargetInventory),
		TargetBeforeCompatiblePreview);
	TestTrue(
		TEXT("The coordinator dispatches the complete exact cross merge"),
		Coordinator->CommitPayloadToTarget(
			CompatiblePayload,
			CompatibleDropTarget));
	TestNull(
		TEXT("The complete merge removes the consumed source identity from the source graph"),
		SourceInventory->FindItemById(CompatibleSourceBefore.ItemId));
	FRpgInventoryEntryView CompatibleTargetAfter;
	if (!TestTrue(
			TEXT("The merge receiver remains addressable"),
			GetEntryView(
				TargetInventory,
				CompatibleTargetBefore.ItemId,
				CompatibleTargetAfter)))
	{
		return false;
	}
	TestEqual(
		TEXT("The merge preserves the receiver's persistent item id"),
		CompatibleTargetAfter.ItemId,
		CompatibleTargetBefore.ItemId);
	TestEqual(
		TEXT("The merge preserves the receiver's replicated entry id"),
		CompatibleTargetAfter.EntryId,
		CompatibleTargetBefore.EntryId);
	TestEqual(
		TEXT("The merge preserves the receiver's runtime instance"),
		CompatibleTargetAfter.Instance.Get(),
		CompatibleTargetBefore.Instance.Get());
	TestEqual(
		TEXT("The compatible receiver reaches its exact stack limit"),
		CompatibleTargetAfter.StackCount,
		10);
	TestTrue(
		TEXT("The merge preserves the receiver's raw placement snapshot"),
		ArePlacementSnapshotsExactlyEqual(
			CompatibleTargetAfter.Placement,
			CompatibleTargetBefore.Placement));
	TestTrue(
		TEXT("The complete merge advances the source revision"),
		SourceInventory->GetInventoryRevision() >
			SourceRevisionBeforeCommit);
	TestTrue(
		TEXT("The complete merge advances the target revision"),
		TargetInventory->GetInventoryRevision() >
			TargetRevisionBeforeCommit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPanelQuickTransferParityTest,
	"SurvivalRpg.Inventory.DragDrop.InventoryPanelFirstFitParity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPanelQuickTransferParityTest::RunTest(
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
		TEXT("InventoryPanelFirstFitController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ControllerSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PawnSpawnParameters;
	PawnSpawnParameters.Name = MakeUniqueObjectName(
		World,
		APawn::StaticClass(),
		TEXT("InventoryPanelFirstFitPawn"));
	PawnSpawnParameters.ObjectFlags = RF_Transient;
	PawnSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APawn* ControllerPawn = World->SpawnActor<APawn>(
		PawnSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("InventoryPanelFirstFitPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	PlayerStateSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);

	FActorSpawnParameters ContainerSpawnParameters;
	ContainerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryContainerActor::StaticClass(),
		TEXT("InventoryPanelFirstFitContainer"));
	ContainerSpawnParameters.ObjectFlags = RF_Transient;
	ContainerSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARpgInventoryContainerActor* Container =
		World->SpawnActor<ARpgInventoryContainerActor>(
			ContainerSpawnParameters);
	if (!TestNotNull(TEXT("The InventoryPanel controller exists"), Controller) ||
		!TestNotNull(TEXT("The InventoryPanel pawn exists"), ControllerPawn) ||
		!TestNotNull(TEXT("The InventoryPanel player state exists"), PlayerState) ||
		!TestNotNull(TEXT("The InventoryPanel target container exists"), Container))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	Controller->Possess(ControllerPawn);
	URpgInventoryManagerComponent* SourceInventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryManagerComponent* TargetInventory =
		Container->GetInventoryManager();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	if (!TestNotNull(TEXT("The InventoryPanel source inventory exists"), SourceInventory) ||
		!TestNotNull(TEXT("The InventoryPanel target inventory exists"), TargetInventory) ||
		!TestNotNull(TEXT("The InventoryPanel action gateway exists"), UiActions) ||
		!TestTrue(
			TEXT("The possessed pawn can access the panel target"),
			UiActions->CanAccessInventory(TargetInventory)))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle TargetRoot =
		FRpgInventoryContainerHandle::MakeRoot(
			TargetInventory->GetDefaultContainerId());
	URpgInventoryItemInstance* SourceItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("The panel-transfer source item exists"), SourceItem))
	{
		return false;
	}
	FRpgInventoryEntryView SourceEntryBefore;
	if (!TestTrue(
			TEXT("The panel-transfer source owns an exact snapshot"),
			GetEntryView(
				SourceInventory,
				SourceItem->GetItemId(),
				SourceEntryBefore)))
	{
		return false;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(Controller, Controller);
	if (!TestNotNull(TEXT("The InventoryPanel coordinator exists"), Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);
	const FRpgInventoryDragPayload Payload =
		MakeInventoryEntryPayload(SourceInventory, SourceItem);
	const FRpgInventoryDropTarget PanelTarget =
		URpgInventoryDragDropCoordinator::MakeInventoryPanelTarget(
			TargetInventory);
	const FString SourceBeforePreview =
		MakeStrictInventorySignature(SourceInventory);
	const FString TargetBeforePreview =
		MakeStrictInventorySignature(TargetInventory);
	const int32 SourceRevisionBeforeCommit =
		SourceInventory->GetInventoryRevision();
	const int32 TargetRevisionBeforeCommit =
		TargetInventory->GetInventoryRevision();
	const FRpgInventoryInteractionPreviewPlan Preview =
		Coordinator->PlanInteractionPreview(Payload, PanelTarget);
	TestTrue(
		TEXT("InventoryPanel preview consumes a complete FirstFit domain plan"),
		Preview.bUsesPlacementPlan &&
			Preview.PlacementPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("An empty panel destination previews as Move"),
		Preview.State,
		ERpgInventoryInteractionPreviewState::Move);
	if (!TestEqual(
			TEXT("The one-unit FirstFit plan contains one placement step"),
			Preview.PlacementPlan.Steps.Num(),
			1))
	{
		return false;
	}
	const FRpgInventoryPlacementStep& PlannedStep =
		Preview.PlacementPlan.Steps[0];
	TestEqual(
		TEXT("The FirstFit step resolves to Place"),
		PlannedStep.Resolution,
		ERpgInventoryPlacementResolution::Place);
	TestTrue(
		TEXT("The public resolved target exactly matches the domain step"),
		ArePlacementSnapshotsExactlyEqual(
			Preview.ResolvedTargetPlacement,
			PlannedStep.Placement));
	TestEqual(
		TEXT("FirstFit selects the external inventory root"),
		PlannedStep.Placement.GetContainerHandle(),
		TargetRoot);
	TestEqual(
		TEXT("FirstFit deterministically selects X zero"),
		PlannedStep.Placement.X,
		0);
	TestEqual(
		TEXT("FirstFit deterministically selects Y zero"),
		PlannedStep.Placement.Y,
		0);
	TestEqual(
		TEXT("InventoryPanel preview is pure for the source graph"),
		MakeStrictInventorySignature(SourceInventory),
		SourceBeforePreview);
	TestEqual(
		TEXT("InventoryPanel preview is pure for the target graph"),
		MakeStrictInventorySignature(TargetInventory),
		TargetBeforePreview);
	TestTrue(
		TEXT("InventoryPanel commit dispatches the planned quick transfer"),
		Coordinator->CommitPayloadToTarget(Payload, PanelTarget));
	TestNull(
		TEXT("The whole-entry FirstFit commit removes the source row"),
		SourceInventory->FindItemById(SourceEntryBefore.ItemId));

	FRpgInventoryEntryView TargetEntryAfter;
	if (!TestTrue(
			TEXT("The target resolves the transferred persistent item id"),
			GetEntryView(
				TargetInventory,
				SourceEntryBefore.ItemId,
				TargetEntryAfter)))
	{
		return false;
	}
	TestEqual(
		TEXT("The FirstFit transfer preserves the persistent item id"),
		TargetEntryAfter.ItemId,
		SourceEntryBefore.ItemId);
	TestTrue(
		TEXT("The reconstructed target row owns a valid replicated entry id"),
		TargetEntryAfter.EntryId.IsValid());
	TestTrue(
		TEXT("Cross-inventory FirstFit reconstructs the runtime instance"),
		TargetEntryAfter.Instance.Get() !=
			SourceEntryBefore.Instance.Get());
	TestEqual(
		TEXT("The reconstructed instance belongs to the target actor"),
		TargetEntryAfter.Instance->GetOuter(),
		static_cast<UObject*>(TargetInventory->GetOwner()));
	TestTrue(
		TEXT("The FirstFit commit uses the exact previewed raw placement"),
		ArePlacementSnapshotsExactlyEqual(
			TargetEntryAfter.Placement,
			Preview.ResolvedTargetPlacement));
	TestEqual(
		TEXT("The FirstFit commit preserves the whole quantity"),
		TargetEntryAfter.StackCount,
		SourceEntryBefore.StackCount);
	TestTrue(
		TEXT("The FirstFit commit advances the source revision"),
		SourceInventory->GetInventoryRevision() >
			SourceRevisionBeforeCommit);
	TestTrue(
		TEXT("The FirstFit commit advances the target revision"),
		TargetInventory->GetInventoryRevision() >
			TargetRevisionBeforeCommit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryMainHandCoordinatorParityTest,
	"SurvivalRpg.Inventory.Intent.Equip.MainHandPlanCommitParity",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryMainHandCoordinatorParityTest::RunTest(
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
		TEXT("MainHandPlanCommitController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("MainHandPlanCommitPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The MainHand parity controller exists"), Controller) ||
		!TestNotNull(TEXT("The MainHand parity player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory =
		PlayerState->GetInventoryManagerComponent();
	URpgInventoryUiActionComponent* UiActions =
		Controller->GetInventoryUiActionComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		Controller->GetEquipmentLoadoutComponent();
	if (!TestNotNull(TEXT("The MainHand parity inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The MainHand parity action gateway exists"), UiActions) ||
		!TestNotNull(TEXT("The MainHand parity loadout mirror exists"), EquipmentLoadout))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle WeaponSlot1 =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	URpgInventoryItemInstance* Weapon =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0));
	if (!TestNotNull(TEXT("The MainHand weapon starts in Pockets"), Weapon))
	{
		return false;
	}
	FRpgInventoryEntryView WeaponBefore;
	if (!TestTrue(
			TEXT("The MainHand weapon owns an exact source snapshot"),
			GetEntryView(
				Inventory,
				Weapon->GetItemId(),
				WeaponBefore)))
	{
		return false;
	}
	TestNull(
		TEXT("MainHand starts without an active selection"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand));

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(Controller, Controller);
	if (!TestNotNull(TEXT("The MainHand parity coordinator exists"), Coordinator))
	{
		return false;
	}
	Coordinator->SetUiActionComponent(UiActions);
	const FRpgInventoryDragPayload Payload =
		MakeInventoryEntryPayload(Inventory, Weapon);
	const FRpgInventoryDropTarget MainHandTarget =
		URpgInventoryDragDropCoordinator::MakeEquipmentTarget(
			ERpgEquipmentSlot::MainHand);
	const FString InventoryBeforePreview =
		MakeStrictInventorySignature(Inventory);
	const int32 RevisionBeforeCommit =
		Inventory->GetInventoryRevision();
	const FRpgInventoryInteractionPreviewPlan Preview =
		Coordinator->PlanInteractionPreview(
			Payload,
			MainHandTarget);
	TestTrue(
		TEXT("Pockets-to-MainHand preview consumes a complete equipment plan"),
		Preview.bUsesPlacementPlan &&
			Preview.PlacementPlan.IsCompleteSuccess());
	TestEqual(
		TEXT("The accepted MainHand target previews as Equip"),
		Preview.State,
		ERpgInventoryInteractionPreviewState::Equip);
	if (!TestEqual(
			TEXT("The MainHand plan contains one atomic placement step"),
			Preview.PlacementPlan.Steps.Num(),
			1))
	{
		return false;
	}
	const FRpgInventoryPlacementStep& PlannedStep =
		Preview.PlacementPlan.Steps[0];
	TestEqual(
		TEXT("The MainHand plan resolves to Place"),
		PlannedStep.Resolution,
		ERpgInventoryPlacementResolution::Place);
	TestEqual(
		TEXT("The MainHand plan resolves the WeaponSlot1 Carry grid"),
		PlannedStep.Placement.GetContainerHandle(),
		WeaponSlot1);
	TestTrue(
		TEXT("The public MainHand target exactly matches the domain step"),
		ArePlacementSnapshotsExactlyEqual(
			Preview.ResolvedTargetPlacement,
			PlannedStep.Placement));
	TestEqual(
		TEXT("Pockets-to-MainHand preview preserves the full inventory snapshot"),
		MakeStrictInventorySignature(Inventory),
		InventoryBeforePreview);
	TestNull(
		TEXT("Preview alone cannot activate MainHand"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand));
	TestTrue(
		TEXT("The coordinator dispatches the accepted MainHand plan"),
		Coordinator->CommitPayloadToTarget(
			Payload,
			MainHandTarget));

	FRpgInventoryEntryView WeaponAfter;
	if (!TestTrue(
			TEXT("The equipped weapon remains addressable"),
			GetEntryView(
				Inventory,
				WeaponBefore.ItemId,
				WeaponAfter)))
	{
		return false;
	}
	TestEqual(
		TEXT("Pockets-to-MainHand preserves the persistent item id"),
		WeaponAfter.ItemId,
		WeaponBefore.ItemId);
	TestEqual(
		TEXT("Pockets-to-MainHand preserves the replicated entry id"),
		WeaponAfter.EntryId,
		WeaponBefore.EntryId);
	TestEqual(
		TEXT("Pockets-to-MainHand preserves the runtime instance"),
		WeaponAfter.Instance.Get(),
		WeaponBefore.Instance.Get());
	TestEqual(
		TEXT("Pockets-to-MainHand preserves the whole quantity"),
		WeaponAfter.StackCount,
		WeaponBefore.StackCount);
	TestTrue(
		TEXT("MainHand commit uses the exact previewed raw placement"),
		ArePlacementSnapshotsExactlyEqual(
			WeaponAfter.Placement,
			Preview.ResolvedTargetPlacement));
	TestNull(
		TEXT("The committed weapon leaves its original Pockets cell"),
		Inventory->GetItemAtContainerCell(Pockets, 0, 0));
	TestEqual(
		TEXT("Post-commit reconciliation activates the same physical weapon"),
		EquipmentLoadout->GetItemInEquipmentSlot(
			ERpgEquipmentSlot::MainHand),
		Weapon);
	TestTrue(
		TEXT("The accepted MainHand commit advances inventory revision"),
		Inventory->GetInventoryRevision() > RevisionBeforeCommit);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
