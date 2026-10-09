#include "RpgCraftingStationWidget.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "SurvivalRpg/Crafting/RpgCraftingStationActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Mvvm/Crafting/RpgCraftingViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryPanelViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModel.h"
#include "SurvivalRpg/UI/RpgCraftingActionButtonWidget.h"
#include "SurvivalRpg/UI/RpgCraftingIngredientEntryWidget.h"
#include "SurvivalRpg/UI/RpgCraftingRecipeEntryWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/IUserListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputBaseTypes.h"
#include "ICommonInputModule.h"
#include "CommonLazyImage.h"
#include "CommonListView.h"
#include "CommonTextBlock.h"
#include "CommonUITypes.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Input/CommonBoundActionBar.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "MVVMSubsystem.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "Widgets/SWidget.h"

namespace RpgCraftingStationWidgetTests
{
	constexpr TCHAR CraftingScreenClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingStationSpatial.CUI_CraftingStationSpatial_C");
	constexpr TCHAR CraftingActionButtonClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingActionButtonSpatial.CUI_CraftingActionButtonSpatial_C");
	constexpr TCHAR RecipeEntryClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingRecipeEntrySpatial.CUI_CraftingRecipeEntrySpatial_C");
	constexpr TCHAR IngredientEntryClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingIngredientEntrySpatial.CUI_CraftingIngredientEntrySpatial_C");
	constexpr TCHAR CraftingActionTablePath[] =
		TEXT(
			"/Game/SurvivalRpg/UI/Input/"
			"DT_RpgUIActions_Crafting.DT_RpgUIActions_Crafting");

	constexpr TCHAR CraftingScreenPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingStationSpatial");
	constexpr TCHAR CraftingActionButtonPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingActionButtonSpatial");
	constexpr TCHAR RecipeEntryPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingRecipeEntrySpatial");
	constexpr TCHAR IngredientEntryPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingIngredientEntrySpatial");
	constexpr TCHAR CraftingActionTablePackageName[] =
		TEXT("/Game/SurvivalRpg/UI/Input/DT_RpgUIActions_Crafting");
	constexpr TCHAR LegacyInventoryPackageName[] =
		TEXT("/Game/SurvivalRpg/Inventory/UI/CUI_Inventory");
	constexpr TCHAR LegacyCraftingScreenPackageName[] =
		TEXT("/Game/SurvivalRpg/Crafting/UI/CUI_CraftingStation");

	class FScopedCraftingWidgetWorld
	{
	public:
		struct FStationContext
		{
			TObjectPtr<ARpgCraftingStationActor> StationActor = nullptr;
			TObjectPtr<URpgCraftingStationComponent> Station = nullptr;

			bool IsValid() const
			{
				return StationActor && Station;
			}
		};

		FScopedCraftingWidgetWorld()
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

		~FScopedCraftingWidgetWorld()
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

		bool InitializePlayerFixture(FAutomationTestBase& Test)
		{
			if (!World)
			{
				Test.AddError(
					TEXT("Could not create an isolated crafting-widget world."));
				return false;
			}

			FActorSpawnParameters ControllerParameters;
			ControllerParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerController::StaticClass(),
				TEXT("CraftingUiController"));
			ControllerParameters.ObjectFlags = RF_Transient;
			ControllerParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Controller =
				World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
					ControllerParameters);

			FActorSpawnParameters PlayerStateParameters;
			PlayerStateParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerState::StaticClass(),
				TEXT("CraftingUiPlayerState"));
			PlayerStateParameters.ObjectFlags = RF_Transient;
			PlayerStateParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			PlayerState =
				World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
					PlayerStateParameters);

			FActorSpawnParameters PawnParameters;
			PawnParameters.Name = MakeUniqueObjectName(
				World,
				APawn::StaticClass(),
				TEXT("CraftingUiPawn"));
			PawnParameters.ObjectFlags = RF_Transient;
			PawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Pawn = World->SpawnActor<APawn>(PawnParameters);

			if (!Test.TestNotNull(
					TEXT("Crafting UI controller fixture exists"),
					Controller.Get()) ||
				!Test.TestNotNull(
					TEXT("Crafting UI player-state fixture exists"),
					PlayerState.Get()) ||
				!Test.TestNotNull(
					TEXT("Crafting UI pawn fixture exists"),
					Pawn.Get()))
			{
				return false;
			}

			Controller->SetPlayerState(PlayerState);
			PlayerState->SetOwner(Controller);
			Controller->Possess(Pawn);
			PlayerInventory = PlayerState->GetInventoryManagerComponent();
			return Test.TestNotNull(
				TEXT("Crafting UI canonical player inventory exists"),
				PlayerInventory.Get());
		}

		FStationContext CreateStation(const TCHAR* DebugName)
		{
			FStationContext Result;
			if (!World)
			{
				return Result;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				ARpgCraftingStationActor::StaticClass(),
				FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Result.StationActor =
				World->SpawnActor<ARpgCraftingStationActor>(SpawnParameters);
			if (Result.StationActor)
			{
				Result.StationActor->SetActorLocation(FVector::ZeroVector);
				Result.Station =
					Result.StationActor->GetCraftingStationComponent();
			}
			return Result;
		}

		bool IsValid() const
		{
			return World != nullptr;
		}

		UWorld* GetTestWorld() const
		{
			return World;
		}

		APawn* GetPawn() const
		{
			return Pawn;
		}

		URpgInventoryManagerComponent* GetPlayerInventory() const
		{
			return PlayerInventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
		TObjectPtr<ARpgInventoryAutomationTestPlayerController> Controller =
			nullptr;
		TObjectPtr<ARpgInventoryAutomationTestPlayerState> PlayerState =
			nullptr;
		TObjectPtr<APawn> Pawn = nullptr;
		TObjectPtr<URpgInventoryManagerComponent> PlayerInventory = nullptr;
	};

	bool OfferSingleRecipe(
		FAutomationTestBase& Test,
		const FScopedCraftingWidgetWorld::FStationContext& Context)
	{
		if (!Context.Station)
		{
			return false;
		}

		URpgCraftingRecipeDefinition* Recipe =
			NewObject<URpgCraftingRecipeDefinition>(
				Context.Station,
				NAME_None,
				RF_Transient);
		URpgCraftingRecipeSet* RecipeSet =
			NewObject<URpgCraftingRecipeSet>(
				Context.Station,
				NAME_None,
				RF_Transient);
		if (!Test.TestNotNull(
				TEXT("Crafting UI focus recipe exists"),
				Recipe) ||
			!Test.TestNotNull(
				TEXT("Crafting UI focus recipe set exists"),
				RecipeSet))
		{
			return false;
		}

		Recipe->DisplayName = FText::FromString(TEXT("Automation Recipe"));
		Recipe->bUnlockedByDefault = true;
		FRpgCraftingOutputItem& Output =
			Recipe->OutputItems.AddDefaulted_GetRef();
		Output.ItemDefinition =
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
		Output.Count = 1;
		RecipeSet->Recipes.Add(Recipe);

		FObjectPropertyBase* AvailableRecipeSetProperty =
			FindFProperty<FObjectPropertyBase>(
				URpgCraftingStationComponent::StaticClass(),
				TEXT("AvailableRecipeSet"));
		if (!Test.TestNotNull(
				TEXT("Crafting station exposes its recipe-set property"),
				AvailableRecipeSetProperty))
		{
			return false;
		}

		AvailableRecipeSetProperty->SetObjectPropertyValue_InContainer(
			Context.Station,
			RecipeSet);
		return true;
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

	UClass* GetListEntryWidgetClass(const UCommonListView* ListView)
	{
		const FClassProperty* EntryWidgetClassProperty =
			ListView
				? FindFProperty<FClassProperty>(
					ListView->GetClass(),
					TEXT("EntryWidgetClass"))
				: nullptr;
		return EntryWidgetClassProperty && ListView
			? Cast<UClass>(
				EntryWidgetClassProperty->GetObjectPropertyValue_InContainer(
					ListView))
			: nullptr;
	}

	int32 GetQuickTransferRouteCount(
		URpgInventoryDragDropCoordinator* Coordinator)
	{
		const FArrayProperty* RoutesProperty =
			FindFProperty<FArrayProperty>(
				URpgInventoryDragDropCoordinator::StaticClass(),
				TEXT("QuickTransferRoutes"));
		if (!RoutesProperty || !Coordinator)
		{
			return INDEX_NONE;
		}

		FScriptArrayHelper Routes(
			RoutesProperty,
			RoutesProperty->ContainerPtrToValuePtr<void>(Coordinator));
		return Routes.Num();
	}

	bool ValidateGraphFreeTypedLeaf(
		FAutomationTestBase& Test,
		UWorld* World,
		const TCHAR* Label,
		UClass* LeafClass,
		UClass* ExpectedNativeClass,
		FName ExpectedSourceName,
		UClass* ExpectedViewModelClass)
	{
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s class loads"), Label),
				LeafClass) ||
			!Test.TestTrue(
				*FString::Printf(
					TEXT("%s derives from its typed native presenter"),
					Label),
				LeafClass &&
					LeafClass->IsChildOf(ExpectedNativeClass)))
		{
			return false;
		}

		UWidgetBlueprintGeneratedClass* GeneratedClass =
			Cast<UWidgetBlueprintGeneratedClass>(LeafClass);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s is an authored Widget Blueprint"), Label),
				GeneratedClass))
		{
			return false;
		}

		const FName GeneratedManualSetter(
			*(FString(TEXT("Set")) + ExpectedSourceName.ToString()));
		// Presentation functions such as state setters are designer-owned; only the
		// presenter's manual-source seam is a contract.
		Test.TestNotNull(
			*FString::Printf(
				TEXT("%s exposes MVVM's manual-source setter"),
				Label),
			GeneratedClass->FindFunctionByName(GeneratedManualSetter));
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s can initialize without a player context"),
				Label),
			GeneratedClass->bCanCallInitializedWithoutPlayerContext);

		const TArray<UWidgetBlueprintGeneratedClassExtension*> ViewExtensions =
			GeneratedClass->GetExtensions(
				UMVVMViewClass::StaticClass(),
				false);
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns exactly one compiled MVVM view"),
				Label),
			ViewExtensions.Num(),
			1);
		const UMVVMViewClass* ViewClass =
			ViewExtensions.Num() == 1
				? Cast<UMVVMViewClass>(ViewExtensions[0])
				: nullptr;
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s MVVM view class exists"), Label),
				ViewClass))
		{
			return false;
		}

		const TArrayView<const FMVVMViewClass_Source> Sources =
			ViewClass->GetSources();
		int32 MatchingViewModelSourceIndex = INDEX_NONE;
		int32 MatchingViewModelSourceCount = 0;
		int32 UserWidgetDestinationSourceCount = 0;
		int32 UnexpectedSourceCount = 0;
		for (int32 SourceIndex = 0;
			SourceIndex < Sources.Num();
			++SourceIndex)
		{
			const FMVVMViewClass_Source& Candidate =
				Sources[SourceIndex];
			if (Candidate.IsViewModel())
			{
				if (Candidate.GetName() == ExpectedSourceName)
				{
					MatchingViewModelSourceIndex = SourceIndex;
					++MatchingViewModelSourceCount;
				}
				else
				{
					++UnexpectedSourceCount;
				}
			}
			else if (Candidate.IsUserWidget())
			{
				// Bindings targeting native setters require MVVM's compiler-owned Self
				// destination source. It is not an additional ViewModel.
				++UserWidgetDestinationSourceCount;
			}
			else
			{
				++UnexpectedSourceCount;
			}
		}
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns exactly one canonical ViewModel source"),
				Label),
			MatchingViewModelSourceCount,
			1);
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns exactly MVVM's compiler-required Self destination"),
				Label),
			UserWidgetDestinationSourceCount,
			1);
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s owns no unexpected MVVM sources"),
				Label),
			UnexpectedSourceCount,
			0);
		if (!Sources.IsValidIndex(MatchingViewModelSourceIndex))
		{
			return false;
		}

		const FMVVMViewClass_Source& Source =
			Sources[MatchingViewModelSourceIndex];
		Test.TestTrue(
			*FString::Printf(TEXT("%s source is a ViewModel"), Label),
			Source.IsViewModel());
		Test.TestEqual(
			*FString::Printf(TEXT("%s source has its canonical name"), Label),
			Source.GetName(),
			ExpectedSourceName);
		Test.TestEqual(
			*FString::Printf(TEXT("%s source expects the exact VM type"), Label),
			Source.GetSourceClass(),
			ExpectedViewModelClass);
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s source is settable by the native presenter"),
				Label),
			Source.CanBeSet());
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s source is optional while the entry is pooled"),
				Label),
			Source.IsOptional());
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s keeps presentation in declarative leaf bindings"),
				Label),
			ViewClass->GetBindings().Num() > 0);
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s exact source owns every authored binding"),
				Label),
			Source.GetBindings().Num(),
			ViewClass->GetBindings().Num());
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s VM permits only presenter-supplied manual composition"),
				Label),
			ExpectedViewModelClass->GetMetaData(
				TEXT("MVVMAllowedContextCreationType")),
			FString(TEXT("Manual")));

		UUserWidget* Widget =
			CreateWidget<UUserWidget>(World, LeafClass);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s initializes"), Label),
				Widget))
		{
			return false;
		}

		UMVVMView* RuntimeView =
			UMVVMSubsystem::GetViewFromUserWidget(Widget);
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("%s runtime MVVM view exists"), Label),
				RuntimeView))
		{
			return false;
		}
		Test.TestNull(
			*FString::Printf(
				TEXT("%s fresh optional source starts empty"),
				Label),
			RuntimeView->GetViewModel(ExpectedSourceName).GetObject());

		TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
		Test.TestTrue(
			*FString::Printf(
				TEXT("%s constructs its graph-free Slate leaf"),
				Label),
			SlateWidget.IsValid());
		Test.TestTrue(
			*FString::Printf(TEXT("%s MVVM view constructs"), Label),
			RuntimeView->IsConstructed());
		Test.TestTrue(
			*FString::Printf(TEXT("%s MVVM sources initialize"), Label),
			RuntimeView->AreSourcesInitialized());
		Test.TestTrue(
			*FString::Printf(TEXT("%s MVVM bindings initialize"), Label),
			RuntimeView->AreBindingsInitialized());

		IUserListEntry::ReleaseEntry(*Widget);
		Test.TestNull(
			*FString::Printf(
				TEXT("%s release leaves no stale pooled MVVM source"),
				Label),
			RuntimeView->GetViewModel(ExpectedSourceName).GetObject());
		SlateWidget.Reset();
		return true;
	}

	URpgCraftingStationScreenPayload* MakePayload(
		UObject* Outer,
		URpgInventoryManagerComponent* PlayerInventory,
		const FScopedCraftingWidgetWorld::FStationContext& StationContext,
		AActor* RequestingActor)
	{
		URpgCraftingStationScreenPayload* Payload =
			NewObject<URpgCraftingStationScreenPayload>(Outer);
		Payload->ScreenTag = RpgGameplayTags::UI_Screen_Crafting;
		Payload->PrimaryInventory = PlayerInventory;
		Payload->ContextActor = StationContext.StationActor;
		Payload->ContextComponent = StationContext.Station;
		Payload->PlayerInventory = PlayerInventory;
		Payload->CraftingStation = StationContext.Station;
		Payload->RequestingActor = RequestingActor;
		return Payload;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingScreenContractTest,
	"SurvivalRpg.Inventory.UI.Crafting.ScreenContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingScreenContractTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgCraftingStationWidgetTests;

	FScopedCraftingWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT("Standalone crafting-widget world is valid"),
			TestWorld.IsValid()))
	{
		return false;
	}

	UClass* ScreenClass = LoadClass<URpgCraftingStationWidget>(
		nullptr,
		CraftingScreenClassPath);
	UClass* ActionButtonClass =
		LoadClass<URpgCraftingActionButtonWidget>(
			nullptr,
			CraftingActionButtonClassPath);
	if (!TestNotNull(
			TEXT("Authored Crafting screen loads"),
			ScreenClass) ||
		!TestNotNull(
			TEXT("Authored Crafting action-button leaf loads"),
			ActionButtonClass))
	{
		return false;
	}

	TestTrue(
		TEXT("Authored Crafting screen derives from the native presenter"),
		ScreenClass->IsChildOf(
			URpgCraftingStationWidget::StaticClass()));
	TestTrue(
		TEXT("Native Crafting presenter derives from the shared inventory screen"),
		URpgCraftingStationWidget::StaticClass()->IsChildOf(
			URpgInventoryInteractionScreenWidget::StaticClass()));
	TestTrue(
		TEXT("Authored Crafting screen retains the payload receiver contract"),
		ScreenClass->ImplementsInterface(
			URpgUIScreenPayloadReceiver::StaticClass()));
	TestTrue(
		TEXT("Crafting action-button asset derives from its native leaf"),
		ActionButtonClass->IsChildOf(
			URpgCraftingActionButtonWidget::StaticClass()));

	URpgCraftingStationWidget* RuntimeWidget =
		CreateWidget<URpgCraftingStationWidget>(
			TestWorld.GetTestWorld(),
			ScreenClass);
	if (!TestNotNull(
			TEXT("Authored Crafting screen initializes"),
			RuntimeWidget))
	{
		return false;
	}

	// Required native bindings: the presenter's stable seam, independent of the authored layout around them.
	const FName BoundWidgetNames[] = {
		TEXT("RecipeList"),
		TEXT("IngredientList"),
		TEXT("CraftButton"),
		TEXT("PauseButton"),
		TEXT("QuantityMinusButton"),
		TEXT("QuantityPlusButton"),
		TEXT("QuantityMaxButton")
	};
	for (const FName PropertyName : BoundWidgetNames)
	{
		const FObjectPropertyBase* Property =
			FindFProperty<FObjectPropertyBase>(
				URpgCraftingStationWidget::StaticClass(),
				PropertyName);
		UWidget* AuthoredWidget =
			RuntimeWidget->GetWidgetFromName(PropertyName);
		TestTrue(
			*FString::Printf(
				TEXT("%s binds into its native presenter property"),
				*PropertyName.ToString()),
			Property && AuthoredWidget &&
				Property->GetObjectPropertyValue_InContainer(
					RuntimeWidget) == AuthoredWidget);
	}

	const FObjectPropertyBase* DragVisualProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgInventoryInteractionScreenWidget::StaticClass(),
			TEXT("DragVisualCanvas"));
	TestTrue(
		TEXT("The shared drag host binds into the screen property"),
		DragVisualProperty &&
			DragVisualProperty->GetObjectPropertyValue_InContainer(
				RuntimeWidget) != nullptr);

	const UCommonListView* RecipeList = Cast<UCommonListView>(RuntimeWidget->GetWidgetFromName(TEXT("RecipeList")));
	const UCommonListView* IngredientList = Cast<UCommonListView>(RuntimeWidget->GetWidgetFromName(TEXT("IngredientList")));
	UClass* RecipeEntryClass = GetListEntryWidgetClass(RecipeList);
	UClass* IngredientEntryClass = GetListEntryWidgetClass(IngredientList);
	TestTrue(
		TEXT("Recipe rows use the typed recipe-entry presenter"),
		RecipeEntryClass && RecipeEntryClass->IsChildOf(URpgCraftingRecipeEntryWidget::StaticClass()));
	TestTrue(
		TEXT("Material rows use the typed ingredient-entry presenter"),
		IngredientEntryClass && IngredientEntryClass->IsChildOf(URpgCraftingIngredientEntryWidget::StaticClass()));

	const URpgCraftingStationWidget* ScreenDefaults =
		Cast<URpgCraftingStationWidget>(ScreenClass->GetDefaultObject());
	const FClassProperty* SectionClassProperty = FindFProperty<FClassProperty>(
		URpgCraftingStationWidget::StaticClass(),
		TEXT("TierSectionEntryClass"));
	UClass* SectionClass = SectionClassProperty && ScreenDefaults
		? Cast<UClass>(SectionClassProperty->GetObjectPropertyValue_InContainer(ScreenDefaults))
		: nullptr;
	TestTrue(
		TEXT("Tier section headers have an authored list entry"),
		SectionClass && SectionClass->ImplementsInterface(UUserObjectListEntry::StaticClass()));
	const FObjectPropertyBase* CatalogProperty = FindFProperty<FObjectPropertyBase>(
		URpgCraftingStationWidget::StaticClass(),
		TEXT("CategoryCatalog"));
	TestTrue(
		TEXT("The screen names its category catalog"),
		CatalogProperty && ScreenDefaults && CatalogProperty->GetObjectPropertyValue_InContainer(ScreenDefaults) != nullptr);

	UDataTable* ActionTable = LoadObject<UDataTable>(
		nullptr,
		CraftingActionTablePath);
	if (!TestNotNull(
			TEXT("Crafting CommonUI action table loads"),
			ActionTable))
	{
		return false;
	}
	const TArray<FName> ActionRows = ActionTable->GetRowNames();
	TestTrue(
		TEXT("Crafting action table contains the start action"),
		ActionRows.Contains(FName(TEXT("UI.Crafting.Craft"))));
	TestTrue(
		TEXT("Crafting action table contains the pause toggle action"),
		ActionRows.Contains(FName(TEXT("UI.Crafting.TogglePause"))));
	TestTrue(
		TEXT("Crafting action table contains the stop action"),
		ActionRows.Contains(FName(TEXT("UI.Crafting.StopOrder"))));
	const FCommonInputActionDataBase* CraftActionRow =
		ActionTable->FindRow<FCommonInputActionDataBase>(
			TEXT("UI.Crafting.Craft"),
			TEXT("Crafting action contract"));
	if (!TestNotNull(
			TEXT("Crafting action row resolves to CommonUI data"),
			CraftActionRow))
	{
		return false;
	}
	TestEqual(
		TEXT("Crafting keyboard shortcut does not collide with focused-widget accept"),
		CraftActionRow
			->GetInputTypeInfo(
				ECommonInputType::MouseAndKeyboard,
				FCommonInputDefaults::GamepadGeneric)
			.GetKey(),
		EKeys::C);
	TestEqual(
		TEXT("Crafting gamepad shortcut does not collide with face-button accept"),
		CraftActionRow
			->GetInputTypeInfo(
				ECommonInputType::Gamepad,
				FCommonInputDefaults::GamepadGeneric)
			.GetKey(),
		EKeys::Gamepad_FaceButton_Left);

	const auto ReadActionHandle = [ScreenDefaults](const TCHAR* PropertyName) -> const FDataTableRowHandle*
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(
			URpgCraftingStationWidget::StaticClass(),
			PropertyName);
		return Property && ScreenDefaults
			? Property->ContainerPtrToValuePtr<FDataTableRowHandle>(ScreenDefaults)
			: nullptr;
	};
	const FDataTableRowHandle* CraftActionHandle = ReadActionHandle(TEXT("CraftInputAction"));
	const FDataTableRowHandle* TogglePauseActionHandle = ReadActionHandle(TEXT("TogglePauseInputAction"));
	TestTrue(
		TEXT("Crafting screen explicitly authors its start action row"),
		CraftActionHandle &&
			CraftActionHandle->DataTable == ActionTable &&
			CraftActionHandle->RowName ==
				FName(TEXT("UI.Crafting.Craft")));
	TestTrue(
		TEXT("Crafting screen explicitly authors its pause action row"),
		TogglePauseActionHandle &&
			TogglePauseActionHandle->DataTable == ActionTable &&
			TogglePauseActionHandle->RowName ==
				FName(TEXT("UI.Crafting.TogglePause")));

	const IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry"))
			.Get();
	TArray<FName> ScreenDependencies;
	TestTrue(
		TEXT("Asset Registry resolves Crafting screen dependencies"),
		AssetRegistry.GetDependencies(
			FName(CraftingScreenPackageName),
			ScreenDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));
	TestTrue(
		TEXT("Crafting screen depends on its typed recipe-entry leaf"),
		ScreenDependencies.Contains(FName(RecipeEntryPackageName)));
	TestTrue(
		TEXT("Crafting screen depends on its typed ingredient-entry leaf"),
		ScreenDependencies.Contains(FName(IngredientEntryPackageName)));
	TestTrue(
		TEXT("Crafting screen owns a cook-visible dependency on its CommonUI actions"),
		ScreenDependencies.Contains(
			FName(CraftingActionTablePackageName)));
	TestFalse(
		TEXT("Crafting screen has no legacy flat-inventory dependency"),
		ScreenDependencies.Contains(FName(LegacyInventoryPackageName)));
	TestFalse(
		TEXT("Crafting screen does not wrap the legacy graph-heavy screen"),
		ScreenDependencies.Contains(
			FName(LegacyCraftingScreenPackageName)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingTypedLeafMvvmTest,
	"SurvivalRpg.Inventory.UI.Crafting.TypedLeafMvvm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingTypedLeafMvvmTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgCraftingStationWidgetTests;

	FScopedCraftingWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT("Standalone typed-leaf world is valid"),
			TestWorld.IsValid()))
	{
		return false;
	}

	const bool bRecipeValid = ValidateGraphFreeTypedLeaf(
		*this,
		TestWorld.GetTestWorld(),
		TEXT("Recipe entry"),
		LoadClass<URpgCraftingRecipeEntryWidget>(
			nullptr,
			RecipeEntryClassPath),
		URpgCraftingRecipeEntryWidget::StaticClass(),
		URpgCraftingRecipeEntryWidget::RecipeViewModelSourceName,
		URpgCraftingRecipeViewModel::StaticClass());
	const bool bIngredientValid = ValidateGraphFreeTypedLeaf(
		*this,
		TestWorld.GetTestWorld(),
		TEXT("Ingredient entry"),
		LoadClass<URpgCraftingIngredientEntryWidget>(
			nullptr,
			IngredientEntryClassPath),
		URpgCraftingIngredientEntryWidget::StaticClass(),
		URpgCraftingIngredientEntryWidget::IngredientViewModelSourceName,
		URpgCraftingIngredientViewModel::StaticClass());
	return bRecipeValid && bIngredientValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingScreenPayloadLifecycleTest,
	"SurvivalRpg.Inventory.UI.Crafting.PayloadLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingScreenPayloadLifecycleTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgCraftingStationWidgetTests;

	FScopedCraftingWidgetWorld TestWorld;
	if (!TestWorld.InitializePlayerFixture(*this))
	{
		return false;
	}

	const FScopedCraftingWidgetWorld::FStationContext ContextA =
		TestWorld.CreateStation(TEXT("CraftingUiStationA"));
	const FScopedCraftingWidgetWorld::FStationContext ContextB =
		TestWorld.CreateStation(TEXT("CraftingUiStationB"));
	if (!TestTrue(TEXT("Crafting station context A is valid"), ContextA.IsValid()) ||
		!TestTrue(TEXT("Crafting station context B is valid"), ContextB.IsValid()))
	{
		return false;
	}
	if (!OfferSingleRecipe(*this, ContextA))
	{
		return false;
	}

	UClass* ScreenClass = LoadClass<URpgCraftingStationWidget>(
		nullptr,
		CraftingScreenClassPath);
	if (!TestNotNull(
			TEXT("Authored Crafting screen loads"),
			ScreenClass))
	{
		return false;
	}

	URpgCraftingStationWidget* Widget =
		CreateWidget<URpgCraftingStationWidget>(
			TestWorld.GetTestWorld(),
			ScreenClass);
	if (!TestNotNull(TEXT("Crafting screen initializes"), Widget))
	{
		return false;
	}
	// Commandlet automation does not run CommonInput's normal startup path,
	// while CommonActivatableWidget::NativeConstruct requires its back action.
	ICommonInputModule::GetSettings().LoadData();
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	if (!TestTrue(
			TEXT("Crafting screen constructs its authored Slate tree"),
			SlateWidget.IsValid()))
	{
		return false;
	}

	URpgCraftingStationViewModel* CraftingViewModel =
		Widget->GetCraftingViewModel();
	if (!TestNotNull(
			TEXT("Crafting screen owns its stable crafting VM"),
			CraftingViewModel))
	{
		return false;
	}
	TestEqual(
		TEXT("Crafting screen owns exactly one direct crafting VM"),
		CountDirectObjectsOfClass<URpgCraftingStationViewModel>(Widget),
		1);

	URpgCraftingStationScreenPayload* PayloadA = MakePayload(
		Widget,
		TestWorld.GetPlayerInventory(),
		ContextA,
		TestWorld.GetPawn());
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadA);
	TestEqual(
		TEXT("Pre-activation Crafting payload is staged"),
		Widget->GetCraftingScreenPayload(),
		PayloadA);
	TestEqual(
		TEXT("Staging performs no Crafting presentation bind"),
		Widget->GetCraftingPresentationBindGeneration(),
		0u);
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadA);
	TestEqual(
		TEXT("Repeated staged payload remains idempotent"),
		Widget->GetCraftingPresentationBindGeneration(),
		0u);

	Widget->ActivateWidget();
	TestTrue(
		TEXT("Crafting screen activates through CommonUI"),
		Widget->IsActivated());
	TestEqual(
		TEXT("Staged payload binds exactly once on activation"),
		Widget->GetCraftingPresentationBindGeneration(),
		1u);
	UCommonListView* RecipeList =
		Cast<UCommonListView>(
			Widget->GetWidgetFromName(TEXT("RecipeList")));
	if (TestNotNull(
			TEXT("Crafting screen exposes its authored recipe list"),
			RecipeList))
	{
		TestTrue(
			TEXT("The recipe list shows a tier header and the recipe"),
			RecipeList->GetNumItems() == 2 &&
				Cast<URpgCraftingTierSectionViewModel>(RecipeList->GetItemAt(0)) &&
				Cast<URpgCraftingRecipeViewModel>(RecipeList->GetItemAt(1)));
		TestEqual(
			TEXT("The recipe row, not its header, is selected"),
			RecipeList->GetSelectedItem(),
			RecipeList->GetItemAt(1));
		TestEqual(
			TEXT("Crafting initially prefers the recipe list for CommonUI focus"),
			Widget->GetDesiredFocusTarget(),
			static_cast<UWidget*>(RecipeList));
	}

	const FObjectPropertyBase* CoordinatorProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgInventoryInteractionScreenWidget::StaticClass(),
			TEXT("InventoryDragDropCoordinator"));
	URpgInventoryDragDropCoordinator* Coordinator =
		CoordinatorProperty
			? Cast<URpgInventoryDragDropCoordinator>(
				CoordinatorProperty->GetObjectPropertyValue_InContainer(
					Widget))
			: nullptr;
	if (!TestNotNull(
			TEXT("Crafting screen owns a shared drag/drop coordinator"),
			Coordinator))
	{
		return false;
	}
	TestEqual(
		TEXT("A station without an inventory exposes no quick-transfer route"),
		GetQuickTransferRouteCount(Coordinator),
		0);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadA);
	TestEqual(
		TEXT("Reapplying the same active payload does not bind again"),
		Widget->GetCraftingPresentationBindGeneration(),
		1u);

	URpgCraftingStationScreenPayload* PayloadB = MakePayload(
		Widget,
		TestWorld.GetPlayerInventory(),
		ContextB,
		TestWorld.GetPawn());
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadB);
	TestEqual(
		TEXT("Active context switch retains payload B"),
		Widget->GetCraftingScreenPayload(),
		PayloadB);
	TestEqual(
		TEXT("Active context switch binds replacement exactly once"),
		Widget->GetCraftingPresentationBindGeneration(),
		2u);
	TestEqual(
		TEXT("Context switch retains the screen-owned Crafting VM"),
		Widget->GetCraftingViewModel(),
		CraftingViewModel);

	Widget->DeactivateWidget();
	TestFalse(
		TEXT("Crafting screen deactivates through CommonUI"),
		Widget->IsActivated());
	TestNull(
		TEXT("Deactivation releases the retained payload"),
		Widget->GetCraftingScreenPayload());
	TestEqual(
		TEXT("Deactivation retains the stable Crafting VM"),
		Widget->GetCraftingViewModel(),
		CraftingViewModel);

	Widget->ActivateWidget();
	TestNull(
		TEXT("Pool reactivation never resurrects a stale payload"),
		Widget->GetCraftingScreenPayload());
	TestEqual(
		TEXT("Pool reactivation without payload performs no bind"),
		Widget->GetCraftingPresentationBindGeneration(),
		2u);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadB);
	TestEqual(
		TEXT("Fresh payload after pooling binds once"),
		Widget->GetCraftingPresentationBindGeneration(),
		3u);

	URpgCraftingStationScreenPayload* MismatchedPayload =
		MakePayload(
			Widget,
			TestWorld.GetPlayerInventory(),
			ContextB,
			TestWorld.GetPawn());
	MismatchedPayload->ContextActor = ContextA.StationActor;
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		MismatchedPayload);
	TestNull(
		TEXT("Station/actor mismatch rejects and clears the active payload"),
		Widget->GetCraftingScreenPayload());
	TestEqual(
		TEXT("Rejected payload never increments bind generation"),
		Widget->GetCraftingPresentationBindGeneration(),
		3u);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		NewObject<URpgInventoryScreenPayload>(Widget));
	TestNull(
		TEXT("A generic inventory payload is rejected"),
		Widget->GetCraftingScreenPayload());

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadA);
	TestEqual(
		TEXT("Valid payload still binds after rejected candidates"),
		Widget->GetCraftingPresentationBindGeneration(),
		4u);

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		nullptr);
	TestNull(
		TEXT("Explicit payload clear releases the Crafting context"),
		Widget->GetCraftingScreenPayload());
	TestEqual(
		TEXT("Explicit payload clear performs no extra bind"),
		Widget->GetCraftingPresentationBindGeneration(),
		4u);

	Widget->DeactivateWidget();
	SlateWidget.Reset();
	return true;
}

#endif
