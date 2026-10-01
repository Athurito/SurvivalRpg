#include "RpgGameModeBase.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Templates/UnrealTemplate.h"
#include "TimerManager.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgStorageAccessRules.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalWorldRestoreTest,
	"SurvivalRpg.Save.WorldSave.PhysicalActorReconstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalWorldRestoreTest::RunTest(const FString& Parameters)
{
	// Exercise the real GameMode reconstruction path without initialization, experience loading or disk writes.
	struct FScopedWorld
	{
		UGameInstance* Instance = nullptr;
		UWorld* World = nullptr;
		FScopedWorld()
		{
			Instance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
		}
		~FScopedWorld()
		{
			Instance->Shutdown();
			if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
			Instance->RemoveFromRoot();
		}
	} Scope;
	if (!TestNotNull(TEXT("Standalone world"), Scope.World)) return false;
	ARpgGameModeBase* Mode = Scope.World->SpawnActor<ARpgGameModeBase>();
	if (!TestNotNull(TEXT("Persistence owner"), Mode)) return false;
	Mode->bEnableDiskPersistence = false;
	ARpgInventoryContainerActor* Original = Scope.World->SpawnActor<ARpgInventoryContainerActor>();
	if (!TestNotNull(TEXT("Original runtime chest"), Original)) return false;
	Original->GetContainerComponent()->SetRuntimeBuilt(true);
	FRpgInventoryGridSize ExpandedGrid;
	ExpandedGrid.Width = 6;
	ExpandedGrid.Height = 8;
	Original->GetInventoryManager()->SetDefaultGridSize(ExpandedGrid);
	Original->GetContainerComponent()->SetUpgradeTier(2);
	const FTransform SavedTransform(FRotator(0, 90, 0), FVector(320, 470, 25));
	Original->SetActorTransform(SavedTransform);
	FRpgStorageAssignment Rule;
	Rule.ItemDefinition = URpgInventoryAutomationTestStatefulMaterialDefinition::StaticClass();
	if (!TestTrue(TEXT("Authored material assignment"), Original->GetContainerComponent()->SetAssignments({Rule}))) return false;
	URpgInventoryItemInstance* Item = Original->GetInventoryManager()->GrantItemDefinition(Rule.ItemDefinition, 4);
	if (!TestNotNull(TEXT("Concrete stored item"), Item)) return false;
	const auto* Fragment = Item->FindFragmentByClass<URpgInventoryAutomationTestStatefulFragment>();
	if (!TestNotNull(TEXT("Mutable item fragment"), Fragment)) return false;
	Fragment->SetTestValue(Item, 91);
	const FRpgInventoryItemId SavedItemId = Item->GetItemId();
	const FName SavedContainerId = Original->GetContainerComponent()->GetPersistentContainerId();
	const int64 SavedOrder = Original->GetContainerComponent()->GetAssignments()[0].AssignmentOrder;
	Mode->MarkWorldContainerSaveDirty(SavedContainerId, Original->GetInventoryManager());
	if (!TestEqual(TEXT("One captured world container"), Mode->WorldContainerSaveDataMap.Num(), 1)) return false;
	URpgWorldSaveGame* Snapshot = NewObject<URpgWorldSaveGame>();
	Snapshot->WorldContainers = Mode->WorldContainerSaveDataMap;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Snapshot serializes"), UGameplayStatics::SaveGameToMemory(Snapshot, Bytes))) return false;
	URpgWorldSaveGame* Loaded = Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Snapshot deserializes"), Loaded)) return false;
	FString Error;
	if (!TestTrue(TEXT("Serialized physical state validates"), Loaded->ValidateForLoad(Error))) { AddInfo(Error); return false; }
	Mode->UnregisterPersistentWorldContainer(Original->GetContainerComponent());
	Original->Destroy();
	Mode->CaptureWorldContainers();
	TestEqual(TEXT("Absent runtime chest remains saved for reconstruction"), Mode->WorldContainerSaveDataMap.Num(), 1);
	Mode->WorldContainerSaveDataMap = Loaded->WorldContainers;
	if (!TestTrue(TEXT("Missing runtime actor is reconstructed"), Mode->RestorePlacedWorldContainers())) return false;
	ARpgInventoryContainerActor* Restored = nullptr;
	int32 Count = 0;
	for (TActorIterator<ARpgInventoryContainerActor> It(Scope.World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && It->GetContainerComponent()->GetPersistentContainerId() == SavedContainerId)
		{ Restored = *It; ++Count; }
	}
	if (!TestEqual(TEXT("Exactly one chest retains the saved identity"), Count, 1) || !Restored) return false;
	TestTrue(TEXT("Saved transform is restored"), Restored->GetActorTransform().Equals(SavedTransform));
	TestTrue(TEXT("Saved expanded grid is restored"), Restored->GetInventoryManager()->GetDefaultGridSize() == ExpandedGrid);
	TestEqual(TEXT("Saved upgrade tier is restored"), Restored->GetContainerComponent()->ExportPhysicalStorageMetadata().UpgradeTier, 2);
	if (TestEqual(TEXT("Assignment survives actor reconstruction"), Restored->GetContainerComponent()->GetAssignments().Num(), 1))
	{ TestEqual(TEXT("Original tie-break order survives"), Restored->GetContainerComponent()->GetAssignments()[0].AssignmentOrder, SavedOrder); }
	URpgInventoryItemInstance* RestoredItem = Restored->GetInventoryManager()->FindItemById(SavedItemId);
	if (!TestNotNull(TEXT("Concrete item identity survives actor reconstruction"), RestoredItem)) return false;
	TestEqual(TEXT("Saved stack count is restored"), Restored->GetInventoryManager()->GetItemStackCount(RestoredItem), 4);
	TestEqual(TEXT("Opaque item state is restored"), static_cast<int32>(Fragment->GetTestValue(RestoredItem)), 91);
	TestTrue(TEXT("A second restore reuses the existing actor"), Mode->RestorePlacedWorldContainers());
	TestEqual(TEXT("Reapplying the snapshot cannot duplicate material"), Restored->GetInventoryManager()->GetTotalItemCountByDefinition(Rule.ItemDefinition), 4);
	TestEqual(TEXT("Only one actor was reconstructed"), Mode->RestoreSpawnedContainers.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgCraftingStationRetentionTest,
	"SurvivalRpg.Save.WorldSave.AbsentCraftingStationRetainsClaims",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingStationRetentionTest::RunTest(const FString& Parameters)
{
	struct FScopedWorld
	{
		UGameInstance* Instance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
		UWorld* World = nullptr;
		FScopedWorld() { Instance->AddToRoot(); Instance->InitializeStandalone(); World = Instance->GetWorld(); }
		~FScopedWorld()
		{
			Instance->Shutdown();
			if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
			Instance->RemoveFromRoot();
		}
	} Scope;
	if (!TestNotNull(TEXT("Retention world"), Scope.World)) return false;
	ARpgGameModeBase* Mode = Scope.World->SpawnActor<ARpgGameModeBase>();
	if (!TestNotNull(TEXT("Retention persistence owner"), Mode)) return false;
	Mode->bEnableDiskPersistence = false;
	ARpgBaseCampActor* Base = Scope.World->SpawnActor<ARpgBaseCampActor>();
	if (!TestNotNull(TEXT("Retention base"), Base) || !TestTrue(TEXT("Shared storage domain"), Base->SetBaseArea(FVector::ZeroVector, 2000))) return false;
	ARpgInventoryContainerActor* Source = Scope.World->SpawnActor<ARpgInventoryContainerActor>();
	if (!TestNotNull(TEXT("Retention source chest"), Source)) return false;
	Source->GetContainerComponent()->EnsurePersistentContainerId();
	const auto Material = URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	if (!TestNotNull(TEXT("Six reservable resources"), Source->GetInventoryManager()->GrantItemDefinition(Material, 6))) return false;
	ARpgInventoryAutomationTestPlayerController* Requester = Scope.World->SpawnActor<ARpgInventoryAutomationTestPlayerController>();
	if (!TestNotNull(TEXT("Retention requester"), Requester)) return false;
	FNameProperty* StationIdProperty = FindFProperty<FNameProperty>(URpgCraftingStationComponent::StaticClass(), TEXT("PersistentStationId"));
	FObjectPropertyBase* RecipesProperty = FindFProperty<FObjectPropertyBase>(URpgCraftingStationComponent::StaticClass(), TEXT("AvailableRecipeSet"));
	if (!TestNotNull(TEXT("Station identity property"), StationIdProperty) || !TestNotNull(TEXT("Recipe catalog property"), RecipesProperty)) return false;
	const FName StationId(TEXT("RetentionStation"));
	auto SpawnStation = [&]()
	{
		ARpgCraftingStationActor* Actor = Scope.World->SpawnActor<ARpgCraftingStationActor>();
		StationIdProperty->SetPropertyValue_InContainer(Actor->GetCraftingStationComponent(), StationId);
		return Actor;
	};
	ARpgCraftingStationActor* Original = SpawnStation();
	URpgCraftingStationComponent* Station = Original->GetCraftingStationComponent();
	Mode->RegisterPersistentCraftingStation(Station);
	TestFalse(TEXT("Fresh startup without a selected snapshot remains usable"), Station->IsPersistenceRestorePending());
	URpgCraftingRecipeDefinition* Recipe = NewObject<URpgCraftingRecipeDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
	Recipe->bUnlockedByDefault = true;
	Recipe->CraftTime = 30.0f;
	FRpgCraftingResourceCost& Cost = Recipe->RequiredResources.AddDefaulted_GetRef();
	Cost.ItemDefinition = Material;
	Cost.Count = 3;
	FRpgCraftingOutputItem& Product = Recipe->OutputItems.AddDefaulted_GetRef();
	Product.ItemDefinition = URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	Product.Count = 1;
	URpgCraftingRecipeSet* Recipes = NewObject<URpgCraftingRecipeSet>();
	Recipes->Recipes.Add(Recipe);
	RecipesProperty->SetObjectPropertyValue_InContainer(Station, Recipes);
	URpgInventoryItemInstance* TrayItem = Station->GetOutputInventory()->GrantItemDefinition(
		URpgInventoryAutomationTestStatefulMaterialDefinition::StaticClass(), 3);
	if (!TestNotNull(TEXT("Stateful tray item"), TrayItem)) return false;
	const auto* Fragment = TrayItem->FindFragmentByClass<URpgInventoryAutomationTestStatefulFragment>();
	if (!TestNotNull(TEXT("Tray fragment"), Fragment)) return false;
	Fragment->SetTestValue(TrayItem, 91);
	const FRpgInventoryItemId TrayItemId = TrayItem->GetItemId();
	if (!TestTrue(TEXT("Two units reserve six actual resources"), Station->QueueCraftRecipe(Requester, Recipe, 2)) ||
		!TestTrue(TEXT("Explicit pause freezes remaining time"), Station->PauseCraftingStation(Requester))) return false;
	TestEqual(TEXT("Paid input is no longer in its source"), Source->GetInventoryManager()->GetTotalItemCountByDefinition(Material), 0);
	const FRpgCraftingStationSaveData Before = Station->ExportCraftingState();
	// Exercise the GameMode seam called by EndPlay without starting the fixture's Experience or disk persistence.
	Mode->UnregisterPersistentCraftingStation(Station);
	Original->Destroy();
	Mode->CaptureCraftingStations();
	if (!TestEqual(TEXT("Missing station remains represented"), Mode->CraftingStationSaveDataMap.Num(), 1)) return false;
	URpgWorldSaveGame* Snapshot = NewObject<URpgWorldSaveGame>();
	Snapshot->CraftingStations = Mode->CraftingStationSaveDataMap;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Unresolved claims serialize"), UGameplayStatics::SaveGameToMemory(Snapshot, Bytes))) return false;
	URpgWorldSaveGame* Loaded = Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	FString Error;
	if (!TestNotNull(TEXT("Unresolved snapshot loads"), Loaded) || !TestTrue(TEXT("Unresolved snapshot remains valid"), Loaded->ValidateForLoad(Error))) return false;
	Mode->CraftingStationSaveDataMap = Loaded->CraftingStations;
	TestTrue(TEXT("Candidate restore permits an unloaded placed station"), Mode->RestoreCraftingStations());
	Mode->bWorldSaveCandidateSelectionComplete = true;
	TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
	auto CompleteRegistration = [&]()
	{
		++GFrameCounter; Scope.World->GetTimerManager().Tick(0.0f);
		++GFrameCounter; Scope.World->GetTimerManager().Tick(0.01f);
	};
	ARpgCraftingStationActor* Returned = SpawnStation();
	Station = Returned->GetCraftingStationComponent();
	Mode->RegisterPersistentCraftingStation(Station);
	TestTrue(TEXT("Returning station waits for authored BeginPlay"), Station->IsPersistenceRestorePending());
	Station->SetOutputInventoryManager(Station->GetOutputInventory());
	Station->GetOutputInventory()->GrantItemDefinition(Product.ItemDefinition, 2);
	Mode->MarkCraftingSaveDirty(Station);
	Mode->CaptureCraftingStations();
	TestEqual(TEXT("Pending notifications retain paid claims"), Mode->CraftingStationSaveDataMap.FindChecked(StationId).Jobs.Num(), 1);
	TestNull(TEXT("Pending tray cannot receive refunds"), RpgStorageAccessRules::FindPersistentInventory(Scope.World,
		RpgStorageAccessRules::GetPersistentInventoryId(Station->GetOutputInventory())));
	TArray<URpgInventoryManagerComponent*> PendingSources;
	RpgStorageAccessRules::ResolveStorageSources(Scope.World, Returned->GetActorLocation(), 1000.0f, PendingSources);
	TestFalse(TEXT("Pending tray cannot supply another station"), PendingSources.Contains(Station->GetOutputInventory()));
	TestFalse(TEXT("Pending station denies direct access"), Station->CanActorAccess(Requester));
	Mode->UnregisterPersistentCraftingStation(Station);
	Returned->Destroy();
	Mode->CaptureCraftingStations();
	CompleteRegistration();
	TestEqual(TEXT("Interrupted pending reentry retains paid claims"), Mode->CraftingStationSaveDataMap.FindChecked(StationId).Jobs.Num(), 1);
	Returned = SpawnStation();
	Station = Returned->GetCraftingStationComponent();
	Mode->RegisterPersistentCraftingStation(Station);
	Station->GetOutputInventory()->GrantItemDefinition(Product.ItemDefinition, 2);
	CompleteRegistration();
	TestFalse(TEXT("Restored station becomes ready"), Station->IsPersistenceRestorePending());
	TestEqual(TEXT("Saved tray replaces later BeginPlay seeds"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(Product.ItemDefinition), 0);
	const FRpgCraftingStationSaveData After = Station->ExportCraftingState();
	if (!TestEqual(TEXT("Returning station has exactly one paid job"), After.Jobs.Num(), 1)) return false;
	TestTrue(TEXT("Explicit pause survives absence"), After.bPaused);
	TestEqual(TEXT("Absence applies no offline progress"), After.Jobs[0].RemainingTime, Before.Jobs[0].RemainingTime);
	if (!TestEqual(TEXT("One durable source credit survives"), After.Jobs[0].Refunds.Num(), 1)) return false;
	TestEqual(TEXT("All six refund credits survive"), After.Jobs[0].Refunds[0].Count, 6);
	TrayItem = Station->GetOutputInventory()->FindItemById(TrayItemId);
	if (!TestNotNull(TEXT("Tray item identity survives absence"), TrayItem)) return false;
	TestEqual(TEXT("Tray stack count survives"), Station->GetOutputInventory()->GetItemStackCount(TrayItem), 3);
	TestEqual(TEXT("Tray opaque state survives"), static_cast<int32>(Fragment->GetTestValue(TrayItem)), 91);
	Station->GetOutputInventory()->GrantItemDefinition(Product.ItemDefinition, 1);
	Mode->RegisterPersistentCraftingStation(Station);
	TestEqual(TEXT("Repeated registration does not replace newer live contents"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(Product.ItemDefinition), 1);
	if (!TestTrue(TEXT("Restored claim refunds once"), Station->CancelCraftJob(Requester, After.Jobs[0].JobId))) return false;
	TestEqual(TEXT("Original source receives exactly its six paid resources"), Source->GetInventoryManager()->GetTotalItemCountByDefinition(Material), 6);
	Mode->CaptureCraftingStations();
	TestEqual(TEXT("Live empty queue replaces retained paid queue"), Mode->CraftingStationSaveDataMap.FindChecked(StationId).Jobs.Num(), 0);
	Mode->UnregisterPersistentCraftingStation(Station);
	Returned->Destroy();
	Mode->CaptureCraftingStations();
	Station = SpawnStation()->GetCraftingStationComponent();
	Mode->RegisterPersistentCraftingStation(Station);
	Station->GetOutputInventory()->GrantItemDefinition(Product.ItemDefinition, 5);
	Mode->MarkCraftingSaveDirty(Station);
	CompleteRegistration();
	TestEqual(TEXT("Later reentry cannot resurrect a canceled claim"), Station->GetCraftingJobs().Num(), 0);
	TestEqual(TEXT("Repeated absence cannot mint another refund"), Source->GetInventoryManager()->GetTotalItemCountByDefinition(Material), 6);
	TestEqual(TEXT("Latest tray survives a second reentry"), Station->GetOutputInventory()->GetTotalItemCountByDefinition(Product.ItemDefinition), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalChestRetentionTest,
	"SurvivalRpg.Save.WorldSave.AbsentChestRetainsItemsAndEmptyState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalChestRetentionTest::RunTest(const FString& Parameters)
{
	struct FScopedWorld
	{
		UGameInstance* Instance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
		UWorld* World = nullptr;
		FScopedWorld() { Instance->AddToRoot(); Instance->InitializeStandalone(); World = Instance->GetWorld(); }
		~FScopedWorld()
		{
			Instance->Shutdown();
			if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
			Instance->RemoveFromRoot();
		}
	} Scope;
	if (!TestNotNull(TEXT("Chest retention world"), Scope.World)) return false;
	ARpgGameModeBase* Mode = Scope.World->SpawnActor<ARpgGameModeBase>();
	if (!TestNotNull(TEXT("Chest persistence owner"), Mode)) return false;
	Mode->bEnableDiskPersistence = false;
	Mode->bWorldSaveCandidateSelectionComplete = true;
	ARpgInventoryContainerActor* Original = Scope.World->SpawnActor<ARpgInventoryContainerActor>();
	if (!TestNotNull(TEXT("Placed chest fixture"), Original)) return false;
	Mode->RegisterPersistentWorldContainer(Original->GetContainerComponent());
	const FName SavedId = Original->GetContainerComponent()->GetPersistentContainerId();
	const auto Material = URpgInventoryAutomationTestStatefulMaterialDefinition::StaticClass();
	URpgInventoryItemInstance* Item = Original->GetInventoryManager()->GrantItemDefinition(Material, 4);
	if (!TestNotNull(TEXT("Stored stateful material"), Item)) return false;
	const auto* Fragment = Item->FindFragmentByClass<URpgInventoryAutomationTestStatefulFragment>();
	if (!TestNotNull(TEXT("Stored state fragment"), Fragment)) return false;
	Fragment->SetTestValue(Item, 73);
	const FRpgInventoryItemId SavedItemId = Item->GetItemId();
	Original->SetActorLocation(FVector(400, 200, 50));
	const FTransform SavedTransform = Original->GetActorTransform();
	Original->GetContainerComponent()->SetUpgradeTier(1);
	FRpgInventoryGridSize SavedGrid;
	SavedGrid.Width = 3; SavedGrid.Height = 2;
	if (!TestTrue(TEXT("Placed chest capacity fixture"), Original->GetInventoryManager()->SetDefaultGridSize(SavedGrid))) return false;
	Mode->UnregisterPersistentWorldContainer(Original->GetContainerComponent());
	Original->Destroy();
	Mode->CaptureWorldContainers();
	if (!TestEqual(TEXT("Missing authored chest remains saved"), Mode->WorldContainerSaveDataMap.Num(), 1)) return false;
	URpgWorldSaveGame* Snapshot = NewObject<URpgWorldSaveGame>();
	Snapshot->WorldContainers = Mode->WorldContainerSaveDataMap;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Absent chest serializes"), UGameplayStatics::SaveGameToMemory(Snapshot, Bytes))) return false;
	URpgWorldSaveGame* Loaded = Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	FString Error;
	if (!TestNotNull(TEXT("Absent chest snapshot loads"), Loaded) || !TestTrue(TEXT("Absent chest snapshot validates"), Loaded->ValidateForLoad(Error))) return false;
	Mode->WorldContainerSaveDataMap = Loaded->WorldContainers;
	TestTrue(TEXT("Missing authored chest does not reject the entire candidate"), Mode->RestorePlacedWorldContainers());
	TestEqual(TEXT("Authored chest waits for its owning level instead of spawning another actor"), Mode->RestoreSpawnedContainers.Num(), 0);
	FNameProperty* IdProperty = FindFProperty<FNameProperty>(URpgInventoryContainerComponent::StaticClass(), TEXT("PersistentContainerId"));
	if (!TestNotNull(TEXT("Authored chest identity property"), IdProperty)) return false;
	auto SpawnReturning = [&]()
	{
		ARpgInventoryContainerActor* Actor = Scope.World->SpawnActor<ARpgInventoryContainerActor>();
		IdProperty->SetPropertyValue_InContainer(Actor->GetContainerComponent(), SavedId);
		Mode->RegisterPersistentWorldContainer(Actor->GetContainerComponent());
		// Mirrors synchronous actor Blueprint BeginPlay grants after native component BeginPlay.
		Actor->GetInventoryManager()->GrantItemDefinition(URpgInventoryAutomationTestMaterialDefinition::StaticClass(), 8);
		Mode->MarkWorldContainerSaveDirty(SavedId, Actor->GetInventoryManager());
		return Actor;
	};
	TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter);
	auto CompleteRegistration = [&]()
	{
		++GFrameCounter; Scope.World->GetTimerManager().Tick(0.0f);
		++GFrameCounter; Scope.World->GetTimerManager().Tick(0.01f);
	};
	ARpgInventoryContainerActor* Interrupted = SpawnReturning();
	TestTrue(TEXT("Returning chest is inaccessible during seed/restore staging"), Interrupted->GetContainerComponent()->IsConstructionPending());
	Mode->UnregisterPersistentWorldContainer(Interrupted->GetContainerComponent());
	Interrupted->Destroy();
	Mode->CaptureWorldContainers();
	CompleteRegistration();
	TestEqual(TEXT("Interrupted reentry cannot overwrite retained stack with seeds"), Mode->WorldContainerSaveDataMap.FindChecked(SavedId).InventoryGraph.Items[0].StackCount, 4);
	ARpgInventoryContainerActor* Returned = SpawnReturning();
	Mode->CaptureWorldContainers();
	CompleteRegistration();
	TestFalse(TEXT("Successful restore publishes the chest"), Returned->GetContainerComponent()->IsConstructionPending());
	TestTrue(TEXT("Saved chest transform returns"), Returned->GetActorTransform().Equals(SavedTransform));
	TestTrue(TEXT("Saved grid returns"), Returned->GetInventoryManager()->GetDefaultGridSize() == SavedGrid);
	TestEqual(TEXT("Saved tier returns"), Returned->GetContainerComponent()->ExportPhysicalStorageMetadata().UpgradeTier, 1);
	TestEqual(TEXT("Blueprint startup seeds are replaced"), Returned->GetInventoryManager()->GetTotalItemCountByDefinition(URpgInventoryAutomationTestMaterialDefinition::StaticClass()), 0);
	Item = Returned->GetInventoryManager()->FindItemById(SavedItemId);
	if (!TestNotNull(TEXT("Retained item identity returns"), Item)) return false;
	TestEqual(TEXT("Retained opaque state returns"), static_cast<int32>(Fragment->GetTestValue(Item)), 73);
	ARpgInventoryContainerActor* Target = Scope.World->SpawnActor<ARpgInventoryContainerActor>();
	Target->GetContainerComponent()->EnsurePersistentContainerId();
	FRpgInventoryBatchOperation Transfer;
	Transfer.SourceInventory = Returned->GetInventoryManager();
	Transfer.TargetInventory = Target->GetInventoryManager();
	Transfer.ItemId = SavedItemId; Transfer.Quantity = 4;
	if (!TestTrue(TEXT("Authorized transfer drains returning chest"), Transfer.SourceInventory->ApplyInventoryBatch({ Transfer }, FGuid::NewGuid()).IsSuccess())) return false;
	Mode->RegisterPersistentWorldContainer(Returned->GetContainerComponent());
	TestEqual(TEXT("Repeated registration cannot replay a previous nonempty snapshot"), Returned->GetInventoryManager()->GetAllEntries().Num(), 0);
	Mode->CaptureWorldContainers();
	TestEqual(TEXT("A live empty graph replaces its old saved contents"), Mode->WorldContainerSaveDataMap.FindChecked(SavedId).InventoryGraph.Items.Num(), 0);
	Mode->UnregisterPersistentWorldContainer(Returned->GetContainerComponent());
	Returned->Destroy();
	Mode->CaptureWorldContainers();
	Returned = SpawnReturning();
	CompleteRegistration();
	TestEqual(TEXT("Empty saved chest cannot resurrect initial loot"), Returned->GetInventoryManager()->GetAllEntries().Num(), 0);
	TestEqual(TEXT("Transferred material exists exactly once"), Target->GetInventoryManager()->GetTotalItemCountByDefinition(Material), 4);
	TestNotNull(TEXT("Transferred identity remains only at its destination"), Target->GetInventoryManager()->FindItemById(SavedItemId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalChestSplitMergeSaveTest,
	"SurvivalRpg.Save.WorldSave.ChestSplitMergeCommittedSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalChestSplitMergeSaveTest::RunTest(const FString& Parameters)
{
	struct FScopedWorld
	{
		UGameInstance* Instance = nullptr;
		UWorld* World = nullptr;
		FScopedWorld()
		{
			Instance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
		}
		~FScopedWorld()
		{
			Instance->Shutdown();
			if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
			Instance->RemoveFromRoot();
		}
	} Scope;
	if (!TestNotNull(TEXT("Standalone save-listener world"), Scope.World)) return false;
	ARpgGameModeBase* Mode = Scope.World->SpawnActor<ARpgGameModeBase>();
	ARpgInventoryContainerActor* Chest = Scope.World->SpawnActor<ARpgInventoryContainerActor>();
	if (!TestNotNull(TEXT("Persistence owner"), Mode) || !TestNotNull(TEXT("Physical chest"), Chest)) return false;
	Mode->bEnableDiskPersistence = false;
	Mode->bWorldSaveCandidateSelectionComplete = true;
	Chest->GetContainerComponent()->SetRuntimeBuilt(true);
	Chest->GetContainerComponent()->EnsurePersistentContainerId();
	URpgInventoryManagerComponent* Inventory = Chest->GetInventoryManager();
	const FName ContainerId = Chest->GetContainerComponent()->GetPersistentContainerId();
	const TSubclassOf<URpgInventoryItemDefinition> Material = URpgInventoryAutomationTestStatefulMaterialDefinition::StaticClass();
	URpgInventoryItemInstance* Original = Inventory->GrantItemDefinition(Material, 9);
	if (!TestNotNull(TEXT("Nine-unit material stack"), Original)) return false;
	const auto* Fragment = Original->FindFragmentByClass<URpgInventoryAutomationTestStatefulFragment>();
	if (!TestNotNull(TEXT("Stateful stack fragment"), Fragment)) return false;
	Fragment->SetTestValue(Original, 47);
	const FRpgInventoryItemId OriginalId = Original->GetItemId();
	const FRpgInventoryEntryView OriginalEntry = Inventory->GetAllEntries()[0];
	Mode->MarkWorldContainerSaveDirty(ContainerId, Inventory);
	Mode->RegisterSaveStateListeners();

	// Save listeners run during real list notifications. The durable map may contain the previous
	// committed graph or the new committed graph, but never a split debit or zero-count merge source.
	int32 MessageCount = 0;
	bool bEveryObservedSnapshotConserved = true;
	UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(Scope.World);
	const FGameplayMessageListenerHandle Observer = Messages.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		[&](FGameplayTag, const FRpgInventoryChangeMessage& Message)
		{
			if (Message.InventoryOwner != Inventory) return;
			++MessageCount;
			const FRpgWorldContainerSaveData* Saved = Mode->WorldContainerSaveDataMap.Find(ContainerId);
			int32 Total = 0;
			TSet<FRpgInventoryItemId> Ids;
			if (!Saved) { bEveryObservedSnapshotConserved = false; return; }
			for (const FRpgInventorySavedItem& Item : Saved->InventoryGraph.Items)
			{
				bEveryObservedSnapshotConserved &= Item.ItemId.IsValid() && Item.StackCount > 0 && !Ids.Contains(Item.ItemId);
				Ids.Add(Item.ItemId);
				Total += Item.StackCount;
			}
			bEveryObservedSnapshotConserved &= Total == 9 && !Mode->bDiskWritesBlockedByRestoreFailure;
		});
	ON_SCOPE_EXIT
	{
		Messages.UnregisterListener(Observer);
		Mode->UnregisterSaveStateListeners();
	};

	FRpgInventoryMutationRequest Split;
	Split.EnsureRequestId();
	Split.Operation = ERpgInventoryMutationOperation::Split;
	Split.ItemId = OriginalId;
	Split.ExpectedEntryId = OriginalEntry.EntryId;
	Split.Source = OriginalEntry.Placement.GetContainerHandle();
	Split.ExpectedSourcePlacement = OriginalEntry.Placement;
	Split.ExpectedSourceQuantity = OriginalEntry.StackCount;
	Split.Target = Split.Source;
	Split.TargetPlacement = OriginalEntry.Placement;
	Split.TargetPlacement.X += OriginalEntry.Placement.Width;
	Split.Quantity = 4;
	if (!TestTrue(TEXT("The same split kernel used by UI succeeds"), Inventory->ExecuteInventoryMutation(Split).IsSuccess())) return false;
	TestTrue(TEXT("Split emits actual inventory notifications"), MessageCount > 0);
	TestFalse(TEXT("Split leaves disk writes enabled"), Mode->bDiskWritesBlockedByRestoreFailure);
	TestTrue(TEXT("Every split notification retains a complete conserved save"), bEveryObservedSnapshotConserved);
	FRpgInventoryEntryView SplitEntry;
	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		if (Entry.ItemId != OriginalId) SplitEntry = Entry;
	}
	if (!TestTrue(TEXT("Split creates a distinct persistent identity"), SplitEntry.ItemId.IsValid() && SplitEntry.ItemId != OriginalId)) return false;
	const FRpgWorldContainerSaveData& SplitSaved = Mode->WorldContainerSaveDataMap.FindChecked(ContainerId);
	TestEqual(TEXT("Committed split is saved without a manual capture"), SplitSaved.InventoryGraph.Items.Num(), 2);
	int32 OriginalSavedCount = 0, SplitSavedCount = 0;
	for (const FRpgInventorySavedItem& Item : SplitSaved.InventoryGraph.Items)
	{
		if (Item.ItemId == OriginalId) OriginalSavedCount = Item.StackCount;
		if (Item.ItemId == SplitEntry.ItemId) SplitSavedCount = Item.StackCount;
	}
	TestEqual(TEXT("Saved original retains five units"), OriginalSavedCount, 5);
	TestEqual(TEXT("Saved new identity contains four units"), SplitSavedCount, 4);

	const int32 MessagesBeforeMerge = MessageCount;
	FRpgInventoryMoveIntent Merge;
	Merge.EnsureRequestId();
	Merge.ItemId = SplitEntry.ItemId;
	Merge.ExpectedEntryId = SplitEntry.EntryId;
	Merge.ExpectedSourcePlacement = SplitEntry.Placement;
	Merge.ExpectedQuantity = SplitEntry.StackCount;
	Merge.TargetPlacement = OriginalEntry.Placement;
	if (!TestTrue(TEXT("Dragging the split stack onto the original merges completely"), Inventory->MoveItem(Merge).IsSuccess())) return false;
	TestTrue(TEXT("Merge emits actual inventory notifications"), MessageCount > MessagesBeforeMerge);
	TestFalse(TEXT("Merge never blocks disk writes on its removed source entry"), Mode->bDiskWritesBlockedByRestoreFailure);
	TestTrue(TEXT("All synchronous save observations conserve the nine units"), bEveryObservedSnapshotConserved);
	const FRpgWorldContainerSaveData& MergedSaved = Mode->WorldContainerSaveDataMap.FindChecked(ContainerId);
	if (!TestEqual(TEXT("Committed merge saves exactly one entry"), MergedSaved.InventoryGraph.Items.Num(), 1)) return false;
	TestTrue(TEXT("Merge keeps the target persistent identity"), MergedSaved.InventoryGraph.Items[0].ItemId == OriginalId);
	TestEqual(TEXT("Merged snapshot contains all nine units"), MergedSaved.InventoryGraph.Items[0].StackCount, 9);
	TestNull(TEXT("Merged-away source identity is absent from live graph"), Inventory->FindItemById(SplitEntry.ItemId));

	URpgWorldSaveGame* Snapshot = NewObject<URpgWorldSaveGame>();
	Snapshot->WorldContainers = Mode->WorldContainerSaveDataMap;
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Committed listener snapshot serializes"), UGameplayStatics::SaveGameToMemory(Snapshot, Bytes))) return false;
	URpgWorldSaveGame* Loaded = Cast<URpgWorldSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	FString Error;
	if (!TestNotNull(TEXT("Merged snapshot deserializes"), Loaded) || !TestTrue(TEXT("Merged snapshot validates"), Loaded->ValidateForLoad(Error))) return false;
	Messages.UnregisterListener(Observer);
	Mode->UnregisterSaveStateListeners();
	Chest->Destroy();
	Mode->WorldContainerSaveDataMap = Loaded->WorldContainers;
	if (!TestTrue(TEXT("Merged snapshot reconstructs its missing runtime chest"), Mode->RestorePlacedWorldContainers())) return false;
	ARpgInventoryContainerActor* Restored = nullptr;
	for (TActorIterator<ARpgInventoryContainerActor> It(Scope.World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && It->GetContainerComponent()->GetPersistentContainerId() == ContainerId) Restored = *It;
	}
	if (!TestNotNull(TEXT("Restored physical chest"), Restored)) return false;
	URpgInventoryItemInstance* RestoredItem = Restored->GetInventoryManager()->FindItemById(OriginalId);
	if (!TestNotNull(TEXT("Merged target identity survives reconstruction"), RestoredItem)) return false;
	TestEqual(TEXT("Restored graph contains one stack"), Restored->GetInventoryManager()->GetAllEntries().Num(), 1);
	TestEqual(TEXT("Restored material total is exactly nine"), Restored->GetInventoryManager()->GetTotalItemCountByDefinition(Material), 9);
	TestEqual(TEXT("Merged runtime state survives the save"), static_cast<int32>(Fragment->GetTestValue(RestoredItem)), 47);
	TestNull(TEXT("Restore cannot resurrect the merged-away identity"), Restored->GetInventoryManager()->FindItemById(SplitEntry.ItemId));
	return true;
}

#endif
