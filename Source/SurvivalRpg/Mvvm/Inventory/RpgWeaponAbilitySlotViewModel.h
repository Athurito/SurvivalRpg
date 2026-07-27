#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgWeaponAbilitySlotViewModel.generated.h"

class UTexture2D;
class URpgAbilitySystemComponent;
class URpgWeaponAbilitySlotViewModel;

/** Broadcast when one weapon ability slot view model changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgWeaponAbilitySlotViewModelChanged, URpgWeaponAbilitySlotViewModel*, SlotViewModel);

/** UI projection for one Q/E/R weapon ability slot. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgWeaponAbilitySlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this slot from owner-only replicated weapon ability state. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Abilities|ViewModel")
	void InitializeSlot(int32 InSlotIndex, const FRpgWeaponAbilityLoadoutSlot& InSlot);

	/** Rebuilds this slot and reads static presentation/cooldown state from the owning player's ASC. */
	void InitializeSlotWithAbilitySystem(int32 InSlotIndex, const FRpgWeaponAbilityLoadoutSlot& InSlot, const URpgAbilitySystemComponent* InAbilitySystem);

	/** Refreshes only cooldown-related fields; safe for a lightweight UI timer. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Abilities|ViewModel")
	void RefreshCooldown(const URpgAbilitySystemComponent* InAbilitySystem);

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	FGameplayTag GetAbilityIdTag() const { return AbilityIdTag; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	bool IsAvailable() const { return bAvailable; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	FName GetHotkeyActionRowName() const { return HotkeyActionRowName; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	FText GetDescription() const { return Description; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	bool IsOnCooldown() const { return bOnCooldown; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	float GetCooldownRemainingTime() const { return CooldownRemainingTime; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	float GetCooldownDuration() const { return CooldownDuration; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	float GetCooldownPercent() const { return CooldownPercent; }

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	FText GetCooldownText() const { return CooldownText; }

	UPROPERTY(BlueprintAssignable, Category = "Weapon Abilities|ViewModel")
	FRpgWeaponAbilitySlotViewModelChanged OnSlotChanged;

protected:
	/** Zero-based slot index: 0=Q, 1=E, 2=R by default input setup. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 SlotIndex = INDEX_NONE;

	/** Selected semantic ability id for this slot. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	FGameplayTag AbilityIdTag;

	/** True when the ability is currently granted and bound to the slot input tag. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bAvailable = false;

	/** Compact display text from the ability CDO, falling back to the ability id tag. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Optional ability description for details/tooltips. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText Description;

	/** Soft ability icon read from the granted ability CDO. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** True while GAS reports an active cooldown effect for this ability. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bOnCooldown = false;

	/** Remaining cooldown seconds. UI-read-only and refreshed locally. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	float CooldownRemainingTime = 0.0f;

	/** Total cooldown duration seconds reported by the active cooldown GameplayEffect. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	float CooldownDuration = 0.0f;

	/** Remaining cooldown fraction in range 0..1, useful for overlay or progress materials. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float CooldownPercent = 0.0f;

	/** Short remaining-time text, empty when no cooldown is active. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText CooldownText;

	/** CommonUI action row name expected in CDT_RpgUIActions_All for this slot's hotkey glyph. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	FName HotkeyActionRowName;
};
