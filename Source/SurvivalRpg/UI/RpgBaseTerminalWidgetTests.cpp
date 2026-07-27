#include "RpgBaseTerminalWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryPanelViewModel.h"
#include "SurvivalRpg/UI/RpgBaseResourceListWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventorySpatialPaneWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonListView.h"
#include "CommonLocalPlayer.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Input/CommonBoundActionBar.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewClass.h"

namespace RpgBaseTerminalWidgetTests
{
	constexpr TCHAR BaseTerminalSpatialClassPath[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseTerminalSpatial.CUI_BaseTerminalSpatial_C");
	constexpr TCHAR SpatialPaneClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryPane.CUI_SpatialInventoryPane_C");
	constexpr TCHAR BaseResourceListSpatialClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/"
			"CUI_BaseResourceListSpatial.CUI_BaseResourceListSpatial_C");
	constexpr TCHAR BaseResourceEntryClassPath[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseResourceEntry.CUI_BaseResourceEntry_C");
	constexpr TCHAR BaseTerminalActionTablePath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Input/"
			"DT_RpgUIActions_BaseTerminal.DT_RpgUIActions_BaseTerminal");
	constexpr TCHAR FeaturedUpgradePath[] =
		TEXT(
			"/Game/SurvivalRpg/Storage/"
			"DA_Upgrade_AutoDeposit.DA_Upgrade_AutoDeposit");

	constexpr TCHAR BaseTerminalSpatialPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseTerminalSpatial");
	constexpr TCHAR SpatialPanePackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryPane");
	constexpr TCHAR SpatialGridPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryGrid");
	constexpr TCHAR BaseResourceListSpatialPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseResourceListSpatial");
	constexpr TCHAR BaseResourceEntryPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseResourceEntry");
	constexpr TCHAR LegacyBaseTerminalPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseTerminal");
	constexpr TCHAR LegacyBaseResourceListPackageName[] =
		TEXT("/Game/SurvivalRpg/UI/CUI_BaseResourceList");
	constexpr TCHAR LegacyInventoryPackageName[] =
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_Inventory");

	class FScopedWidgetWorld
	{
	public:
		struct FBaseTerminalContext
		{
			ARpgBaseCampActor* BaseCamp = nullptr;
			AActor* StationOwner = nullptr;
			URpgBaseStorageStationComponent* Station = nullptr;
			URpgBaseStorageComponent* BaseStorage = nullptr;
			URpgInventoryManagerComponent* ArmoryInventory = nullptr;
			URpgInventoryManagerComponent* PlayerInventory = nullptr;

			bool IsValid() const
			{
				return BaseCamp &&
					StationOwner &&
					Station &&
					BaseStorage &&
					ArmoryInventory &&
					PlayerInventory;
			}
		};

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
			if (!World)
			{
				return;
			}

			FActorSpawnParameters ControllerSpawnParameters;
			ControllerSpawnParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerController::StaticClass(),
				TEXT("BaseTerminalPlayerController"));
			ControllerSpawnParameters.ObjectFlags = RF_Transient;
			Controller = World->SpawnActor<
				ARpgInventoryAutomationTestPlayerController>(
				ControllerSpawnParameters);

			FActorSpawnParameters PlayerStateSpawnParameters;
			PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerState::StaticClass(),
				TEXT("BaseTerminalPlayerState"));
			PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
			PlayerState = World->SpawnActor<
				ARpgInventoryAutomationTestPlayerState>(
				PlayerStateSpawnParameters);
			LocalPlayer = NewObject<UCommonLocalPlayer>(
				GEngine,
				NAME_None,
				RF_Transient);
			if (Controller && PlayerState && LocalPlayer)
			{
				Controller->SetPlayerState(PlayerState);
				PlayerState->SetOwner(Controller);

				// InitializeStandalone creates an uninitialized dummy world, so
				// controller PostInitializeComponents has not populated the
				// player-controller list used by UMG's owning-player lookup.
				World->AddController(Controller);
				Controller->SetPlayer(LocalPlayer);
			}
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
			return World != nullptr && Controller != nullptr &&
				LocalPlayer != nullptr &&
				PlayerState != nullptr &&
				PlayerState->GetInventoryManagerComponent() != nullptr;
		}

		UWorld* GetTestWorld() const
		{
			return World;
		}

		ARpgInventoryAutomationTestPlayerController* GetPlayerController() const
		{
			return Controller;
		}

		URpgInventoryManagerComponent* CreateInventory(const TCHAR* DebugName)
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

		FBaseTerminalContext CreateBaseTerminalContext(
			const TCHAR* DebugName)
		{
			FBaseTerminalContext Result;
			if (!World)
			{
				return Result;
			}

			FActorSpawnParameters BaseSpawnParameters;
			BaseSpawnParameters.Name = MakeUniqueObjectName(
				World,
				ARpgBaseCampActor::StaticClass(),
				FName(*FString::Printf(TEXT("%s_BaseCamp"), DebugName)));
			BaseSpawnParameters.ObjectFlags = RF_Transient;
			Result.BaseCamp =
				World->SpawnActor<ARpgBaseCampActor>(BaseSpawnParameters);
			if (!Result.BaseCamp)
			{
				return Result;
			}

			FActorSpawnParameters StationSpawnParameters;
			StationSpawnParameters.Name = MakeUniqueObjectName(
				World,
				AActor::StaticClass(),
				FName(*FString::Printf(TEXT("%s_Station"), DebugName)));
			StationSpawnParameters.ObjectFlags = RF_Transient;
			Result.StationOwner =
				World->SpawnActor<AActor>(StationSpawnParameters);
			if (!Result.StationOwner)
			{
				return Result;
			}

			Result.Station = NewObject<URpgBaseStorageStationComponent>(
				Result.StationOwner,
				MakeUniqueObjectName(
					Result.StationOwner,
					URpgBaseStorageStationComponent::StaticClass(),
					TEXT("StorageStation")),
				RF_Transient);
			Result.StationOwner->AddInstanceComponent(Result.Station);
			Result.Station->RegisterComponent();
			Result.Station->SetLinkedBaseCamp(Result.BaseCamp);

			Result.BaseStorage =
				Result.BaseCamp->GetBaseStorageComponent();
			Result.ArmoryInventory =
				Result.BaseCamp->GetArmoryInventoryComponent();
			Result.PlayerInventory = PlayerState
				? PlayerState->GetInventoryManagerComponent()
				: nullptr;
			return Result;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
		TObjectPtr<ARpgInventoryAutomationTestPlayerController> Controller;
		TObjectPtr<ARpgInventoryAutomationTestPlayerState> PlayerState;
		TObjectPtr<UCommonLocalPlayer> LocalPlayer;
	};

	URpgBaseStorageScreenPayload* MakePayload(
		UObject* Outer,
		const FScopedWidgetWorld::FBaseTerminalContext& Context)
	{
		URpgBaseStorageScreenPayload* Payload =
			NewObject<URpgBaseStorageScreenPayload>(Outer);
		Payload->PrimaryInventory = Context.PlayerInventory;
		Payload->SecondaryInventory = Context.ArmoryInventory;
		Payload->ContextActor = Context.StationOwner;
		Payload->ContextComponent = Context.Station;
		Payload->PlayerInventory = Context.PlayerInventory;
		Payload->BaseStorage = Context.BaseStorage;
		Payload->ArmoryInventory = Context.ArmoryInventory;
		Payload->StationComponent = Context.Station;
		return Payload;
	}

	template <typename ObjectType>
	int32 CountDirectObjectsOfClass(const UObject* Outer)
	{
		TArray<UObject*> DirectChildren;
		GetObjectsWithOuter(Outer, DirectChildren, EGetObjectsFlags::None);

		int32 Count = 0;
		for (const UObject* Candidate : DirectChildren)
		{
			if (Candidate && Candidate->IsA<ObjectType>())
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

	int32 CountFunctionsDeclaredByClass(const UClass* Class)
	{
		int32 Count = 0;
		if (Class)
		{
			for (TFieldIterator<UFunction> FunctionIt(
					Class,
					EFieldIteratorFlags::ExcludeSuper);
				FunctionIt;
				++FunctionIt)
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseTerminalSpatialCompositionTest,
	"SurvivalRpg.Inventory.UI.BaseTerminalSpatialComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseTerminalSpatialCompositionTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgBaseTerminalWidgetTests;

	UClass* BaseTerminalClass = LoadClass<URpgBaseTerminalWidget>(
		nullptr,
		BaseTerminalSpatialClassPath);
	UClass* SpatialPaneClass = LoadClass<URpgInventorySpatialPaneWidget>(
		nullptr,
		SpatialPaneClassPath);
	UClass* BaseResourceListClass =
		LoadClass<URpgBaseResourceListWidget>(
			nullptr,
			BaseResourceListSpatialClassPath);
	if (!TestNotNull(
			TEXT("Authored Base Terminal Spatial class loads"),
			BaseTerminalClass) ||
		!TestNotNull(
			TEXT("Canonical authored Spatial Pane class loads"),
			SpatialPaneClass) ||
		!TestNotNull(
			TEXT("Canonical authored Base Resource List class loads"),
			BaseResourceListClass))
	{
		return false;
	}

	TestTrue(
		TEXT("Authored Base Terminal derives from the native terminal presenter"),
		BaseTerminalClass->IsChildOf(
			URpgBaseTerminalWidget::StaticClass()));
	TestTrue(
		TEXT("Native Base Terminal derives from the shared inventory interaction screen"),
		URpgBaseTerminalWidget::StaticClass()->IsChildOf(
			URpgInventoryInteractionScreenWidget::StaticClass()));
	TestTrue(
		TEXT("Authored Base Terminal retains the local payload receiver contract"),
		BaseTerminalClass->ImplementsInterface(
			URpgUIScreenPayloadReceiver::StaticClass()));
	TestTrue(
		TEXT("Authored Spatial Pane derives from the passive native pane"),
		SpatialPaneClass->IsChildOf(
			URpgInventorySpatialPaneWidget::StaticClass()));
	TestFalse(
		TEXT("The reusable Spatial Pane is not an activatable inventory screen"),
		SpatialPaneClass->IsChildOf(
			URpgInventoryInteractionScreenWidget::StaticClass()));
	TestFalse(
		TEXT("The reusable Spatial Pane never accepts screen payloads"),
		SpatialPaneClass->ImplementsInterface(
			URpgUIScreenPayloadReceiver::StaticClass()));
	TestTrue(
		TEXT("Authored Base Resource List derives from its typed native presenter"),
		BaseResourceListClass->IsChildOf(
			URpgBaseResourceListWidget::StaticClass()));

	UWidgetBlueprintGeneratedClass* BaseTerminalGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(BaseTerminalClass);
	UWidgetBlueprintGeneratedClass* SpatialPaneGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(SpatialPaneClass);
	UWidgetBlueprintGeneratedClass* BaseResourceListGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(BaseResourceListClass);
	if (!TestNotNull(
			TEXT("Base Terminal is an authored Widget Blueprint"),
			BaseTerminalGeneratedClass) ||
		!TestNotNull(
			TEXT("Spatial Pane is an authored Widget Blueprint"),
			SpatialPaneGeneratedClass) ||
		!TestNotNull(
			TEXT("Base Resource List is an authored Widget Blueprint"),
			BaseResourceListGeneratedClass))
	{
		return false;
	}

	UWidgetBlueprintGeneratedClass* GraphFreeClasses[] = {
		BaseTerminalGeneratedClass,
		SpatialPaneGeneratedClass,
		BaseResourceListGeneratedClass
	};
	for (UWidgetBlueprintGeneratedClass* GraphFreeClass :
		GraphFreeClasses)
	{
		TestEqual(
			*FString::Printf(
				TEXT("%s declares no compiled Blueprint graph functions"),
				*GetNameSafe(GraphFreeClass)),
			CountFunctionsDeclaredByClass(GraphFreeClass),
			0);
		TestEqual(
			*FString::Printf(
				TEXT("%s owns no screen-level MVVM extension"),
				*GetNameSafe(GraphFreeClass)),
			GraphFreeClass
				? GraphFreeClass
					->GetExtensions(
						UMVVMViewClass::StaticClass(),
						false)
					.Num()
				: INDEX_NONE,
			0);
	}

	const UWidgetTree* SpatialPaneTree =
		SpatialPaneGeneratedClass->GetWidgetTreeArchetype();
	const UWidgetTree* BaseResourceListTree =
		BaseResourceListGeneratedClass->GetWidgetTreeArchetype();
	const UWidgetTree* BaseTerminalTree =
		BaseTerminalGeneratedClass->GetWidgetTreeArchetype();
	if (!TestNotNull(
			TEXT("Spatial Pane has an authored WidgetTree"),
			SpatialPaneTree) ||
		!TestNotNull(
			TEXT("Base Resource List has an authored WidgetTree"),
			BaseResourceListTree) ||
		!TestNotNull(
			TEXT("Base Terminal has an authored WidgetTree"),
			BaseTerminalTree))
	{
		return false;
	}

	UWidget* AuthoredSpatialGrid =
		SpatialPaneTree->FindWidget(TEXT("SpatialGrid"));
	TestTrue(
		TEXT("Spatial Pane authors exactly the canonical spatial grid leaf"),
		AuthoredSpatialGrid &&
			AuthoredSpatialGrid->IsA<URpgInventorySpatialGridWidget>());
	TestNull(
		TEXT("The authored spatial grid is the Pane root without another wrapper"),
		AuthoredSpatialGrid ? AuthoredSpatialGrid->GetParent() : nullptr);

	UWidget* AuthoredResourceList =
		BaseResourceListTree->FindWidget(TEXT("ResourceList"));
	TestTrue(
		TEXT("Base Resource List authors exactly one CommonListView root"),
		AuthoredResourceList &&
			AuthoredResourceList->IsA<UCommonListView>());
	TestNull(
		TEXT("The authored CommonListView is the resource presenter root"),
		AuthoredResourceList ? AuthoredResourceList->GetParent() : nullptr);

	UOverlay* AuthoredRootOverlay = Cast<UOverlay>(
		BaseTerminalTree->FindWidget(TEXT("RootOverlay")));
	UWidget* AuthoredContentRow =
		BaseTerminalTree->FindWidget(TEXT("ContentRow"));
	UWidget* AuthoredPane =
		BaseTerminalTree->FindWidget(TEXT("PlayerInventoryPane"));
	UWidget* AuthoredBaseResourceList =
		BaseTerminalTree->FindWidget(TEXT("BaseResourceList"));
	UWidget* AuthoredDepositButton =
		BaseTerminalTree->FindWidget(TEXT("DepositAllButton"));
	UWidget* AuthoredInstallButton =
		BaseTerminalTree->FindWidget(TEXT("InstallUpgradeButton"));
	UWidget* AuthoredActionBar =
		BaseTerminalTree->FindWidget(TEXT("ActionBar"));
	UWidget* AuthoredDragVisualCanvas =
		BaseTerminalTree->FindWidget(TEXT("DragVisualCanvas"));

	TestNotNull(
		TEXT("RootOverlay is authored as the terminal root"),
		AuthoredRootOverlay);
	TestTrue(
		TEXT("ContentRow is the authored two-column composition"),
		AuthoredContentRow &&
			AuthoredContentRow->IsA<UHorizontalBox>());
	TestEqual(
		TEXT("PlayerInventoryPane uses the exact canonical Pane class"),
		AuthoredPane ? AuthoredPane->GetClass() : nullptr,
		SpatialPaneClass);
	TestEqual(
		TEXT("BaseResourceList uses the exact typed resource-list class"),
		AuthoredBaseResourceList
			? AuthoredBaseResourceList->GetClass()
			: nullptr,
		BaseResourceListClass);
	TestTrue(
		TEXT("DepositAllButton is an authored pointer button"),
		AuthoredDepositButton &&
			AuthoredDepositButton->IsA<UButton>());
	TestTrue(
		TEXT("InstallUpgradeButton is an authored pointer button"),
		AuthoredInstallButton &&
			AuthoredInstallButton->IsA<UButton>());
	TestTrue(
		TEXT("ActionBar uses CommonUI's bound action bar"),
		AuthoredActionBar &&
			AuthoredActionBar->IsA<UCommonBoundActionBar>());
	TestTrue(
		TEXT("DragVisualCanvas is the authored top-level drag host"),
		AuthoredDragVisualCanvas &&
			AuthoredDragVisualCanvas->IsA<UCanvasPanel>());
	if (AuthoredRootOverlay && AuthoredDragVisualCanvas)
	{
		TestEqual(
			TEXT("DragVisualCanvas is the final root child and renders above terminal content"),
			AuthoredRootOverlay->GetChildIndex(AuthoredDragVisualCanvas),
			AuthoredRootOverlay->GetChildrenCount() - 1);
		TestEqual(
			TEXT("DragVisualCanvas never intercepts terminal pointer input"),
			AuthoredDragVisualCanvas->GetVisibility(),
			ESlateVisibility::HitTestInvisible);
	}

	TestNull(
		TEXT("Legacy misspelled player-inventory wrapper is absent"),
		BaseTerminalTree->FindWidget(TEXT("CUI_Invenory_Player")));
	TestNull(
		TEXT("Legacy player TileView wrapper is absent"),
		BaseTerminalTree->FindWidget(
			TEXT("CUI_Inventory_PlayerInventory")));
	TestNull(
		TEXT("Legacy imperative resource-list wrapper is absent"),
		BaseTerminalTree->FindWidget(TEXT("CUI_BaseResourceList")));
	TestNull(
		TEXT("Armory is intentionally not exposed as an accidental second Pane in this slice"),
		BaseTerminalTree->FindWidget(TEXT("ArmoryInventoryPane")));

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT("Standalone widget world is valid"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgBaseTerminalWidget* Widget =
		CreateWidget<URpgBaseTerminalWidget>(
			TestWorld.GetTestWorld(),
			BaseTerminalClass);
	if (!TestNotNull(
			TEXT("Authored Base Terminal initializes"),
			Widget))
	{
		return false;
	}

	URpgInventorySpatialPaneWidget* RuntimePane =
		Cast<URpgInventorySpatialPaneWidget>(
			Widget->GetWidgetFromName(TEXT("PlayerInventoryPane")));
	URpgBaseResourceListWidget* RuntimeResourceList =
		Cast<URpgBaseResourceListWidget>(
			Widget->GetWidgetFromName(TEXT("BaseResourceList")));
	UButton* RuntimeDepositButton = Cast<UButton>(
		Widget->GetWidgetFromName(TEXT("DepositAllButton")));
	UButton* RuntimeInstallButton = Cast<UButton>(
		Widget->GetWidgetFromName(TEXT("InstallUpgradeButton")));
	UCanvasPanel* RuntimeDragVisualCanvas = Cast<UCanvasPanel>(
		Widget->GetWidgetFromName(TEXT("DragVisualCanvas")));

	TestEqual(
		TEXT("PlayerInventoryPane binds into the native presenter property"),
		Widget->GetPlayerInventoryPane(),
		RuntimePane);
	TestNotNull(
		TEXT("Runtime Pane exposes its exact authored SpatialGrid"),
		RuntimePane ? RuntimePane->GetSpatialGrid() : nullptr);
	TestNotNull(
		TEXT("Runtime Pane creates its stable native panel VM"),
		RuntimePane ? RuntimePane->GetPanelViewModel() : nullptr);
	TestNotNull(
		TEXT("Runtime resource presenter exposes its CommonListView"),
		RuntimeResourceList
			? RuntimeResourceList->GetResourceList()
			: nullptr);

	const FObjectPropertyBase* BaseResourceListProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgBaseTerminalWidget::StaticClass(),
			TEXT("BaseResourceList"));
	const FObjectPropertyBase* DepositAllButtonProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgBaseTerminalWidget::StaticClass(),
			TEXT("DepositAllButton"));
	const FObjectPropertyBase* InstallUpgradeButtonProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgBaseTerminalWidget::StaticClass(),
			TEXT("InstallUpgradeButton"));
	const FObjectPropertyBase* DragVisualCanvasProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgInventoryInteractionScreenWidget::StaticClass(),
			TEXT("DragVisualCanvas"));
	TestTrue(
		TEXT("BaseResourceList binds into the typed native presenter property"),
		BaseResourceListProperty &&
			BaseResourceListProperty
				->GetObjectPropertyValue_InContainer(Widget) ==
				RuntimeResourceList);
	TestTrue(
		TEXT("DepositAllButton binds into its native command property"),
		DepositAllButtonProperty &&
			DepositAllButtonProperty
				->GetObjectPropertyValue_InContainer(Widget) ==
				RuntimeDepositButton);
	TestTrue(
		TEXT("InstallUpgradeButton binds into its native command property"),
		InstallUpgradeButtonProperty &&
			InstallUpgradeButtonProperty
				->GetObjectPropertyValue_InContainer(Widget) ==
				RuntimeInstallButton);
	TestTrue(
		TEXT("DragVisualCanvas binds into the shared interaction-screen property"),
		DragVisualCanvasProperty &&
			DragVisualCanvasProperty
				->GetObjectPropertyValue_InContainer(Widget) ==
				RuntimeDragVisualCanvas);

	const FStructProperty* DepositActionProperty =
		FindFProperty<FStructProperty>(
			URpgBaseTerminalWidget::StaticClass(),
			TEXT("DepositAllInputAction"));
	const FStructProperty* UpgradeActionProperty =
		FindFProperty<FStructProperty>(
			URpgBaseTerminalWidget::StaticClass(),
			TEXT("InstallUpgradeInputAction"));
	const FSoftObjectProperty* FeaturedUpgradeProperty =
		FindFProperty<FSoftObjectProperty>(
			URpgBaseTerminalWidget::StaticClass(),
			TEXT("FeaturedUpgrade"));
	const URpgBaseTerminalWidget* TerminalDefaults =
		Cast<URpgBaseTerminalWidget>(
			BaseTerminalClass->GetDefaultObject());
	const FDataTableRowHandle* DepositAction =
		DepositActionProperty && TerminalDefaults
			? DepositActionProperty
				->ContainerPtrToValuePtr<FDataTableRowHandle>(
					TerminalDefaults)
			: nullptr;
	const FDataTableRowHandle* UpgradeAction =
		UpgradeActionProperty && TerminalDefaults
			? UpgradeActionProperty
				->ContainerPtrToValuePtr<FDataTableRowHandle>(
					TerminalDefaults)
			: nullptr;
	const FSoftObjectPtr* FeaturedUpgrade =
		FeaturedUpgradeProperty && TerminalDefaults
			? FeaturedUpgradeProperty
				->ContainerPtrToValuePtr<FSoftObjectPtr>(
					TerminalDefaults)
			: nullptr;
	TestEqual(
		TEXT("Deposit-all uses the authored BaseTerminal CommonUI action table"),
		DepositAction && DepositAction->DataTable
			? DepositAction->DataTable->GetPathName()
			: FString(),
		FString(BaseTerminalActionTablePath));
	TestEqual(
		TEXT("Deposit-all uses its semantic CommonUI action row"),
		DepositAction ? DepositAction->RowName : NAME_None,
		FName(TEXT("UI.BaseTerminal.DepositAll")));
	TestEqual(
		TEXT("Install-upgrade uses the authored BaseTerminal CommonUI action table"),
		UpgradeAction && UpgradeAction->DataTable
			? UpgradeAction->DataTable->GetPathName()
			: FString(),
		FString(BaseTerminalActionTablePath));
	TestEqual(
		TEXT("Install-upgrade uses its semantic CommonUI action row"),
		UpgradeAction ? UpgradeAction->RowName : NAME_None,
		FName(TEXT("UI.BaseTerminal.InstallUpgrade")));
	TestEqual(
		TEXT("Terminal authors the featured upgrade as designer data"),
		FeaturedUpgrade
			? FeaturedUpgrade->ToSoftObjectPath().ToString()
			: FString(),
		FString(FeaturedUpgradePath));

	UCommonListView* RuntimeCommonList =
		RuntimeResourceList
			? RuntimeResourceList->GetResourceList()
			: nullptr;
	const FClassProperty* EntryWidgetClassProperty =
		RuntimeCommonList
			? FindFProperty<FClassProperty>(
				RuntimeCommonList->GetClass(),
				TEXT("EntryWidgetClass"))
			: nullptr;
	const UClass* ResourceEntryClass =
		EntryWidgetClassProperty && RuntimeCommonList
			? Cast<UClass>(
				EntryWidgetClassProperty
					->GetObjectPropertyValue_InContainer(
						RuntimeCommonList))
			: nullptr;
	TestEqual(
		TEXT("Resource rows keep the existing typed entry leaf"),
		ResourceEntryClass
			? ResourceEntryClass->GetPathName()
			: FString(),
		FString(BaseResourceEntryClassPath));

	const IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry"))
			.Get();
	TArray<FName> TerminalDependencies;
	TArray<FName> PaneDependencies;
	TArray<FName> ResourceListDependencies;
	TestTrue(
		TEXT("Asset Registry resolves Base Terminal dependencies"),
		AssetRegistry.GetDependencies(
			FName(BaseTerminalSpatialPackageName),
			TerminalDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));
	TestTrue(
		TEXT("Asset Registry resolves Spatial Pane dependencies"),
		AssetRegistry.GetDependencies(
			FName(SpatialPanePackageName),
			PaneDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));
	TestTrue(
		TEXT("Asset Registry resolves Base Resource List dependencies"),
		AssetRegistry.GetDependencies(
			FName(BaseResourceListSpatialPackageName),
			ResourceListDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));

	TestTrue(
		TEXT("Base Terminal depends on the canonical reusable Spatial Pane"),
		TerminalDependencies.Contains(FName(SpatialPanePackageName)));
	TestTrue(
		TEXT("Base Terminal depends on the typed Base Resource List"),
		TerminalDependencies.Contains(
			FName(BaseResourceListSpatialPackageName)));
	TestFalse(
		TEXT("Base Terminal does not bypass the Pane by depending directly on the spatial grid"),
		TerminalDependencies.Contains(FName(SpatialGridPackageName)));
	TestFalse(
		TEXT("Base Terminal no longer depends on its legacy graph-heavy screen"),
		TerminalDependencies.Contains(
			FName(LegacyBaseTerminalPackageName)));
	TestFalse(
		TEXT("Base Terminal no longer depends on the legacy imperative resource list"),
		TerminalDependencies.Contains(
			FName(LegacyBaseResourceListPackageName)));
	TestFalse(
		TEXT("Base Terminal no longer depends on the legacy flat inventory wrapper"),
		TerminalDependencies.Contains(
			FName(LegacyInventoryPackageName)));
	TestTrue(
		TEXT("Spatial Pane alone owns the spatial-grid asset dependency"),
		PaneDependencies.Contains(FName(SpatialGridPackageName)));
	TestTrue(
		TEXT("Typed resource list alone owns the resource-entry asset dependency"),
		ResourceListDependencies.Contains(
			FName(BaseResourceEntryPackageName)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseTerminalContextLifecycleTest,
	"SurvivalRpg.Inventory.UI.BaseTerminalContextLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseTerminalContextLifecycleTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgBaseTerminalWidgetTests;

	FScopedWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT("Standalone widget world is valid"),
			TestWorld.IsValid()))
	{
		return false;
	}

	ARpgInventoryAutomationTestPlayerController* TestController =
		TestWorld.GetPlayerController();
	ARpgInventoryAutomationTestPlayerState* TestPlayerState =
		TestController
			? Cast<ARpgInventoryAutomationTestPlayerState>(
				TestController->PlayerState)
			: nullptr;
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		TestController
			? TestController->GetPlayerInventoryLayoutComponent()
			: nullptr;
	FRpgInventorySlotGroupView PrimaryContentGroup;
	if (!TestNotNull(
			TEXT("Base Terminal fixture controller owns its PlayerState"),
			TestPlayerState) ||
		!TestNotNull(
			TEXT("Base Terminal fixture controller owns its layout component"),
			InventoryLayout) ||
		!TestNotNull(
			TEXT("Base Terminal fixture layout resolves from PawnData"),
			InventoryLayout ? InventoryLayout->GetLayoutDefinition() : nullptr) ||
		!TestTrue(
			TEXT("Base Terminal fixture exposes one primary content role"),
			InventoryLayout &&
				InventoryLayout->TryGetSlotGroupBySemanticRole(
					RpgGameplayTags::Rpg_Inventory_Layout_Role_Content_Primary,
					PrimaryContentGroup)))
	{
		return false;
	}

	const FScopedWidgetWorld::FBaseTerminalContext ContextA =
		TestWorld.CreateBaseTerminalContext(TEXT("ContextA"));
	const FScopedWidgetWorld::FBaseTerminalContext ContextB =
		TestWorld.CreateBaseTerminalContext(TEXT("ContextB"));
	if (!TestTrue(
			TEXT("Base Terminal context A is complete"),
			ContextA.IsValid()) ||
		!TestTrue(
			TEXT("Base Terminal context B is complete"),
			ContextB.IsValid()))
	{
		return false;
	}

	UClass* BaseTerminalClass = LoadClass<URpgBaseTerminalWidget>(
		nullptr,
		BaseTerminalSpatialClassPath);
	if (!TestNotNull(
			TEXT("Authored Base Terminal Spatial class loads"),
			BaseTerminalClass))
	{
		return false;
	}

	URpgBaseTerminalWidget* Widget =
		CreateWidget<URpgBaseTerminalWidget>(
			TestWorld.GetPlayerController(),
			BaseTerminalClass);
	if (!TestNotNull(TEXT("Base Terminal widget exists"), Widget))
	{
		return false;
	}

	URpgPlayerInventoryLayoutComponent* WidgetInventoryLayout =
		Widget->GetOwningPlayer()
			? Widget->GetOwningPlayer()->FindComponentByClass<
				URpgPlayerInventoryLayoutComponent>()
			: nullptr;
	FRpgInventorySlotGroupView WidgetPrimaryContentGroup;
	if (!TestEqual(
			TEXT("Base Terminal widget retains the fixture player controller"),
			Widget->GetOwningPlayer(),
			static_cast<APlayerController*>(TestController)) ||
		!TestNotNull(
			TEXT("Base Terminal widget discovers the controller layout component"),
			WidgetInventoryLayout) ||
		!TestTrue(
			TEXT("Base Terminal widget-facing layout exposes the primary content role"),
			WidgetInventoryLayout &&
				WidgetInventoryLayout->TryGetSlotGroupBySemanticRole(
					RpgGameplayTags::Rpg_Inventory_Layout_Role_Content_Primary,
					WidgetPrimaryContentGroup)))
	{
		return false;
	}

	URpgInventorySpatialPaneWidget* PlayerPane =
		Widget->GetPlayerInventoryPane();
	URpgBaseResourceListWidget* ResourceList =
		Cast<URpgBaseResourceListWidget>(
			Widget->GetWidgetFromName(TEXT("BaseResourceList")));
	if (!TestNotNull(
			TEXT("Authored Player Inventory Pane is bound"),
			PlayerPane) ||
		!TestNotNull(
			TEXT("Authored Base Resource List is bound"),
			ResourceList) ||
		!TestNotNull(
			TEXT("Authored Pane exposes its spatial grid"),
			PlayerPane ? PlayerPane->GetSpatialGrid() : nullptr))
	{
		return false;
	}

	URpgInventoryPanelViewModel* PlayerPaneViewModel =
		PlayerPane->GetPanelViewModel();
	if (!TestNotNull(
			TEXT("Pane owns a stable panel view model before screen activation"),
			PlayerPaneViewModel))
	{
		return false;
	}
	TestEqual(
		TEXT("Pane VM is owned by the reusable Pane"),
		PlayerPaneViewModel->GetOuter(),
		static_cast<UObject*>(PlayerPane));
	TestEqual(
		TEXT("Pane owns exactly one direct panel VM"),
		CountDirectObjectsOfClass<URpgInventoryPanelViewModel>(
			PlayerPane),
		1);

	URpgBaseStorageScreenPayload* PayloadA =
		MakePayload(Widget, ContextA);
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadA);
	TestEqual(
		TEXT("Pre-activation Base Terminal payload is staged"),
		Widget->GetBaseStorageScreenPayload(),
		PayloadA);
	TestFalse(
		TEXT("Staging does not resolve the Pockets handle before activation"),
		Widget->GetPlayerPaneContainerHandle().IsValid());
	TestNull(
		TEXT("Staging does not bind the Player Pane"),
		PlayerPane->GetBoundInventory());
	TestNull(
		TEXT("Staging does not create the Base Storage VM"),
		Widget->GetBaseStorageViewModel());
	TestNull(
		TEXT("Staging does not create an inventory coordinator"),
		Widget->GetInventoryDragDropCoordinator());
	TestEqual(
		TEXT("Staging does not bind terminal presentation"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		0u);

	Widget->ActivateWidget();
	TestTrue(
		TEXT("Base Terminal activates through the CommonUI lifecycle"),
		Widget->IsActivated());
	TestEqual(
		TEXT("Payload delivered before activation binds exactly once"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		1u);

	URpgInventoryDragDropCoordinator* Coordinator =
		Widget->GetInventoryDragDropCoordinator();
	URpgInventoryPanelNavigationCoordinator* PanelNavigator =
		Widget->GetInventoryPanelNavigator();
	URpgBaseStorageViewModel* BaseStorageViewModel =
		Widget->GetBaseStorageViewModel();
	if (!TestNotNull(
			TEXT("Base Terminal drag/drop coordinator exists"),
			Coordinator) ||
		!TestNotNull(
			TEXT("Base Terminal panel navigator exists"),
			PanelNavigator) ||
		!TestNotNull(
			TEXT("Base Terminal owns its stable Base Storage VM"),
			BaseStorageViewModel))
	{
		return false;
	}

	const FRpgInventoryContainerHandle PocketsHandle =
		FRpgInventoryContainerHandle::MakeRoot(
			URpgPlayerInventoryLayoutComponent::PocketsGroupId);
	TestEqual(
		TEXT("Drag/drop coordinator is owned by the Base Terminal screen"),
		Coordinator->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("Panel navigator is owned by the Base Terminal screen"),
		PanelNavigator->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("Base Storage VM is owned by the Base Terminal screen"),
		BaseStorageViewModel->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("Base Terminal owns exactly one direct Base Storage VM"),
		CountDirectObjectsOfClass<URpgBaseStorageViewModel>(Widget),
		1);
	TestEqual(
		TEXT("Player Pane retains its single VM during activation"),
		PlayerPane->GetPanelViewModel(),
		PlayerPaneViewModel);
	TestEqual(
		TEXT("Player Pane observes context A's player inventory"),
		PlayerPane->GetBoundInventory(),
		ContextA.PlayerInventory);
	TestTrue(
		TEXT("Player Pane projects exactly the canonical Pockets root"),
		PlayerPane->GetBoundContainerHandle() == PocketsHandle);
	TestTrue(
		TEXT("Base Terminal exposes exactly the canonical Pockets root"),
		Widget->GetPlayerPaneContainerHandle() == PocketsHandle);
	TestEqual(
		TEXT("Pane VM observes context A's player inventory"),
		PlayerPaneViewModel->GetObservedInventory(),
		ContextA.PlayerInventory);
	TestTrue(
		TEXT("Pane VM filters exactly the Pockets root"),
		PlayerPaneViewModel->GetContainerFilter() == PocketsHandle);
	TestEqual(
		TEXT("Base Storage VM observes context A's replicated base storage"),
		BaseStorageViewModel->GetBaseStorage(),
		ContextA.BaseStorage);
	TestEqual(
		TEXT("Resource presenter receives the screen-owned Base Storage VM"),
		ResourceList->GetBaseStorageViewModel(),
		BaseStorageViewModel);
	TestEqual(
		TEXT("Resource presenter binds its VM listener exactly once"),
		CountDelegateBindingsTo(
			BaseStorageViewModel->OnResourcesChanged,
			ResourceList),
		1);
	TestEqual(
		TEXT("Spatial grid binds its Pane VM listener exactly once"),
		CountDelegateBindingsTo(
			PlayerPaneViewModel->OnEntriesChanged,
			PlayerPane->GetSpatialGrid()),
		1);
	TestEqual(
		TEXT("Base Terminal binds context A's upgrade listener exactly once"),
		CountDelegateBindingsTo(
			ContextA.Station->OnInstalledUpgradesChanged,
			Widget),
		1);
	TestEqual(
		TEXT("The one navigation panel has the terminal Pockets identity"),
		PanelNavigator->GetActivePanelId(),
		FName(TEXT("BaseTerminal.Player.Pockets")));
	TestEqual(
		TEXT("The active terminal panel reflects context A's player inventory"),
		PanelNavigator->GetActiveInventory(),
		ContextA.PlayerInventory);
	TestEqual(
		TEXT("The active terminal panel focuses context A's player inventory"),
		Coordinator->GetFocusedInventory(),
		ContextA.PlayerInventory);

	FRpgInventoryDragPayload HeldPayload;
	HeldPayload.SourceType =
		ERpgInventoryDragSourceType::InventoryEntry;
	HeldPayload.SourceInventory = ContextA.PlayerInventory;
	HeldPayload.EntryId = FGuid::NewGuid();
	HeldPayload.StackCount = 1;
	if (!TestTrue(
			TEXT("Transient Base Terminal payload owns canonical item metadata"),
			RpgInventoryAutomationTestTypes::PopulateCanonicalSpatialItem(
				HeldPayload,
				ContextA.PlayerInventory,
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass())))
	{
		return false;
	}
	TestTrue(
		TEXT("A transient interaction starts before the context switch"),
		Coordinator->BeginHold(HeldPayload));

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadA);
	TestTrue(
		TEXT("Reapplying the same payload preserves the active interaction"),
		Coordinator->HasHeldPayload());
	TestEqual(
		TEXT("Reapplying the same payload preserves the screen coordinator"),
		Widget->GetInventoryDragDropCoordinator(),
		Coordinator);
	TestEqual(
		TEXT("Reapplying the same payload preserves the panel navigator"),
		Widget->GetInventoryPanelNavigator(),
		PanelNavigator);
	TestEqual(
		TEXT("Reapplying the same payload preserves the Base Storage VM"),
		Widget->GetBaseStorageViewModel(),
		BaseStorageViewModel);
	TestEqual(
		TEXT("Reapplying the same payload preserves the Pane VM"),
		PlayerPane->GetPanelViewModel(),
		PlayerPaneViewModel);
	TestEqual(
		TEXT("Reapplying the same payload does not bind presentation again"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		1u);
	TestEqual(
		TEXT("Same-payload delivery never duplicates the resource-list listener"),
		CountDelegateBindingsTo(
			BaseStorageViewModel->OnResourcesChanged,
			ResourceList),
		1);
	TestEqual(
		TEXT("Same-payload delivery never duplicates the grid listener"),
		CountDelegateBindingsTo(
			PlayerPaneViewModel->OnEntriesChanged,
			PlayerPane->GetSpatialGrid()),
		1);
	TestEqual(
		TEXT("Same-payload delivery never duplicates the station listener"),
		CountDelegateBindingsTo(
			ContextA.Station->OnInstalledUpgradesChanged,
			Widget),
		1);

	URpgBaseStorageScreenPayload* PayloadB =
		MakePayload(Widget, ContextB);
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadB);
	TestFalse(
		TEXT("Context switch cancels the old held interaction"),
		Coordinator->HasHeldPayload());
	TestEqual(
		TEXT("Context switch replaces the retained payload"),
		Widget->GetBaseStorageScreenPayload(),
		PayloadB);
	TestEqual(
		TEXT("Context switch binds the replacement exactly once"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		2u);
	TestEqual(
		TEXT("Context switch retains the screen-owned coordinator"),
		Widget->GetInventoryDragDropCoordinator(),
		Coordinator);
	TestEqual(
		TEXT("Context switch retains the screen-owned panel navigator"),
		Widget->GetInventoryPanelNavigator(),
		PanelNavigator);
	TestEqual(
		TEXT("Context switch retains the screen-owned Base Storage VM"),
		Widget->GetBaseStorageViewModel(),
		BaseStorageViewModel);
	TestEqual(
		TEXT("Context switch retains the Pane-owned panel VM"),
		PlayerPane->GetPanelViewModel(),
		PlayerPaneViewModel);
	TestEqual(
		TEXT("Context switch removes the old station listener"),
		CountDelegateBindingsTo(
			ContextA.Station->OnInstalledUpgradesChanged,
			Widget),
		0);
	TestEqual(
		TEXT("Context switch binds the new station listener exactly once"),
		CountDelegateBindingsTo(
			ContextB.Station->OnInstalledUpgradesChanged,
			Widget),
		1);
	TestEqual(
		TEXT("Context switch keeps one resource-list listener"),
		CountDelegateBindingsTo(
			BaseStorageViewModel->OnResourcesChanged,
			ResourceList),
		1);
	TestEqual(
		TEXT("Context switch keeps one spatial-grid listener"),
		CountDelegateBindingsTo(
			PlayerPaneViewModel->OnEntriesChanged,
			PlayerPane->GetSpatialGrid()),
		1);
	TestEqual(
		TEXT("Context switch rebinds the Player Pane to context B"),
		PlayerPane->GetBoundInventory(),
		ContextB.PlayerInventory);
	TestTrue(
		TEXT("Context switch retains the exact Pockets graph address"),
		PlayerPane->GetBoundContainerHandle() == PocketsHandle);
	TestEqual(
		TEXT("Context switch rebinds the Pane VM to context B"),
		PlayerPaneViewModel->GetObservedInventory(),
		ContextB.PlayerInventory);
	TestEqual(
		TEXT("Context switch rebinds the Base VM to context B"),
		BaseStorageViewModel->GetBaseStorage(),
		ContextB.BaseStorage);
	TestEqual(
		TEXT("Context switch refreshes navigation to context B"),
		PanelNavigator->GetActiveInventory(),
		ContextB.PlayerInventory);
	TestEqual(
		TEXT("Context switch focuses only context B's player inventory"),
		Coordinator->GetFocusedInventory(),
		ContextB.PlayerInventory);

	Widget->DeactivateWidget();
	TestFalse(
		TEXT("Base Terminal deactivates through the CommonUI lifecycle"),
		Widget->IsActivated());
	TestEqual(
		TEXT("Deactivation retains the coordinator for CommonUI pooling"),
		Widget->GetInventoryDragDropCoordinator(),
		Coordinator);
	TestEqual(
		TEXT("Deactivation retains the navigator for CommonUI pooling"),
		Widget->GetInventoryPanelNavigator(),
		PanelNavigator);
	TestEqual(
		TEXT("Deactivation retains the stable Base Storage VM"),
		Widget->GetBaseStorageViewModel(),
		BaseStorageViewModel);
	TestEqual(
		TEXT("Deactivation retains the stable Pane VM"),
		PlayerPane->GetPanelViewModel(),
		PlayerPaneViewModel);
	TestNull(
		TEXT("Deactivation releases the retained payload"),
		Widget->GetBaseStorageScreenPayload());
	TestFalse(
		TEXT("Deactivation invalidates the Pockets handle"),
		Widget->GetPlayerPaneContainerHandle().IsValid());
	TestNull(
		TEXT("Deactivation releases the Player Pane inventory"),
		PlayerPane->GetBoundInventory());
	TestNull(
		TEXT("Deactivation unbinds the Pane VM"),
		PlayerPaneViewModel->GetObservedInventory());
	TestNull(
		TEXT("Deactivation unbinds the Base Storage VM"),
		BaseStorageViewModel->GetBaseStorage());
	TestNull(
		TEXT("Deactivation releases the resource presenter's VM"),
		ResourceList->GetBaseStorageViewModel());
	TestEqual(
		TEXT("Deactivation removes the resource-list listener"),
		CountDelegateBindingsTo(
			BaseStorageViewModel->OnResourcesChanged,
			ResourceList),
		0);
	TestEqual(
		TEXT("Deactivation removes the spatial-grid listener"),
		CountDelegateBindingsTo(
			PlayerPaneViewModel->OnEntriesChanged,
			PlayerPane->GetSpatialGrid()),
		0);
	TestEqual(
		TEXT("Deactivation removes the station listener"),
		CountDelegateBindingsTo(
			ContextB.Station->OnInstalledUpgradesChanged,
			Widget),
		0);
	TestEqual(
		TEXT("Deactivation clears the navigation registry"),
		PanelNavigator->GetActivePanelId(),
		NAME_None);
	TestNull(
		TEXT("Deactivation clears focused inventory"),
		Coordinator->GetFocusedInventory());
	TestEqual(
		TEXT("Deactivation does not bind presentation again"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		2u);

	Widget->ActivateWidget();
	TestTrue(
		TEXT("The pooled Base Terminal can activate again"),
		Widget->IsActivated());
	TestEqual(
		TEXT("Pool reactivation reuses the coordinator"),
		Widget->GetInventoryDragDropCoordinator(),
		Coordinator);
	TestEqual(
		TEXT("Pool reactivation reuses the navigator"),
		Widget->GetInventoryPanelNavigator(),
		PanelNavigator);
	TestEqual(
		TEXT("Pool reactivation reuses the Base Storage VM"),
		Widget->GetBaseStorageViewModel(),
		BaseStorageViewModel);
	TestEqual(
		TEXT("Pool reactivation reuses the Pane VM"),
		PlayerPane->GetPanelViewModel(),
		PlayerPaneViewModel);
	TestNull(
		TEXT("Pool reactivation does not resurrect a stale payload"),
		Widget->GetBaseStorageScreenPayload());
	TestEqual(
		TEXT("Pool reactivation without payload performs no bind"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		2u);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadB);
	TestEqual(
		TEXT("Fresh payload after pool reactivation binds exactly once"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		3u);
	TestEqual(
		TEXT("Fresh payload after pooling reuses the Base Storage VM"),
		Widget->GetBaseStorageViewModel(),
		BaseStorageViewModel);
	TestEqual(
		TEXT("Fresh payload after pooling reuses the Pane VM"),
		PlayerPane->GetPanelViewModel(),
		PlayerPaneViewModel);
	TestEqual(
		TEXT("Fresh payload after pooling restores one resource listener"),
		CountDelegateBindingsTo(
			BaseStorageViewModel->OnResourcesChanged,
			ResourceList),
		1);
	TestEqual(
		TEXT("Fresh payload after pooling restores one grid listener"),
		CountDelegateBindingsTo(
			PlayerPaneViewModel->OnEntriesChanged,
			PlayerPane->GetSpatialGrid()),
		1);
	TestEqual(
		TEXT("Fresh payload after pooling restores one station listener"),
		CountDelegateBindingsTo(
			ContextB.Station->OnInstalledUpgradesChanged,
			Widget),
		1);

	URpgBaseStorageScreenPayload* BaseMismatchPayload =
		MakePayload(Widget, ContextB);
	BaseMismatchPayload->BaseStorage = ContextA.BaseStorage;
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		BaseMismatchPayload);
	TestNull(
		TEXT("A station/base mismatch resets the active terminal context"),
		Widget->GetBaseStorageScreenPayload());
	TestNull(
		TEXT("A rejected active payload releases the Player Pane"),
		PlayerPane->GetBoundInventory());
	TestNull(
		TEXT("A rejected active payload unbinds the Base Storage VM"),
		BaseStorageViewModel->GetBaseStorage());
	TestNull(
		TEXT("A rejected active payload releases the resource presenter"),
		ResourceList->GetBaseStorageViewModel());
	TestEqual(
		TEXT("A rejected active payload removes the resource listener"),
		CountDelegateBindingsTo(
			BaseStorageViewModel->OnResourcesChanged,
			ResourceList),
		0);
	TestEqual(
		TEXT("A rejected active payload removes the grid listener"),
		CountDelegateBindingsTo(
			PlayerPaneViewModel->OnEntriesChanged,
			PlayerPane->GetSpatialGrid()),
		0);
	TestEqual(
		TEXT("A rejected active payload removes the station listener"),
		CountDelegateBindingsTo(
			ContextB.Station->OnInstalledUpgradesChanged,
			Widget),
		0);
	TestEqual(
		TEXT("A rejected payload never increments the bind generation"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		3u);

	auto ExpectRejectedPayload =
		[this,
		 Widget,
		 PlayerPane,
		 ResourceList,
		 BaseStorageViewModel](
			const TCHAR* Scenario,
			UObject* CandidatePayload)
	{
		IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
			Widget,
			CandidatePayload);
		TestNull(
			*FString::Printf(
				TEXT("%s leaves no retained payload"),
				Scenario),
			Widget->GetBaseStorageScreenPayload());
		TestFalse(
			*FString::Printf(
				TEXT("%s leaves no valid Pane handle"),
				Scenario),
			Widget->GetPlayerPaneContainerHandle().IsValid());
		TestNull(
			*FString::Printf(
				TEXT("%s leaves the Player Pane released"),
				Scenario),
			PlayerPane->GetBoundInventory());
		TestNull(
			*FString::Printf(
				TEXT("%s leaves the Base VM unbound"),
				Scenario),
			BaseStorageViewModel->GetBaseStorage());
		TestNull(
			*FString::Printf(
				TEXT("%s leaves the resource presenter released"),
				Scenario),
			ResourceList->GetBaseStorageViewModel());
		TestEqual(
			*FString::Printf(
				TEXT("%s does not bind terminal presentation"),
				Scenario),
			Widget->GetBaseTerminalPresentationBindGeneration(),
			3u);
	};

	ExpectRejectedPayload(
		TEXT("A non-terminal payload"),
		NewObject<URpgInventoryScreenPayload>(Widget));

	URpgBaseStorageScreenPayload* MissingPlayerPayload =
		MakePayload(Widget, ContextA);
	MissingPlayerPayload->PlayerInventory = nullptr;
	ExpectRejectedPayload(
		TEXT("A payload without PlayerInventory"),
		MissingPlayerPayload);

	URpgBaseStorageScreenPayload* PrimaryMismatchPayload =
		MakePayload(Widget, ContextA);
	PrimaryMismatchPayload->PrimaryInventory =
		TestWorld.CreateInventory(TEXT("MismatchedPlayerInventory"));
	ExpectRejectedPayload(
		TEXT("A payload whose PrimaryInventory duplicates another source"),
		PrimaryMismatchPayload);

	URpgBaseStorageScreenPayload* MissingStationPayload =
		MakePayload(Widget, ContextA);
	MissingStationPayload->StationComponent = nullptr;
	ExpectRejectedPayload(
		TEXT("A payload without StationComponent"),
		MissingStationPayload);

	URpgBaseStorageScreenPayload* ContextComponentMismatchPayload =
		MakePayload(Widget, ContextA);
	ContextComponentMismatchPayload->ContextComponent =
		ContextB.Station;
	ExpectRejectedPayload(
		TEXT("A payload whose ContextComponent is not its station"),
		ContextComponentMismatchPayload);

	URpgBaseStorageScreenPayload* SecondaryMismatchPayload =
		MakePayload(Widget, ContextA);
	SecondaryMismatchPayload->SecondaryInventory = nullptr;
	ExpectRejectedPayload(
		TEXT("A payload whose SecondaryInventory does not mirror ArmoryInventory"),
		SecondaryMismatchPayload);

	URpgBaseStorageScreenPayload* StationArmoryMismatchPayload =
		MakePayload(Widget, ContextA);
	StationArmoryMismatchPayload->SecondaryInventory =
		ContextB.ArmoryInventory;
	StationArmoryMismatchPayload->ArmoryInventory =
		ContextB.ArmoryInventory;
	ExpectRejectedPayload(
		TEXT("A payload whose armory is not owned by its station"),
		StationArmoryMismatchPayload);

	URpgBaseStorageScreenPayload* AliasedPlayerArmoryPayload =
		MakePayload(Widget, ContextA);
	AliasedPlayerArmoryPayload->PrimaryInventory =
		ContextA.ArmoryInventory;
	AliasedPlayerArmoryPayload->PlayerInventory =
		ContextA.ArmoryInventory;
	AliasedPlayerArmoryPayload->SecondaryInventory =
		ContextA.ArmoryInventory;
	AliasedPlayerArmoryPayload->ArmoryInventory =
		ContextA.ArmoryInventory;
	ExpectRejectedPayload(
		TEXT("A payload that aliases PlayerInventory and ArmoryInventory"),
		AliasedPlayerArmoryPayload);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadA);
	TestEqual(
		TEXT("A valid payload still binds after rejected candidates"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		4u);
	TestEqual(
		TEXT("The recovered context reuses the Base Storage VM"),
		Widget->GetBaseStorageViewModel(),
		BaseStorageViewModel);
	TestEqual(
		TEXT("The recovered context reuses the Pane VM"),
		PlayerPane->GetPanelViewModel(),
		PlayerPaneViewModel);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		nullptr);
	TestNull(
		TEXT("Clearing the payload releases the retained payload"),
		Widget->GetBaseStorageScreenPayload());
	TestFalse(
		TEXT("Clearing the payload invalidates the Pane handle"),
		Widget->GetPlayerPaneContainerHandle().IsValid());
	TestNull(
		TEXT("Clearing the payload releases the Player Pane"),
		PlayerPane->GetBoundInventory());
	TestNull(
		TEXT("Clearing the payload unbinds the Base Storage VM"),
		BaseStorageViewModel->GetBaseStorage());
	TestEqual(
		TEXT("Clearing the payload does not perform another bind"),
		Widget->GetBaseTerminalPresentationBindGeneration(),
		4u);

	Widget->DeactivateWidget();
	return true;
}

#endif
