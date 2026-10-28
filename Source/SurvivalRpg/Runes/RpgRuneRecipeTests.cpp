#include "RpgRuneAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "NativeGameplayTags.h"
#include "RpgInventoryFragment_Rune.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Core/Game/RpgWorldSaveGame.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgRecipeUnlockComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneRecipeTest_RecipeA, "Recipe.AutomationRune.A");

namespace RpgRuneRecipeTests
{
	const FName StorageId(TEXT("Storage"));
	const FName RecipeUnlockFeatureId(TEXT("Crafting.RecipeUnlocks"));

	class FScopedTestWorld
	{
	public:
		/** A standalone world; with bWithGameMode it also gets the RPG GameMode and its GameState. */
		explicit FScopedTestWorld(const bool bWithGameMode)
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			if (!World)
			{
				return;
			}

			if (bWithGameMode)
			{
				FURL GameUrl;
				GameUrl.AddOption(*FString::Printf(TEXT("game=%s"), *ARpgGameModeBase::StaticClass()->GetPathName()));
				if (World->SetGameMode(GameUrl))
				{
					World->GetAuthGameMode<ARpgGameModeBase>()->bEnableDiskPersistence = false;
					World->InitializeActorsForPlay(GameUrl);
				}
			}
			else if (ARpgGameStateBase* GameState = World->SpawnActor<ARpgGameStateBase>())
			{
				World->SetGameState(GameState);
			}
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

		URpgRecipeUnlockComponent* GetRecipeUnlocks() const
		{
			const ARpgGameStateBase* GameState = World ? World->GetGameState<ARpgGameStateBase>() : nullptr;
			return GameState ? GameState->GetRecipeUnlockComponent() : nullptr;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	/** Rune A's locked base recipe, which outputs rune A. */
	URpgCraftingRecipeDefinition* CreateRuneRecipe()
	{
		URpgCraftingRecipeDefinition* Recipe =
			NewObject<URpgCraftingRecipeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Recipe->RecipeUnlockTag = TAG_RuneRecipeTest_RecipeA;
		Recipe->bUnlockedByDefault = false;
		FRpgCraftingOutputItem& Output = Recipe->OutputItems.AddDefaulted_GetRef();
		Output.ItemDefinition = URpgRuneAutomationTestRuneDefinition::StaticClass();
		Output.Count = 1;
		return Recipe;
	}

	FRpgInventoryGridPlacement Storage(const int32 X, const int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(FRpgInventoryContainerHandle::MakeRoot(StorageId));
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}

	FRpgInventoryGridPlacement FirstSocket(const URpgInventoryManagerComponent& Inventory, URpgInventoryItemInstance* Tool)
	{
		FRpgInventoryGridPlacement ToolPlacement;
		Inventory.GetItemPlacement(Tool, ToolPlacement);
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(
			Tool->GetItemId(),
			RpgRuneAutomationTestSocketGridId,
			ToolPlacement.GetContainerHandle().GetDirectChildDepth()));
		Placement.X = 0;
		Placement.Y = 0;
		return Placement;
	}

	/** Moves Item's whole entry to Target through the production intent, as the inventory UI does. */
	ERpgInventoryMutationResultCode Move(
		URpgInventoryManagerComponent& Inventory,
		const URpgInventoryItemInstance* Item,
		const FRpgInventoryGridPlacement& Target)
	{
		for (const FRpgInventoryEntryView& Entry : Inventory.GetAllEntries())
		{
			if (Item && Entry.ItemId == Item->GetItemId())
			{
				FRpgInventoryMoveIntent Intent;
				Intent.EnsureRequestId();
				Intent.ItemId = Entry.ItemId;
				Intent.ExpectedEntryId = Entry.EntryId;
				Intent.ExpectedSourcePlacement = Entry.Placement;
				Intent.ExpectedQuantity = Entry.StackCount;
				Intent.TargetPlacement = Target;
				return Inventory.MoveItem(Intent).Code;
			}
		}
		return ERpgInventoryMutationResultCode::ItemNotFound;
	}

	void SetDefaultStorage(URpgInventoryManagerComponent& Inventory)
	{
		FRpgInventoryGridSize GridSize;
		GridSize.Width = 6;
		GridSize.Height = 3;
		Inventory.SetDefaultGridSize(GridSize);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneRecipeDiscoveryTest,
	"SurvivalRpg.Runes.Recipes.FindingARuneTeachesTheWorldItsRecipe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneRecipeDiscoveryTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneRecipeTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedTestWorld TestWorld(false);
	UWorld* World = TestWorld.GetWorld();
	URpgRecipeUnlockComponent* Unlocks = TestWorld.GetRecipeUnlocks();
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World ? World->SpawnActor<ARpgInventoryAutomationTestPlayerState>() : nullptr;
	if (!TestNotNull(TEXT("The world has a recipe store"), Unlocks) ||
		!TestNotNull(TEXT("The player state spawns"), PlayerState))
	{
		return false;
	}
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	if (!TestNotNull(TEXT("The player has an inventory"), Inventory))
	{
		return false;
	}
	SetDefaultStorage(*Inventory);
	PlayerState->DispatchBeginPlay();

	URpgCraftingRecipeDefinition* Recipe = CreateRuneRecipe();
	URpgRuneAutomationTestRuneDefinition::SetBaseRecipe(Recipe);
	ON_SCOPE_EXIT
	{
		URpgRuneAutomationTestRuneDefinition::SetBaseRecipe(nullptr);
	};
	TestFalse(TEXT("The rune's recipe starts locked"), Unlocks->IsRecipeUnlocked(Recipe));

	// A rune in someone else's chest is not found yet.
	AActor* ChestOwner = World->SpawnActor<AActor>();
	URpgInventoryManagerComponent* Chest = NewObject<URpgInventoryManagerComponent>(ChestOwner);
	ChestOwner->AddInstanceComponent(Chest);
	Chest->RegisterComponent();
	SetDefaultStorage(*Chest);
	TestNotNull(
		TEXT("A rune lies in a chest"),
		Chest->AddItemDefinitionToPlacement(URpgRuneAutomationTestRuneDefinition::StaticClass(), 1, Storage(0, 0)));
	TestNotNull(
		TEXT("The player picks up a plain item"),
		Inventory->AddItemDefinitionToPlacement(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1, Storage(0, 0)));
	TestFalse(TEXT("Neither teaches the recipe"), Unlocks->IsRecipeUnlocked(Recipe));

	URpgInventoryItemInstance* Rune = Inventory->AddItemDefinitionToPlacement(
		URpgRuneAutomationTestRuneDefinition::StaticClass(), 1, Storage(1, 0));
	if (!TestNotNull(TEXT("The player finds a rune"), Rune))
	{
		return false;
	}
	TestTrue(TEXT("Finding the rune teaches the world its recipe"), Unlocks->IsRecipeUnlocked(Recipe));
	TestTrue(TEXT("The rune is not consumed"), Inventory->ContainsItemInstance(Rune));
	TestEqual(TEXT("The rune keeps rank I"), Rune->GetRuneRank(), 1);
	TestEqual(
		TEXT("Carrying a second rune teaches nothing new"),
		URpgInventoryFragment_Rune::LearnBaseRecipesOfRunesIn(*Inventory, *Unlocks),
		0);

	// A rune in a tool's sockets counts as carried too.
	URpgInventoryItemInstance* Tool = Inventory->AddItemDefinitionToPlacement(
		URpgRuneAutomationTestToolDefinition::StaticClass(), 1, Storage(2, 0));
	if (!TestNotNull(TEXT("The player carries a socketed tool"), Tool) ||
		!TestEqual(
			TEXT("The rune moves into the tool's first socket"),
			Move(*Inventory, Rune, FirstSocket(*Inventory, Tool)),
			ERpgInventoryMutationResultCode::Success))
	{
		return false;
	}
	TestTrue(TEXT("The pristine world forgets the recipe"), Unlocks->RestoreWorldSaveData(nullptr));
	TestFalse(TEXT("The recipe is locked again"), Unlocks->IsRecipeUnlocked(Recipe));
	TestNotNull(
		TEXT("The player picks up another plain item"),
		Inventory->AddItemDefinitionToPlacement(URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1, Storage(3, 0)));
	TestTrue(TEXT("The socketed rune teaches its recipe on the next change"), Unlocks->IsRecipeUnlocked(Recipe));

	TestTrue(TEXT("The pristine world forgets the recipe again"), Unlocks->RestoreWorldSaveData(nullptr));
	ChestOwner->SetRole(ROLE_SimulatedProxy);
	TestEqual(
		TEXT("A client never teaches recipes"),
		URpgInventoryFragment_Rune::LearnBaseRecipesOfRunesIn(*Chest, *Unlocks),
		0);
	ChestOwner->SetRole(ROLE_Authority);
	TestFalse(TEXT("The recipe stays locked"), Unlocks->IsRecipeUnlocked(Recipe));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneRecipeWorldSaveTest,
	"SurvivalRpg.Runes.Recipes.RecipeKnowledgeSavesWithTheWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneRecipeWorldSaveTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneRecipeTests;
	ConfigureRuneAutomationTestDefinitions();
	FScopedTestWorld TestWorld(true);
	UWorld* World = TestWorld.GetWorld();
	ARpgGameModeBase* Mode = World ? World->GetAuthGameMode<ARpgGameModeBase>() : nullptr;
	ARpgGameStateBase* GameState = World ? World->GetGameState<ARpgGameStateBase>() : nullptr;
	URpgRecipeUnlockComponent* Unlocks = TestWorld.GetRecipeUnlocks();
	if (!TestNotNull(TEXT("The world has the RPG GameMode"), Mode) ||
		!TestNotNull(TEXT("The GameMode spawned the RPG GameState"), GameState) ||
		!TestNotNull(TEXT("The GameState owns the recipe store"), Unlocks))
	{
		return false;
	}
	GameState->DispatchBeginPlay();

	TestTrue(TEXT("The world learns a recipe"), Unlocks->UnlockRecipeTag(TAG_RuneRecipeTest_RecipeA));
	TestFalse(
		TEXT("A tag outside Recipe is no recipe"),
		Unlocks->UnlockRecipeTag(FGameplayTag::RequestGameplayTag(TEXT("Rune.AutomationTest.A"))));

	Mode->CaptureWorldSaveParticipants();
	const FRpgWorldFeatureSaveData* Entry = Mode->WorldFeatureSaveDataMap.Find(RecipeUnlockFeatureId);
	TestTrue(
		TEXT("Beginning play registered the store with the world save"),
		Entry && Entry->SchemaVersion == URpgRecipeUnlockComponent::CurrentSaveSchemaVersion);

	URpgWorldSaveGame* Snapshot = Mode->BuildWorldSaveSnapshot();
	TArray<uint8> Bytes;
	if (!TestNotNull(TEXT("The world snapshot is built"), Snapshot) ||
		!TestTrue(TEXT("The snapshot serializes"), UGameplayStatics::SaveGameToMemory(Snapshot, Bytes)))
	{
		return false;
	}
	const URpgWorldSaveGame* Loaded = Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	FString Error;
	if (!TestNotNull(TEXT("The snapshot deserializes"), Loaded) ||
		!TestTrue(TEXT("The loaded snapshot validates"), Loaded->ValidateForLoad(Error)))
	{
		AddInfo(Error);
		return false;
	}

	TestTrue(TEXT("Restoring the pristine world succeeds"), Unlocks->RestoreWorldSaveData(nullptr));
	TestFalse(TEXT("The pristine world knows no recipe"), Unlocks->IsRecipeTagUnlocked(TAG_RuneRecipeTest_RecipeA));
	Mode->WorldFeatureSaveDataMap = Loaded->WorldFeatures;
	TestTrue(TEXT("The loaded world restores its features"), Mode->RestoreWorldSaveParticipants());
	TestTrue(TEXT("The recipe is known again after loading"), Unlocks->IsRecipeTagUnlocked(TAG_RuneRecipeTest_RecipeA));

	// A payload with a tag outside Recipe or another schema rejects the snapshot and changes nothing.
	FRpgRecipeUnlockSaveData BadData;
	BadData.UnlockedRecipeTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Rune.AutomationTest.A")));
	FRpgWorldFeatureSaveData BadEntry;
	BadEntry.SchemaVersion = URpgRecipeUnlockComponent::CurrentSaveSchemaVersion;
	{
		FMemoryWriter Writer(BadEntry.Payload, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FRpgRecipeUnlockSaveData::StaticStruct()->SerializeItem(Archive, &BadData, nullptr);
	}
	AddExpectedError(TEXT("is not a Recipe tag"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A tag outside Recipe is rejected"), Unlocks->RestoreWorldSaveData(&BadEntry));
	BadEntry = *Loaded->WorldFeatures.Find(RecipeUnlockFeatureId);
	BadEntry.SchemaVersion = URpgRecipeUnlockComponent::CurrentSaveSchemaVersion + 1;
	AddExpectedError(TEXT("Saved recipe unlocks have schema"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Another schema is rejected"), Unlocks->RestoreWorldSaveData(&BadEntry));
	TestTrue(TEXT("Rejected saves keep the known recipe"), Unlocks->IsRecipeTagUnlocked(TAG_RuneRecipeTest_RecipeA));

	GameState->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("A client cannot restore recipe knowledge"), Unlocks->RestoreWorldSaveData(nullptr));
	GameState->SetRole(ROLE_Authority);
	TestTrue(TEXT("The client attempt changed nothing"), Unlocks->IsRecipeTagUnlocked(TAG_RuneRecipeTest_RecipeA));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
