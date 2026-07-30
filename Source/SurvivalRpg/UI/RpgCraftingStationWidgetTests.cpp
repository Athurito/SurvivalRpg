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
#include "SurvivalRpg/UI/RpgCraftingJobEntryWidget.h"
#include "SurvivalRpg/UI/RpgCraftingRecipeEntryWidget.h"
#include "SurvivalRpg/UI/RpgInventoryInteractionScreenWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventorySpatialPaneWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryPaneWidget.h"

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
#include "Components/CheckBox.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
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
	constexpr TCHAR JobEntryClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingJobEntrySpatial.CUI_CraftingJobEntrySpatial_C");
	constexpr TCHAR SpatialPaneClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryPane.CUI_SpatialInventoryPane_C");
	constexpr TCHAR PlayerInventoryPaneClassPath[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_PlayerInventoryPane.CUI_PlayerInventoryPane_C");
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
	constexpr TCHAR JobEntryPackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Crafting/UI/"
			"CUI_CraftingJobEntrySpatial");
	constexpr TCHAR CraftingActionTablePackageName[] =
		TEXT("/Game/SurvivalRpg/UI/Input/DT_RpgUIActions_Crafting");
	constexpr TCHAR SpatialPanePackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/SpatialInventory/"
			"CUI_SpatialInventoryPane");
	constexpr TCHAR PlayerInventoryPanePackageName[] =
		TEXT(
			"/Game/SurvivalRpg/Inventory/UI/"
			"CUI_PlayerInventoryPane");
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
			TObjectPtr<URpgInventoryManagerComponent> OutputInventory = nullptr;

			bool IsValid() const
			{
				return StationActor && Station && OutputInventory;
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
				Result.OutputInventory =
					Result.StationActor->GetOutputInventoryComponent();
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

	int32 CountUnexpectedFunctionsDeclaredByClass(
		const UClass* Class,
		const TSet<FName>& AllowedFunctions)
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
				if (!AllowedFunctions.Contains(FunctionIt->GetFName()))
				{
					++Count;
				}
			}
		}
		return Count;
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
		Test.TestNotNull(
			*FString::Printf(
				TEXT("%s exposes only MVVM's expected manual-source setter"),
				Label),
			GeneratedClass->FindFunctionByName(GeneratedManualSetter));
		Test.TestEqual(
			*FString::Printf(
				TEXT("%s declares no user-authored or conversion-wrapper functions"),
				Label),
			CountUnexpectedFunctionsDeclaredByClass(
				GeneratedClass,
				{ GeneratedManualSetter }),
			0);
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
		Payload->SecondaryInventory = StationContext.OutputInventory;
		Payload->ContextActor = StationContext.StationActor;
		Payload->ContextComponent = StationContext.Station;
		Payload->PlayerInventory = PlayerInventory;
		Payload->CraftingStation = StationContext.Station;
		Payload->OutputInventory = StationContext.OutputInventory;
		Payload->RequestingActor = RequestingActor;
		return Payload;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingSpatialCompositionTest,
	"SurvivalRpg.Inventory.UI.Crafting.SpatialComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgCraftingSpatialCompositionTest::RunTest(
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
	UClass* RecipeEntryClass =
		LoadClass<URpgCraftingRecipeEntryWidget>(
			nullptr,
			RecipeEntryClassPath);
	UClass* IngredientEntryClass =
		LoadClass<URpgCraftingIngredientEntryWidget>(
			nullptr,
			IngredientEntryClassPath);
	UClass* JobEntryClass =
		LoadClass<URpgCraftingJobEntryWidget>(
			nullptr,
			JobEntryClassPath);
	UClass* SpatialPaneClass =
		LoadClass<URpgInventorySpatialPaneWidget>(
			nullptr,
			SpatialPaneClassPath);
	UClass* PlayerInventoryPaneClass =
		LoadClass<URpgPlayerInventoryPaneWidget>(
			nullptr,
			PlayerInventoryPaneClassPath);
	if (!TestNotNull(
			TEXT("Authored Crafting Spatial screen loads"),
			ScreenClass) ||
		!TestNotNull(
			TEXT("Authored Crafting action-button leaf loads"),
			ActionButtonClass) ||
		!TestNotNull(
			TEXT("Authored recipe-entry leaf loads"),
			RecipeEntryClass) ||
		!TestNotNull(
			TEXT("Authored ingredient-entry leaf loads"),
			IngredientEntryClass) ||
		!TestNotNull(
			TEXT("Authored job-entry leaf loads"),
			JobEntryClass) ||
		!TestNotNull(
			TEXT("Canonical authored Spatial Pane loads"),
			SpatialPaneClass) ||
		!TestNotNull(
			TEXT("Canonical authored Player Inventory Pane loads"),
			PlayerInventoryPaneClass))
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
		TEXT("Crafting action-button asset derives from its graph-free native leaf"),
		ActionButtonClass->IsChildOf(
			URpgCraftingActionButtonWidget::StaticClass()));
	TestFalse(
		TEXT("Player Inventory Pane is passive and not an activatable inventory screen"),
		PlayerInventoryPaneClass->IsChildOf(
			URpgInventoryInteractionScreenWidget::StaticClass()));
	TestFalse(
		TEXT("Player Inventory Pane never receives screen payloads"),
		PlayerInventoryPaneClass->ImplementsInterface(
			URpgUIScreenPayloadReceiver::StaticClass()));

	UWidgetBlueprintGeneratedClass* ScreenGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(ScreenClass);
	UWidgetBlueprintGeneratedClass* ActionButtonGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(ActionButtonClass);
	if (!TestNotNull(
			TEXT("Crafting screen is an authored Widget Blueprint"),
			ScreenGeneratedClass) ||
		!TestNotNull(
			TEXT("Crafting action button is an authored Widget Blueprint"),
			ActionButtonGeneratedClass))
	{
		return false;
	}

	TestEqual(
		TEXT("Crafting screen declares no compiled Blueprint graph functions"),
		CountFunctionsDeclaredByClass(ScreenGeneratedClass),
		0);
	TestEqual(
		TEXT("Crafting screen owns no root-level MVVM extension"),
		ScreenGeneratedClass
			->GetExtensions(UMVVMViewClass::StaticClass(), false)
			.Num(),
		0);
	TestEqual(
		TEXT("Crafting action button declares no Blueprint graph functions"),
		CountFunctionsDeclaredByClass(ActionButtonGeneratedClass),
		0);
	TestEqual(
		TEXT("Crafting action button owns no ambiguous MVVM extension"),
		ActionButtonGeneratedClass
			->GetExtensions(UMVVMViewClass::StaticClass(), false)
			.Num(),
		0);

	const UWidgetTree* ScreenTree =
		ScreenGeneratedClass->GetWidgetTreeArchetype();
	const UWidgetTree* ActionButtonTree =
		ActionButtonGeneratedClass->GetWidgetTreeArchetype();
	const UWidgetTree* RecipeEntryTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(RecipeEntryClass)
			->GetWidgetTreeArchetype();
	const UWidgetTree* IngredientEntryTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(IngredientEntryClass)
			->GetWidgetTreeArchetype();
	const UWidgetTree* JobEntryTree =
		CastChecked<UWidgetBlueprintGeneratedClass>(JobEntryClass)
			->GetWidgetTreeArchetype();
	if (!TestNotNull(
			TEXT("Crafting screen has an authored WidgetTree"),
			ScreenTree) ||
		!TestNotNull(
			TEXT("Crafting action button has an authored WidgetTree"),
			ActionButtonTree) ||
		!TestNotNull(
			TEXT("Recipe entry has an authored WidgetTree"),
			RecipeEntryTree) ||
		!TestNotNull(
			TEXT("Ingredient entry has an authored WidgetTree"),
			IngredientEntryTree) ||
		!TestNotNull(
			TEXT("Job entry has an authored WidgetTree"),
			JobEntryTree))
	{
		return false;
	}

	UOverlay* RootOverlay =
		Cast<UOverlay>(ScreenTree->FindWidget(TEXT("RootOverlay")));
	URpgPlayerInventoryPaneWidget* PlayerInventoryPane =
		Cast<URpgPlayerInventoryPaneWidget>(
			ScreenTree->FindWidget(TEXT("PlayerInventoryPane")));
	URpgInventorySpatialPaneWidget* OutputInventoryPane =
		Cast<URpgInventorySpatialPaneWidget>(
			ScreenTree->FindWidget(TEXT("OutputInventoryPane")));
	UCommonListView* RecipeList =
		Cast<UCommonListView>(
			ScreenTree->FindWidget(TEXT("RecipeList")));
	UCommonListView* IngredientList =
		Cast<UCommonListView>(
			ScreenTree->FindWidget(TEXT("IngredientList")));
	UCommonListView* JobsList =
		Cast<UCommonListView>(
			ScreenTree->FindWidget(TEXT("CraftingJobsList")));
	UCanvasPanel* DragVisualCanvas =
		Cast<UCanvasPanel>(
			ScreenTree->FindWidget(TEXT("DragVisualCanvas")));

	TestNotNull(
		TEXT("RootOverlay is the authored Crafting screen root"),
		RootOverlay);
	TestEqual(
		TEXT("PlayerInventoryPane uses the exact canonical reusable Pane class"),
		PlayerInventoryPane ? PlayerInventoryPane->GetClass() : nullptr,
		PlayerInventoryPaneClass);
	TestNull(
		TEXT("Legacy reduced PlayerGroupsPanel is absent"),
		ScreenTree->FindWidget(TEXT("PlayerGroupsPanel")));
	TestEqual(
		TEXT("OutputInventoryPane uses exactly the reusable Spatial Pane class"),
		OutputInventoryPane ? OutputInventoryPane->GetClass() : nullptr,
		SpatialPaneClass);
	TestNotNull(
		TEXT("RecipeList is authored as a CommonListView"),
		RecipeList);
	TestNotNull(
		TEXT("IngredientList is authored as a CommonListView"),
		IngredientList);
	TestNotNull(
		TEXT("CraftingJobsList is authored as a CommonListView"),
		JobsList);
	TestEqual(
		TEXT("RecipeList uses the exact typed recipe-entry class"),
		GetListEntryWidgetClass(RecipeList),
		RecipeEntryClass);
	TestEqual(
		TEXT("IngredientList uses the exact typed ingredient-entry class"),
		GetListEntryWidgetClass(IngredientList),
		IngredientEntryClass);
	TestEqual(
		TEXT("CraftingJobsList uses the exact typed job-entry class"),
		GetListEntryWidgetClass(JobsList),
		JobEntryClass);

	const FName TextWidgetNames[] = {
		TEXT("RecipeNameText"),
		TEXT("RecipeDescriptionText"),
		TEXT("CraftTimeText"),
		TEXT("CraftQuantityText")
	};
	for (const FName WidgetName : TextWidgetNames)
	{
		TestTrue(
			*FString::Printf(
				TEXT("%s is authored as a CommonTextBlock"),
				*WidgetName.ToString()),
			ScreenTree->FindWidget(WidgetName) &&
				ScreenTree->FindWidget(WidgetName)
					->IsA<UCommonTextBlock>());
	}
	TestTrue(
		TEXT("RecipeIcon is authored as a CommonLazyImage"),
		ScreenTree->FindWidget(TEXT("RecipeIcon")) &&
			ScreenTree->FindWidget(TEXT("RecipeIcon"))
				->IsA<UCommonLazyImage>());
	TestTrue(
		TEXT("AutoDepositCheckBox is authored as a CheckBox"),
		ScreenTree->FindWidget(TEXT("AutoDepositCheckBox")) &&
			ScreenTree->FindWidget(TEXT("AutoDepositCheckBox"))
				->IsA<UCheckBox>());
	TestTrue(
		TEXT("ActionBar uses CommonUI's bound action bar"),
		ScreenTree->FindWidget(TEXT("ActionBar")) &&
			ScreenTree->FindWidget(TEXT("ActionBar"))
				->IsA<UCommonBoundActionBar>());

	const FName ActionButtonNames[] = {
		TEXT("CraftButton"),
		TEXT("PauseButton"),
		TEXT("QuantityMinusButton"),
		TEXT("QuantityPlusButton"),
		TEXT("QuantityFiveButton"),
		TEXT("QuantityTenButton"),
		TEXT("QuantityMaxButton")
	};
	for (const FName WidgetName : ActionButtonNames)
	{
		UWidget* AuthoredButton = ScreenTree->FindWidget(WidgetName);
		TestEqual(
			*FString::Printf(
				TEXT("%s uses the exact graph-free Crafting action leaf"),
				*WidgetName.ToString()),
			AuthoredButton ? AuthoredButton->GetClass() : nullptr,
			ActionButtonClass);
	}

	TestNotNull(
		TEXT("DragVisualCanvas is authored as the top-level drag host"),
		DragVisualCanvas);
	if (DragVisualCanvas)
	{
		TestEqual(
			TEXT("DragVisualCanvas never intercepts Crafting input"),
			DragVisualCanvas->GetVisibility(),
			ESlateVisibility::HitTestInvisible);
	}
	if (RootOverlay && DragVisualCanvas)
	{
		TestEqual(
			TEXT("DragVisualCanvas is the final root child"),
			RootOverlay->GetChildIndex(DragVisualCanvas),
			RootOverlay->GetChildrenCount() - 1);
	}

	TArray<UWidget*> ScreenWidgets;
	ScreenTree->GetAllWidgets(ScreenWidgets);
	int32 SpatialPaneCount = 0;
	int32 PlayerInventoryPaneCount = 0;
	for (const UWidget* Widget : ScreenWidgets)
	{
		if (Widget && Widget->IsA<URpgInventorySpatialPaneWidget>())
		{
			++SpatialPaneCount;
		}
		if (Widget && Widget->IsA<URpgPlayerInventoryPaneWidget>())
		{
			++PlayerInventoryPaneCount;
		}
	}
	TestEqual(
		TEXT("Crafting screen authors exactly one Spatial Pane: station output"),
		SpatialPaneCount,
		1);
	TestEqual(
		TEXT("Crafting screen authors exactly one complete Player Inventory Pane"),
		PlayerInventoryPaneCount,
		1);

	TestTrue(
		TEXT("Crafting action button authors its native Text label"),
		ActionButtonTree->FindWidget(TEXT("Text")) &&
			ActionButtonTree->FindWidget(TEXT("Text"))
				->IsA<UCommonTextBlock>());
	TestTrue(
		TEXT("Job row cancel control uses the exact Crafting action leaf"),
		JobEntryTree->FindWidget(TEXT("Button_Cancel")) &&
			JobEntryTree->FindWidget(TEXT("Button_Cancel"))
				->GetClass() == ActionButtonClass);
	TestTrue(
		TEXT("Job row authors its progress destination"),
		JobEntryTree->FindWidget(TEXT("ProgressBar")) &&
			JobEntryTree->FindWidget(TEXT("ProgressBar"))
				->IsA<UProgressBar>());

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

	const FName BoundWidgetNames[] = {
		TEXT("PlayerInventoryPane"),
		TEXT("OutputInventoryPane"),
		TEXT("RecipeList"),
		TEXT("IngredientList"),
		TEXT("CraftingJobsList"),
		TEXT("CraftButton"),
		TEXT("PauseButton"),
		TEXT("QuantityMinusButton"),
		TEXT("QuantityPlusButton"),
		TEXT("QuantityFiveButton"),
		TEXT("QuantityTenButton"),
		TEXT("QuantityMaxButton"),
		TEXT("AutoDepositCheckBox")
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
			Property &&
				Property->GetObjectPropertyValue_InContainer(
					RuntimeWidget) == AuthoredWidget);
	}

	const FObjectPropertyBase* DragVisualProperty =
		FindFProperty<FObjectPropertyBase>(
			URpgInventoryInteractionScreenWidget::StaticClass(),
			TEXT("DragVisualCanvas"));
	TestTrue(
		TEXT("DragVisualCanvas binds into the shared screen property"),
		DragVisualProperty &&
			DragVisualProperty->GetObjectPropertyValue_InContainer(
				RuntimeWidget) ==
				RuntimeWidget->GetWidgetFromName(
					TEXT("DragVisualCanvas")));

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
	TestEqual(
		TEXT("Crafting action table contains exactly two semantic actions"),
		ActionRows.Num(),
		2);
	TestTrue(
		TEXT("Crafting action table contains the craft action"),
		ActionRows.Contains(FName(TEXT("UI.Crafting.Craft"))));
	TestTrue(
		TEXT("Crafting action table contains the pause toggle action"),
		ActionRows.Contains(FName(TEXT("UI.Crafting.TogglePause"))));
	const FCommonInputActionDataBase* CraftActionRow =
		ActionTable->FindRow<FCommonInputActionDataBase>(
			TEXT("UI.Crafting.Craft"),
			TEXT("Crafting Spatial action contract"));
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

	const FStructProperty* CraftActionProperty =
		FindFProperty<FStructProperty>(
			URpgCraftingStationWidget::StaticClass(),
			TEXT("CraftInputAction"));
	const FStructProperty* TogglePauseActionProperty =
		FindFProperty<FStructProperty>(
			URpgCraftingStationWidget::StaticClass(),
			TEXT("TogglePauseInputAction"));
	const URpgCraftingStationWidget* ScreenDefaults =
		Cast<URpgCraftingStationWidget>(
			ScreenGeneratedClass->GetDefaultObject());
	const FDataTableRowHandle* CraftActionHandle =
		CraftActionProperty && ScreenDefaults
			? CraftActionProperty
				->ContainerPtrToValuePtr<FDataTableRowHandle>(
					ScreenDefaults)
			: nullptr;
	const FDataTableRowHandle* TogglePauseActionHandle =
		TogglePauseActionProperty && ScreenDefaults
			? TogglePauseActionProperty
				->ContainerPtrToValuePtr<FDataTableRowHandle>(
					ScreenDefaults)
			: nullptr;
	TestTrue(
		TEXT("Crafting screen explicitly authors its craft action row"),
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
		TEXT("Asset Registry resolves Crafting Spatial dependencies"),
		AssetRegistry.GetDependencies(
			FName(CraftingScreenPackageName),
			ScreenDependencies,
			UE::AssetRegistry::EDependencyCategory::Package));
	TestTrue(
		TEXT("Crafting screen depends on the reusable Spatial Pane"),
		ScreenDependencies.Contains(FName(SpatialPanePackageName)));
	TestTrue(
		TEXT("Crafting screen depends on the reusable Player Inventory Pane"),
		ScreenDependencies.Contains(FName(PlayerInventoryPanePackageName)));
	TestTrue(
		TEXT("Crafting screen depends on its graph-free action-button leaf"),
		ScreenDependencies.Contains(
			FName(CraftingActionButtonPackageName)));
	TestTrue(
		TEXT("Crafting screen depends on its typed recipe-entry leaf"),
		ScreenDependencies.Contains(FName(RecipeEntryPackageName)));
	TestTrue(
		TEXT("Crafting screen depends on its typed ingredient-entry leaf"),
		ScreenDependencies.Contains(FName(IngredientEntryPackageName)));
	TestTrue(
		TEXT("Crafting screen depends on its typed job-entry leaf"),
		ScreenDependencies.Contains(FName(JobEntryPackageName)));
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
	const bool bJobValid = ValidateGraphFreeTypedLeaf(
		*this,
		TestWorld.GetTestWorld(),
		TEXT("Job entry"),
		LoadClass<URpgCraftingJobEntryWidget>(
			nullptr,
			JobEntryClassPath),
		URpgCraftingJobEntryWidget::StaticClass(),
		URpgCraftingJobEntryWidget::JobViewModelSourceName,
		URpgCraftingJobViewModel::StaticClass());
	return bRecipeValid && bIngredientValid && bJobValid;
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
			TEXT("Authored Crafting Spatial screen loads"),
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
	URpgPlayerInventoryPaneWidget* PlayerInventoryPane =
		Cast<URpgPlayerInventoryPaneWidget>(
			Widget->GetWidgetFromName(TEXT("PlayerInventoryPane")));
	if (!TestNotNull(
			TEXT("Crafting screen embeds the complete reusable player Pane"),
			PlayerInventoryPane))
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
	URpgPlayerInventoryViewModel* PlayerViewModel =
		Widget->GetCraftingPlayerInventoryViewModel();
	URpgInventorySpatialPaneWidget* OutputPane =
		Widget->GetOutputInventoryPane();
	URpgInventoryPanelViewModel* OutputPaneViewModel =
		OutputPane ? OutputPane->GetPanelViewModel() : nullptr;
	if (!TestNotNull(
			TEXT("Crafting screen owns its stable crafting VM"),
			CraftingViewModel) ||
		!TestNotNull(
			TEXT("Crafting screen exposes its Pane-owned stable player VM"),
			PlayerViewModel) ||
		!TestNotNull(
			TEXT("Crafting screen binds its authored output Pane"),
			OutputPane) ||
		!TestNotNull(
			TEXT("Output Pane owns its stable panel VM"),
			OutputPaneViewModel))
	{
		return false;
	}

	TestEqual(
		TEXT("Crafting screen owns exactly one direct crafting VM"),
		CountDirectObjectsOfClass<URpgCraftingStationViewModel>(Widget),
		1);
	TestEqual(
		TEXT("Crafting screen owns no duplicate direct player-layout VM"),
		CountDirectObjectsOfClass<URpgPlayerInventoryViewModel>(Widget),
		0);
	TestEqual(
		TEXT("Crafting player Pane owns exactly one stable player-layout VM"),
		CountDirectObjectsOfClass<URpgPlayerInventoryViewModel>(
			PlayerInventoryPane),
		1);
	TestEqual(
		TEXT("Crafting player VM has the passive Pane as its Outer"),
		PlayerViewModel->GetOuter(),
		static_cast<UObject*>(PlayerInventoryPane));
	TestEqual(
		TEXT("Output Pane owns exactly one direct panel VM"),
		CountDirectObjectsOfClass<URpgInventoryPanelViewModel>(OutputPane),
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
		TEXT("Crafting payload remains owned by the activatable root"),
		PayloadA->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("Staging performs no Crafting presentation bind"),
		Widget->GetCraftingPresentationBindGeneration(),
		0u);
	TestNull(
		TEXT("Staging leaves the output Pane unbound"),
		OutputPane->GetBoundInventory());
	TestFalse(
		TEXT("Staging leaves the output root unresolved"),
		Widget->GetOutputPaneContainerHandle().IsValid());

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
	TestEqual(
		TEXT("Output Pane observes station A's exact output inventory"),
		OutputPane->GetBoundInventory(),
		ContextA.OutputInventory.Get());
	const FRpgInventoryContainerHandle ExpectedOutputA =
		FRpgInventoryContainerHandle::MakeRoot(
			ContextA.OutputInventory->GetDefaultContainerId());
	TestTrue(
		TEXT("Output Pane projects station A's exact output root"),
		OutputPane->GetBoundContainerHandle() == ExpectedOutputA);
	TestTrue(
		TEXT("Screen exposes station A's exact output root"),
		Widget->GetOutputPaneContainerHandle() == ExpectedOutputA);
	TestEqual(
		TEXT("Output Pane VM observes station A"),
		OutputPaneViewModel->GetObservedInventory(),
		ContextA.OutputInventory.Get());
	UCommonListView* RecipeList =
		Cast<UCommonListView>(
			Widget->GetWidgetFromName(TEXT("RecipeList")));
	if (TestNotNull(
			TEXT("Crafting screen exposes its authored recipe list"),
			RecipeList))
	{
		TestTrue(
			TEXT("Crafting focus fixture projects at least one recipe"),
			RecipeList->GetNumItems() > 0);
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
	URpgInventoryPanelNavigationCoordinator* PanelNavigator =
		Widget->GetInventoryPanelNavigator();
	if (!TestNotNull(
			TEXT("Crafting screen owns a shared panel navigator"),
			PanelNavigator))
	{
		return false;
	}
	TestEqual(
		TEXT("Crafting drag/drop coordinator is owned by the activatable root"),
		Coordinator->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("Crafting panel navigator is owned by the activatable root"),
		PanelNavigator->GetOuter(),
		static_cast<UObject*>(Widget));
	TestEqual(
		TEXT("Crafting owns exactly one direct drag/drop coordinator"),
		CountDirectObjectsOfClass<URpgInventoryDragDropCoordinator>(Widget),
		1);
	TestEqual(
		TEXT("Crafting owns exactly one direct panel navigator"),
		CountDirectObjectsOfClass<URpgInventoryPanelNavigationCoordinator>(Widget),
		1);
	TestEqual(
		TEXT("Passive player Pane owns no drag/drop coordinator"),
		CountDirectObjectsOfClass<URpgInventoryDragDropCoordinator>(
			PlayerInventoryPane),
		0);
	TestEqual(
		TEXT("Passive player Pane owns no panel navigator"),
		CountDirectObjectsOfClass<URpgInventoryPanelNavigationCoordinator>(
			PlayerInventoryPane),
		0);
	TestEqual(
		TEXT("Quick transfer exposes station output to player inventory"),
		Coordinator->ResolveQuickTransferTarget(
			ContextA.OutputInventory),
		TestWorld.GetPlayerInventory());
	TestEqual(
		TEXT("Crafting screen registers exactly one directional quick-transfer route"),
		GetQuickTransferRouteCount(Coordinator),
		1);
	TestNull(
		TEXT("Crafting screen exposes no player-to-output shortcut"),
		Coordinator->ResolveQuickTransferTarget(
			TestWorld.GetPlayerInventory()));

	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		PayloadA);
	TestEqual(
		TEXT("Reapplying the same active payload does not bind again"),
		Widget->GetCraftingPresentationBindGeneration(),
		1u);
	TestEqual(
		TEXT("Same-payload delivery retains the stable Crafting VM"),
		Widget->GetCraftingViewModel(),
		CraftingViewModel);
	TestEqual(
		TEXT("Same-payload delivery retains the stable Pane-owned player VM"),
		Widget->GetCraftingPlayerInventoryViewModel(),
		PlayerViewModel);
	TestEqual(
		TEXT("Same-payload delivery retains the Pane VM"),
		OutputPane->GetPanelViewModel(),
		OutputPaneViewModel);

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
		TEXT("Context switch binds station B's output inventory"),
		OutputPane->GetBoundInventory(),
		ContextB.OutputInventory.Get());
	TestEqual(
		TEXT("Context switch retains the screen-owned Crafting VM"),
		Widget->GetCraftingViewModel(),
		CraftingViewModel);
	TestEqual(
		TEXT("Context switch retains the Pane-owned player VM"),
		Widget->GetCraftingPlayerInventoryViewModel(),
		PlayerViewModel);
	TestEqual(
		TEXT("Context switch retains the Pane-owned panel VM"),
		OutputPane->GetPanelViewModel(),
		OutputPaneViewModel);

	Widget->DeactivateWidget();
	TestFalse(
		TEXT("Crafting screen deactivates through CommonUI"),
		Widget->IsActivated());
	TestNull(
		TEXT("Deactivation releases the retained payload"),
		Widget->GetCraftingScreenPayload());
	TestNull(
		TEXT("Deactivation releases output observation"),
		OutputPane->GetBoundInventory());
	TestNull(
		TEXT("Deactivation unbinds the output Pane VM"),
		OutputPaneViewModel->GetObservedInventory());
	TestFalse(
		TEXT("Deactivation invalidates the output root"),
		Widget->GetOutputPaneContainerHandle().IsValid());
	TestEqual(
		TEXT("Deactivation retains the stable Crafting VM"),
		Widget->GetCraftingViewModel(),
		CraftingViewModel);
	TestEqual(
		TEXT("Deactivation retains the stable Pane-owned player VM"),
		Widget->GetCraftingPlayerInventoryViewModel(),
		PlayerViewModel);
	TestEqual(
		TEXT("Deactivation retains the stable Pane VM"),
		OutputPane->GetPanelViewModel(),
		OutputPaneViewModel);
	TestEqual(
		TEXT("Deactivation clears every screen-owned quick-transfer route"),
		GetQuickTransferRouteCount(Coordinator),
		0);

	Widget->ActivateWidget();
	TestTrue(
		TEXT("Pooled Crafting screen can reactivate"),
		Widget->IsActivated());
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
	TestEqual(
		TEXT("Fresh pooled bind reuses the stable Crafting VM"),
		Widget->GetCraftingViewModel(),
		CraftingViewModel);
	TestEqual(
		TEXT("Fresh pooled bind reuses the stable Pane-owned player VM"),
		Widget->GetCraftingPlayerInventoryViewModel(),
		PlayerViewModel);
	TestEqual(
		TEXT("Fresh pooled bind reuses the stable Pane VM"),
		OutputPane->GetPanelViewModel(),
		OutputPaneViewModel);

	URpgCraftingStationScreenPayload* MismatchedOutputPayload =
		MakePayload(
			Widget,
			TestWorld.GetPlayerInventory(),
			ContextB,
			TestWorld.GetPawn());
	MismatchedOutputPayload->SecondaryInventory =
		ContextA.OutputInventory;
	MismatchedOutputPayload->OutputInventory =
		ContextA.OutputInventory;
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		Widget,
		MismatchedOutputPayload);
	TestNull(
		TEXT("Station/output mismatch rejects and clears the active payload"),
		Widget->GetCraftingScreenPayload());
	TestNull(
		TEXT("Rejected payload releases output presentation"),
		OutputPane->GetBoundInventory());
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
	TestEqual(
		TEXT("Generic payload rejection performs no bind"),
		Widget->GetCraftingPresentationBindGeneration(),
		3u);

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
	TestNull(
		TEXT("Explicit payload clear releases the output Pane"),
		OutputPane->GetBoundInventory());
	TestEqual(
		TEXT("Explicit payload clear performs no extra bind"),
		Widget->GetCraftingPresentationBindGeneration(),
		4u);

	Widget->DeactivateWidget();
	SlateWidget.Reset();
	return true;
}

#endif
