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
		case ERpgEquipmentSlot::Backpack:
			return NSLOCTEXT("RpgEquipmentSlots", "Backpack", "Backpack");
		case ERpgEquipmentSlot::Belt:
			return NSLOCTEXT("RpgEquipmentSlots", "Belt", "Belt");
		case ERpgEquipmentSlot::Pouch:
			return NSLOCTEXT("RpgEquipmentSlots", "Pouch", "Pouch");
		case ERpgEquipmentSlot::ResourceBag:
			return NSLOCTEXT("RpgEquipmentSlots", "ResourceBag", "Resource Bag");
		default:
			return FText::GetEmpty();
		}
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
