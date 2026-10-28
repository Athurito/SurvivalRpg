#include "RpgRuneAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryAddressSlotViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventorySlotGroupViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModel.h"

namespace RpgRuneSocketLayoutTests
{
	class FScopedPlayerWorld
	{
	public:
		FScopedPlayerWorld()
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

		~FScopedPlayerWorld()
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

	private:
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
	};

	FRpgInventoryGridPlacement MakePlacement(const FRpgInventoryContainerHandle& Handle, int32 X, int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(Handle);
		Placement.X = X;
		Placement.Y = Y;
		Placement.Width = 1;
		Placement.Height = 1;
		return Placement;
	}

	FRpgInventoryMoveIntent MakeMoveIntent(
		const URpgInventoryManagerComponent* Inventory,
		const URpgInventoryItemInstance* Item,
		const FRpgInventoryGridPlacement& Target)
	{
		FRpgInventoryMoveIntent Intent;
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Item && Entry.ItemId == Item->GetItemId())
			{
				Intent.EnsureRequestId();
				Intent.ItemId = Entry.ItemId;
				Intent.ExpectedEntryId = Entry.EntryId;
				Intent.ExpectedSourcePlacement = Entry.Placement;
				Intent.ExpectedQuantity = Entry.StackCount;
				Intent.TargetPlacement = Target;
			}
		}
		return Intent;
	}

	const FRpgInventorySlotGroupView* FindSocketGroup(const TArray<FRpgInventorySlotGroupView>& Groups)
	{
		return Groups.FindByPredicate([](const FRpgInventorySlotGroupView& Group)
		{
			return Group.GroupKind == ERpgInventorySlotGroupKind::Socket;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgRuneSocketCarrySlotLayoutTest,
	"SurvivalRpg.Runes.Sockets.CarrySlotShowsSockets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgRuneSocketCarrySlotLayoutTest::RunTest(const FString& Parameters)
{
	using namespace RpgRuneSocketLayoutTests;
	ConfigureRuneAutomationTestDefinitions();

	FScopedPlayerWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("The test world exists"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(SpawnParameters);
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(SpawnParameters);
	if (!TestNotNull(TEXT("The controller exists"), Controller) ||
		!TestNotNull(TEXT("The player state exists"), PlayerState))
	{
		return false;
	}
	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	URpgPlayerInventoryLayoutComponent* Layout = Controller->GetPlayerInventoryLayoutComponent();
	if (!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The inventory layout exists"), Layout))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	const FRpgInventoryContainerHandle CarryPrimary =
		FRpgInventoryContainerHandle::MakeRoot(URpgPlayerInventoryLayoutComponent::WeaponSlot1GroupId);
	URpgInventoryItemInstance* Weapon = Inventory->AddItemDefinitionToPlacement(
		URpgRuneAutomationTestSocketedWeaponDefinition::StaticClass(), 1, MakePlacement(Pockets, 0, 0));
	URpgInventoryItemInstance* Rune = Inventory->AddItemDefinitionToPlacement(
		URpgRuneAutomationTestRuneDefinition::StaticClass(), 1, MakePlacement(Pockets, 1, 0));
	URpgInventoryItemInstance* Unit = Inventory->AddItemDefinitionToPlacement(
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(), 1, MakePlacement(Pockets, 2, 0));
	if (!TestNotNull(TEXT("The socketed weapon starts in the pockets"), Weapon) ||
		!TestNotNull(TEXT("A rune starts in the pockets"), Rune) ||
		!TestNotNull(TEXT("An item that is no rune starts in the pockets"), Unit))
	{
		return false;
	}

	TestNull(TEXT("A socketed item in the pockets shows no socket grid"), FindSocketGroup(Layout->GetSlotGroups()));

	if (!TestTrue(
			TEXT("The weapon moves into the primary carry slot"),
			Inventory->MoveEquipmentItem(MakeMoveIntent(Inventory, Weapon, MakePlacement(CarryPrimary, 0, 0))).IsSuccess()))
	{
		return false;
	}

	const FRpgInventoryContainerHandle Sockets = FRpgInventoryContainerHandle::MakeItemOwned(
		Weapon->GetItemId(), RpgRuneAutomationTestSocketGridId, CarryPrimary.GetDirectChildDepth());
	{
		const TArray<FRpgInventorySlotGroupView> Groups = Layout->GetSlotGroups();
		const FRpgInventorySlotGroupView* SocketGroup = FindSocketGroup(Groups);
		if (!TestNotNull(TEXT("The carried weapon shows its socket grid"), SocketGroup))
		{
			return false;
		}
		TestEqual(TEXT("The socket group is the weapon's item-owned grid"), SocketGroup->ContainerHandle, Sockets);
		TestTrue(
			TEXT("The socket group names the carry slot that provides it"),
			SocketGroup->SourceCarryRole == RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);
		TestFalse(TEXT("The socket group has no semantic role of its own"), SocketGroup->SemanticRole.IsValid());
		TestFalse(TEXT("The socket group is not equipment-provided storage"), SocketGroup->bProvidedByEquipment);
		TestEqual(
			TEXT("Exactly one socket group exists"),
			Groups.FilterByPredicate([](const FRpgInventorySlotGroupView& Group)
			{
				return Group.GroupKind == ERpgInventorySlotGroupKind::Socket;
			}).Num(),
			1);
		TestFalse(
			TEXT("No content group exposes the socket grid"),
			Groups.ContainsByPredicate([&Sockets](const FRpgInventorySlotGroupView& Group)
			{
				return Group.ContainerHandle == Sockets && Group.GroupKind == ERpgInventorySlotGroupKind::Content;
			}));

		FRpgInventorySlotGroupView CarryGroup;
		TestTrue(
			TEXT("The primary carry role stays unique"),
			Layout->TryGetSlotGroupBySemanticRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary, CarryGroup) &&
				CarryGroup.ContainerHandle == CarryPrimary);
	}

	FRpgInventorySlotAddress SocketAddress;
	SocketAddress.SetContainerHandle(Sockets);
	SocketAddress.X = 0;
	SocketAddress.Y = 0;
	TestTrue(TEXT("A rune may target a socket"), Layout->CanItemUseSlotAddress(Rune, SocketAddress));
	TestFalse(TEXT("An item that is no rune may not target a socket"), Layout->CanItemUseSlotAddress(Unit, SocketAddress));
	TestFalse(TEXT("A socket is no content address"), Layout->IsContentSlotAddress(SocketAddress));

	URpgPlayerInventoryViewModel* ViewModel = NewObject<URpgPlayerInventoryViewModel>(Controller);
	ViewModel->BindPlayerController(Controller);
	URpgInventorySlotGroupViewModel* SocketViewModel =
		ViewModel->GetSocketGroupForCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);
	if (!TestNotNull(TEXT("The view model projects the primary carry slot's sockets"), SocketViewModel))
	{
		ViewModel->UnbindPlayerInventory();
		return false;
	}
	TestTrue(TEXT("The projection is a socket group"), SocketViewModel->IsSocketGroup());
	TestFalse(TEXT("The projection is no content group"), SocketViewModel->IsContentGroup());
	TestEqual(TEXT("The projection has three sockets"), SocketViewModel->GetSlots().Num(), 3);
	TestFalse(
		TEXT("The inventory groups do not list the sockets"),
		ViewModel->GetInventoryGroups().Contains(SocketViewModel));
	TestEqual(TEXT("The socket group resolves by its handle"), ViewModel->GetSlotGroupByHandle(Sockets), SocketViewModel);
	TestNull(
		TEXT("The empty secondary carry slot has no sockets"),
		ViewModel->GetSocketGroupForCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Secondary));

	// Fill the six free pocket cells: a further grant must fail instead of using a socket.
	for (int32 Index = 0; Index < 6; ++Index)
	{
		TestNotNull(
			TEXT("A filler rune lands in the pockets"),
			Inventory->GrantItemDefinition(URpgRuneAutomationTestRuneDefinition::StaticClass()));
	}
	TestNull(
		TEXT("Automatic placement never fills a socket"),
		Inventory->GrantItemDefinition(URpgRuneAutomationTestOtherRuneDefinition::StaticClass()));
	TestTrue(TEXT("The sockets are still empty"), Inventory->GetSocketedItems(Weapon).IsEmpty());

	TestTrue(
		TEXT("An explicit move sockets the rune"),
		Inventory->MoveItem(MakeMoveIntent(Inventory, Rune, MakePlacement(Sockets, 0, 0))).IsSuccess());
	ViewModel->BindPlayerController(Controller);
	SocketViewModel =
		ViewModel->GetSocketGroupForCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary);
	const TArray<URpgInventoryAddressSlotViewModel*> SocketSlots =
		SocketViewModel ? SocketViewModel->GetSlots() : TArray<URpgInventoryAddressSlotViewModel*>();
	FGuid RuneEntryId;
	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		if (Entry.ItemId == Rune->GetItemId())
		{
			RuneEntryId = Entry.EntryId;
		}
	}
	TestTrue(
		TEXT("The first socket slot shows the socketed rune"),
		SocketSlots.Num() == 3 && SocketSlots[0] && RuneEntryId.IsValid() && SocketSlots[0]->GetEntryId() == RuneEntryId);

	TestTrue(
		TEXT("The weapon moves back into the pockets with its rune"),
		Inventory->MoveEquipmentItem(MakeMoveIntent(Inventory, Weapon, MakePlacement(Pockets, 1, 0))).IsSuccess());
	ViewModel->BindPlayerController(Controller);
	TestNull(TEXT("A weapon in the pockets shows no socket grid"), FindSocketGroup(Layout->GetSlotGroups()));
	TestNull(
		TEXT("The view model drops the sockets with the weapon"),
		ViewModel->GetSocketGroupForCarryRole(RpgGameplayTags::Rpg_Inventory_Layout_Role_Carry_Primary));
	TestEqual(TEXT("The rune stays socketed in the weapon"), Inventory->GetSocketedItems(Weapon).Num(), 1);

	ViewModel->UnbindPlayerInventory();
	return true;
}

#endif
