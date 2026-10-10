#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Misc/AutomationTest.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootResolver.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerMessageTags.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgItemTooltipViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPickupFeedViewModels.h"
#include "SurvivalRpg/UI/RpgHudAutomationTestTypes.h"
#include "SurvivalRpg/UI/RpgHudFadeBox.h"
#include "SurvivalRpg/UI/RpgInventoryScreenMessages.h"
#include "SurvivalRpg/UI/RpgTrailingProgressBar.h"
#include "SurvivalRpg/UI/RpgUISettings.h"
#include "SurvivalRpg/UI/RpgViewModelEntryBox.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHudFadeStateTest,
	"SurvivalRpg.UI.Hud.FadeHoldsThenFades",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHudFadeStateTest::RunTest(const FString& Parameters)
{
	FRpgHudFadeSettings Settings;
	Settings.HiddenOpacity = 0.4f;
	Settings.HoldSeconds = 2.0f;
	Settings.FadeInSeconds = 0.5f;
	Settings.FadeOutSeconds = 1.0f;

	FRpgHudFadeState State;
	TestTrue(TEXT("A new box holds its content first"), State.Advance(10.0, 0.0f, Settings));
	TestEqual(TEXT("A new box starts fully visible"), State.Opacity, 1.0f);
	TestTrue(TEXT("The hold keeps the box advancing"), State.Advance(11.9, 0.1f, Settings));
	TestEqual(TEXT("The content stays visible during the hold"), State.Opacity, 1.0f);

	TestTrue(TEXT("The fade-out keeps the box advancing"), State.Advance(12.5, 0.5f, Settings));
	TestEqual(TEXT("Half the fade-out time drops half the range"), State.Opacity, 0.5f);
	TestFalse(TEXT("The box settles at its hidden opacity"), State.Advance(13.0, 0.5f, Settings));
	TestEqual(TEXT("The hidden opacity is a floor"), State.Opacity, 0.4f);

	State.SetPinned(true);
	State.Advance(14.0, 0.25f, Settings);
	TestEqual(TEXT("Pinning fades in at the fade-in rate"), State.Opacity, 0.9f);
	TestFalse(TEXT("A pinned box at full opacity settles"), State.Advance(14.25, 0.25f, Settings));
	TestFalse(TEXT("A pinned box never fades"), State.Advance(30.0, 5.0f, Settings) || State.Opacity < 1.0f);

	State.SetPinned(false);
	TestTrue(TEXT("Unpinning starts a hold"), State.Advance(40.0, 0.1f, Settings));
	TestEqual(TEXT("The hold after unpinning keeps full opacity"), State.Opacity, 1.0f);
	State.Advance(42.0, 1.0f, Settings);
	TestEqual(TEXT("After the hold the box fades out"), State.Opacity, 0.4f);

	State.Pulse();
	State.Advance(50.0, 0.5f, Settings);
	TestEqual(TEXT("A pulse shows the content again"), State.Opacity, 1.0f);
	TestTrue(TEXT("A pulse holds like an unpin"), State.Advance(51.5, 0.1f, Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgProgressTrailStateTest,
	"SurvivalRpg.UI.Hud.TrailDrainsAfterDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgProgressTrailStateTest::RunTest(const FString& Parameters)
{
	FRpgProgressTrailState Trail;
	Trail.SetTarget(0.8f);
	TestEqual(TEXT("A rise shows at once"), Trail.Displayed, 0.8f);
	TestFalse(TEXT("A settled trail stops advancing"), Trail.Advance(1.0, 0.1f, 0.5f, 1.0f));

	Trail.SetTarget(0.5f);
	TestEqual(TEXT("A drop keeps the old value first"), Trail.Displayed, 0.8f);
	TestTrue(TEXT("A drop keeps the trail advancing"), Trail.Advance(2.0, 0.1f, 0.5f, 1.0f));
	TestEqual(TEXT("The trail lingers during the delay"), Trail.Displayed, 0.8f);
	Trail.Advance(2.6, 0.1f, 0.5f, 1.0f);
	TestEqual(TEXT("After the delay the trail drains at its speed"), Trail.Displayed, 0.7f, 0.001f);

	Trail.SetTarget(0.4f);
	Trail.Advance(2.7, 0.1f, 0.5f, 1.0f);
	TestEqual(TEXT("A further drop restarts the delay"), Trail.Displayed, 0.7f, 0.001f);
	TestFalse(TEXT("The drain stops at the target"), Trail.Advance(3.5, 2.0f, 0.5f, 1.0f));
	TestEqual(TEXT("The trail ends at the target"), Trail.Displayed, 0.4f);

	Trail.SetTarget(0.9f);
	TestEqual(TEXT("A rise during a trail jumps to the new value"), Trail.Displayed, 0.9f);
	return true;
}

namespace RpgHudWidgetTests
{
	class FScopedHudWorld
	{
	public:
		FScopedHudWorld()
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

		~FScopedHudWorld()
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
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	/** Creates a concrete item of ItemDefinition in a scratch inventory outside the player's graph. */
	URpgInventoryItemInstance* MakeItem(UWorld* World, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags = RF_Transient;
		AActor* Owner = World ? World->SpawnActor<AActor>(SpawnParameters) : nullptr;
		if (!Owner)
		{
			return nullptr;
		}
		URpgInventoryManagerComponent* Scratch = NewObject<URpgInventoryManagerComponent>(Owner, NAME_None, RF_Transient);
		Owner->AddInstanceComponent(Scratch);
		Scratch->RegisterComponent();
		return Scratch->AddItemDefinition(ItemDefinition, 1);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHudPickupFeedTest,
	"SurvivalRpg.UI.Hud.PickupFeedNetsMergesAndSuppresses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHudPickupFeedTest::RunTest(const FString& Parameters)
{
	using namespace RpgHudWidgetTests;
	FScopedHudWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	if (!TestNotNull(TEXT("A game world exists"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(SpawnParameters);
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(SpawnParameters);
	if (!TestNotNull(TEXT("The controller fixture exists"), Controller) ||
		!TestNotNull(TEXT("The player state fixture exists"), PlayerState))
	{
		return false;
	}
	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);
	URpgInventoryManagerComponent* Inventory = PlayerState->GetInventoryManagerComponent();
	URpgInventoryManagerComponent* OtherInventory = NewObject<URpgInventoryManagerComponent>(GetTransientPackage());
	URpgInventoryItemInstance* Wood = MakeItem(World, URpgInventoryAutomationTestStackItemDefinition::StaticClass());
	URpgInventoryItemInstance* Stone = MakeItem(World, URpgInventoryAutomationTestMaterialDefinition::StaticClass());
	if (!TestNotNull(TEXT("The player inventory exists"), Inventory) ||
		!TestNotNull(TEXT("A stack item exists"), Wood) ||
		!TestNotNull(TEXT("A material item exists"), Stone))
	{
		return false;
	}

	URpgUISettings* Settings = GetMutableDefault<URpgUISettings>();
	TGuardValue<float> WarmupGuard(Settings->HudPickupWarmupSeconds, 0.0f);
	UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(World);
	auto Broadcast = [&Messages](UActorComponent* Owner, URpgInventoryItemInstance* Item, int32 Delta, bool bFromRestore = false)
	{
		FRpgInventoryChangeMessage Message;
		Message.InventoryOwner = Owner;
		Message.Instance = Item;
		Message.Delta = Delta;
		Message.NewCount = FMath::Max(0, Delta);
		Message.bFromRestore = bFromRestore;
		Messages.BroadcastMessage(TAG_Rpg_Inventory_Message_StackChanged, Message);
	};
	auto SetScreenOpen = [&Messages, Controller](bool bOpen)
	{
		FRpgInventoryScreenActivationMessage Message;
		Message.OwningPlayer = Controller;
		Message.bActive = bOpen;
		Messages.BroadcastMessage(RpgGameplayTags::Rpg_Inventory_Message_ScreenActivation, Message);
	};

	URpgPickupFeedViewModel* Feed = NewObject<URpgPickupFeedViewModel>(GetTransientPackage());
	Feed->BindPlayerController(Controller);
	auto CountOf = [Feed](const URpgInventoryItemInstance* Item)
	{
		for (const URpgPickupFeedEntryViewModel* Entry : Feed->GetEntries())
		{
			if (Entry && Entry->GetItemDefinition() == Item->GetItemDef())
			{
				return Entry->GetCount();
			}
		}
		return 0;
	};

	Broadcast(Inventory, Wood, 5);
	Feed->FlushPendingGains();
	TestEqual(TEXT("A gain shows one notification"), Feed->GetEntries().Num(), 1);
	TestEqual(TEXT("The notification counts the gained units"), CountOf(Wood), 5);

	Broadcast(Inventory, Wood, 3);
	Feed->FlushPendingGains();
	TestEqual(TEXT("A further gain of the same item merges"), Feed->GetEntries().Num(), 1);
	TestEqual(TEXT("The merged notification adds up"), CountOf(Wood), 8);

	Broadcast(Inventory, Wood, -2);
	Broadcast(Inventory, Wood, 2);
	Broadcast(Inventory, Wood, -3);
	Broadcast(OtherInventory, Wood, 4);
	Broadcast(Inventory, Wood, 6, true);
	Feed->FlushPendingGains();
	TestEqual(TEXT("Moves, losses, other inventories and restores add nothing"), CountOf(Wood), 8);

	Broadcast(Inventory, Stone, 2);
	Feed->FlushPendingGains();
	TestEqual(TEXT("Another item gets its own notification"), Feed->GetEntries().Num(), 2);
	TestTrue(
		TEXT("The newest notification comes last"),
		Feed->GetEntries().Last() && Feed->GetEntries().Last()->GetItemDefinition() == Stone->GetItemDef());

	SetScreenOpen(true);
	Broadcast(Inventory, Wood, 1);
	Feed->FlushPendingGains();
	SetScreenOpen(false);
	Broadcast(Inventory, Wood, 1);
	Feed->FlushPendingGains();
	TestEqual(TEXT("Gains while a screen is open or just closed are screen moves"), CountOf(Wood), 8);

	const double HoldSeconds = Settings->HudPickupHoldSeconds;
	const double FadeSeconds = Settings->HudPickupFadeSeconds;
	Feed->UpdateExpiry(HoldSeconds + 0.01);
	TestTrue(
		TEXT("Notifications past their hold fade out"),
		Feed->GetEntries().Num() == 2 && Feed->GetEntries()[0]->IsExpiring());
	Feed->AddGain(Wood->GetItemDef(), 1);
	TestFalse(TEXT("A new gain revives a fading notification"), Feed->GetEntries()[0]->IsExpiring());
	Feed->UpdateExpiry(HoldSeconds + FadeSeconds + 0.01);
	TestEqual(TEXT("Faded notifications are removed"), Feed->GetEntries().Num(), 0);

	const TArray<TSubclassOf<URpgInventoryItemDefinition>> Definitions = {
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
		URpgInventoryAutomationTestNoTraitsItemDefinition::StaticClass(),
		URpgInventoryAutomationTestStackItemDefinition::StaticClass(),
		URpgInventoryAutomationTestMaterialDefinition::StaticClass(),
		URpgInventoryAutomationTestBulkConsumableDefinition::StaticClass(),
		URpgInventoryAutomationTestWideItemDefinition::StaticClass(),
		URpgInventoryAutomationTestLargeItemDefinition::StaticClass()};
	for (const TSubclassOf<URpgInventoryItemDefinition>& Definition : Definitions)
	{
		Feed->AddGain(Definition, 1);
	}
	const int32 MaxEntries = FMath::Max(1, Settings->HudPickupMaxEntries);
	TestEqual(TEXT("The feed keeps at most the configured number of notifications"), Feed->GetEntries().Num(), FMath::Min(MaxEntries, Definitions.Num()));
	TestTrue(
		TEXT("The oldest notification goes first"),
		Feed->GetEntries().Last() && Feed->GetEntries().Last()->GetItemDefinition() == Definitions.Last());

	Feed->Unbind();
	TestEqual(TEXT("Unbinding clears the feed"), Feed->GetEntries().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgHudViewModelEntryBoxTest,
	"SurvivalRpg.UI.Hud.ViewModelEntryBoxKeepsEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgHudViewModelEntryBoxTest::RunTest(const FString& Parameters)
{
	using namespace RpgHudWidgetTests;
	FScopedHudWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	URpgHudAutomationTestEntry* Host = World
		? CreateWidget<URpgHudAutomationTestEntry>(World, URpgHudAutomationTestEntry::StaticClass())
		: nullptr;
	if (Host && !Host->WidgetTree)
	{
		Host->WidgetTree = NewObject<UWidgetTree>(Host, TEXT("WidgetTree"), RF_Transient);
	}
	// Entries are created through the box's owning widget tree, as in an authored Widget Blueprint.
	URpgViewModelEntryBox* Box = Host ? NewObject<URpgViewModelEntryBox>(Host->WidgetTree) : nullptr;
	const FClassProperty* EntryClassProperty =
		FindFProperty<FClassProperty>(URpgViewModelEntryBox::StaticClass(), TEXT("EntryWidgetClass"));
	if (!TestNotNull(TEXT("A host widget exists"), Host) ||
		!TestNotNull(TEXT("The entry box exists"), Box) ||
		!TestNotNull(TEXT("The entry class property exists"), EntryClassProperty))
	{
		return false;
	}
	EntryClassProperty->SetObjectPropertyValue_InContainer(Box, URpgHudAutomationTestEntry::StaticClass());

	UObject* ItemA = NewObject<URpgItemStatRowViewModel>(GetTransientPackage());
	UObject* ItemB = NewObject<URpgItemStatRowViewModel>(GetTransientPackage());
	UObject* ItemC = NewObject<URpgItemStatRowViewModel>(GetTransientPackage());
	auto ShownItems = [Box]()
	{
		TArray<UObject*> Items;
		for (UUserWidget* Entry : Box->GetAllEntries())
		{
			const URpgHudAutomationTestEntry* TestEntry = Cast<URpgHudAutomationTestEntry>(Entry);
			Items.Add(TestEntry ? TestEntry->GetEntryItem() : nullptr);
		}
		return Items;
	};

	Box->SetViewModelItems({ItemA, ItemB});
	TestTrue(TEXT("Each item gets one entry in order"), ShownItems() == TArray<UObject*>({ItemA, ItemB}));
	UUserWidget* EntryForB = Box->GetAllEntries().IsValidIndex(1) ? Box->GetAllEntries()[1] : nullptr;

	Box->SetViewModelItems({ItemB, ItemC});
	TestTrue(TEXT("Removing and appending keeps the order"), ShownItems() == TArray<UObject*>({ItemB, ItemC}));
	TestTrue(
		TEXT("A surviving item keeps its entry widget"),
		EntryForB && Box->GetAllEntries().IsValidIndex(0) && Box->GetAllEntries()[0] == EntryForB);

	Box->SetViewModelItems({ItemC, ItemB});
	TestTrue(TEXT("A reorder rebuilds in the new order"), ShownItems() == TArray<UObject*>({ItemC, ItemB}));

	Box->ClearViewModelItems();
	TestEqual(TEXT("Clearing removes every entry"), Box->GetNumEntries(), 0);
	return true;
}

#endif
