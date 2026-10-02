#pragma once

#include "MVVMViewModelBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Inventory/RpgPhysicalStorageTypes.h"
#include "RpgPhysicalStorageViewModel.generated.h"

class URpgInventoryContainerComponent;
struct FRpgInventoryChangeMessage;

/** Read-only projection of one shared chest. Gameplay mutations remain controller requests. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgPhysicalStorageViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	/** Observes replicated metadata and actual inventory capacity; rebinding releases both previous listeners. */
	void BindContainer(URpgInventoryContainerComponent* Container);
	/** Releases the observation when its CommonUI screen deactivates. */
	void UnbindContainer();
	/** Refreshes confirmed gameplay state without predicting assignment order or capacity. */
	void Refresh();
	/** Owner-local command progress. This never changes any chest metadata. */
	void SetCommandPending(bool bPending);
	/** Owner-local, request-correlated result supplied by the screen. */
	void SetCommandResult(bool bSucceeded, const FText& Message);

	/** Confirmed slot rules; empty inventory does not remove them. */
	UFUNCTION(BlueprintPure, Category = "Storage|ViewModel")
	TArray<FRpgStorageAssignment> GetAssignments() const { return Assignments; }

	/** True for accessible shared physical storage, false for corpse/loot presenters. */
	UFUNCTION(BlueprintPure, Category = "Storage|ViewModel")
	bool IsPhysicalStorage() const { return bIsPhysicalStorage; }

	/** Stable command target copied from replicated gameplay metadata. */
	UFUNCTION(BlueprintPure, Category = "Storage|ViewModel")
	FName GetContainerId() const { return ContainerId; }

	/** Confirmed revision included with every chest command. */
	UFUNCTION(BlueprintPure, Category = "Storage|ViewModel")
	int32 GetSettingsRevision() const { return SettingsRevision; }

	/** Zero-based confirmed capacity tier. */
	UFUNCTION(BlueprintPure, Category = "Storage|ViewModel")
	int32 GetCurrentTier() const { return CurrentTier; }

	/** Whether this screen awaits its own server result. */
	UFUNCTION(BlueprintPure, Category = "Storage|ViewModel")
	bool IsCommandPending() const { return bCommandPending; }

protected:
	virtual void BeginDestroy() override;

	/** Replicated assignment slots; presentation may enumerate these to build designer-authored entries. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	TArray<FRpgStorageAssignment> Assignments;

	/** Enables physical chest controls only for a supported shared container. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	bool bIsPhysicalStorage = false;

	/** Stable storage ID, independent of actor transform or UI lifetime. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	FName ContainerId;

	/** Spatial base ownership; None represents an outside chest. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	FName BaseId;

	/** Zero-based confirmed upgrade tier; UI may display tier plus one. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	int32 CurrentTier = 0;

	/** Replicated revision used for stale command rejection. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	int32 SettingsRevision = INDEX_NONE;

	/** Confirmed root-grid width in cells. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	int32 GridWidth = 0;

	/** Confirmed root-grid height in cells. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	int32 GridHeight = 0;

	/** Local in-flight state for this screen's command; never replicated or saved. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	bool bCommandPending = false;

	/** Local last acknowledged result; does not predict inventory state. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	bool bLastCommandSucceeded = false;

	/** Localized rejection or completion message returned by authority. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Storage|ViewModel")
	FText LastCommandMessage;

private:
	void HandleSettingsChanged(URpgInventoryContainerComponent* Container);
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	TWeakObjectPtr<URpgInventoryContainerComponent> ObservedContainer;
	FDelegateHandle SettingsChangedHandle;
	FGameplayMessageListenerHandle InventoryChangedHandle;
};
