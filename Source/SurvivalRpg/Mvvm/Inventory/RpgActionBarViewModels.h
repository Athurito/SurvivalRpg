#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgActionBarViewModels.generated.h"

class APlayerController;
class UTexture2D;
class URpgAbilitySystemComponent;
struct FRpgInventoryChangeMessage;
struct FRpgPlayerInventoryLayoutChangedMessage;
class URpgInventoryItemInstance;
class URpgInventoryManagerComponent;
class URpgPlayerInventoryLayoutComponent;
class URpgActionBarSlotViewModel;
class URpgWeaponAbilitySlotViewModel;

/** Broadcast when one general actionbar slot view model changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgActionBarSlotViewModelChanged, URpgActionBarSlotViewModel*, SlotViewModel);

/** Broadcast when the general actionbar slot list changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgActionBarViewModelSlotsChanged);

/** Broadcast when one weapon ability slot view model changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgWeaponAbilitySlotViewModelChanged, URpgWeaponAbilitySlotViewModel*, SlotViewModel);

/** Broadcast when the weapon ability slot list changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgWeaponAbilityLoadoutViewModelSlotsChanged);

/** UI projection for one 1..8 general actionbar slot. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgActionBarSlotViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Rebuilds this slot from owner-only replicated actionbar state. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|ViewModel")
	void InitializeSlot(int32 InSlotIndex, const FRpgActionBarSlot& InSlot, URpgInventoryItemInstance* ResolvedItem, int32 InStackCount);

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	ERpgActionBarSlotType GetSlotType() const { return SlotType; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	bool HasContent() const { return bHasContent; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	URpgInventoryItemInstance* GetItemInstance() const { return ItemInstance.Get(); }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	FRpgInventorySlotAddress GetSlotAddress() const { return SlotAddress; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	int32 GetStackCount() const { return StackCount; }

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	FName GetHotkeyActionRowName() const { return HotkeyActionRowName; }

	UPROPERTY(BlueprintAssignable, Category = "Action Bar|ViewModel")
	FRpgActionBarSlotViewModelChanged OnSlotChanged;

protected:
	/** Zero-based slot index used by commands and row-handle lookup. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 SlotIndex = INDEX_NONE;

	/** Whether this slot is empty, bound to a normal inventory slot, or bound to a carry slot. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	ERpgActionBarSlotType SlotType = ERpgActionBarSlotType::Empty;

	/** True when the slot has a source-slot assignment, even if that source slot is currently empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	bool bHasContent = false;

	/** Logical source slot this actionbar entry activates. Empty entries have an invalid address. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FRpgInventorySlotAddress SlotAddress;

	/** Inventory-owned item currently resolved from SlotAddress. Null when the bound source slot is empty. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;

	/** Current stack count for the resolved item, read from the player's inventory when available. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	int32 StackCount = 0;

	/** Optional item icon read from the item currently resolved from SlotAddress. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Short item name or source-slot fallback text for compact slot UI. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FText ShortDisplayName;

	/** CommonUI action row name expected in CDT_RpgUIActions_All for this slot's hotkey glyph. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	FName HotkeyActionRowName;
};

/** UI projection for the owner-only general actionbar component. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgActionBarViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Resolves and observes the general actionbar on an RPG player controller. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|ViewModel")
	void BindPlayerController(APlayerController* InPlayerController);

	/** Starts observing one actionbar component. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|ViewModel")
	void BindActionBar(URpgActionBarComponent* InActionBar, URpgInventoryManagerComponent* InPlayerInventory);

	/** Starts observing one actionbar component and the layout used to resolve its slot-address bindings. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|ViewModel")
	void BindActionBarWithLayout(URpgActionBarComponent* InActionBar, URpgInventoryManagerComponent* InPlayerInventory, URpgPlayerInventoryLayoutComponent* InInventoryLayout);

	/** Stops observing the current actionbar. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|ViewModel")
	void UnbindActionBar();

	/** Rebuilds the slot view models from replicated owner-only actionbar state. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar|ViewModel")
	void RefreshSlots();

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	TArray<URpgActionBarSlotViewModel*> GetSlots() const;

	UFUNCTION(BlueprintPure, Category = "Action Bar|ViewModel")
	URpgActionBarSlotViewModel* GetSlotAtIndex(int32 SlotIndex) const;

	UPROPERTY(BlueprintAssignable, Category = "Action Bar|ViewModel")
	FRpgActionBarViewModelSlotsChanged OnSlotsChanged;

protected:
	virtual void BeginDestroy() override;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgActionBarSlotViewModel>> Slots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action Bar|ViewModel", meta = (AllowPrivateAccess = "true", ClampMin = "1", UIMin = "1"))
	int32 DefaultSlotCount = 8;

private:
	void RegisterMessageListener();
	void UnregisterMessageListener();
	void HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message);
	void HandlePlayerInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandlePlayerInventoryLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message);

	TWeakObjectPtr<URpgActionBarComponent> ObservedActionBar;
	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedPlayerInventory;
	TWeakObjectPtr<URpgPlayerInventoryLayoutComponent> ObservedInventoryLayout;
	FGameplayMessageListenerHandle SlotsChangedHandle;
	FGameplayMessageListenerHandle InventoryChangedHandle;
	FGameplayMessageListenerHandle LayoutChangedHandle;
};

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

/** UI projection for the owner-only Q/E/R weapon ability loadout. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgWeaponAbilityLoadoutViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Resolves and observes the weapon ability loadout on an RPG player controller. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Abilities|ViewModel")
	void BindPlayerController(APlayerController* InPlayerController);

	/** Starts observing one weapon ability loadout component. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Abilities|ViewModel")
	void BindWeaponAbilityLoadout(URpgWeaponAbilityLoadoutComponent* InLoadout);

	/** Starts observing one weapon ability loadout and the owning ASC used for icon/cooldown projection. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Abilities|ViewModel")
	void BindWeaponAbilityLoadoutWithAbilitySystem(URpgWeaponAbilityLoadoutComponent* InLoadout, URpgAbilitySystemComponent* InAbilitySystem);

	/** Stops observing the current weapon ability loadout. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Abilities|ViewModel")
	void UnbindWeaponAbilityLoadout();

	/** Rebuilds the slot view models from replicated owner-only weapon ability state. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Abilities|ViewModel")
	void RefreshSlots();

	/** Refreshes cooldown fields without rebuilding the slot list. */
	UFUNCTION(BlueprintCallable, Category = "Weapon Abilities|ViewModel")
	void RefreshCooldowns();

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	TArray<URpgWeaponAbilitySlotViewModel*> GetSlots() const;

	UFUNCTION(BlueprintPure, Category = "Weapon Abilities|ViewModel")
	URpgWeaponAbilitySlotViewModel* GetSlotAtIndex(int32 SlotIndex) const;

	UPROPERTY(BlueprintAssignable, Category = "Weapon Abilities|ViewModel")
	FRpgWeaponAbilityLoadoutViewModelSlotsChanged OnSlotsChanged;

protected:
	virtual void BeginDestroy() override;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgWeaponAbilitySlotViewModel>> Slots;

	/** Local UI refresh interval for cooldown text/progress. This does not drive gameplay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Abilities|ViewModel", meta = (AllowPrivateAccess = "true", ClampMin = "0.05", UIMin = "0.05", Units = "s"))
	float CooldownRefreshInterval = 0.1f;

private:
	void RegisterMessageListener();
	void UnregisterMessageListener();
	void StartCooldownRefreshTimer();
	void StopCooldownRefreshTimer();
	void HandleWeaponAbilityLoadoutChanged(FGameplayTag Channel, const FRpgWeaponAbilityLoadoutChangedMessage& Message);

	TWeakObjectPtr<URpgWeaponAbilityLoadoutComponent> ObservedLoadout;
	TWeakObjectPtr<URpgAbilitySystemComponent> ObservedAbilitySystem;
	FGameplayMessageListenerHandle SlotsChangedHandle;
	FTimerHandle CooldownRefreshTimerHandle;
};
