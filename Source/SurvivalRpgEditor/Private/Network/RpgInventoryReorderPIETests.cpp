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
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "UObject/StrongObjectPtr.h"

#if ENABLE_PIE_NETWORK_TEST

/**
 * Regression for a server crash in FastArray replication of the inventory list.
 *
 * The inventory re-sorts its replicated entries by placement after each move. With delta struct serialization, the
 * engine's shadow fix-up cannot follow a reorder that rotates three or more entries and fails the check
 * FixedShadowIndex > Index in FRepLayout::DeltaSerializeFastArrayProperty. A remote owner hit it when equipping a
 * second item. This test produces the smallest such rotation with two ordinary owner moves.
 */
namespace RpgInventoryReorderPIETests
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

	URpgInventoryManagerComponent* PlayerInventory(const ARpgPlayerController* Controller)
	{
		const ARpgPlayerState* State = Controller ? Controller->GetPlayerState<ARpgPlayerState>() : nullptr;
		return State ? State->GetInventoryManagerComponent() : nullptr;
	}

	bool ControllerReady(ARpgPlayerController* Controller)
	{
		return IsValid(Controller) && IsValid(Controller->GetPawn()) && PlayerInventory(Controller) &&
			Controller->GetInventoryUiActionComponent() && Controller->GetPlayerInventoryLayoutComponent();
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

	bool IsAt(UWorld* World, const FRpgInventoryItemId& ItemId, const FRpgInventoryGridPlacement& Placement)
	{
		FRpgInventoryEntryView Entry;
		return FindEntry(PlayerInventory(LocalController(World)), ItemId, Entry)
			&& Entry.Placement.GetContainerHandle() == Placement.GetContainerHandle()
			&& Entry.Placement.X == Placement.X && Entry.Placement.Y == Placement.Y;
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

NETWORK_TEST_CLASS(InventoryReorderPIE, "SurvivalRpg.Network")
{
	using FState = RpgInventoryReorderPIETests::FState;
	RpgInventoryReorderPIETests::FScopedMode ModeScope;
	TStrongObjectPtr<URpgPawnData> PawnData;
	TStrongObjectPtr<UClass> UnitClass;
	TStrongObjectPtr<UClass> ControllerClass;
	TUniquePtr<FPIENetworkComponent<FState>> Network;
	FRpgInventoryItemId FirstItem;
	FRpgInventoryGridPlacement FirstPlacement;
	FRpgInventoryGridPlacement ParkingPlacement;

	TEST_METHOD(OwnerMovesThatRotateEntriesReplicate)
	{
		using namespace RpgInventoryReorderPIETests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{ TestRunner->AddError(TEXT("Inventory reorder automation refuses to interrupt an existing PIE session.")); return; }
		UnitClass.Reset(LoadClass<URpgInventoryItemDefinition>(nullptr, TEXT("/Script/SurvivalRpg.RpgInventoryAutomationTestUnitItemDefinition")));
		ControllerClass.Reset(LoadClass<ARpgPlayerController>(nullptr, TEXT("/Script/SurvivalRpg.RpgInventoryAutomationTestPlayerController")));
		PawnData.Reset(LoadObject<URpgPawnData>(nullptr, TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData.DA_PawnData")));
		ASSERT_THAT(IsNotNull(UnitClass.Get()));
		ASSERT_THAT(IsNotNull(ControllerClass.Get()));
		ASSERT_THAT(IsNotNull(PawnData.Get()));
		if (!UnitClass || !ControllerClass || !PawnData) return;
		ModeScope.Start(ControllerClass.Get());
		Network = MakeUnique<FPIENetworkComponent<FState>>(TestRunner, TestCommandBuilder, bInitializing);
		FNetworkComponentBuilder<FState>().WithClients(1).AsDedicatedServer()
			.WithGameInstanceClass(UGameInstance::StaticClass()).WithGameMode(AGameModeBase::StaticClass()).Build(*Network);
		Network->UntilServer(TEXT("The remote owner connects to the dedicated authority"), [](FState& State)
		{
			return ActiveWorld(State.World) && State.World->GetNetMode() == NM_DedicatedServer && ControllerReady(ServerController(State, 0));
		}, Timeout())
		.ThenServer(TEXT("Grant three 1x1 items through the production inventory API"), [this](FState& State)
		{
			ARpgPlayerController* PC = ServerController(State, 0);
			PC->GetPlayerState<ARpgPlayerState>()->SetPawnData(PawnData.Get());
			URpgInventoryManagerComponent* Inventory = PlayerInventory(PC);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				ASSERT_THAT(IsNotNull(Inventory->GrantItemDefinition(UnitClass.Get())));
			}
			// The first entry in placement order is the one the moves rotate around.
			const TArray<FRpgInventoryEntryView> Entries = Inventory->GetAllEntries();
			ASSERT_THAT(AreEqual(3, Entries.Num()));
			if (Entries.Num() != 3) return;
			FirstItem = Entries[0].ItemId;
			FirstPlacement = Entries[0].Placement;
			ParkingPlacement = FirstPlacement;
			ParkingPlacement.X = FirstPlacement.X + 4;
		})
		.UntilClient(TEXT("The owner's replica receives all three items"), 0, [](FState& State)
		{
			const URpgInventoryManagerComponent* Inventory = PlayerInventory(LocalController(State.World));
			return Inventory && Inventory->GetAllEntries().Num() == 3;
		}, Timeout())
		.ThenClient(TEXT("Move the first item behind the others: order B, C, A"), 0, [this](FState& State)
			{ RequestMove(State.World, FirstItem, ParkingPlacement); })
		.UntilClient(TEXT("The owner sees the parked item"), 0, [this](FState& State)
			{ return IsAt(State.World, FirstItem, ParkingPlacement); }, Timeout())
		.ThenClient(TEXT("Move it back to the front: order A, B, C rotates three replicated entries"), 0, [this](FState& State)
			{ RequestMove(State.World, FirstItem, FirstPlacement); })
		.UntilClient(TEXT("The owner sees the item back in front after the rotating reorder replicated"), 0, [this](FState& State)
		{
			const URpgInventoryManagerComponent* Inventory = PlayerInventory(LocalController(State.World));
			return IsAt(State.World, FirstItem, FirstPlacement) && Inventory && Inventory->GetAllEntries().Num() == 3;
		}, Timeout());
	}
};

#endif // ENABLE_PIE_NETWORK_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
