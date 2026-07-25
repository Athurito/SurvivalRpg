#include "RpgActionBarSlotViewModel.h"

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgActionBarSlotViewModel)

namespace
{
	constexpr ETextIdenticalModeFlags ActionBarSlotTextIdentityFlags =
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

	FText ActionBarAbilityIdToDisplayText(FGameplayTag AbilityIdTag)
	{
		return AbilityIdTag.IsValid()
			? FText::FromName(AbilityIdTag.GetTagName())
			: FText::GetEmpty();
	}

	struct FRpgActionBarAbilitySlotPresentation
	{
		TSoftObjectPtr<UTexture2D> Icon;
		FText DisplayName;
		FText Description;
	};

	FRpgActionBarAbilitySlotPresentation BuildActionBarAbilityPresentation(
		FGameplayTag AbilityIdTag,
		const URpgAbilitySystemComponent* AbilitySystem)
	{
		FRpgActionBarAbilitySlotPresentation Presentation;
		Presentation.DisplayName = ActionBarAbilityIdToDisplayText(AbilityIdTag);

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
		const FRpgActionBarAbilitySlotPresentation AbilityPresentation =
			BuildActionBarAbilityPresentation(InSlot.AbilityId, AbilitySystem);
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
			ActionBarSlotTextIdentityFlags);
	const bool bAvailableChanged = bAvailable != bNewAvailable;
	const bool bBlockedReasonChanged = BlockedReason != NewBlockedReason;
	const bool bAbilityIdChanged = AbilityId != NewAbilityId;
	const bool bCarrySemanticRoleChanged =
		CarrySemanticRole != NewCarrySemanticRole;
	const bool bIconChanged = Icon != NewIcon;
	const bool bShortDisplayNameChanged =
		!ShortDisplayName.IdenticalTo(
			NewShortDisplayName,
			ActionBarSlotTextIdentityFlags);
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
