#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "RpgActionBarSlotViewModel.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Mvvm/RpgViewModelInvalidationQueue.h"

#include "RpgActionBarViewModel.generated.h"

class APlayerController;
class URpgInventoryManagerComponent;
class URpgPlayerInventoryLayoutComponent;
struct FRpgInventoryChangeMessage;
struct FRpgPlayerInventoryLayoutChangedMessage;

/** Broadcast when the general actionbar slot list changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgActionBarViewModelSlotsChanged);

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
	void RequestRefreshSlots();
	void ExecuteQueuedRefreshSlots();
	void CancelQueuedRefreshSlots();
	void HandleActionBarSlotsChanged(FGameplayTag Channel, const FRpgActionBarSlotsChangedMessage& Message);
	void HandlePlayerInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandlePlayerInventoryLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message);

	TWeakObjectPtr<URpgActionBarComponent> ObservedActionBar;
	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedPlayerInventory;
	TWeakObjectPtr<URpgPlayerInventoryLayoutComponent> ObservedInventoryLayout;
	FGameplayMessageListenerHandle SlotsChangedHandle;
	FGameplayMessageListenerHandle InventoryChangedHandle;
	FGameplayMessageListenerHandle LayoutChangedHandle;
	FRpgViewModelInvalidationQueue RefreshSlotsQueue;
};
