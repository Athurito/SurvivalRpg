#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgLootHarvestNetworkTestTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Runes/RpgRuneAutomationTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if ENABLE_PIE_NETWORK_TEST

/**
 * A remote owner sockets runes into a tool through the inventory UI's request path. The dedicated server enforces one
 * rune of each kind per tool, the rank it sets replicates with the rune, and the other player never receives the
 * owner's runes.
 */
namespace RpgRuneSocketPIETests
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

	ARpgPlayerController* ServerController(FState& State, int32 Index)
	{
		return ActiveWorld(State.World) && State.ClientConnections.IsValidIndex(Index) && IsValid(State.ClientConnections[Index])
			? Cast<ARpgPlayerController>(State.ClientConnections[Index]->PlayerController) : nullptr;
	}

	URpgInventoryManagerComponent* PlayerInventory(const AController* Controller)
	{
		const ARpgPlayerState* State = Controller ? Controller->GetPlayerState<ARpgPlayerState>() : nullptr;
		return State ? State->GetInventoryManagerComponent() : nullptr;
	}

	bool ControllerReady(ARpgPlayerController* Controller)
	{
		return IsValid(Controller) && IsValid(Controller->GetPawn()) && PlayerInventory(Controller) &&
			Controller->GetInventoryUiActionComponent() && Controller->GetPlayerInventoryLayoutComponent();
	}

	/** The server's inventory of the player with PlayerId, or null. */
	URpgInventoryManagerComponent* ServerInventoryOf(FState& State, const int32 PlayerId)
	{
		for (int32 Index = 0; Index < State.ClientConnections.Num(); ++Index)
		{
			const ARpgPlayerController* Controller = ServerController(State, Index);
			const APlayerState* PlayerState = Controller ? Controller->PlayerState : nullptr;
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId)
			{
				return PlayerInventory(Controller);
			}
		}
		return nullptr;
	}

	/** This machine's replica of the inventory of the player with PlayerId, or null. */
	URpgInventoryManagerComponent* ReplicaInventoryOf(UWorld* World, const int32 PlayerId)
	{
		const AGameStateBase* GameState = ActiveWorld(World) ? World->GetGameState() : nullptr;
		for (APlayerState* PlayerState : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			const ARpgPlayerState* RpgPlayerState = Cast<ARpgPlayerState>(PlayerState);
			if (RpgPlayerState && RpgPlayerState->GetPlayerId() == PlayerId)
			{
				return RpgPlayerState->GetInventoryManagerComponent();
			}
		}
		return nullptr;
	}

	bool FindEntry(const URpgInventoryManagerComponent* Inventory, const FRpgInventoryItemId& ItemId, FRpgInventoryEntryView& OutEntry)
	{
		if (!Inventory) return false;
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

	/** Socket X of the tool ToolId as this inventory's replica sees it. */
	FRpgInventoryGridPlacement Socket(const URpgInventoryManagerComponent* Inventory, const FRpgInventoryItemId& ToolId, int32 X)
	{
		FRpgInventoryEntryView ToolEntry;
		FRpgInventoryGridPlacement Placement;
		if (FindEntry(Inventory, ToolId, ToolEntry))
		{
			Placement.SetContainerHandle(FRpgInventoryContainerHandle::MakeItemOwned(
				ToolId, RpgRuneAutomationTestSocketGridId, ToolEntry.Placement.GetContainerHandle().GetDirectChildDepth()));
			Placement.X = X;
			Placement.Y = 0;
		}
		return Placement;
	}

	TArray<FRpgInventoryItemId> SocketedIds(const URpgInventoryManagerComponent* Inventory, const FRpgInventoryItemId& ToolId)
	{
		TArray<FRpgInventoryItemId> Ids;
		if (Inventory)
		{
			for (const URpgInventoryItemInstance* Item : Inventory->GetSocketedItems(Inventory->FindItemById(ToolId)))
			{
				Ids.Add(Item->GetItemId());
			}
		}
		return Ids;
	}

	/** Sends one whole-entry move from the owning client exactly as the inventory UI does. */
	void RequestMove(UWorld* World, const FRpgInventoryItemId& ItemId, const FRpgInventoryGridPlacement& Target)
	{
		ARpgPlayerController* Controller = LocalController(World);
		URpgInventoryManagerComponent* Inventory = PlayerInventory(Controller);
		FRpgInventoryEntryView Entry;
		if (!Controller || !FindEntry(Inventory, ItemId, Entry)) return;
		FRpgInventoryMoveIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = Entry.ItemId;
		Intent.ExpectedEntryId = Entry.EntryId;
		Intent.ExpectedSourcePlacement = Entry.Placement;
		Intent.ExpectedQuantity = Entry.StackCount;
		Intent.TargetPlacement = Target;
		Controller->GetInventoryUiActionComponent()->RequestMoveInventoryItem(Inventory, Intent);
	}
}

NETWORK_TEST_CLASS(RuneSocketPIE, "SurvivalRpg.Network")
{
	using FState = RpgRuneSocketPIETests::FState;
	RpgRuneSocketPIETests::FScopedMode ModeScope;
	TStrongObjectPtr<URpgPawnData> PawnData;
	TStrongObjectPtr<UClass> ControllerClass;
	TUniquePtr<FPIENetworkComponent<FState>> Network;
	int32 OwnerPlayerId = INDEX_NONE;
	FRpgInventoryItemId ToolId;
	FRpgInventoryItemId RuneAId;
	FRpgInventoryItemId SecondRuneAId;
	FRpgInventoryItemId RuneBId;

	TEST_METHOD(OwnerSocketsOneRunePerKindAndTheRankReplicates)
	{
		using namespace RpgRuneSocketPIETests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{ TestRunner->AddError(TEXT("Rune socket automation refuses to interrupt an existing PIE session.")); return; }
		ConfigureRuneAutomationTestDefinitions();
		ControllerClass.Reset(LoadClass<ARpgPlayerController>(nullptr, TEXT("/Script/SurvivalRpg.RpgInventoryAutomationTestPlayerController")));
		PawnData.Reset(LoadObject<URpgPawnData>(nullptr, TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData.DA_PawnData")));
		ASSERT_THAT(IsNotNull(ControllerClass.Get()));
		ASSERT_THAT(IsNotNull(PawnData.Get()));
		if (!ControllerClass || !PawnData) return;
		ModeScope.Start(ControllerClass.Get());
		Network = MakeUnique<FPIENetworkComponent<FState>>(TestRunner, TestCommandBuilder, bInitializing);
		FNetworkComponentBuilder<FState>().WithClients(2).AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass()).WithGameMode(AGameModeBase::StaticClass()).Build(*Network);
		Network->UntilServer(TEXT("Both remote players connect to the dedicated authority"), [](FState& State)
		{
			return ActiveWorld(State.World) && State.World->GetNetMode() == NM_DedicatedServer &&
				ControllerReady(ServerController(State, 0)) && ControllerReady(ServerController(State, 1));
		}, Timeout())
		.ThenServer(TEXT("Every player gets a tool, two runes A and a rune B"), [this](FState& State)
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				ARpgPlayerController* PC = ServerController(State, Index);
				PC->GetPlayerState<ARpgPlayerState>()->SetPawnData(PawnData.Get());
				URpgInventoryManagerComponent* Inventory = PlayerInventory(PC);
				ASSERT_THAT(IsNotNull(Inventory->GrantItemDefinition(URpgRuneAutomationTestToolDefinition::StaticClass())));
				ASSERT_THAT(IsNotNull(Inventory->GrantItemDefinition(URpgRuneAutomationTestRuneDefinition::StaticClass())));
				ASSERT_THAT(IsNotNull(Inventory->GrantItemDefinition(URpgRuneAutomationTestRuneDefinition::StaticClass())));
				ASSERT_THAT(IsNotNull(Inventory->GrantItemDefinition(URpgRuneAutomationTestOtherRuneDefinition::StaticClass())));
			}
		})
		.UntilClient(TEXT("The owner's replica receives its four items"), 0, [](FState& State)
		{
			const URpgInventoryManagerComponent* Inventory = PlayerInventory(LocalController(State.World));
			return Inventory && Inventory->GetAllEntries().Num() == 4;
		}, Timeout())
		.ThenClient(TEXT("The owner requests rune A into the first socket"), 0, [this](FState& State)
		{
			ARpgPlayerController* Controller = LocalController(State.World);
			const URpgInventoryManagerComponent* Inventory = PlayerInventory(Controller);
			ASSERT_THAT(IsNotNull(Inventory));
			OwnerPlayerId = Controller->PlayerState->GetPlayerId();
			for (const URpgInventoryItemInstance* Item : Inventory->GetAllItems())
			{
				const UClass* Definition = Item->GetItemDef().Get();
				if (Definition == URpgRuneAutomationTestToolDefinition::StaticClass()) ToolId = Item->GetItemId();
				else if (Definition == URpgRuneAutomationTestOtherRuneDefinition::StaticClass()) RuneBId = Item->GetItemId();
				else if (!RuneAId.IsValid()) RuneAId = Item->GetItemId();
				else SecondRuneAId = Item->GetItemId();
			}
			ASSERT_THAT(IsTrue(ToolId.IsValid() && RuneAId.IsValid() && SecondRuneAId.IsValid() && RuneBId.IsValid()));
			RequestMove(State.World, RuneAId, Socket(Inventory, ToolId, 0));
		})
		.UntilClient(TEXT("The owner sees rune A in the tool"), 0, [this](FState& State)
		{
			return SocketedIds(PlayerInventory(LocalController(State.World)), ToolId) == TArray<FRpgInventoryItemId>{RuneAId};
		}, Timeout())
		.ThenClient(TEXT("The owner requests the second rune A, then rune B, into the second socket"), 0, [this](FState& State)
		{
			const URpgInventoryManagerComponent* Inventory = PlayerInventory(LocalController(State.World));
			RequestMove(State.World, SecondRuneAId, Socket(Inventory, ToolId, 1));
			RequestMove(State.World, RuneBId, Socket(Inventory, ToolId, 1));
		})
		.UntilServer(TEXT("The server refused the second rune A and socketed rune B"), [this](FState& State)
		{
			return SocketedIds(ServerInventoryOf(State, OwnerPlayerId), ToolId) == TArray<FRpgInventoryItemId>{RuneAId, RuneBId};
		}, Timeout())
		.ThenServer(TEXT("The server raises the owner's rune A to rank III"), [this](FState& State)
		{
			URpgInventoryManagerComponent* Inventory = ServerInventoryOf(State, OwnerPlayerId);
			URpgInventoryItemInstance* RuneA = Inventory ? Inventory->FindItemById(RuneAId) : nullptr;
			ASSERT_THAT(IsNotNull(RuneA));
			ASSERT_THAT(IsTrue(RuneA->SetRuneRank(3)));
		})
		.UntilClient(TEXT("The owner receives rank III on the socketed rune and both runes in the tool"), 0, [this](FState& State)
		{
			const URpgInventoryManagerComponent* Inventory = PlayerInventory(LocalController(State.World));
			const URpgInventoryItemInstance* RuneA = Inventory ? Inventory->FindItemById(RuneAId) : nullptr;
			return RuneA && RuneA->GetRuneRank() == 3 &&
				SocketedIds(Inventory, ToolId) == TArray<FRpgInventoryItemId>{RuneAId, RuneBId};
		}, Timeout())
		.ThenClient(TEXT("The other player never receives the owner's tool or runes"), 1, [this](FState& State)
		{
			const URpgInventoryManagerComponent* OwnerReplica = ReplicaInventoryOf(State.World, OwnerPlayerId);
			ASSERT_THAT(IsTrue(!OwnerReplica || OwnerReplica->GetAllEntries().IsEmpty()));
			const URpgInventoryManagerComponent* OwnInventory = PlayerInventory(LocalController(State.World));
			ASSERT_THAT(IsTrue(OwnInventory && OwnInventory->GetSocketedItems(OwnInventory->FindItemById(ToolId)).IsEmpty()));
		});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
