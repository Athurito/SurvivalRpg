#include "RpgActionBarViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarViewModels)

namespace
{
	struct FRpgActionSlotPresentation
	{
		TSoftObjectPtr<UTexture2D> Icon;
		FText ShortDisplayName;
	};

	FRpgActionSlotPresentation BuildActionBarDefinitionPresentation(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		FRpgActionSlotPresentation Presentation;
		const URpgInventoryItemDefinition* ItemCDO = ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
		if (!ItemCDO)
		{
			return Presentation;
		}

		Presentation.ShortDisplayName = ItemCDO->DisplayName;
		if (const URpgInventoryFragment_UIData* UIData = Cast<URpgInventoryFragment_UIData>(
			ItemCDO->FindFragmentByClass(URpgInventoryFragment_UIData::StaticClass())))
		{
			Presentation.Icon = UIData->Icon;
			Presentation.ShortDisplayName = UIData->ShortDisplayName.IsEmpty() ? ItemCDO->DisplayName : UIData->ShortDisplayName;
		}
		return Presentation;
	}

	FRpgActionSlotPresentation BuildActionBarItemPresentation(const URpgInventoryItemInstance* ItemInstance)
	{
		FRpgActionSlotPresentation Presentation;
		if (!ItemInstance)
		{
			return Presentation;
		}

		return BuildActionBarDefinitionPresentation(ItemInstance->GetItemDef());
	}

	FText AbilityIdToDisplayText(FGameplayTag AbilityIdTag)
	{
		return AbilityIdTag.IsValid()
			? FText::FromName(AbilityIdTag.GetTagName())
			: FText::GetEmpty();
	}

	struct FRpgAbilitySlotPresentation
	{
		TSoftObjectPtr<UTexture2D> Icon;
		FText DisplayName;
		FText Description;
	};

	FRpgAbilitySlotPresentation BuildAbilityPresentation(FGameplayTag AbilityIdTag, const URpgAbilitySystemComponent* AbilitySystem)
	{
		FRpgAbilitySlotPresentation Presentation;
		Presentation.DisplayName = AbilityIdToDisplayText(AbilityIdTag);

		const URpgGameplayAbility* AbilityCDO = AbilitySystem ? AbilitySystem->FindAbilityCDOByAbilityId(AbilityIdTag) : nullptr;
		if (!AbilityCDO)
		{
			return Presentation;
		}

		const FText AbilityDisplayName = AbilityCDO->GetAbilityDisplayName();
		Presentation.DisplayName = AbilityDisplayName.IsEmpty() ? Presentation.DisplayName : AbilityDisplayName;
		Presentation.Description = AbilityCDO->GetAbilityDescription();
		Presentation.Icon = AbilityCDO->GetAbilityIcon();
		return Presentation;
	}

	FText BuildCooldownText(float RemainingSeconds)
	{
		if (RemainingSeconds <= 0.0f)
		{
			return FText::GetEmpty();
		}

		FNumberFormattingOptions NumberFormatting;
		if (RemainingSeconds < 10.0f)
		{
			NumberFormatting.MinimumFractionalDigits = 1;
			NumberFormatting.MaximumFractionalDigits = 1;
			return FText::AsNumber(FMath::Max(0.1f, RemainingSeconds), &NumberFormatting);
		}

		NumberFormatting.MinimumFractionalDigits = 0;
		NumberFormatting.MaximumFractionalDigits = 0;
		return FText::AsNumber(FMath::CeilToInt(RemainingSeconds), &NumberFormatting);
	}
}

void URpgActionBarSlotViewModel::InitializeSlot(int32 InSlotIndex, const FRpgActionBarSlot& InSlot, URpgInventoryItemInstance* ResolvedItem, int32 InStackCount)
{
	InitializeSlotWithAbilitySystem(InSlotIndex, InSlot, ResolvedItem, InStackCount, nullptr);
}

void URpgActionBarSlotViewModel::InitializeSlotWithAbilitySystem(
	int32 InSlotIndex,
	const FRpgActionBarSlot& InSlot,
	URpgInventoryItemInstance* ResolvedItem,
	int32 InStackCount,
	const URpgAbilitySystemComponent* AbilitySystem)
{
	const bool bWasChanged =
		SlotIndex != InSlotIndex ||
		SlotType != InSlot.SlotType ||
		SlotAddress != InSlot.SlotAddress ||
		ItemInstance != ResolvedItem ||
		StackCount != InStackCount ||
		bAvailable != InSlot.bAvailable ||
		BlockedReason != InSlot.BlockedReason ||
		AbilityId != InSlot.AbilityId;

	SlotIndex = InSlotIndex;
	SlotType = InSlot.SlotType;
	bHasContent = !InSlot.IsEmpty();
	SlotAddress = InSlot.SlotAddress;
	ItemInstance = ResolvedItem;
	StackCount = ResolvedItem ? InStackCount : 0;
	StackCountText = StackCount > 1
		? FText::AsNumber(StackCount)
		: FText::GetEmpty();
	bAvailable = InSlot.bAvailable;
	BlockedReason = InSlot.BlockedReason;
	AbilityId = InSlot.AbilityId;
	HotkeyActionRowName = InSlotIndex >= 0
		? FName(*FString::Printf(TEXT("UI.ActionBar.Slot.%d"), InSlotIndex + 1))
		: NAME_None;

	FRpgActionSlotPresentation Presentation = BuildActionBarItemPresentation(ItemInstance);
	if (InSlot.SlotType == ERpgActionBarSlotType::Consumable && !ItemInstance)
	{
		Presentation = BuildActionBarDefinitionPresentation(InSlot.ConsumableDefinition);
	}
	if (InSlot.SlotType == ERpgActionBarSlotType::Ability)
	{
		const FRpgAbilitySlotPresentation AbilityPresentation = BuildAbilityPresentation(InSlot.AbilityId, AbilitySystem);
		Presentation.Icon = AbilityPresentation.Icon;
		Presentation.ShortDisplayName = AbilityPresentation.DisplayName;
	}
	Icon = Presentation.Icon;
	ShortDisplayName = !Presentation.ShortDisplayName.IsEmpty()
		? Presentation.ShortDisplayName
		: InSlot.SlotType == ERpgActionBarSlotType::CarrySlot
			? FText::FromName(InSlot.CarryRole)
			: (bHasContent ? FText::FromName(InSlot.AbilityId.GetTagName()) : FText::GetEmpty());

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotType);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasContent);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotAddress);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCountText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAvailable);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BlockedReason);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AbilityId);
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
	BindActionBarWithLayout(
		RpgPlayerController ? RpgPlayerController->GetActionBarComponent() : nullptr,
		RpgPlayerState ? RpgPlayerState->GetInventoryManagerComponent() : nullptr,
		RpgPlayerController ? RpgPlayerController->GetPlayerInventoryLayoutComponent() : nullptr);
}

void URpgActionBarViewModel::BindActionBar(URpgActionBarComponent* InActionBar, URpgInventoryManagerComponent* InPlayerInventory)
{
	BindActionBarWithLayout(InActionBar, InPlayerInventory, nullptr);
}

void URpgActionBarViewModel::BindActionBarWithLayout(URpgActionBarComponent* InActionBar, URpgInventoryManagerComponent* InPlayerInventory, URpgPlayerInventoryLayoutComponent* InInventoryLayout)
{
	if (ObservedActionBar.Get() == InActionBar && ObservedPlayerInventory.Get() == InPlayerInventory && ObservedInventoryLayout.Get() == InInventoryLayout)
	{
		RefreshSlots();
		return;
	}

	UnregisterMessageListener();
	ObservedActionBar = InActionBar;
	ObservedPlayerInventory = InPlayerInventory;
	ObservedInventoryLayout = InInventoryLayout;
	RegisterMessageListener();
	RefreshSlots();
}

void URpgActionBarViewModel::UnbindActionBar()
{
	UnregisterMessageListener();
	ObservedActionBar.Reset();
	ObservedPlayerInventory.Reset();
	ObservedInventoryLayout.Reset();
	RefreshSlots();
}

void URpgActionBarViewModel::RefreshSlots()
{
	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();
	const ARpgPlayerController* RpgPlayerController = ActionBar ? Cast<ARpgPlayerController>(ActionBar->GetOwner()) : nullptr;
	const URpgAbilitySystemComponent* AbilitySystem = RpgPlayerController ? RpgPlayerController->GetRpgAbilitySystemComponent() : nullptr;
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
		URpgInventoryItemInstance* ResolvedItem = InventoryLayout ? InventoryLayout->GetItemInSlotAddress(SourceSlot.SlotAddress) : nullptr;
		const int32 StackCount = (PlayerInventory && ResolvedItem)
			? PlayerInventory->GetItemStackCount(ResolvedItem)
			: 0;
		SlotViewModel->InitializeSlotWithAbilitySystem(SlotIndex, SourceSlot, ResolvedItem, StackCount, AbilitySystem);
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

	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandlePlayerInventoryChanged);

	LayoutChangedHandle = MessageSubsystem.RegisterListener<FRpgPlayerInventoryLayoutChangedMessage>(
		RpgGameplayTags::Rpg_InventoryLayout_Message_Changed,
		this,
		&ThisClass::HandlePlayerInventoryLayoutChanged);
}

void URpgActionBarViewModel::UnregisterMessageListener()
{
	if (SlotsChangedHandle.IsValid())
	{
		SlotsChangedHandle.Unregister();
	}

	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}

	if (LayoutChangedHandle.IsValid())
	{
		LayoutChangedHandle.Unregister();
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

void URpgActionBarViewModel::HandlePlayerInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	if (PlayerInventory && Message.InventoryOwner == PlayerInventory)
	{
		RefreshSlots();
	}
}

void URpgActionBarViewModel::HandlePlayerInventoryLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();
	if (InventoryLayout && Message.LayoutComponent == InventoryLayout)
	{
		RefreshSlots();
	}
}

void URpgWeaponAbilitySlotViewModel::InitializeSlot(int32 InSlotIndex, const FRpgWeaponAbilityLoadoutSlot& InSlot)
{
	InitializeSlotWithAbilitySystem(InSlotIndex, InSlot, nullptr);
}

void URpgWeaponAbilitySlotViewModel::InitializeSlotWithAbilitySystem(
	int32 InSlotIndex,
	const FRpgWeaponAbilityLoadoutSlot& InSlot,
	const URpgAbilitySystemComponent* InAbilitySystem)
{
	const bool bWasChanged =
		SlotIndex != InSlotIndex ||
		AbilityIdTag != InSlot.AbilityIdTag ||
		bAvailable != InSlot.bAvailable;
	const FRpgAbilitySlotPresentation Presentation = BuildAbilityPresentation(InSlot.AbilityIdTag, InAbilitySystem);
	const bool bPresentationChanged =
		!DisplayName.EqualTo(Presentation.DisplayName) ||
		!Description.EqualTo(Presentation.Description) ||
		Icon != Presentation.Icon;

	SlotIndex = InSlotIndex;
	AbilityIdTag = InSlot.AbilityIdTag;
	bAvailable = InSlot.bAvailable;
	DisplayName = Presentation.DisplayName;
	Description = Presentation.Description;
	Icon = Presentation.Icon;
	HotkeyActionRowName = InSlotIndex >= 0
		? FName(*FString::Printf(TEXT("UI.WeaponAbility.%d"), InSlotIndex + 1))
		: NAME_None;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AbilityIdTag);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAvailable);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HotkeyActionRowName);

	RefreshCooldown(InAbilitySystem);

	if (bWasChanged || bPresentationChanged)
	{
		OnSlotChanged.Broadcast(this);
	}
}

void URpgWeaponAbilitySlotViewModel::RefreshCooldown(const URpgAbilitySystemComponent* InAbilitySystem)
{
	float NewCooldownRemainingTime = 0.0f;
	float NewCooldownDuration = 0.0f;
	const bool bNewOnCooldown = bAvailable && AbilityIdTag.IsValid() && InAbilitySystem
		&& InAbilitySystem->GetCooldownTimeRemainingAndDurationForAbilityId(
			AbilityIdTag,
			NewCooldownRemainingTime,
			NewCooldownDuration);
	const float NewCooldownPercent = bNewOnCooldown && NewCooldownDuration > 0.0f
		? FMath::Clamp(NewCooldownRemainingTime / NewCooldownDuration, 0.0f, 1.0f)
		: 0.0f;
	const FText NewCooldownText = bNewOnCooldown ? BuildCooldownText(NewCooldownRemainingTime) : FText::GetEmpty();

	const bool bCooldownChanged =
		bOnCooldown != bNewOnCooldown ||
		!FMath::IsNearlyEqual(CooldownRemainingTime, NewCooldownRemainingTime, 0.01f) ||
		!FMath::IsNearlyEqual(CooldownDuration, NewCooldownDuration, 0.01f) ||
		!FMath::IsNearlyEqual(CooldownPercent, NewCooldownPercent, 0.001f) ||
		!CooldownText.EqualTo(NewCooldownText);

	bOnCooldown = bNewOnCooldown;
	CooldownRemainingTime = bNewOnCooldown ? NewCooldownRemainingTime : 0.0f;
	CooldownDuration = bNewOnCooldown ? NewCooldownDuration : 0.0f;
	CooldownPercent = NewCooldownPercent;
	CooldownText = NewCooldownText;

	if (bCooldownChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bOnCooldown);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CooldownRemainingTime);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CooldownDuration);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CooldownPercent);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CooldownText);
	}
}

void URpgWeaponAbilityLoadoutViewModel::BindPlayerController(APlayerController* InPlayerController)
{
	const ARpgPlayerController* RpgPlayerController = Cast<ARpgPlayerController>(InPlayerController);
	BindWeaponAbilityLoadoutWithAbilitySystem(
		RpgPlayerController ? RpgPlayerController->GetWeaponAbilityLoadoutComponent() : nullptr,
		RpgPlayerController ? RpgPlayerController->GetRpgAbilitySystemComponent() : nullptr);
}

void URpgWeaponAbilityLoadoutViewModel::BindWeaponAbilityLoadout(URpgWeaponAbilityLoadoutComponent* InLoadout)
{
	BindWeaponAbilityLoadoutWithAbilitySystem(InLoadout, nullptr);
}

void URpgWeaponAbilityLoadoutViewModel::BindWeaponAbilityLoadoutWithAbilitySystem(
	URpgWeaponAbilityLoadoutComponent* InLoadout,
	URpgAbilitySystemComponent* InAbilitySystem)
{
	if (ObservedLoadout.Get() == InLoadout && ObservedAbilitySystem.Get() == InAbilitySystem)
	{
		RefreshSlots();
		return;
	}

	StopCooldownRefreshTimer();
	UnregisterMessageListener();
	ObservedLoadout = InLoadout;
	ObservedAbilitySystem = InAbilitySystem;
	RegisterMessageListener();
	RefreshSlots();
	StartCooldownRefreshTimer();
}

void URpgWeaponAbilityLoadoutViewModel::UnbindWeaponAbilityLoadout()
{
	StopCooldownRefreshTimer();
	UnregisterMessageListener();
	ObservedLoadout.Reset();
	ObservedAbilitySystem.Reset();
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
		SlotViewModel->InitializeSlotWithAbilitySystem(SlotIndex, SourceSlot, ObservedAbilitySystem.Get());
		Slots.Add(SlotViewModel);
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	OnSlotsChanged.Broadcast();
}

void URpgWeaponAbilityLoadoutViewModel::RefreshCooldowns()
{
	const URpgAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get();
	for (URpgWeaponAbilitySlotViewModel* Slot : Slots)
	{
		if (Slot)
		{
			Slot->RefreshCooldown(AbilitySystem);
		}
	}
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
	StopCooldownRefreshTimer();
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

void URpgWeaponAbilityLoadoutViewModel::StartCooldownRefreshTimer()
{
	StopCooldownRefreshTimer();

	UWorld* World = nullptr;
	if (URpgAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get())
	{
		World = AbilitySystem->GetWorld();
	}
	else if (URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get())
	{
		World = Loadout->GetWorld();
	}

	if (!World || CooldownRefreshInterval <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CooldownRefreshTimerHandle,
		this,
		&ThisClass::RefreshCooldowns,
		CooldownRefreshInterval,
		true);
}

void URpgWeaponAbilityLoadoutViewModel::StopCooldownRefreshTimer()
{
	UWorld* World = nullptr;
	if (URpgAbilitySystemComponent* AbilitySystem = ObservedAbilitySystem.Get())
	{
		World = AbilitySystem->GetWorld();
	}
	else if (URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get())
	{
		World = Loadout->GetWorld();
	}

	if (World)
	{
		World->GetTimerManager().ClearTimer(CooldownRefreshTimerHandle);
	}
	CooldownRefreshTimerHandle.Invalidate();
}

void URpgWeaponAbilityLoadoutViewModel::HandleWeaponAbilityLoadoutChanged(FGameplayTag Channel, const FRpgWeaponAbilityLoadoutChangedMessage& Message)
{
	const URpgWeaponAbilityLoadoutComponent* Loadout = ObservedLoadout.Get();
	if (Loadout && Message.LoadoutComponent == Loadout)
	{
		RefreshSlots();
	}
}
