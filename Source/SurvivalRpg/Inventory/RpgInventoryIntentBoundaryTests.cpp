#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"

#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/Player.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

namespace RpgInventoryIntentBoundaryTests
{
	class FScopedIntentBoundaryWorld
	{
	public:
		FScopedIntentBoundaryWorld()
		{
			GameInstance =
				NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedIntentBoundaryWorld()
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

		UGameInstance* GetGameInstance() const
		{
			return GameInstance;
		}

		URpgInventoryManagerComponent* CreateInventory(
			const TCHAR* DebugName)
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
			AActor* OwnerActor =
				World->SpawnActor<AActor>(SpawnParameters);
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

	FRpgInventoryGridPlacement MakeRootPlacement(
		FName ContainerId,
		int32 X,
		int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(
			FRpgInventoryContainerHandle::MakeRoot(ContainerId));
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}

	ARpgInventoryAutomationTestPlayerController* SpawnPlayerController(
		UWorld* World,
		const TCHAR* DebugName)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(
			World,
			ARpgInventoryAutomationTestPlayerController::StaticClass(),
			FName(DebugName));
		SpawnParameters.ObjectFlags = RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<
			ARpgInventoryAutomationTestPlayerController>(
			SpawnParameters);
	}

	bool CreatePlayerInventoryFixture(
		UWorld* World,
		const TCHAR* DebugName,
		ARpgInventoryAutomationTestPlayerController*& OutController,
		ARpgInventoryAutomationTestPlayerState*& OutPlayerState,
		URpgInventoryManagerComponent*& OutInventory)
	{
		OutController = SpawnPlayerController(World, DebugName);

		FActorSpawnParameters PlayerStateSpawnParameters;
		PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
			World,
			ARpgInventoryAutomationTestPlayerState::StaticClass(),
			FName(*FString::Printf(TEXT("%sState"), DebugName)));
		PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
		PlayerStateSpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		OutPlayerState =
			World
				? World->SpawnActor<
					  ARpgInventoryAutomationTestPlayerState>(
					  PlayerStateSpawnParameters)
				: nullptr;
		if (!OutController || !OutPlayerState)
		{
			OutInventory = nullptr;
			return false;
		}

		OutController->SetPlayerState(OutPlayerState);
		OutPlayerState->SetOwner(OutController);
		OutInventory = OutPlayerState->GetInventoryManagerComponent();
		return OutInventory != nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryRestoreStatusPerControllerTest,
	"SurvivalRpg.Inventory.IntentBoundary.RestoreStatusIsPerController",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryRestoreStatusPerControllerTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInventoryIntentBoundaryTests;

	FScopedIntentBoundaryWorld TestWorld;
	if (!TestTrue(
			TEXT("An isolated restore-lifecycle world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	FURL GameUrl;
	const FString GameModeOption = FString::Printf(
		TEXT("game=%s"),
		*ARpgGameModeBase::StaticClass()->GetPathName());
	GameUrl.AddOption(*GameModeOption);
	if (!TestTrue(
			TEXT("The isolated world installs the real RPG GameMode"),
			TestWorld.GetWorld()->SetGameMode(GameUrl)))
	{
		return false;
	}

	ARpgGameModeBase* GameMode =
		Cast<ARpgGameModeBase>(
			TestWorld.GetWorld()->GetAuthGameMode());
	if (!TestNotNull(TEXT("The RPG GameMode exists"), GameMode))
	{
		return false;
	}
	GameMode->bEnableDiskPersistence = false;
	TestWorld.GetWorld()->InitializeActorsForPlay(GameUrl);

	ARpgInventoryAutomationTestPlayerController* FirstController =
		SpawnPlayerController(
			TestWorld.GetWorld(),
			TEXT("RestoreStatusFirstController"));
	ARpgInventoryAutomationTestPlayerController* SecondController =
		SpawnPlayerController(
			TestWorld.GetWorld(),
			TEXT("RestoreStatusSecondController"));
	if (!TestNotNull(
			TEXT("The first controller exists"),
			FirstController) ||
		!TestNotNull(
			TEXT("The second controller exists"),
			SecondController))
	{
		return false;
	}

	UPlayer* FirstPlayer = NewObject<UPlayer>(
		TestWorld.GetGameInstance(),
		NAME_None,
		RF_Transient);
	UPlayer* SecondPlayer = NewObject<UPlayer>(
		TestWorld.GetGameInstance(),
		NAME_None,
		RF_Transient);
	FirstController->SetPlayer(FirstPlayer);
	SecondController->SetPlayer(SecondPlayer);
	if (!TestNotNull(
			TEXT("The first controller has a PlayerState"),
			FirstController->PlayerState.Get()) ||
		!TestNotNull(
			TEXT("The second controller has a PlayerState"),
			SecondController->PlayerState.Get()))
	{
		return false;
	}

	TestEqual(
		TEXT("Both offline fixtures deliberately resolve the same profile key"),
		GameMode->GetPlayerProfileKey(FirstController),
		GameMode->GetPlayerProfileKey(SecondController));
	TestFalse(
		TEXT("The first connection has no restore result before PostLogin"),
		GameMode->IsPlayerProfileRestoreComplete(FirstController));
	TestFalse(
		TEXT("The second connection has no restore result before PostLogin"),
		GameMode->IsPlayerProfileRestoreComplete(SecondController));

	GameMode->PostLogin(FirstController);
	TestTrue(
		TEXT("The first connection records its completed no-save restore attempt"),
		GameMode->IsPlayerProfileRestoreComplete(FirstController));
	TestFalse(
		TEXT("A completed attempt is not reported as a restored graph"),
		GameMode->HasRestoredPlayerProfile(FirstController));
	TestFalse(
		TEXT("The same profile key does not complete another controller's restore"),
		GameMode->IsPlayerProfileRestoreComplete(SecondController));

	GameMode->PostLogin(SecondController);
	TestTrue(
		TEXT("The second connection receives its own restore attempt"),
		GameMode->IsPlayerProfileRestoreComplete(SecondController));
	TestFalse(
		TEXT("The second no-save attempt is not reported as a restored graph"),
		GameMode->HasRestoredPlayerProfile(SecondController));

	GameMode->Logout(FirstController);
	TestFalse(
		TEXT("Logout removes only the exiting controller's restore result"),
		GameMode->IsPlayerProfileRestoreComplete(FirstController));
	TestTrue(
		TEXT("Logout preserves the other controller's restore result"),
		GameMode->IsPlayerProfileRestoreComplete(SecondController));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryRestoreIntentBoundaryTest,
	"SurvivalRpg.Inventory.IntentBoundary.RestoreReconstructsDefinitionPlacement",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryRestoreIntentBoundaryTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInventoryIntentBoundaryTests;

	FScopedIntentBoundaryWorld TestWorld;
	if (!TestTrue(
			TEXT("An isolated restore-intent world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("RestoreIntentSource"));
	URpgInventoryManagerComponent* ValidTarget =
		TestWorld.CreateInventory(TEXT("RestoreIntentValidTarget"));
	URpgInventoryManagerComponent* FootprintDriftTarget =
		TestWorld.CreateInventory(TEXT("RestoreIntentFootprintTarget"));
	URpgInventoryManagerComponent* RotatedFootprintTarget =
		TestWorld.CreateInventory(TEXT("RestoreIntentRotatedFootprintTarget"));
	URpgInventoryManagerComponent* ReconstructedOverlapTarget =
		TestWorld.CreateInventory(TEXT("RestoreIntentReconstructedOverlapTarget"));
	if (!TestNotNull(
			TEXT("The restore source inventory exists"),
			SourceInventory) ||
		!TestNotNull(
			TEXT("The valid restore target exists"),
			ValidTarget) ||
		!TestNotNull(
			TEXT("The footprint-drift target exists"),
			FootprintDriftTarget) ||
		!TestNotNull(
			TEXT("The rotated-footprint target exists"),
			RotatedFootprintTarget) ||
		!TestNotNull(
			TEXT("The reconstructed-overlap target exists"),
			ReconstructedOverlapTarget))
	{
		return false;
	}

	URpgInventoryItemInstance* WideItem =
		SourceInventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
			1,
			MakeRootPlacement(TEXT("Storage"), 0, 0));
	if (!TestNotNull(TEXT("The wide restore fixture exists"), WideItem))
	{
		return false;
	}

	const FRpgInventoryGraphSaveData ValidGraph =
		SourceInventory->ExportInventoryGraph();
	if (!TestEqual(
			TEXT("The current-schema fixture exports one item"),
			ValidGraph.Items.Num(),
			1))
	{
		return false;
	}
	TestEqual(
		TEXT("The fixture uses the current graph schema"),
		ValidGraph.SchemaVersion,
		FRpgInventoryGraphSaveData::CurrentSchemaVersion);

	FRpgInventoryMutationResult ValidRestoreResult;
	TestTrue(
		TEXT("A valid current-schema graph restores atomically"),
		ValidTarget->RestoreInventoryGraph(
			ValidGraph,
			ValidRestoreResult));
	TestEqual(
		TEXT("A successful persistence reconstruction reports Restore"),
		ValidRestoreResult.Operation,
		ERpgInventoryMutationOperation::Restore);
	TestEqual(
		TEXT("The valid restore reports success"),
		ValidRestoreResult.Code,
		ERpgInventoryMutationResultCode::Success);

	const TArray<FRpgInventoryEntryView> ValidEntriesBeforeLegacyOnlyRestore =
		ValidTarget->GetAllEntries();
	const int32 ValidRevisionBeforeLegacyOnlyRestore =
		ValidTarget->GetInventoryRevision();
	const uint64 ValidEpochBeforeLegacyOnlyRestore =
		ValidTarget->GetMutationEpoch();
	FRpgInventoryGraphSaveData LegacyOnlyPlacementGraph = ValidGraph;
	LegacyOnlyPlacementGraph.Items[0].Placement.ContainerId_DEPRECATED =
		LegacyOnlyPlacementGraph.Items[0].Container.Root;
	LegacyOnlyPlacementGraph.Items[0].Placement.ContainerHandle =
		FRpgInventoryContainerHandle();
	FRpgInventoryMutationResult LegacyOnlyPlacementResult;
	TestFalse(
		TEXT("Current-schema restore rejects a placement that only carries the deprecated root id"),
		ValidTarget->RestoreInventoryGraph(
			LegacyOnlyPlacementGraph,
			LegacyOnlyPlacementResult));
	TestEqual(
		TEXT("A deprecated-only current-schema placement reports an invalid container"),
		LegacyOnlyPlacementResult.Code,
		ERpgInventoryMutationResultCode::InvalidContainer);
	TestEqual(
		TEXT("Rejected deprecated-only restore preserves the inventory revision"),
		ValidTarget->GetInventoryRevision(),
		ValidRevisionBeforeLegacyOnlyRestore);
	TestEqual(
		TEXT("Rejected deprecated-only restore preserves the mutation epoch"),
		ValidTarget->GetMutationEpoch(),
		ValidEpochBeforeLegacyOnlyRestore);
	const TArray<FRpgInventoryEntryView> ValidEntriesAfterLegacyOnlyRestore =
		ValidTarget->GetAllEntries();
	if (TestEqual(
			TEXT("Rejected deprecated-only restore preserves the live entry"),
			ValidEntriesAfterLegacyOnlyRestore.Num(),
			ValidEntriesBeforeLegacyOnlyRestore.Num()) &&
		ValidEntriesAfterLegacyOnlyRestore.IsValidIndex(0) &&
		ValidEntriesBeforeLegacyOnlyRestore.IsValidIndex(0))
	{
		TestEqual(
			TEXT("Rejected deprecated-only restore preserves the runtime instance"),
			ValidEntriesAfterLegacyOnlyRestore[0].Instance.Get(),
			ValidEntriesBeforeLegacyOnlyRestore[0].Instance.Get());
		TestTrue(
			TEXT("Rejected deprecated-only restore preserves persistent identity"),
			ValidEntriesAfterLegacyOnlyRestore[0].ItemId ==
				ValidEntriesBeforeLegacyOnlyRestore[0].ItemId);
	}

	FRpgInventoryGraphSaveData ConflictingShadowGraph = ValidGraph;
	ConflictingShadowGraph.Items[0].Placement.ContainerId_DEPRECATED =
		TEXT("ConflictingDeprecatedRoot");
	FRpgInventoryMutationResult ConflictingShadowResult;
	TestTrue(
		TEXT("A canonical current-schema handle remains authoritative over a conflicting deprecated shadow"),
		ValidTarget->RestoreInventoryGraph(
			ConflictingShadowGraph,
			ConflictingShadowResult));
	TestEqual(
		TEXT("The canonical conflicting-shadow restore succeeds"),
		ConflictingShadowResult.Code,
		ERpgInventoryMutationResultCode::Success);
	const TArray<FRpgInventoryEntryView> EntriesAfterConflictingShadowRestore =
		ValidTarget->GetAllEntries();
	if (TestEqual(
			TEXT("The canonical conflicting-shadow restore retains one entry"),
			EntriesAfterConflictingShadowRestore.Num(),
			1) &&
		EntriesAfterConflictingShadowRestore.IsValidIndex(0))
	{
		TestTrue(
			TEXT("Canonical restore removes the deprecated shadow from runtime state"),
			EntriesAfterConflictingShadowRestore[0]
				.Placement.ContainerId_DEPRECATED.IsNone());
	}
	const FRpgInventoryGraphSaveData ReExportedConflictingShadowGraph =
		ValidTarget->ExportInventoryGraph();
	if (TestEqual(
			TEXT("Re-export after canonical restore retains one row"),
			ReExportedConflictingShadowGraph.Items.Num(),
			1) &&
		ReExportedConflictingShadowGraph.Items.IsValidIndex(0))
	{
		TestTrue(
			TEXT("Current graph export never rewrites the deprecated shadow"),
			ReExportedConflictingShadowGraph.Items[0]
				.Placement.ContainerId_DEPRECATED.IsNone());
	}

	FRpgInventoryGraphSaveData FootprintDriftGraph = ValidGraph;
	FootprintDriftGraph.Items[0].Placement.Width = 37;
	FootprintDriftGraph.Items[0].Placement.Height = 0;
	const int32 FootprintRevisionBeforeRestore =
		FootprintDriftTarget->GetInventoryRevision();
	FRpgInventoryMutationResult FootprintDriftResult;
	TestTrue(
		TEXT("Saved footprint fields are reconstructed from the current item definition"),
		FootprintDriftTarget->RestoreInventoryGraph(
			FootprintDriftGraph,
			FootprintDriftResult));
	TestEqual(
		TEXT("Definition-based persistence reconstruction reports Restore"),
		FootprintDriftResult.Operation,
		ERpgInventoryMutationOperation::Restore);
	TestEqual(
		TEXT("Definition-based footprint reconstruction succeeds"),
		FootprintDriftResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The reconstructed graph contains its original item"),
		FootprintDriftTarget->GetUsedEntryCount(),
		1);
	TestTrue(
		TEXT("A successful reconstructed restore advances the inventory revision"),
		FootprintDriftTarget->GetInventoryRevision() >
			FootprintRevisionBeforeRestore);
	const TArray<FRpgInventoryEntryView> ReconstructedEntries =
		FootprintDriftTarget->GetAllEntries();
	if (!TestEqual(
			TEXT("Exactly one reconstructed entry is available"),
			ReconstructedEntries.Num(),
			1) ||
		!ReconstructedEntries.IsValidIndex(0))
	{
		return false;
	}
	TestTrue(
		TEXT("Footprint reconstruction preserves persistent item identity"),
		ReconstructedEntries[0].ItemId == ValidGraph.Items[0].ItemId);
	TestEqual(
		TEXT("The current wide definition restores its canonical width"),
		ReconstructedEntries[0].Placement.Width,
		2);
	TestEqual(
		TEXT("The current wide definition restores its canonical height"),
		ReconstructedEntries[0].Placement.Height,
		1);
	TestFalse(
		TEXT("The unrotated save intent remains unrotated"),
		ReconstructedEntries[0].Placement.bRotated);

	FRpgInventoryGraphSaveData RotatedFootprintGraph = ValidGraph;
	RotatedFootprintGraph.Items[0].Placement.Width = 0;
	RotatedFootprintGraph.Items[0].Placement.Height = 99;
	RotatedFootprintGraph.Items[0].Placement.bRotated = true;
	FRpgInventoryMutationResult RotatedFootprintResult;
	TestTrue(
		TEXT("A permitted saved rotation survives definition-based reconstruction"),
		RotatedFootprintTarget->RestoreInventoryGraph(
			RotatedFootprintGraph,
			RotatedFootprintResult));
	TestEqual(
		TEXT("A permitted reconstructed rotation reports success"),
		RotatedFootprintResult.Code,
		ERpgInventoryMutationResultCode::Success);
	const TArray<FRpgInventoryEntryView> RotatedEntries =
		RotatedFootprintTarget->GetAllEntries();
	if (!TestEqual(
			TEXT("Exactly one rotated reconstructed entry is available"),
			RotatedEntries.Num(),
			1) ||
		!RotatedEntries.IsValidIndex(0))
	{
		return false;
	}
	TestEqual(
		TEXT("Rotated reconstruction keeps the definition's unrotated width"),
		RotatedEntries[0].Placement.Width,
		2);
	TestEqual(
		TEXT("Rotated reconstruction keeps the definition's unrotated height"),
		RotatedEntries[0].Placement.Height,
		1);
	TestTrue(
		TEXT("Rotated reconstruction preserves an allowed orientation"),
		RotatedEntries[0].Placement.bRotated);
	TestEqual(
		TEXT("The rotated item occupies one grid column"),
		RotatedEntries[0].Placement.GetOccupiedSize().Width,
		1);
	TestEqual(
		TEXT("The rotated item occupies two grid rows"),
		RotatedEntries[0].Placement.GetOccupiedSize().Height,
		2);

	FRpgInventoryGraphSaveData ReconstructedOverlapGraph = ValidGraph;
	ReconstructedOverlapGraph.Items[0].Placement.Width = 1;
	ReconstructedOverlapGraph.Items[0].Placement.Height = 1;
	FRpgInventorySavedItem OverlappingRow =
		ReconstructedOverlapGraph.Items[0];
	OverlappingRow.ItemId = FRpgInventoryItemId::NewId();
	OverlappingRow.Placement.X = 1;
	ReconstructedOverlapGraph.Items.Add(MoveTemp(OverlappingRow));
	const int32 OverlapRevisionBeforeRestore =
		ReconstructedOverlapTarget->GetInventoryRevision();
	FRpgInventoryMutationResult ReconstructedOverlapResult;
	TestFalse(
		TEXT("Current footprints that overlap after reconstruction reject the complete graph"),
		ReconstructedOverlapTarget->RestoreInventoryGraph(
			ReconstructedOverlapGraph,
			ReconstructedOverlapResult));
	TestEqual(
		TEXT("A reconstructed overlap reports the shared occupied result"),
		ReconstructedOverlapResult.Code,
		ERpgInventoryMutationResultCode::Occupied);
	TestEqual(
		TEXT("Rejected reconstructed overlap leaves the target empty"),
		ReconstructedOverlapTarget->GetUsedEntryCount(),
		0);
	TestEqual(
		TEXT("Rejected reconstructed overlap does not advance the revision"),
		ReconstructedOverlapTarget->GetInventoryRevision(),
		OverlapRevisionBeforeRestore);

	URpgInventoryManagerComponent* RotationSource =
		TestWorld.CreateInventory(TEXT("RestoreIntentRotationSource"));
	if (!TestNotNull(
			TEXT("The rotation-drift source exists"),
			RotationSource))
	{
		return false;
	}
	URpgInventoryItemInstance* WeaponItem =
		RotationSource->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestWeaponItemDefinition::StaticClass(),
			1,
			MakeRootPlacement(TEXT("Storage"), 0, 0));
	if (!TestNotNull(
			TEXT("The rotation-drift weapon fixture exists"),
			WeaponItem))
	{
		return false;
	}

	FRpgInventoryGraphSaveData RotationDriftGraph =
		RotationSource->ExportInventoryGraph();
	if (!TestEqual(
			TEXT("The rotation fixture exports one item"),
			RotationDriftGraph.Items.Num(),
			1))
	{
		return false;
	}

	const FRpgInventoryContainerHandle CarryHandle =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	FRpgInventorySavedItem& RotationDriftItem =
		RotationDriftGraph.Items[0];
	RotationDriftItem.Container = CarryHandle;
	RotationDriftItem.Placement.SetContainerHandle(CarryHandle);
	RotationDriftItem.Placement.X = 0;
	RotationDriftItem.Placement.Y = 0;
	RotationDriftItem.Placement.Width = 1;
	RotationDriftItem.Placement.Height = 1;
	RotationDriftItem.Placement.bRotated = true;

	ARpgInventoryAutomationTestPlayerController* TargetController = nullptr;
	ARpgInventoryAutomationTestPlayerState* TargetPlayerState = nullptr;
	URpgInventoryManagerComponent* RotationDriftTarget = nullptr;
	if (!TestTrue(
			TEXT("A real player-inventory restore target exists"),
			CreatePlayerInventoryFixture(
				TestWorld.GetWorld(),
				TEXT("RestoreRotationTarget"),
				TargetController,
				TargetPlayerState,
				RotationDriftTarget)))
	{
		return false;
	}

	FRpgInventoryMutationResult RotationDriftResult;
	TestTrue(
		TEXT("A Carry-root save row is reconstructed to its current single-cell contract"),
		RotationDriftTarget->RestoreInventoryGraph(
			RotationDriftGraph,
			RotationDriftResult));
	TestEqual(
		TEXT("Single-cell reconstruction remains a Restore operation"),
		RotationDriftResult.Operation,
		ERpgInventoryMutationOperation::Restore);
	TestEqual(
		TEXT("Single-cell reconstruction reports success"),
		RotationDriftResult.Code,
		ERpgInventoryMutationResultCode::Success);
	TestEqual(
		TEXT("The reconstructed Carry graph contains its item"),
		RotationDriftTarget->GetUsedEntryCount(),
		1);
	const TArray<FRpgInventoryEntryView> CarryEntries =
		RotationDriftTarget->GetAllEntries();
	if (!TestEqual(
			TEXT("Exactly one reconstructed Carry entry is available"),
			CarryEntries.Num(),
			1) ||
		!CarryEntries.IsValidIndex(0))
	{
		return false;
	}
	TestEqual(
		TEXT("Carry reconstruction enforces one-cell width"),
		CarryEntries[0].Placement.Width,
		1);
	TestEqual(
		TEXT("Carry reconstruction enforces one-cell height"),
		CarryEntries[0].Placement.Height,
		1);
	TestFalse(
		TEXT("Rotation is semantically removed from a single-cell Carry root"),
		CarryEntries[0].Placement.bRotated);

	URpgInventoryManagerComponent* FixedRotationSource =
		TestWorld.CreateInventory(TEXT("RestoreIntentFixedRotationSource"));
	URpgInventoryManagerComponent* FixedRotationTarget =
		TestWorld.CreateInventory(TEXT("RestoreIntentFixedRotationTarget"));
	if (!TestNotNull(
			TEXT("The fixed-orientation source exists"),
			FixedRotationSource) ||
		!TestNotNull(
			TEXT("The fixed-orientation target exists"),
			FixedRotationTarget))
	{
		return false;
	}
	URpgInventoryItemInstance* FixedWideItem =
		FixedRotationSource->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestFixedWideItemDefinition::StaticClass(),
			1,
			MakeRootPlacement(TEXT("Storage"), 0, 0));
	if (!TestNotNull(
			TEXT("The fixed-orientation item fixture exists"),
			FixedWideItem))
	{
		return false;
	}
	FRpgInventoryGraphSaveData DisallowedRotationGraph =
		FixedRotationSource->ExportInventoryGraph();
	if (!TestEqual(
			TEXT("The fixed-orientation fixture exports one item"),
			DisallowedRotationGraph.Items.Num(),
			1))
	{
		return false;
	}
	DisallowedRotationGraph.Items[0].Placement.bRotated = true;
	const int32 FixedRotationRevisionBeforeRestore =
		FixedRotationTarget->GetInventoryRevision();
	FRpgInventoryMutationResult DisallowedRotationResult;
	TestFalse(
		TEXT("A saved rotation forbidden by the current item definition is rejected"),
		FixedRotationTarget->RestoreInventoryGraph(
			DisallowedRotationGraph,
			DisallowedRotationResult));
	TestEqual(
		TEXT("A definition-forbidden rotation reports invalid placement"),
		DisallowedRotationResult.Code,
		ERpgInventoryMutationResultCode::InvalidPlacement);
	TestEqual(
		TEXT("Rejected definition rotation leaves the target empty"),
		FixedRotationTarget->GetUsedEntryCount(),
		0);
	TestEqual(
		TEXT("Rejected definition rotation does not advance the revision"),
		FixedRotationTarget->GetInventoryRevision(),
		FixedRotationRevisionBeforeRestore);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
