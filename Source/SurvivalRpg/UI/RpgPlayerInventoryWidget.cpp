#include "RpgPlayerInventoryWidget.h"

#include "MVVMSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgLoadoutSlotWidgets.h"
#include "SurvivalRpg/UI/RpgPlayerInventoryLayoutViews.h"
#include "View/MVVMView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPlayerInventoryWidget)

URpgPlayerInventoryWidget::URpgPlayerInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URpgPlayerInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();
	BP_OnPlayerInventoryViewModelReady(PlayerInventoryViewModel);
}

void URpgPlayerInventoryWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	EnsurePlayerInventoryCoordinator();
	BindPlayerInventoryViewModel();
}

void URpgPlayerInventoryWidget::NativeOnDeactivated()
{
	if (DragDropCoordinator && DragDropCoordinator->HasHeldPayload())
	{
		DragDropCoordinator->CancelHold();
	}

	Super::NativeOnDeactivated();
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryCoordinator()
{
	if (!DragDropCoordinator)
	{
		DragDropCoordinator = URpgInventoryDragDropCoordinator::CreateInventoryDragDropCoordinator(this, GetOwningPlayer());
	}

	ForwardCoordinatorToChildren();
}

void URpgPlayerInventoryWidget::BindPlayerInventoryViewModel()
{
	EnsurePlayerInventoryViewModel();
	BindViewModelDelegates();

	if (PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel->BindPlayerController(GetOwningPlayer());
	}

	RefreshPlayerInventoryViews();
}

void URpgPlayerInventoryWidget::RefreshPlayerInventoryViews()
{
	RefreshGearSlots();
	RefreshSlotGroups();
	RefreshActionBar();
}

FString URpgPlayerInventoryWidget::GetPlayerInventoryWidgetDebugSummary() const
{
	const int32 CarryCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetCarryGroups().Num() : INDEX_NONE;
	const int32 InventoryGroupCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetInventoryGroups().Num() : INDEX_NONE;
	const int32 ActionBarCount = PlayerInventoryViewModel ? PlayerInventoryViewModel->GetActionBarSlots().Num() : INDEX_NONE;

	return FString::Printf(
		TEXT("PlayerInventoryWidget VM=%s Coordinator=%s CarryGroupsList=%s InventoryGroupsList=%s ActionBarTileView=%s CarryGroups=%d InventoryGroups=%d ActionBarSlots=%d"),
		*GetNameSafe(PlayerInventoryViewModel),
		*GetNameSafe(DragDropCoordinator),
		*GetNameSafe(CarryGroupsList),
		*GetNameSafe(InventoryGroupsList),
		*GetNameSafe(ActionBarTileView),
		CarryCount,
		InventoryGroupCount,
		ActionBarCount);
}

void URpgPlayerInventoryWidget::RefreshSlotGroups()
{
	if (!PlayerInventoryViewModel)
	{
		return;
	}

	if (CarryGroupsList)
	{
		CarryGroupsList->SetSlotGroupItems(PlayerInventoryViewModel->GetCarryGroups());
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->SetSlotGroupItems(PlayerInventoryViewModel->GetInventoryGroups());
	}
}

void URpgPlayerInventoryWidget::RefreshActionBar()
{
	if (!PlayerInventoryViewModel || !ActionBarTileView)
	{
		return;
	}

	ActionBarTileView->SetActionBarSlotItems(PlayerInventoryViewModel->GetActionBarSlots());
}

void URpgPlayerInventoryWidget::RefreshGearSlots()
{
	if (!PlayerInventoryViewModel)
	{
		return;
	}

	SetGearSlotViewModel(Gear_Head, ERpgEquipmentSlot::Head, false);
	SetGearSlotViewModel(Gear_Chest, ERpgEquipmentSlot::Chest, false);
	SetGearSlotViewModel(Gear_Hands, ERpgEquipmentSlot::Hands, false);
	SetGearSlotViewModel(Gear_Legs, ERpgEquipmentSlot::Legs, false);
	SetGearSlotViewModel(Gear_Feet, ERpgEquipmentSlot::Feet, false);

	SetGearSlotViewModel(Gear_Backpack, ERpgEquipmentSlot::Backpack, true);
	SetGearSlotViewModel(Gear_Belt, ERpgEquipmentSlot::Belt, true);
	SetGearSlotViewModel(Gear_Pouch, ERpgEquipmentSlot::Pouch, true);
	SetGearSlotViewModel(Gear_ResourceBag, ERpgEquipmentSlot::ResourceBag, true);
}

void URpgPlayerInventoryWidget::HandleGearSlotsChanged()
{
	RefreshGearSlots();
}

void URpgPlayerInventoryWidget::HandleSlotGroupsChanged()
{
	RefreshSlotGroups();
}

void URpgPlayerInventoryWidget::HandleActionBarSlotsChanged()
{
	RefreshActionBar();
}

void URpgPlayerInventoryWidget::EnsurePlayerInventoryViewModel()
{
	if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this))
	{
		View->InitializeSources();
		for (const FMVVMView_Source& Source : View->GetSources())
		{
			if (URpgPlayerInventoryViewModel* ExistingViewModel = Cast<URpgPlayerInventoryViewModel>(Source.Source))
			{
				if (PlayerInventoryViewModel != ExistingViewModel)
				{
					bViewModelDelegatesBound = false;
				}
				PlayerInventoryViewModel = ExistingViewModel;
				return;
			}
		}
	}

	if (!PlayerInventoryViewModel)
	{
		PlayerInventoryViewModel = NewObject<URpgPlayerInventoryViewModel>(this);
	}
}

void URpgPlayerInventoryWidget::BindViewModelDelegates()
{
	if (!PlayerInventoryViewModel || bViewModelDelegatesBound)
	{
		return;
	}

	PlayerInventoryViewModel->OnGearSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleGearSlotsChanged);
	PlayerInventoryViewModel->OnSlotGroupsChanged.AddUniqueDynamic(this, &ThisClass::HandleSlotGroupsChanged);
	PlayerInventoryViewModel->OnActionBarSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleActionBarSlotsChanged);
	bViewModelDelegatesBound = true;
}

void URpgPlayerInventoryWidget::SetGearSlotViewModel(URpgEquipmentSlotWidget* GearSlotWidget, ERpgEquipmentSlot EquipmentSlot, bool bBagSlot) const
{
	if (!GearSlotWidget || !PlayerInventoryViewModel)
	{
		return;
	}

	GearSlotWidget->SetDragDropCoordinator(DragDropCoordinator);
	GearSlotWidget->SetEquipmentSlotViewModel(
		bBagSlot
			? PlayerInventoryViewModel->GetBagSlot(EquipmentSlot)
			: PlayerInventoryViewModel->GetArmorSlot(EquipmentSlot));
}

void URpgPlayerInventoryWidget::ForwardCoordinatorToChildren()
{
	if (CarryGroupsList)
	{
		CarryGroupsList->SetDragDropCoordinator(DragDropCoordinator);
	}

	if (InventoryGroupsList)
	{
		InventoryGroupsList->SetDragDropCoordinator(DragDropCoordinator);
	}

	if (ActionBarTileView)
	{
		ActionBarTileView->SetDragDropCoordinator(DragDropCoordinator);
	}

	if (Gear_Head)
	{
		Gear_Head->SetDragDropCoordinator(DragDropCoordinator);
	}
	if (Gear_Chest)
	{
		Gear_Chest->SetDragDropCoordinator(DragDropCoordinator);
	}
	if (Gear_Hands)
	{
		Gear_Hands->SetDragDropCoordinator(DragDropCoordinator);
	}
	if (Gear_Legs)
	{
		Gear_Legs->SetDragDropCoordinator(DragDropCoordinator);
	}
	if (Gear_Feet)
	{
		Gear_Feet->SetDragDropCoordinator(DragDropCoordinator);
	}
	if (Gear_Backpack)
	{
		Gear_Backpack->SetDragDropCoordinator(DragDropCoordinator);
	}
	if (Gear_Belt)
	{
		Gear_Belt->SetDragDropCoordinator(DragDropCoordinator);
	}
	if (Gear_Pouch)
	{
		Gear_Pouch->SetDragDropCoordinator(DragDropCoordinator);
	}
	if (Gear_ResourceBag)
	{
		Gear_ResourceBag->SetDragDropCoordinator(DragDropCoordinator);
	}
}
