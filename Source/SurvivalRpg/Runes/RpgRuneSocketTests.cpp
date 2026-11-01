#include "RpgRuneAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "RpgInventoryFragment_Rune.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/Effects/RpgAbilityTuningEffectComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentAutomationTestTypes.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/SkillTrees/RpgSkillTreeViewModels.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeComponent.h"
#include "SurvivalRpg/Progression/SkillTrees/RpgSkillTreeDefinition.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillGameplayTags.h"
#include "SurvivalRpg/Progression/Skills/RpgTradeSkillProgressionComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneSocketTest_RankAbility, "Ability.AutomationRune.RankII");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneSocketTest_TunedAbility, "Ability.AutomationRune.Tuned");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneSocketTest_Tuning, "Ability.Tuning.AutomationRune.Value");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneSocketTest_Tree, "SkillTree.Tree.AutomationRune");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneSocketTest_PlainNode, "SkillTree.Node.AutomationRune.Plain");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneSocketTest_RuneBoundNode, "SkillTree.Node.AutomationRune.RuneBound");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneSocketTest_RuneBoundGranted, "SkillTree.AutomationRune.RuneBoundGranted");

namespace RpgRuneSocketTests
{
	const FName StorageId(TEXT("Storage"));

	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
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

		~FScopedTestWorld()
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

		UWorld* GetWorld() const { return World; }

		/** An actor-owned inventory with a 6x3 storage grid; its items are outered to the actor. */
		URpgInventoryManagerComponent* CreateInventory(const TCHAR* DebugName) const
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			AActor* OwnerActor = World ? World->SpawnActor<AActor>(SpawnParameters) : nullptr;
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
			FRpgInventoryGridSize GridSize;
			GridSize.Width = 6;
			GridSize.Height = 3;
			Inventory->SetDefaultGridSize(GridSize);
			return Inventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	FRpgInventoryGridPlacement MakePlacement(const FRpgInventoryContainerHandle& Container, const int32 X, const int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(Container);
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}

	FRpgInventoryGridPlacement Storage(const int32 X, const int32 Y)
	{
		return MakePlacement(FRpgInventoryContainerHandle::MakeRoot(StorageId), X, Y);
	}

	/** Socket X of the item ToolItem, wherever the tool sits. */
	FRpgInventoryGridPlacement Socket(
		const URpgInventoryManagerComponent& Inventory,
		URpgInventoryItemInstance* ToolItem,
		const int32 X)
	{
		FRpgInventoryGridPlacement ToolPlacement;
		Inventory.GetItemPlacement(ToolItem, ToolPlacement);
		return MakePlacement(
			FRpgInventoryContainerHandle::MakeItemOwned(
				ToolItem->GetItemId(),
				RpgRuneAutomationTestSocketGridId,
				ToolPlacement.GetContainerHandle().GetDirectChildDepth()),
			X,
			0);
	}

	bool FindEntry(const URpgInventoryManagerComponent& Inventory, const URpgInventoryItemInstance* Item, FRpgInventoryEntryView& OutEntry)
	{
		for (const FRpgInventoryEntryView& Entry : Inventory.GetAllEntries())
		{
			if (Item && Entry.ItemId == Item->GetItemId())
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}

	/** Moves Item's whole entry to Target through the production intent, as the inventory UI does. */
	ERpgInventoryMutationResultCode Move(
		URpgInventoryManagerComponent& Inventory,
		URpgInventoryItemInstance* Item,
		const FRpgInventoryGridPlacement& Target)
	{
		FRpgInventoryEntryView Entry;
		if (!FindEntry(Inventory, Item, Entry))
		{
			return ERpgInventoryMutationResultCode::ItemNotFound;
		}

		FRpgInventoryMoveIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = Entry.ItemId;
		Intent.ExpectedEntryId = Entry.EntryId;
		Intent.ExpectedSourcePlacement = Entry.Placement;
		Intent.ExpectedQuantity = Entry.StackCount;
		Intent.TargetPlacement = Target;
		return Inventory.MoveItem(Intent).Code;
	}

	/** Transfers Item's whole entry, with its subtree, from Source to Target in Destination. */
	ERpgInventoryMutationResultCode Transfer(
		URpgInventoryManagerComponent& Source,
		URpgInventoryManagerComponent& Destination,
		URpgInventoryItemInstance* Item,
		const FRpgInventoryGridPlacement& Target)
	{
		FRpgInventoryEntryView Entry;
		if (!FindEntry(Source, Item, Entry))
		{
			return ERpgInventoryMutationResultCode::ItemNotFound;
		}

		FRpgInventoryTransferIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = Entry.ItemId;
		Intent.ExpectedEntryId = Entry.EntryId;
		Intent.ExpectedSourcePlacement = Entry.Placement;
		Intent.ExpectedSourceQuantity = Entry.StackCount;
		Intent.Quantity = Entry.StackCount;
		Intent.TargetContainer = Target.GetContainerHandle();
		Intent.TargetPlacement = Target;
		return Source.TransferItem(&Destination, Intent).Code;
	}

	bool IsAt(const URpgInventoryManagerComponent& Inventory, const URpgInventoryItemInstance* Item, const FRpgInventoryGridPlacement& Placement)
	{
		FRpgInventoryEntryView Entry;
		return FindEntry(Inventory, Item, Entry) && Entry.Placement.GetContainerHandle() == Placement.GetContainerHandle() &&
			Entry.Placement.X == Placement.X && Entry.Placement.Y == Placement.Y;
	}

	TArray<FRpgInventoryItemId> SocketedIds(const URpgInventoryManagerComponent& Inventory, const URpgInventoryItemInstance* Tool)
	{
		TArray<FRpgInventoryItemId> Ids;
		for (const URpgInventoryItemInstance* Item : Inventory.GetSocketedItems(Tool))
		{
			Ids.Add(Item->GetItemId());
		}
		return Ids;
	}

	struct FSocketFixture
	{
		URpgInventoryManagerComponent* Inventory = nullptr;
		URpgInventoryItemInstance* Tool = nullptr;
		URpgInventoryItemInstance* RuneA = nullptr;
		URpgInventoryItemInstance* SecondRuneA = nullptr;
		URpgInventoryItemInstance* RuneB = nullptr;
		URpgInventoryItemInstance* Plain = nullptr;
	};

	/** Tool at 0/0, rune A at 1/0, a second rune A at 2/0, rune B at 3/0 and a plain item at 4/0 of storage. */
	bool MakeSocketFixture(FAutomationTestBase& Test, const FScopedTestWorld& TestWorld, FSocketFixture& Out)
	{
		ConfigureRuneAutomationTestDefinitions();
		Out.Inventory = TestWorld.CreateInventory(TEXT("RuneOwner"));
		if (!Test.TestNotNull(TEXT("The inventory exists"), Out.Inventory))
		{
			return false;
		}

		Out.Tool = Out.Inventory->AddItemDefinitionToPlacement(URpgRuneAutomationTestToolDefinition::StaticClass(), 1, Storage(0, 0));
		Out.RuneA = Out.Inventory->AddItemDefinitionToPlacement(URpgRuneAutomationTestRuneDefinition::StaticClass(), 1, Storage(1, 0));
		Out.SecondRuneA = Out.Inventory->AddItemDefinitionToPlacement(URpgRuneAutomationTestRuneDefinition::StaticClass(), 1, Storage(2, 0));
		Out.RuneB = Out.Inventory->AddItemDefinitionToPlacement(URpgRuneAutomationTestOtherRuneDefinition::StaticClass(), 1, Storage(3, 0));
		Out.Plain = Out.Inventory->AddItemDefinitionToPlacement(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1, Storage(4, 0));
		return Test.TestNotNull(TEXT("The tool is granted"), Out.Tool) &&
			Test.TestNotNull(TEXT("Rune A is granted"), Out.RuneA) &&
			Test.TestNotNull(TEXT("A second rune A is granted"), Out.SecondRuneA) &&
			Test.TestNotNull(TEXT("Rune B is granted"), Out.RuneB) &&
			Test.TestNotNull(TEXT("A plain item is granted"), Out.Plain);
	}

	URpgAbilitySet* MakeAbilitySet(const FGameplayTag AbilityId)
	{
		URpgAbilitySet* AbilitySet = NewObject<URpgAbilitySet>(GetTransientPackage(), NAME_None, RF_Transient);
		AbilitySet->AddGrantedGameplayAbility(URpgInventoryAutomationTestUseAbility::StaticClass(), 1, FGameplayTag(), AbilityId);
		return AbilitySet;
	}

	int32 CountSpecsWithId(const URpgAbilitySystemComponent& AbilitySystem, const FGameplayTag AbilityId)
	{
		int32 Count = 0;
		for (const FGameplayAbilitySpec& Spec : AbilitySystem.GetActivatableAbilities())
		{
			Count += Spec.GetDynamicSpecSourceTags().HasTagExact(AbilityId) ? 1 : 0;
		}
		return Count;
	}

	/** Restores the rank II ability set of the test rune when a test ends. */
	struct FScopedRankAbilitySet
	{
		explicit FScopedRankAbilitySet(const URpgAbilitySet* AbilitySet)
		{
			URpgRuneAutomationTestRuneDefinition::SetRankAbilitySet(2, AbilitySet);
		}

		~FScopedRankAbilitySet()
		{
			URpgRuneAutomationTestRuneDefinition::SetRankAbilitySet(2, nullptr);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneSocketPlacementRulesTest,
	"SurvivalRpg.Runes.Sockets.PlacementRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneSocketPlacementRulesTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneSocketTests;

	FScopedTestWorld TestWorld;
	FSocketFixture Fixture;
	if (!MakeSocketFixture(*this, TestWorld, Fixture))
	{
		return false;
	}
	URpgInventoryManagerComponent& Inventory = *Fixture.Inventory;
	URpgInventoryItemInstance* Tool = Fixture.Tool;

	TestEqual(TEXT("A rune goes into a free socket"),
		Move(Inventory, Fixture.RuneA, Socket(Inventory, Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestTrue(TEXT("The tool reports the socketed rune"),
		SocketedIds(Inventory, Tool) == TArray<FRpgInventoryItemId>{Fixture.RuneA->GetItemId()});

	TestEqual(TEXT("A second rune of the same kind is rejected"),
		Move(Inventory, Fixture.SecondRuneA, Socket(Inventory, Tool, 1)),
		ERpgInventoryMutationResultCode::DuplicateSocketedItem);
	TestTrue(TEXT("The rejected rune stays where it was"), IsAt(Inventory, Fixture.SecondRuneA, Storage(2, 0)));

	TestEqual(TEXT("A rune of another kind takes the next socket"),
		Move(Inventory, Fixture.RuneB, Socket(Inventory, Tool, 1)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("An item that is no rune is rejected"),
		Move(Inventory, Fixture.Plain, Socket(Inventory, Tool, 2)), ERpgInventoryMutationResultCode::ItemNotAllowed);

	TestEqual(TEXT("A rune dropped on the socketed rune of its kind swaps with it"),
		Move(Inventory, Fixture.SecondRuneA, Socket(Inventory, Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestTrue(TEXT("The new rune sits in the socket"), IsAt(Inventory, Fixture.SecondRuneA, Socket(Inventory, Tool, 0)));
	TestTrue(TEXT("The replaced rune takes the new rune's place"), IsAt(Inventory, Fixture.RuneA, Storage(2, 0)));

	TestEqual(TEXT("A socketed rune moves to another socket of its tool"),
		Move(Inventory, Fixture.SecondRuneA, Socket(Inventory, Tool, 2)), ERpgInventoryMutationResultCode::Success);
	TestTrue(TEXT("The sockets list the runes in socket order"),
		SocketedIds(Inventory, Tool) == TArray<FRpgInventoryItemId>{Fixture.RuneB->GetItemId(), Fixture.SecondRuneA->GetItemId()});

	URpgInventoryItemInstance* Bag =
		Inventory.AddItemDefinitionToPlacement(URpgRuneAutomationTestBagDefinition::StaticClass(), 1, Storage(0, 1));
	if (!TestNotNull(TEXT("A bag that refuses nested containers is granted"), Bag))
	{
		return false;
	}
	const FRpgInventoryContainerHandle BagHandle = FRpgInventoryContainerHandle::MakeItemOwned(Bag->GetItemId(), TEXT("Main"), 1);
	TestEqual(TEXT("A socketed tool is no nested container, so the bag takes it"),
		Move(Inventory, Tool, MakePlacement(BagHandle, 0, 0)), ERpgInventoryMutationResultCode::Success);
	TestTrue(TEXT("The runes move with the tool"),
		SocketedIds(Inventory, Tool) == TArray<FRpgInventoryItemId>{Fixture.RuneB->GetItemId(), Fixture.SecondRuneA->GetItemId()});
	TestEqual(TEXT("The socket rule still holds inside the bag"),
		Move(Inventory, Fixture.RuneA, Socket(Inventory, Tool, 0)), ERpgInventoryMutationResultCode::DuplicateSocketedItem);

	URpgInventoryFragment_ItemContainer* InvalidSockets = NewObject<URpgInventoryFragment_ItemContainer>();
	FRpgInventoryItemContainerDefinition& InvalidGrid = InvalidSockets->ProvidedContainers.AddDefaulted_GetRef();
	InvalidGrid.ContainerId = TEXT("Sockets");
	InvalidGrid.GridSize.Width = 3;
	InvalidGrid.GridSize.Height = 1;
	InvalidGrid.bSocketGrid = true;
	InvalidGrid.bAllowNestedContainers = true;
	TestFalse(TEXT("A socket grid that allows nested containers is invalid"),
		InvalidSockets->HasStructurallyValidProvidedContainers());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneSocketTransferTest,
	"SurvivalRpg.Runes.Sockets.TransferKeepsRunesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneSocketTransferTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneSocketTests;

	FScopedTestWorld TestWorld;
	FSocketFixture Fixture;
	if (!MakeSocketFixture(*this, TestWorld, Fixture))
	{
		return false;
	}
	URpgInventoryManagerComponent& Player = *Fixture.Inventory;
	URpgInventoryManagerComponent* Chest = TestWorld.CreateInventory(TEXT("RuneChest"));
	if (!TestNotNull(TEXT("The chest exists"), Chest))
	{
		return false;
	}

	TestEqual(TEXT("Rune A goes into the tool"),
		Move(Player, Fixture.RuneA, Socket(Player, Fixture.Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The second rune A goes into the chest"),
		Transfer(Player, *Chest, Fixture.SecondRuneA, Storage(0, 0)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Rune B goes into the chest"),
		Transfer(Player, *Chest, Fixture.RuneB, Storage(1, 0)), ERpgInventoryMutationResultCode::Success);

	TestEqual(TEXT("A rune of a socketed kind cannot be transferred into the tool"),
		Transfer(*Chest, Player, Fixture.SecondRuneA, Socket(Player, Fixture.Tool, 1)),
		ERpgInventoryMutationResultCode::DuplicateSocketedItem);
	// A transfer rebuilds the item in its new inventory, so the chest holds new instances under the same ids.
	TestNotNull(TEXT("The chest keeps the rejected rune"), Chest->FindItemById(Fixture.SecondRuneA->GetItemId()));
	TestEqual(TEXT("A rune of another kind is transferred into a socket"),
		Transfer(*Chest, Player, Fixture.RuneB, Socket(Player, Fixture.Tool, 1)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The tool holds rune A and rune B"), Player.GetSocketedItems(Fixture.Tool).Num(), 2);

	const FRpgInventoryItemId ToolId = Fixture.Tool->GetItemId();
	const FRpgInventoryItemId RuneAId = Fixture.RuneA->GetItemId();
	const FRpgInventoryItemId RuneBId = Fixture.RuneB->GetItemId();
	TestEqual(TEXT("The socketed tool moves into the chest as a whole"),
		Transfer(Player, *Chest, Fixture.Tool, Storage(2, 0)), ERpgInventoryMutationResultCode::Success);
	URpgInventoryItemInstance* ChestTool = Chest->FindItemById(ToolId);
	if (!TestNotNull(TEXT("The chest holds the tool under its id"), ChestTool))
	{
		return false;
	}
	TestTrue(TEXT("The runes travel in the tool's sockets"),
		SocketedIds(*Chest, ChestTool) == TArray<FRpgInventoryItemId>{RuneAId, RuneBId});
	TestNull(TEXT("The player keeps no copy of the tool"), Player.FindItemById(ToolId));
	TestNull(TEXT("The player keeps no copy of rune A"), Player.FindItemById(RuneAId));
	TestNull(TEXT("The player keeps no copy of rune B"), Player.FindItemById(RuneBId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneRankPersistenceTest,
	"SurvivalRpg.Runes.RankPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneRankPersistenceTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneSocketTests;

	FScopedTestWorld TestWorld;
	FSocketFixture Fixture;
	if (!MakeSocketFixture(*this, TestWorld, Fixture))
	{
		return false;
	}
	URpgInventoryManagerComponent& Inventory = *Fixture.Inventory;

	TestEqual(TEXT("A new rune starts at rank I"), Fixture.RuneA->GetRuneRank(), 1);
	TestTrue(TEXT("Rank III is accepted"), Fixture.RuneA->SetRuneRank(3));
	TestFalse(TEXT("Rank IV does not exist"), Fixture.RuneA->SetRuneRank(4));
	TestFalse(TEXT("Rank 0 does not exist"), Fixture.RuneA->SetRuneRank(0));
	TestEqual(TEXT("A rejected rank changes nothing"), Fixture.RuneA->GetRuneRank(), 3);
	TestFalse(TEXT("A rune with one rank cannot reach rank II"), Fixture.RuneB->SetRuneRank(2));
	TestEqual(TEXT("An item that is no rune has no rank"), Fixture.Plain->GetRuneRank(), 0);
	TestFalse(TEXT("An item that is no rune takes no rank"), Fixture.Plain->SetRuneRank(1));

	TestEqual(TEXT("Rune A goes into the tool"),
		Move(Inventory, Fixture.RuneA, Socket(Inventory, Fixture.Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Rune B goes into the tool"),
		Move(Inventory, Fixture.RuneB, Socket(Inventory, Fixture.Tool, 2)), ERpgInventoryMutationResultCode::Success);

	const FRpgInventoryGraphSaveData SaveData = Inventory.ExportInventoryGraph();
	URpgInventoryManagerComponent* Loaded = TestWorld.CreateInventory(TEXT("RuneLoaded"));
	FRpgInventoryMutationResult LoadResult;
	if (!TestNotNull(TEXT("The loading inventory exists"), Loaded) ||
		!TestTrue(TEXT("The saved graph loads"), Loaded->RestoreInventoryGraph(SaveData, LoadResult)))
	{
		return false;
	}

	URpgInventoryItemInstance* LoadedTool = Loaded->FindItemById(Fixture.Tool->GetItemId());
	URpgInventoryItemInstance* LoadedRuneA = Loaded->FindItemById(Fixture.RuneA->GetItemId());
	if (!TestNotNull(TEXT("The tool loads under its id"), LoadedTool) ||
		!TestNotNull(TEXT("Rune A loads under its id"), LoadedRuneA))
	{
		return false;
	}
	TestTrue(TEXT("The runes load in their sockets"),
		SocketedIds(*Loaded, LoadedTool) == TArray<FRpgInventoryItemId>{Fixture.RuneA->GetItemId(), Fixture.RuneB->GetItemId()});
	TestEqual(TEXT("The rank follows the rune through the save"), LoadedRuneA->GetRuneRank(), 3);
	TestTrue(TEXT("Rune A keeps its socket"), IsAt(*Loaded, LoadedRuneA, Socket(*Loaded, LoadedTool, 0)));

	const FRpgInventorySavedItem* SavedRuneA = SaveData.Items.FindByPredicate(
		[&Fixture](const FRpgInventorySavedItem& Item)
		{
			return Item.ItemId == Fixture.RuneA->GetItemId();
		});
	if (!TestNotNull(TEXT("The save holds rune A"), SavedRuneA))
	{
		return false;
	}

	FRpgInventoryGraphSaveData DuplicateKind = SaveData;
	FRpgInventorySavedItem& Copy = DuplicateKind.Items.Add_GetRef(*SavedRuneA);
	Copy.ItemId = FRpgInventoryItemId::NewId();
	Copy.Placement.X = 1;
	URpgInventoryManagerComponent* Rejecting = TestWorld.CreateInventory(TEXT("RuneRejecting"));
	FRpgInventoryMutationResult RejectResult;
	TestFalse(TEXT("A save with two runes of one kind in a tool does not load"),
		Rejecting->RestoreInventoryGraph(DuplicateKind, RejectResult));
	TestTrue(TEXT("The rejected load changes nothing"), Rejecting->GetAllEntries().IsEmpty());

	FRpgInventoryGraphSaveData InvalidRank = SaveData;
	for (FRpgInventorySavedItem& Item : InvalidRank.Items)
	{
		for (FRpgInventoryFragmentStatePayload& Payload : Item.RuntimeState)
		{
			if (Item.ItemId == Fixture.RuneA->GetItemId() && Payload.FragmentId == TEXT("Inventory.Rune.Rank") &&
				Payload.Payload.Num() == 1)
			{
				Payload.Payload[0] = 4;
			}
		}
	}
	URpgInventoryManagerComponent* RankRejecting = TestWorld.CreateInventory(TEXT("RuneRankRejecting"));
	FRpgInventoryMutationResult RankResult;
	TestFalse(TEXT("A save with a rank above the rune's highest does not load"),
		RankRejecting->RestoreInventoryGraph(InvalidRank, RankResult));
	TestTrue(TEXT("The rejected rank changes nothing"), RankRejecting->GetAllEntries().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneSocketEquippedGrantsTest,
	"SurvivalRpg.Runes.Sockets.EquippedToolGrants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneSocketEquippedGrantsTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneSocketTests;

	FScopedTestWorld TestWorld;
	FSocketFixture Fixture;
	if (!MakeSocketFixture(*this, TestWorld, Fixture))
	{
		return false;
	}
	URpgInventoryManagerComponent& Inventory = *Fixture.Inventory;
	ARpgEquipmentAutomationTestPawn* Pawn = TestWorld.GetWorld()->SpawnActor<ARpgEquipmentAutomationTestPawn>();
	URpgAbilitySystemComponent* AbilitySystem = Pawn ? Pawn->GetRpgAbilitySystemComponent() : nullptr;
	URpgEquipmentManagerComponent* Equipment = Pawn ? Pawn->GetEquipmentManagerComponent() : nullptr;
	if (!TestNotNull(TEXT("The pawn has an ability system"), AbilitySystem) ||
		!TestNotNull(TEXT("The pawn has an equipment manager"), Equipment))
	{
		return false;
	}
	AbilitySystem->InitAbilityActorInfo(Pawn, Pawn);
	AbilitySystem->SetForceGrantAuthorityForTests(true);
	FScopedRankAbilitySet RankAbility(MakeAbilitySet(TAG_RuneSocketTest_RankAbility));

	const FGameplayTag RuneTagA = FGameplayTag::RequestGameplayTag(TEXT("Rune.AutomationTest.A"));
	const FGameplayTag RuneTagB = FGameplayTag::RequestGameplayTag(TEXT("Rune.AutomationTest.B"));
	const FGameplayTag RankI = FGameplayTag::RequestGameplayTag(TEXT("Rune.AutomationTest.Rank.I"));
	const FGameplayTag RankII = FGameplayTag::RequestGameplayTag(TEXT("Rune.AutomationTest.Rank.II"));

	URpgEquipmentInstance* MainHand = Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(), ERpgEquipmentSlot::MainHand, Fixture.Tool);
	if (!TestNotNull(TEXT("The tool equips"), MainHand))
	{
		return false;
	}
	TestEqual(TEXT("A tool without runes grants no rune"), AbilitySystem->GetTagCount(RuneTagA), 0);

	TestEqual(TEXT("Rune A goes into the equipped tool"),
		Move(Inventory, Fixture.RuneA, Socket(Inventory, Fixture.Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The wielder carries rune A"), AbilitySystem->GetTagCount(RuneTagA), 1);
	TestEqual(TEXT("Rank I grants its tag"), AbilitySystem->GetTagCount(RankI), 1);

	TestEqual(TEXT("Rune A moves to another socket"),
		Move(Inventory, Fixture.RuneA, Socket(Inventory, Fixture.Tool, 1)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Moving between sockets grants nothing twice"), AbilitySystem->GetTagCount(RuneTagA), 1);
	TestEqual(TEXT("A plain move elsewhere changes no grant"),
		Move(Inventory, Fixture.Plain, Storage(5, 2)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Rank I is still granted once"), AbilitySystem->GetTagCount(RankI), 1);

	TestTrue(TEXT("Rune A reaches rank II"), Fixture.RuneA->SetRuneRank(2));
	TestTrue(TEXT("The rank change is published"), Inventory.MarkItemRuntimeStateChanged(Fixture.RuneA->GetItemId()));
	TestEqual(TEXT("The published rank removes rank I's grants at once"), AbilitySystem->GetTagCount(RankI), 0);
	TestEqual(TEXT("The published rank grants rank II once"), AbilitySystem->GetTagCount(RankII), 1);
	TestEqual(TEXT("Rune B goes into the tool"),
		Move(Inventory, Fixture.RuneB, Socket(Inventory, Fixture.Tool, 2)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Rune B is granted"), AbilitySystem->GetTagCount(RuneTagB), 1);
	TestEqual(TEXT("Rank I left with the old rank"), AbilitySystem->GetTagCount(RankI), 0);
	TestEqual(TEXT("Rank II arrived"), AbilitySystem->GetTagCount(RankII), 1);
	TestEqual(TEXT("Rank II's ability is granted once"), CountSpecsWithId(*AbilitySystem, TAG_RuneSocketTest_RankAbility), 1);
	TestEqual(TEXT("Rune A is still granted once"), AbilitySystem->GetTagCount(RuneTagA), 1);

	TestEqual(TEXT("Rune A leaves the tool"),
		Move(Inventory, Fixture.RuneA, Storage(1, 0)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Rune A's grants leave with it"), AbilitySystem->GetTagCount(RuneTagA), 0);
	TestEqual(TEXT("Rank II leaves with rune A"), AbilitySystem->GetTagCount(RankII), 0);
	TestEqual(TEXT("Rank II's ability leaves with rune A"), CountSpecsWithId(*AbilitySystem, TAG_RuneSocketTest_RankAbility), 0);
	TestEqual(TEXT("Rune B stays"), AbilitySystem->GetTagCount(RuneTagB), 1);

	Equipment->UnequipItem(MainHand);
	TestEqual(TEXT("Unequipping the tool removes its runes"), AbilitySystem->GetTagCount(RuneTagB), 0);
	TestEqual(TEXT("Rune A goes back into the unequipped tool"),
		Move(Inventory, Fixture.RuneA, Socket(Inventory, Fixture.Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("An unequipped tool grants nothing"), AbilitySystem->GetTagCount(RuneTagA), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneSocketedTuningTest,
	"SurvivalRpg.Runes.Tunings.SocketedRuneTunesItsTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneSocketedTuningTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneSocketTests;
	FScopedTestWorld TestWorld;
	FSocketFixture Fixture;
	if (!MakeSocketFixture(*this, TestWorld, Fixture))
	{
		return false;
	}
	URpgInventoryManagerComponent& Inventory = *Fixture.Inventory;
	URpgInventoryItemInstance* OtherTool = Inventory.AddItemDefinitionToPlacement(
		URpgRuneAutomationTestToolDefinition::StaticClass(), 1, Storage(5, 0));
	URpgInventoryFragment_Rune* RuneA = const_cast<URpgInventoryFragment_Rune*>(
		URpgInventoryFragment_Rune::FindRuneOfItem(Fixture.RuneA));
	if (!TestNotNull(TEXT("A second tool is granted"), OtherTool) || !TestNotNull(TEXT("Rune A has its fragment"), RuneA) ||
		!TestEqual(TEXT("Rune A has three ranks"), RuneA->Ranks.Num(), 3))
	{
		return false;
	}

	// Rank I adds 0.1 for every ability of the tool; rank II adds 0.15 and doubles the value for one ability id.
	const TArray<FRpgRuneRank> AuthoredRanks = RuneA->Ranks;
	ON_SCOPE_EXIT
	{
		RuneA->Ranks = AuthoredRanks;
	};
	auto MakeTuning = [](const FGameplayTag AbilityId, const ERpgSkillTreeTuningOperation Operation, const float Value)
	{
		FRpgSkillTreeAbilityTuning Tuning;
		Tuning.AbilityIdTag = AbilityId;
		Tuning.TuningTag = TAG_RuneSocketTest_Tuning;
		Tuning.Operation = Operation;
		Tuning.Value = Value;
		return Tuning;
	};
	RuneA->Ranks[0].AbilityTunings = {MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Add, 0.1f)};
	RuneA->Ranks[1].AbilityTunings = {
		MakeTuning(FGameplayTag(), ERpgSkillTreeTuningOperation::Add, 0.15f),
		MakeTuning(TAG_RuneSocketTest_TunedAbility, ERpgSkillTreeTuningOperation::Multiply, 2.0f)};

	URpgEquipmentInstance* ToolEquipment = NewObject<URpgEquipmentInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	URpgEquipmentInstance* OtherEquipment = NewObject<URpgEquipmentInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	ToolEquipment->SetInstigator(Fixture.Tool);
	OtherEquipment->SetInstigator(OtherTool);
	const FGameplayAbilitySpec ToolSpec(URpgInventoryAutomationTestUseAbility::StaticClass(), 1, INDEX_NONE, ToolEquipment);
	FGameplayAbilitySpec TunedAbilitySpec(URpgInventoryAutomationTestUseAbility::StaticClass(), 1, INDEX_NONE, ToolEquipment);
	TunedAbilitySpec.GetDynamicSpecSourceTags().AddTag(TAG_RuneSocketTest_TunedAbility);
	const FGameplayAbilitySpec OtherSpec(URpgInventoryAutomationTestUseAbility::StaticClass(), 1, INDEX_NONE, OtherEquipment);
	const FGameplayAbilityActorInfo ActorInfo;
	auto Resolve = [&ActorInfo](const FGameplayAbilitySpec& Spec)
	{
		return URpgSkillTreeComponent::ResolveAbilityTuningForSpec(Spec, ActorInfo, TAG_RuneSocketTest_Tuning, 1.0f);
	};

	TestEqual(TEXT("A rune in storage tunes nothing"), Resolve(ToolSpec), 1.0f);
	TestEqual(TEXT("Rune A goes into the tool"), Move(Inventory, Fixture.RuneA, Socket(Inventory, Fixture.Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("Rank I tunes the abilities of its tool"), Resolve(ToolSpec), 1.1f);
	TestEqual(TEXT("The rune never tunes another tool"), Resolve(OtherSpec), 1.0f);

	TestTrue(TEXT("Rune A reaches rank II"), Fixture.RuneA->SetRuneRank(2));
	TestEqual(TEXT("Rank II replaces rank I's tunings"), Resolve(ToolSpec), 1.15f);
	TestEqual(TEXT("An ability-specific tuning applies only to that ability"), Resolve(TunedAbilitySpec), 2.3f);

	TestEqual(TEXT("Rune A leaves the tool"), Move(Inventory, Fixture.RuneA, Storage(1, 1)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("An unsocketed rune tunes nothing"), Resolve(ToolSpec), 1.0f);
	TestEqual(TEXT("A spec without source equipment keeps its base value"),
		URpgSkillTreeComponent::ResolveAbilityTuningForSpec(
			FGameplayAbilitySpec(URpgInventoryAutomationTestUseAbility::StaticClass()), ActorInfo, TAG_RuneSocketTest_Tuning, 1.0f),
		1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgActiveEffectTuningFoldOrderTest,
	"SurvivalRpg.AbilitySystem.Tuning.ActiveEffectTunings.FoldAfterTreeAndRunes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgActiveEffectTuningFoldOrderTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneSocketTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	ARpgEquipmentAutomationTestPawn* Pawn = World ? World->SpawnActor<ARpgEquipmentAutomationTestPawn>() : nullptr;
	if (!TestNotNull(TEXT("The player state spawns"), PlayerState) || !TestNotNull(TEXT("The pawn spawns"), Pawn))
	{
		return false;
	}
	Pawn->SetPlayerState(PlayerState);
	URpgAbilitySystemComponent* AbilitySystem = Pawn->GetRpgAbilitySystemComponent();
	URpgSkillTreeComponent* SkillTrees = PlayerState->GetSkillTreeComponent();
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	if (!TestNotNull(TEXT("The pawn has an ability system"), AbilitySystem) ||
		!TestNotNull(TEXT("The player has skill trees"), SkillTrees) ||
		!TestNotNull(TEXT("The player has an inventory"), Inventory))
	{
		return false;
	}
	AbilitySystem->InitAbilityActorInfo(Pawn, Pawn);

	// Without a controller the player inventory has no layout; its default storage grid holds the items.
	FRpgInventoryGridSize GridSize;
	GridSize.Width = 6;
	GridSize.Height = 3;
	Inventory->SetDefaultGridSize(GridSize);
	URpgInventoryItemInstance* Tool = Inventory->AddItemDefinitionToPlacement(
		URpgRuneAutomationTestTreeToolDefinition::StaticClass(), 1, Storage(0, 0));
	URpgInventoryItemInstance* Rune = Inventory->AddItemDefinitionToPlacement(
		URpgRuneAutomationTestRuneDefinition::StaticClass(), 1, Storage(1, 0));
	URpgInventoryFragment_Rune* RuneA = const_cast<URpgInventoryFragment_Rune*>(URpgInventoryFragment_Rune::FindRuneOfItem(Rune));
	if (!TestNotNull(TEXT("The tool lies in storage"), Tool) || !TestNotNull(TEXT("Rune A lies in storage"), Rune) ||
		!TestNotNull(TEXT("Rune A has its fragment"), RuneA) || !TestTrue(TEXT("Rune A has ranks"), !RuneA->Ranks.IsEmpty()))
	{
		return false;
	}

	auto MakeTuning = [](const ERpgSkillTreeTuningOperation Operation, const float Value)
	{
		FRpgSkillTreeAbilityTuning Tuning;
		Tuning.TuningTag = TAG_RuneSocketTest_Tuning;
		Tuning.Operation = Operation;
		Tuning.Value = Value;
		return Tuning;
	};
	// Every source sets and adds: the tree sets 4 and adds 10, rune A's rank I sets 6 and adds 0.5, the effect sets 20
	// and adds 1.
	URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Tree->TreeTag = TAG_RuneSocketTest_Tree;
	Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
	Tree->MaxPoints = 10;
	FRpgSkillTreeNode& Plain = Tree->Nodes.AddDefaulted_GetRef();
	Plain.NodeTag = TAG_RuneSocketTest_PlainNode;
	Plain.AbilityTunings = {
		MakeTuning(ERpgSkillTreeTuningOperation::Set, 4.0f), MakeTuning(ERpgSkillTreeTuningOperation::Add, 10.0f)};
	URpgRuneAutomationTestTreeToolDefinition::SetTestSkillTree(Tree);
	const TArray<FRpgRuneRank> AuthoredRanks = RuneA->Ranks;
	ON_SCOPE_EXIT
	{
		URpgRuneAutomationTestTreeToolDefinition::SetTestSkillTree(nullptr);
		RuneA->Ranks = AuthoredRanks;
	};
	RuneA->Ranks[0].AbilityTunings = {
		MakeTuning(ERpgSkillTreeTuningOperation::Set, 6.0f), MakeTuning(ERpgSkillTreeTuningOperation::Add, 0.5f)};
	const UGameplayEffect* Spell = URpgAbilityTuningEffectComponent::MakeTransientEffectForTests({
		MakeTuning(ERpgSkillTreeTuningOperation::Set, 20.0f), MakeTuning(ERpgSkillTreeTuningOperation::Add, 1.0f)});

	SkillTrees->RegisterSkillTree(Tree);
	URpgTradeSkillProgressionComponent* TradeSkills = PlayerState->GetTradeSkillProgressionComponent();
	for (int32 Guard = 0; Guard < 200 && TradeSkills->GetSkillLevelByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Logging) < 4; ++Guard)
	{
		TradeSkills->AddSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Logging, 100.0f);
	}
	TestEqual(TEXT("Plain is learned"), SkillTrees->UnlockNode(TAG_RuneSocketTest_Tree, TAG_RuneSocketTest_PlainNode), ERpgSkillTreeUnlockResult::Unlockable);

	URpgEquipmentInstance* ToolEquipment = NewObject<URpgEquipmentInstance>(Pawn, NAME_None, RF_Transient);
	ToolEquipment->SetInstigator(Tool);
	const FGameplayAbilitySpec ToolSpec(URpgInventoryAutomationTestUseAbility::StaticClass(), 1, INDEX_NONE, ToolEquipment);
	const FGameplayAbilitySpec UnsourcedSpec(URpgInventoryAutomationTestUseAbility::StaticClass());
	const FGameplayAbilityActorInfo& ActorInfo = *AbilitySystem->AbilityActorInfo;
	auto Tuned = [&ActorInfo](const FGameplayAbilitySpec& Spec)
	{
		return URpgGameplayAbility::GetTunedValueForSpec(Spec, ActorInfo, TAG_RuneSocketTest_Tuning, 1.0f);
	};

	TestEqual(TEXT("The tree alone sets 4 and adds 10"), Tuned(ToolSpec), 14.0f);
	TestEqual(TEXT("Rune A goes into the tool"), Move(*Inventory, Rune, Socket(*Inventory, Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The rune's Set follows the tree's, and the Adds sum"), Tuned(ToolSpec), 16.5f);

	const FActiveGameplayEffectHandle SpellHandle =
		AbilitySystem->ApplyGameplayEffectToSelf(Spell, 1.0f, AbilitySystem->MakeEffectContext());
	TestTrue(TEXT("The spell effect applies"), SpellHandle.IsValid());
	TestEqual(TEXT("The effect's Set beats the tree's and the rune's, and Adds sum across all three"), Tuned(ToolSpec), 31.5f);
	TestEqual(TEXT("The item-only resolver ignores active effects"),
		URpgSkillTreeComponent::ResolveAbilityTuningForSpec(ToolSpec, ActorInfo, TAG_RuneSocketTest_Tuning, 1.0f), 16.5f);
	TestEqual(TEXT("The effect alone tunes an ability without source item"), Tuned(UnsourcedSpec), 21.0f);

	TestTrue(TEXT("The spell effect is removed"), AbilitySystem->RemoveActiveGameplayEffect(SpellHandle));
	TestEqual(TEXT("Without the effect the item sources remain"), Tuned(ToolSpec), 16.5f);
	TestEqual(TEXT("Without the effect an unsourced ability keeps its base"), Tuned(UnsourcedSpec), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneBoundTalentTest,
	"SurvivalRpg.Runes.Talents.RuneBoundNodesSleepWithoutTheirRune",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneBoundTalentTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneSocketTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	ARpgEquipmentAutomationTestPawn* Pawn = World ? World->SpawnActor<ARpgEquipmentAutomationTestPawn>() : nullptr;
	if (!TestNotNull(TEXT("The player state spawns"), PlayerState) || !TestNotNull(TEXT("The pawn spawns"), Pawn))
	{
		return false;
	}
	Pawn->SetPlayerState(PlayerState);
	URpgAbilitySystemComponent* AbilitySystem = Pawn->GetRpgAbilitySystemComponent();
	URpgEquipmentManagerComponent* Equipment = Pawn->GetEquipmentManagerComponent();
	URpgSkillTreeComponent* SkillTrees = PlayerState->GetSkillTreeComponent();
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	if (!TestNotNull(TEXT("The pawn has an ability system"), AbilitySystem) ||
		!TestNotNull(TEXT("The pawn has an equipment manager"), Equipment) ||
		!TestNotNull(TEXT("The player has skill trees"), SkillTrees) ||
		!TestNotNull(TEXT("The player has an inventory"), Inventory))
	{
		return false;
	}
	AbilitySystem->InitAbilityActorInfo(Pawn, Pawn);
	AbilitySystem->SetForceGrantAuthorityForTests(true);

	// Plain always works; RuneBound needs rune A in the tool's sockets.
	const FGameplayTag RuneTagA = FGameplayTag::RequestGameplayTag(TEXT("Rune.AutomationTest.A"));
	URpgSkillTreeDefinition* Tree = NewObject<URpgSkillTreeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Tree->TreeTag = TAG_RuneSocketTest_Tree;
	Tree->MasterySkillTag = RpgTradeSkillGameplayTags::Skill_Gathering_Logging;
	Tree->MaxPoints = 10;
	auto AddTuning = [](FRpgSkillTreeNode& Node, const float Value)
	{
		FRpgSkillTreeAbilityTuning& Tuning = Node.AbilityTunings.AddDefaulted_GetRef();
		Tuning.TuningTag = TAG_RuneSocketTest_Tuning;
		Tuning.Operation = ERpgSkillTreeTuningOperation::Add;
		Tuning.Value = Value;
	};
	FRpgSkillTreeNode& Plain = Tree->Nodes.AddDefaulted_GetRef();
	Plain.NodeTag = TAG_RuneSocketTest_PlainNode;
	AddTuning(Plain, 10.0f);
	FRpgSkillTreeNode& RuneBound = Tree->Nodes.AddDefaulted_GetRef();
	RuneBound.NodeTag = TAG_RuneSocketTest_RuneBoundNode;
	RuneBound.Column = 1;
	RuneBound.RequiredRunes.AddTag(RuneTagA);
	RuneBound.GrantedTags.AddTag(TAG_RuneSocketTest_RuneBoundGranted);
	AddTuning(RuneBound, 50.0f);
	URpgRuneAutomationTestTreeToolDefinition::SetTestSkillTree(Tree);
	ON_SCOPE_EXIT
	{
		URpgRuneAutomationTestTreeToolDefinition::SetTestSkillTree(nullptr);
	};

	// Without a controller the player inventory has no layout; its default storage grid holds the items.
	FRpgInventoryGridSize GridSize;
	GridSize.Width = 6;
	GridSize.Height = 3;
	Inventory->SetDefaultGridSize(GridSize);
	URpgInventoryItemInstance* Tool = Inventory->AddItemDefinitionToPlacement(
		URpgRuneAutomationTestTreeToolDefinition::StaticClass(), 1, Storage(0, 0));
	URpgInventoryItemInstance* Rune = Inventory->AddItemDefinitionToPlacement(
		URpgRuneAutomationTestRuneDefinition::StaticClass(), 1, Storage(1, 0));
	if (!TestNotNull(TEXT("The tool lies in storage"), Tool) || !TestNotNull(TEXT("Rune A lies in storage"), Rune))
	{
		return false;
	}
	URpgEquipmentInstance* MainHand = Equipment->EquipItemInSlotWithInstigator(
		URpgEquipmentAutomationTestSwordDefinition::StaticClass(), ERpgEquipmentSlot::MainHand, Tool);
	if (!TestNotNull(TEXT("The tool is in hand"), MainHand))
	{
		return false;
	}

	URpgTradeSkillProgressionComponent* TradeSkills = PlayerState->GetTradeSkillProgressionComponent();
	for (int32 Guard = 0; Guard < 200 && TradeSkills->GetSkillLevelByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Logging) < 4; ++Guard)
	{
		TradeSkills->AddSkillXPByTag(RpgTradeSkillGameplayTags::Skill_Gathering_Logging, 100.0f);
	}
	TestEqual(TEXT("Plain is learned"), SkillTrees->UnlockNode(TAG_RuneSocketTest_Tree, TAG_RuneSocketTest_PlainNode), ERpgSkillTreeUnlockResult::Unlockable);
	TestEqual(TEXT("A rune-bound talent can be learned before its rune is socketed"),
		SkillTrees->UnlockNode(TAG_RuneSocketTest_Tree, TAG_RuneSocketTest_RuneBoundNode), ERpgSkillTreeUnlockResult::Unlockable);

	const FGameplayAbilitySpec ToolSpec(URpgInventoryAutomationTestUseAbility::StaticClass(), 1, INDEX_NONE, MainHand);
	const FGameplayAbilityActorInfo& ActorInfo = *AbilitySystem->AbilityActorInfo;
	auto Tuned = [&ToolSpec, &ActorInfo]()
	{
		return URpgSkillTreeComponent::ResolveAbilityTuningForSpec(ToolSpec, ActorInfo, TAG_RuneSocketTest_Tuning, 0.0f);
	};
	URpgSkillTreeViewModel* TreeView = NewObject<URpgSkillTreeViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	TreeView->BindSkillTree(SkillTrees, TAG_RuneSocketTest_Tree);
	auto Dormant = [TreeView]()
	{
		TreeView->Refresh();
		const URpgSkillTreeNodeViewModel* Node = TreeView->FindNode(TAG_RuneSocketTest_RuneBoundNode);
		return Node && Node->NeedsRunes() && Node->IsDormant();
	};

	TestFalse(TEXT("Without rune A the talent sleeps on the tool"), URpgSkillTreeComponent::IsNodeActiveForItem(RuneBound, Tool));
	TestEqual(TEXT("A sleeping talent adds no tuning"), Tuned(), 10.0f);
	TestEqual(TEXT("A sleeping talent grants nothing"), AbilitySystem->GetTagCount(TAG_RuneSocketTest_RuneBoundGranted), 0);
	TestTrue(TEXT("The skill screen shows it learned but sleeping"), Dormant());

	TestEqual(TEXT("Rune A goes into the tool in hand"),
		Move(*Inventory, Rune, Socket(*Inventory, Tool, 0)), ERpgInventoryMutationResultCode::Success);
	TestTrue(TEXT("With rune A the talent works"), URpgSkillTreeComponent::IsNodeActiveForItem(RuneBound, Tool));
	TestEqual(TEXT("The awake talent tunes the tool's abilities"), Tuned(), 60.0f);
	TestEqual(TEXT("The awake talent is granted once"), AbilitySystem->GetTagCount(TAG_RuneSocketTest_RuneBoundGranted), 1);
	TestFalse(TEXT("The skill screen shows it awake"), Dormant());

	// The rune bar lists the sockets of the tool in hand and highlights the talents of a clicked rune.
	URpgSkillProgressionViewModel* Progression = NewObject<URpgSkillProgressionViewModel>(GetTransientPackage(), NAME_None, RF_Transient);
	Progression->BindPlayerState(PlayerState);
	URpgSkillTreeViewModel* BarTree = nullptr;
	for (URpgSkillTreeViewModel* Candidate : Progression->GetSkillTrees())
	{
		BarTree = Candidate && Candidate->GetTreeTag() == TAG_RuneSocketTest_Tree ? Candidate : BarTree;
	}
	auto Highlighted = [BarTree](const FGameplayTag NodeTag)
	{
		const URpgSkillTreeNodeViewModel* Node = BarTree ? BarTree->FindNode(NodeTag) : nullptr;
		return Node && Node->IsRuneHighlighted();
	};
	TestTrue(TEXT("The rune bar shows the tool in hand"), Progression->HasRuneTool());
	TestEqual(TEXT("The rune bar shows its three sockets"), Progression->GetRuneSlots().Num(), 3);
	TestTrue(TEXT("The first socket holds rune A"), Progression->IsRuneSlotFilled(0) && !Progression->IsRuneSlotFilled(1));
	TestTrue(TEXT("The bar lists the tree"), BarTree != nullptr);
	Progression->ToggleRuneHighlight(0);
	TestTrue(TEXT("Clicking rune A highlights it"), Progression->IsRuneSlotHighlighted(0) && Progression->GetHighlightedRuneTag() == RuneTagA);
	TestTrue(TEXT("Rune A's talent is highlighted"), Highlighted(TAG_RuneSocketTest_RuneBoundNode));
	TestFalse(TEXT("A talent without runes is not highlighted"), Highlighted(TAG_RuneSocketTest_PlainNode));
	Progression->ToggleRuneHighlight(0);
	TestFalse(TEXT("Clicking rune A again ends the highlight"), Highlighted(TAG_RuneSocketTest_RuneBoundNode));
	Progression->ToggleRuneHighlight(1);
	TestFalse(TEXT("An empty socket highlights nothing"), Progression->GetHighlightedRuneTag().IsValid());
	Progression->ToggleRuneHighlight(0);

	TestEqual(TEXT("Rune A leaves the tool"),
		Move(*Inventory, Rune, Storage(2, 0)), ERpgInventoryMutationResultCode::Success);
	Progression->Refresh();
	TestFalse(TEXT("The highlight ends when its rune leaves the tool"), Highlighted(TAG_RuneSocketTest_RuneBoundNode));
	TestFalse(TEXT("The bar shows the emptied socket"), Progression->IsRuneSlotFilled(0));
	Progression->Unbind();
	TestEqual(TEXT("The talent sleeps again"), Tuned(), 10.0f);
	TestEqual(TEXT("Its grant leaves with the rune"), AbilitySystem->GetTagCount(TAG_RuneSocketTest_RuneBoundGranted), 0);
	TestTrue(TEXT("It stays learned without repurchase"), SkillTrees->IsNodeUnlocked(TAG_RuneSocketTest_Tree, TAG_RuneSocketTest_RuneBoundNode));
	TestTrue(TEXT("The skill screen shows it sleeping again"), Dormant());

	TestEqual(TEXT("Rune A goes back into the tool"),
		Move(*Inventory, Rune, Socket(*Inventory, Tool, 2)), ERpgInventoryMutationResultCode::Success);
	TestEqual(TEXT("The talent wakes without repurchase"), AbilitySystem->GetTagCount(TAG_RuneSocketTest_RuneBoundGranted), 1);
	Equipment->UnequipItem(MainHand);
	TestEqual(TEXT("Unequipping the tool removes the talent's grant"), AbilitySystem->GetTagCount(TAG_RuneSocketTest_RuneBoundGranted), 0);
	TestTrue(TEXT("With no tool of the tree in hand the talent shows as sleeping"), Dormant());
	TreeView->UnbindSkillTree();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
