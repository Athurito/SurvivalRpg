#include "RpgWeaponAbilitySlotViewModel.h"

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWeaponAbilitySlotViewModel)

namespace
{
	constexpr ETextIdenticalModeFlags WeaponAbilitySlotTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	FText WeaponAbilityIdToDisplayText(FGameplayTag AbilityIdTag)
	{
		return AbilityIdTag.IsValid()
			? FText::FromName(AbilityIdTag.GetTagName())
			: FText::GetEmpty();
	}

	struct FRpgWeaponAbilitySlotPresentation
	{
		TSoftObjectPtr<UTexture2D> Icon;
		FText DisplayName;
		FText Description;
	};

	FRpgWeaponAbilitySlotPresentation BuildWeaponAbilityPresentation(
		FGameplayTag AbilityIdTag,
		const URpgAbilitySystemComponent* AbilitySystem)
	{
		FRpgWeaponAbilitySlotPresentation Presentation;
		Presentation.DisplayName = WeaponAbilityIdToDisplayText(AbilityIdTag);

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
	const FRpgWeaponAbilitySlotPresentation Presentation =
		BuildWeaponAbilityPresentation(NewAbilityIdTag, InAbilitySystem);
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
		!DisplayName.IdenticalTo(NewDisplayName, WeaponAbilitySlotTextIdentityFlags);
	const bool bDescriptionChanged =
		!Description.IdenticalTo(NewDescription, WeaponAbilitySlotTextIdentityFlags);
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
		!CooldownText.IdenticalTo(NewCooldownText, WeaponAbilitySlotTextIdentityFlags);

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
