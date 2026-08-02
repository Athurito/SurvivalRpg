#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryDragDropCoordinator.h"
#include "RpgInventoryDragDropTypes.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "RpgPlayerInventoryLayoutDefinition.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"

#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/Player.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

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

	bool AssignTransientLayoutPawnData(APlayerController* Controller)
	{
		ARpgPlayerState* PlayerState = Controller
			? Controller->GetPlayerState<ARpgPlayerState>()
			: nullptr;
		if (!PlayerState || PlayerState->GetPawnData<URpgPawnData>())
		{
			return false;
		}

		URpgPlayerInventoryLayoutDefinition* LayoutDefinition =
			NewObject<URpgPlayerInventoryLayoutDefinition>(
				PlayerState,
				NAME_None,
				RF_Transient);
		URpgPawnData* PawnData = NewObject<URpgPawnData>(
			PlayerState,
			NAME_None,
			RF_Transient);
		if (!LayoutDefinition || !PawnData)
		{
			return false;
		}

		PawnData->InventoryLayoutDefinition = LayoutDefinition;
		PlayerState->SetPawnData(PawnData);
		return PlayerState->GetPawnData<URpgPawnData>() == PawnData;
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

	TestNotEqual(
		TEXT("Two offline controllers without NetIds receive distinct private profile keys"),
		GameMode->GetPlayerProfileKey(FirstController),
		GameMode->GetPlayerProfileKey(SecondController));
	TestFalse(
		TEXT("The first connection has no restore result before PostLogin"),
		GameMode->IsPlayerProfileRestoreComplete(FirstController));
	TestFalse(
		TEXT("The second connection has no restore result before PostLogin"),
		GameMode->IsPlayerProfileRestoreComplete(SecondController));
	FRpgPlayerSaveData& FirstSaveData =
		GameMode->GetOrCreatePlayerSaveData(FirstController);
	FirstSaveData.bHasInventoryGraph = true;
	FirstSaveData.InventoryGraph = FRpgInventoryGraphSaveData();
	FRpgPlayerSaveData& SecondSaveData =
		GameMode->GetOrCreatePlayerSaveData(SecondController);
	SecondSaveData.bHasInventoryGraph = true;
	SecondSaveData.InventoryGraph = FRpgInventoryGraphSaveData();

	GameMode->PostLogin(FirstController);
	TestFalse(
		TEXT("PostLogin before PawnData does not cache a failed restore attempt"),
		GameMode->IsPlayerProfileRestoreComplete(FirstController));
	TestFalse(
		TEXT("The first profile does not affect another controller's restore state"),
		GameMode->IsPlayerProfileRestoreComplete(SecondController));
	if (!TestTrue(
			TEXT("The first PlayerState receives a transient PawnData-backed layout"),
			AssignTransientLayoutPawnData(FirstController)))
	{
		return false;
	}

	TestTrue(
		TEXT("The real deferred retry attempts the first restore once PawnData is ready"),
		GameMode->TryRestorePlayerProfileWhenReady(FirstController));
	TestTrue(
		TEXT("The first connection restores after its PawnData layout becomes ready"),
		GameMode->IsPlayerProfileRestoreComplete(FirstController));
	TestTrue(
		TEXT("The deferred first graph restore succeeds instead of retaining an early failure"),
		GameMode->HasRestoredPlayerProfile(FirstController));

	GameMode->PostLogin(SecondController);
	TestFalse(
		TEXT("The second connection independently waits for its own PawnData layout"),
		GameMode->IsPlayerProfileRestoreComplete(SecondController));
	if (!TestTrue(
			TEXT("The second PlayerState receives a transient PawnData-backed layout"),
			AssignTransientLayoutPawnData(SecondController)))
	{
		return false;
	}

	TestTrue(
		TEXT("The real deferred retry attempts the second restore once PawnData is ready"),
		GameMode->TryRestorePlayerProfileWhenReady(SecondController));
	TestTrue(
		TEXT("The second connection receives its own completed restore attempt"),
		GameMode->IsPlayerProfileRestoreComplete(SecondController));
	TestTrue(
		TEXT("The second connection restores its private saved graph independently"),
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
	FRpgInventoryMissingSpatialFragmentContractTest,
	"SurvivalRpg.Inventory.IntentBoundary.MissingSpatialFragmentFailsClosed",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryMissingSpatialFragmentContractTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	using namespace RpgInventoryIntentBoundaryTests;

	FScopedIntentBoundaryWorld TestWorld;
	if (!TestTrue(
			TEXT("An isolated missing-spatial contract world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	ARpgInventoryAutomationTestPlayerController* Controller = nullptr;
	ARpgInventoryAutomationTestPlayerState* PlayerState = nullptr;
	URpgInventoryManagerComponent* Inventory = nullptr;
	if (!TestTrue(
			TEXT("A real player-inventory fixture exists"),
			CreatePlayerInventoryFixture(
				TestWorld.GetWorld(),
				TEXT("MissingSpatialContract"),
				Controller,
				PlayerState,
				Inventory)))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle GearChest =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::GearChestGroupId);
	const FRpgInventoryContainerHandle CarryMainHand =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	FRpgInventoryGridSize GridSize;
	if (!TestTrue(
			TEXT("The Content fixture handle resolves"),
			Inventory->GetGridSizeForContainerHandle(Pockets, GridSize)) ||
		!TestTrue(
			TEXT("The Gear fixture handle resolves"),
			Inventory->GetGridSizeForContainerHandle(GearChest, GridSize)) ||
		!TestTrue(
			TEXT("The Carry fixture handle resolves"),
			Inventory->GetGridSizeForContainerHandle(
				CarryMainHand,
				GridSize)))
	{
		return false;
	}

	const int32 EmptyRevision = Inventory->GetInventoryRevision();
	const uint64 EmptyEpoch = Inventory->GetMutationEpoch();
	FRpgInventoryPlacementQuery ExactQuery;
	ExactQuery.Purpose = ERpgInventoryPlacementPurpose::Add;
	ExactQuery.Search = ERpgInventoryPlacementSearch::Exact;
	ExactQuery.Subject = FRpgInventoryPlacementSubject::FromDefinition(
		URpgInventoryAutomationTestMissingSpatialItemDefinition::
			StaticClass(),
		1);
	ExactQuery.TargetContainer = Pockets;
	ExactQuery.ExactPlacement = MakeRootPlacement(
		URpgPlayerInventoryLayoutComponent::PocketsGroupId,
		1,
		0);
	const FRpgInventoryPlacementPlan ExactPlan =
		Inventory->EvaluatePlacement(ExactQuery);
	TestEqual(
		TEXT("Exact Content evaluation reports an invalid placement"),
		ExactPlan.Code,
		ERpgInventoryMutationResultCode::InvalidPlacement);
	TestFalse(
		TEXT("Exact Content evaluation does not produce a successful plan"),
		ExactPlan.IsSuccess());
	TestEqual(
		TEXT("Exact Content evaluation produces no placement steps"),
		ExactPlan.Steps.Num(),
		0);
	TestEqual(
		TEXT("Exact Content evaluation applies no quantity"),
		ExactPlan.AppliedQuantity,
		0);
	TestFalse(
		TEXT("The exact legacy preflight rejects the malformed definition"),
		Inventory->CanAddItemDefinitionToPlacement(
			URpgInventoryAutomationTestMissingSpatialItemDefinition::
				StaticClass(),
			1,
			ExactQuery.ExactPlacement));
	TestNull(
		TEXT("The exact legacy mutation seam cannot create the malformed item"),
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestMissingSpatialItemDefinition::
				StaticClass(),
			1,
			ExactQuery.ExactPlacement));

	FRpgInventoryPlacementQuery FirstFitQuery;
	FirstFitQuery.Purpose = ERpgInventoryPlacementPurpose::Add;
	FirstFitQuery.Search = ERpgInventoryPlacementSearch::FirstFit;
	FirstFitQuery.Subject = FRpgInventoryPlacementSubject::FromDefinition(
		URpgInventoryAutomationTestMissingSpatialItemDefinition::
			StaticClass(),
		1);
	FirstFitQuery.TargetContainer = Pockets;
	const FRpgInventoryPlacementPlan FirstFitPlan =
		Inventory->EvaluatePlacement(FirstFitQuery);
	TestEqual(
		TEXT("FirstFit Content evaluation reports an invalid placement instead of NoSpace"),
		FirstFitPlan.Code,
		ERpgInventoryMutationResultCode::InvalidPlacement);
	TestFalse(
		TEXT("FirstFit Content evaluation does not produce a successful plan"),
		FirstFitPlan.IsSuccess());
	TestEqual(
		TEXT("FirstFit Content evaluation produces no placement steps"),
		FirstFitPlan.Steps.Num(),
		0);
	TestEqual(
		TEXT("FirstFit Content evaluation applies no quantity"),
		FirstFitPlan.AppliedQuantity,
		0);
	TestFalse(
		TEXT("The FirstFit legacy preflight rejects the malformed definition"),
		Inventory->CanAddItemDefinition(
			URpgInventoryAutomationTestMissingSpatialItemDefinition::
				StaticClass(),
			1));
	TestNull(
		TEXT("The FirstFit legacy mutation seam cannot create the malformed item"),
		Inventory->AddItemDefinition(
			URpgInventoryAutomationTestMissingSpatialItemDefinition::
				StaticClass(),
			1));
	TestEqual(
		TEXT("Rejected placement paths leave the inventory empty"),
		Inventory->GetUsedEntryCount(),
		0);
	TestEqual(
		TEXT("Rejected placement paths preserve the inventory revision"),
		Inventory->GetInventoryRevision(),
		EmptyRevision);
	TestEqual(
		TEXT("Rejected placement paths preserve the mutation epoch"),
		Inventory->GetMutationEpoch(),
		EmptyEpoch);

	URpgInventoryItemInstance* Sentinel =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakeRootPlacement(
				URpgPlayerInventoryLayoutComponent::PocketsGroupId,
				0,
				0));
	if (!TestNotNull(
			TEXT("A valid sentinel item exists before malformed restores"),
			Sentinel))
	{
		return false;
	}
	const TArray<FRpgInventoryEntryView> BaselineEntries =
		Inventory->GetAllEntries();
	if (!TestEqual(
			TEXT("The restore baseline contains exactly one entry"),
			BaselineEntries.Num(),
			1) ||
		!BaselineEntries.IsValidIndex(0))
	{
		return false;
	}
	const FRpgInventoryEntryView BaselineEntry = BaselineEntries[0];
	const int32 BaselineRevision = Inventory->GetInventoryRevision();
	const uint64 BaselineEpoch = Inventory->GetMutationEpoch();

	auto TestRejectedRestore =
		[this,
		 Inventory,
		 &BaselineEntry,
		 BaselineRevision,
		 BaselineEpoch](
			const TCHAR* CaseName,
			TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
			const FRpgInventoryContainerHandle& Container) -> bool
	{
		FRpgInventoryGraphSaveData Graph;
		FRpgInventorySavedItem& Row =
			Graph.Items.AddDefaulted_GetRef();
		Row.ItemId = FRpgInventoryItemId::NewId();
		Row.ItemDefinition = ItemDefinition;
		Row.StackCount = 1;
		Row.Container = Container;
		Row.Placement.SetContainerHandle(Container);
		Row.Placement.X = 0;
		Row.Placement.Y = 0;
		Row.Placement.Width = 1;
		Row.Placement.Height = 1;

		FRpgInventoryMutationResult RestoreResult;
		const bool bRestored =
			Inventory->RestoreInventoryGraph(Graph, RestoreResult);
		TestFalse(
			*FString::Printf(
				TEXT("%s restore rejects the malformed definition"),
				CaseName),
			bRestored);
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection remains a Restore operation"),
				CaseName),
			RestoreResult.Operation,
			ERpgInventoryMutationOperation::Restore);
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection reports InvalidPlacement"),
				CaseName),
			RestoreResult.Code,
			ERpgInventoryMutationResultCode::InvalidPlacement);
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection applies no quantity"),
				CaseName),
			RestoreResult.AppliedQuantity,
			0);
		TestTrue(
			*FString::Printf(
				TEXT("%s rejection publishes no mutation deltas"),
				CaseName),
			RestoreResult.Deltas.IsEmpty());
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection preserves the inventory revision"),
				CaseName),
			Inventory->GetInventoryRevision(),
			BaselineRevision);
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection preserves the mutation epoch"),
				CaseName),
			Inventory->GetMutationEpoch(),
			BaselineEpoch);

		const TArray<FRpgInventoryEntryView> Entries =
			Inventory->GetAllEntries();
		if (!TestEqual(
				*FString::Printf(
					TEXT("%s rejection preserves the live entry count"),
					CaseName),
				Entries.Num(),
				1) ||
			!Entries.IsValidIndex(0))
		{
			return false;
		}
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection preserves the runtime instance"),
				CaseName),
			Entries[0].Instance.Get(),
			BaselineEntry.Instance.Get());
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection preserves the entry id"),
				CaseName),
			Entries[0].EntryId,
			BaselineEntry.EntryId);
		TestTrue(
			*FString::Printf(
				TEXT("%s rejection preserves the item id"),
				CaseName),
			Entries[0].ItemId == BaselineEntry.ItemId);
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection preserves the placement"),
				CaseName),
			Entries[0].Placement,
			BaselineEntry.Placement);
		TestEqual(
			*FString::Printf(
				TEXT("%s rejection preserves the stack count"),
				CaseName),
			Entries[0].StackCount,
			BaselineEntry.StackCount);
		return !bRestored;
	};

	if (!TestRejectedRestore(
			TEXT("Content"),
			URpgInventoryAutomationTestMissingSpatialItemDefinition::
				StaticClass(),
			Pockets) ||
		!TestRejectedRestore(
			TEXT("Gear"),
			URpgInventoryAutomationTestMissingSpatialArmorItemDefinition::
				StaticClass(),
			GearChest) ||
		!TestRejectedRestore(
			TEXT("Carry"),
			URpgInventoryAutomationTestMissingSpatialWeaponItemDefinition::
				StaticClass(),
			CarryMainHand))
	{
		return false;
	}

	URpgInventoryItemInstance* DetachedMissingSpatialItem =
		NewObject<URpgInventoryItemInstance>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);
	FObjectPropertyBase* ItemDefinitionProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgInventoryItemInstance::StaticClass(),
			TEXT("ItemDef"));
	if (!TestNotNull(
			TEXT("The test can initialize the private replicated definition field"),
			ItemDefinitionProperty) ||
		!TestNotNull(
			TEXT("A detached malformed drag fixture exists"),
			DetachedMissingSpatialItem))
	{
		return false;
	}
	ItemDefinitionProperty->SetObjectPropertyValue_InContainer(
		DetachedMissingSpatialItem,
		URpgInventoryAutomationTestMissingSpatialWeaponItemDefinition::
			StaticClass());
	if (!TestTrue(
			TEXT("The detached drag fixture owns the intended malformed definition"),
			DetachedMissingSpatialItem->GetItemDef() ==
				URpgInventoryAutomationTestMissingSpatialWeaponItemDefinition::
					StaticClass()))
	{
		return false;
	}

	FRpgInventoryDragPayload Payload =
		URpgInventoryDragDropCoordinator::MakeEquipmentPayload(
			DetachedMissingSpatialItem,
			ERpgEquipmentSlot::MainHand);
	Payload.SourceInventory = Inventory;
	Payload.EntryId = FGuid::NewGuid();
	Payload.StackCount = 1;
	Payload.SourcePlacement = MakeRootPlacement(
		URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId,
		0,
		0);
	TestTrue(
		TEXT("The drag fixture isolates the missing spatial contract behind otherwise valid equipment data"),
		Payload.SourceInventory != nullptr &&
			Payload.ItemInstance != nullptr &&
			Payload.EntryId.IsValid() &&
			Payload.StackCount > 0 &&
			Payload.SourcePlacement.IsValid() &&
			Payload.EquipmentSlot == ERpgEquipmentSlot::MainHand);
	TestFalse(
		TEXT("The drag payload factory does not synthesize a 1x1 footprint"),
		Payload.ItemFootprint.IsValid());
	TestFalse(
		TEXT("A missing-spatial equipment payload fails closed"),
		URpgInventoryDragDropCoordinator::IsPayloadValid(Payload));
	FRpgInventoryDragPayload SpoofedUnitPayload = Payload;
	SpoofedUnitPayload.ItemFootprint.Width = 1;
	SpoofedUnitPayload.ItemFootprint.Height = 1;
	TestFalse(
		TEXT("A client-supplied 1x1 footprint cannot replace the missing definition contract"),
		URpgInventoryDragDropCoordinator::IsPayloadValid(
			SpoofedUnitPayload));
	TestEqual(
		TEXT("Rejected drag payload validation preserves the inventory revision"),
		Inventory->GetInventoryRevision(),
		BaselineRevision);
	TestEqual(
		TEXT("Rejected drag payload validation preserves the mutation epoch"),
		Inventory->GetMutationEpoch(),
		BaselineEpoch);
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

	const TArray<FRpgInventoryEntryView> ValidEntriesBeforeMissingHandleRestore =
		ValidTarget->GetAllEntries();
	if (TestEqual(
		TEXT("The restored current-schema graph retains one live entry"),
		ValidEntriesBeforeMissingHandleRestore.Num(),
		1) &&
		ValidEntriesBeforeMissingHandleRestore.IsValidIndex(0))
	{
		TestTrue(
			TEXT("Restore preserves the exact canonical placement handle"),
			ValidEntriesBeforeMissingHandleRestore[0]
				.Placement.GetContainerHandle() ==
				ValidGraph.Items[0].Placement.GetContainerHandle());
	}
	const FRpgInventoryGraphSaveData ReExportedValidGraph =
		ValidTarget->ExportInventoryGraph();
	if (TestEqual(
		TEXT("Re-export retains one current-schema row"),
		ReExportedValidGraph.Items.Num(),
		1) &&
		ReExportedValidGraph.Items.IsValidIndex(0))
	{
		TestTrue(
			TEXT("Current graph export writes the exact canonical placement handle"),
			ReExportedValidGraph.Items[0].Placement.GetContainerHandle() ==
				ValidGraph.Items[0].Placement.GetContainerHandle());
	}

	const int32 ValidRevisionBeforeMissingHandleRestore =
		ValidTarget->GetInventoryRevision();
	const uint64 ValidEpochBeforeMissingHandleRestore =
		ValidTarget->GetMutationEpoch();
	FRpgInventoryGraphSaveData MissingHandlePlacementGraph = ValidGraph;
	MissingHandlePlacementGraph.Items[0].Placement.SetContainerHandle(
		FRpgInventoryContainerHandle());
	FRpgInventoryMutationResult MissingHandlePlacementResult;
	TestFalse(
		TEXT("Current-schema restore requires every placement to carry a canonical handle"),
		ValidTarget->RestoreInventoryGraph(
			MissingHandlePlacementGraph,
			MissingHandlePlacementResult));
	TestEqual(
		TEXT("A current-schema placement without a handle reports an invalid container"),
		MissingHandlePlacementResult.Code,
		ERpgInventoryMutationResultCode::InvalidContainer);
	TestEqual(
		TEXT("Rejected missing-handle restore preserves the inventory revision"),
		ValidTarget->GetInventoryRevision(),
		ValidRevisionBeforeMissingHandleRestore);
	TestEqual(
		TEXT("Rejected missing-handle restore preserves the mutation epoch"),
		ValidTarget->GetMutationEpoch(),
		ValidEpochBeforeMissingHandleRestore);
	const TArray<FRpgInventoryEntryView> ValidEntriesAfterMissingHandleRestore =
		ValidTarget->GetAllEntries();
	if (TestEqual(
			TEXT("Rejected missing-handle restore preserves the live entry"),
			ValidEntriesAfterMissingHandleRestore.Num(),
			ValidEntriesBeforeMissingHandleRestore.Num()) &&
		ValidEntriesAfterMissingHandleRestore.IsValidIndex(0) &&
		ValidEntriesBeforeMissingHandleRestore.IsValidIndex(0))
	{
		TestEqual(
			TEXT("Rejected missing-handle restore preserves the runtime instance"),
			ValidEntriesAfterMissingHandleRestore[0].Instance.Get(),
			ValidEntriesBeforeMissingHandleRestore[0].Instance.Get());
		TestTrue(
			TEXT("Rejected missing-handle restore preserves persistent identity"),
			ValidEntriesAfterMissingHandleRestore[0].ItemId ==
				ValidEntriesBeforeMissingHandleRestore[0].ItemId);
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
