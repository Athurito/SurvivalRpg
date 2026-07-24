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
	constexpr ETextIdenticalModeFlags ActionBarTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

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

	template <typename ViewModelType>
	bool AreViewModelArraysEqual(
		const TArray<TObjectPtr<ViewModelType>>& A,
		const TArray<TObjectPtr<ViewModelType>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Get() != B[Index].Get())
			{
				return false;
			}
		}

		return true;
	}
}

void URpgActionBarSlotViewModel::InitializeSlot(int32 InSlotIndex, const FRpgActionBarSlot& InSlot, URpgInventoryItemInstance* ResolvedItem, int32 InStackCount)
{
	InitializeSlotWithAbilitySystem(InSlotIndex, InSlot, ResolvedItem, InStackCount, nullptr, FText::GetEmpty());
}

void URpgActionBarSlotViewModel::InitializeSlotWithAbilitySystem(
	int32 InSlotIndex,
	const FRpgActionBarSlot& InSlot,
	URpgInventoryItemInstance* ResolvedItem,
	int32 InStackCount,
	const URpgAbilitySystemComponent* AbilitySystem,
	FText CarryDisplayName)
{
	const int32 NewSlotIndex = InSlotIndex;
	const ERpgActionBarSlotType NewSlotType = InSlot.SlotType;
	const bool bNewHasContent = !InSlot.IsEmpty();
	const FRpgInventorySlotAddress NewSlotAddress = InSlot.SlotAddress;
	const TObjectPtr<URpgInventoryItemInstance> NewItemInstance =
		ResolvedItem;
	const int32 NewStackCount = ResolvedItem ? InStackCount : 0;
	const FText NewStackCountText = NewStackCount > 1
		? FText::AsNumber(NewStackCount)
		: FText::GetEmpty();
	const bool bNewAvailable = InSlot.bAvailable;
	const ERpgQuickAccessBlockedReason NewBlockedReason =
		InSlot.BlockedReason;
	const FGameplayTag NewAbilityId = InSlot.AbilityId;
	const FGameplayTag NewCarrySemanticRole = InSlot.CarrySemanticRole;
	const FName NewHotkeyActionRowName = InSlotIndex >= 0
		? FName(*FString::Printf(TEXT("UI.ActionBar.Slot.%d"), InSlotIndex + 1))
		: NAME_None;

	FRpgActionSlotPresentation Presentation =
		BuildActionBarItemPresentation(NewItemInstance);
	if (InSlot.SlotType == ERpgActionBarSlotType::Consumable &&
		!NewItemInstance)
	{
		Presentation = BuildActionBarDefinitionPresentation(InSlot.ConsumableDefinition);
	}
	if (InSlot.SlotType == ERpgActionBarSlotType::Ability)
	{
		const FRpgAbilitySlotPresentation AbilityPresentation = BuildAbilityPresentation(InSlot.AbilityId, AbilitySystem);
		Presentation.Icon = AbilityPresentation.Icon;
		Presentation.ShortDisplayName = AbilityPresentation.DisplayName;
	}
	const TSoftObjectPtr<UTexture2D> NewIcon = Presentation.Icon;
	const FText NewShortDisplayName = !Presentation.ShortDisplayName.IsEmpty()
		? Presentation.ShortDisplayName
		: InSlot.SlotType == ERpgActionBarSlotType::CarrySlot
			? (!CarryDisplayName.IsEmpty()
				? CarryDisplayName
				: NSLOCTEXT("RpgActionBar", "CarrySlotFallback", "Carry Slot"))
			: (bNewHasContent
				? FText::FromName(InSlot.AbilityId.GetTagName())
				: FText::GetEmpty());

	const bool bSlotIndexChanged = SlotIndex != NewSlotIndex;
	const bool bSlotTypeChanged = SlotType != NewSlotType;
	const bool bHasContentChanged = bHasContent != bNewHasContent;
	const bool bSlotAddressChanged = SlotAddress != NewSlotAddress;
	const bool bItemInstanceChanged = ItemInstance != NewItemInstance;
	const bool bStackCountChanged = StackCount != NewStackCount;
	const bool bStackCountTextChanged =
		!StackCountText.IdenticalTo(
			NewStackCountText,
			ActionBarTextIdentityFlags);
	const bool bAvailableChanged = bAvailable != bNewAvailable;
	const bool bBlockedReasonChanged = BlockedReason != NewBlockedReason;
	const bool bAbilityIdChanged = AbilityId != NewAbilityId;
	const bool bCarrySemanticRoleChanged =
		CarrySemanticRole != NewCarrySemanticRole;
	const bool bIconChanged = Icon != NewIcon;
	const bool bShortDisplayNameChanged =
		!ShortDisplayName.IdenticalTo(
			NewShortDisplayName,
			ActionBarTextIdentityFlags);
	const bool bHotkeyActionRowNameChanged =
		HotkeyActionRowName != NewHotkeyActionRowName;
	const bool bWasChanged =
		bSlotIndexChanged ||
		bSlotTypeChanged ||
		bHasContentChanged ||
		bSlotAddressChanged ||
		bItemInstanceChanged ||
		bStackCountChanged ||
		bStackCountTextChanged ||
		bAvailableChanged ||
		bBlockedReasonChanged ||
		bAbilityIdChanged ||
		bCarrySemanticRoleChanged ||
		bIconChanged ||
		bShortDisplayNameChanged ||
		bHotkeyActionRowNameChanged;

	SlotIndex = NewSlotIndex;
	SlotType = NewSlotType;
	bHasContent = bNewHasContent;
	SlotAddress = NewSlotAddress;
	ItemInstance = NewItemInstance;
	StackCount = NewStackCount;
	StackCountText = NewStackCountText;
	bAvailable = bNewAvailable;
	BlockedReason = NewBlockedReason;
	AbilityId = NewAbilityId;
	CarrySemanticRole = NewCarrySemanticRole;
	Icon = NewIcon;
	ShortDisplayName = NewShortDisplayName;
	HotkeyActionRowName = NewHotkeyActionRowName;

	if (bSlotIndexChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotIndex);
	}
	if (bSlotTypeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotType);
	}
	if (bHasContentChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bHasContent);
	}
	if (bSlotAddressChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotAddress);
	}
	if (bItemInstanceChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	}
	if (bStackCountChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	}
	if (bStackCountTextChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCountText);
	}
	if (bAvailableChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAvailable);
	}
	if (bBlockedReasonChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(BlockedReason);
	}
	if (bAbilityIdChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AbilityId);
	}
	if (bCarrySemanticRoleChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CarrySemanticRole);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bShortDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	}
	if (bHotkeyActionRowNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HotkeyActionRowName);
	}

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
	CancelQueuedRefreshSlots();

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
		FText CarryDisplayName;
		if (InventoryLayout && SourceSlot.CarrySemanticRole.IsValid())
		{
			FRpgInventorySlotGroupView CarryGroup;
			if (InventoryLayout->TryGetSlotGroupBySemanticRole(
					SourceSlot.CarrySemanticRole,
					CarryGroup) &&
				CarryGroup.GroupKind == ERpgInventorySlotGroupKind::Carry)
			{
				CarryDisplayName = CarryGroup.DisplayName;
			}
		}
		SlotViewModel->InitializeSlotWithAbilitySystem(
			SlotIndex,
			SourceSlot,
			ResolvedItem,
			StackCount,
			AbilitySystem,
			CarryDisplayName);
		Slots.Add(SlotViewModel);
	}

	if (!AreViewModelArraysEqual(PreviousSlots, Slots))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	}
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
	CancelQueuedRefreshSlots();
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

void URpgActionBarViewModel::RequestRefreshSlots()
{
	UWorld* World = nullptr;
	if (URpgActionBarComponent* ActionBar = ObservedActionBar.Get())
	{
		World = ActionBar->GetWorld();
	}
	else if (URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get())
	{
		World = PlayerInventory->GetWorld();
	}
	else if (URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get())
	{
		World = InventoryLayout->GetWorld();
	}

	if (!World)
	{
		RefreshSlots();
		return;
	}

	RefreshSlotsQueue.Queue(
		World,
		this,
		&ThisClass::ExecuteQueuedRefreshSlots);
}

void URpgActionBarViewModel::ExecuteQueuedRefreshSlots()
{
	if (!RefreshSlotsQueue.Consume())
	{
		return;
	}

	RefreshSlots();
}

void URpgActionBarViewModel::CancelQueuedRefreshSlots()
{
	RefreshSlotsQueue.Cancel();
}

void URpgActionBarViewModel::HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message)
{
	const URpgActionBarComponent* ActionBar = ObservedActionBar.Get();
	if (ActionBar && Message.ActionBarComponent == ActionBar)
	{
		RequestRefreshSlots();
	}
}

void URpgActionBarViewModel::HandlePlayerInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const URpgInventoryManagerComponent* PlayerInventory = ObservedPlayerInventory.Get();
	if (PlayerInventory && Message.InventoryOwner == PlayerInventory)
	{
		RequestRefreshSlots();
	}
}

void URpgActionBarViewModel::HandlePlayerInventoryLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message)
{
	const URpgPlayerInventoryLayoutComponent* InventoryLayout = ObservedInventoryLayout.Get();
	if (InventoryLayout && Message.LayoutComponent == InventoryLayout)
	{
		RequestRefreshSlots();
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
	const int32 NewSlotIndex = InSlotIndex;
	const FGameplayTag NewAbilityIdTag = InSlot.AbilityIdTag;
	const bool bNewAvailable = InSlot.bAvailable;
	const FRpgAbilitySlotPresentation Presentation =
		BuildAbilityPresentation(NewAbilityIdTag, InAbilitySystem);
	const FText NewDisplayName = Presentation.DisplayName;
	const FText NewDescription = Presentation.Description;
	const TSoftObjectPtr<UTexture2D> NewIcon = Presentation.Icon;
	const FName NewHotkeyActionRowName = InSlotIndex >= 0
		? FName(*FString::Printf(TEXT("UI.WeaponAbility.%d"), InSlotIndex + 1))
		: NAME_None;

	const bool bSlotIndexChanged = SlotIndex != NewSlotIndex;
	const bool bAbilityIdTagChanged = AbilityIdTag != NewAbilityIdTag;
	const bool bAvailableChanged = bAvailable != bNewAvailable;
	const bool bDisplayNameChanged =
		!DisplayName.IdenticalTo(NewDisplayName, ActionBarTextIdentityFlags);
	const bool bDescriptionChanged =
		!Description.IdenticalTo(NewDescription, ActionBarTextIdentityFlags);
	const bool bIconChanged = Icon != NewIcon;
	const bool bHotkeyActionRowNameChanged =
		HotkeyActionRowName != NewHotkeyActionRowName;
	const bool bWasChanged =
		bSlotIndexChanged ||
		bAbilityIdTagChanged ||
		bAvailableChanged ||
		bDisplayNameChanged ||
		bDescriptionChanged ||
		bIconChanged ||
		bHotkeyActionRowNameChanged;

	SlotIndex = NewSlotIndex;
	AbilityIdTag = NewAbilityIdTag;
	bAvailable = bNewAvailable;
	DisplayName = NewDisplayName;
	Description = NewDescription;
	Icon = NewIcon;
	HotkeyActionRowName = NewHotkeyActionRowName;

	RefreshCooldown(InAbilitySystem);

	if (bSlotIndexChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SlotIndex);
	}
	if (bAbilityIdTagChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(AbilityIdTag);
	}
	if (bAvailableChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bAvailable);
	}
	if (bDisplayNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	}
	if (bDescriptionChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	}
	if (bIconChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	}
	if (bHotkeyActionRowNameChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(HotkeyActionRowName);
	}

	if (bWasChanged)
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

	const float PublishedCooldownRemainingTime =
		bNewOnCooldown ? NewCooldownRemainingTime : 0.0f;
	const float PublishedCooldownDuration =
		bNewOnCooldown ? NewCooldownDuration : 0.0f;
	const bool bOnCooldownChanged = bOnCooldown != bNewOnCooldown;
	const bool bCooldownRemainingTimeChanged =
		CooldownRemainingTime != PublishedCooldownRemainingTime;
	const bool bCooldownDurationChanged =
		CooldownDuration != PublishedCooldownDuration;
	const bool bCooldownPercentChanged =
		CooldownPercent != NewCooldownPercent;
	const bool bCooldownTextChanged =
		!CooldownText.IdenticalTo(NewCooldownText, ActionBarTextIdentityFlags);

	bOnCooldown = bNewOnCooldown;
	CooldownRemainingTime = PublishedCooldownRemainingTime;
	CooldownDuration = PublishedCooldownDuration;
	CooldownPercent = NewCooldownPercent;
	CooldownText = NewCooldownText;

	if (bOnCooldownChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bOnCooldown);
	}
	if (bCooldownRemainingTimeChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CooldownRemainingTime);
	}
	if (bCooldownDurationChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CooldownDuration);
	}
	if (bCooldownPercentChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CooldownPercent);
	}
	if (bCooldownTextChanged)
	{
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

	if (!AreViewModelArraysEqual(PreviousSlots, Slots))
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Slots);
	}
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
