#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "CommonUserWidget.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgCraftingJobEntryWidget.generated.h"

class URpgCraftingActionButtonWidget;
class URpgCraftingJobViewModel;
class URpgCraftingStationWidget;
class UCommonLazyImage;
class UCommonTextBlock;
class UProgressBar;
class UTexture2D;

/**
 * Pooled crafting-job row.
 *
 * The row injects only its read-only MVVM data. Its cancel button forwards a typed intent to the active screen;
 * it never discovers the player controller, station, or RPC component on its own.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgCraftingJobEntryWidget
	: public UCommonUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	/** Exact source name authored by CUI_CraftingJobEntrySpatial. */
	static const FName JobViewModelSourceName;

	/** Supplies the screen that owns crafting command routing for this generated entry. */
	void SetCommandOwner(URpgCraftingStationWidget* InCommandOwner);

	/** Read-only replicated job projection currently represented by this pooled widget. */
	UFUNCTION(BlueprintPure, Category = "Crafting|Job")
	URpgCraftingJobViewModel* GetJobViewModel() const
	{
		return JobViewModel.Get();
	}

	/** MVVM destinations that keep all row formatting in the native passive leaf. */
	UFUNCTION(BlueprintCallable, Category = "Crafting|Job")
	void SetJobIcon(TSoftObjectPtr<UTexture2D> InIcon);

	UFUNCTION(BlueprintCallable, Category = "Crafting|Job")
	void SetQuantityCompleted(int32 InQuantity);

	UFUNCTION(BlueprintCallable, Category = "Crafting|Job")
	void SetQuantityTotal(int32 InQuantity);

	UFUNCTION(BlueprintCallable, Category = "Crafting|Job")
	void SetJobProgress(float InProgress);

	UFUNCTION(BlueprintCallable, Category = "Crafting|Job")
	void SetCanCancelFromViewModel(bool bInCanCancel);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	//~IUserObjectListEntry interface
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	//~End of IUserObjectListEntry interface

	//~IUserListEntry interface
	virtual void NativeOnEntryReleased() override;
	//~End of IUserListEntry interface

	virtual void NativeDestruct() override;

	/** Authored cancel control. It signals the screen and owns no gameplay mutation. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<URpgCraftingActionButtonWidget> Button_Cancel = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DisplayNameText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> QuantityCompletedText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> QuantityTotalText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar = nullptr;

private:
	void SetJobViewModel(URpgCraftingJobViewModel* InViewModel);
	void HandleCancelClicked();
	void RefreshCancelAvailability();

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingJobViewModel> JobViewModel = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<URpgCraftingStationWidget> CommandOwner = nullptr;

	bool bCanCancelFromViewModel = false;
};
