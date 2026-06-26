#include "RpgLoadoutViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgLoadoutViewModels)

namespace
{
	struct FRpgLoadoutItemPresentation
	{
		TSoftObjectPtr<UTexture2D> Icon;
		FText ShortDisplayName;
	};

	FRpgLoadoutItemPresentation BuildItemPresentation(const URpgInventoryItemInstance* ItemInstance)
	{
		FRpgLoadoutItemPresentation Presentation;
		if (!ItemInstance)
		{
			return Presentation;
		}

		FText DisplayName = FText::GetEmpty();
		if (const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition = ItemInstance->GetItemDef())
		{
			if (const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDefinition))
			{
				DisplayName = ItemCDO->DisplayName;
			}
		}

		if (const URpgInventoryFragment_UIData* UIData = ItemInstance->FindFragmentByClass<URpgInventoryFragment_UIData>())
		{
			Presentation.Icon = UIData->Icon;
			Presentation.ShortDisplayName = UIData->ShortDisplayName.IsEmpty() ? DisplayName : UIData->ShortDisplayName;
		}
		else
		{
			Presentation.ShortDisplayName = DisplayName;
		}

		return Presentation;
	}

	FText EquipmentSlotToDisplayText(ERpgEquipmentSlot EquipmentSlot)
	{
		switch (EquipmentSlot)
		{
		case ERpgEquipmentSlot::MainHand:
			return NSLOCTEXT("RpgEquipmentSlots", "MainHand", "Main Hand");
		case ERpgEquipmentSlot::OffHand:
			return NSLOCTEXT("RpgEquipmentSlots", "OffHand", "Off Hand");
		case ERpgEquipmentSlot::Head:
			return NSLOCTEXT("RpgEquipmentSlots", "Head", "Head");
		case ERpgEquipmentSlot::Chest:
			return NSLOCTEXT("RpgEquipmentSlots", "Chest", "Chest");
		case ERpgEquipmentSlot::Hands:
			return NSLOCTEXT("RpgEquipmentSlots", "Hands", "Hands");
		case ERpgEquipmentSlot::Legs:
			return NSLOCTEXT("RpgEquipmentSlots", "Legs", "Legs");
		case ERpgEquipmentSlot::Feet:
			return NSLOCTEXT("RpgEquipmentSlots", "Feet", "Feet");
		default:
			return FText::GetEmpty();
		}
	}
}

void URpgQuickBarSlotViewModel::InitializeSlot(int32 InSlotIndex, const FRpgQuickBarLoadoutSlot& InLoadoutSlot, bool bInActiveSlot)
{
	const bool bWasChanged =
		SlotIndex != InSlotIndex ||
		bIsActiveSlot != bInActiveSlot ||
		MainHandItem != InLoadoutSlot.MainHandItem ||
		OffHandItem != InLoadoutSlot.OffHandItem;

	SlotIndex = InSlotIndex;
	KeyLabel = InSlotIndex >= 0 ? FText::AsNumber(InSlotIndex + 1) : FText::GetEmpty();
	bIsActiveSlot = bInActiveSlot;
	MainHandItem = InLoadoutSlot.MainHandItem;
	OffHandItem = InLoadoutSlot.OffHandItem;
	bHasAnyItem = MainHandItem != nullptr || OffHandItem != nullptr;

	const FRpgLoadoutItemPresentation MainHandPresentation = BuildItemPresentation(MainHandItem);
	MainHandIcon = MainHandPresentation.Icon;
	MainHandShortDisplayName = MainHandPresentation.ShortDisplayName;

	const FRpgLoadoutItemPresentation OffHandPresentation = BuildItemPresentation(OffHandItem);
	OffHandIcon = OffHandPresentation.Icon;
	OffHandShortDisplayName = OffHandPresentation.ShortDisplayName;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(KeyLabel);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsActiveSlot);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasAnyItem);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MainHandItem);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OffHandItem);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MainHandIcon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OffHandIcon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(MainHandShortDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(OffHandShortDisplayName);

	if (bWasChanged)
	{
		OnSlotChanged.Broadcast(this);
	}
}

URpgInventoryItemInstance* URpgQuickBarSlotViewModel::GetItemForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	switch (EquipmentSlot)
	{
	case ERpgEquipmentSlot::MainHand:
		return MainHandItem.Get();
	case ERpgEquipmentSlot::OffHand:
		return OffHandItem.Get();
	default:
		return nullptr;
	}
}

bool URpgQuickBarSlotViewModel::HasItemForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	return GetItemForEquipmentSlot(EquipmentSlot) != nullptr;
}

TSoftObjectPtr<UTexture2D> URpgQuickBarSlotViewModel::GetIconForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	switch (EquipmentSlot)
	{
	case ERpgEquipmentSlot::MainHand:
		return MainHandIcon;
	case ERpgEquipmentSlot::OffHand:
		return OffHandIcon;
	default:
		return TSoftObjectPtr<UTexture2D>();
	}
}

FText URpgQuickBarSlotViewModel::GetShortDisplayNameForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	switch (EquipmentSlot)
	{
	case ERpgEquipmentSlot::MainHand:
		return MainHandShortDisplayName;
	case ERpgEquipmentSlot::OffHand:
		return OffHandShortDisplayName;
	default:
		return FText::GetEmpty();
	}
}

void URpgQuickBarViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	BindQuickBar(nullptr);
}

void URpgQuickBarViewModel::BindQuickBar(URpgQuickBarComponent* InQuickBar)
{
	if (ObservedQuickBar.Get() == InQuickBar)
	{
		RefreshSlots();
		return;
	}

	UnregisterMessageListeners();
	ObservedQuickBar = InQuickBar;
	RegisterMessageListeners();
	RefreshSlots();
}

void URpgQuickBarViewModel::UnbindQuickBar()
{
	UnregisterMessageListeners();
	ObservedQuickBar.Reset();
	ActiveSlotIndex = INDEX_NONE;
	RefreshSlots();
}

void URpgQuickBarViewModel::RefreshSlots()
{
	URpgQuickBarComponent* QuickBar = ObservedQuickBar.Get();
	const TArray<FRpgQuickBarLoadoutSlot> LoadoutSlots = QuickBar ? QuickBar->GetLoadoutSlots() : TArray<FRpgQuickBarLoadoutSlot>();
	ActiveSlotIndex = QuickBar ? QuickBar->GetActiveSlotIndex() : INDEX_NONE;

	const int32 SlotCount = QuickBar
		? FMath::Max(QuickBar->GetNumSlots(), LoadoutSlots.Num())
		: FMath::Max(1, DefaultSlotCount);

	TArray<TObjectPtr<URpgQuickBarSlotViewModel>> PreviousSlots = MoveTemp(Slots);
	Slots.Reset();
	Slots.Reserve(SlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		URpgQuickBarSlotViewModel* SlotViewModel = PreviousSlots.IsValidIndex(SlotIndex) ? PreviousSlots[SlotIndex].Get() : nullptr;
		if (!SlotViewModel)
		{
			SlotViewModel = NewObject<URpgQuickBarSlotViewModel>(this);
		}

		const FRpgQuickBarLoadoutSlot EmptySlot;
		const FRpgQuickBarLoadoutSlot& LoadoutSlot = LoadoutSlots.IsValidIndex(SlotIndex) ? LoadoutSlots[SlotIndex] : EmptySlot;
		SlotViewModel->InitializeSlot(SlotIndex, LoadoutSlot, SlotIndex == ActiveSlotIndex);
		Slots.Add(SlotViewModel);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ActiveSlotIndex);
	OnSlotsChanged.Broadcast();
}

TArray<URpgQuickBarSlotViewModel*> URpgQuickBarViewModel::GetSlots() const
{
	TArray<URpgQuickBarSlotViewModel*> Result;
	Result.Reserve(Slots.Num());
	for (URpgQuickBarSlotViewModel* Slot : Slots)
	{
		Result.Add(Slot);
	}
	return Result;
}

URpgQuickBarSlotViewModel* URpgQuickBarViewModel::GetSlotAtIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

void URpgQuickBarViewModel::BeginDestroy()
{
	UnregisterMessageListeners();
	Super::BeginDestroy();
}

void URpgQuickBarViewModel::RegisterMessageListeners()
{
	UnregisterMessageListeners();
}

void URpgQuickBarViewModel::UnregisterMessageListeners()
{
	if (SlotsChangedHandle.IsValid())
	{
		SlotsChangedHandle.Unregister();
	}

	if (ActiveIndexChangedHandle.IsValid())
	{
		ActiveIndexChangedHandle.Unregister();
	}
}

void URpgQuickBarViewModel::HandleQuickBarSlotsChanged(FGameplayTag Channel, const FRpgQuickBarSlotsChangedMessage& Message)
{
	const URpgQuickBarComponent* QuickBar = ObservedQuickBar.Get();
	if (QuickBar && Message.Owner == QuickBar->GetOwner())
	{
		RefreshSlots();
	}
}

void URpgQuickBarViewModel::HandleQuickBarActiveIndexChanged(FGameplayTag Channel, const FRpgQuickBarActiveIndexChangedMessage& Message)
{
	const URpgQuickBarComponent* QuickBar = ObservedQuickBar.Get();
	if (QuickBar && Message.Owner == QuickBar->GetOwner())
	{
		RefreshSlots();
	}
}

void URpgEquipmentSlotViewModel::InitializeSlot(ERpgEquipmentSlot InEquipmentSlot, URpgInventoryItemInstance* InItem)
{
	const bool bWasChanged = EquipmentSlot != InEquipmentSlot || ItemInstance != InItem;

	EquipmentSlot = InEquipmentSlot;
	SlotLabel = EquipmentSlotToDisplayText(InEquipmentSlot);
	ItemInstance = InItem;
	bHasItem = ItemInstance != nullptr;

	const FRpgLoadoutItemPresentation Presentation = BuildItemPresentation(ItemInstance);
	Icon = Presentation.Icon;
	ShortDisplayName = Presentation.ShortDisplayName;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EquipmentSlot);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotLabel);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasItem);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);

	if (bWasChanged)
	{
		OnSlotChanged.Broadcast(this);
	}
}

void URpgEquipmentLoadoutViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(InPlayerController);
	BindEquipmentLoadout(RpgPlayerController ? RpgPlayerController->GetEquipmentLoadoutComponent() : nullptr);
}

void URpgEquipmentLoadoutViewModel::BindEquipmentLoadout(URpgEquipmentLoadoutComponent* InEquipmentLoadout)
{
	if (ObservedEquipmentLoadout.Get() == InEquipmentLoadout)
	{
		RefreshSlots();
		return;
	}

	UnregisterMessageListener();
	ObservedEquipmentLoadout = InEquipmentLoadout;
	RegisterMessageListener();
	RefreshSlots();
}

void URpgEquipmentLoadoutViewModel::UnbindEquipmentLoadout()
{
	UnregisterMessageListener();
	ObservedEquipmentLoadout.Reset();
	RefreshSlots();
}

void URpgEquipmentLoadoutViewModel::RefreshSlots()
{
	URpgEquipmentLoadoutComponent* EquipmentLoadout = ObservedEquipmentLoadout.Get();

	TArray<TObjectPtr<URpgEquipmentSlotViewModel>> PreviousSlots = MoveTemp(Slots);
	Slots.Reset();
	Slots.Reserve(GetDefaultEquipmentSlotOrder().Num());

	int32 SlotViewModelIndex = 0;
	for (const ERpgEquipmentSlot EquipmentSlot : GetDefaultEquipmentSlotOrder())
	{
		URpgEquipmentSlotViewModel* SlotViewModel = PreviousSlots.IsValidIndex(SlotViewModelIndex) ? PreviousSlots[SlotViewModelIndex].Get() : nullptr;
		if (!SlotViewModel)
		{
			SlotViewModel = NewObject<URpgEquipmentSlotViewModel>(this);
		}

		URpgInventoryItemInstance* Item = EquipmentLoadout ? EquipmentLoadout->GetItemInEquipmentSlot(EquipmentSlot) : nullptr;
		SlotViewModel->InitializeSlot(EquipmentSlot, Item);
		Slots.Add(SlotViewModel);
		++SlotViewModelIndex;
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	OnSlotsChanged.Broadcast();
}

TArray<URpgEquipmentSlotViewModel*> URpgEquipmentLoadoutViewModel::GetSlots() const
{
	TArray<URpgEquipmentSlotViewModel*> Result;
	Result.Reserve(Slots.Num());
	for (URpgEquipmentSlotViewModel* Slot : Slots)
	{
		Result.Add(Slot);
	}
	return Result;
}

URpgEquipmentSlotViewModel* URpgEquipmentLoadoutViewModel::GetSlotForEquipmentSlot(ERpgEquipmentSlot EquipmentSlot) const
{
	for (URpgEquipmentSlotViewModel* Slot : Slots)
	{
		if (Slot && Slot->GetEquipmentSlot() == EquipmentSlot)
		{
			return Slot;
		}
	}
	return nullptr;
}

void URpgEquipmentLoadoutViewModel::BeginDestroy()
{
	UnregisterMessageListener();
	Super::BeginDestroy();
}

void URpgEquipmentLoadoutViewModel::RegisterMessageListener()
{
	UnregisterMessageListener();

	URpgEquipmentLoadoutComponent* EquipmentLoadout = ObservedEquipmentLoadout.Get();
	UWorld* World = EquipmentLoadout ? EquipmentLoadout->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	SlotsChangedHandle = MessageSubsystem.RegisterListener<FRpgEquipmentLoadoutSlotsChangedMessage>(
		RpgGameplayTags::Rpg_EquipmentLoadout_Message_SlotsChanged,
		this,
		&ThisClass::HandleEquipmentLoadoutSlotsChanged);
}

void URpgEquipmentLoadoutViewModel::UnregisterMessageListener()
{
	if (SlotsChangedHandle.IsValid())
	{
		SlotsChangedHandle.Unregister();
	}
}

void URpgEquipmentLoadoutViewModel::HandleEquipmentLoadoutSlotsChanged(FGameplayTag Channel, const FRpgEquipmentLoadoutSlotsChangedMessage& Message)
{
	const URpgEquipmentLoadoutComponent* EquipmentLoadout = ObservedEquipmentLoadout.Get();
	if (EquipmentLoadout && Message.Owner == EquipmentLoadout->GetOwner())
	{
		RefreshSlots();
	}
}

TConstArrayView<ERpgEquipmentSlot> URpgEquipmentLoadoutViewModel::GetDefaultEquipmentSlotOrder()
{
	static constexpr ERpgEquipmentSlot DefaultSlots[] =
	{
		ERpgEquipmentSlot::Head,
		ERpgEquipmentSlot::Chest,
		ERpgEquipmentSlot::Hands,
		ERpgEquipmentSlot::Legs,
		ERpgEquipmentSlot::Feet
	};

	return TConstArrayView<ERpgEquipmentSlot>(DefaultSlots, UE_ARRAY_COUNT(DefaultSlots));
}
