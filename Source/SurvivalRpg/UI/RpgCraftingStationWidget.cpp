#include "RpgCraftingStationWidget.h"

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonLazyImage.h"
#include "CommonListView.h"
#include "CommonTextBlock.h"
#include "Components/EditableTextBox.h"
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
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventoryScreenPresentationContext.h"
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
	/** Mirrors the view model's choice into a list's single selection, so selectable rows show their selected style. */
	template <typename ItemType, typename PredicateType>
	void SyncListSelection(UCommonListView* ListView, const TArray<ItemType*>& Items, PredicateType IsChosen)
	{
		if (!ListView)
		{
			return;
		}
		for (ItemType* Item : Items)
		{
			if (Item && IsChosen(Item))
			{
				if (ListView->GetSelectedItem() != Item)
				{
					ListView->SetSelectedItem(Item);
				}
				return;
			}
		}
		ListView->ClearSelection();
	}

	template <typename ItemType>
	void ReconcileListItems(
		UCommonListView* ListView,
		const TArray<ItemType*>& DesiredItems)
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
	ValidateCommonInputActionRow(
		CompileLog,
		StopOrderInputAction,
		LOCTEXT("StopOrderInputActionLabel", "StopOrderInputAction"),
		/*bRequired=*/ false);
	if (TierSectionEntryClass && !TierSectionEntryClass->ImplementsInterface(UUserObjectListEntry::StaticClass()))
	{
		CompileLog.Error(LOCTEXT("TierSectionEntryClassInvalid", "TierSectionEntryClass must implement UserObjectListEntry."));
	}
}

#endif

void URpgCraftingStationWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsureCraftingViewModels();

	if (CraftButton)
	{
		CraftButton->SetCraftButtonText(
			NSLOCTEXT("RpgCrafting", "DefaultStartAction", "Start crafting"));
	}
	if (QuantityMinusButton)
	{
		QuantityMinusButton->SetCraftButtonText(FText::FromString(TEXT("-")));
	}
	if (QuantityPlusButton)
	{
		QuantityPlusButton->SetCraftButtonText(FText::FromString(TEXT("+")));
	}
	if (QuantityMaxButton)
	{
		QuantityMaxButton->SetCraftButtonText(
			NSLOCTEXT("RpgCrafting", "CraftMaxButton", "Max"));
	}
	if (StopOrderButton)
	{
		StopOrderButton->SetCraftButtonText(
			NSLOCTEXT("RpgCrafting", "StopOrderButton", "Stop remaining"));
	}
	SetPopupOpen(TierFilterPopup, false);
	SetPopupOpen(TargetStoragePopup, false);
	RefreshDropdownLabels();

	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
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
	StopOrderProgressRefresh();
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
	if (!bCraftingContextBound || !PlayerInventoryPane)
	{
		return;
	}

	FRpgInventoryScreenPresentationContext Context;
	Context.DragDropCoordinator = GetScreenDragDropCoordinator();
	Context.PanelNavigationCoordinator = GetScreenPanelNavigationCoordinator();
	Context.PresentationHost = this;
	PlayerInventoryPane->SetInteractionContext(
		Context,
		TEXT("Player"));
}

void URpgCraftingStationWidget::RegisterInventoryScreenNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (Navigator && bCraftingContextBound && PlayerInventoryPane)
	{
		PlayerInventoryPane->RegisterNavigationPanels(Navigator);
	}
}

void URpgCraftingStationWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->AppendSpatialGrids(OutGrids);
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

FText URpgCraftingStationWidget::ResolveQuickTransferDisplayName() const
{
	return Super::ResolveQuickTransferDisplayName();
}

void URpgCraftingStationWidget::RequestStartCraftingOrder()
{
	if (!bCraftingContextBound || !CraftingViewModel ||
		!CraftingStation || !CraftingViewModel->CanStartOrder())
	{
		return;
	}

	URpgCraftingRecipeDefinition* Recipe =
		CraftingViewModel->GetSelectedRecipe();
	const int32 Quantity = CraftingViewModel->GetCraftQuantity();
	const FName TargetId = CraftingViewModel->GetSelectedTargetContainerId();
	if (!Recipe || Quantity <= 0 || TargetId.IsNone())
	{
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		UiActions->RequestStartCraftingOrder(
			CraftingStation,
			Recipe,
			Quantity,
			TargetId);
	}
}

void URpgCraftingStationWidget::RequestStopCraftingOrder()
{
	if (!bCraftingContextBound || !CraftingStation || !CraftingStation->HasCraftingOrder())
	{
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		UiActions->RequestStopCraftingOrder(
			CraftingStation,
			CraftingStation->GetCurrentOrder().OrderId);
	}
}

void URpgCraftingStationWidget::RequestToggleCraftingPause()
{
	if (!bCraftingContextBound || !CraftingStation || !CraftingStation->HasCraftingOrder())
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

void URpgCraftingStationWidget::RequestSelectTargetStorage(FName ContainerId)
{
	if (!bCraftingContextBound || !CraftingViewModel || ContainerId.IsNone())
	{
		return;
	}

	CraftingViewModel->SelectTargetStorage(ContainerId);
	if (CraftingStation && CraftingStation->HasCraftingOrder() &&
		CraftingStation->GetCurrentOrder().TargetContainerId != ContainerId)
	{
		if (URpgInventoryUiActionComponent* UiActions =
			ResolveInventoryUiActionComponent())
		{
			UiActions->RequestSetCraftingOrderTarget(
				CraftingStation,
				CraftingStation->GetCurrentOrder().OrderId,
				ContainerId);
		}
	}
}

void URpgCraftingStationWidget::RequestCycleTargetStorage(int32 Direction)
{
	if (!bCraftingContextBound || !CraftingViewModel)
	{
		return;
	}

	const TArray<URpgCraftingStorageOptionViewModel*> Options =
		CraftingViewModel->GetTargetStorageOptions();
	if (Options.IsEmpty() || Direction == 0)
	{
		return;
	}
	const FName Current = CraftingViewModel->GetSelectedTargetContainerId();
	int32 Index = Options.IndexOfByPredicate([Current](const URpgCraftingStorageOptionViewModel* Option)
	{
		return Option && Option->GetContainerId() == Current;
	});
	Index = (FMath::Max(0, Index) + (Direction > 0 ? 1 : -1) + Options.Num()) % Options.Num();
	if (Options[Index])
	{
		RequestSelectTargetStorage(Options[Index]->GetContainerId());
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
		CraftingStation != NewPayload->CraftingStation ||
		RequestingActor != NewPayload->RequestingActor;
	if (bContextChanged)
	{
		ResetCraftingContext();
	}

	CraftingScreenPayload = NewPayload;
	PlayerInventory = NewPayload->PlayerInventory;
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
	return Payload &&
		Payload->ScreenTag == RpgGameplayTags::UI_Screen_Crafting &&
		Payload->PlayerInventory &&
		Payload->PrimaryInventory == Payload->PlayerInventory &&
		Payload->CraftingStation &&
		Payload->ContextComponent == Payload->CraftingStation &&
		Payload->ContextActor == Payload->CraftingStation->GetOwner() &&
		Payload->RequestingActor;
}

bool URpgCraftingStationWidget::BindCraftingContext()
{
	if (bCraftingContextBound ||
		!IsActivated() ||
		!CraftingScreenPayload ||
		!PlayerInventory ||
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
	if (!Coordinator || !Navigator)
	{
		UE_LOG(
			LogRpgCraftingStationWidget,
			Error,
			TEXT("%s rejected Crafting presentation because the screen interaction context is missing."),
			*GetNameSafe(this));
		ResetCraftingContext();
		return false;
	}

	if (GetOwningPlayer())
	{
		URpgInventoryManagerComponent* CanonicalPlayerInventory =
			Coordinator->GetPlayerInventory();
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

	EnsureCraftingViewModels();

	// Arm the lifecycle guard before either VM can synchronously notify the screen during its first projection build.
	bCraftingContextBound = true;
	if (PlayerInventoryPane)
	{
		FRpgInventoryScreenPresentationContext PanePresentationContext;
		PanePresentationContext.DragDropCoordinator = Coordinator;
		PanePresentationContext.PanelNavigationCoordinator = Navigator;
		PanePresentationContext.PresentationHost = this;
		PlayerInventoryPane->BindPlayerInventory(
			GetOwningPlayer(),
			PanePresentationContext,
			TEXT("Player"));
	}
	if (CraftingViewModel)
	{
		CraftingViewModel->SetPresentationCatalog(CategoryCatalog);
		CraftingViewModel->BindCraftingStation(
			CraftingStation,
			RequestingActor);
	}

	RefreshCategoryItems();
	RefreshTierOptionItems();
	RefreshRecipeItems();
	RefreshSelectedRecipePresentation();
	RefreshStationHeaderPresentation();
	StartOrderProgressRefresh();
	++CraftingPresentationBindGeneration;
	return true;
}

void URpgCraftingStationWidget::ResetCraftingContext()
{
	bCraftingContextBound = false;
	StopOrderProgressRefresh();

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
	if (CraftingViewModel)
	{
		CraftingViewModel->UnbindCraftingStation();
	}
	UCommonListView* Lists[] = {
		RecipeList,
		IngredientList,
		CategoryList,
		TierFilterList,
		TargetStorageList,
		PreviewStatList
	};
	for (UCommonListView* List : Lists)
	{
		if (List)
		{
			List->ClearListItems();
		}
	}
	SetPopupOpen(TierFilterPopup, false);
	SetPopupOpen(TargetStoragePopup, false);

	CraftingScreenPayload = nullptr;
	PlayerInventory = nullptr;
	CraftingStation = nullptr;
	RequestingActor = nullptr;
	RefreshSelectedRecipePresentation();
	RefreshStationHeaderPresentation();
	RefreshCraftingActionAvailability();
}

void URpgCraftingStationWidget::RefreshStationHeaderPresentation()
{
	if (!StationIcon)
	{
		return;
	}

	const TSoftObjectPtr<UTexture2D> Icon =
		bCraftingContextBound && CraftingViewModel
			? CraftingViewModel->GetStationIcon()
			: TSoftObjectPtr<UTexture2D>();
	if (Icon.IsNull())
	{
		StationIcon->SetBrushFromTexture(nullptr);
		StationIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// The texture size lets an authored ScaleBox keep the icon's aspect ratio.
	StationIcon->SetBrushFromLazyTexture(Icon, /*bMatchSize=*/ true);
	StationIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
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
		CraftingViewModel->OnCategoriesChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleCategoriesChanged);
		CraftingViewModel->OnTierOptionsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleTierOptionsChanged);
		CraftingViewModel->OnSelectedRecipeDetailsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleSelectedRecipeDetailsChanged);
	}
}

void URpgCraftingStationWidget::UnbindViewModelDelegates()
{
	if (CraftingViewModel)
	{
		CraftingViewModel->OnRecipesChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRecipesChanged);
		CraftingViewModel->OnCategoriesChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCategoriesChanged);
		CraftingViewModel->OnTierOptionsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleTierOptionsChanged);
		CraftingViewModel->OnSelectedRecipeDetailsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleSelectedRecipeDetailsChanged);
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
		RecipeList->OnGetEntryClassForItem().BindUObject(
			this,
			&ThisClass::HandleGetRecipeEntryClass);
		RecipeList->OnIsItemSelectableOrNavigable().BindUObject(
			this,
			&ThisClass::HandleIsRecipeItemSelectable);
	}
	if (CategoryList)
	{
		CategoryList->OnItemClicked().AddUObject(
			this,
			&ThisClass::HandleCategoryItemClicked);
	}
	if (TierFilterList)
	{
		TierFilterList->OnItemClicked().AddUObject(
			this,
			&ThisClass::HandleTierOptionClicked);
	}
	if (TargetStorageList)
	{
		TargetStorageList->OnItemClicked().AddUObject(
			this,
			&ThisClass::HandleTargetOptionClicked);
	}
	if (RecipeSearchBox)
	{
		RecipeSearchBox->OnTextChanged.AddDynamic(
			this,
			&ThisClass::HandleSearchTextChanged);
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
	if (StopOrderButton)
	{
		StopOrderButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleStopOrderClicked);
	}
	if (SortDirectionButton)
	{
		SortDirectionButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleSortDirectionClicked);
	}
	if (TierFilterButton)
	{
		TierFilterButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleTierFilterButtonClicked);
	}
	if (TargetStorageButton)
	{
		TargetStorageButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleTargetStorageButtonClicked);
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
	if (QuantityMaxButton)
	{
		QuantityMaxButton->OnClicked().AddUObject(
			this,
			&ThisClass::HandleQuantityMaxClicked);
	}
}

void URpgCraftingStationWidget::UnbindAuthoredControlEvents()
{
	if (RecipeList)
	{
		RecipeList->OnItemSelectionChanged().RemoveAll(this);
		RecipeList->OnGetEntryClassForItem().Unbind();
		RecipeList->OnIsItemSelectableOrNavigable().Unbind();
	}
	UCommonListView* ClickLists[] = {
		CategoryList,
		TierFilterList,
		TargetStorageList
	};
	for (UCommonListView* List : ClickLists)
	{
		if (List)
		{
			List->OnItemClicked().RemoveAll(this);
		}
	}
	if (RecipeSearchBox)
	{
		RecipeSearchBox->OnTextChanged.RemoveDynamic(
			this,
			&ThisClass::HandleSearchTextChanged);
	}

	URpgCraftingActionButtonWidget* Buttons[] = {
		CraftButton,
		PauseButton,
		StopOrderButton,
		SortDirectionButton,
		TierFilterButton,
		TargetStorageButton,
		QuantityMinusButton,
		QuantityPlusButton,
		QuantityMaxButton
	};
	for (URpgCraftingActionButtonWidget* Button : Buttons)
	{
		if (Button)
		{
			Button->OnClicked().RemoveAll(this);
		}
	}
}

void URpgCraftingStationWidget::RefreshRecipeItems()
{
	const TArray<UObject*> Items =
		bCraftingContextBound && CraftingViewModel
			? CraftingViewModel->GetRecipeListItems()
			: TArray<UObject*>();
	ReconcileListItems(RecipeList, Items);

	if (!RecipeList || Items.IsEmpty())
	{
		return;
	}

	URpgCraftingRecipeDefinition* SelectedRecipe =
		CraftingViewModel ? CraftingViewModel->GetSelectedRecipe() : nullptr;
	for (UObject* Item : Items)
	{
		const URpgCraftingRecipeViewModel* RecipeRow = Cast<URpgCraftingRecipeViewModel>(Item);
		if (RecipeRow && RecipeRow->GetRecipeDefinition() == SelectedRecipe)
		{
			if (RecipeList->GetSelectedItem() != Item)
			{
				RecipeList->SetSelectedItem(Item);
			}
			break;
		}
	}
}

void URpgCraftingStationWidget::RefreshCategoryItems()
{
	const TArray<URpgCraftingCategoryViewModel*> Rows =
		bCraftingContextBound && CraftingViewModel
			? CraftingViewModel->GetCategoryRows()
			: TArray<URpgCraftingCategoryViewModel*>();
	ReconcileListItems(CategoryList, Rows);
	const FGameplayTag Filter = CraftingViewModel ? CraftingViewModel->GetCategoryFilter() : FGameplayTag();
	SyncListSelection(CategoryList, Rows, [&Filter](const URpgCraftingCategoryViewModel* Row)
	{
		return Filter.IsValid()
			? Row->GetKind() != ERpgCraftingCategoryRowKind::All && Row->GetCategoryTag() == Filter
			: Row->GetKind() == ERpgCraftingCategoryRowKind::All;
	});
}

void URpgCraftingStationWidget::RefreshTierOptionItems()
{
	const TArray<URpgCraftingTierOptionViewModel*> Options =
		bCraftingContextBound && CraftingViewModel
			? CraftingViewModel->GetTierOptions()
			: TArray<URpgCraftingTierOptionViewModel*>();
	ReconcileListItems(TierFilterList, Options);
	const int32 Tier = CraftingViewModel ? CraftingViewModel->GetTierFilter() : 0;
	SyncListSelection(TierFilterList, Options, [Tier](const URpgCraftingTierOptionViewModel* Option)
	{
		return Option->GetTier() == Tier;
	});
}

void URpgCraftingStationWidget::RefreshSelectedRecipePresentation()
{
	URpgCraftingRecipeDefinition* Recipe =
		bCraftingContextBound && CraftingViewModel
			? CraftingViewModel->GetSelectedRecipe()
			: nullptr;

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
	if (CraftQuantityText)
	{
		CraftQuantityText->SetText(
			Recipe && CraftingViewModel ? FText::AsNumber(CraftingViewModel->GetCraftQuantity()) : FText::GetEmpty());
	}
	if (CraftButton)
	{
		const FText StartText =
			bCraftingContextBound && CraftingViewModel ? CraftingViewModel->GetStartActionText() : FText::GetEmpty();
		CraftButton->SetCraftButtonText(
			StartText.IsEmpty()
				? NSLOCTEXT("RpgCrafting", "DefaultStartAction", "Start crafting")
				: StartText);
	}
	if (RecipeIcon)
	{
		const TSoftObjectPtr<UTexture2D> Icon = Recipe && CraftingViewModel
			? CraftingViewModel->GetPreviewIcon()
			: TSoftObjectPtr<UTexture2D>();
		if (!Icon.IsNull())
		{
			// The texture size lets the authored ScaleBox keep portrait item icons at their aspect ratio.
			RecipeIcon->SetBrushFromLazyTexture(Icon, /*bMatchSize=*/ true);
		}
		else
		{
			RecipeIcon->SetBrushFromTexture(nullptr);
		}
	}

	const bool bHasDetails = bCraftingContextBound && CraftingViewModel;
	ReconcileListItems(
		IngredientList,
		Recipe && bHasDetails ? CraftingViewModel->GetSelectedIngredients() : TArray<URpgCraftingIngredientViewModel*>());
	ReconcileListItems(
		PreviewStatList,
		Recipe && bHasDetails ? CraftingViewModel->GetPreviewRows() : TArray<URpgCraftingDetailRowViewModel*>());
	const TArray<URpgCraftingStorageOptionViewModel*> TargetOptions =
		bHasDetails ? CraftingViewModel->GetTargetStorageOptions() : TArray<URpgCraftingStorageOptionViewModel*>();
	ReconcileListItems(TargetStorageList, TargetOptions);
	const FName TargetId = bHasDetails ? CraftingViewModel->GetSelectedTargetContainerId() : NAME_None;
	SyncListSelection(TargetStorageList, TargetOptions, [TargetId](const URpgCraftingStorageOptionViewModel* Option)
	{
		return Option->GetContainerId() == TargetId;
	});
	RefreshCategoryItems();
	RefreshTierOptionItems();
	RefreshCraftingActionAvailability();
}

void URpgCraftingStationWidget::RefreshCraftingActionAvailability()
{
	const bool bHasContext =
		bCraftingContextBound && CraftingStation && CraftingViewModel;
	const bool bHasOrder = bHasContext && CraftingStation->HasCraftingOrder();
	const bool bAccess = bHasContext && RequestingActor && CraftingStation->CanActorAccess(RequestingActor);
	const int32 Quantity = bHasContext ? CraftingViewModel->GetCraftQuantity() : 0;
	const bool bHasSelection = bHasContext && CraftingViewModel->GetSelectedRecipe();
	const int32 MaxOrderQuantity = bHasContext ? CraftingStation->GetMaxOrderQuantity() : 0;

	if (CraftButton)
	{
		CraftButton->SetIsEnabled(bHasContext && CraftingViewModel->CanStartOrder());
	}
	if (PauseButton)
	{
		PauseButton->SetIsEnabled(bHasOrder && bAccess);
		PauseButton->SetCraftButtonText(
			bHasOrder && CraftingStation->IsCraftingPaused()
				? NSLOCTEXT("RpgCrafting", "ResumeCraftingButton", "Resume")
				: NSLOCTEXT("RpgCrafting", "PauseCraftingButton", "Pause"));
	}
	if (StopOrderButton)
	{
		StopOrderButton->SetIsEnabled(bHasOrder && bAccess);
	}

	// The quantity controls keep their authored visibility so the details layout does not jump; they only disable.
	if (QuantityMinusButton)
	{
		QuantityMinusButton->SetIsEnabled(bHasSelection && Quantity > 1);
	}
	if (QuantityPlusButton)
	{
		QuantityPlusButton->SetIsEnabled(bHasSelection && Quantity < MaxOrderQuantity);
	}
	if (QuantityMaxButton)
	{
		QuantityMaxButton->SetIsEnabled(bHasSelection);
	}
	if (SortDirectionButton)
	{
		SortDirectionButton->SetIsEnabled(bHasContext);
	}
	if (TierFilterButton)
	{
		TierFilterButton->SetIsEnabled(bHasContext);
	}
	if (TargetStorageButton)
	{
		TargetStorageButton->SetIsEnabled(bHasContext && !CraftingViewModel->GetTargetStorageOptions().IsEmpty());
	}
	RefreshDropdownLabels();

	if (CraftActionBinding.IsValid())
	{
		CraftActionBinding.SetDisplayInActionBar(bHasContext && CraftingViewModel->CanStartOrder());
	}
	if (TogglePauseActionBinding.IsValid())
	{
		TogglePauseActionBinding.SetDisplayInActionBar(bHasOrder);
	}
	if (StopOrderActionBinding.IsValid())
	{
		StopOrderActionBinding.SetDisplayInActionBar(bHasOrder);
	}
}

void URpgCraftingStationWidget::StartOrderProgressRefresh()
{
	StopOrderProgressRefresh();
	if (UWorld* World = GetWorld();
		World && OrderProgressRefreshInterval > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			OrderProgressTimer,
			this,
			&ThisClass::HandleOrderProgressTimer,
			FMath::Max(0.05f, OrderProgressRefreshInterval),
			true);
	}
}

void URpgCraftingStationWidget::StopOrderProgressRefresh()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OrderProgressTimer);
	}
	OrderProgressTimer.Invalidate();
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
					&ThisClass::RequestStartCraftingOrder)));
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
	if (IsActionRowValid(StopOrderInputAction))
	{
		StopOrderActionBinding = RegisterUIActionBinding(
			FBindUIActionArgs(
				StopOrderInputAction,
				true,
				FSimpleDelegate::CreateUObject(
					this,
					&ThisClass::RequestStopCraftingOrder)));
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
	if (StopOrderActionBinding.IsValid())
	{
		StopOrderActionBinding.Unregister();
	}
	CraftActionBinding = FUIActionBindingHandle();
	TogglePauseActionBinding = FUIActionBindingHandle();
	StopOrderActionBinding = FUIActionBindingHandle();
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

TSubclassOf<UUserWidget> URpgCraftingStationWidget::HandleGetRecipeEntryClass(UObject* Item) const
{
	// Recipe rows fall back to the list's authored entry class.
	return Cast<URpgCraftingTierSectionViewModel>(Item) ? TierSectionEntryClass : nullptr;
}

bool URpgCraftingStationWidget::HandleIsRecipeItemSelectable(UObject* Item) const
{
	return Cast<URpgCraftingRecipeViewModel>(Item) != nullptr;
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

void URpgCraftingStationWidget::HandleCategoryItemClicked(UObject* Item)
{
	if (bCraftingContextBound && CraftingViewModel)
	{
		CraftingViewModel->ActivateCategoryRow(Cast<URpgCraftingCategoryViewModel>(Item));
	}
}

void URpgCraftingStationWidget::HandleTierOptionClicked(UObject* Item)
{
	const URpgCraftingTierOptionViewModel* Option = Cast<URpgCraftingTierOptionViewModel>(Item);
	if (bCraftingContextBound && CraftingViewModel && Option)
	{
		CraftingViewModel->SetTierFilter(Option->GetTier());
	}
	SetPopupOpen(TierFilterPopup, false);
	RefreshDropdownLabels();
}

void URpgCraftingStationWidget::HandleTargetOptionClicked(UObject* Item)
{
	if (const URpgCraftingStorageOptionViewModel* Option = Cast<URpgCraftingStorageOptionViewModel>(Item))
	{
		RequestSelectTargetStorage(Option->GetContainerId());
	}
	SetPopupOpen(TargetStoragePopup, false);
	RefreshDropdownLabels();
}

void URpgCraftingStationWidget::HandleTierFilterButtonClicked()
{
	if (TierFilterPopup)
	{
		SetPopupOpen(TierFilterPopup, !TierFilterPopup->IsVisible());
		SetPopupOpen(TargetStoragePopup, false);
	}
}

void URpgCraftingStationWidget::HandleTargetStorageButtonClicked()
{
	if (TargetStoragePopup)
	{
		SetPopupOpen(TargetStoragePopup, !TargetStoragePopup->IsVisible());
		SetPopupOpen(TierFilterPopup, false);
	}
}

void URpgCraftingStationWidget::SetPopupOpen(UWidget* Popup, bool bOpen)
{
	if (Popup)
	{
		Popup->SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void URpgCraftingStationWidget::RefreshDropdownLabels()
{
	const bool bHasContext = bCraftingContextBound && CraftingViewModel;
	if (SortDirectionButton)
	{
		SortDirectionButton->SetCraftButtonText(bHasContext ? CraftingViewModel->GetTierSortText() : FText::GetEmpty());
	}
	if (TierFilterButton)
	{
		TierFilterButton->SetCraftButtonText(bHasContext ? CraftingViewModel->GetTierFilterText() : FText::GetEmpty());
	}
	if (TargetStorageButton)
	{
		const FText TargetName = bHasContext ? CraftingViewModel->GetSelectedTargetName() : FText::GetEmpty();
		TargetStorageButton->SetCraftButtonText(
			TargetName.IsEmpty() ? NSLOCTEXT("RpgCrafting", "NoTargetStorage", "No connected chest") : TargetName);
	}
}

void URpgCraftingStationWidget::HandleCraftClicked()
{
	RequestStartCraftingOrder();
}

void URpgCraftingStationWidget::HandlePauseClicked()
{
	RequestToggleCraftingPause();
}

void URpgCraftingStationWidget::HandleStopOrderClicked()
{
	RequestStopCraftingOrder();
}

void URpgCraftingStationWidget::HandleSearchTextChanged(const FText& Text)
{
	if (bCraftingContextBound && CraftingViewModel)
	{
		CraftingViewModel->SetSearchText(Text);
	}
}

void URpgCraftingStationWidget::HandleSortDirectionClicked()
{
	if (bCraftingContextBound && CraftingViewModel)
	{
		CraftingViewModel->ToggleTierSortDirection();
	}
	RefreshDropdownLabels();
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

void URpgCraftingStationWidget::HandleQuantityMaxClicked()
{
	if (CraftingViewModel)
	{
		CraftingViewModel->SetCraftQuantityToMax();
	}
}

void URpgCraftingStationWidget::HandleOrderProgressTimer()
{
	if (bCraftingContextBound && CraftingViewModel)
	{
		CraftingViewModel->RefreshOrderProgress();
	}
}

void URpgCraftingStationWidget::HandleRecipesChanged()
{
	RefreshRecipeItems();
	RefreshSelectedRecipePresentation();
}

void URpgCraftingStationWidget::HandleCategoriesChanged()
{
	RefreshCategoryItems();
}

void URpgCraftingStationWidget::HandleTierOptionsChanged()
{
	RefreshTierOptionItems();
}

void URpgCraftingStationWidget::HandleSelectedRecipeDetailsChanged()
{
	RefreshSelectedRecipePresentation();
}

void URpgCraftingStationWidget::HandlePlayerInventoryPaneNavigationPanelsChanged()
{
	if (bCraftingContextBound)
	{
		QueueDeferredInventoryScreenRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
