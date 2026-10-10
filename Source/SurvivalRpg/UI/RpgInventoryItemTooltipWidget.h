#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"

#include "RpgInventoryItemTooltipWidget.generated.h"

class STextBlock;
class SVerticalBox;
class URpgInventoryEntryViewModel;
class URpgInventoryItemInstance;
class URpgInventoryItemizationFragmentViewModel;
class URpgItemTooltipViewModel;

/** Broadcast when a tooltip's read-only inventory presentation changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FRpgInventoryItemTooltipChanged,
	class URpgInventoryItemTooltipWidget*,
	TooltipWidget);

/**
 * Read-only inventory tooltip for static item UI data and replicated generated-item rolls.
 *
 * The native class renders a complete fallback tooltip. A Widget Blueprint child may provide its own widget tree and
 * bind the two manual MVVM sources: the hovered item and, for equippable items, the item currently equipped in the
 * same slot. While the player holds Shift the hovered model shows the comparison.
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

	/** Read model of the hovered item. UI read-only. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tooltip")
	URpgItemTooltipViewModel* GetTooltipViewModel() const { return TooltipViewModel; }

	/** Read model of the equipped counterpart; HasItem is false when nothing comparable is equipped. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Tooltip")
	URpgItemTooltipViewModel* GetComparisonViewModel() const { return ComparisonViewModel; }

	/** Shows or hides the comparison, as holding Shift does. Ignored while nothing comparable is equipped. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Tooltip")
	void SetComparisonShown(bool bShown);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	/** Manual MVVM source in the Widget Blueprint that receives the hovered item's read model. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip")
	FName TooltipViewModelSourceName = TEXT("Tooltip");

	/** Manual MVVM source in the Widget Blueprint that receives the equipped counterpart's read model. Designer data. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Tooltip")
	FName ComparisonViewModelSourceName = TEXT("Comparison");

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
	void RefreshComparison();
	void UnbindPresentationDelegates();
	URpgInventoryItemInstance* ResolveEquippedCounterpart() const;

	/** External or internally-created read-only entry presenter currently displayed. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryEntryViewModel> EntryViewModel = nullptr;

	/** Reusable entry presenter for equipment/address surfaces that only expose a concrete item instance. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryEntryViewModel> OwnedEntryViewModel = nullptr;

	/** Itemization presenter observed for replicated roll changes. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryItemizationFragmentViewModel> ItemizationViewModel = nullptr;

	/** Read model of the hovered item, assigned to TooltipViewModelSourceName. */
	UPROPERTY(Transient)
	TObjectPtr<URpgItemTooltipViewModel> TooltipViewModel = nullptr;

	/** Read model of the equipped counterpart, assigned to ComparisonViewModelSourceName. */
	UPROPERTY(Transient)
	TObjectPtr<URpgItemTooltipViewModel> ComparisonViewModel = nullptr;

	/** Entry presenter built for the equipped counterpart. */
	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryEntryViewModel> ComparisonEntryViewModel = nullptr;

	/** Equipped counterpart the comparison was built for. */
	TWeakObjectPtr<URpgInventoryItemInstance> ComparedItem;

	bool bComparisonShown = false;

	TSharedPtr<STextBlock> NativeNameText;
	TSharedPtr<STextBlock> NativeRarityAndLevelText;
	TSharedPtr<STextBlock> NativeStackText;
	TSharedPtr<STextBlock> NativeDescriptionText;
	TSharedPtr<SVerticalBox> NativeStatRows;
};
