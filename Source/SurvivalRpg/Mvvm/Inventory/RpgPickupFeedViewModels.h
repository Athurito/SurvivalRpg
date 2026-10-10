#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "MVVMViewModelBase.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgPickupFeedViewModels.generated.h"

class APlayerController;
class UTexture2D;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;
struct FRpgInventoryChangeMessage;
struct FRpgInventoryScreenActivationMessage;

/** One HUD notification for items gained from the world, such as "12 × Wood". */
UCLASS(BlueprintType, meta = (MVVMAllowedContextCreationType = "Manual"))
class SURVIVALRPG_API URpgPickupFeedEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Starts the notification with Count units gained at Now, in world seconds. */
	void InitializeEntry(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InCount, double Now);

	/** Adds further units gained at Now and keeps the notification alive. */
	void AddCount(int32 Extra, double Now);

	/** Marks the notification as fading out before removal. */
	void SetExpiring(bool bInExpiring);

	TSubclassOf<URpgInventoryItemDefinition> GetItemDefinition() const { return ItemDefinition; }
	int32 GetCount() const { return Count; }
	double GetLastGainTime() const { return LastGainTime; }
	bool IsExpiring() const { return bExpiring; }

protected:
	/** Item name from the definition. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Pickups", meta = (AllowPrivateAccess = "true"))
	FText DisplayName;

	/** Item icon from UIData. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Pickups", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> Icon;

	/** Units gained since the notification appeared. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Pickups", meta = (AllowPrivateAccess = "true"))
	int32 Count = 0;

	/** "12 × Wood". */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Pickups", meta = (AllowPrivateAccess = "true"))
	FText LabelText;

	/** True while the notification fades out; widgets unpin their fade box. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Pickups", meta = (AllowPrivateAccess = "true"))
	bool bExpiring = false;

private:
	void RefreshLabel();

	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
	double LastGainTime = 0.0;
};

/**
 * HUD read model of items the local player gains from the world: pickups, harvest yields and recovered loot.
 *
 * It nets the replicated stack changes of the player inventory per item definition, so moves inside the inventory
 * cancel out. Changes while an inventory, storage or crafting screen is open, changes restored from a save, and the
 * burst while the inventory first replicates are not announced. Gains of the same item merge into one notification
 * while it is shown. Read-only; it never changes inventory state. Owned per local player by URpgUiSubsystem.
 */
UCLASS(BlueprintType, DisplayName = "Pickup Feed")
class SURVIVALRPG_API URpgPickupFeedViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Starts observing the inventory messages of this local player's world. */
	void BindPlayerController(APlayerController* InPlayerController);

	/** Stops observing and clears all notifications. */
	void Unbind();

	/** Announces Count gained units of ItemDefinition, merging into a shown notification of the same item. */
	void AddGain(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 GainedCount);

	/** Turns netted pending stack changes into notifications now instead of after the settle delay. */
	void FlushPendingGains();

	/** Fades notifications past their hold time and removes faded ones. Driven by a timer while notifications exist. */
	void UpdateExpiry(double Now);

	const TArray<TObjectPtr<URpgPickupFeedEntryViewModel>>& GetEntries() const { return Entries; }

protected:
	/** Shown notifications, oldest first. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Pickups", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<URpgPickupFeedEntryViewModel>> Entries;

	/** True while at least one notification is shown. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "HUD|Pickups", meta = (AllowPrivateAccess = "true"))
	bool bHasEntries = false;

private:
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandleScreenActivation(FGameplayTag Channel, const FRpgInventoryScreenActivationMessage& Message);
	void HandleExpiryTimer();
	URpgInventoryManagerComponent* FindPlayerInventory() const;
	bool IsSuppressed(double Now) const;
	double GetNow() const;
	UWorld* GetFeedWorld() const;
	void SetEntries(TArray<TObjectPtr<URpgPickupFeedEntryViewModel>>&& NewEntries);
	void UpdateExpiryTimer();

	TWeakObjectPtr<APlayerController> PlayerController;

	/** Player inventory the warm-up window belongs to. */
	TWeakObjectPtr<URpgInventoryManagerComponent> ObservedInventory;
	double ObserveStartTime = 0.0;

	int32 OpenScreenCount = 0;
	double LastScreenCloseTime = -1.0e9;

	/** Net stack change per item definition since the last flush. */
	TMap<TSubclassOf<URpgInventoryItemDefinition>, int32> PendingDeltas;

	FGameplayMessageListenerHandle InventoryListenerHandle;
	FGameplayMessageListenerHandle ScreenListenerHandle;
	FTimerHandle FlushTimerHandle;
	FTimerHandle ExpiryTimerHandle;
};
