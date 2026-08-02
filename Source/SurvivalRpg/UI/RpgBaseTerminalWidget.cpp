#include "RpgBaseTerminalWidget.h"

#include "CommonListView.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Base/RpgPersonalStorageLockerActor.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDropCoordinator.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ContainmentProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"
#include "SurvivalRpg/UI/RpgBaseResourceListWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventorySpatialGridWidget.h"
#include "SurvivalRpg/UI/RpgInventorySpatialPaneWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryPaneWidget.h"
#include "SurvivalRpg/UI/RpgUIScreenBlueprintLibrary.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseTerminalWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgBaseTerminalWidget, Log, All);

#define LOCTEXT_NAMESPACE "RpgBaseTerminalWidget"

namespace RpgBaseTerminalWidget
{
FText GetDefinitionDisplayName(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
{
	const URpgInventoryItemDefinition* Definition =
		ItemDefinition ? ItemDefinition->GetDefaultObject<URpgInventoryItemDefinition>() : nullptr;
	return Definition && !Definition->DisplayName.IsEmpty()
		? Definition->DisplayName
		: FText::FromString(GetNameSafe(ItemDefinition.Get()));
}

FText GetOutcomeReason(ERpgBaseStorageResultCode Code)
{
	switch (Code)
	{
	case ERpgBaseStorageResultCode::Success:
		return LOCTEXT("OutcomeSuccess", "stored");
	case ERpgBaseStorageResultCode::Partial:
		return LOCTEXT("OutcomePartial", "partially stored; capacity reached");
	case ERpgBaseStorageResultCode::CapacityFull:
		return LOCTEXT("OutcomeCapacityFull", "left behind; capacity full");
	case ERpgBaseStorageResultCode::UnsupportedMode:
		return LOCTEXT("OutcomeUnsupported", "left behind; individual or unsupported item");
	case ERpgBaseStorageResultCode::NoAccess:
		return LOCTEXT("OutcomeNoAccess", "left behind; access lost");
	case ERpgBaseStorageResultCode::Stale:
		return LOCTEXT("OutcomeStale", "left behind; inventory changed");
	case ERpgBaseStorageResultCode::Conflict:
		return LOCTEXT("OutcomeConflict", "left behind; concurrent change");
	default:
		return LOCTEXT("OutcomeRejected", "left behind; rejected");
	}
}

FText GetCategoryLabel(ERpgInventoryItemCategory Category)
{
	switch (Category)
	{
	case ERpgInventoryItemCategory::Material: return LOCTEXT("CategoryMaterial", "Materials");
	case ERpgInventoryItemCategory::Weapon: return LOCTEXT("CategoryWeapon", "Weapons");
	case ERpgInventoryItemCategory::Shield: return LOCTEXT("CategoryShield", "Shields");
	case ERpgInventoryItemCategory::Armor: return LOCTEXT("CategoryArmor", "Armor");
	case ERpgInventoryItemCategory::Consumable: return LOCTEXT("CategoryConsumable", "Consumables");
	case ERpgInventoryItemCategory::Tool: return LOCTEXT("CategoryTool", "Tools");
	case ERpgInventoryItemCategory::Rune: return LOCTEXT("CategoryRune", "Runes");
	case ERpgInventoryItemCategory::Quest: return LOCTEXT("CategoryQuest", "Quest");
	case ERpgInventoryItemCategory::Misc: return LOCTEXT("CategoryMisc", "Miscellaneous");
	case ERpgInventoryItemCategory::None:
	default:
		return LOCTEXT("CategoryAll", "All categories");
	}
}

FText GetSortLabel(
	ERpgBaseResourceLocalSortMode SortMode,
	bool bDescending)
{
	FText Mode;
	switch (SortMode)
	{
	case ERpgBaseResourceLocalSortMode::Name: Mode = LOCTEXT("SortName", "Name"); break;
	case ERpgBaseResourceLocalSortMode::Category: Mode = LOCTEXT("SortCategory", "Category"); break;
	case ERpgBaseResourceLocalSortMode::StoredCount: Mode = LOCTEXT("SortStored", "Stored"); break;
	case ERpgBaseResourceLocalSortMode::FreeCapacity: Mode = LOCTEXT("SortFree", "Free space"); break;
	case ERpgBaseResourceLocalSortMode::FillRatio: Mode = LOCTEXT("SortFill", "Fill level"); break;
	case ERpgBaseResourceLocalSortMode::ReplicatedOrder:
	default:
		Mode = LOCTEXT("SortDefault", "Default");
		break;
	}
	return SortMode == ERpgBaseResourceLocalSortMode::ReplicatedOrder
		? Mode
		: FText::Format(
			LOCTEXT("SortDirectionFormat", "{0} {1}"),
			Mode,
			bDescending ? LOCTEXT("SortDescending", "descending") : LOCTEXT("SortAscending", "ascending"));
}

template <typename CostType>
FText FormatCosts(const TArray<CostType>& Costs)
{
	TArray<FString> Parts;
	Parts.Reserve(Costs.Num());
	for (const CostType& Cost : Costs)
	{
		if (Cost.ItemDefinition && Cost.Count > 0)
		{
			Parts.Add(FText::Format(
				LOCTEXT("CostEntryFormat", "{0} x {1}"),
				FText::AsNumber(Cost.Count),
				GetDefinitionDisplayName(Cost.ItemDefinition)).ToString());
		}
	}
	return Parts.IsEmpty()
		? LOCTEXT("NoOperationCosts", "No material cost")
		: FText::FromString(FString::Join(Parts, TEXT(", ")));
}
}

#if WITH_EDITOR

void URpgBaseTerminalWidget::ValidateCompiledDefaults(
	IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);

	ValidateCommonInputActionRow(
		CompileLog,
		DepositAllInputAction,
		LOCTEXT("DepositAllInputActionLabel", "DepositAllInputAction"),
		/*bRequired=*/ true);
	ValidateCommonInputActionRow(
		CompileLog,
		InstallUpgradeInputAction,
		LOCTEXT("InstallUpgradeInputActionLabel", "InstallUpgradeInputAction"),
		/*bRequired=*/ true);
}

#endif

URpgInventorySpatialPaneWidget* URpgBaseTerminalWidget::GetPlayerInventoryPane() const
{
	return Cast<URpgInventorySpatialPaneWidget>(PlayerInventoryPane.Get());
}

URpgPlayerInventoryPaneWidget* URpgBaseTerminalWidget::GetReusablePlayerInventoryPane() const
{
	return Cast<URpgPlayerInventoryPaneWidget>(PlayerInventoryPane.Get());
}

void URpgBaseTerminalWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (DepositAllButton)
	{
		DepositAllButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleDepositAllClicked);
	}
	if (InstallUpgradeButton)
	{
		InstallUpgradeButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleInstallUpgradeClicked);
	}
	if (MaterialsDomainButton)
	{
		MaterialsDomainButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleMaterialsDomainClicked);
	}
	if (ArmoryDomainButton)
	{
		ArmoryDomainButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleArmoryDomainClicked);
	}
	if (PersonalDomainButton)
	{
		PersonalDomainButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandlePersonalDomainClicked);
	}
	if (RiftDomainButton)
	{
		RiftDomainButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleRiftDomainClicked);
	}
	if (StorageSearchBox)
	{
		StorageSearchBox->OnTextChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleStorageSearchTextChanged);
	}
	if (StorageCategoryFilterButton)
	{
		StorageCategoryFilterButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleStorageCategoryFilterClicked);
	}
	if (StorageSortButton)
	{
		StorageSortButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleStorageSortClicked);
	}
	if (StorageWithdrawOneButton)
	{
		StorageWithdrawOneButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleWithdrawOneClicked);
	}
	if (StorageWithdrawTenButton)
	{
		StorageWithdrawTenButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleWithdrawTenClicked);
	}
	if (StorageWithdrawMaxButton)
	{
		StorageWithdrawMaxButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleWithdrawMaxClicked);
	}
	if (StorageWithdrawCustomInput)
	{
		StorageWithdrawCustomInput->OnTextCommitted.AddUniqueDynamic(
			this,
			&ThisClass::HandleCustomWithdrawCommitted);
		if (StorageWithdrawCustomInput->GetText().IsEmpty())
		{
			StorageWithdrawCustomInput->SetText(FText::AsNumber(1));
		}
	}
	if (StabilizeRiftButton)
	{
		StabilizeRiftButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleStabilizeRiftClicked);
	}
	if (ExtractRiftButton)
	{
		ExtractRiftButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleExtractRiftClicked);
	}
	if (CleanseRiftButton)
	{
		CleanseRiftButton->OnClicked.AddUniqueDynamic(
			this,
			&ThisClass::HandleCleanseRiftClicked);
	}
	if (BaseResourceList && BaseResourceList->GetResourceList())
	{
		BaseResourceList->GetResourceList()->OnItemSelectionChanged().AddUObject(
			this,
			&ThisClass::HandleMaterialSelectionChanged);
	}

	if (URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		ReusablePane->OnNavigationPanelsChanged.AddUObject(
			this,
			&ThisClass::HandlePlayerInventoryPaneNavigationPanelsChanged);
		ReusablePane->ReleaseInventoryPresentation();
	}
	else if (URpgInventorySpatialPaneWidget* LegacyPane =
		GetPlayerInventoryPane())
	{
		LegacyPane->ReleaseInventoryPresentation();
	}

	ReleaseDomainPanes();
	if (BaseResourceList)
	{
		BaseResourceList->ReleaseBaseStoragePresentation();
	}

	UpdateDomainPresentation();
	UpdateTerminalActionAvailability();
	RefreshTerminalControlPresentation();
}

void URpgBaseTerminalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RegisterBaseStorageFeedbackListener();
	RegisterBaseTerminalActionBindings();
	UpdateTerminalActionAvailability();
	UpdateDomainActionAvailability();
}

void URpgBaseTerminalWidget::NativeOnDeactivated()
{
	UnregisterBaseStorageFeedbackListener();
	UnregisterBaseTerminalActionBindings();
	StopAccessValidation();
	Super::NativeOnDeactivated();
}

void URpgBaseTerminalWidget::NativeDestruct()
{
	UnregisterBaseStorageFeedbackListener();
	StopAccessValidation();
	if (BaseResourceList && BaseResourceList->GetResourceList())
	{
		BaseResourceList->GetResourceList()->OnItemSelectionChanged().RemoveAll(this);
	}
	if (URpgInventoryPanelNavigationCoordinator* Navigator =
		GetScreenPanelNavigationCoordinator())
	{
		Navigator->OnActiveSelectionChanged.RemoveDynamic(
			this,
			&ThisClass::HandleActiveInventorySelectionChanged);
	}
	if (URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		ReusablePane->OnNavigationPanelsChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

UWidget* URpgBaseTerminalWidget::NativeGetDesiredFocusTarget() const
{
	if (ActiveDomain == ERpgBaseTerminalDomain::Materials &&
		BaseResourceList && BaseResourceList->GetResourceList())
	{
		return BaseResourceList->GetResourceList();
	}

	if (URpgInventorySpatialPaneWidget* DomainPane =
		GetDomainPane(ActiveDomain);
		DomainPane && DomainPane->GetSpatialGrid())
	{
		return DomainPane->GetSpatialGrid();
	}

	if (const URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		if (UWidget* Preferred = ReusablePane->GetPreferredFocusTarget())
		{
			return Preferred;
		}
	}
	if (const URpgInventorySpatialPaneWidget* LegacyPane =
		GetPlayerInventoryPane())
	{
		if (LegacyPane->GetSpatialGrid())
		{
			return LegacyPane->GetSpatialGrid();
		}
	}

	return Super::NativeGetDesiredFocusTarget();
}

bool URpgBaseTerminalWidget::NativeHandlePreviousPanelAction()
{
	return CycleActiveDomain(-1) ||
		Super::NativeHandlePreviousPanelAction();
}

bool URpgBaseTerminalWidget::NativeHandleNextPanelAction()
{
	return CycleActiveDomain(1) ||
		Super::NativeHandleNextPanelAction();
}

void URpgBaseTerminalWidget::ReceiveScreenPayload_Implementation(UObject* Payload)
{
	ApplyBaseStorageScreenPayload(Payload);
}

void URpgBaseTerminalWidget::BindInventoryScreenPresentation()
{
	if (!BindBaseTerminalContext())
	{
		ResetBaseTerminalContext();
	}
}

void URpgBaseTerminalWidget::UnbindInventoryScreenPresentation()
{
	ResetBaseTerminalContext();
}

void URpgBaseTerminalWidget::ForwardInventoryInteractionContextToChildren()
{
	if (!bBaseTerminalContextBound)
	{
		return;
	}

	URpgInventoryDragDropCoordinator* Coordinator =
		GetScreenDragDropCoordinator();
	URpgInventoryPanelNavigationCoordinator* Navigator =
		GetScreenPanelNavigationCoordinator();
	if (Navigator)
	{
		Navigator->OnActiveSelectionChanged.RemoveDynamic(
			this,
			&ThisClass::HandleActiveInventorySelectionChanged);
		Navigator->OnActiveSelectionChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleActiveInventorySelectionChanged);
	}
	if (URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		FRpgInventoryScreenPresentationContext Context;
		Context.DragDropCoordinator = Coordinator;
		Context.PanelNavigationCoordinator = Navigator;
		Context.PresentationHost = this;
		ReusablePane->SetInteractionContext(
			Context,
			TEXT("BaseTerminal.Player"));
	}
	else if (URpgInventorySpatialPaneWidget* LegacyPane =
		GetPlayerInventoryPane())
	{
		LegacyPane->SetInteractionContext(
			Coordinator,
			Navigator,
			TEXT("BaseTerminal.Player.Pockets"),
			this);
	}

	if (URpgInventorySpatialPaneWidget* DomainPane =
		GetDomainPane(ActiveDomain))
	{
		DomainPane->SetInteractionContext(
			Coordinator,
			Navigator,
			GetDomainPanelId(ActiveDomain),
			this);
	}
}

void URpgBaseTerminalWidget::RegisterInventoryScreenNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (!Navigator || !bBaseTerminalContextBound)
	{
		return;
	}

	if (URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		ReusablePane->RegisterNavigationPanels(Navigator);
	}
	else if (URpgInventorySpatialPaneWidget* LegacyPane =
		GetPlayerInventoryPane())
	{
		LegacyPane->RegisterNavigationPanel(Navigator);
	}

	if (URpgInventorySpatialPaneWidget* DomainPane =
		GetDomainPane(ActiveDomain))
	{
		DomainPane->RegisterNavigationPanel(Navigator);
	}
}

FName URpgBaseTerminalWidget::GetInitialInventoryNavigationPanelId() const
{
	if (ActiveDomain != ERpgBaseTerminalDomain::Materials)
	{
		if (const URpgInventorySpatialPaneWidget* DomainPane =
			GetDomainPane(ActiveDomain);
			DomainPane && DomainPane->GetBoundInventory())
		{
			return GetDomainPanelId(ActiveDomain);
		}
	}

	if (const URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		return ReusablePane->GetPreferredNavigationPanelId();
	}
	return GetPlayerInventoryPane()
		? FName(TEXT("BaseTerminal.Player.Pockets"))
		: NAME_None;
}

void URpgBaseTerminalWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	if (const URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		ReusablePane->AppendSpatialGrids(OutGrids);
	}
	else if (const URpgInventorySpatialPaneWidget* LegacyPane =
		GetPlayerInventoryPane();
		LegacyPane && LegacyPane->GetSpatialGrid())
	{
		OutGrids.AddUnique(LegacyPane->GetSpatialGrid());
	}

	if (const URpgInventorySpatialPaneWidget* DomainPane =
		GetDomainPane(ActiveDomain);
		DomainPane && DomainPane->GetSpatialGrid())
	{
		OutGrids.AddUnique(DomainPane->GetSpatialGrid());
	}
}

bool URpgBaseTerminalWidget::RouteInventoryPayloadToScreenSpecificTarget(
	const FRpgInventoryDragPayload& Payload,
	FVector2D GhostCenterScreenPosition,
	bool bCommit,
	bool& bOutTargetAddressed)
{
	bOutTargetAddressed = false;
	URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane();
	UWidget* Target = nullptr;
	if (!ReusablePane ||
		!ReusablePane->ResolveNonSpatialDropTarget(
			GhostCenterScreenPosition,
			Target) ||
		!Target)
	{
		return false;
	}

	bOutTargetAddressed = true;
	SwitchActivePointerDropTarget(Target);
	return ReusablePane->ApplyPayloadToNonSpatialDropTarget(
		Target,
		Payload,
		GhostCenterScreenPosition,
		bCommit);
}

void URpgBaseTerminalWidget::ClearInventoryScreenSpecificDragPreviews()
{
	if (URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		ReusablePane->ClearExternalDragPreviews();
	}
}

bool URpgBaseTerminalWidget::UpdateInventoryScreenSpecificControllerDragVisual(
	const FRpgInventoryDragPayload& Payload)
{
	FVector2D AnchorScreenPosition = FVector2D::ZeroVector;
	URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane();
	if (!ReusablePane ||
		!ReusablePane->ResolveControllerDragVisualAnchor(
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

void URpgBaseTerminalWidget::RefreshInventoryScreenSpecificInteractionPresentation(
	ERpgInventoryInteractionPreviewState PreviewState,
	bool bHasPayload,
	bool bPendingRequest)
{
	if (URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		ReusablePane->RefreshInteractionPresentation(
			PreviewState,
			bHasPayload,
			bPendingRequest);
	}
}

URpgInventoryManagerComponent* URpgBaseTerminalWidget::GetDomainInventory(
	ERpgBaseTerminalDomain Domain) const
{
	switch (Domain)
	{
	case ERpgBaseTerminalDomain::Armory:
		return ArmoryInventory.Get();
	case ERpgBaseTerminalDomain::Personal:
		return PersonalInventory.Get();
	case ERpgBaseTerminalDomain::Rift:
		return RiftInventory.Get();
	case ERpgBaseTerminalDomain::Materials:
	default:
		return nullptr;
	}
}

TArray<ERpgBaseTerminalDomain> URpgBaseTerminalWidget::GetAvailableDomains() const
{
	TArray<ERpgBaseTerminalDomain> Result;
	for (ERpgBaseTerminalDomain Domain :
		{
			ERpgBaseTerminalDomain::Materials,
			ERpgBaseTerminalDomain::Armory,
			ERpgBaseTerminalDomain::Personal,
			ERpgBaseTerminalDomain::Rift
		})
	{
		if (IsDomainAvailable(Domain))
		{
			Result.Add(Domain);
		}
	}
	return Result;
}

bool URpgBaseTerminalWidget::IsDomainAvailable(
	ERpgBaseTerminalDomain Domain) const
{
	switch (Domain)
	{
	case ERpgBaseTerminalDomain::Materials:
		return BaseStorage != nullptr;
	case ERpgBaseTerminalDomain::Personal:
		return BaseStorage && PersonalInventory &&
			BaseStorage->HasInstalledCapability(
				RpgGameplayTags::Storage_Capability_PersonalLocker);
	case ERpgBaseTerminalDomain::Rift:
		return BaseStorage && RiftInventory &&
			(IsRiftKnowledgeDiscovered() || IsRiftContainmentInstalled());
	case ERpgBaseTerminalDomain::Armory:
	default:
		return ArmoryInventory != nullptr;
	}
}

bool URpgBaseTerminalWidget::CanPresentDomain(
	ERpgBaseTerminalDomain Domain) const
{
	if (!bBaseTerminalContextBound || !IsDomainAvailable(Domain))
	{
		return false;
	}

	return Domain == ERpgBaseTerminalDomain::Materials
		? BaseResourceList != nullptr
		: GetDomainPane(Domain) != nullptr;
}

bool URpgBaseTerminalWidget::SetActiveDomain(
	ERpgBaseTerminalDomain NewDomain)
{
	if (!CanPresentDomain(NewDomain))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::DomainUnavailable,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT(
				"DomainUnavailable",
				"This storage domain is not available at the current terminal."));
		return false;
	}

	if (ActiveDomain == NewDomain)
	{
		return true;
	}

	if (URpgInventoryDragDropCoordinator* Coordinator =
		GetScreenDragDropCoordinator())
	{
		Coordinator->ForceCancelInteraction();
		Coordinator->ClearQuickTransferTargets();
		Coordinator->SetFocusedInventory(nullptr);
	}

	ActiveDomain = NewDomain;
	LastUsedDomain = NewDomain;
	PendingExtractionConfirmationItemId = FRpgInventoryItemId();
	SubmittedExtractionPreviewItemId = FRpgInventoryItemId();
	ClearLocalFeedback();
	UpdateDomainPresentation();
	ForwardInventoryInteractionContextToChildren();
	ConfigureQuickTransferRoutes();
	RefreshInventoryScreenNavigationPanels();
	RefreshInventoryControllerFocus();
	OnActiveDomainChanged.Broadcast(ActiveDomain);
	RefreshTerminalControlPresentation();
	return true;
}

bool URpgBaseTerminalWidget::SetOptionalDomainInventory(
	ERpgBaseTerminalDomain Domain,
	URpgInventoryManagerComponent* Inventory)
{
	if ((Domain != ERpgBaseTerminalDomain::Personal &&
		Domain != ERpgBaseTerminalDomain::Rift) ||
		!BaseStorageScreenPayload ||
		(Inventory && Inventory == PlayerInventory))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Error,
			LOCTEXT(
				"InvalidOptionalDomainContext",
				"The optional storage domain could not be attached to this terminal context."));
		return false;
	}

	TObjectPtr<URpgInventoryManagerComponent>& Target =
		Domain == ERpgBaseTerminalDomain::Personal
			? PersonalInventory
			: RiftInventory;
	if (Target == Inventory)
	{
		return true;
	}

	Target = Inventory;
	if (bBaseTerminalContextBound)
	{
		ReleaseDomainPanes();
		BindAvailableDomainPanes();
		if (!CanPresentDomain(ActiveDomain))
		{
			SetActiveDomain(ERpgBaseTerminalDomain::Materials);
		}
		else
		{
			UpdateDomainPresentation();
			ForwardInventoryInteractionContextToChildren();
			ConfigureQuickTransferRoutes();
			RefreshInventoryScreenNavigationPanels();
			RefreshInventoryControllerFocus();
		}
	}
	else
	{
		UpdateDomainActionAvailability();
	}
	return true;
}

void URpgBaseTerminalWidget::ClearLocalFeedback()
{
	if (LocalFeedback.Code == ERpgBaseTerminalFeedbackCode::None &&
		LocalFeedback.Message.IsEmpty())
	{
		return;
	}

	LocalFeedback = FRpgBaseTerminalLocalFeedback();
	LocalFeedback.Domain = ActiveDomain;
	OnLocalFeedbackChanged.Broadcast(LocalFeedback);
	RefreshAuthoritativeResultPresentation();
}

int32 URpgBaseTerminalWidget::ResolveWithdrawQuantity(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	ERpgBaseTerminalQuantityPreset Preset,
	int32 CustomQuantity) const
{
	if (!bBaseTerminalContextBound ||
		!BaseStorage ||
		!StationComponent ||
		!ItemDefinition ||
		!StationComponent->AllowsResourceDefinition(ItemDefinition))
	{
		return 0;
	}

	return ResolvePresetQuantity(
		Preset,
		CustomQuantity,
		BaseStorage->GetResourceCount(ItemDefinition));
}

bool URpgBaseTerminalWidget::RequestWithdrawResource(
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	ERpgBaseTerminalQuantityPreset Preset,
	int32 CustomQuantity)
{
	if (Preset == ERpgBaseTerminalQuantityPreset::Custom &&
		CustomQuantity <= 0)
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidQuantity,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("InvalidWithdrawQuantity", "Enter a withdraw quantity greater than zero."));
		return false;
	}

	const int32 Quantity = ResolveWithdrawQuantity(
		ItemDefinition,
		Preset,
		CustomQuantity);
	if (Quantity <= 0)
	{
		const bool bHasContext = bBaseTerminalContextBound &&
			StationComponent && BaseStorage && ItemDefinition;
		SetLocalFeedback(
			bHasContext
				? ERpgBaseTerminalFeedbackCode::NoAvailableQuantity
				: ERpgBaseTerminalFeedbackCode::InvalidContext,
			bHasContext
				? ERpgBaseTerminalFeedbackSeverity::Warning
				: ERpgBaseTerminalFeedbackSeverity::Error,
			bHasContext
				? LOCTEXT("NothingToWithdraw", "No withdrawable units are currently available.")
				: LOCTEXT("WithdrawInvalidContext", "The terminal context is no longer valid."));
		return false;
	}

	URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent();
	if (!UiActions)
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Error,
			LOCTEXT("WithdrawActionsMissing", "The withdraw request could not be dispatched."));
		return false;
	}

	FRpgBaseTerminalQuantityRequest Request;
	Request.Direction = ERpgBaseTerminalQuantityDirection::Withdraw;
	Request.Preset = Preset;
	Request.ItemDefinition = ItemDefinition;
	Request.Quantity = Quantity;
	FRpgBaseStorageWithdrawRequest StorageRequest;
	if (!BuildStorageRequestContext(StorageRequest.Context))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Error,
			LOCTEXT("WithdrawContextMissing", "The storage network identity is no longer valid."));
		return false;
	}
	StorageRequest.ItemDefinition = ItemDefinition;
	StorageRequest.RequestedCount = Quantity;
	UiActions->RequestWithdrawFromBase(StorageRequest);
	OnQuantityRequestPrepared.Broadcast(Request);
	SetLocalFeedback(
		ERpgBaseTerminalFeedbackCode::RequestSubmitted,
		ERpgBaseTerminalFeedbackSeverity::Info,
		LOCTEXT("WithdrawSubmitted", "Withdraw request submitted for server validation."),
		Quantity,
		true);
	return true;
}

int32 URpgBaseTerminalWidget::ResolveDepositQuantity(
	URpgInventoryItemInstance* Item,
	ERpgBaseTerminalQuantityPreset Preset,
	int32 CustomQuantity) const
{
	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		Item ? Item->GetItemDef() : nullptr;
	if (!bBaseTerminalContextBound ||
		!PlayerInventory ||
		!BaseStorage ||
		!StationComponent ||
		!ItemDefinition ||
		!StationComponent->AllowsResourceDefinition(ItemDefinition))
	{
		return 0;
	}

	const int32 AvailableQuantity = FMath::Min(
		PlayerInventory->GetItemStackCount(Item),
		BaseStorage->GetFreeResourceCapacity(ItemDefinition));
	return ResolvePresetQuantity(
		Preset,
		CustomQuantity,
		AvailableQuantity);
}

bool URpgBaseTerminalWidget::RequestDepositMaterialStack(
	URpgInventoryItemInstance* Item,
	ERpgBaseTerminalQuantityPreset Preset,
	int32 CustomQuantity)
{
	if (Preset == ERpgBaseTerminalQuantityPreset::Custom &&
		CustomQuantity <= 0)
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidQuantity,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("InvalidDepositQuantity", "Enter a deposit quantity greater than zero."));
		return false;
	}

	const int32 Quantity = ResolveDepositQuantity(
		Item,
		Preset,
		CustomQuantity);
	if (Quantity <= 0)
	{
		const bool bHasContext = bBaseTerminalContextBound &&
			StationComponent && BaseStorage && PlayerInventory && Item;
		SetLocalFeedback(
			bHasContext
				? ERpgBaseTerminalFeedbackCode::NoAvailableQuantity
				: ERpgBaseTerminalFeedbackCode::InvalidContext,
			bHasContext
				? ERpgBaseTerminalFeedbackSeverity::Warning
				: ERpgBaseTerminalFeedbackSeverity::Error,
			bHasContext
				? LOCTEXT("NothingToDeposit", "No depositable units or free resource capacity are available.")
				: LOCTEXT("DepositInvalidContext", "The terminal context is no longer valid."));
		return false;
	}

	URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent();
	if (!UiActions)
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Error,
			LOCTEXT("DepositActionsMissing", "The deposit request could not be dispatched."));
		return false;
	}

	FRpgBaseTerminalQuantityRequest Request;
	Request.Direction = ERpgBaseTerminalQuantityDirection::Deposit;
	Request.Preset = Preset;
	Request.ItemDefinition = Item->GetItemDef();
	Request.ItemInstance = Item;
	Request.Quantity = Quantity;
	const TArray<FRpgInventoryEntryView> PlayerEntries =
		PlayerInventory->GetAllEntries();
	const FRpgInventoryEntryView* Entry =
		PlayerEntries.FindByPredicate(
			[Item](const FRpgInventoryEntryView& Candidate)
			{
				return Candidate.Instance == Item &&
					Candidate.ItemId == Item->GetItemId();
			});
	FRpgBaseStorageDepositRequest StorageRequest;
	if (!Entry ||
		!BuildStorageRequestContext(StorageRequest.Context))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::Stale,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("DepositSnapshotStale", "The selected stack changed; select it again."));
		return false;
	}
	StorageRequest.ItemId = Entry->ItemId;
	StorageRequest.ExpectedEntryId = Entry->EntryId;
	StorageRequest.ExpectedSourcePlacement = Entry->Placement;
	StorageRequest.ExpectedInventoryRevision =
		PlayerInventory->GetInventoryRevision();
	StorageRequest.ExpectedSourceQuantity = Entry->StackCount;
	StorageRequest.RequestedCount = Quantity;
	UiActions->RequestDepositItemToBase(StorageRequest);
	OnQuantityRequestPrepared.Broadcast(Request);
	SetLocalFeedback(
		ERpgBaseTerminalFeedbackCode::RequestSubmitted,
		ERpgBaseTerminalFeedbackSeverity::Info,
		LOCTEXT("DepositSubmitted", "Deposit request submitted for server validation."),
		Quantity,
		true);
	return true;
}

void URpgBaseTerminalWidget::RequestDepositAllMaterials()
{
	FRpgBaseStorageSmartDepositRequest Request;
	if (!bBaseTerminalContextBound || !StationComponent ||
		!BuildStorageRequestContext(Request.Context))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Error,
			LOCTEXT("DepositAllInvalidContext", "The terminal context is no longer valid."));
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		UiActions->RequestSmartDepositToBase(Request);
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::RequestSubmitted,
			ERpgBaseTerminalFeedbackSeverity::Info,
			LOCTEXT("DepositAllSubmitted", "Deposit-all request submitted for server validation."),
			0,
			true);
	}
	else
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Error,
			LOCTEXT("DepositAllActionsMissing", "The deposit-all request could not be dispatched."));
	}
}

void URpgBaseTerminalWidget::RequestInstallFeaturedUpgrade()
{
	if (!bBaseTerminalContextBound ||
		!StationComponent ||
		FeaturedUpgrade.IsNull())
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("UpgradeUnavailable", "No installable terminal upgrade is currently available."));
		return;
	}

	URpgBaseStorageUpgradeDefinition* Upgrade = FeaturedUpgrade.LoadSynchronous();
	if (!Upgrade || !StationComponent->CanInstallUpgrade(Upgrade))
	{
		UpdateTerminalActionAvailability();
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("UpgradeRequirementsNotMet", "The upgrade requirements are not currently met."));
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		FRpgBaseStorageUpgradeRequest Request;
		if (!BuildStorageRequestContext(Request.Context))
		{
			SetLocalFeedback(
				ERpgBaseTerminalFeedbackCode::InvalidContext,
				ERpgBaseTerminalFeedbackSeverity::Error,
				LOCTEXT("UpgradeContextMissing", "The storage network identity is no longer valid."));
			return;
		}
		Request.UpgradeId = Upgrade->GetPrimaryAssetId();
		Request.ExpectedAnchorId = Upgrade->TargetAnchorId;
		UiActions->RequestInstallBaseStorageUpgradeById(Request);
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::RequestSubmitted,
			ERpgBaseTerminalFeedbackSeverity::Info,
			LOCTEXT("UpgradeSubmitted", "Upgrade request submitted for server validation."),
			0,
			true);
	}
}

bool URpgBaseTerminalWidget::RequestDecommissionUpgrade(
	URpgBaseStorageUpgradeDefinition* Upgrade)
{
	URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent();
	FRpgBaseStorageUpgradeRequest Request;
	if (!UiActions || !Upgrade || !BaseStorage ||
		!BaseStorage->HasInstalledUpgrade(Upgrade) ||
		!BuildStorageRequestContext(Request.Context))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("DecommissionUnavailable", "This upgrade cannot be removed from the current base."));
		return false;
	}

	Request.UpgradeId = Upgrade->GetPrimaryAssetId();
	Request.ExpectedAnchorId = Upgrade->TargetAnchorId;
	UiActions->RequestDecommissionBaseStorageUpgrade(Request);
	SetLocalFeedback(
		ERpgBaseTerminalFeedbackCode::RequestSubmitted,
		ERpgBaseTerminalFeedbackSeverity::Info,
		LOCTEXT("DecommissionSubmitted", "Upgrade removal submitted for server validation."),
		0,
		true);
	return true;
}

bool URpgBaseTerminalWidget::RequestStabilizeRiftItem(
	URpgInventoryItemInstance* Item)
{
	URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent();
	FRpgBaseStorageRiftItemRequest Request;
	if (!UiActions || !RiftInventory || !Item ||
		!RiftInventory->ContainsItemInstance(Item) ||
		!BuildStorageRequestContext(Request.Context))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("StabilizeUnavailable", "The selected Rift object is no longer available."));
		return false;
	}

	Request.ItemId = Item->GetItemId();
	Request.ExpectedContainmentRevision =
		RiftInventory->GetInventoryRevision();
	Request.bExpectedStabilized = false;
	UiActions->RequestStabilizeContainedItem(Request);
	SetLocalFeedback(
		ERpgBaseTerminalFeedbackCode::RequestSubmitted,
		ERpgBaseTerminalFeedbackSeverity::Info,
		LOCTEXT("StabilizeSubmitted", "Stabilization submitted for server validation."),
		1,
		true);
	return true;
}

bool URpgBaseTerminalWidget::RequestExtractRiftItem(
	URpgInventoryItemInstance* Item,
	bool bConfirmed)
{
	URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent();
	FRpgBaseStorageRiftItemRequest Request;
	if (!UiActions || !RiftInventory || !Item ||
		!RiftInventory->ContainsItemInstance(Item) ||
		!BuildStorageRequestContext(Request.Context))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("ExtractUnavailable", "The selected Rift object is no longer available."));
		return false;
	}

	Request.ItemId = Item->GetItemId();
	Request.ExpectedContainmentRevision =
		RiftInventory->GetInventoryRevision();
	Request.bExpectedStabilized = true;
	Request.bConfirmed = bConfirmed;
	if (bConfirmed)
	{
		PendingExtractionConfirmationItemId = FRpgInventoryItemId();
		SubmittedExtractionPreviewItemId = FRpgInventoryItemId();
		SubmittedExtractionPreviewRequestId.Invalidate();
	}
	else
	{
		PendingExtractionConfirmationItemId = FRpgInventoryItemId();
		SubmittedExtractionPreviewItemId = Item->GetItemId();
		SubmittedExtractionPreviewRequestId = Request.Context.RequestId;
	}
	UiActions->RequestExtractContainedItem(Request);
	SetLocalFeedback(
		ERpgBaseTerminalFeedbackCode::RequestSubmitted,
		ERpgBaseTerminalFeedbackSeverity::Info,
		bConfirmed
			? LOCTEXT("ExtractSubmitted", "Extraction submitted for server validation.")
			: LOCTEXT("ExtractPreviewRequested", "Extraction preview requested; confirm the shown output and strain before committing."),
		1,
		true);
	return true;
}

bool URpgBaseTerminalWidget::RequestCleanseRiftStrain()
{
	URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent();
	FRpgBaseStorageCleanseRequest Request;
	if (!UiActions || !BuildStorageRequestContext(Request.Context))
	{
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Warning,
			LOCTEXT("CleanseUnavailable", "Rift strain cannot be cleansed from the current terminal."));
		return false;
	}

	UiActions->RequestCleanseBaseStorageRiftStrain(Request);
	SetLocalFeedback(
		ERpgBaseTerminalFeedbackCode::RequestSubmitted,
		ERpgBaseTerminalFeedbackSeverity::Info,
		LOCTEXT("CleanseSubmitted", "Strain cleanse submitted for server validation."),
		BaseStorage ? BaseStorage->GetRiftCleanseAmount() : 0,
		true);
	return true;
}

void URpgBaseTerminalWidget::ApplyBaseStorageScreenPayload(UObject* Payload)
{
	URpgBaseStorageScreenPayload* NewPayload =
		Cast<URpgBaseStorageScreenPayload>(Payload);
	if (!IsPayloadCoherent(NewPayload))
	{
		ResetBaseTerminalContext();
		SetLocalFeedback(
			ERpgBaseTerminalFeedbackCode::InvalidContext,
			ERpgBaseTerminalFeedbackSeverity::Error,
			LOCTEXT("InvalidTerminalPayload", "The terminal connection is no longer valid."));
		return;
	}

	const bool bContextChanged =
		BaseStorageScreenPayload != NewPayload ||
		PlayerInventory != NewPayload->PlayerInventory ||
		ArmoryInventory != NewPayload->ArmoryInventory ||
		PersonalInventory != NewPayload->PersonalInventory ||
		RiftInventory != NewPayload->RiftInventory ||
		BaseStorage != NewPayload->BaseStorage ||
		StationComponent != NewPayload->StationComponent;
	if (bContextChanged)
	{
		ResetBaseTerminalContext();
	}

	BaseStorageScreenPayload = NewPayload;
	PlayerInventory = NewPayload->PlayerInventory;
	ArmoryInventory = NewPayload->ArmoryInventory;
	PersonalInventory = NewPayload->PersonalInventory;
	RiftInventory = NewPayload->RiftInventory;
	BaseStorage = NewPayload->BaseStorage;
	if (BaseStorage)
	{
		KnownNetworkRevision = KnownNetworkRevision == INDEX_NONE
			? BaseStorage->GetNetworkRevision()
			: FMath::Max(
				KnownNetworkRevision,
				BaseStorage->GetNetworkRevision());
	}
	StationComponent = NewPayload->StationComponent;
	if (bContextChanged)
	{
		ClearLocalFeedback();
	}

	if (!IsActivated() || bBaseTerminalContextBound)
	{
		return;
	}

	if (BindBaseTerminalContext())
	{
		ForwardInventoryInteractionContextToChildren();
		RefreshInventoryScreenNavigationPanels();
		RefreshInventoryControllerFocus();
	}
}

bool URpgBaseTerminalWidget::IsPayloadCoherent(
	const URpgBaseStorageScreenPayload* Payload) const
{
	if (!Payload ||
		!Payload->PlayerInventory ||
		!Payload->PrimaryInventory ||
		Payload->PlayerInventory != Payload->PrimaryInventory ||
		!Payload->StationComponent ||
		Payload->ContextComponent != Payload->StationComponent ||
		!Payload->BaseStorage ||
		Payload->BaseStorage != Payload->StationComponent->GetBaseStorage())
	{
		return false;
	}

	URpgInventoryManagerComponent* StationArmory =
		Payload->StationComponent->GetArmoryInventory();
	if (Payload->ArmoryInventory != Payload->SecondaryInventory ||
		Payload->ArmoryInventory != StationArmory ||
		Payload->ArmoryInventory == Payload->PlayerInventory)
	{
		return false;
	}

	const ARpgBaseCampActor* BaseCamp =
		Payload->StationComponent->GetBaseCamp();
	if (!BaseCamp ||
		Payload->RiftInventory != BaseCamp->GetContainmentInventoryComponent() ||
		Payload->RiftInventory == Payload->PlayerInventory ||
		Payload->RiftInventory == Payload->ArmoryInventory)
	{
		return false;
	}

	if (Payload->PersonalInventory)
	{
		const ARpgPersonalStorageLockerActor* Locker = Cast<
			ARpgPersonalStorageLockerActor>(Payload->PersonalInventory->GetOwner());
		if (!Locker || Locker->GetInventoryManager() != Payload->PersonalInventory ||
			Locker->GetOwner() != GetOwningPlayer() ||
			Locker->GetBaseId() != BaseCamp->GetBaseId() ||
			Payload->PersonalInventory == Payload->PlayerInventory ||
			Payload->PersonalInventory == Payload->ArmoryInventory ||
			Payload->PersonalInventory == Payload->RiftInventory)
		{
			return false;
		}
	}

	return true;
}

bool URpgBaseTerminalWidget::BindBaseTerminalContext()
{
	if (bBaseTerminalContextBound ||
		!IsActivated() ||
		!BaseStorageScreenPayload ||
		!PlayerInventory ||
		!BaseStorage ||
		!StationComponent)
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
		return false;
	}

	if (GetOwningPlayer())
	{
		URpgInventoryManagerComponent* CanonicalPlayerInventory =
			Coordinator->GetPlayerInventory();
		if (!CanonicalPlayerInventory ||
			CanonicalPlayerInventory != PlayerInventory)
		{
			UE_LOG(
				LogRpgBaseTerminalWidget,
				Warning,
				TEXT("%s rejected BaseTerminal payload: PlayerInventory [%s] does not match the owning player's canonical inventory [%s]."),
				*GetNameSafe(this),
				*GetNameSafe(PlayerInventory),
				*GetNameSafe(CanonicalPlayerInventory));
			ResetBaseTerminalContext();
			return false;
		}

		PlayerInventory = CanonicalPlayerInventory;
	}

	PlayerPaneContainerHandle = FRpgInventoryContainerHandle();
	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		GetOwningPlayer()
			? GetOwningPlayer()->FindComponentByClass<URpgPlayerInventoryLayoutComponent>()
			: nullptr;
	FRpgInventorySlotGroupView PrimaryContentGroup;
	const bool bHasPrimaryContent = InventoryLayout &&
		InventoryLayout->TryGetSlotGroupBySemanticRole(
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Content_Primary,
			PrimaryContentGroup) &&
		PrimaryContentGroup.GroupKind == ERpgInventorySlotGroupKind::Content;
	if (GetPlayerInventoryPane() && !bHasPrimaryContent)
	{
		UE_LOG(
			LogRpgBaseTerminalWidget,
			Warning,
			TEXT("%s rejected BaseTerminal payload: the owning player's layout has no unique primary content role for the legacy pane."),
			*GetNameSafe(this));
		ResetBaseTerminalContext();
		return false;
	}
	if (bHasPrimaryContent)
	{
		PlayerPaneContainerHandle = PrimaryContentGroup.ContainerHandle;
	}

	// Set the guard before pane binding because the reusable pane can synchronously request a navigation refresh.
	bBaseTerminalContextBound = true;
	EnsureBaseStorageViewModel();
	if (BaseStorageViewModel)
	{
		BaseStorageViewModel->BindBaseStorage(
			BaseStorage,
			StationComponent->GetAllowedResourceDefinitions());
	}
	if (BaseResourceList)
	{
		BaseResourceList->SetBaseStorageViewModel(BaseStorageViewModel);
	}

	if (URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		FRpgInventoryScreenPresentationContext Context;
		Context.DragDropCoordinator = Coordinator;
		Context.PanelNavigationCoordinator = Navigator;
		Context.PresentationHost = this;
		ReusablePane->BindPlayerInventory(
			GetOwningPlayer(),
			Context,
			TEXT("BaseTerminal.Player"));
	}
	else if (URpgInventorySpatialPaneWidget* LegacyPane =
		GetPlayerInventoryPane())
	{
		LegacyPane->BindInventoryContainer(
			PlayerInventory,
			PlayerPaneContainerHandle);
	}
	else
	{
		UE_LOG(
			LogRpgBaseTerminalWidget,
			Warning,
			TEXT("%s has no authored PlayerInventoryPane; material presentation remains available without inventory drag/drop."),
			*GetNameSafe(this));
	}

	BindAvailableDomainPanes();
	SelectInitialDomain();
	StationComponent->OnInstalledUpgradesChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleInstalledUpgradesChanged);
	UpdateDomainPresentation();
	ConfigureQuickTransferRoutes();
	UpdateTerminalActionAvailability();
	RefreshTerminalControlPresentation();
	StartAccessValidation();
	++BaseTerminalPresentationBindGeneration;
	return true;
}

bool URpgBaseTerminalWidget::HasAutoDepositableLoot() const
{
	if (!PlayerInventory || !BaseStorage || !StationComponent)
	{
		return false;
	}
	for (const FRpgInventoryEntryView& Entry : PlayerInventory->GetAllEntries())
	{
		const URpgInventoryItemInstance* Item = Entry.Instance;
		const TSubclassOf<URpgInventoryItemDefinition> Definition =
			Item ? Item->GetItemDef() : nullptr;
		if (Item && Entry.StackCount > 0 &&
			Item->CanCollapseIntoDefinitionCount() &&
			StationComponent->AllowsResourceDefinition(Definition) &&
			BaseStorage->CanAutoDepositBulk(Definition) &&
			BaseStorage->GetFreeResourceCapacity(Definition) > 0)
		{
			return true;
		}
	}
	return false;
}

bool URpgBaseTerminalWidget::IsRiftKnowledgeDiscovered() const
{
	const ARpgGameStateBase* GameState = GetWorld()
		? GetWorld()->GetGameState<ARpgGameStateBase>() : nullptr;
	const URpgWorldStorageKnowledgeComponent* Knowledge = GameState
		? GameState->GetWorldStorageKnowledgeComponent() : nullptr;
	return Knowledge && Knowledge->HasKnowledgeTag(
		RpgGameplayTags::Storage_Knowledge_RiftContainment);
}

bool URpgBaseTerminalWidget::IsRiftContainmentInstalled() const
{
	return BaseStorage &&
		BaseStorage->HasInstalledCapability(
			RpgGameplayTags::Storage_Capability_RiftContainment) &&
		BaseStorage->GetContainmentSlotCapacity() > 0;
}

void URpgBaseTerminalWidget::SelectInitialDomain()
{
	const ERpgBaseTerminalDomain Desired = HasAutoDepositableLoot()
		? ERpgBaseTerminalDomain::Materials
		: (CanPresentDomain(LastUsedDomain)
			? LastUsedDomain
			: ERpgBaseTerminalDomain::Materials);
	if (ActiveDomain != Desired)
	{
		ActiveDomain = Desired;
		OnActiveDomainChanged.Broadcast(ActiveDomain);
	}
}

void URpgBaseTerminalWidget::EnsureBaseStorageViewModel()
{
	if (!BaseStorageViewModel)
	{
		BaseStorageViewModel = NewObject<URpgBaseStorageViewModel>(this);
		BaseStorageViewModel->OnResourcesChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleBaseStorageResourcesChanged);
	}
}

void URpgBaseTerminalWidget::RegisterBaseTerminalActionBindings()
{
	UnregisterBaseTerminalActionBindings();

	if (IsActionRowValid(DepositAllInputAction))
	{
		DepositAllActionBinding = RegisterUIActionBinding(
			FBindUIActionArgs(
				DepositAllInputAction,
				true,
				FSimpleDelegate::CreateUObject(
					this,
					&ThisClass::RequestDepositAllMaterials)));
	}
	if (IsActionRowValid(InstallUpgradeInputAction))
	{
		InstallUpgradeActionBinding = RegisterUIActionBinding(
			FBindUIActionArgs(
				InstallUpgradeInputAction,
				true,
				FSimpleDelegate::CreateUObject(
					this,
					&ThisClass::RequestInstallFeaturedUpgrade)));
	}
}

void URpgBaseTerminalWidget::UnregisterBaseTerminalActionBindings()
{
	if (DepositAllActionBinding.IsValid())
	{
		DepositAllActionBinding.Unregister();
	}
	if (InstallUpgradeActionBinding.IsValid())
	{
		InstallUpgradeActionBinding.Unregister();
	}

	DepositAllActionBinding = FUIActionBindingHandle();
	InstallUpgradeActionBinding = FUIActionBindingHandle();
}

void URpgBaseTerminalWidget::RegisterBaseStorageFeedbackListener()
{
	UnregisterBaseStorageFeedbackListener();
	if (!GetWorld())
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(GetWorld());
	BaseStorageCommandFeedbackHandle = MessageSubsystem.RegisterListener<
		FRpgBaseStorageCommandFeedbackMessage>(
			RpgGameplayTags::Rpg_BaseStorage_Message_CommandFeedback,
			this,
			&ThisClass::HandleBaseStorageCommandFeedback);
}

void URpgBaseTerminalWidget::UnregisterBaseStorageFeedbackListener()
{
	if (BaseStorageCommandFeedbackHandle.IsValid())
	{
		BaseStorageCommandFeedbackHandle.Unregister();
	}
}

bool URpgBaseTerminalWidget::BuildStorageRequestContext(
	FRpgBaseStorageRequestContext& OutContext) const
{
	OutContext = FRpgBaseStorageRequestContext();
	const ARpgBaseCampActor* BaseCamp = StationComponent
		? StationComponent->GetBaseCamp()
		: nullptr;
	if (!bBaseTerminalContextBound || !BaseCamp || !BaseStorage ||
		BaseCamp->GetBaseId().IsNone())
	{
		return false;
	}

	OutContext.RequestId = FGuid::NewGuid();
	OutContext.BaseId = BaseCamp->GetBaseId();
	OutContext.ExpectedNetworkRevision = KnownNetworkRevision == INDEX_NONE
		? BaseStorage->GetNetworkRevision()
		: FMath::Max(
			KnownNetworkRevision,
			BaseStorage->GetNetworkRevision());
	return true;
}

void URpgBaseTerminalWidget::HandleBaseStorageCommandFeedback(
	FGameplayTag Channel,
	const FRpgBaseStorageCommandFeedbackMessage& Message)
{
	const ARpgBaseCampActor* BaseCamp = StationComponent
		? StationComponent->GetBaseCamp()
		: nullptr;
	if (Channel !=
			RpgGameplayTags::Rpg_BaseStorage_Message_CommandFeedback ||
		!BaseCamp || Message.Result.BaseId != BaseCamp->GetBaseId() ||
		(Message.Recipient && Message.Recipient != GetOwningPlayer()))
	{
		return;
	}

	KnownNetworkRevision = Message.Result.NetworkRevision;
	LastCommandResult = Message.Result;
	if (SubmittedExtractionPreviewRequestId.IsValid() &&
		Message.Result.RequestId == SubmittedExtractionPreviewRequestId)
	{
		PendingExtractionConfirmationItemId =
			Message.Result.Code == ERpgBaseStorageResultCode::ConfirmationRequired
				? SubmittedExtractionPreviewItemId
				: FRpgInventoryItemId();
		SubmittedExtractionPreviewItemId = FRpgInventoryItemId();
		SubmittedExtractionPreviewRequestId.Invalidate();
	}
	OnAuthoritativeCommandResult.Broadcast(LastCommandResult);
	ERpgBaseTerminalFeedbackCode FeedbackCode =
		ERpgBaseTerminalFeedbackCode::ServerRejected;
	ERpgBaseTerminalFeedbackSeverity Severity =
		ERpgBaseTerminalFeedbackSeverity::Error;
	FText FeedbackText;
	switch (Message.Result.Code)
	{
	case ERpgBaseStorageResultCode::Success:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::AuthoritativeSuccess;
		Severity = ERpgBaseTerminalFeedbackSeverity::Success;
		FeedbackText = LOCTEXT(
			"StorageCommandSuccess",
			"Storage action completed successfully.");
		break;
	case ERpgBaseStorageResultCode::Partial:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::AuthoritativePartial;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = FText::Format(
			LOCTEXT(
				"StorageCommandPartial",
				"Stored {0} of {1} eligible units; the remainder stayed in your inventory."),
			FText::AsNumber(Message.Result.AppliedCount),
			FText::AsNumber(Message.Result.RequestedCount));
		break;
	case ERpgBaseStorageResultCode::ConfirmationRequired:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::ConfirmationRequired;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandConfirmation",
			"Review the deterministic output and strain, then confirm extraction.");
		break;
	case ERpgBaseStorageResultCode::NoAccess:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::NoAccess;
		FeedbackText = LOCTEXT(
			"StorageCommandNoAccess",
			"You no longer have access, range, or owner permission for this action.");
		break;
	case ERpgBaseStorageResultCode::MissingItem:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::MissingItem;
		FeedbackText = LOCTEXT(
			"StorageCommandMissingItem",
			"The selected item is no longer in the expected inventory.");
		break;
	case ERpgBaseStorageResultCode::Stale:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::Stale;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandStale",
			"Storage changed before the action arrived. Refresh the selection and try again.");
		break;
	case ERpgBaseStorageResultCode::CapacityFull:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::CapacityFull;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandCapacityFull",
			"This storage domain has no capacity left; no extra items were consumed.");
		break;
	case ERpgBaseStorageResultCode::NoPlacement:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::NoPlacement;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandNoPlacement",
			"No valid inventory placement is available for the result.");
		break;
	case ERpgBaseStorageResultCode::UnsupportedMode:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::UnsupportedMode;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandUnsupported",
			"This item is stateful, special, or not approved for the selected storage route.");
		break;
	case ERpgBaseStorageResultCode::CapabilityLocked:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::CapabilityLocked;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandCapabilityLocked",
			"The required storage capability has not been installed at this base.");
		break;
	case ERpgBaseStorageResultCode::KnowledgeMissing:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::KnowledgeMissing;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandKnowledgeMissing",
			"The co-op world has not discovered the required knowledge yet.");
		break;
	case ERpgBaseStorageResultCode::MissingCosts:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::MissingCosts;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandMissingCosts",
			"The base and player inventory do not contain the required materials.");
		break;
	case ERpgBaseStorageResultCode::StrainBlocked:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::StrainBlocked;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandStrainBlocked",
			"Extraction would exceed 100 strain. Cleanse the vault first.");
		break;
	case ERpgBaseStorageResultCode::Conflict:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::Conflict;
		Severity = ERpgBaseTerminalFeedbackSeverity::Warning;
		FeedbackText = LOCTEXT(
			"StorageCommandConflict",
			"The action conflicted with current storage state and was not committed.");
		break;
	case ERpgBaseStorageResultCode::InvalidRequest:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::ServerRejected;
		FeedbackText = LOCTEXT(
			"StorageCommandInvalid",
			"The server rejected an invalid or mismatched storage request.");
		break;
	case ERpgBaseStorageResultCode::InternalRollback:
	default:
		FeedbackCode = ERpgBaseTerminalFeedbackCode::ServerRejected;
		FeedbackText = LOCTEXT(
			"StorageCommandRollback",
			"The storage action was aborted because its atomic rollback could not be confirmed.");
		break;
	}

	SetLocalFeedback(
		FeedbackCode,
		Severity,
		MoveTemp(FeedbackText),
		Message.Result.RequestedCount,
		false);
}

void URpgBaseTerminalWidget::ResetBaseTerminalContext()
{
	bBaseTerminalContextBound = false;
	StopAccessValidation();

	if (StationComponent)
	{
		StationComponent->OnInstalledUpgradesChanged.RemoveDynamic(
			this,
			&ThisClass::HandleInstalledUpgradesChanged);
	}

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

	if (URpgPlayerInventoryPaneWidget* ReusablePane =
		GetReusablePlayerInventoryPane())
	{
		ReusablePane->ReleaseInventoryPresentation();
	}
	else if (URpgInventorySpatialPaneWidget* LegacyPane =
		GetPlayerInventoryPane())
	{
		LegacyPane->ReleaseInventoryPresentation();
	}
	ReleaseDomainPanes();
	if (BaseResourceList)
	{
		BaseResourceList->ReleaseBaseStoragePresentation();
	}
	if (BaseStorageViewModel)
	{
		BaseStorageViewModel->UnbindBaseStorage();
	}

	BaseStorageScreenPayload = nullptr;
	PlayerInventory = nullptr;
	ArmoryInventory = nullptr;
	PersonalInventory = nullptr;
	RiftInventory = nullptr;
	BaseStorage = nullptr;
	KnownNetworkRevision = INDEX_NONE;
	LastCommandResult = FRpgBaseStorageCommandResult();
	SelectedMaterialDefinition = nullptr;
	PendingExtractionConfirmationItemId = FRpgInventoryItemId();
	SubmittedExtractionPreviewItemId = FRpgInventoryItemId();
	SubmittedExtractionPreviewRequestId.Invalidate();
	StationComponent = nullptr;
	PlayerPaneContainerHandle = FRpgInventoryContainerHandle();
	if (ActiveDomain != ERpgBaseTerminalDomain::Materials)
	{
		ActiveDomain = ERpgBaseTerminalDomain::Materials;
		OnActiveDomainChanged.Broadcast(ActiveDomain);
	}
	UpdateDomainPresentation();
	UpdateTerminalActionAvailability();
	RefreshTerminalControlPresentation();
}

void URpgBaseTerminalWidget::UpdateTerminalActionAvailability()
{
	const bool bCanUseTerminal =
		bBaseTerminalContextBound && StationComponent != nullptr;
	if (DepositAllButton)
	{
		DepositAllButton->SetIsEnabled(bCanUseTerminal);
		DepositAllButton->SetVisibility(
			ActiveDomain == ERpgBaseTerminalDomain::Materials
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (DepositAllActionBinding.IsValid())
	{
		DepositAllActionBinding.SetDisplayInActionBar(bCanUseTerminal);
	}

	bool bCanInstallUpgrade = false;
	if (bCanUseTerminal && !FeaturedUpgrade.IsNull())
	{
		if (const URpgBaseStorageUpgradeDefinition* Upgrade =
			FeaturedUpgrade.LoadSynchronous())
		{
			bCanInstallUpgrade =
				StationComponent->CanInstallUpgrade(Upgrade);
		}
	}
	if (InstallUpgradeButton)
	{
		InstallUpgradeButton->SetIsEnabled(bCanInstallUpgrade);
		InstallUpgradeButton->SetVisibility(
			ActiveDomain == ERpgBaseTerminalDomain::Rift &&
				IsRiftKnowledgeDiscovered() &&
				!IsRiftContainmentInstalled()
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (InstallUpgradeActionBinding.IsValid())
	{
		InstallUpgradeActionBinding.SetDisplayInActionBar(
			bCanInstallUpgrade);
	}
	UpdateDomainActionAvailability();
	RefreshTerminalControlPresentation();
}

void URpgBaseTerminalWidget::UpdateDomainPresentation()
{
	BP_OnRiftDomainLockStateChanged(
		IsRiftKnowledgeDiscovered(),
		IsRiftContainmentInstalled());
	if (BaseResourceList)
	{
		BaseResourceList->SetVisibility(
			ActiveDomain == ERpgBaseTerminalDomain::Materials
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}

	for (ERpgBaseTerminalDomain Domain :
		{
			ERpgBaseTerminalDomain::Armory,
			ERpgBaseTerminalDomain::Personal,
			ERpgBaseTerminalDomain::Rift
		})
	{
		if (URpgInventorySpatialPaneWidget* Pane = GetDomainPane(Domain))
		{
			Pane->SetVisibility(
				ActiveDomain == Domain &&
					(Domain != ERpgBaseTerminalDomain::Rift ||
					 IsRiftContainmentInstalled())
					? ESlateVisibility::Visible
					: ESlateVisibility::Collapsed);
		}
	}
	UpdateDomainActionAvailability();
	RefreshTerminalControlPresentation();
}

void URpgBaseTerminalWidget::UpdateDomainActionAvailability()
{
	if (MaterialsDomainButton)
	{
		MaterialsDomainButton->SetIsEnabled(
			CanPresentDomain(ERpgBaseTerminalDomain::Materials));
	}
	if (ArmoryDomainButton)
	{
		ArmoryDomainButton->SetIsEnabled(
			CanPresentDomain(ERpgBaseTerminalDomain::Armory));
	}
	if (PersonalDomainButton)
	{
		PersonalDomainButton->SetVisibility(
			IsDomainAvailable(ERpgBaseTerminalDomain::Personal)
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		PersonalDomainButton->SetIsEnabled(
			CanPresentDomain(ERpgBaseTerminalDomain::Personal));
	}
	if (RiftDomainButton)
	{
		RiftDomainButton->SetVisibility(
			IsDomainAvailable(ERpgBaseTerminalDomain::Rift)
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		RiftDomainButton->SetIsEnabled(
			CanPresentDomain(ERpgBaseTerminalDomain::Rift));
	}
}

void URpgBaseTerminalWidget::RefreshTerminalControlPresentation()
{
	RefreshMaterialControlPresentation();
	RefreshRiftControlPresentation();
	RefreshAuthoritativeResultPresentation();
}

void URpgBaseTerminalWidget::RefreshMaterialControlPresentation()
{
	const bool bMaterialsActive = bBaseTerminalContextBound &&
		ActiveDomain == ERpgBaseTerminalDomain::Materials;
	for (UWidget* Widget :
		{
			static_cast<UWidget*>(StorageSearchBox.Get()),
			static_cast<UWidget*>(StorageCategoryFilterButton.Get()),
			static_cast<UWidget*>(StorageSortButton.Get()),
			static_cast<UWidget*>(StorageCapacityText.Get()),
			static_cast<UWidget*>(StorageWithdrawOneButton.Get()),
			static_cast<UWidget*>(StorageWithdrawTenButton.Get()),
			static_cast<UWidget*>(StorageWithdrawMaxButton.Get()),
			static_cast<UWidget*>(StorageWithdrawCustomInput.Get())
		})
	{
		if (Widget)
		{
			Widget->SetVisibility(
				bMaterialsActive
					? ESlateVisibility::Visible
					: ESlateVisibility::Collapsed);
		}
	}

	if (!BaseStorageViewModel)
	{
		if (StorageCapacityText)
		{
			StorageCapacityText->SetText(LOCTEXT("NoMaterialCapacity", "Materials unavailable"));
		}
		return;
	}

	const TArray<URpgBaseResourceEntryViewModel*> Resources =
		BaseStorageViewModel->GetResources();
	URpgBaseResourceEntryViewModel* SelectedResource = nullptr;
	for (URpgBaseResourceEntryViewModel* Resource : Resources)
	{
		if (Resource &&
			Resource->GetItemDefinition() == SelectedMaterialDefinition)
		{
			SelectedResource = Resource;
			break;
		}
	}
	if (!SelectedResource && !Resources.IsEmpty())
	{
		SelectedResource = Resources[0];
		SelectedMaterialDefinition = SelectedResource
			? SelectedResource->GetItemDefinition()
			: nullptr;
		if (SelectedResource && BaseResourceList &&
			BaseResourceList->GetResourceList() &&
			BaseResourceList->GetResourceList()->GetSelectedItem() != SelectedResource)
		{
			BaseResourceList->GetResourceList()->SetSelectedItem(SelectedResource);
		}
	}
	else if (!SelectedResource)
	{
		SelectedMaterialDefinition = nullptr;
	}

	const FRpgBaseStorageLocalSummary Summary =
		BaseStorageViewModel->GetSummary();
	if (StorageCapacityText)
	{
		StorageCapacityText->SetText(FText::Format(
			Summary.bHasCapacityWarning
				? LOCTEXT("MaterialCapacityWarning", "{0}/{1} material points - capacity warning")
				: LOCTEXT("MaterialCapacity", "{0}/{1} material points"),
			FText::AsNumber(Summary.UsedCapacityPoints),
			FText::AsNumber(Summary.MaterialCapacityPoints)));
		StorageCapacityText->SetColorAndOpacity(FSlateColor(
			Summary.bHasCapacityWarning
				? FLinearColor(1.0f, 0.55f, 0.12f, 1.0f)
				: FLinearColor::White));
	}
	if (StorageCategoryFilterText)
	{
		StorageCategoryFilterText->SetText(
			RpgBaseTerminalWidget::GetCategoryLabel(
				BaseStorageViewModel->GetCategoryFilter()));
	}
	if (StorageSortText)
	{
		StorageSortText->SetText(RpgBaseTerminalWidget::GetSortLabel(
			BaseStorageViewModel->GetLocalSortMode(),
			BaseStorageViewModel->IsLocalSortDescending()));
	}

	const bool bCanWithdraw = bMaterialsActive && SelectedResource &&
		SelectedResource->GetCount() > 0;
	for (UButton* Button :
		{
			StorageWithdrawOneButton.Get(),
			StorageWithdrawTenButton.Get(),
			StorageWithdrawMaxButton.Get()
		})
	{
		if (Button)
		{
			Button->SetIsEnabled(bCanWithdraw);
		}
	}
	if (StorageWithdrawCustomInput)
	{
		StorageWithdrawCustomInput->SetIsEnabled(bCanWithdraw);
	}
}

void URpgBaseTerminalWidget::RefreshRiftControlPresentation()
{
	const bool bRiftActive = bBaseTerminalContextBound &&
		ActiveDomain == ERpgBaseTerminalDomain::Rift;
	const bool bKnowledge = IsRiftKnowledgeDiscovered();
	const bool bInstalled = IsRiftContainmentInstalled();
	const bool bShowInstalledControls = bRiftActive && bInstalled;

	if (RiftLockStateText)
	{
		RiftLockStateText->SetVisibility(
			bRiftActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bKnowledge)
		{
			RiftLockStateText->SetText(LOCTEXT("RiftUndiscovered", "Rift storage undiscovered"));
		}
		else if (!bInstalled)
		{
			RiftLockStateText->SetText(LOCTEXT("RiftInstallationRequired", "Rift Containment I requires physical installation"));
		}
		else
		{
			const int32 OccupiedSlots = RiftInventory
				? RiftInventory->GetAllItems().Num()
				: 0;
			RiftLockStateText->SetText(FText::Format(
				LOCTEXT("RiftSlotsFormat", "Sealed slots {0}/{1}"),
				FText::AsNumber(OccupiedSlots),
				FText::AsNumber(BaseStorage ? BaseStorage->GetContainmentSlotCapacity() : 0)));
		}
	}
	if (StorageStrainText)
	{
		StorageStrainText->SetVisibility(
			bShowInstalledControls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		StorageStrainText->SetText(FText::Format(
			LOCTEXT("RiftStrainFormat", "Strain {0}/100"),
			FText::AsNumber(BaseStorage ? BaseStorage->GetRiftStrain() : 0)));
	}
	if (StorageRiftPreviewRow)
	{
		StorageRiftPreviewRow->SetVisibility(
			bShowInstalledControls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (StorageRiftActionRow)
	{
		StorageRiftActionRow->SetVisibility(
			bShowInstalledControls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	URpgInventoryItemInstance* Item = ResolveSelectedRiftItem();
	const URpgInventoryFragment_ContainmentProfile* Profile = Item
		? Item->FindFragmentByClass<URpgInventoryFragment_ContainmentProfile>()
		: nullptr;
	const bool bStabilized = Item && Item->IsContainmentStabilized();
	if (RiftActionCostText)
	{
		RiftActionCostText->SetText(
			!Item
				? LOCTEXT("RiftSelectItemCost", "Select a contained Rift object")
				: (bStabilized
					? LOCTEXT("RiftAlreadyStabilized", "Stabilization: complete")
					: (Profile
						? FText::Format(
							LOCTEXT("RiftStabilizationCost", "Stabilization: {0}"),
							RpgBaseTerminalWidget::FormatCosts(Profile->StabilizationCosts))
						: LOCTEXT("RiftInvalidProfile", "Missing containment profile"))));
	}
	if (RiftActionPreviewText)
	{
		FText Preview = LOCTEXT("RiftSelectItemPreview", "Select an object to preview deterministic extraction");
		if (Item && Profile && Profile->HasExtractionOutput())
		{
			const int32 StrainBefore = BaseStorage ? BaseStorage->GetRiftStrain() : 0;
			const int32 StrainDelta = BaseStorage
				? BaseStorage->GetMitigatedRiftStrainDelta(Profile->ExtractionStrain)
				: Profile->ExtractionStrain;
			int32 StrainAfter = FMath::Clamp(StrainBefore + StrainDelta, 0, 100);
			int32 OutputCount = Profile->ExtractionOutputCount;
			TSubclassOf<URpgInventoryItemDefinition> OutputDefinition =
				Profile->ExtractionOutputDefinition;
			if (PendingExtractionConfirmationItemId == Item->GetItemId() &&
				LastCommandResult.Code == ERpgBaseStorageResultCode::ConfirmationRequired)
			{
				OutputDefinition = LastCommandResult.RiftOutputItemDefinition;
				OutputCount = LastCommandResult.RiftOutputCount;
				StrainAfter = LastCommandResult.RiftStrainAfter;
			}
			Preview = FText::Format(
				PendingExtractionConfirmationItemId == Item->GetItemId()
					? LOCTEXT("RiftExtractionConfirmPreview", "Confirm: {0} x {1}; strain {2} -> {3}")
					: LOCTEXT("RiftExtractionPreview", "Extraction: {0} x {1}; strain {2} -> {3}"),
				FText::AsNumber(OutputCount),
				RpgBaseTerminalWidget::GetDefinitionDisplayName(OutputDefinition),
				FText::AsNumber(StrainBefore),
				FText::AsNumber(StrainAfter));
		}
		RiftActionPreviewText->SetText(Preview);
	}

	if (StabilizeRiftButton)
	{
		StabilizeRiftButton->SetIsEnabled(
			bShowInstalledControls && Item && Profile && !bStabilized);
	}
	if (ExtractRiftButton)
	{
		ExtractRiftButton->SetIsEnabled(
			bShowInstalledControls && Item && Profile && bStabilized &&
			Profile->HasExtractionOutput());
	}
	if (CleanseRiftButton)
	{
		CleanseRiftButton->SetIsEnabled(
			bShowInstalledControls && BaseStorage &&
			BaseStorage->GetCleanseableRiftStrain() > 0);
	}
}

void URpgBaseTerminalWidget::RefreshAuthoritativeResultPresentation()
{
	if (StorageActionResultText)
	{
		StorageActionResultText->SetText(LocalFeedback.Message);
		StorageActionResultText->SetVisibility(
			LocalFeedback.Message.IsEmpty()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible);
	}
	if (!StorageSmartDepositResultText)
	{
		return;
	}

	TArray<FString> Lines;
	Lines.Reserve(LastCommandResult.ResourceOutcomes.Num());
	for (const FRpgBaseStorageResourceCommandOutcome& Outcome :
		LastCommandResult.ResourceOutcomes)
	{
		Lines.Add(FText::Format(
			LOCTEXT("ResourceOutcomeFormat", "{0}: {1}/{2} - {3}"),
			RpgBaseTerminalWidget::GetDefinitionDisplayName(Outcome.ItemDefinition),
			FText::AsNumber(Outcome.AppliedCount),
			FText::AsNumber(Outcome.RequestedCount),
			RpgBaseTerminalWidget::GetOutcomeReason(Outcome.Code)).ToString());
	}
	StorageSmartDepositResultText->SetText(
		FText::FromString(FString::Join(Lines, TEXT("\n"))));
	StorageSmartDepositResultText->SetVisibility(
		Lines.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
}

bool URpgBaseTerminalWidget::CycleActiveDomain(int32 Direction)
{
	if (!bBaseTerminalContextBound || Direction == 0)
	{
		return false;
	}

	const TArray<ERpgBaseTerminalDomain> DomainOrder =
	{
		ERpgBaseTerminalDomain::Materials,
		ERpgBaseTerminalDomain::Armory,
		ERpgBaseTerminalDomain::Personal,
		ERpgBaseTerminalDomain::Rift
	};
	const int32 CurrentIndex = FMath::Max(0, DomainOrder.IndexOfByKey(ActiveDomain));
	for (int32 Offset = 1; Offset <= DomainOrder.Num(); ++Offset)
	{
		const int32 CandidateIndex = (CurrentIndex +
			(Direction > 0 ? Offset : -Offset) + DomainOrder.Num() * 2) % DomainOrder.Num();
		if (CanPresentDomain(DomainOrder[CandidateIndex]))
		{
			return SetActiveDomain(DomainOrder[CandidateIndex]);
		}
	}
	return false;
}

URpgInventoryItemInstance* URpgBaseTerminalWidget::ResolveSelectedRiftItem() const
{
	const URpgInventorySpatialPaneWidget* Pane = RiftInventoryPane.Get();
	const URpgInventorySpatialGridWidget* Grid = Pane ? Pane->GetSpatialGrid() : nullptr;
	const FRpgInventoryItemId ItemId = Grid
		? Grid->GetSelectedItemId()
		: FRpgInventoryItemId();
	return RiftInventory && ItemId.IsValid()
		? RiftInventory->FindItemById(ItemId)
		: nullptr;
}

int32 URpgBaseTerminalWidget::ReadCustomWithdrawQuantity() const
{
	if (!StorageWithdrawCustomInput)
	{
		return 0;
	}
	const FString QuantityString =
		StorageWithdrawCustomInput->GetText().ToString().TrimStartAndEnd();
	return QuantityString.IsNumeric()
		? FMath::Max(0, FCString::Atoi(*QuantityString))
		: 0;
}

void URpgBaseTerminalWidget::BindAvailableDomainPanes()
{
	for (ERpgBaseTerminalDomain Domain :
		{
			ERpgBaseTerminalDomain::Armory,
			ERpgBaseTerminalDomain::Personal,
			ERpgBaseTerminalDomain::Rift
		})
	{
		URpgInventorySpatialPaneWidget* Pane = GetDomainPane(Domain);
		URpgInventoryManagerComponent* Inventory = GetDomainInventory(Domain);
		if (Pane && Inventory &&
			(Domain != ERpgBaseTerminalDomain::Rift ||
			 IsRiftContainmentInstalled()))
		{
			Pane->BindInventoryContainer(
				Inventory,
				FRpgInventoryContainerHandle::MakeRoot(
					Inventory->GetDefaultContainerId()));
		}
	}
}

void URpgBaseTerminalWidget::ReleaseDomainPanes()
{
	for (URpgInventorySpatialPaneWidget* Pane :
		{
			ArmoryInventoryPane.Get(),
			PersonalInventoryPane.Get(),
			RiftInventoryPane.Get()
		})
	{
		if (Pane)
		{
			Pane->ReleaseInventoryPresentation();
		}
	}
}

void URpgBaseTerminalWidget::ConfigureQuickTransferRoutes()
{
	URpgInventoryDragDropCoordinator* Coordinator =
		GetScreenDragDropCoordinator();
	if (!Coordinator)
	{
		return;
	}

	Coordinator->ClearQuickTransferTargets();
	URpgInventoryManagerComponent* DomainInventory =
		GetDomainInventory(ActiveDomain);
	if (ActiveDomain != ERpgBaseTerminalDomain::Materials &&
		(ActiveDomain != ERpgBaseTerminalDomain::Rift ||
		 IsRiftContainmentInstalled()) &&
		GetDomainPane(ActiveDomain) &&
		PlayerInventory &&
		DomainInventory &&
		DomainInventory != PlayerInventory)
	{
		Coordinator->SetQuickTransferTarget(
			PlayerInventory,
			DomainInventory);
		Coordinator->SetQuickTransferTarget(
			DomainInventory,
			PlayerInventory);
	}
}

URpgInventorySpatialPaneWidget* URpgBaseTerminalWidget::GetDomainPane(
	ERpgBaseTerminalDomain Domain) const
{
	switch (Domain)
	{
	case ERpgBaseTerminalDomain::Armory:
		return ArmoryInventoryPane.Get();
	case ERpgBaseTerminalDomain::Personal:
		return PersonalInventoryPane.Get();
	case ERpgBaseTerminalDomain::Rift:
		return RiftInventoryPane.Get();
	case ERpgBaseTerminalDomain::Materials:
	default:
		return nullptr;
	}
}

FName URpgBaseTerminalWidget::GetDomainPanelId(
	ERpgBaseTerminalDomain Domain) const
{
	switch (Domain)
	{
	case ERpgBaseTerminalDomain::Armory:
		return TEXT("BaseTerminal.Armory");
	case ERpgBaseTerminalDomain::Personal:
		return TEXT("BaseTerminal.Personal");
	case ERpgBaseTerminalDomain::Rift:
		return TEXT("BaseTerminal.Rift");
	case ERpgBaseTerminalDomain::Materials:
	default:
		return NAME_None;
	}
}

int32 URpgBaseTerminalWidget::ResolvePresetQuantity(
	ERpgBaseTerminalQuantityPreset Preset,
	int32 CustomQuantity,
	int32 AvailableQuantity) const
{
	const int32 ClampedAvailable = FMath::Max(0, AvailableQuantity);
	if (ClampedAvailable <= 0)
	{
		return 0;
	}

	int32 RequestedQuantity = 0;
	switch (Preset)
	{
	case ERpgBaseTerminalQuantityPreset::One:
		RequestedQuantity = 1;
		break;
	case ERpgBaseTerminalQuantityPreset::Ten:
		RequestedQuantity = 10;
		break;
	case ERpgBaseTerminalQuantityPreset::Max:
		RequestedQuantity = ClampedAvailable;
		break;
	case ERpgBaseTerminalQuantityPreset::Custom:
		RequestedQuantity = CustomQuantity;
		break;
	default:
		break;
	}

	return RequestedQuantity > 0
		? FMath::Min(RequestedQuantity, ClampedAvailable)
		: 0;
}

void URpgBaseTerminalWidget::SetLocalFeedback(
	ERpgBaseTerminalFeedbackCode Code,
	ERpgBaseTerminalFeedbackSeverity Severity,
	FText Message,
	int32 RequestedQuantity,
	bool bAwaitingAuthoritativeUpdate)
{
	LocalFeedback.Code = Code;
	LocalFeedback.Severity = Severity;
	LocalFeedback.Message = MoveTemp(Message);
	LocalFeedback.Domain = ActiveDomain;
	LocalFeedback.RequestedQuantity = FMath::Max(0, RequestedQuantity);
	LocalFeedback.bAwaitingAuthoritativeUpdate =
		bAwaitingAuthoritativeUpdate;
	OnLocalFeedbackChanged.Broadcast(LocalFeedback);
	RefreshAuthoritativeResultPresentation();
}

void URpgBaseTerminalWidget::StartAccessValidation()
{
	StopAccessValidation();
	UWorld* World = GetWorld();
	if (!World || !bBaseTerminalContextBound)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AccessValidationTimer,
		this,
		&ThisClass::ValidateTerminalAccess,
		FMath::Max(0.05f, AccessValidationInterval),
		true);
}

void URpgBaseTerminalWidget::StopAccessValidation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AccessValidationTimer);
	}
	AccessValidationTimer.Invalidate();
}

void URpgBaseTerminalWidget::ValidateTerminalAccess()
{
	if (!bBaseTerminalContextBound || !IsActivated())
	{
		return;
	}

	APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		return;
	}
	if (StationComponent && StationComponent->CanActorAccess(OwningPawn))
	{
		return;
	}

	SetLocalFeedback(
		ERpgBaseTerminalFeedbackCode::AccessLost,
		ERpgBaseTerminalFeedbackSeverity::Warning,
		LOCTEXT("TerminalAccessLost", "Terminal access was lost. Move back into interaction range."));
	URpgUIScreenBlueprintLibrary::CloseUIScreen(
		GetOwningPlayer(),
		RpgGameplayTags::UI_Screen_BaseTerminal);
	if (IsActivated())
	{
		DeactivateWidget();
	}
}

void URpgBaseTerminalWidget::HandlePlayerInventoryPaneNavigationPanelsChanged()
{
	if (bBaseTerminalContextBound)
	{
		QueueDeferredInventoryScreenRefresh();
	}
}

URpgInventoryUiActionComponent*
URpgBaseTerminalWidget::ResolveInventoryUiActionComponent() const
{
	APlayerController* PlayerController = GetOwningPlayer();
	return PlayerController
		? PlayerController->FindComponentByClass<URpgInventoryUiActionComponent>()
		: nullptr;
}

void URpgBaseTerminalWidget::HandleDepositAllClicked()
{
	RequestDepositAllMaterials();
}

void URpgBaseTerminalWidget::HandleInstallUpgradeClicked()
{
	RequestInstallFeaturedUpgrade();
}

void URpgBaseTerminalWidget::HandleMaterialsDomainClicked()
{
	SetActiveDomain(ERpgBaseTerminalDomain::Materials);
}

void URpgBaseTerminalWidget::HandleArmoryDomainClicked()
{
	SetActiveDomain(ERpgBaseTerminalDomain::Armory);
}

void URpgBaseTerminalWidget::HandlePersonalDomainClicked()
{
	SetActiveDomain(ERpgBaseTerminalDomain::Personal);
}

void URpgBaseTerminalWidget::HandleRiftDomainClicked()
{
	SetActiveDomain(ERpgBaseTerminalDomain::Rift);
}

void URpgBaseTerminalWidget::HandleStorageSearchTextChanged(
	const FText& SearchText)
{
	if (BaseStorageViewModel)
	{
		BaseStorageViewModel->SetSearchText(SearchText);
	}
}

void URpgBaseTerminalWidget::HandleStorageCategoryFilterClicked()
{
	if (!BaseStorageViewModel)
	{
		return;
	}
	constexpr uint8 CategoryCount =
		static_cast<uint8>(ERpgInventoryItemCategory::Misc) + 1;
	const uint8 NextCategory =
		(static_cast<uint8>(BaseStorageViewModel->GetCategoryFilter()) + 1) %
		CategoryCount;
	BaseStorageViewModel->SetCategoryFilter(
		static_cast<ERpgInventoryItemCategory>(NextCategory));
}

void URpgBaseTerminalWidget::HandleStorageSortClicked()
{
	if (!BaseStorageViewModel)
	{
		return;
	}

	ERpgBaseResourceLocalSortMode NextMode =
		ERpgBaseResourceLocalSortMode::ReplicatedOrder;
	bool bDescending = false;
	switch (BaseStorageViewModel->GetLocalSortMode())
	{
	case ERpgBaseResourceLocalSortMode::ReplicatedOrder:
		NextMode = ERpgBaseResourceLocalSortMode::Name;
		break;
	case ERpgBaseResourceLocalSortMode::Name:
		NextMode = ERpgBaseResourceLocalSortMode::Category;
		break;
	case ERpgBaseResourceLocalSortMode::Category:
		NextMode = ERpgBaseResourceLocalSortMode::StoredCount;
		bDescending = true;
		break;
	case ERpgBaseResourceLocalSortMode::StoredCount:
		NextMode = ERpgBaseResourceLocalSortMode::FreeCapacity;
		break;
	case ERpgBaseResourceLocalSortMode::FreeCapacity:
		NextMode = ERpgBaseResourceLocalSortMode::FillRatio;
		bDescending = true;
		break;
	case ERpgBaseResourceLocalSortMode::FillRatio:
	default:
		break;
	}
	BaseStorageViewModel->SetLocalSort(NextMode, bDescending);
}

void URpgBaseTerminalWidget::HandleWithdrawOneClicked()
{
	RequestWithdrawResource(
		SelectedMaterialDefinition,
		ERpgBaseTerminalQuantityPreset::One);
}

void URpgBaseTerminalWidget::HandleWithdrawTenClicked()
{
	RequestWithdrawResource(
		SelectedMaterialDefinition,
		ERpgBaseTerminalQuantityPreset::Ten);
}

void URpgBaseTerminalWidget::HandleWithdrawMaxClicked()
{
	RequestWithdrawResource(
		SelectedMaterialDefinition,
		ERpgBaseTerminalQuantityPreset::Max);
}

void URpgBaseTerminalWidget::HandleCustomWithdrawCommitted(
	const FText& Text,
	ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnCleared ||
		CommitMethod == ETextCommit::Default)
	{
		return;
	}
	RequestWithdrawResource(
		SelectedMaterialDefinition,
		ERpgBaseTerminalQuantityPreset::Custom,
		ReadCustomWithdrawQuantity());
}

void URpgBaseTerminalWidget::HandleStabilizeRiftClicked()
{
	RequestStabilizeRiftItem(ResolveSelectedRiftItem());
}

void URpgBaseTerminalWidget::HandleExtractRiftClicked()
{
	URpgInventoryItemInstance* Item = ResolveSelectedRiftItem();
	if (!Item)
	{
		RequestExtractRiftItem(nullptr, false);
		return;
	}
	const bool bConfirmed =
		PendingExtractionConfirmationItemId == Item->GetItemId();
	RequestExtractRiftItem(Item, bConfirmed);
}

void URpgBaseTerminalWidget::HandleCleanseRiftClicked()
{
	RequestCleanseRiftStrain();
}

void URpgBaseTerminalWidget::HandleBaseStorageResourcesChanged()
{
	RefreshTerminalControlPresentation();
}

void URpgBaseTerminalWidget::HandleActiveInventorySelectionChanged()
{
	if (ActiveDomain == ERpgBaseTerminalDomain::Rift)
	{
		const URpgInventoryItemInstance* Item = ResolveSelectedRiftItem();
		if (!Item ||
			PendingExtractionConfirmationItemId != Item->GetItemId())
		{
			PendingExtractionConfirmationItemId = FRpgInventoryItemId();
		}
		RefreshRiftControlPresentation();
	}
}

void URpgBaseTerminalWidget::HandleMaterialSelectionChanged(
	UObject* SelectedItem)
{
	const URpgBaseResourceEntryViewModel* Resource =
		Cast<URpgBaseResourceEntryViewModel>(SelectedItem);
	SelectedMaterialDefinition = Resource
		? Resource->GetItemDefinition()
		: nullptr;
	RefreshMaterialControlPresentation();
}

void URpgBaseTerminalWidget::HandleInstalledUpgradesChanged(
	URpgBaseStorageStationComponent* ChangedStation)
{
	if (ChangedStation == StationComponent)
	{
		if (BaseStorageViewModel)
		{
			BaseStorageViewModel->SetAllowedResources(
				ChangedStation->GetAllowedResourceDefinitions());
		}
		ReleaseDomainPanes();
		BindAvailableDomainPanes();
		UpdateDomainPresentation();
		ConfigureQuickTransferRoutes();
		UpdateTerminalActionAvailability();
	}
}

#undef LOCTEXT_NAMESPACE
