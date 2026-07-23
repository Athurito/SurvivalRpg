#include "RpgBaseTerminalWidget.h"

#include "Components/Button.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIInputTypes.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"
#include "SurvivalRpg/UI/RpgBaseResourceListWidget.h"
#include "SurvivalRpg/UI/RpgInventoryPanelNavigationCoordinator.h"
#include "SurvivalRpg/UI/RpgInventorySpatialPaneWidget.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgBaseTerminalWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRpgBaseTerminalWidget, Log, All);

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
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
	}
	if (BaseResourceList)
	{
		BaseResourceList->ReleaseBaseStoragePresentation();
	}

	UpdateTerminalActionAvailability();
}

void URpgBaseTerminalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RegisterBaseTerminalActionBindings();
	UpdateTerminalActionAvailability();
}

void URpgBaseTerminalWidget::NativeOnDeactivated()
{
	UnregisterBaseTerminalActionBindings();
	Super::NativeOnDeactivated();
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
	if (!bBaseTerminalContextBound || !PlayerInventoryPane)
	{
		return;
	}

	PlayerInventoryPane->SetInteractionContext(
		GetScreenDragDropCoordinator(),
		GetScreenPanelNavigationCoordinator(),
		TEXT("BaseTerminal.Player.Pockets"),
		this);
}

void URpgBaseTerminalWidget::RegisterInventoryScreenNavigationPanels(
	URpgInventoryPanelNavigationCoordinator* Navigator)
{
	if (Navigator && bBaseTerminalContextBound && PlayerInventoryPane)
	{
		PlayerInventoryPane->RegisterNavigationPanel(Navigator);
	}
}

void URpgBaseTerminalWidget::AppendInventoryScreenSpatialGrids(
	TArray<URpgInventorySpatialGridWidget*>& OutGrids) const
{
	if (PlayerInventoryPane && PlayerInventoryPane->GetSpatialGrid())
	{
		OutGrids.AddUnique(PlayerInventoryPane->GetSpatialGrid());
	}
}

void URpgBaseTerminalWidget::RequestDepositAllMaterials()
{
	if (!bBaseTerminalContextBound || !StationComponent)
	{
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		UiActions->RequestDepositAllMaterialsToBase(StationComponent);
	}
}

void URpgBaseTerminalWidget::RequestInstallFeaturedUpgrade()
{
	if (!bBaseTerminalContextBound ||
		!StationComponent ||
		FeaturedUpgrade.IsNull())
	{
		return;
	}

	URpgBaseStorageUpgradeDefinition* Upgrade = FeaturedUpgrade.LoadSynchronous();
	if (!Upgrade || !StationComponent->CanInstallUpgrade(Upgrade))
	{
		UpdateTerminalActionAvailability();
		return;
	}

	if (URpgInventoryUiActionComponent* UiActions =
		ResolveInventoryUiActionComponent())
	{
		UiActions->RequestInstallBaseStorageUpgrade(
			StationComponent,
			Upgrade);
	}
}

void URpgBaseTerminalWidget::ApplyBaseStorageScreenPayload(UObject* Payload)
{
	URpgBaseStorageScreenPayload* NewPayload =
		Cast<URpgBaseStorageScreenPayload>(Payload);
	if (!IsPayloadCoherent(NewPayload))
	{
		ResetBaseTerminalContext();
		return;
	}

	const bool bContextChanged =
		BaseStorageScreenPayload != NewPayload ||
		PlayerInventory != NewPayload->PlayerInventory ||
		ArmoryInventory != NewPayload->ArmoryInventory ||
		BaseStorage != NewPayload->BaseStorage ||
		StationComponent != NewPayload->StationComponent;
	if (bContextChanged)
	{
		ResetBaseTerminalContext();
	}

	BaseStorageScreenPayload = NewPayload;
	PlayerInventory = NewPayload->PlayerInventory;
	ArmoryInventory = NewPayload->ArmoryInventory;
	BaseStorage = NewPayload->BaseStorage;
	StationComponent = NewPayload->StationComponent;

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
	if (GetOwningPlayer())
	{
		URpgInventoryManagerComponent* CanonicalPlayerInventory =
			Coordinator ? Coordinator->GetPlayerInventory() : nullptr;
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

	const URpgPlayerInventoryLayoutComponent* InventoryLayout =
		GetOwningPlayer()
			? GetOwningPlayer()->FindComponentByClass<URpgPlayerInventoryLayoutComponent>()
			: nullptr;
	FRpgInventorySlotGroupView PrimaryContentGroup;
	if (!InventoryLayout ||
		!InventoryLayout->TryGetSlotGroupBySemanticRole(
			RpgGameplayTags::Rpg_Inventory_Layout_Role_Content_Primary,
			PrimaryContentGroup) ||
		PrimaryContentGroup.GroupKind != ERpgInventorySlotGroupKind::Content)
	{
		UE_LOG(
			LogRpgBaseTerminalWidget,
			Warning,
			TEXT("%s rejected BaseTerminal payload: the owning player's layout has no unique primary content role."),
			*GetNameSafe(this));
		ResetBaseTerminalContext();
		return false;
	}

	bBaseTerminalContextBound = true;
	PlayerPaneContainerHandle = PrimaryContentGroup.ContainerHandle;

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
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->BindInventoryContainer(
			PlayerInventory,
			PlayerPaneContainerHandle);
	}

	StationComponent->OnInstalledUpgradesChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleInstalledUpgradesChanged);
	UpdateTerminalActionAvailability();
	++BaseTerminalPresentationBindGeneration;
	return true;
}

void URpgBaseTerminalWidget::EnsureBaseStorageViewModel()
{
	if (!BaseStorageViewModel)
	{
		BaseStorageViewModel = NewObject<URpgBaseStorageViewModel>(this);
	}
}

void URpgBaseTerminalWidget::EnsureDefaultBaseTerminalActionRows()
{
	if (DepositAllInputAction.DataTable &&
		!DepositAllInputAction.RowName.IsNone() &&
		InstallUpgradeInputAction.DataTable &&
		!InstallUpgradeInputAction.RowName.IsNone())
	{
		return;
	}

	UDataTable* ActionTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/SurvivalRpg/UI/Input/DT_RpgUIActions_BaseTerminal.DT_RpgUIActions_BaseTerminal"));
	if (!ActionTable)
	{
		UE_LOG(
			LogRpgBaseTerminalWidget,
			Warning,
			TEXT("%s could not load the BaseTerminal CommonUI action table."),
			*GetNameSafe(this));
		return;
	}

	DepositAllInputAction.DataTable = ActionTable;
	DepositAllInputAction.RowName = TEXT("UI.BaseTerminal.DepositAll");
	InstallUpgradeInputAction.DataTable = ActionTable;
	InstallUpgradeInputAction.RowName = TEXT("UI.BaseTerminal.InstallUpgrade");
}

void URpgBaseTerminalWidget::RegisterBaseTerminalActionBindings()
{
	UnregisterBaseTerminalActionBindings();
	EnsureDefaultBaseTerminalActionRows();

	if (DepositAllInputAction.DataTable &&
		!DepositAllInputAction.RowName.IsNone())
	{
		DepositAllActionBinding = RegisterUIActionBinding(
			FBindUIActionArgs(
				DepositAllInputAction,
				true,
				FSimpleDelegate::CreateUObject(
					this,
					&ThisClass::RequestDepositAllMaterials)));
	}
	if (InstallUpgradeInputAction.DataTable &&
		!InstallUpgradeInputAction.RowName.IsNone())
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

void URpgBaseTerminalWidget::ResetBaseTerminalContext()
{
	bBaseTerminalContextBound = false;

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
	if (PlayerInventoryPane)
	{
		PlayerInventoryPane->ReleaseInventoryPresentation();
	}
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
	BaseStorage = nullptr;
	StationComponent = nullptr;
	PlayerPaneContainerHandle = FRpgInventoryContainerHandle();
	UpdateTerminalActionAvailability();
}

void URpgBaseTerminalWidget::UpdateTerminalActionAvailability()
{
	const bool bCanUseTerminal =
		bBaseTerminalContextBound && StationComponent != nullptr;
	if (DepositAllButton)
	{
		DepositAllButton->SetIsEnabled(bCanUseTerminal);
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
	}
	if (InstallUpgradeActionBinding.IsValid())
	{
		InstallUpgradeActionBinding.SetDisplayInActionBar(
			bCanInstallUpgrade);
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
		UpdateTerminalActionAvailability();
	}
}
