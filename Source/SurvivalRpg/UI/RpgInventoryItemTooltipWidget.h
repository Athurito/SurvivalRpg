#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "RpgInventoryItemTooltipWidget.generated.h"

class STextBlock;
class SVerticalBox;
class URpgInventoryEntryViewModel;
class URpgInventoryItemInstance;
class URpgInventoryItemizationFragmentViewModel;

/** Broadcast when a tooltip's read-only inventory presentation changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgInventoryItemTooltipChanged,
	class URpgInventoryItemTooltipWidget*,
	TooltipWidget);

/**
 * Read-only inventory tooltip for static item UI data and replicated generated-item rolls.
 *
 * The native class renders a complete fallback tooltip. A Widget Blueprint child may provide its own widget tree and
 * react to BP_OnTooltipPresentationChanged or bind to the exposed entry/itemization view models instead.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API URpgInventoryItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgInventoryItemTooltipWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Creates one tooltip owned by Host's local UI world; returns null when the host has no usable world yet. */
	static URpgInventoryItemTooltipWidget* CreateForHost(
		UUserWidget* Host,
		TSubclassOf<URpgInventoryItemTooltipWidget> TooltipClass);

	/** Binds to an existing read-only inventory entry presenter and follows its itemization child presenter. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Tooltip")
	void SetEntryViewModel(URpgInventoryEntryViewModel* InEntryViewModel);

	/**
	 * Builds an internal read-only entry presenter for an item surface that has no inventory entry presenter.
	 * The concrete item remains authoritative elsewhere; this method never mutates it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Tooltip")
	void SetItemInstance(URpgInventoryItemInstance* InItemInstance, int32 InStackCount = 1);

	/** Unbinds delegates and clears all item presentation before a pooled source widget is reused. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Tooltip")
	void ClearItem();

	/** Read-only inventory entry currently rendered by the tooltip. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tooltip")
	URpgInventoryEntryViewModel* GetEntryViewModel() const { return EntryViewModel.Get(); }

	/** Generated-item child presenter, or null for materials and legacy equipment. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tooltip")
	URpgInventoryItemizationFragmentViewModel* GetItemizationViewModel() const
	{
		return ItemizationViewModel.Get();
	}

	/** Full localized item name rendered in the tooltip header. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tooltip")
	FText GetDisplayName() const;

	/** Optional localized flavor or usage description from UIData. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tooltip")
	FText GetDescription() const;

	/** Current replicated stack count represented by the tooltip. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tooltip")
	int32 GetStackCount() const;

	/** True when an item entry is bound and can be displayed. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tooltip")
	bool HasItem() const;

	/** Fired after item, stack, rarity, level, base-stat, or affix presentation changes. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Tooltip")
	FRpgInventoryItemTooltipChanged OnTooltipPresentationChanged;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeDestruct() override;

	/**
	 * Presentation hook for authored Widget Blueprint tooltips.
	 * Both arguments are read-only projections; gameplay and inventory mutations must remain outside the widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Tooltip", meta = (DisplayName = "On Tooltip Presentation Changed"))
	void BP_OnTooltipPresentationChanged(
		URpgInventoryEntryViewModel* NewEntryViewModel,
		URpgInventoryItemizationFragmentViewModel* NewItemizationViewModel);

	/** Minimum desired width, in Slate units, used only by the native fallback tooltip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip|Native Fallback", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float NativeMinimumWidth = 280.0f;

	/** Inner padding, in Slate units, used only by the native fallback tooltip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip|Native Fallback", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float NativePadding = 12.0f;

	/** Background tint used only by the native fallback tooltip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip|Native Fallback")
	FLinearColor NativeBackgroundColor = FLinearColor(0.025f, 0.02f, 0.035f, 0.98f);

	/** Header font size, in Slate units, used only by the native fallback tooltip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip|Native Fallback", meta = (ClampMin = "1", UIMin = "1"))
	int32 NativeHeaderFontSize = 17;

	/** Body font size, in Slate units, used only by the native fallback tooltip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip|Native Fallback", meta = (ClampMin = "1", UIMin = "1"))
	int32 NativeBodyFontSize = 13;

	/** Color for definition-authored base stat rows in the native fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip|Native Fallback")
	FLinearColor NativeBaseStatColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);

	/** Color for rolled affix rows in the native fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip|Native Fallback")
	FLinearColor NativeAffixColor = FLinearColor(0.45f, 0.78f, 1.0f, 1.0f);

private:
	UFUNCTION()
	void HandleEntryChanged(URpgInventoryEntryViewModel* ChangedEntryViewModel);

	UFUNCTION()
	void HandleItemizationPresentationChanged(
		URpgInventoryItemizationFragmentViewModel* ChangedViewModel);

	void RefreshBoundItemizationViewModel();
	void RefreshPresentation();
	void RefreshNativePresentation();
	void UnbindPresentationDelegates();

	/** External or internally-created read-only entry presenter currently displayed. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryEntryViewModel> EntryViewModel = nullptr;

	/** Reusable entry presenter for equipment/address surfaces that only expose a concrete item instance. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryEntryViewModel> OwnedEntryViewModel = nullptr;

	/** Itemization presenter observed for replicated roll changes. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryItemizationFragmentViewModel> ItemizationViewModel = nullptr;

	TSharedPtr<STextBlock> NativeNameText;
	TSharedPtr<STextBlock> NativeRarityAndLevelText;
	TSharedPtr<STextBlock> NativeStackText;
	TSharedPtr<STextBlock> NativeDescriptionText;
	TSharedPtr<SVerticalBox> NativeStatRows;
};
