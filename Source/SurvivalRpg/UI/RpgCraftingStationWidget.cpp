#include "RpgCraftingStationWidget.h"

#include "CommonLazyImage.h"
#include "CommonListView.h"
#include "CommonTextBlock.h"
#include "Components/CheckBox.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Mvvm/Crafting/RpgCraftingViewModels.h"
#include "SurvivalRpg/UI/RpgCraftingActionButtonWidget.h"
#include "SurvivalRpg/UI/RpgCraftingJobEntryWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"
#include "SurvivalRpg/UI/RpgInventorySpatialPaneWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryPaneWidget.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingStationWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgCraftingStationWidget, Log, All);

#define LOCTEXT_NAMESPACE "RpgCraftingStationWidget"

URpgPlayerInventoryViewModel*
URpgCraftingStationWidget::GetCraftingPlayerInventoryViewModel() const
{
	return PlayerInventoryPane
		? PlayerInventoryPane->GetPlayerInventoryViewModel()
		: nullptr;
}

namespace
{
	template <typename ViewModelType>
	void ReconcileListItems(
		UCommonListView* ListView,
		const TArray<ViewModelType*>& DesiredItems)
	{
		if (!ListView)
		{
			return;
		}

		const TArray<UObject*>& ExistingItems = ListView->GetListItems();
		bool bMatches = ExistingItems.Num() == DesiredItems.Num();
		for (int32 Index = 0; bMatches && Index < DesiredItems.Num(); ++Index)
		{
			bMatches = ExistingItems[Index] == DesiredItems[Index];
		}

		if (!bMatches)
		{
			ListView->SetListItems(DesiredItems);
		}
	}

	FText MakeCraftTimeText(float Seconds)
	{
		return FText::Format(
			NSLOCTEXT("RpgCrafting", "CraftTimeFormat", "{0} s"),
			FText::AsNumber(FMath::Max(0.0f, Seconds)));
	}
}

#if WITH_EDITOR

void URpgCraftingStationWidget::ValidateCompiledDefaults(
	IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);

	ValidateCommonInputActionRow(
		CompileLog,
		CraftInputAction,
		LOCTEXT("CraftInputActionLabel", "CraftInputAction"),
		/*bRequired=*/ true);
	ValidateCommonInputActionRow(
		CompileLog,
		TogglePauseInputAction,
		LOCTEXT("TogglePauseInputActionLabel", "TogglePauseInputAction"),
		/*bRequired=*/ true);
}

#endif

void URpgCraftingStationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureCraftingViewModels();

	if (CraftButton)
	{
		CraftButton->SetCraftButtonText(
			NSLOCTEXT("RpgCrafting", "CraftButton", "Craft"));
	}
	if (QuantityMinusButton)
	{
		QuantityMinusButton->SetCraftButtonText(FText::FromString(TEXT("-1")));
	}
	if (QuantityPlusButton)
	{
		QuantityPlusButton->SetCraftButtonText(FText::FromString(TEXT("+1")));
	}
	if (QuantityFiveButton)
	{
		QuantityFiveButton->SetCraftButtonText(FText::FromString(TEXT("5")));
	}
	if (QuantityTenButton)
	{
		QuantityTenButton->SetCraftButtonText(FText::FromString(TEXT("10")));
	}
	if (QuantityMaxButton)
	{
		QuantityMaxButton->SetCraftButtonText(
			NSLOCTEXT("RpgCrafting", "CraftMaxButton", "Max"));
	}

	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
	}
	if (OutputInventoryPane)
	{
		OutputInventoryPane->ReleaseInventoryPresentation();
	}
	RefreshSelectedRecipePresentation();
	RefreshCraftingActionAvailability();
}

void URpgCraftingStationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureCraftingViewModels();
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->OnNavigationPanelsChanged.RemoveAll(this);
		PlayerInventoryPane->OnNavigationPanelsChanged.AddUObject(
			this,
			&ThisClass::HandlePlayerInventoryPaneNavigationPanelsChanged);
	}
	BindViewModelDelegates();
	BindAuthoredControlEvents();
	RefreshSelectedRecipePresentation();
	RefreshCraftingActionAvailability();
}

void URpgCraftingStationWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RegisterCraftingActionBindings();
	RefreshCraftingActionAvailability();
}

void URpgCraftingStationWidget::NativeOnDeactivated()
{
	UnregisterCraftingActionBindings();
	Super::NativeOnDeactivated();
}

void URpgCraftingStationWidget::NativeDestruct()
{
	UnregisterCraftingActionBindings();
	StopJobProgressRefresh();
	UnbindAuthoredControlEvents();
	UnbindViewModelDelegates();
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->OnNavigationPanelsChanged.RemoveAll(this);
	}
	ResetCraftingContext();
	Super::NativeDestruct();
}

UWidget* URpgCraftingStationWidget::NativeGetDesiredFocusTarget() const
{
	if (RecipeList && RecipeList->GetNumItems() > 0)
	{
		return RecipeList;
	}
	if (OutputInventoryPane && OutputInventoryPane->GetSpatialGrid())
	{
		return OutputInventoryPane->GetSpatialGrid();
	}
	return Super::NativeGetDesiredFocusTarget();
}

void URpgCraftingStationWidget::ReceiveScreenPayload_Implementation(
	UObject* Payload)
{
	ApplyCraftingScreenPayload(Payload);
}

void URpgCraftingStationWidget::BindInventoryScreenPresentation()
{
	if (!BindCraftingContext())
	{
		ResetCraftingContext();
	}
}

void URpgCraftingStationWidget::UnbindInventoryScreenPresentation()
{
	ResetCraftingContext();
}

void URpgCraftingStationWidget::ForwardInventoryInteractionContextToChildren()
{
	if (!bCraftingContextBound)
	{
		return;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		GetScreenDragDropCoordinator();
	URpgInventoryPanelNavigationCoordinator* Navigator =
		GetScreenPanelNavigationCoordinator();

	if (PlayerInventoryPane)
	{
		FRpgInventoryScreenPresentationContext Context;
		Context.DragDropCoordinator = Coordinator;
		Context.PanelNavigationCoordinator = Navigator;
		Context.PresentationHost = this;
		PlayerInventoryPane->SetInteractionContext(
			Context,
			TEXT("Player"));
	}

	if (OutputInventoryPane)
	{
		OutputInventoryPane->SetInteractionContext(
			Coordinator,
			Navigator,
			TEXT("Crafting.Output"),
			this);
	}
}

void URpgCraftingStationWidget::RegisterInventoryScreenNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (!Navigator || !bCraftingContextBound)
	{
		return;
	}

	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->RegisterNavigationPanels(Navigator);
	}
	if (OutputInventoryPane)
	{
		OutputInventoryPane->RegisterNavigationPanel(Navigator);
	}
}

void URpgCraftingStationWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->AppendSpatialGrids(OutGrids);
	}
	if (OutputInventoryPane && OutputInventoryPane->GetSpatialGrid())
	{
		OutGrids.AddUnique(OutputInventoryPane->GetSpatialGrid());
	}
}

bool URpgCraftingStationWidget::RouteInventoryPayloadToScreenSpecificTarget(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	UWidget* Target = nullptr;
	if (!PlayerInventoryPane ||
		!PlayerInventoryPane->ResolveNonSpatialDropTarget(
			GhostCenterScreenPosition,
			Target) ||
		!Target)
	{
		return false;
	}

	bOutTargetAddressed = true;
	SwitchActivePointerDropTarget(Target);
	return PlayerInventoryPane->ApplyPayloadToNonSpatialDropTarget(
		Target,
		Payload,
		GhostCenterScreenPosition,
		bCommit);
}

void URpgCraftingStationWidget::ClearInventoryScreenSpecificDragPreviews()
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ClearExternalDragPreviews();
	}
}

bool URpgCraftingStationWidget::UpdateInventoryScreenSpecificControllerDragVisual(
	const FRpgInventoryDragPayload& Payload)
{
	FVector2D AnchorScreenPosition = FVector2D::ZeroVector;
	if (!PlayerInventoryPane ||
		!PlayerInventoryPane->ResolveControllerDragVisualAnchor(
			AnchorScreenPosition))
	{
		return false;
	}

	UpdateFreePointerDragVisual(
		Payload,
		AnchorScreenPosition,
		nullptr,
		true);
	return true;
}

void URpgCraftingStationWidget::RefreshInventoryScreenSpecificInteractionPresentation(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->RefreshInteractionPresentation(
			PreviewState,
			bHasPayload,
			bPendingRequest);
	}
}

void URpgCraftingStationWidget::RequestCraftSelectedRecipe()
{
	if (!bCraftingContextBound || !CraftingViewModel ||
		!CraftingStation || !CraftingViewModel->CanCraftSelectedRecipe())
	{
		return;
	}

	URpgCraftingRecipeDefinition* Recipe =
		CraftingViewModel->GetSelectedRecipe();
	const int32 Quantity = CraftingViewModel->GetCraftQuantity();
	if (!Recipe || Quantity <= 0)
	{
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		UiActions->RequestCraftRecipe(
			CraftingStation,
			Recipe,
			Quantity);
	}
}

void URpgCraftingStationWidget::RequestCancelCraftJob(
	URpgCraftingJobViewModel* JobViewModel)
{
	if (!bCraftingContextBound || !CraftingViewModel ||
		!CraftingStation || !JobViewModel ||
		!JobViewModel->CanCancelJob() ||
		!CraftingViewModel->GetJobs().Contains(JobViewModel))
	{
		return;
	}

	const FGuid JobId = JobViewModel->GetJobId();
	if (!JobId.IsValid())
	{
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		UiActions->RequestCancelCraftJob(CraftingStation, JobId);
	}
}

void URpgCraftingStationWidget::RequestToggleCraftingPause()
{
	if (!bCraftingContextBound || !CraftingStation)
	{
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		if (CraftingStation->IsCraftingPaused())
		{
			UiActions->RequestResumeCraftingStation(CraftingStation);
		}
		else
		{
			UiActions->RequestPauseCraftingStation(CraftingStation);
		}
	}
}

void URpgCraftingStationWidget::
	RequestSetCraftingOutputAutoDepositEnabled(bool bEnabled)
{
	if (!bCraftingContextBound || !CraftingStation ||
		CraftingStation->IsCraftingOutputAutoDepositEnabled() == bEnabled)
	{
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		UiActions->RequestSetCraftingOutputAutoDepositEnabled(
			CraftingStation,
			bEnabled);
	}
}

void URpgCraftingStationWidget::ApplyCraftingScreenPayload(UObject* Payload)
{
	URpgCraftingStationScreenPayload* NewPayload =
		Cast<URpgCraftingStationScreenPayload>(Payload);
	if (!IsPayloadCoherent(NewPayload))
	{
		ResetCraftingContext();
		return;
	}

	const bool bContextChanged =
		CraftingScreenPayload != NewPayload ||
		PlayerInventory != NewPayload->PlayerInventory ||
		OutputInventory != NewPayload->OutputInventory ||
		CraftingStation != NewPayload->CraftingStation ||
		RequestingActor != NewPayload->RequestingActor;
	if (bContextChanged)
	{
		ResetCraftingContext();
	}

	CraftingScreenPayload = NewPayload;
	PlayerInventory = NewPayload->PlayerInventory;
	OutputInventory = NewPayload->OutputInventory;
	CraftingStation = NewPayload->CraftingStation;
	RequestingActor = NewPayload->RequestingActor;

	if (!IsActivated() || bCraftingContextBound)
	{
		return;
	}

	if (BindCraftingContext())
	{
		ForwardInventoryInteractionContextToChildren();
		RefreshInventoryScreenNavigationPanels();
		RefreshInventoryControllerFocus();
	}
}

bool URpgCraftingStationWidget::IsPayloadCoherent(
	const URpgCraftingStationScreenPayload* Payload) const
{
	if (!Payload ||
		Payload->ScreenTag != RpgGameplayTags::UI_Screen_Crafting ||
		!Payload->PlayerInventory ||
		Payload->PrimaryInventory != Payload->PlayerInventory ||
		!Payload->OutputInventory ||
		Payload->SecondaryInventory != Payload->OutputInventory ||
		Payload->PlayerInventory == Payload->OutputInventory ||
		!Payload->CraftingStation ||
		Payload->ContextComponent != Payload->CraftingStation ||
		Payload->ContextActor != Payload->CraftingStation->GetOwner() ||
		Payload->CraftingStation->GetOutputInventory() !=
			Payload->OutputInventory ||
		!Payload->RequestingActor)
	{
		return false;
	}

	return true;
}

bool URpgCraftingStationWidget::BindCraftingContext()
{
	if (bCraftingContextBound ||
		!IsActivated() ||
		!CraftingScreenPayload ||
		!PlayerInventory ||
		!OutputInventory ||
		!CraftingStation ||
		!RequestingActor)
	{
		return false;
	}

	EnsureInventoryInteractionObjects();
	URpgInventoryDragDropCoordinator* Coordinator =
		GetScreenDragDropCoordinator();
	URpgInventoryPanelNavigationCoordinator* Navigator =
		GetScreenPanelNavigationCoordinator();
	if (!PlayerInventoryPane || !Coordinator || !Navigator)
	{
		UE_LOG(
			LogRpgCraftingStationWidget,
			Error,
			TEXT("%s rejected Crafting presentation because the required player pane or screen interaction context is missing."),
			*GetNameSafe(this));
		ResetCraftingContext();
		return false;
	}

	if (GetOwningPlayer())
	{
		URpgInventoryManagerComponent* CanonicalPlayerInventory =
			Coordinator ? Coordinator->GetPlayerInventory() : nullptr;
		APawn* OwningPawn = GetOwningPlayerPawn();
		if (!CanonicalPlayerInventory ||
			CanonicalPlayerInventory != PlayerInventory ||
			!OwningPawn ||
			RequestingActor != OwningPawn)
		{
			UE_LOG(
				LogRpgCraftingStationWidget,
				Warning,
				TEXT("%s rejected Crafting payload: player inventory or requesting pawn is not canonical for the owning player."),
				*GetNameSafe(this));
			ResetCraftingContext();
			return false;
		}

		PlayerInventory = CanonicalPlayerInventory;
	}

	if (!CraftingStation->CanActorAccess(RequestingActor))
	{
		UE_LOG(
			LogRpgCraftingStationWidget,
			Warning,
			TEXT("%s rejected Crafting payload because %s cannot access station %s."),
			*GetNameSafe(this),
			*GetNameSafe(RequestingActor),
			*GetNameSafe(CraftingStation->GetOwner()));
		ResetCraftingContext();
		return false;
	}

	OutputPaneContainerHandle = FRpgInventoryContainerHandle::MakeRoot(
		OutputInventory->GetDefaultContainerId());
	if (!OutputPaneContainerHandle.IsValid() ||
		!OutputInventoryPane ||
		!OutputInventoryPane->GetSpatialGrid())
	{
		UE_LOG(
			LogRpgCraftingStationWidget,
			Error,
			TEXT("%s rejected Crafting presentation because the authored output pane or output root is invalid."),
			*GetNameSafe(this));
		ResetCraftingContext();
		return false;
	}

	EnsureCraftingViewModels();
	if (OutputInventoryPane)
	{
		OutputInventoryPane->BindInventoryContainer(
			OutputInventory,
			OutputPaneContainerHandle);
	}
	if (OutputInventoryPane->GetBoundInventory() != OutputInventory ||
		OutputInventoryPane->GetBoundContainerHandle() !=
			OutputPaneContainerHandle)
	{
		UE_LOG(
			LogRpgCraftingStationWidget,
			Error,
			TEXT("%s rejected Crafting presentation because the output pane failed to bind the exact station-output root."),
			*GetNameSafe(this));
		ResetCraftingContext();
		return false;
	}

	// Arm the lifecycle guard before either VM can synchronously notify the screen during its first projection build.
	bCraftingContextBound = true;
	FRpgInventoryScreenPresentationContext PanePresentationContext;
	PanePresentationContext.DragDropCoordinator = Coordinator;
	PanePresentationContext.PanelNavigationCoordinator = Navigator;
	PanePresentationContext.PresentationHost = this;
	PlayerInventoryPane->BindPlayerInventory(
		GetOwningPlayer(),
		PanePresentationContext,
		TEXT("Player"));
	if (CraftingViewModel)
	{
		CraftingViewModel->BindCraftingStation(
			CraftingStation,
			RequestingActor);
	}

	RefreshRecipeItems();
	RefreshSelectedRecipePresentation();
	RefreshJobItems();
	ConfigureQuickTransferRoutes();
	StartJobProgressRefresh();
	++CraftingPresentationBindGeneration;
	return true;
}

void URpgCraftingStationWidget::ResetCraftingContext()
{
	bCraftingContextBound = false;
	StopJobProgressRefresh();

	if (URpgInventoryDragDropCoordinator* Coordinator =
		GetScreenDragDropCoordinator())
	{
		Coordinator->ForceCancelInteraction();
		Coordinator->ClearQuickTransferTargets();
		Coordinator->SetFocusedInventory(nullptr);
	}
	if (URpgInventoryPanelNavigationCoordinator* Navigator =
		GetScreenPanelNavigationCoordinator())
	{
		Navigator->ClearPanels();
	}

	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
	}
	if (OutputInventoryPane)
	{
		OutputInventoryPane->ReleaseInventoryPresentation();
	}
	if (CraftingViewModel)
	{
		CraftingViewModel->UnbindCraftingStation();
	}
	if (RecipeList)
	{
		RecipeList->ClearListItems();
	}
	if (IngredientList)
	{
		IngredientList->ClearListItems();
	}
	if (CraftingJobsList)
	{
		CraftingJobsList->ClearListItems();
	}

	CraftingScreenPayload = nullptr;
	PlayerInventory = nullptr;
	OutputInventory = nullptr;
	CraftingStation = nullptr;
	RequestingActor = nullptr;
	OutputPaneContainerHandle = FRpgInventoryContainerHandle();
	RefreshSelectedRecipePresentation();
	RefreshCraftingActionAvailability();
}

void URpgCraftingStationWidget::EnsureCraftingViewModels()
{
	if (!CraftingViewModel)
	{
		CraftingViewModel =
			NewObject<URpgCraftingStationViewModel>(this);
	}
}

void URpgCraftingStationWidget::BindViewModelDelegates()
{
	UnbindViewModelDelegates();

	if (CraftingViewModel)
	{
		CraftingViewModel->OnRecipesChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleRecipesChanged);
		CraftingViewModel->OnSelectedRecipeDetailsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleSelectedRecipeDetailsChanged);
		CraftingViewModel->OnJobsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleJobsChanged);
	}
}

void URpgCraftingStationWidget::UnbindViewModelDelegates()
{
	if (CraftingViewModel)
	{
		CraftingViewModel->OnRecipesChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRecipesChanged);
		CraftingViewModel->OnSelectedRecipeDetailsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleSelectedRecipeDetailsChanged);
		CraftingViewModel->OnJobsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleJobsChanged);
	}
}

void URpgCraftingStationWidget::BindAuthoredControlEvents()
{
	UnbindAuthoredControlEvents();

	if (RecipeList)
	{
		RecipeList->OnItemSelectionChanged().AddUObject(
			this,
			&ThisClass::HandleRecipeSelectionChanged);
	}
	if (CraftingJobsList)
	{
		CraftingJobsList->OnEntryWidgetGenerated().AddUObject(
			this,
			&ThisClass::HandleJobEntryGenerated);
		CraftingJobsList->OnEntryWidgetReleased().AddUObject(
			this,
			&ThisClass::HandleJobEntryReleased);
	}
	if (CraftButton)
	{
		CraftButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleCraftClicked);
	}
	if (PauseButton)
	{
		PauseButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandlePauseClicked);
	}
	if (QuantityMinusButton)
	{
		QuantityMinusButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleQuantityMinusClicked);
	}
	if (QuantityPlusButton)
	{
		QuantityPlusButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleQuantityPlusClicked);
	}
	if (QuantityFiveButton)
	{
		QuantityFiveButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleQuantityFiveClicked);
	}
	if (QuantityTenButton)
	{
		QuantityTenButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleQuantityTenClicked);
	}
	if (QuantityMaxButton)
	{
		QuantityMaxButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleQuantityMaxClicked);
	}
	if (AutoDepositCheckBox)
	{
		AutoDepositCheckBox->OnCheckStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleAutoDepositCheckStateChanged);
	}
}

void URpgCraftingStationWidget::UnbindAuthoredControlEvents()
{
	if (RecipeList)
	{
		RecipeList->OnItemSelectionChanged().RemoveAll(this);
	}
	if (CraftingJobsList)
	{
		CraftingJobsList->OnEntryWidgetGenerated().RemoveAll(this);
		CraftingJobsList->OnEntryWidgetReleased().RemoveAll(this);
	}

	URpgCraftingActionButtonWidget* Buttons[] = {
		CraftButton,
		PauseButton,
		QuantityMinusButton,
		QuantityPlusButton,
		QuantityFiveButton,
		QuantityTenButton,
		QuantityMaxButton
	};
	for (URpgCraftingActionButtonWidget* Button : Buttons)
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
	if (AutoDepositCheckBox)
	{
		AutoDepositCheckBox->OnCheckStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleAutoDepositCheckStateChanged);
	}
}

void URpgCraftingStationWidget::RefreshRecipeItems()
{
	const TArray<URpgCraftingRecipeViewModel*> Recipes =
		bCraftingContextBound && CraftingViewModel
			? CraftingViewModel->GetFilteredRecipes()
			: TArray<URpgCraftingRecipeViewModel*>();
	ReconcileListItems(RecipeList, Recipes);

	if (!RecipeList || Recipes.IsEmpty())
	{
		return;
	}

	URpgCraftingRecipeDefinition* SelectedRecipe =
		CraftingViewModel ? CraftingViewModel->GetSelectedRecipe() : nullptr;
	URpgCraftingRecipeViewModel* SelectedRow = nullptr;
	for (URpgCraftingRecipeViewModel* RecipeRow : Recipes)
	{
		if (RecipeRow &&
			RecipeRow->GetRecipeDefinition() == SelectedRecipe)
		{
			SelectedRow = RecipeRow;
			break;
		}
	}
	if (SelectedRow && RecipeList->GetSelectedItem() != SelectedRow)
	{
		RecipeList->SetSelectedItem(SelectedRow);
	}
}

void URpgCraftingStationWidget::RefreshSelectedRecipePresentation()
{
	URpgCraftingRecipeDefinition* Recipe =
		bCraftingContextBound && CraftingViewModel
			? CraftingViewModel->GetSelectedRecipe()
			: nullptr;
	const int32 Quantity =
		Recipe && CraftingViewModel
			? CraftingViewModel->GetCraftQuantity()
			: 0;

	if (RecipeNameText)
	{
		RecipeNameText->SetText(
			Recipe ? Recipe->DisplayName : FText::GetEmpty());
	}
	if (RecipeDescriptionText)
	{
		RecipeDescriptionText->SetText(
			Recipe ? Recipe->Description : FText::GetEmpty());
	}
	if (CraftTimeText)
	{
		CraftTimeText->SetText(
			Recipe && CraftingViewModel
				? MakeCraftTimeText(
					CraftingViewModel->GetSelectedTotalCraftTime())
				: FText::GetEmpty());
	}
	if (CraftQuantityText)
	{
		CraftQuantityText->SetText(
			Recipe ? FText::AsNumber(Quantity) : FText::GetEmpty());
	}
	if (RecipeIcon)
	{
		if (Recipe && !Recipe->Icon.IsNull())
		{
			RecipeIcon->SetBrushFromLazyTexture(Recipe->Icon);
		}
		else
		{
			RecipeIcon->SetBrushFromTexture(nullptr);
		}
	}

	const TArray<URpgCraftingIngredientViewModel*> Ingredients =
		Recipe && CraftingViewModel
			? CraftingViewModel->GetSelectedIngredients()
			: TArray<URpgCraftingIngredientViewModel*>();
	ReconcileListItems(IngredientList, Ingredients);
	RefreshCraftingActionAvailability();
}

void URpgCraftingStationWidget::RefreshJobItems()
{
	const TArray<URpgCraftingJobViewModel*> Jobs =
		bCraftingContextBound && CraftingViewModel
			? CraftingViewModel->GetJobs()
			: TArray<URpgCraftingJobViewModel*>();
	ReconcileListItems(CraftingJobsList, Jobs);
	RefreshCraftingActionAvailability();
}

void URpgCraftingStationWidget::RefreshCraftingActionAvailability()
{
	const bool bHasContext =
		bCraftingContextBound && CraftingStation && CraftingViewModel;
	const int32 Quantity =
		bHasContext ? CraftingViewModel->GetCraftQuantity() : 0;
	const int32 Maximum =
		bHasContext
			? CraftingViewModel->GetMaxSelectedCraftQuantity()
			: 0;

	if (CraftButton)
	{
		CraftButton->SetIsEnabled(
			bHasContext &&
			CraftingViewModel->CanCraftSelectedRecipe());
	}
	if (PauseButton)
	{
		PauseButton->SetIsEnabled(
			bHasContext &&
			RequestingActor &&
			CraftingStation->CanActorAccess(RequestingActor));
		PauseButton->SetCraftButtonText(
			bHasContext && CraftingStation->IsCraftingPaused()
				? NSLOCTEXT("RpgCrafting", "ResumeCraftingButton", "Resume")
				: NSLOCTEXT("RpgCrafting", "PauseCraftingButton", "Pause"));
	}

	const bool bShowQuantityOptions = Maximum > 1;
	const ESlateVisibility QuantityVisibility =
		bShowQuantityOptions
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed;
	if (QuantityMinusButton)
	{
		QuantityMinusButton->SetVisibility(QuantityVisibility);
		QuantityMinusButton->SetIsEnabled(Quantity > 1);
	}
	if (QuantityPlusButton)
	{
		QuantityPlusButton->SetVisibility(QuantityVisibility);
		QuantityPlusButton->SetIsEnabled(
			Maximum > 0 && Quantity < Maximum);
	}
	if (QuantityFiveButton)
	{
		QuantityFiveButton->SetVisibility(QuantityVisibility);
		QuantityFiveButton->SetIsEnabled(
			Maximum >= 5 && Quantity != 5);
	}
	if (QuantityTenButton)
	{
		QuantityTenButton->SetVisibility(QuantityVisibility);
		QuantityTenButton->SetIsEnabled(
			Maximum >= 10 && Quantity != 10);
	}
	if (QuantityMaxButton)
	{
		QuantityMaxButton->SetVisibility(QuantityVisibility);
		QuantityMaxButton->SetIsEnabled(
			Maximum > 1 && Quantity < Maximum);
	}

	if (AutoDepositCheckBox)
	{
		TGuardValue<bool> ApplyingGuard(
			bApplyingAutoDepositCheckState,
			true);
		AutoDepositCheckBox->SetIsEnabled(
			bHasContext &&
			CraftingStation->HasCraftingOutputAutoDepositAccess());
		AutoDepositCheckBox->SetIsChecked(
			bHasContext &&
			CraftingStation->IsCraftingOutputAutoDepositEnabled());
	}

	const bool bCanCraft =
		bHasContext && CraftingViewModel->CanCraftSelectedRecipe();
	if (CraftActionBinding.IsValid())
	{
		CraftActionBinding.SetDisplayInActionBar(bCanCraft);
	}
	if (TogglePauseActionBinding.IsValid())
	{
		TogglePauseActionBinding.SetDisplayInActionBar(bHasContext);
	}
}

void URpgCraftingStationWidget::ConfigureQuickTransferRoutes()
{
	URpgInventoryDragDropCoordinator* Coordinator =
		GetScreenDragDropCoordinator();
	if (!Coordinator)
	{
		return;
	}

	Coordinator->ClearQuickTransferTargets();
	if (OutputInventory && PlayerInventory &&
		OutputInventory != PlayerInventory)
	{
		// The station tray is withdrawal-only. The authoritative transfer policy independently rejects forged
		// Player -> Output requests, while this screen exposes only the intended convenience direction.
		Coordinator->SetQuickTransferTarget(
			OutputInventory,
			PlayerInventory);
	}
}

void URpgCraftingStationWidget::StartJobProgressRefresh()
{
	StopJobProgressRefresh();
	if (UWorld* World = GetWorld();
		World && JobProgressRefreshInterval > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			JobProgressTimer,
			this,
			&ThisClass::HandleJobProgressTimer,
			FMath::Max(0.05f, JobProgressRefreshInterval),
			true);
	}
}

void URpgCraftingStationWidget::StopJobProgressRefresh()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JobProgressTimer);
	}
	JobProgressTimer.Invalidate();
}

void URpgCraftingStationWidget::RegisterCraftingActionBindings()
{
	UnregisterCraftingActionBindings();

	if (IsActionRowValid(CraftInputAction))
	{
		CraftActionBinding = RegisterUIActionBinding(
			FBindUIActionArgs(
				CraftInputAction,
				true,
				FSimpleDelegate::CreateUObject(
					this,
					&ThisClass::RequestCraftSelectedRecipe)));
	}
	if (IsActionRowValid(TogglePauseInputAction))
	{
		TogglePauseActionBinding = RegisterUIActionBinding(
			FBindUIActionArgs(
				TogglePauseInputAction,
				true,
				FSimpleDelegate::CreateUObject(
					this,
					&ThisClass::RequestToggleCraftingPause)));
	}
}

void URpgCraftingStationWidget::UnregisterCraftingActionBindings()
{
	if (CraftActionBinding.IsValid())
	{
		CraftActionBinding.Unregister();
	}
	if (TogglePauseActionBinding.IsValid())
	{
		TogglePauseActionBinding.Unregister();
	}
	CraftActionBinding = FUIActionBindingHandle();
	TogglePauseActionBinding = FUIActionBindingHandle();
}

URpgInventoryUiActionComponent*
URpgCraftingStationWidget::ResolveInventoryUiActionComponent() const
{
	APlayerController* PlayerController = GetOwningPlayer();
	return PlayerController
		? PlayerController
			->FindComponentByClass<URpgInventoryUiActionComponent>()
		: nullptr;
}

void URpgCraftingStationWidget::HandleRecipeSelectionChanged(
	UObject* SelectedItem)
{
	URpgCraftingRecipeViewModel* RecipeRow =
		Cast<URpgCraftingRecipeViewModel>(SelectedItem);
	if (bCraftingContextBound && CraftingViewModel && RecipeRow)
	{
		CraftingViewModel->SelectRecipe(
			RecipeRow->GetRecipeDefinition());
	}
}

void URpgCraftingStationWidget::HandleJobEntryGenerated(
	UUserWidget& EntryWidget)
{
	if (URpgCraftingJobEntryWidget* JobEntry =
		Cast<URpgCraftingJobEntryWidget>(&EntryWidget))
	{
		JobEntry->SetCommandOwner(this);
	}
}

void URpgCraftingStationWidget::HandleJobEntryReleased(
	UUserWidget& EntryWidget)
{
	if (URpgCraftingJobEntryWidget* JobEntry =
		Cast<URpgCraftingJobEntryWidget>(&EntryWidget))
	{
		JobEntry->SetCommandOwner(nullptr);
	}
}

void URpgCraftingStationWidget::HandleCraftClicked()
{
	RequestCraftSelectedRecipe();
}

void URpgCraftingStationWidget::HandlePauseClicked()
{
	RequestToggleCraftingPause();
}

void URpgCraftingStationWidget::HandleQuantityMinusClicked()
{
	if (CraftingViewModel)
	{
		CraftingViewModel->IncreaseCraftQuantity(-1);
	}
}

void URpgCraftingStationWidget::HandleQuantityPlusClicked()
{
	if (CraftingViewModel)
	{
		CraftingViewModel->IncreaseCraftQuantity(1);
	}
}

void URpgCraftingStationWidget::HandleQuantityFiveClicked()
{
	if (CraftingViewModel)
	{
		CraftingViewModel->SetCraftQuantity(5);
	}
}

void URpgCraftingStationWidget::HandleQuantityTenClicked()
{
	if (CraftingViewModel)
	{
		CraftingViewModel->SetCraftQuantity(10);
	}
}

void URpgCraftingStationWidget::HandleQuantityMaxClicked()
{
	if (CraftingViewModel)
	{
		CraftingViewModel->SetCraftQuantityToMax();
	}
}

void URpgCraftingStationWidget::HandleJobProgressTimer()
{
	if (bCraftingContextBound && CraftingViewModel)
	{
		CraftingViewModel->RefreshJobs();
	}
}

void URpgCraftingStationWidget::HandleAutoDepositCheckStateChanged(
	bool bChecked)
{
	if (!bApplyingAutoDepositCheckState)
	{
		RequestSetCraftingOutputAutoDepositEnabled(bChecked);
	}
}

void URpgCraftingStationWidget::HandleRecipesChanged()
{
	RefreshRecipeItems();
	RefreshSelectedRecipePresentation();
}

void URpgCraftingStationWidget::HandleSelectedRecipeDetailsChanged()
{
	RefreshSelectedRecipePresentation();
}

void URpgCraftingStationWidget::HandleJobsChanged()
{
	RefreshJobItems();
}

void URpgCraftingStationWidget::HandlePlayerInventoryPaneNavigationPanelsChanged()
{
	if (bCraftingContextBound)
	{
		QueueDeferredInventoryScreenRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
