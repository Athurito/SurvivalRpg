#include "RpgStorageInventoryWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryWidget.h"
#include "SurvivalRpg/UI/RpgInventorySlotGroupPanelWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Input/CommonBoundActionBar.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace RpgStorageInventoryWidgetTests
{
	class FScopedWidgetWorld
	{
	public:
		FScopedWidgetWorld()
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

		~FScopedWidgetWorld()
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
			return World != nullptr;
		}

		UWorld* GetTestWorld() const
		{
			return World;
		}

		URpgInventoryManagerComponent* CreateInventory(const TCHAR* DebugName)
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(World, AActor::StaticClass(), FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			AActor* OwnerActor = World->SpawnActor<AActor>(SpawnParameters);
			if (!OwnerActor)
			{
				return nullptr;
			}

			URpgInventoryManagerComponent* Inventory = NewObject<URpgInventoryManagerComponent>(
				OwnerActor,
				MakeUniqueObjectName(OwnerActor, URpgInventoryManagerComponent::StaticClass(), TEXT("Inventory")),
				RF_Transient);
			OwnerActor->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	URpgInventoryScreenPayload* MakePayload(
		UObject* Outer,
		URpgInventoryManagerComponent* PrimaryInventory,
		URpgInventoryManagerComponent* SecondaryInventory)
	{
		URpgInventoryScreenPayload* Payload = NewObject<URpgInventoryScreenPayload>(Outer);
		Payload->PrimaryInventory = PrimaryInventory;
		Payload->SecondaryInventory = SecondaryInventory;
		return Payload;
	}

	UClass* LoadStorageSpatialWidgetClass()
	{
		return LoadClass<URpgStorageInventoryWidget>(
			nullptr,
			TEXT("/Game/SurvivalRpg/UI/CUI_StorageSpatial.CUI_StorageSpatial_C"));
	}

	int32 CountDirectPlayerInventoryViewModels(const UObject* Outer)
	{
		TArray<UObject*> DirectChildren;
		GetObjectsWithOuter(Outer, DirectChildren, EGetObjectsFlags::None);

		int32 Count = 0;
		for (const UObject* Candidate : DirectChildren)
		{
			if (Candidate && Candidate->IsA<URpgPlayerInventoryViewModel>())
			{
				++Count;
			}
		}
		return Count;
	}

	int32 CountDelegateBindingsTo(
		const FRpgPlayerInventoryViewModelChanged& Delegate,
		const UObject* Target)
	{
		int32 Count = 0;
		for (const UObject* BoundObject : Delegate.GetAllObjects())
		{
			if (BoundObject == Target)
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgStorageSpatialCompositionTest,
	"SurvivalRpg.Inventory.UI.StorageSpatialComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStorageSpatialCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgStorageInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* StorageWidgetClass = LoadStorageSpatialWidgetClass();
	if (!TestNotNull(TEXT("Authored Storage Spatial class loads"), StorageWidgetClass))
	{
		return false;
	}
	TestTrue(
		TEXT("Authored class derives from the native Storage presenter"),
		StorageWidgetClass->IsChildOf(URpgStorageInventoryWidget::StaticClass()));
	TestTrue(
		TEXT("The native Player presenter derives from the shared inventory interaction screen"),
		URpgPlayerInventoryWidget::StaticClass()->IsChildOf(
			URpgInventoryInteractionScreenWidget::StaticClass()));
	TestTrue(
		TEXT("The native Storage presenter derives from the shared inventory interaction screen"),
		URpgStorageInventoryWidget::StaticClass()->IsChildOf(
			URpgInventoryInteractionScreenWidget::StaticClass()));
	TestFalse(
		TEXT("The native Storage presenter no longer inherits the Player screen contract"),
		URpgStorageInventoryWidget::StaticClass()->IsChildOf(
			URpgPlayerInventoryWidget::StaticClass()));
	TestTrue(
		TEXT("The authored Storage class retains the shared inventory interaction contract"),
		StorageWidgetClass->IsChildOf(
			URpgInventoryInteractionScreenWidget::StaticClass()));
	TestFalse(
		TEXT("The authored Storage class does not reintroduce the Player screen contract"),
		StorageWidgetClass->IsChildOf(
			URpgPlayerInventoryWidget::StaticClass()));

	URpgStorageInventoryWidget* Widget =
		CreateWidget<URpgStorageInventoryWidget>(TestWorld.GetTestWorld(), StorageWidgetClass);
	if (!TestNotNull(TEXT("Authored Storage Spatial widget initializes"), Widget))
	{
		return false;
	}

	UWidget* PlayerPanel = Widget->GetWidgetFromName(TEXT("PlayerGroupsPanel"));
	UWidget* SecondaryGrid = Widget->GetWidgetFromName(TEXT("SecondaryInventoryGrid"));
	UWidget* DragVisualCanvas = Widget->GetWidgetFromName(TEXT("DragVisualCanvas"));
	UOverlay* RootOverlay = Cast<UOverlay>(Widget->GetWidgetFromName(TEXT("RootOverlay")));
	UWidget* ContentRow = Widget->GetWidgetFromName(TEXT("ContentRow"));
	UWidget* ActionBar = Widget->GetWidgetFromName(TEXT("ActionBar"));
	TestNotNull(TEXT("RootOverlay is authored as the screen root"), RootOverlay);
	TestTrue(
		TEXT("ContentRow is the authored two-column layout"),
		ContentRow && ContentRow->IsA<UHorizontalBox>());
	TestTrue(
		TEXT("ActionBar uses CommonUI's bound action bar"),
		ActionBar && ActionBar->IsA<UCommonBoundActionBar>());
	TestTrue(
		TEXT("PlayerGroupsPanel has the aggregate spatial panel type"),
		PlayerPanel && PlayerPanel->IsA<URpgInventorySlotGroupPanelWidget>());
	TestTrue(
		TEXT("SecondaryInventoryGrid has the exact spatial grid type"),
		SecondaryGrid && SecondaryGrid->IsA<URpgInventorySpatialGridWidget>());
	const FObjectPropertyBase* PlayerPanelProperty = FindFProperty<FObjectPropertyBase>(
		URpgStorageInventoryWidget::StaticClass(),
		TEXT("PlayerGroupsPanel"));
	const FObjectPropertyBase* SecondaryGridProperty = FindFProperty<FObjectPropertyBase>(
		URpgStorageInventoryWidget::StaticClass(),
		TEXT("SecondaryInventoryGrid"));
	TestTrue(
		TEXT("PlayerGroupsPanel is bound into the native presenter property"),
		PlayerPanelProperty &&
			PlayerPanelProperty->GetObjectPropertyValue_InContainer(Widget) == PlayerPanel);
	TestTrue(
		TEXT("SecondaryInventoryGrid is bound into the native presenter property"),
		SecondaryGridProperty &&
			SecondaryGridProperty->GetObjectPropertyValue_InContainer(Widget) == SecondaryGrid);
	TestTrue(
		TEXT("DragVisualCanvas is the authored top-level drag host"),
		DragVisualCanvas && DragVisualCanvas->IsA<UCanvasPanel>());
	if (DragVisualCanvas)
	{
		TestEqual(
			TEXT("DragVisualCanvas never intercepts inventory pointer input"),
			DragVisualCanvas->GetVisibility(),
			ESlateVisibility::HitTestInvisible);
	}
	if (RootOverlay && DragVisualCanvas)
	{
		TestEqual(
			TEXT("DragVisualCanvas is the final root child and therefore renders above both inventories"),
			RootOverlay->GetChildIndex(DragVisualCanvas),
			RootOverlay->GetChildrenCount() - 1);
	}

	TestNull(
		TEXT("Legacy player TileView wrapper is absent"),
		Widget->GetWidgetFromName(TEXT("CUI_Inventory_PlayerInventory")));
	TestNull(
		TEXT("Legacy storage TileView wrapper is absent"),
		Widget->GetWidgetFromName(TEXT("CUI_Inventory_StorageInventory")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgStorageInventoryWidgetContextLifecycleTest,
	"SurvivalRpg.Inventory.UI.StorageContextLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStorageInventoryWidgetContextLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgStorageInventoryWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* PrimaryA = TestWorld.CreateInventory(TEXT("PrimaryA"));
	URpgInventoryManagerComponent* SecondaryA = TestWorld.CreateInventory(TEXT("SecondaryA"));
	URpgInventoryManagerComponent* PrimaryB = TestWorld.CreateInventory(TEXT("PrimaryB"));
	URpgInventoryManagerComponent* SecondaryB = TestWorld.CreateInventory(TEXT("SecondaryB"));
	if (!TestNotNull(TEXT("Primary inventory A exists"), PrimaryA) ||
		!TestNotNull(TEXT("Secondary inventory A exists"), SecondaryA) ||
		!TestNotNull(TEXT("Primary inventory B exists"), PrimaryB) ||
		!TestNotNull(TEXT("Secondary inventory B exists"), SecondaryB))
	{
		return false;
	}

	UClass* StorageWidgetClass = LoadStorageSpatialWidgetClass();
	if (!TestNotNull(TEXT("Authored Storage Spatial class loads"), StorageWidgetClass))
	{
		return false;
	}

	URpgStorageInventoryWidget* Widget =
		CreateWidget<URpgStorageInventoryWidget>(TestWorld.GetTestWorld(), StorageWidgetClass);
	if (!TestNotNull(TEXT("Storage widget exists"), Widget))
	{
		return false;
	}

	URpgInventoryScreenPayload* PayloadA = MakePayload(Widget, PrimaryA, SecondaryA);
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(Widget, PayloadA);
	TestEqual(TEXT("Pre-activation payload is staged"), Widget->GetInventoryScreenPayload(), PayloadA);
	TestFalse(TEXT("Staging does not resolve the secondary root before activation"), Widget->GetSecondaryInventoryRootHandle().IsValid());
	TestNull(TEXT("Staging does not create an inventory coordinator"), Widget->GetInventoryDragDropCoordinator());
	TestEqual(
		TEXT("Staging does not bind presentation"),
		Widget->GetStoragePresentationBindGeneration(),
		0u);

	Widget->ActivateWidget();
	TestTrue(TEXT("Storage widget activates through the CommonUI lifecycle"), Widget->IsActivated());
	TestEqual(
		TEXT("Payload delivered before activation binds exactly once"),
		Widget->GetStoragePresentationBindGeneration(),
		1u);
	TestTrue(TEXT("First secondary root handle is valid"), Widget->GetSecondaryInventoryRootHandle().IsValid());

	URpgInventoryDragDropCoordinator* Coordinator = Widget->GetInventoryDragDropCoordinator();
	if (!TestNotNull(TEXT("Storage drag/drop coordinator exists"), Coordinator))
	{
		return false;
	}
	URpgInventoryPanelNavigationCoordinator* PanelNavigator = Widget->GetInventoryPanelNavigator();
	if (!TestNotNull(TEXT("Storage panel navigator exists"), PanelNavigator))
	{
		return false;
	}
	TestEqual(
		TEXT("The drag/drop coordinator is owned by the Storage screen"),
		Coordinator->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("The panel navigator is owned by the Storage screen"),
		PanelNavigator->GetOuter(),
		static_cast<UObject*>(Widget));

	URpgPlayerInventoryViewModel* StoragePlayerViewModel =
		Widget->GetStoragePlayerInventoryViewModel();
	if (!TestNotNull(
		TEXT("Storage screen owns an aggregate player-side view model"),
		StoragePlayerViewModel))
	{
		return false;
	}
	TestEqual(
		TEXT("Storage player-side VM is owned by the Storage screen"),
		StoragePlayerViewModel->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("Storage screen owns exactly one direct aggregate player-side VM"),
		CountDirectPlayerInventoryViewModels(Widget),
		1);
	TestEqual(
		TEXT("Storage binds its slot-group presenter delegate exactly once"),
		CountDelegateBindingsTo(
			StoragePlayerViewModel->OnSlotGroupsChanged,
			Widget),
		1);
	TestEqual(
		TEXT("Storage never binds the Player screen's gear presenter delegate"),
		CountDelegateBindingsTo(
			StoragePlayerViewModel->OnGearSlotsChanged,
			Widget),
		0);
	TestEqual(
		TEXT("Storage never binds the Player screen's actionbar presenter delegate"),
		CountDelegateBindingsTo(
			StoragePlayerViewModel->OnActionBarSlotsChanged,
			Widget),
		0);

	UClass* PlayerWidgetClass = LoadClass<URpgPlayerInventoryWidget>(
		nullptr,
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_PlayerInventory.CUI_PlayerInventory_C"));
	URpgPlayerInventoryWidget* PlayerWidget = PlayerWidgetClass
		? CreateWidget<URpgPlayerInventoryWidget>(
			TestWorld.GetTestWorld(),
			PlayerWidgetClass)
		: nullptr;
	if (!TestNotNull(
		TEXT("Authored Player screen initializes beside Storage"),
		PlayerWidget) ||
		!TestNotNull(
			TEXT("Player screen owns its aggregate VM"),
			PlayerWidget ? PlayerWidget->GetPlayerInventoryViewModel() : nullptr))
	{
		return false;
	}
	TestNotEqual(
		TEXT("Player and Storage screens never share an aggregate VM instance"),
		PlayerWidget->GetPlayerInventoryViewModel(),
		StoragePlayerViewModel);

	TestEqual(
		TEXT("First secondary inventory routes back to its primary inventory"),
		Coordinator->ResolveQuickTransferTarget(SecondaryA),
		PrimaryA);
	TestEqual(
		TEXT("First primary inventory routes to its secondary inventory"),
		Coordinator->ResolveQuickTransferTarget(PrimaryA),
		SecondaryA);

	FRpgInventoryDragPayload HeldPayload;
	HeldPayload.SourceType = ERpgInventoryDragSourceType::InventoryEntry;
	HeldPayload.SourceInventory = SecondaryA;
	HeldPayload.EntryId = FGuid::NewGuid();
	HeldPayload.StackCount = 1;
	if (!TestTrue(
			TEXT("Transient Storage payload owns canonical item metadata"),
			RpgInventoryAutomationTestTypes::PopulateCanonicalSpatialItem(
				HeldPayload,
				SecondaryA,
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass())))
	{
		return false;
	}
	TestTrue(TEXT("A transient interaction starts before the context switch"), Coordinator->BeginHold(HeldPayload));
	Coordinator->SetFocusedInventory(SecondaryA);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(Widget, PayloadA);
	TestTrue(TEXT("Reapplying the same payload preserves the active interaction"), Coordinator->HasHeldPayload());
	TestEqual(TEXT("Reapplying the same payload preserves focused inventory"), Coordinator->GetFocusedInventory(), SecondaryA);
	TestEqual(
		TEXT("Reapplying the same payload preserves the single screen drag/drop coordinator"),
		Widget->GetInventoryDragDropCoordinator(),
		Coordinator);
	TestEqual(
		TEXT("Reapplying the same payload preserves the single screen panel navigator"),
		Widget->GetInventoryPanelNavigator(),
		PanelNavigator);
	TestEqual(
		TEXT("Reapplying the same active payload does not bind or refresh it again"),
		Widget->GetStoragePresentationBindGeneration(),
		1u);

	URpgInventoryScreenPayload* PayloadB = MakePayload(Widget, PrimaryB, SecondaryB);
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(Widget, PayloadB);
	TestFalse(TEXT("Context switch cancels the old interaction"), Coordinator->HasHeldPayload());
	URpgInventoryManagerComponent* FocusedAfterContextSwitch = Coordinator->GetFocusedInventory();
	TestNotEqual(
		TEXT("Context switch never retains focus on the old secondary inventory"),
		FocusedAfterContextSwitch,
		SecondaryA);
	TestTrue(
		TEXT("Context switch leaves focus clear or selects an inventory from the new context"),
		!FocusedAfterContextSwitch ||
			FocusedAfterContextSwitch == PrimaryB ||
			FocusedAfterContextSwitch == SecondaryB);
	TestNull(
		TEXT("Context switch removes the old quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(SecondaryA));
	TestNull(
		TEXT("Context switch removes the old reverse quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(PrimaryA));
	TestEqual(
		TEXT("Context switch installs the new quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(SecondaryB),
		PrimaryB);
	TestEqual(
		TEXT("Context switch installs the new reverse quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(PrimaryB),
		SecondaryB);
	TestEqual(TEXT("Second valid payload replaces the first"), Widget->GetInventoryScreenPayload(), PayloadB);
	TestEqual(
		TEXT("An active context transition binds the replacement exactly once"),
		Widget->GetStoragePresentationBindGeneration(),
		2u);

	Coordinator->SetFocusedInventory(SecondaryB);
	Widget->DeactivateWidget();
	TestFalse(TEXT("Storage widget deactivates through the CommonUI lifecycle"), Widget->IsActivated());
	TestEqual(
		TEXT("Deactivation retains the screen-owned drag/drop coordinator for CommonUI pooling"),
		Widget->GetInventoryDragDropCoordinator(),
		Coordinator);
	TestEqual(
		TEXT("Deactivation retains the screen-owned panel navigator for CommonUI pooling"),
		Widget->GetInventoryPanelNavigator(),
		PanelNavigator);
	TestEqual(
		TEXT("Deactivation retains the Storage-owned aggregate VM for CommonUI pooling"),
		Widget->GetStoragePlayerInventoryViewModel(),
		StoragePlayerViewModel);
	TestNull(TEXT("Deactivation releases the retained payload"), Widget->GetInventoryScreenPayload());
	TestFalse(TEXT("Deactivation invalidates the secondary root handle"), Widget->GetSecondaryInventoryRootHandle().IsValid());
	TestNull(TEXT("Deactivation clears focused inventory"), Coordinator->GetFocusedInventory());
	TestNull(
		TEXT("Deactivation removes the active quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(SecondaryB));
	TestNull(
		TEXT("Deactivation removes the active reverse quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(PrimaryB));
	TestEqual(
		TEXT("Deactivation never performs another presentation bind"),
		Widget->GetStoragePresentationBindGeneration(),
		2u);

	Widget->ActivateWidget();
	TestTrue(TEXT("The pooled Storage widget can activate again"), Widget->IsActivated());
	TestEqual(
		TEXT("Pool reactivation reuses the single screen drag/drop coordinator"),
		Widget->GetInventoryDragDropCoordinator(),
		Coordinator);
	TestEqual(
		TEXT("Pool reactivation reuses the single screen panel navigator"),
		Widget->GetInventoryPanelNavigator(),
		PanelNavigator);
	TestEqual(
		TEXT("Pool reactivation reuses the single Storage-owned aggregate VM"),
		Widget->GetStoragePlayerInventoryViewModel(),
		StoragePlayerViewModel);
	TestNull(TEXT("Reactivation does not resurrect a stale payload"), Widget->GetInventoryScreenPayload());
	TestEqual(
		TEXT("Reactivation without a staged payload performs no Storage bind"),
		Widget->GetStoragePresentationBindGeneration(),
		2u);
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(Widget, PayloadB);
	TestEqual(TEXT("Reused widget accepts a fresh payload"), Widget->GetInventoryScreenPayload(), PayloadB);
	TestEqual(
		TEXT("Fresh payload after pool reactivation preserves the single screen drag/drop coordinator"),
		Widget->GetInventoryDragDropCoordinator(),
		Coordinator);
	TestEqual(
		TEXT("Fresh payload after pool reactivation preserves the single screen panel navigator"),
		Widget->GetInventoryPanelNavigator(),
		PanelNavigator);
	TestEqual(
		TEXT("Fresh payload after pool reactivation preserves the Storage-owned aggregate VM"),
		Widget->GetStoragePlayerInventoryViewModel(),
		StoragePlayerViewModel);
	TestEqual(
		TEXT("Payload delivered after activation binds exactly once"),
		Widget->GetStoragePresentationBindGeneration(),
		3u);
	TestEqual(
		TEXT("Reused widget restores the forward quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(PrimaryB),
		SecondaryB);
	TestEqual(
		TEXT("Reused widget restores the reverse quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(SecondaryB),
		PrimaryB);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(Widget, nullptr);
	TestNull(TEXT("Clearing the payload releases the retained payload"), Widget->GetInventoryScreenPayload());
	TestFalse(TEXT("Clearing the payload invalidates the secondary root handle"), Widget->GetSecondaryInventoryRootHandle().IsValid());
	TestNull(TEXT("Clearing the payload clears focused inventory"), Coordinator->GetFocusedInventory());
	TestNull(
		TEXT("Clearing the payload removes the active quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(SecondaryB));
	TestNull(
		TEXT("Clearing the payload removes the active reverse quick-transfer route"),
		Coordinator->ResolveQuickTransferTarget(PrimaryB));
	TestEqual(
		TEXT("Clearing the payload does not bind presentation"),
		Widget->GetStoragePresentationBindGeneration(),
		3u);

	URpgInventoryScreenPayload* IncompletePayload = MakePayload(Widget, PrimaryA, nullptr);
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(Widget, IncompletePayload);
	TestNull(TEXT("An incomplete dual-inventory payload leaves the screen reset"), Widget->GetInventoryScreenPayload());
	TestEqual(
		TEXT("An incomplete payload does not bind presentation"),
		Widget->GetStoragePresentationBindGeneration(),
		3u);

	URpgInventoryScreenPayload* AliasedPayload = MakePayload(Widget, PrimaryA, PrimaryA);
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(Widget, AliasedPayload);
	TestNull(TEXT("A payload cannot use the same inventory for both screen sides"), Widget->GetInventoryScreenPayload());
	TestEqual(
		TEXT("An aliased dual-inventory payload does not bind presentation"),
		Widget->GetStoragePresentationBindGeneration(),
		3u);

	Widget->DeactivateWidget();
	return true;
}

#endif
