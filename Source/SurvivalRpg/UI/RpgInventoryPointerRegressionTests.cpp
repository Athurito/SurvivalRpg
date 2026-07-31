#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialItemWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropOperation.h"
#include "SurvivalRpg/Inventory/RpgInventoryInteractionSession.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/UI/RpgInventoryDragVisualWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPointerAutomationTestScreen.h"

#include "Blueprint/UserWidget.h"
#include "CommonLocalPlayer.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Input/DragAndDrop.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"
#include "Rendering/SlateLayoutTransform.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

namespace RpgInventoryPointerRegressionTests
{
	class FScopedWidgetWorld
	{
	public:
		FScopedWidgetWorld()
		{
			GameInstance = NewObject<UGameInstance>(
				GEngine,
				NAME_None,
				RF_Transient);
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

		UWorld* GetWorld() const
		{
			return World;
		}

		bool InitializePlayerFixture()
		{
			if (!World)
			{
				return false;
			}
			if (Controller && PlayerState && LocalPlayer)
			{
				return true;
			}

			FActorSpawnParameters ControllerParameters;
			ControllerParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerController::StaticClass(),
				TEXT("PointerRoutingController"));
			ControllerParameters.ObjectFlags = RF_Transient;
			ControllerParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Controller = World->SpawnActor<
				ARpgInventoryAutomationTestPlayerController>(
					ControllerParameters);

			FActorSpawnParameters PlayerStateParameters;
			PlayerStateParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerState::StaticClass(),
				TEXT("PointerRoutingPlayerState"));
			PlayerStateParameters.ObjectFlags = RF_Transient;
			PlayerStateParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			PlayerState = World->SpawnActor<
				ARpgInventoryAutomationTestPlayerState>(
					PlayerStateParameters);

			LocalPlayer = NewObject<UCommonLocalPlayer>(
				GEngine,
				NAME_None,
				RF_Transient);
			if (!Controller || !PlayerState || !LocalPlayer)
			{
				return false;
			}

			Controller->SetPlayerState(PlayerState);
			PlayerState->SetOwner(Controller);
			World->AddController(Controller);
			Controller->SetPlayer(LocalPlayer);
			return Controller->GetRpgPlayerState() == PlayerState &&
				PlayerState->GetInventoryManagerComponent() != nullptr &&
				Controller->GetInventoryUiActionComponent() != nullptr;
		}

		ARpgInventoryAutomationTestPlayerController* GetController() const
		{
			return Controller;
		}

		URpgInventoryManagerComponent* GetPlayerInventory() const
		{
			return PlayerState
				? PlayerState->GetInventoryManagerComponent()
				: nullptr;
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
			AActor* OwnerActor = World->SpawnActor<AActor>(
				SpawnParameters);
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
		TObjectPtr<ARpgInventoryAutomationTestPlayerController> Controller;
		TObjectPtr<ARpgInventoryAutomationTestPlayerState> PlayerState;
		TObjectPtr<UCommonLocalPlayer> LocalPlayer;
	};

	UClass* LoadPlayerInventoryScreenClass()
	{
		return LoadClass<URpgInventoryInteractionScreenWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_PlayerInventory.CUI_PlayerInventory_C"));
	}

	bool MakeValidPointerPayload(
		URpgInventoryManagerComponent* SourceInventory,
		FRpgInventoryDragPayload& OutPayload)
	{
		OutPayload = FRpgInventoryDragPayload();
		OutPayload.SourceType =
			ERpgInventoryDragSourceType::InventoryEntry;
		OutPayload.SourceInventory = SourceInventory;
		OutPayload.EntryId = FGuid::NewGuid();
		OutPayload.StackCount = 1;
		if (!SourceInventory ||
			!RpgInventoryAutomationTestTypes::PopulateCanonicalSpatialItem(
				OutPayload,
				SourceInventory,
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass()))
		{
			return false;
		}

		OutPayload.SourcePlacement.SetContainerHandle(
			FRpgInventoryContainerHandle::MakeRoot(
				SourceInventory->GetDefaultContainerId()));
		OutPayload.SourcePlacement.X = 0;
		OutPayload.SourcePlacement.Y = 0;
		OutPayload.SourcePlacement.Width =
			OutPayload.ItemFootprint.Width;
		OutPayload.SourcePlacement.Height =
			OutPayload.ItemFootprint.Height;
		return URpgInventoryDragDropCoordinator::IsPayloadValid(
			OutPayload);
	}

	FRpgInventoryGridPlacement MakePlacement(
		const FRpgInventoryContainerHandle& ContainerHandle,
		int32 X,
		int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(ContainerHandle);
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}

	bool MakeOwnedPointerPayload(
		URpgInventoryManagerComponent* SourceInventory,
		URpgInventoryItemInstance* Item,
		FRpgInventoryDragPayload& OutPayload)
	{
		OutPayload = FRpgInventoryDragPayload();
		if (!SourceInventory || !Item)
		{
			return false;
		}

		for (const FRpgInventoryEntryView& Entry :
			SourceInventory->GetAllEntries())
		{
			if (Entry.Instance != Item)
			{
				continue;
			}

			OutPayload.SourceType =
				ERpgInventoryDragSourceType::InventoryEntry;
			OutPayload.SourceInventory = SourceInventory;
			OutPayload.ItemInstance = Item;
			OutPayload.EntryId = Entry.EntryId;
			OutPayload.StackCount = Entry.StackCount;
			OutPayload.SourcePlacement = Entry.Placement;
			OutPayload.ItemFootprint =
				Entry.Placement.GetUnrotatedSize();
			return URpgInventoryDragDropCoordinator::IsPayloadValid(
				OutPayload);
		}

		return false;
	}

	FRpgInventoryGridSize MakeFootprint(int32 Width, int32 Height)
	{
		FRpgInventoryGridSize Footprint;
		Footprint.Width = Width;
		Footprint.Height = Height;
		return Footprint;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPointerDragLeaveLifecycleTest,
	"SurvivalRpg.Inventory.UI.Pointer.DragLeaveLifecycle",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPointerDragLeaveLifecycleTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPointerRegressionTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT("Standalone pointer-test world is valid"),
			TestWorld.IsValid()))
	{
		return false;
	}

	UClass* ScreenClass = LoadPlayerInventoryScreenClass();
	if (!TestNotNull(
			TEXT("Authored Player Inventory screen class loads"),
			ScreenClass))
	{
		return false;
	}

	TStrongObjectPtr<URpgInventoryInteractionScreenWidget> ScreenHolder(
		CreateWidget<URpgInventoryInteractionScreenWidget>(
			TestWorld.GetWorld(),
			ScreenClass));
	URpgInventoryInteractionScreenWidget* Screen = ScreenHolder.Get();
	if (!TestNotNull(
			TEXT("Real inventory interaction screen initializes"),
			Screen))
	{
		return false;
	}

	Screen->EnsureInventoryInteractionObjects();
	URpgInventoryDragDropCoordinator* Coordinator =
		Screen->InventoryDragDropCoordinator.Get();
	URpgInventoryInteractionSession* Session = Coordinator
		? Coordinator->GetInteractionSession()
		: nullptr;
	TStrongObjectPtr<URpgInventoryAutomationTestInteractionDelegateCounter>
		DelegateCounter(
		NewObject<URpgInventoryAutomationTestInteractionDelegateCounter>(
			Screen));
	URpgInventoryManagerComponent* SourceInventory =
		TestWorld.CreateInventory(TEXT("PointerDragSource"));
	if (!TestNotNull(
			TEXT("Screen owns its real drag/drop coordinator"),
			Coordinator) ||
		!TestNotNull(
			TEXT("Coordinator owns its real interaction session"),
			Session) ||
		!TestNotNull(
			TEXT("Pointer lifecycle owns a delegate counter"),
			DelegateCounter.Get()) ||
		!TestNotNull(
			TEXT("Pointer fixture owns a source inventory"),
			SourceInventory))
	{
		return false;
	}
	Session->OnInteractionStateChanged.AddDynamic(
		DelegateCounter.Get(),
		&URpgInventoryAutomationTestInteractionDelegateCounter::RecordInteractionState);
	Session->OnPayloadChanged.AddDynamic(
		DelegateCounter.Get(),
		&URpgInventoryAutomationTestInteractionDelegateCounter::RecordPayloadChanged);

	FRpgInventoryDragPayload Payload;
	if (!TestTrue(
			TEXT("Pointer fixture builds a canonical valid payload"),
			MakeValidPointerPayload(SourceInventory, Payload)) ||
		!TestTrue(
			TEXT("Real coordinator begins a mouse-owned interaction"),
			Coordinator->BeginPointerDrag(Payload)))
	{
		return false;
	}

	FRpgInventoryDropTarget PreviewTarget =
		URpgInventoryDragDropCoordinator::MakeInventoryPanelTarget(
			SourceInventory);
	Session->SetPreviewTarget(
		PreviewTarget,
		ERpgInventoryInteractionPreviewState::Move);

	FRpgInventorySpatialPreviewDescriptor SpatialPreview;
	SpatialPreview.bValid = true;
	SpatialPreview.EntryId = Payload.EntryId;
	SpatialPreview.Target = PreviewTarget;
	SpatialPreview.TargetPlacement = Payload.SourcePlacement;
	SpatialPreview.PreviewState =
		ERpgInventoryInteractionPreviewState::Move;
	Session->SetSpatialPreviewDescriptor(SpatialPreview);

	UButton* PreviewOwner = NewObject<UButton>(Screen);
	TStrongObjectPtr<URpgInventoryDragDropOperation> OperationHolder(
		NewObject<URpgInventoryDragDropOperation>(Screen));
	URpgInventoryDragDropOperation* Operation = OperationHolder.Get();
	URpgInventoryDragVisualWidget* Decorator =
		NewObject<URpgInventoryDragVisualWidget>(Operation);
	if (!TestNotNull(
			TEXT("Pointer preview has a concrete target owner"),
			PreviewOwner) ||
		!TestNotNull(
			TEXT("Pointer interaction has a real UMG drag operation"),
			Operation) ||
		!TestNotNull(
			TEXT("Pointer operation has a real drag decorator"),
			Decorator))
	{
		return false;
	}

	Operation->InventoryPayload = Payload;
	Operation->SetInteractionSession(Session);
	Operation->DefaultDragVisual = Decorator;
	Operation->SetScreenOwnedDragVisualActive(true);
	Screen->ActivePointerDropTarget = PreviewOwner;
	Screen->ActivePointerDragOperation = Operation;
	Screen->bHasLastPointerDragScreenPosition = true;
	Screen->LastPointerDragScreenPosition = FVector2D(640.0f, 360.0f);

	TestTrue(
		TEXT("Precondition retains the mouse payload"),
		Session->HasPayload());
	TestEqual(
		TEXT("Precondition records mouse input ownership"),
		Session->GetInputMode(),
		ERpgInventoryInteractionInputMode::Mouse);
	TestEqual(
		TEXT("Precondition publishes a move preview"),
		Session->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::Move);
	TestTrue(
		TEXT("Precondition publishes a spatial preview descriptor"),
		Session->GetSpatialPreviewDescriptor().bValid);
	TestTrue(
		TEXT("Precondition hides the operation decorator while the screen owns the ghost"),
		FMath::IsNearlyZero(Decorator->GetRenderOpacity()));

	const FPointerEvent PointerEvent;
	const FDragDropEvent DragDropEvent(
		PointerEvent,
		TSharedPtr<FDragDropOperation>());
	Screen->NativeOnDragLeave(DragDropEvent, Operation);

	TestFalse(
		TEXT("Leaving the screen releases the previous pointer target"),
		Screen->ActivePointerDropTarget.IsValid());
	TestEqual(
		TEXT("Leaving the screen clears only the target preview"),
		Session->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::None);
	TestEqual(
		TEXT("Leaving the screen clears the preview target"),
		Session->GetTarget().TargetType,
		ERpgInventoryDropTargetType::None);
	TestFalse(
		TEXT("Leaving the screen clears the stale spatial descriptor"),
		Session->GetSpatialPreviewDescriptor().bValid);
	TestTrue(
		TEXT("Leaving the screen does not cancel the interaction payload"),
		Session->HasPayload());
	TestTrue(
		TEXT("Coordinator still exposes the payload after screen leave"),
		Coordinator->HasHeldPayload());
	TestEqual(
		TEXT("Screen leave preserves mouse input ownership"),
		Session->GetInputMode(),
		ERpgInventoryInteractionInputMode::Mouse);
	TestNull(
		TEXT("Screen releases its transient operation reference on leave"),
		Screen->ActivePointerDragOperation.Get());
	TestFalse(
		TEXT("Screen clears its cached pointer position on leave"),
		Screen->bHasLastPointerDragScreenPosition);
	TestTrue(
		TEXT("Screen leave returns presentation to the UMG decorator"),
		FMath::IsNearlyEqual(
			Decorator->GetRenderOpacity(),
			1.0f,
			KINDA_SMALL_NUMBER));

	// A weak target can become stale between pointer events when a dynamic presenter is pooled or rebuilt. Null
	// switching must still clear the old preview instead of mistaking the stale weak pointer for an already-empty one.
	Screen->ActivePointerDropTarget = PreviewOwner;
	TWeakObjectPtr<UButton> StaleTarget = PreviewOwner;
	PreviewOwner = nullptr;
	CollectGarbage(RF_NoFlags, true);
	TestTrue(
		TEXT("The discarded pointer target becomes a stale weak object"),
		StaleTarget.IsStale());
	TestTrue(
		TEXT("The screen still tracks the stale target identity before cleanup"),
		Screen->ActivePointerDropTarget.IsStale());

	Session->SetPreviewTarget(
		PreviewTarget,
		ERpgInventoryInteractionPreviewState::Move);
	Session->SetSpatialPreviewDescriptor(SpatialPreview);
	Screen->SwitchActivePointerDropTarget(nullptr);
	TestFalse(
		TEXT("Null target switching resets a stale weak target"),
		Screen->ActivePointerDropTarget.IsStale());
	TestEqual(
		TEXT("Stale target cleanup clears the semantic preview"),
		Session->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::None);
	TestFalse(
		TEXT("Stale target cleanup clears its spatial descriptor"),
		Session->GetSpatialPreviewDescriptor().bValid);

	Session->SetPreviewTarget(
		PreviewTarget,
		ERpgInventoryInteractionPreviewState::Move);
	Session->SetSpatialPreviewDescriptor(SpatialPreview);
	TestEqual(
		TEXT("The retained payload can publish a target again after re-entry"),
		Session->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::Move);
	TestTrue(
		TEXT("The retained payload can publish its spatial candidate again after re-entry"),
		Session->GetSpatialPreviewDescriptor().bValid);

	const FGeometry EmptyDropGeometry;
	TestFalse(
		TEXT("A release without an addressed target remains unhandled"),
		Screen->NativeOnDrop(
			EmptyDropGeometry,
			DragDropEvent,
			Operation));
	TestTrue(
		TEXT("An unhandled release leaves payload teardown to DragCancelled"),
		Session->HasPayload());

	const int32 StateCallbacksBeforeCancel =
		DelegateCounter->GetInteractionStateInvocationCount();
	const int32 PayloadCallbacksBeforeCancel =
		DelegateCounter->GetPayloadInvocationCount();

	Operation->DragCancelled_Implementation(PointerEvent);
	TestFalse(
		TEXT("Actual UMG drag cancellation clears the session payload"),
		Session->HasPayload());
	TestFalse(
		TEXT("Actual UMG drag cancellation clears the coordinator payload"),
		Coordinator->HasHeldPayload());
	TestEqual(
		TEXT("Actual cancellation releases input ownership"),
		Session->GetInputMode(),
		ERpgInventoryInteractionInputMode::None);
	TestEqual(
		TEXT("Actual cancellation publishes exactly one state transition"),
		DelegateCounter->GetInteractionStateInvocationCount(),
		StateCallbacksBeforeCancel + 1);
	TestEqual(
		TEXT("Actual cancellation publishes exactly one payload transition"),
		DelegateCounter->GetPayloadInvocationCount(),
		PayloadCallbacksBeforeCancel + 1);

	Operation->DragCancelled_Implementation(PointerEvent);
	Coordinator->ForceCancelInteraction();
	TestEqual(
		TEXT("Repeated Slate and screen teardown cancellation is state-idempotent"),
		DelegateCounter->GetInteractionStateInvocationCount(),
		StateCallbacksBeforeCancel + 1);
	TestEqual(
		TEXT("Repeated Slate and screen teardown cancellation is payload-idempotent"),
		DelegateCounter->GetPayloadInvocationCount(),
		PayloadCallbacksBeforeCancel + 1);

	Operation->SynchronizeFromInteractionSession();
	TestFalse(
		TEXT("The operation synchronizes to an empty payload after cancellation"),
		URpgInventoryDragDropCoordinator::IsPayloadValid(
			Operation->InventoryPayload));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPointerAddressedReentryCommitTest,
	"SurvivalRpg.Inventory.UI.Pointer.AddressedReentryCommit",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPointerAddressedReentryCommitTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPointerRegressionTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT("Addressed pointer fixture initializes an owning player"),
			TestWorld.InitializePlayerFixture()))
	{
		return false;
	}

	ARpgInventoryAutomationTestPlayerController* Controller =
		TestWorld.GetController();
	URpgInventoryManagerComponent* Inventory =
		TestWorld.GetPlayerInventory();
	const FRpgInventoryContainerHandle Pockets =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	URpgInventoryItemInstance* Item = Inventory
		? Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(Pockets, 0, 0))
		: nullptr;
	FRpgInventoryDragPayload Payload;
	if (!TestNotNull(
			TEXT("Addressed pointer fixture owns the real player inventory"),
			Inventory) ||
		!TestNotNull(
			TEXT("Addressed pointer fixture places one real source item"),
			Item) ||
		!TestTrue(
			TEXT("Addressed pointer fixture captures the exact owned entry"),
			MakeOwnedPointerPayload(Inventory, Item, Payload)))
	{
		return false;
	}

	TStrongObjectPtr<URpgInventoryPointerAutomationTestScreen> ScreenHolder(
		CreateWidget<URpgInventoryPointerAutomationTestScreen>(
			Controller,
			URpgInventoryPointerAutomationTestScreen::StaticClass()));
	URpgInventoryPointerAutomationTestScreen* Screen =
		ScreenHolder.Get();
	if (!TestNotNull(
			TEXT("Transient addressed-target screen initializes"),
			Screen))
	{
		return false;
	}

	Screen->EnsureTestInteractionObjects();
	URpgInventoryDragDropCoordinator* Coordinator =
		Screen->GetTestDragDropCoordinator();
	URpgInventoryInteractionSession* Session = Coordinator
		? Coordinator->GetInteractionSession()
		: nullptr;
	if (!TestNotNull(
			TEXT("Transient screen owns the production drag/drop coordinator"),
			Coordinator) ||
		!TestNotNull(
			TEXT("Transient screen owns the production interaction session"),
			Session) ||
		!TestTrue(
			TEXT("Production coordinator starts the real pointer payload"),
			Coordinator && Coordinator->BeginPointerDrag(Payload)))
	{
		return false;
	}

	FRpgInventoryDropTarget Target;
	Target.TargetType = ERpgInventoryDropTargetType::InventorySlot;
	Target.TargetInventory = Inventory;
	Target.TargetPlacement = Payload.SourcePlacement;
	Target.TargetPlacement.X = 2;
	Target.TargetPlacement.Y = 0;
	UButton* TargetOwner = NewObject<UButton>(Screen);
	Screen->ConfigureAddressedTarget(TargetOwner, Target);

	TStrongObjectPtr<URpgInventoryDragDropOperation> OperationHolder(
		NewObject<URpgInventoryDragDropOperation>(Screen));
	URpgInventoryDragDropOperation* Operation = OperationHolder.Get();
	if (!TestNotNull(
			TEXT("Addressed routing owns a real UMG drag operation"),
			Operation) ||
		!TestNotNull(
			TEXT("Addressed routing owns a concrete target presenter"),
			TargetOwner))
	{
		return false;
	}
	Operation->InventoryPayload = Payload;
	Operation->SetInteractionSession(Session);

	const FGeometry ScreenGeometry;
	const FPointerEvent PointerEvent;
	const FDragDropEvent DragDropEvent(
		PointerEvent,
		TSharedPtr<FDragDropOperation>());
	TestTrue(
		TEXT("The first real NativeOnDragOver addresses and previews the target"),
		Screen->InvokeNativePointerDragOver(
			ScreenGeometry,
			DragDropEvent,
			Operation));
	TestEqual(
		TEXT("The first pointer entry runs exactly one preview route"),
		Screen->GetPreviewRouteCount(),
		1);
	TestEqual(
		TEXT("The addressed target publishes the accepted move preview"),
		Session->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::Move);
	TestEqual(
		TEXT("The addressed preview retains the exact destination"),
		Session->GetTarget().TargetPlacement.X,
		Target.TargetPlacement.X);

	const int32 ClearsBeforeLeave =
		Screen->GetExternalPreviewClearCount();
	Screen->InvokeNativePointerDragLeave(
		DragDropEvent,
		Operation);
	TestTrue(
		TEXT("NativeOnDragLeave clears the addressed target preview"),
		Screen->GetExternalPreviewClearCount() > ClearsBeforeLeave);
	TestEqual(
		TEXT("NativeOnDragLeave clears semantic preview state"),
		Session->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::None);
	TestTrue(
		TEXT("NativeOnDragLeave retains the mouse payload for re-entry"),
		Session->HasPayload());

	TestTrue(
		TEXT("A real NativeOnDragOver re-entry addresses the target again"),
		Screen->InvokeNativePointerDragOver(
			ScreenGeometry,
			DragDropEvent,
			Operation));
	TestEqual(
		TEXT("Pointer re-entry executes a second preview route"),
		Screen->GetPreviewRouteCount(),
		2);
	TestEqual(
		TEXT("Pointer re-entry republishes the move preview"),
		Session->GetPreviewState(),
		ERpgInventoryInteractionPreviewState::Move);

	const int32 CommitsBeforeDrop = Screen->GetCommitRouteCount();
	TestTrue(
		TEXT("The addressed real NativeOnDrop commits through the coordinator"),
		Screen->InvokeNativePointerDrop(
			ScreenGeometry,
			DragDropEvent,
			Operation));
	TestEqual(
		TEXT("NativeOnDrop executes exactly one addressed commit route"),
		Screen->GetCommitRouteCount(),
		CommitsBeforeDrop + 1);

	FRpgInventoryEntryView MovedEntry;
	for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
	{
		if (Entry.ItemId == Item->GetItemId())
		{
			MovedEntry = Entry;
			break;
		}
	}
	TestEqual(
		TEXT("The successful NativeOnDrop moves the authoritative entry to the addressed X cell"),
		MovedEntry.Placement.X,
		Target.TargetPlacement.X);
	TestEqual(
		TEXT("The successful NativeOnDrop preserves the addressed Y cell"),
		MovedEntry.Placement.Y,
		Target.TargetPlacement.Y);
	TestFalse(
		TEXT("Synchronous authoritative acknowledgement releases the pointer payload"),
		Session->HasPayload());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPointerSpatialItemPresentationTest,
	"SurvivalRpg.Inventory.UI.Pointer.SpatialItemPresentation",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPointerSpatialItemPresentationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPointerRegressionTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT("Standalone presentation-test world is valid"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventorySpatialItemWidget* SpatialItem =
		CreateWidget<URpgInventorySpatialItemWidget>(
			TestWorld.GetWorld(),
			URpgInventorySpatialItemWidget::StaticClass());
	URpgInventoryDragVisualWidget* ItemVisual =
		CreateWidget<URpgInventoryDragVisualWidget>(
			TestWorld.GetWorld(),
			URpgInventoryDragVisualWidget::StaticClass());
	if (!TestNotNull(
			TEXT("Native spatial-item presenter initializes"),
			SpatialItem) ||
		!TestNotNull(
			TEXT("Native item visual initializes"),
			ItemVisual))
	{
		return false;
	}

	SpatialItem->ItemVisual = ItemVisual;
	SpatialItem->SetEntryFilterOpacity(0.40f);
	TestTrue(
		TEXT("Filter opacity is applied in the normal state"),
		FMath::IsNearlyEqual(
			SpatialItem->AppliedContentOpacity,
			0.40f,
			KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Normal item visual receives filter opacity"),
		FMath::IsNearlyEqual(
			ItemVisual->GetRenderOpacity(),
			0.40f,
			KINDA_SMALL_NUMBER));

	SpatialItem->CurrentDragDropVisualState =
		ERpgInventorySlotDragVisualState::HeldSource;
	SpatialItem->ApplyNativePresentationStyle();
	const float ExpectedHeldOpacity =
		0.40f * SpatialItem->HeldSourceOpacity;
	TestTrue(
		TEXT("Held-source default remains the requested strong 30 percent dim"),
		FMath::IsNearlyEqual(
			SpatialItem->HeldSourceOpacity,
			0.30f,
			KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Held-source and filter opacity compose multiplicatively"),
		FMath::IsNearlyEqual(
			SpatialItem->AppliedContentOpacity,
			ExpectedHeldOpacity,
			KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Composed held-source opacity reaches the item visual"),
		FMath::IsNearlyEqual(
			ItemVisual->GetRenderOpacity(),
			ExpectedHeldOpacity,
			KINDA_SMALL_NUMBER));

	SpatialItem->CurrentDragDropVisualState =
		ERpgInventorySlotDragVisualState::Normal;
	SpatialItem->ApplyNativePresentationStyle();
	TestTrue(
		TEXT("Returning to normal removes only held-source dimming"),
		FMath::IsNearlyEqual(
			SpatialItem->AppliedContentOpacity,
			0.40f,
			KINDA_SMALL_NUMBER));

	SpatialItem->CurrentDragDropVisualState =
		ERpgInventorySlotDragVisualState::HeldSource;
	SpatialItem->ApplyNativePresentationStyle();
	SpatialItem->ReleaseSpatialItemState();
	TestEqual(
		TEXT("Pooling release restores the normal drag state"),
		SpatialItem->CurrentDragDropVisualState,
		ERpgInventorySlotDragVisualState::Normal);
	TestTrue(
		TEXT("Pooling release clears stale filter dimming"),
		FMath::IsNearlyEqual(
			SpatialItem->EntryFilterOpacity,
			1.0f,
			KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Pooling release restores the calculated content opacity"),
		FMath::IsNearlyEqual(
			SpatialItem->AppliedContentOpacity,
			1.0f,
			KINDA_SMALL_NUMBER));
	TestTrue(
		TEXT("Pooling release restores the item visual opacity"),
		FMath::IsNearlyEqual(
			ItemVisual->GetRenderOpacity(),
			1.0f,
			KINDA_SMALL_NUMBER));

	SpatialItem->ReleaseSpatialItemState();
	TestTrue(
		TEXT("Repeated pooling release remains idempotently opaque"),
		FMath::IsNearlyEqual(
			SpatialItem->AppliedContentOpacity,
			1.0f,
			KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPointerAnchorScaleRegressionTest,
	"SurvivalRpg.Inventory.UI.Pointer.AnchorScale2x2",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPointerAnchorScaleRegressionTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryPointerRegressionTests;

	struct FScaleCase
	{
		const TCHAR* Name;
		float SourceCellSize;
		float SourceCellPadding;
		float SourceLayoutScale;
		float TargetCellSize;
		float TargetCellPadding;
		float TargetLayoutScale;
	};

	const FScaleCase ScaleCases[] = {
		{
			TEXT("CompactSourceToLargeTarget"),
			52.0f,
			1.0f,
			0.75f,
			92.0f,
			4.0f,
			1.40f
		},
		{
			TEXT("LargeSourceToCompactTarget"),
			88.0f,
			3.0f,
			1.60f,
			46.0f,
			2.0f,
			0.80f
		}
	};
	const FRpgInventoryGridSize Footprint = MakeFootprint(2, 2);
	const FIntPoint ExpectedOrigin(3, 2);

	for (const FScaleCase& ScaleCase : ScaleCases)
	{
		const FVector2D SourceLocalSize =
			URpgInventoryDragVisualWidget::CalculateExactVisualSize(
				Footprint,
				false,
				ScaleCase.SourceCellSize,
				ScaleCase.SourceCellPadding);
		const FGeometry SourceGeometry = FGeometry::MakeRoot(
			SourceLocalSize,
			FSlateLayoutTransform(
				ScaleCase.SourceLayoutScale,
				FVector2D(115.0f, 73.0f)));

		const FGeometry TargetGeometry = FGeometry::MakeRoot(
			FVector2D(1200.0f, 800.0f),
			FSlateLayoutTransform(
				ScaleCase.TargetLayoutScale,
				FVector2D(780.0f, 95.0f)));
		const float TargetStride =
			ScaleCase.TargetCellSize +
			ScaleCase.TargetCellPadding;
		const FVector2D ExpectedTargetTopLeft(
			ExpectedOrigin.X * TargetStride,
			ExpectedOrigin.Y * TargetStride);

		for (int32 GrabY = 0; GrabY < 2; ++GrabY)
		{
			for (int32 GrabX = 0; GrabX < 2; ++GrabX)
			{
				const FString CaseLabel = FString::Printf(
					TEXT("%s grabbed cell (%d,%d)"),
					ScaleCase.Name,
					GrabX,
					GrabY);
				const FVector2D SourceLocalPointer(
					(static_cast<float>(GrabX) + 0.5f) *
						SourceLocalSize.X / 2.0f,
					(static_cast<float>(GrabY) + 0.5f) *
						SourceLocalSize.Y / 2.0f);

				FRpgInventoryDragPayload Payload;
				Payload.ItemFootprint = Footprint;
				URpgInventoryDragDropCoordinator::CapturePointerDragAnchor(
					Payload,
					SourceLocalPointer,
					SourceLocalSize);
				const FVector2D SourceScreenTopLeft =
					SourceGeometry.LocalToAbsolute(
						FVector2D::ZeroVector);
				const FVector2D SourceScreenSize =
					SourceGeometry.LocalToAbsolute(SourceLocalSize) -
					SourceScreenTopLeft;
				const FVector2D SourcePointerScreen =
					SourceGeometry.LocalToAbsolute(
						SourceLocalPointer);
				URpgInventoryDragDropCoordinator::
					CapturePointerDragAnchorScreenGeometry(
						Payload,
						SourceScreenTopLeft,
						SourcePointerScreen,
						SourceScreenSize);

				TestTrue(
					*(CaseLabel + TEXT(": anchor is valid")),
					Payload.DragAnchor.bValid);
				TestEqual(
					*(CaseLabel + TEXT(": grabbed cell X")),
					Payload.DragAnchor.GrabbedCell.X,
					GrabX);
				TestEqual(
					*(CaseLabel + TEXT(": grabbed cell Y")),
					Payload.DragAnchor.GrabbedCell.Y,
					GrabY);
				TestTrue(
					*(CaseLabel + TEXT(": screen geometry records source layout scale")),
					Payload.DragAnchor.SourceScreenVisualSize.Equals(
						SourceScreenSize,
						KINDA_SMALL_NUMBER));

				const FVector2D TargetGrabPixels =
					URpgInventoryDragDropCoordinator::ResolveTargetGrabPixels(
						Payload,
						false,
						ScaleCase.TargetCellSize,
						ScaleCase.TargetCellPadding);
				const FVector2D PointerScreen =
					TargetGeometry.LocalToAbsolute(
						ExpectedTargetTopLeft +
						TargetGrabPixels);
				const FVector2D ReconstructedTopLeft =
					TargetGeometry.AbsoluteToLocal(PointerScreen) -
					TargetGrabPixels;
				const FIntPoint ResolvedOrigin(
					FMath::RoundToInt(
						ReconstructedTopLeft.X / TargetStride),
					FMath::RoundToInt(
						ReconstructedTopLeft.Y / TargetStride));

				TestTrue(
					*(CaseLabel + TEXT(": target-local top-left survives source and target scales")),
					ReconstructedTopLeft.Equals(
						ExpectedTargetTopLeft,
						KINDA_SMALL_NUMBER));
				TestEqual(
					*(CaseLabel + TEXT(": resolved target placement X")),
					ResolvedOrigin.X,
					ExpectedOrigin.X);
				TestEqual(
					*(CaseLabel + TEXT(": resolved target placement Y")),
					ResolvedOrigin.Y,
					ExpectedOrigin.Y);
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
