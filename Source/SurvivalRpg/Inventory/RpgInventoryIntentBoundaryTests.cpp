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
	"SurvivalRpg.Inventory.IntentBoundary.RestoreReportsIntentAndRejectsPlacementDrift",
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
	if (!TestNotNull(
			TEXT("The restore source inventory exists"),
			SourceInventory) ||
		!TestNotNull(
			TEXT("The valid restore target exists"),
			ValidTarget) ||
		!TestNotNull(
			TEXT("The footprint-drift target exists"),
			FootprintDriftTarget))
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

	FRpgInventoryGraphSaveData FootprintDriftGraph = ValidGraph;
	FootprintDriftGraph.Items[0].Placement.Width += 1;
	FRpgInventoryMutationResult FootprintDriftResult;
	TestFalse(
		TEXT("Current-schema footprint drift is rejected fail-closed"),
		FootprintDriftTarget->RestoreInventoryGraph(
			FootprintDriftGraph,
			FootprintDriftResult));
	TestEqual(
		TEXT("A rejected persistence reconstruction still reports Restore"),
		FootprintDriftResult.Operation,
		ERpgInventoryMutationOperation::Restore);
	TestEqual(
		TEXT("Footprint drift is classified as invalid placement"),
		FootprintDriftResult.Code,
		ERpgInventoryMutationResultCode::InvalidPlacement);
	TestEqual(
		TEXT("A rejected footprint graph leaves the target empty"),
		FootprintDriftTarget->GetUsedEntryCount(),
		0);

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
	TestFalse(
		TEXT("A rotated save row for a non-rotating Carry cell is rejected"),
		RotationDriftTarget->RestoreInventoryGraph(
			RotationDriftGraph,
			RotationDriftResult));
	TestEqual(
		TEXT("Rejected rotation drift remains a Restore operation"),
		RotationDriftResult.Operation,
		ERpgInventoryMutationOperation::Restore);
	TestEqual(
		TEXT("Rotation drift is classified as invalid placement"),
		RotationDriftResult.Code,
		ERpgInventoryMutationResultCode::InvalidPlacement);
	TestEqual(
		TEXT("A rejected rotation graph leaves the player inventory empty"),
		RotationDriftTarget->GetUsedEntryCount(),
		0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
