#include "RpgActionBarViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarViewModels)

namespace
{
	struct FRpgActionSlotPresentation
	{
		TSoftObjectPtr<UTexture2D> Icon;
		FText ShortDisplayName;
	};

	FRpgActionSlotPresentation BuildItemPresentation(const URpgInventoryItemInstance* ItemInstance)
	{
		FRpgActionSlotPresentation Presentation;
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

	FText AbilityIdToDisplayText(FGameplayTag AbilityIdTag)
	{
		return AbilityIdTag.IsValid()
			? FText::FromName(AbilityIdTag.GetTagName())
			: FText::GetEmpty();
	}
}

void URpgActionBarSlotViewModel::InitializeSlot(int32 InSlotIndex, const FRpgActionBarSlot& InSlot, int32 InStackCount)
{
	const bool bWasChanged =
		SlotIndex != InSlotIndex ||
		SlotType != InSlot.SlotType ||
		ItemInstance != InSlot.ItemInstance ||
		AbilityIdTag != InSlot.AbilityIdTag ||
		StackCount != InStackCount;

	SlotIndex = InSlotIndex;
	SlotType = InSlot.SlotType;
	bHasContent = !InSlot.IsEmpty();
	ItemInstance = InSlot.ItemInstance;
	AbilityIdTag = InSlot.AbilityIdTag;
	StackCount = InSlot.SlotType == ERpgActionBarSlotType::InventoryItem ? InStackCount : 0;
	HotkeyActionRowName = InSlotIndex >= 0
		? FName(*FString::Printf(TEXT("UI.ActionBar.Slot.%d"), InSlotIndex + 1))
		: NAME_None;

	const FRpgActionSlotPresentation Presentation = BuildItemPresentation(ItemInstance);
	Icon = Presentation.Icon;
	ShortDisplayName = InSlot.SlotType == ERpgActionBarSlotType::Ability
		? AbilityIdToDisplayText(AbilityIdTag)
		: Presentation.ShortDisplayName;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotType);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasContent);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AbilityIdTag);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HotkeyActionRowName);

	if (bWasChanged)
	{
		OnSlotChanged.Broadcast(this);
	}
}

void URpgActionBarViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(InPlayerController);
	const ARpgPlayerState* RpgPlayerState = RpgPlayerController ? RpgPlayerController->GetRpgPlayerState() : nullptr;
	BindActionBar(
		RpgPlayerController ? RpgPlayerController->GetActionBarComponent() : nullptr,
		RpgPlayerState ? RpgPlayerState->GetInventoryManagerComponent() : nullptr);
}

void URpgActionBarViewModel::BindActionBar(URpgActionBarComponent* InActionBar, URpgInventoryManagerComponent* InPlayerInventory)
{
	if (ObservedActionBar.Get() == InActionBar && ObservedPlayerInventory.Get() == InPlayerInventory)
	{
		RefreshSlots();
		return;
	}

	UnregisterMessageListener();
	ObservedActionBar = InActionBar;
	ObservedPlayerInventory = InPlayerInventory;
	RegisterMessageListener();
	RefreshSlots();
}

void URpgActionBarViewModel::UnbindActionBar()
{
	UnregisterMessageListener();
	ObservedActionBar.Reset();
	ObservedPlayerInventory.Reset();
	RefreshSlots();
}

void URpgActionBarViewModel::RefreshSlots()
{
	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	const TArray<FRpgActionBarSlot> SourceSlots = ActionBar ? ActionBar->GetSlots() : TArray<FRpgActionBarSlot>();
	const int32 SlotCount = ActionBar ? FMath::Max(ActionBar->GetNumSlots(), SourceSlots.Num()) : FMath::Max(1, DefaultSlotCount);

	TArray<TObjectPtr<URpgActionBarSlotViewModel>> PreviousSlots = MoveTemp(Slots);
	Slots.Reset();
	Slots.Reserve(SlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		URpgActionBarSlotViewModel* SlotViewModel = PreviousSlots.IsValidIndex(SlotIndex) ? PreviousSlots[SlotIndex].Get() : nullptr;
		if (!SlotViewModel)
		{
			SlotViewModel = NewObject<URpgActionBarSlotViewModel>(this);
		}

		const FRpgActionBarSlot EmptySlot;
		const FRpgActionBarSlot& SourceSlot = SourceSlots.IsValidIndex(SlotIndex) ? SourceSlots[SlotIndex] : EmptySlot;
		const int32 StackCount = (PlayerInventory && SourceSlot.ItemInstance)
			? PlayerInventory->GetItemStackCount(SourceSlot.ItemInstance)
			: 0;
		SlotViewModel->InitializeSlot(SlotIndex, SourceSlot, StackCount);
		Slots.Add(SlotViewModel);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	OnSlotsChanged.Broadcast();
}

TArray<URpgActionBarSlotViewModel*> URpgActionBarViewModel::GetSlots() const
{
	TArray<URpgActionBarSlotViewModel*> Result;
	Result.Reserve(Slots.Num());
	for (URpgActionBarSlotViewModel* Slot : Slots)
	{
		Result.Add(Slot);
	}
	return Result;
}

URpgActionBarSlotViewModel* URpgActionBarViewModel::GetSlotAtIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

void URpgActionBarViewModel::BeginDestroy()
{
	UnregisterMessageListener();
	Super::BeginDestroy();
}

void URpgActionBarViewModel::RegisterMessageListener()
{
	UnregisterMessageListener();

	URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	UWorld* World = ActionBar ? ActionBar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	SlotsChangedHandle = MessageSubsystem.RegisterListener<FRpgActionBarSlotsChangedMessage>(
		RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged,
		this,
		&ThisClass::HandleActionBarSlotsChanged);
}

void URpgActionBarViewModel::UnregisterMessageListener()
{
	if (SlotsChangedHandle.IsValid())
	{
		SlotsChangedHandle.Unregister();
	}
}

void URpgActionBarViewModel::HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message)
{
	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	if (ActionBar && Message.ActionBarComponent == ActionBar)
	{
		RefreshSlots();
	}
}

void URpgWeaponAbilitySlotViewModel::InitializeSlot(int32 InSlotIndex, const FRpgWeaponAbilityLoadoutSlot& InSlot)
{
	const bool bWasChanged =
		SlotIndex != InSlotIndex ||
		AbilityIdTag != InSlot.AbilityIdTag ||
		bAvailable != InSlot.bAvailable;

	SlotIndex = InSlotIndex;
	AbilityIdTag = InSlot.AbilityIdTag;
	bAvailable = InSlot.bAvailable;
	DisplayName = AbilityIdToDisplayText(AbilityIdTag);
	HotkeyActionRowName = InSlotIndex >= 0
		? FName(*FString::Printf(TEXT("UI.WeaponAbility.%d"), InSlotIndex + 1))
		: NAME_None;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AbilityIdTag);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAvailable);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HotkeyActionRowName);

	if (bWasChanged)
	{
		OnSlotChanged.Broadcast(this);
	}
}

void URpgWeaponAbilityLoadoutViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(InPlayerController);
	BindWeaponAbilityLoadout(RpgPlayerController ? RpgPlayerController->GetWeaponAbilityLoadoutComponent() : nullptr);
}

void URpgWeaponAbilityLoadoutViewModel::BindWeaponAbilityLoadout(URpgWeaponAbilityLoadoutComponent* InLoadout)
{
	if (ObservedLoadout.Get() == InLoadout)
	{
		RefreshSlots();
		return;
	}

	UnregisterMessageListener();
	ObservedLoadout = InLoadout;
	RegisterMessageListener();
	RefreshSlots();
}

void URpgWeaponAbilityLoadoutViewModel::UnbindWeaponAbilityLoadout()
{
	UnregisterMessageListener();
	ObservedLoadout.Reset();
	RefreshSlots();
}

void URpgWeaponAbilityLoadoutViewModel::RefreshSlots()
{
	const URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get();
	const TArray<FRpgWeaponAbilityLoadoutSlot> SourceSlots = Loadout ? Loadout->GetSlots() : TArray<FRpgWeaponAbilityLoadoutSlot>();
	const int32 SlotCount = Loadout ? FMath::Max(Loadout->GetNumSlots(), SourceSlots.Num()) : 3;

	TArray<TObjectPtr<URpgWeaponAbilitySlotViewModel>> PreviousSlots = MoveTemp(Slots);
	Slots.Reset();
	Slots.Reserve(SlotCount);

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		URpgWeaponAbilitySlotViewModel* SlotViewModel = PreviousSlots.IsValidIndex(SlotIndex) ? PreviousSlots[SlotIndex].Get() : nullptr;
		if (!SlotViewModel)
		{
			SlotViewModel = NewObject<URpgWeaponAbilitySlotViewModel>(this);
		}

		const FRpgWeaponAbilityLoadoutSlot EmptySlot;
		const FRpgWeaponAbilityLoadoutSlot& SourceSlot = SourceSlots.IsValidIndex(SlotIndex) ? SourceSlots[SlotIndex] : EmptySlot;
		SlotViewModel->InitializeSlot(SlotIndex, SourceSlot);
		Slots.Add(SlotViewModel);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	OnSlotsChanged.Broadcast();
}

TArray<URpgWeaponAbilitySlotViewModel*> URpgWeaponAbilityLoadoutViewModel::GetSlots() const
{
	TArray<URpgWeaponAbilitySlotViewModel*> Result;
	Result.Reserve(Slots.Num());
	for (URpgWeaponAbilitySlotViewModel* Slot : Slots)
	{
		Result.Add(Slot);
	}
	return Result;
}

URpgWeaponAbilitySlotViewModel* URpgWeaponAbilityLoadoutViewModel::GetSlotAtIndex(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

void URpgWeaponAbilityLoadoutViewModel::BeginDestroy()
{
	UnregisterMessageListener();
	Super::BeginDestroy();
}

void URpgWeaponAbilityLoadoutViewModel::RegisterMessageListener()
{
	UnregisterMessageListener();

	URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get();
	UWorld* World = Loadout ? Loadout->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	SlotsChangedHandle = MessageSubsystem.RegisterListener<FRpgWeaponAbilityLoadoutChangedMessage>(
		RpgGameplayTags::Rpg_WeaponAbilityLoadout_Message_SlotsChanged,
		this,
		&ThisClass::HandleWeaponAbilityLoadoutChanged);
}

void URpgWeaponAbilityLoadoutViewModel::UnregisterMessageListener()
{
	if (SlotsChangedHandle.IsValid())
	{
		SlotsChangedHandle.Unregister();
	}
}

void URpgWeaponAbilityLoadoutViewModel::HandleWeaponAbilityLoadoutChanged(FGameplayTag Channel, const FRpgWeaponAbilityLoadoutChangedMessage& Message)
{
	const URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get();
	if (Loadout && Message.LoadoutComponent == Loadout)
	{
		RefreshSlots();
	}
}
