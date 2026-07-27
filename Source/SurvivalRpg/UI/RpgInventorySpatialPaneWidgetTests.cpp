#include "RpgInventorySpatialPaneWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryPanelViewModel.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"
#include "View/MVVMViewClass.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"

namespace RpgInventorySpatialPaneWidgetTests
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
			SpawnParameters.Name =
				MakeUniqueObjectName(World, AActor::StaticClass(), FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			AActor* OwnerActor = World->SpawnActor<AActor>(SpawnParameters);
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

	UClass* LoadSpatialPaneWidgetClass()
	{
		return LoadClass<URpgInventorySpatialPaneWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
				"CUI_SpatialInventoryPane.CUI_SpatialInventoryPane_C"));
	}

	UClass* LoadSpatialGridWidgetClass()
	{
		return LoadClass<URpgInventorySpatialGridWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
				"CUI_SpatialInventoryGrid.CUI_SpatialInventoryGrid_C"));
	}

	int32 CountDirectPanelViewModels(const UObject* Outer)
	{
		TArray<UObject*> DirectChildren;
		GetObjectsWithOuter(Outer, DirectChildren, EGetObjectsFlags::None);

		int32 Count = 0;
		for (const UObject* Candidate : DirectChildren)
		{
			if (Candidate && Candidate->IsA<URpgInventoryPanelViewModel>())
			{
				++Count;
			}
		}
		return Count;
	}

	template <typename DelegateType>
	int32 CountDelegateBindingsTo(
		const DelegateType& Delegate,
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
	FRpgInventorySpatialPaneCompositionTest,
	"SurvivalRpg.Inventory.UI.SpatialPaneComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventorySpatialPaneCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventorySpatialPaneWidgetTests;

	UClass* PaneWidgetClass = LoadSpatialPaneWidgetClass();
	UClass* SpatialGridWidgetClass = LoadSpatialGridWidgetClass();
	if (!TestNotNull(TEXT("Canonical spatial-pane class loads"), PaneWidgetClass) ||
		!TestNotNull(TEXT("Canonical authored spatial-grid class loads"), SpatialGridWidgetClass))
	{
		return false;
	}

	TestTrue(
		TEXT("Canonical pane derives from the passive native spatial-pane presenter"),
		PaneWidgetClass->IsChildOf(URpgInventorySpatialPaneWidget::StaticClass()));
	TestFalse(
		TEXT("Reusable pane does not own a CommonUI screen lifecycle"),
		PaneWidgetClass->IsChildOf(UCommonActivatableWidget::StaticClass()));
	TestFalse(
		TEXT("Reusable pane cannot receive screen payloads"),
		PaneWidgetClass->ImplementsInterface(
			URpgUIScreenPayloadReceiver::StaticClass()));

	UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(PaneWidgetClass);
	if (!TestNotNull(
		TEXT("Canonical pane is an authored Widget Blueprint"),
		GeneratedClass))
	{
		return false;
	}

	TestNull(
		TEXT("Graph-free pane compiles no UberGraph function"),
		GeneratedClass->UberGraphFunction.Get());
	TestNull(
		TEXT("Graph-free pane allocates no persistent UberGraph frame"),
		GeneratedClass->UberGraphFramePointerProperty);

	int32 DirectBlueprintFunctionCount = 0;
	for (TFieldIterator<UFunction> FunctionIt(
			 GeneratedClass,
			 EFieldIteratorFlags::ExcludeSuper);
		 FunctionIt;
		 ++FunctionIt)
	{
		++DirectBlueprintFunctionCount;
	}
	TestEqual(
		TEXT("Graph-free pane compiles no Blueprint-owned functions"),
		DirectBlueprintFunctionCount,
		0);
	TestEqual(
		TEXT("Pane owns no ambiguous compiled MVVM extension"),
		GeneratedClass->GetExtensions(
			UMVVMViewClass::StaticClass(),
			false).Num(),
		0);

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventorySpatialPaneWidget* Pane =
		CreateWidget<URpgInventorySpatialPaneWidget>(
			TestWorld.GetTestWorld(),
			PaneWidgetClass);
	if (!TestNotNull(TEXT("Canonical spatial pane initializes"), Pane))
	{
		return false;
	}

	TestNull(
		TEXT("Runtime pane deliberately owns no MVVM view"),
		UMVVMSubsystem::GetViewFromUserWidget(Pane));
	TestEqual(
		TEXT("Unbound pane starts collapsed"),
		Pane->GetVisibility(),
		ESlateVisibility::Collapsed);

	URpgInventorySpatialGridWidget* SpatialGrid = Pane->GetSpatialGrid();
	if (!TestNotNull(TEXT("Pane exposes its exact authored SpatialGrid"), SpatialGrid))
	{
		return false;
	}
	TestEqual(
		TEXT("SpatialGrid uses the canonical authored grid class"),
		SpatialGrid->GetClass(),
		SpatialGridWidgetClass);
	TestEqual(
		TEXT("SpatialGrid is the pane's single authored root"),
		Pane->WidgetTree ? Pane->WidgetTree->RootWidget.Get() : nullptr,
		static_cast<UWidget*>(SpatialGrid));

	TArray<UWidget*> AuthoredWidgets;
	if (Pane->WidgetTree)
	{
		Pane->WidgetTree->GetAllWidgets(AuthoredWidgets);
	}
	TestEqual(
		TEXT("Pane composition contains exactly one authored widget"),
		AuthoredWidgets.Num(),
		1);

	int32 SpatialGridCount = 0;
	for (const UWidget* AuthoredWidget : AuthoredWidgets)
	{
		if (AuthoredWidget &&
			AuthoredWidget->IsA<URpgInventorySpatialGridWidget>())
		{
			++SpatialGridCount;
		}
	}
	TestEqual(
		TEXT("Pane composition contains exactly one spatial grid"),
		SpatialGridCount,
		1);

	const FObjectPropertyBase* SpatialGridProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgInventorySpatialPaneWidget::StaticClass(),
			TEXT("SpatialGrid"));
	TestTrue(
		TEXT("Authored SpatialGrid binds into the native pane property"),
		SpatialGridProperty &&
			SpatialGridProperty->GetObjectPropertyValue_InContainer(Pane) ==
				SpatialGrid);

	URpgInventoryPanelViewModel* PanelViewModel =
		Pane->GetPanelViewModel();
	if (!TestNotNull(
		TEXT("Native pane creates its stable read-only panel view model"),
		PanelViewModel))
	{
		return false;
	}
	TestEqual(
		TEXT("Panel view model is owned by the pane"),
		PanelViewModel->GetOuter(),
		static_cast<UObject*>(Pane));
	TestEqual(
		TEXT("Pane owns exactly one direct panel view model"),
		CountDirectPanelViewModels(Pane),
		1);
	TestNull(
		TEXT("Fresh panel view model observes no gameplay inventory"),
		PanelViewModel->GetObservedInventory());
	TestFalse(
		TEXT("Fresh panel view model has no container filter"),
		PanelViewModel->GetContainerFilter().IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventorySpatialPaneLifecycleTest,
	"SurvivalRpg.Inventory.UI.SpatialPaneLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventorySpatialPaneLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RpgInventorySpatialPaneWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(TEXT("Standalone widget world is valid"), TestWorld.IsValid()))
	{
		return false;
	}

	UClass* PaneWidgetClass = LoadSpatialPaneWidgetClass();
	if (!TestNotNull(TEXT("Canonical spatial-pane class loads"), PaneWidgetClass))
	{
		return false;
	}

	URpgInventorySpatialPaneWidget* FirstPane =
		CreateWidget<URpgInventorySpatialPaneWidget>(
			TestWorld.GetTestWorld(),
			PaneWidgetClass);
	URpgInventorySpatialPaneWidget* SecondPane =
		CreateWidget<URpgInventorySpatialPaneWidget>(
			TestWorld.GetTestWorld(),
			PaneWidgetClass);
	if (!TestNotNull(TEXT("First spatial pane initializes"), FirstPane) ||
		!TestNotNull(TEXT("Second spatial pane initializes"), SecondPane))
	{
		return false;
	}

	TSharedPtr<SWidget> FirstSlateWidget = FirstPane->TakeWidget();
	TSharedPtr<SWidget> SecondSlateWidget = SecondPane->TakeWidget();
	if (!TestTrue(
			TEXT("First pane constructs its authored Slate representation"),
			FirstSlateWidget.IsValid()) ||
		!TestTrue(
			TEXT("Second pane constructs its authored Slate representation"),
			SecondSlateWidget.IsValid()))
	{
		return false;
	}

	URpgInventorySpatialGridWidget* FirstGrid =
		FirstPane->GetSpatialGrid();
	URpgInventorySpatialGridWidget* SecondGrid =
		SecondPane->GetSpatialGrid();
	URpgInventoryPanelViewModel* FirstViewModel =
		FirstPane->GetPanelViewModel();
	URpgInventoryPanelViewModel* SecondViewModel =
		SecondPane->GetPanelViewModel();
	if (!TestNotNull(TEXT("First pane owns a spatial grid"), FirstGrid) ||
		!TestNotNull(TEXT("Second pane owns a spatial grid"), SecondGrid) ||
		!TestNotNull(TEXT("First pane owns a panel view model"), FirstViewModel) ||
		!TestNotNull(TEXT("Second pane owns a panel view model"), SecondViewModel))
	{
		return false;
	}

	TestNotEqual(
		TEXT("Two pane instances never share their read-only projection"),
		FirstViewModel,
		SecondViewModel);
	TestEqual(
		TEXT("First VM remains owned by the first pane"),
		FirstViewModel->GetOuter(),
		static_cast<UObject*>(FirstPane));
	TestEqual(
		TEXT("Second VM remains owned by the second pane"),
		SecondViewModel->GetOuter(),
		static_cast<UObject*>(SecondPane));

	URpgInventoryManagerComponent* FirstInventory =
		TestWorld.CreateInventory(TEXT("SpatialPaneInventoryA"));
	URpgInventoryManagerComponent* SecondInventory =
		TestWorld.CreateInventory(TEXT("SpatialPaneInventoryB"));
	if (!TestNotNull(TEXT("First inventory exists"), FirstInventory) ||
		!TestNotNull(TEXT("Second inventory exists"), SecondInventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle FirstHandle =
		FRpgInventoryContainerHandle::MakeRoot(
			FirstInventory->GetDefaultContainerId());
	const FRpgInventoryContainerHandle SecondHandle =
		FRpgInventoryContainerHandle::MakeRoot(
			SecondInventory->GetDefaultContainerId());
	const FName FirstPanelId(TEXT("FirstSpatialPane"));
	const FName SecondPanelId(TEXT("SecondSpatialPane"));

	URpgInventoryDragDropCoordinator* Coordinator =
		URpgInventoryDragDropCoordinator::
			CreateInventoryDragDropCoordinator(FirstPane, nullptr);
	URpgInventoryPanelNavigationCoordinator* Navigator =
		URpgInventoryPanelNavigationCoordinator::
			CreateInventoryPanelNavigationCoordinator(
				FirstPane,
				nullptr,
				Coordinator);
	if (!TestNotNull(
			TEXT("Screen-supplied drag/drop coordinator exists"),
			Coordinator) ||
		!TestNotNull(
			TEXT("Screen-supplied navigation coordinator exists"),
			Navigator))
	{
		return false;
	}

	FirstPane->SetInteractionContext(
		Coordinator,
		Navigator,
		FirstPanelId,
		nullptr);
	FirstPane->BindInventoryContainer(FirstInventory, FirstHandle);
	FirstPane->SetInteractionContext(
		Coordinator,
		Navigator,
		FirstPanelId,
		nullptr);
	FirstPane->BindInventoryContainer(FirstInventory, FirstHandle);

	TestEqual(
		TEXT("First pane exposes its exact bound inventory"),
		FirstPane->GetBoundInventory(),
		FirstInventory);
	TestEqual(
		TEXT("First pane retains the exact graph address"),
		FirstPane->GetBoundContainerHandle(),
		FirstHandle);
	TestEqual(
		TEXT("First pane retains its semantic navigation id"),
		FirstPane->GetPanelId(),
		FirstPanelId);
	TestEqual(
		TEXT("Bound pane becomes visible without taking child hit testing"),
		FirstPane->GetVisibility(),
		ESlateVisibility::SelfHitTestInvisible);
	TestEqual(
		TEXT("First VM observes the first inventory"),
		FirstViewModel->GetObservedInventory(),
		FirstInventory);
	TestEqual(
		TEXT("First VM uses the exact container filter"),
		FirstViewModel->GetContainerFilter(),
		FirstHandle);
	TestEqual(
		TEXT("Idempotent binding retains the stable pane-owned VM"),
		FirstPane->GetPanelViewModel(),
		FirstViewModel);
	TestEqual(
		TEXT("Idempotent binding leaves one VM refresh delegate on the grid"),
		CountDelegateBindingsTo(
			FirstViewModel->OnEntriesChanged,
			FirstGrid),
		1);
	TestEqual(
		TEXT("Repeated interaction context leaves one coordinator delegate on the grid"),
		CountDelegateBindingsTo(
			Coordinator->OnHeldPayloadChanged,
			FirstGrid),
		1);

	Navigator->BeginPanelRefresh();
	FirstPane->RegisterNavigationPanel();
	FirstPane->RegisterNavigationPanel();
	Navigator->EndPanelRefresh();
	TestEqual(
		TEXT("Registered pane becomes the active navigation panel"),
		Navigator->GetActivePanelId(),
		FirstPanelId);
	TestEqual(
		TEXT("Navigator routes to the pane's exact spatial grid"),
		Navigator->GetActiveSpatialGridWidget(),
		FirstGrid);
	TestEqual(
		TEXT("Navigator routes actions to the pane's bound inventory"),
		Navigator->GetActiveInventory(),
		FirstInventory);
	TestFalse(
		TEXT("Repeated registration does not duplicate the first panel"),
		Navigator->ActivatePanelByIndex(1));

	SecondPane->SetInteractionContext(
		Coordinator,
		Navigator,
		SecondPanelId,
		nullptr);
	SecondPane->BindInventoryContainer(
		SecondInventory,
		SecondHandle);
	SecondPane->RegisterNavigationPanel();
	SecondPane->RegisterNavigationPanel();
	TestFalse(
		TEXT("Repeated registration does not duplicate the second panel"),
		Navigator->ActivatePanelByIndex(2));
	TestTrue(
		TEXT("Navigator can activate the independently registered second pane"),
		Navigator->ActivatePanelById(SecondPanelId));
	TestEqual(
		TEXT("Second panel activation resolves the second grid"),
		Navigator->GetActiveSpatialGridWidget(),
		SecondGrid);
	TestEqual(
		TEXT("Second panel activation resolves the second inventory"),
		Navigator->GetActiveInventory(),
		SecondInventory);
	TestEqual(
		TEXT("Second VM observes only the second inventory"),
		SecondViewModel->GetObservedInventory(),
		SecondInventory);
	TestEqual(
		TEXT("Second VM owns exactly one grid delegate"),
		CountDelegateBindingsTo(
			SecondViewModel->OnEntriesChanged,
			SecondGrid),
		1);
	TestEqual(
		TEXT("Shared coordinator owns one delegate for the second grid"),
		CountDelegateBindingsTo(
			Coordinator->OnHeldPayloadChanged,
			SecondGrid),
		1);

	Navigator->ClearPanels();
	TestEqual(
		TEXT("Screen-owned navigator clears its active panel before pane release"),
		Navigator->GetActivePanelIndex(),
		INDEX_NONE);
	TestNull(
		TEXT("Cleared navigator retains no active spatial grid"),
		Navigator->GetActiveSpatialGridWidget());

	FirstPane->ReleaseInventoryPresentation();
	TestNull(
		TEXT("Released pane drops its observed gameplay inventory"),
		FirstPane->GetBoundInventory());
	TestFalse(
		TEXT("Released pane drops its exact graph address"),
		FirstPane->GetBoundContainerHandle().IsValid());
	TestTrue(
		TEXT("Released pane drops its screen-owned navigation id"),
		FirstPane->GetPanelId().IsNone());
	TestEqual(
		TEXT("Released pane becomes collapsed"),
		FirstPane->GetVisibility(),
		ESlateVisibility::Collapsed);
	TestEqual(
		TEXT("Release retains the stable pane-owned VM"),
		FirstPane->GetPanelViewModel(),
		FirstViewModel);
	TestNull(
		TEXT("Released VM no longer observes gameplay state"),
		FirstViewModel->GetObservedInventory());
	TestFalse(
		TEXT("Released VM no longer filters a container"),
		FirstViewModel->GetContainerFilter().IsValid());
	TestEqual(
		TEXT("Released VM no longer targets its grid"),
		CountDelegateBindingsTo(
			FirstViewModel->OnEntriesChanged,
			FirstGrid),
		0);
	TestEqual(
		TEXT("Released grid no longer targets the shared coordinator"),
		CountDelegateBindingsTo(
			Coordinator->OnHeldPayloadChanged,
			FirstGrid),
		0);

	TestEqual(
		TEXT("Releasing the first pane does not unbind the second pane"),
		SecondPane->GetBoundInventory(),
		SecondInventory);
	TestEqual(
		TEXT("Releasing the first pane preserves the second exact graph address"),
		SecondPane->GetBoundContainerHandle(),
		SecondHandle);
	TestEqual(
		TEXT("First-pane release preserves the second VM delegate"),
		CountDelegateBindingsTo(
			SecondViewModel->OnEntriesChanged,
			SecondGrid),
		1);
	TestEqual(
		TEXT("First-pane release preserves the second coordinator delegate"),
		CountDelegateBindingsTo(
			Coordinator->OnHeldPayloadChanged,
			SecondGrid),
		1);

	FirstPane->ReleaseInventoryPresentation();
	TestEqual(
		TEXT("Repeated release remains idempotent"),
		FirstPane->GetPanelViewModel(),
		FirstViewModel);
	TestEqual(
		TEXT("Repeated release does not create another panel VM"),
		CountDirectPanelViewModels(FirstPane),
		1);
	TestEqual(
		TEXT("Repeated release leaves no stale VM delegate"),
		CountDelegateBindingsTo(
			FirstViewModel->OnEntriesChanged,
			FirstGrid),
		0);

	const FName ReboundPanelId(TEXT("ReboundSpatialPane"));
	FirstPane->SetInteractionContext(
		Coordinator,
		Navigator,
		ReboundPanelId,
		nullptr);
	FirstPane->BindInventoryContainer(
		SecondInventory,
		SecondHandle);
	FirstPane->RegisterNavigationPanel();
	TestEqual(
		TEXT("Rebound pane reuses the same stable VM"),
		FirstPane->GetPanelViewModel(),
		FirstViewModel);
	TestEqual(
		TEXT("Rebound VM observes the replacement inventory"),
		FirstViewModel->GetObservedInventory(),
		SecondInventory);
	TestEqual(
		TEXT("Rebound VM observes the replacement graph address"),
		FirstViewModel->GetContainerFilter(),
		SecondHandle);
	TestEqual(
		TEXT("Rebound pane restores exactly one VM delegate"),
		CountDelegateBindingsTo(
			FirstViewModel->OnEntriesChanged,
			FirstGrid),
		1);
	TestEqual(
		TEXT("Rebound pane restores exactly one coordinator delegate"),
		CountDelegateBindingsTo(
			Coordinator->OnHeldPayloadChanged,
			FirstGrid),
		1);
	TestEqual(
		TEXT("Rebound navigation resolves the new semantic panel id"),
		Navigator->GetActivePanelId(),
		ReboundPanelId);
	TestEqual(
		TEXT("Rebound navigation resolves the replacement inventory"),
		Navigator->GetActiveInventory(),
		SecondInventory);
	TestEqual(
		TEXT("Rebinding the first pane does not replace the second pane's VM"),
		SecondPane->GetPanelViewModel(),
		SecondViewModel);
	TestEqual(
		TEXT("Rebinding the first pane leaves the second projection intact"),
		SecondViewModel->GetObservedInventory(),
		SecondInventory);

	Navigator->ClearPanels();
	FirstPane->ReleaseInventoryPresentation();
	SecondPane->ReleaseInventoryPresentation();
	TestEqual(
		TEXT("Final release removes the first coordinator delegate"),
		CountDelegateBindingsTo(
			Coordinator->OnHeldPayloadChanged,
			FirstGrid),
		0);
	TestEqual(
		TEXT("Final release removes the second coordinator delegate"),
		CountDelegateBindingsTo(
			Coordinator->OnHeldPayloadChanged,
			SecondGrid),
		0);
	TestEqual(
		TEXT("Final release removes the first VM delegate"),
		CountDelegateBindingsTo(
			FirstViewModel->OnEntriesChanged,
			FirstGrid),
		0);
	TestEqual(
		TEXT("Final release removes the second VM delegate"),
		CountDelegateBindingsTo(
			SecondViewModel->OnEntriesChanged,
			SecondGrid),
		0);

	FirstSlateWidget.Reset();
	SecondSlateWidget.Reset();
	return true;
}

#endif
