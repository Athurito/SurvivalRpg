#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Components/BoxComponent.h"
#include "Network/RpgLootHarvestNetworkTestTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgPhysicalStoragePIETests
{
	struct FState : FBasePIENetworkComponentState {};
	FTimespan Timeout() { return FTimespan::FromSeconds(60); }

	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(World) && !World->bIsTearingDown;
		return false;
	}

	/** Configures only temporary CQTest worlds; AGameModeBase has no project disk-save lifecycle. */
	struct FScopedMode
	{
		FDelegateHandle Handle;
		~FScopedMode() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start(UClass* ControllerClass)
		{
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddLambda([ControllerClass](AGameModeBase* Mode)
			{
				if (!Mode || Mode->GetClass() != AGameModeBase::StaticClass() || !Mode->GetWorld() || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
				Mode->PlayerControllerClass = ControllerClass;
				Mode->PlayerStateClass = ARpgNetworkAutomationHarvesterState::StaticClass();
				Mode->DefaultPawnClass = ADefaultPawn::StaticClass();
			});
		}
	};

	ARpgPlayerController* LocalController(UWorld* World)
	{
		return ActiveWorld(World) ? Cast<ARpgPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	URpgInventoryManagerComponent* PlayerInventory(const ARpgPlayerController* Controller)
	{
		const ARpgPlayerState* State = Controller ? Controller->GetPlayerState<ARpgPlayerState>() : nullptr;
		return State ? State->GetInventoryManagerComponent() : nullptr;
	}

	ARpgPlayerController* ServerController(FState& State, int32 Index)
	{
		return ActiveWorld(State.World) && State.ClientConnections.IsValidIndex(Index) && IsValid(State.ClientConnections[Index])
			? Cast<ARpgPlayerController>(State.ClientConnections[Index]->PlayerController) : nullptr;
	}

	bool ControllerReady(ARpgPlayerController* Controller)
	{
		return IsValid(Controller) && IsValid(Controller->GetPawn()) && PlayerInventory(Controller) &&
			Controller->GetInventoryUiActionComponent() && Controller->GetPlayerInventoryLayoutComponent();
	}

	ARpgInventoryContainerActor* Chest(UWorld* World, FName Id)
	{
		if (!ActiveWorld(World)) return nullptr;
		for (TActorIterator<ARpgInventoryContainerActor> It(World); It; ++It)
			if (It->GetContainerComponent()->GetPersistentContainerId() == Id) return *It;
		return nullptr;
	}

	/** Read the real controller's reflected admission record so rejected requests are observed after server receipt. */
	const FRpgPhysicalStorageCommandRecord* Completed(ARpgPlayerController* Controller, FGuid RequestId)
	{
		const URpgInventoryUiActionComponent* Actions = Controller ? Controller->GetInventoryUiActionComponent() : nullptr;
		const FArrayProperty* Property = FindFProperty<FArrayProperty>(URpgInventoryUiActionComponent::StaticClass(), TEXT("PhysicalStorageCommands"));
		if (!Actions || !Property) return nullptr;
		const auto* Records = Property->ContainerPtrToValuePtr<TArray<FRpgPhysicalStorageCommandRecord>>(Actions);
		return Records->FindByPredicate([RequestId](const FRpgPhysicalStorageCommandRecord& Row) { return Row.Request.RequestId == RequestId && !Row.bInFlight; });
	}

	FRpgPhysicalStorageRequest Request(UWorld* World, FName Id, ERpgPhysicalStorageCommand Command)
	{
		FRpgPhysicalStorageRequest Result;
		Result.RequestId = FGuid::NewGuid();
		Result.ContainerId = Id;
		Result.Command = Command;
		if (const ARpgInventoryContainerActor* Actor = Chest(World, Id)) Result.ExpectedSettingsRevision = Actor->GetContainerComponent()->GetSettingsRevision();
		return Result;
	}

	URpgCraftingStationComponent* Station(UWorld* World)
	{
		if (!ActiveWorld(World)) return nullptr;
		for (TActorIterator<ARpgCraftingStationActor> It(World); It; ++It)
			if (!It->IsActorBeingDestroyed()) return It->GetCraftingStationComponent();
		return nullptr;
	}

	int32 RefundCount(const URpgCraftingStationComponent* Crafting, UClass* ItemClass)
	{
		int32 Result = 0;
		if (Crafting)
			for (const FRpgCraftingRefundEntry& Refund : Crafting->GetCurrentOrder().UnitCredits)
				if (Refund.ItemDefinition == ItemClass) Result += Refund.Count;
		return Result;
	}

	TArray<ARpgInventoryContainerActor*> BuiltChests(UWorld* World)
	{
		TArray<ARpgInventoryContainerActor*> Result;
		if (ActiveWorld(World))
			for (TActorIterator<ARpgInventoryContainerActor> It(World); It; ++It)
				if (!It->IsActorBeingDestroyed() && It->GetContainerComponent()->ExportPhysicalStorageMetadata().bRuntimeBuilt)
					Result.Add(*It);
		return Result;
	}
}

NETWORK_TEST_CLASS(PhysicalStoragePIE, "SurvivalRpg.Network")
{
	using FState = RpgPhysicalStoragePIETests::FState;
	RpgPhysicalStoragePIETests::FScopedMode ModeScope;
	TStrongObjectPtr<URpgBaseBuildableDefinition> Definition;
	TStrongObjectPtr<URpgPawnData> PawnData;
	TStrongObjectPtr<UClass> MaterialClass;
	TStrongObjectPtr<UClass> ControllerClass;
	TStrongObjectPtr<UClass> StationClass;
	TStrongObjectPtr<UClass> BlockerClass;
	TStrongObjectPtr<URpgCraftingRecipeDefinition> Recipe;
	TUniquePtr<FPIENetworkComponent<FState>> Network;
	FName ContainerId;
	FName OutputContainerId;
	FRpgInventoryItemId StoredItemId;
	FRpgPhysicalStorageRequest AssignRequest, DepositRequest, UpgradeRequest, RejectedRequest;
	int64 AssignedOrder = 0;
	int32 UpgradedRevision = 0;
	FRpgPhysicalStorageRequest Barriers[2], CompetingBuilds[2], BeginRelocateRequest, RelocateRequest;
	FGuid PaidJobId;
	FName ConstructedId;
	int32 WithdrawalWinner = INDEX_NONE;
	int32 BuildWinner = INDEX_NONE;
	int32 RecipeCost = 0;
	FVector RelocatedLocation = FVector(200, -75, 0);

	TEST_METHOD(RemoteCommandsConserveItemsAndReconstructOnLateJoin)
	{
		using namespace RpgPhysicalStoragePIETests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{ TestRunner->AddError(TEXT("Physical storage automation refuses to interrupt an existing PIE session.")); return; }
		MaterialClass.Reset(LoadClass<URpgInventoryItemDefinition>(nullptr, TEXT("/Script/SurvivalRpg.RpgInventoryAutomationTestMaterialDefinition")));
		ControllerClass.Reset(LoadClass<ARpgPlayerController>(nullptr, TEXT("/Script/SurvivalRpg.RpgInventoryAutomationTestPlayerController")));
		PawnData.Reset(LoadObject<URpgPawnData>(nullptr, TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData.DA_PawnData")));
		ASSERT_THAT(IsNotNull(MaterialClass.Get()));
		ASSERT_THAT(IsNotNull(ControllerClass.Get()));
		ASSERT_THAT(IsNotNull(PawnData.Get()));
		if (!MaterialClass || !ControllerClass || !PawnData) return;
		ModeScope.Start(ControllerClass.Get());
		Network = MakeUnique<FPIENetworkComponent<FState>>(TestRunner, TestCommandBuilder, bInitializing);
		FNetworkComponentBuilder<FState>().WithClients(2).AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass()).WithGameMode(AGameModeBase::StaticClass()).Build(*Network);
		Network->UntilServer(TEXT("Two independent remote owners connect to the dedicated authority"), [](FState& State)
		{
			return ActiveWorld(State.World) && State.World->GetNetMode() == NM_DedicatedServer &&
				State.World->GetAuthGameMode() && State.World->GetAuthGameMode()->GetClass() == AGameModeBase::StaticClass() &&
				ControllerReady(ServerController(State, 0)) && ControllerReady(ServerController(State, 1));
		}, Timeout())
		.ThenServer(TEXT("Seed isolated server fixtures through production inventory APIs"), [this](FState& State)
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				ARpgPlayerController* PC = ServerController(State, Index);
				PC->GetPlayerState<ARpgPlayerState>()->SetPawnData(PawnData.Get());
				PC->GetPawn()->SetActorLocation(FVector(0, Index * 100, 100));
			}
			ARpgBaseCampActor* Base = State.World->SpawnActor<ARpgBaseCampActor>();
			ASSERT_THAT(IsNotNull(Base)); if (!Base) return;
			ASSERT_THAT(IsTrue(Base->SetBaseArea(FVector(0, 0, 100), 2000)));
			ARpgInventoryContainerActor* Actor = State.World->SpawnActor<ARpgInventoryContainerActor>(FVector(100, 0, 100), FRotator::ZeroRotator);
			ASSERT_THAT(IsNotNull(Actor)); if (!Actor) return;
			Actor->bAlwaysRelevant = true;
			Actor->GetContainerComponent()->EnsurePersistentContainerId();
			ContainerId = Actor->GetContainerComponent()->GetPersistentContainerId();
			Definition.Reset(NewObject<URpgBaseBuildableDefinition>());
			Definition->ChestUpgradeTiers.SetNum(3);
			for (int32 Tier = 0; Tier < 3; ++Tier)
			{
				Definition->ChestUpgradeTiers[Tier].GridSize.Width = 6;
				Definition->ChestUpgradeTiers[Tier].GridSize.Height = 4 + Tier * 2;
				if (Tier > 0)
				{
					FRpgBaseBuildResourceCost Cost;
					Cost.ItemDefinition = MaterialClass.Get(); Cost.Count = Tier == 1 ? 5 : 10;
					Definition->ChestUpgradeTiers[Tier].Costs.Add(Cost);
				}
			}
			FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(URpgInventoryContainerComponent::StaticClass(), TEXT("BuildableDefinition"));
			ASSERT_THAT(IsNotNull(Property)); if (!Property) return;
			Property->SetObjectPropertyValue_InContainer(Actor->GetContainerComponent(), Definition.Get());
			ASSERT_THAT(IsTrue(Actor->GetInventoryManager()->SetDefaultGridSize(Definition->ChestUpgradeTiers[0].GridSize)));
			URpgInventoryItemInstance* Item = PlayerInventory(ServerController(State, 0))->GrantItemDefinition(MaterialClass.Get(), 9);
			ASSERT_THAT(IsNotNull(Item)); if (Item) StoredItemId = Item->GetItemId();
			ASSERT_THAT(IsNotNull(PlayerInventory(ServerController(State, 1))->GrantItemDefinition(MaterialClass.Get(), 10)));
		})
		.UntilClients(TEXT("Each remote receives its own stock and shared chest capacity"), [this](FState& State)
		{
			ARpgPlayerController* PC = LocalController(State.World);
			ARpgInventoryContainerActor* Actor = Chest(State.World, ContainerId);
			return ControllerReady(PC) && PC->IsLocalController() && !PC->HasAuthority() && Actor &&
				Actor->GetInventoryManager()->GetDefaultGridSize() == Definition->ChestUpgradeTiers[0].GridSize &&
				PlayerInventory(PC)->GetTotalItemCountByDefinition(MaterialClass.Get()) == (State.ClientIndex == 0 ? 9 : 10);
		}, Timeout())
		.ThenClient(TEXT("Remote owner submits an exact material assignment over its controller RPC"), 0, [this](FState& State)
		{
			AssignRequest = Request(State.World, ContainerId, ERpgPhysicalStorageCommand::SetAssignments);
			FRpgStorageAssignment Rule; Rule.ItemDefinition = MaterialClass.Get(); Rule.AssignmentOrder = 999999;
			AssignRequest.Assignments = { Rule };
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(AssignRequest);
		})
		.UntilServer(TEXT("Server accepts assignment and allocates its own saved order"), [this](FState& State)
		{
			const auto* Record = Completed(ServerController(State, 0), AssignRequest.RequestId);
			const auto* Actor = Chest(State.World, ContainerId);
			if (!Record || !Actor || Actor->GetContainerComponent()->GetAssignments().Num() != 1) return false;
			AssignedOrder = Actor->GetContainerComponent()->GetAssignments()[0].AssignmentOrder;
			return Record->bSucceeded && AssignedOrder > 0 && AssignedOrder != 999999;
		}, Timeout())
		.UntilClients(TEXT("Assignment and server-issued order reach both remote worlds"), [this](FState& State)
		{
			const auto* Actor = Chest(State.World, ContainerId);
			return Actor && Actor->GetContainerComponent()->GetAssignments().Num() == 1 &&
				Actor->GetContainerComponent()->GetAssignments()[0].AssignmentOrder == AssignedOrder;
		}, Timeout())
		.ThenClient(TEXT("Remote owner deposits through the existing production RPC"), 0, [this](FState& State)
		{
			DepositRequest = Request(State.World, ContainerId, ERpgPhysicalStorageCommand::DepositMaterials);
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(DepositRequest);
		})
		.UntilServer(TEXT("Deposit conserves stock and never consumes the second player inventory"), [this](FState& State)
		{
			const auto* Record = Completed(ServerController(State, 0), DepositRequest.RequestId);
			const auto* Actor = Chest(State.World, ContainerId);
			return Record && Record->bSucceeded && Actor && Actor->GetInventoryManager()->GetTotalItemCountByDefinition(MaterialClass.Get()) == 9 &&
				Actor->GetInventoryManager()->FindItemById(StoredItemId) &&
				PlayerInventory(ServerController(State, 0))->GetTotalItemCountByDefinition(MaterialClass.Get()) == 0 &&
				PlayerInventory(ServerController(State, 1))->GetTotalItemCountByDefinition(MaterialClass.Get()) == 10;
		}, Timeout())
		.ThenServer(TEXT("Add three acting-player units so upgrade must consume player first and chest second"), [this](FState& State)
		{
			ASSERT_THAT(IsNotNull(PlayerInventory(ServerController(State, 0))->GrantItemDefinition(MaterialClass.Get(), 3)));
		})
		.ThenClient(TEXT("Remote upgrades the shared chest with authoritative costs"), 0, [this](FState& State)
		{
			UpgradeRequest = Request(State.World, ContainerId, ERpgPhysicalStorageCommand::Upgrade);
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(UpgradeRequest);
		})
		.UntilServer(TEXT("Upgrade pays three from requester and two from chest, with no other-player debit"), [this](FState& State)
		{
			const auto* Record = Completed(ServerController(State, 0), UpgradeRequest.RequestId);
			const auto* Actor = Chest(State.World, ContainerId);
			if (!Record || !Record->bSucceeded || !Actor) return false;
			UpgradedRevision = Actor->GetContainerComponent()->GetSettingsRevision();
			return Actor->GetContainerComponent()->ExportPhysicalStorageMetadata().UpgradeTier == 1 &&
				Actor->GetInventoryManager()->GetDefaultGridSize() == Definition->ChestUpgradeTiers[1].GridSize &&
				Actor->GetInventoryManager()->GetTotalItemCountByDefinition(MaterialClass.Get()) == 7 &&
				PlayerInventory(ServerController(State, 0))->GetTotalItemCountByDefinition(MaterialClass.Get()) == 0 &&
				PlayerInventory(ServerController(State, 1))->GetTotalItemCountByDefinition(MaterialClass.Get()) == 10;
		}, Timeout())
		.UntilClients(TEXT("Both clients reconstruct upgraded capacity and the surviving item identity"), [this](FState& State)
		{
			return HasFinalChest(State.World) && PlayerInventory(LocalController(State.World))->GetTotalItemCountByDefinition(MaterialClass.Get()) == (State.ClientIndex == 0 ? 0 : 10);
		}, Timeout())
		.ThenClient(TEXT("Replay the accepted upgrade, then request a tier affordable only with someone else's stock"), 0, [this](FState& State)
		{
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(UpgradeRequest);
			RejectedRequest = Request(State.World, ContainerId, ERpgPhysicalStorageCommand::Upgrade);
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(RejectedRequest);
		})
		.UntilServer(TEXT("Second upgrade is explicitly rejected after receipt without debit or replay effects"), [this](FState& State)
		{
			const auto* Record = Completed(ServerController(State, 0), RejectedRequest.RequestId);
			return Record && !Record->bSucceeded && HasFinalChest(State.World) &&
				PlayerInventory(ServerController(State, 0))->GetTotalItemCountByDefinition(MaterialClass.Get()) == 0 &&
				PlayerInventory(ServerController(State, 1))->GetTotalItemCountByDefinition(MaterialClass.Get()) == 10;
		}, Timeout())
		.ThenClientJoins(Timeout())
		.UntilClient(TEXT("Late join receives already-upgraded grid, exact assignment order and existing item identity"), 2, [this](FState& State)
		{
			return ControllerReady(LocalController(State.World)) && State.World->GetNetMode() == NM_Client && HasFinalChest(State.World);
		}, Timeout())
		.ThenClients(TEXT("Every peer agrees on shared state without seeing another player's private stock"), [this](FState& State)
		{
			ASSERT_THAT(IsTrue(HasFinalChest(State.World)));
			ARpgPlayerController* Local = LocalController(State.World);
			for (TActorIterator<ARpgPlayerState> It(State.World); It; ++It)
			{
				if (*It != Local->PlayerState)
				{
					ASSERT_THAT(AreEqual(It->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(MaterialClass.Get()), 0));
				}
			}
		})
		.ThenServer(TEXT("Late join never charges again or duplicates chest stock"), [this](FState& State)
		{
			ASSERT_THAT(IsTrue(HasFinalChest(State.World)));
			ASSERT_THAT(AreEqual(State.World->GetNetDriver()->ClientConnections.Num(), 3));
			ASSERT_THAT(AreEqual(PlayerInventory(ServerController(State, 1))->GetTotalItemCountByDefinition(MaterialClass.Get()), 10));
		});
	}

	/** Real owner RPCs race for one order's payment and then one concrete stack while the paid unit waits for room. */
	TEST_METHOD(ConcurrentCraftWithdrawalAndCancellationConserveSharedStock)
	{
		using namespace RpgPhysicalStoragePIETests;
		if (!PrepareCompetition(true)) return;
		Network->ThenServer(TEXT("Seed one order payment and an empty one-cell target chest"), [this](FState& State)
		{
			auto* Crafting = Station(State.World);
			ASSERT_THAT(IsNotNull(Crafting)); if (!Crafting) return;
			auto* Output = State.World->SpawnActor<ARpgInventoryContainerActor>(Definition->BuildActorClass, FVector(200, 300, 0), FRotator::ZeroRotator);
			ASSERT_THAT(IsNotNull(Output)); if (!Output) return;
			Output->bAlwaysRelevant = true;
			Output->GetContainerComponent()->EnsurePersistentContainerId();
			OutputContainerId = Output->GetContainerComponent()->GetPersistentContainerId();
			FRpgInventoryGridSize OneCell; OneCell.Width = 1; OneCell.Height = 1;
			ASSERT_THAT(IsTrue(Output->GetInventoryManager()->SetDefaultGridSize(OneCell)));
			ASSERT_THAT(IsNotNull(Chest(State.World, ContainerId)->GetInventoryManager()->GrantItemDefinition(MaterialClass.Get(), RecipeCost)));
			for (int32 Index = 0; Index < 2; ++Index)
			{
				ASSERT_THAT(IsTrue(Crafting->CanActorAccess(ServerController(State, Index)->GetPawn())));
				ASSERT_THAT(IsTrue(Crafting->CanStartCraftingOrder(ServerController(State, Index)->GetPawn(), Recipe.Get(), 1, OutputContainerId)));
			}
			ASSERT_THAT(AreEqual(Crafting->GetAvailableResourceCount(MaterialClass.Get()), RecipeCost));
		})
		.UntilClients(TEXT("Both remote owners observe the same scarce stock and the empty target chest"), [this](FState& State)
		{
			const auto* Crafting = Station(State.World);
			const auto* Actor = Chest(State.World, ContainerId);
			return ControllerReady(LocalController(State.World)) && Actor && Crafting && Chest(State.World, OutputContainerId) &&
				Actor->GetInventoryManager()->GetTotalItemCountByDefinition(MaterialClass.Get()) == RecipeCost;
		}, Timeout())
		.ThenClients(TEXT("Both remote clients start the same order before the next server tick"), [this](FState& State)
		{
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestStartCraftingOrder(Station(State.World), Recipe.Get(), 1, OutputContainerId);
			SendBarrier(State);
		})
		.UntilServer(TEXT("Both reliable start requests have reached authority"), [this](FState& State) { return BarriersCompleted(State); }, Timeout())
		.ThenServer(TEXT("Exactly one paid order wins; the rejected start cannot spend or produce anything"), [this](FState& State)
		{
			const FRpgCraftingOrder& Order = Station(State.World)->GetCurrentOrder();
			ASSERT_THAT(IsTrue(Order.IsActive())); if (!Order.IsActive()) return;
			PaidJobId = Order.OrderId;
			ASSERT_THAT(AreEqual(Order.QuantityTotal, 1));
			ASSERT_THAT(AreEqual(Order.QuantityCompleted, 0));
			ASSERT_THAT(AreEqual(RefundCount(Station(State.World), MaterialClass.Get()), RecipeCost));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, MaterialClass.Get()), 0));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, Recipe->OutputItems[0].ItemDefinition), 0));
			// A separate delivery fills the target while the paid unit is produced.
			ASSERT_THAT(IsNotNull(Chest(State.World, OutputContainerId)->GetInventoryManager()->GrantItemDefinition(BlockerClass.Get(), 1)));
			// A separate delivery arrives while the original recipe owns its paid refund credit.
			auto* Item = Chest(State.World, ContainerId)->GetInventoryManager()->GrantItemDefinition(MaterialClass.Get(), 5);
			ASSERT_THAT(IsNotNull(Item)); if (Item) StoredItemId = Item->GetItemId();
			const auto Entries = Chest(State.World, ContainerId)->GetInventoryManager()->GetAllEntries();
			ASSERT_THAT(AreEqual(Entries.Num(), 1)); if (Entries.Num() != 1) return;
			FRpgInventoryQuickTransferRequest Probe;
			Probe.RequestId = FGuid::NewGuid(); Probe.ItemId = Entries[0].ItemId; Probe.ExpectedEntryId = Entries[0].EntryId;
			Probe.ExpectedSourcePlacement = Entries[0].Placement; Probe.ExpectedSourceQuantity = 5; Probe.StackCount = 5;
			for (int32 Index = 0; Index < 2; ++Index)
			{
				FRpgInventoryContainerHandle Target; FRpgInventoryGridPlacement Placement;
				auto* PC = ServerController(State, Index);
				ASSERT_THAT(IsTrue(PC->GetInventoryUiActionComponent()->FindQuickTransferDestination(
					Chest(State.World, ContainerId)->GetInventoryManager(), PlayerInventory(PC), Probe, Target, Placement)));
			}
		})
		.UntilClients(TEXT("Both clients capture the same new concrete stack before racing to withdraw it"), [this](FState& State)
		{
			const auto* Actor = Chest(State.World, ContainerId);
			if (!Actor) return false;
			const auto* Inventory = Actor->GetInventoryManager();
			return Inventory->FindItemById(StoredItemId) && Inventory->GetTotalItemCountByDefinition(MaterialClass.Get()) == 5;
		}, Timeout())
		.ThenClients(TEXT("Both owners send a quick-transfer RPC using the identical source identity and count"), [this](FState& State)
		{
			auto* Source = Chest(State.World, ContainerId)->GetInventoryManager();
			const auto Entries = Source->GetAllEntries();
			const auto* Entry = Entries.FindByPredicate([this](const FRpgInventoryEntryView& Row) { return Row.ItemId == StoredItemId; });
			ASSERT_THAT(IsNotNull(Entry)); if (!Entry) return;
			FRpgInventoryQuickTransferRequest Transfer;
			Transfer.RequestId = FGuid::NewGuid(); Transfer.ItemId = Entry->ItemId; Transfer.ExpectedEntryId = Entry->EntryId;
			Transfer.ExpectedSourcePlacement = Entry->Placement; Transfer.ExpectedSourceQuantity = Entry->StackCount; Transfer.StackCount = 5;
			auto* PC = LocalController(State.World);
			PC->GetInventoryUiActionComponent()->RequestQuickTransferItem(Source, PlayerInventory(PC), Transfer);
			SendBarrier(State);
		})
		.UntilServer(TEXT("Both withdrawal RPCs have reached authority"), [this](FState& State) { return BarriersCompleted(State); }, Timeout())
		.ThenServer(TEXT("One owner receives all five units with the original item ID; paid recipe credit is untouched"), [this](FState& State)
		{
			const int32 First = PlayerInventory(ServerController(State, 0))->GetTotalItemCountByDefinition(MaterialClass.Get());
			const int32 Second = PlayerInventory(ServerController(State, 1))->GetTotalItemCountByDefinition(MaterialClass.Get());
			ASSERT_THAT(IsTrue((First == 5 && Second == 0) || (First == 0 && Second == 5)));
			WithdrawalWinner = First == 5 ? 0 : 1;
			ASSERT_THAT(IsNotNull(PlayerInventory(ServerController(State, WithdrawalWinner))->FindItemById(StoredItemId)));
			ASSERT_THAT(IsNull(PlayerInventory(ServerController(State, 1 - WithdrawalWinner))->FindItemById(StoredItemId)));
			ASSERT_THAT(AreEqual(Chest(State.World, ContainerId)->GetInventoryManager()->GetTotalItemCountByDefinition(MaterialClass.Get()), 0));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, MaterialClass.Get()) + RefundCount(Station(State.World), MaterialClass.Get()), 5 + RecipeCost));
		})
		.UntilServer(TEXT("The paid unit waits for room without losing its refund credit"), [this](FState& State)
		{
			const FRpgCraftingOrder& Order = Station(State.World)->GetCurrentOrder();
			return Order.OrderId == PaidJobId && Order.bUnitPaid && Order.State == ERpgCraftingOrderState::WaitingForSpace;
		}, Timeout())
		.ThenServer(TEXT("Validate relocation independently before it races with cancellation"), [this](FState& State)
		{
			FText Reason;
			ASSERT_THAT(IsTrue(ServerController(State, 1)->GetInventoryUiActionComponent()->CanPlacePhysicalStorage(
				Definition.Get(), FTransform(RelocatedLocation), ContainerId, Reason)));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, Recipe->OutputItems[0].ItemDefinition), 0));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, MaterialClass.Get()) + RefundCount(Station(State.World), MaterialClass.Get()), 5 + RecipeCost));
		})
		.ThenClient(TEXT("The second owner begins relocation while still beside the original chest"), 1, [this](FState& State)
		{
			BeginRelocateRequest = Request(State.World, ContainerId, ERpgPhysicalStorageCommand::BeginRelocate);
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(BeginRelocateRequest);
		})
		.UntilServer(TEXT("Authority admits the owner-bound relocation session"), [this](FState& State)
		{
			return Completed(ServerController(State, 1), BeginRelocateRequest.RequestId) != nullptr;
		}, Timeout())
		.ThenServer(TEXT("The admitted builder walks beyond original chest interaction range while staying in its base"), [this](FState& State)
		{
			ASSERT_THAT(IsTrue(Completed(ServerController(State, 1), BeginRelocateRequest.RequestId)->bSucceeded));
			APawn* Pawn = ServerController(State, 1)->GetPawn();
			Pawn->SetActorLocation(FVector(-250, -350, 100));
			ASSERT_THAT(IsFalse(Chest(State.World, ContainerId)->GetContainerComponent()->CanActorAccess(Pawn)));
			FText Reason;
			ASSERT_THAT(IsTrue(ServerController(State, 1)->GetInventoryUiActionComponent()->CanPlacePhysicalStorage(
				Definition.Get(), FTransform(RelocatedLocation), ContainerId, Reason)));
		})
		.ThenClients(TEXT("One owner stops the paid order while the other relocates its original refund chest"), [this](FState& State)
		{
			auto* Actions = LocalController(State.World)->GetInventoryUiActionComponent();
			if (State.ClientIndex == 0) Actions->RequestStopCraftingOrder(Station(State.World), PaidJobId);
			else
			{
				RelocateRequest = Request(State.World, ContainerId, ERpgPhysicalStorageCommand::Relocate);
				RelocateRequest.Transform = FTransform(RelocatedLocation);
				RelocateRequest.RelocationSessionId = BeginRelocateRequest.RequestId;
				Actions->RequestPhysicalStorageCommand(RelocateRequest);
			}
			SendBarrier(State);
		})
		.UntilServer(TEXT("Cancellation and relocation have both completed on the server"), [this](FState& State)
		{
			return BarriersCompleted(State) && Completed(ServerController(State, 1), RelocateRequest.RequestId);
		}, Timeout())
		.ThenServer(TEXT("Refund follows stable chest identity and conserves every unit across the competing actions"), [this](FState& State)
		{
			ASSERT_THAT(IsTrue(Completed(ServerController(State, 1), RelocateRequest.RequestId)->bSucceeded));
			ASSERT_THAT(IsTrue(HasCanceledCraftSnapshot(State.World)));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, MaterialClass.Get()), 5 + RecipeCost));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, Recipe->OutputItems[0].ItemDefinition), 0));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, BlockerClass.Get()), 1));
			ASSERT_THAT(IsNotNull(PlayerInventory(ServerController(State, WithdrawalWinner))->FindItemById(StoredItemId)));
		})
		.UntilClients(TEXT("Both original peers converge on the canceled queue, moved chest and single withdrawal winner"), [this](FState& State)
		{
			return HasCanceledCraftSnapshot(State.World) &&
				PlayerInventory(LocalController(State.World))->GetTotalItemCountByDefinition(MaterialClass.Get()) == (State.ClientIndex == WithdrawalWinner ? 5 : 0);
		}, Timeout())
		.ThenClientJoins(Timeout())
		.UntilClient(TEXT("Late join reconstructs the conserved post-contention snapshot without restarting or refunding the job"), 2, [this](FState& State)
		{
			return ControllerReady(LocalController(State.World)) && HasCanceledCraftSnapshot(State.World);
		}, Timeout())
		.ThenClients(TEXT("Contended material remains private to the winning owner on every peer"), [this](FState& State)
		{
			auto* Local = LocalController(State.World);
			for (TActorIterator<ARpgPlayerState> It(State.World); It; ++It)
				if (*It != Local->PlayerState)
				{
					ASSERT_THAT(AreEqual(It->GetInventoryManagerComponent()->GetTotalItemCountByDefinition(MaterialClass.Get()), 0));
					ASSERT_THAT(IsNull(It->GetInventoryManagerComponent()->FindItemById(StoredItemId)));
				}
		})
		.ThenServer(TEXT("Late join has no economic effect after cancellation"), [this](FState& State)
		{
			ASSERT_THAT(IsTrue(HasCanceledCraftSnapshot(State.World)));
			ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, MaterialClass.Get()), 5 + RecipeCost));
			ASSERT_THAT(AreEqual(RefundCount(Station(State.World), MaterialClass.Get()), 0));
		});
	}

	TEST_METHOD(ConcurrentRemoteBuildsSpendOneSharedPaymentExactlyOnce)
	{
		using namespace RpgPhysicalStoragePIETests;
		if (!PrepareCompetition(false)) return;
		Network->ThenServer(TEXT("Provide exactly one complete construction payment shared by both builders"), [this](FState& State)
		{
			for (const auto& Cost : Definition->BuildCosts)
			{
				ASSERT_THAT(IsNotNull(Chest(State.World, ContainerId)->GetInventoryManager()->GrantItemDefinition(Cost.ItemDefinition, Cost.Count)));
				ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, Cost.ItemDefinition), Cost.Count));
			}
			for (int32 Index = 0; Index < 2; ++Index)
			{
				FText Reason;
				ASSERT_THAT(IsTrue(ServerController(State, Index)->GetInventoryUiActionComponent()->CanPlacePhysicalStorage(
					Definition.Get(), BuildTransform(Index), NAME_None, Reason)));
			}
		})
		.UntilClients(TEXT("Both clients see the exact same shared construction budget"), [this](FState& State)
		{
			const auto* Actor = Chest(State.World, ContainerId);
			if (!Actor || !ControllerReady(LocalController(State.World))) return false;
			for (const auto& Cost : Definition->BuildCosts)
				if (Actor->GetInventoryManager()->GetTotalItemCountByDefinition(Cost.ItemDefinition) != Cost.Count) return false;
			return true;
		}, Timeout())
		.ThenClients(TEXT("Both owners request different valid buildings in the same client step"), [this](FState& State)
		{
			auto& Build = CompetingBuilds[State.ClientIndex];
			Build.RequestId = FGuid::NewGuid(); Build.Command = ERpgPhysicalStorageCommand::Build;
			Build.BuildableDefinition = Definition.Get(); Build.Transform = BuildTransform(State.ClientIndex);
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(Build);
		})
		.UntilServer(TEXT("Authority has completed both competing reliable construction requests"), [this](FState& State)
		{
			return Completed(ServerController(State, 0), CompetingBuilds[0].RequestId) && Completed(ServerController(State, 1), CompetingBuilds[1].RequestId);
		}, Timeout())
		.ThenServer(TEXT("Exactly one build succeeds and all cost items are charged exactly once"), [this](FState& State)
		{
			const bool First = Completed(ServerController(State, 0), CompetingBuilds[0].RequestId)->bSucceeded;
			const bool Second = Completed(ServerController(State, 1), CompetingBuilds[1].RequestId)->bSucceeded;
			ASSERT_THAT(IsTrue(First != Second));
			BuildWinner = First ? 0 : 1;
			const auto Built = BuiltChests(State.World);
			ASSERT_THAT(AreEqual(Built.Num(), 1)); if (Built.Num() != 1) return;
			ConstructedId = Built[0]->GetContainerComponent()->GetPersistentContainerId();
			ASSERT_THAT(IsFalse(ConstructedId.IsNone()));
			ASSERT_THAT(IsTrue(ConstructedId != ContainerId));
			ASSERT_THAT(IsTrue(Built[0]->GetActorLocation().Equals(BuildTransform(BuildWinner).GetLocation(), 0.1f)));
			ASSERT_THAT(IsFalse(Built[0]->GetContainerComponent()->IsConstructionPending()));
			ASSERT_THAT(IsTrue(Built[0]->GetInventoryManager()->GetAllEntries().IsEmpty()));
			ASSERT_THAT(IsTrue(Built[0]->GetInventoryManager()->GetDefaultGridSize() == Definition->ChestUpgradeTiers[0].GridSize));
			for (const auto& Cost : Definition->BuildCosts)
			{
				ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, Cost.ItemDefinition), 0));
			}
		})
		.ThenClients(TEXT("Both owners replay their original accepted or rejected construction RPC"), [this](FState& State)
		{
			LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(CompetingBuilds[State.ClientIndex]);
			SendBarrier(State);
		})
		.UntilServer(TEXT("Both replays have reached authority"), [this](FState& State) { return BarriersCompleted(State); }, Timeout())
		.ThenServer(TEXT("Neither successful nor rejected replay creates another actor or changes the debit"), [this](FState& State)
		{
			ASSERT_THAT(AreEqual(BuiltChests(State.World).Num(), 1));
			ASSERT_THAT(IsNotNull(Chest(State.World, ConstructedId)));
			for (const auto& Cost : Definition->BuildCosts)
			{
				ASSERT_THAT(AreEqual(SharedAndPlayerCount(State, Cost.ItemDefinition), 0));
			}
		})
		.ThenClientJoins(Timeout())
		.UntilClients(TEXT("All peers including late join see exactly the single paid construction and empty shared stock"), [this](FState& State)
		{
			const auto Built = BuiltChests(State.World);
			const auto* Stock = Chest(State.World, ContainerId);
			const auto* Constructed = Chest(State.World, ConstructedId);
			if (!ControllerReady(LocalController(State.World)) || Built.Num() != 1 || !Stock || !Constructed) return false;
			if (!Constructed->GetActorLocation().Equals(BuildTransform(BuildWinner).GetLocation(), 0.1f) ||
				Constructed->GetInventoryManager()->GetDefaultGridSize() != Definition->ChestUpgradeTiers[0].GridSize) return false;
			for (const auto& Cost : Definition->BuildCosts)
				if (Stock->GetInventoryManager()->GetTotalItemCountByDefinition(Cost.ItemDefinition) != 0 ||
					PlayerInventory(LocalController(State.World))->GetTotalItemCountByDefinition(Cost.ItemDefinition) != 0) return false;
			return Constructed->GetInventoryManager()->GetAllEntries().IsEmpty();
		}, Timeout());
	}

	/** The two clients use content assets as network-addressable parameters without modifying asset defaults. */
	bool PrepareCompetition(bool bWithCrafting)
	{
		using namespace RpgPhysicalStoragePIETests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{ TestRunner->AddError(TEXT("Physical storage automation refuses to interrupt an existing PIE session.")); return false; }
		ControllerClass.Reset(LoadClass<ARpgPlayerController>(nullptr, TEXT("/Script/SurvivalRpg.RpgInventoryAutomationTestPlayerController")));
		PawnData.Reset(LoadObject<URpgPawnData>(nullptr, TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData.DA_PawnData")));
		Definition.Reset(LoadObject<URpgBaseBuildableDefinition>(nullptr, TEXT("/Game/SurvivalRpg/Storage/Physical/DA_Buildable_SharedChest.DA_Buildable_SharedChest")));
		if (!ControllerClass || !PawnData || !Definition || !Definition->BuildActorClass || Definition->BuildCosts.IsEmpty() || Definition->ChestUpgradeTiers.IsEmpty())
		{ TestRunner->AddError(TEXT("Competition fixture requires the authored controller layout and buildable physical chest.")); return false; }
		if (bWithCrafting)
		{
			StationClass.Reset(LoadClass<ARpgCraftingStationActor>(nullptr, TEXT("/Game/SurvivalRpg/Storage/Physical/BP_SharedWorkbench.BP_SharedWorkbench_C")));
			Recipe.Reset(LoadObject<URpgCraftingRecipeDefinition>(nullptr, TEXT("/Game/SurvivalRpg/Storage/Physical/DA_Recipe_Planks.DA_Recipe_Planks")));
			BlockerClass.Reset(LoadClass<URpgInventoryItemDefinition>(nullptr, TEXT("/Game/SurvivalRpg/Storage/Physical/Items/ID_Iron.ID_Iron_C")));
			if (!StationClass || !Recipe || !BlockerClass || Recipe->RequiredResources.Num() != 1 || Recipe->OutputItems.Num() != 1 ||
				!Recipe->RequiredResources[0].ItemDefinition || Recipe->RequiredResources[0].Count <= 0 ||
				!Recipe->OutputItems[0].ItemDefinition || Recipe->OutputItems[0].ItemDefinition == BlockerClass.Get() || Recipe->CraftTime <= 0)
			{ TestRunner->AddError(TEXT("Competition fixture requires a timed one-material recipe and a distinct one-cell output blocker.")); return false; }
			MaterialClass.Reset(Recipe->RequiredResources[0].ItemDefinition.Get());
			RecipeCost = Recipe->RequiredResources[0].Count;
		}
		ModeScope.Start(ControllerClass.Get());
		Network = MakeUnique<FPIENetworkComponent<FState>>(TestRunner, TestCommandBuilder, bInitializing);
		FNetworkComponentBuilder<FState>().WithClients(2).AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass()).WithGameMode(AGameModeBase::StaticClass()).Build(*Network);
		Network->UntilServer(TEXT("Two remote owners are ready for competing commands"), [](FState& State)
		{
			return ActiveWorld(State.World) && State.World->GetNetMode() == NM_DedicatedServer &&
				ControllerReady(ServerController(State, 0)) && ControllerReady(ServerController(State, 1));
		}, Timeout())
		.ThenServer(TEXT("Create an isolated base, valid floor and authored shared actors"), [this, bWithCrafting](FState& State)
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				auto* PC = ServerController(State, Index);
				PC->GetPlayerState<ARpgPlayerState>()->SetPawnData(PawnData.Get());
				PC->GetPawn()->SetActorLocation(FVector(-80, Index == 0 ? -50 : 50, 100));
			}
			auto* Base = State.World->SpawnActor<ARpgBaseCampActor>();
			ASSERT_THAT(IsNotNull(Base)); if (!Base) return;
			ASSERT_THAT(IsTrue(Base->SetBaseArea(FVector::ZeroVector, 2000)));
			auto* Floor = State.World->SpawnActor<AActor>();
			ASSERT_THAT(IsNotNull(Floor)); if (!Floor) return;
			auto* Box = NewObject<UBoxComponent>(Floor, NAME_None, RF_Transient);
			Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(10000, 10000, 10));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
			Floor->SetActorLocation(FVector(0, 0, -10));
			auto* Actor = State.World->SpawnActor<ARpgInventoryContainerActor>(Definition->BuildActorClass, FVector(200, 0, 0), FRotator::ZeroRotator);
			ASSERT_THAT(IsNotNull(Actor)); if (!Actor) return;
			Actor->bAlwaysRelevant = true;
			Actor->GetContainerComponent()->EnsurePersistentContainerId();
			ContainerId = Actor->GetContainerComponent()->GetPersistentContainerId();
			ASSERT_THAT(IsTrue(Actor->GetInventoryManager()->GetAllEntries().IsEmpty()));
			if (bWithCrafting)
			{
				auto* Workbench = State.World->SpawnActor<ARpgCraftingStationActor>(StationClass.Get(), FVector(0, 200, 0), FRotator::ZeroRotator);
				ASSERT_THAT(IsNotNull(Workbench)); if (!Workbench) return;
				Workbench->bAlwaysRelevant = true;
			}
		});
		return true;
	}

	/** Same-component reliable ordering proves earlier RPC receipt. A fresh unrelated cancel token has no gameplay effect. */
	void SendBarrier(FState& State)
	{
		using namespace RpgPhysicalStoragePIETests;
		auto& Barrier = Barriers[State.ClientIndex];
		// Cancel is admitted even outside interaction range, unlike an assignment command after relocation.
		Barrier = Request(State.World, ContainerId, ERpgPhysicalStorageCommand::CancelRelocate);
		Barrier.RelocationSessionId = FGuid::NewGuid();
		LocalController(State.World)->GetInventoryUiActionComponent()->RequestPhysicalStorageCommand(Barrier);
	}

	bool BarriersCompleted(FState& State) const
	{
		using namespace RpgPhysicalStoragePIETests;
		return Completed(ServerController(State, 0), Barriers[0].RequestId) && Completed(ServerController(State, 1), Barriers[1].RequestId);
	}

	int32 SharedAndPlayerCount(FState& State, UClass* ItemClass) const
	{
		// Count every physical inventory, including any unexpected spawned output/drop, rather than just the intended destination.
		int32 Result = 0;
		for (TActorIterator<AActor> It(State.World); It; ++It)
		{
			if (It->IsActorBeingDestroyed()) continue;
			TArray<URpgInventoryManagerComponent*> Inventories;
			It->GetComponents(Inventories);
			for (const auto* Inventory : Inventories) Result += Inventory->GetTotalItemCountByDefinition(ItemClass);
		}
		return Result;
	}

	bool HasCanceledCraftSnapshot(UWorld* World) const
	{
		using namespace RpgPhysicalStoragePIETests;
		const auto* Actor = Chest(World, ContainerId);
		const auto* Crafting = Station(World);
		const auto* Output = Chest(World, OutputContainerId);
		return Actor && Crafting && Output && Actor->GetActorLocation().Equals(RelocatedLocation, 0.1f) &&
			Actor->GetInventoryManager()->GetTotalItemCountByDefinition(MaterialClass.Get()) == RecipeCost && !Crafting->HasCraftingOrder() &&
			Output->GetInventoryManager()->GetTotalItemCountByDefinition(BlockerClass.Get()) == 1 &&
			Output->GetInventoryManager()->GetTotalItemCountByDefinition(Recipe->OutputItems[0].ItemDefinition) == 0;
	}

	FTransform BuildTransform(int32 ClientIndex) const { return FTransform(FVector(400, ClientIndex == 0 ? -250 : 250, 0)); }

	bool HasFinalChest(UWorld* World) const
	{
		const auto* Actor = RpgPhysicalStoragePIETests::Chest(World, ContainerId);
		if (!Actor) return false;
		const auto* Container = Actor->GetContainerComponent();
		const auto* Inventory = Actor->GetInventoryManager();
		const auto Metadata = Container->ExportPhysicalStorageMetadata();
		return Metadata.PersistentContainerId == ContainerId && Metadata.UpgradeTier == 1 &&
			Metadata.SettingsRevision == UpgradedRevision && Metadata.GridSize == Definition->ChestUpgradeTiers[1].GridSize &&
			Metadata.Assignments.Num() == 1 && Metadata.Assignments[0].ItemDefinition == MaterialClass.Get() &&
			Metadata.Assignments[0].AssignmentOrder == AssignedOrder && Metadata.AssignmentOrderHighWaterMark >= AssignedOrder &&
			Inventory->GetTotalItemCountByDefinition(MaterialClass.Get()) == 7 && Inventory->FindItemById(StoredItemId);
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
