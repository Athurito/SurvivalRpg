#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "RpgWeaponAbilitySlotViewModel.h"
#include "SurvivalRpg/Equipment/RpgWeaponAbilityLoadoutComponent.h"
#include "TimerManager.h"

#include "RpgWeaponAbilityLoadoutViewModel.generated.h"

class APlayerController;
class URpgAbilitySystemComponent;

/** Broadcast when the weapon ability slot list changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgWeaponAbilityLoadoutViewModelSlotsChanged);

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
