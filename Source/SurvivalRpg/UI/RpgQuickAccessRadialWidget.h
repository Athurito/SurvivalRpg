#pragma once

#include "CommonUserWidget.h"
#include "Engine/DataTable.h"
#include "UIExtensionSystem.h"

#include "RpgQuickAccessRadialWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;
class URpgActionBarSlotViewModel;
class URpgActionBarViewModel;
class URpgPlayerGameplayInputRouterComponent;

/**
 * Non-interactive authored presentation for one quick-access radial segment.
 *
 * Gameplay data comes exclusively from URpgActionBarSlotViewModel. Selection is local router
 * presentation state; this widget never activates, mutates, or accepts drops for an actionbar slot.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgQuickAccessRadialSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgQuickAccessRadialSlotWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Exact optional manual MVVM source owned by the canonical radial slot Blueprint. */
	static const FName ActionBarSlotViewModelSourceName;

	/** Assigns the read-only actionbar projection represented by this authored segment. */
	UFUNCTION(BlueprintCallable, Category = "Quick Access|Radial")
	void SetActionBarSlotViewModel(URpgActionBarSlotViewModel* InSlotViewModel);

	/** Updates the local selection highlight without changing gameplay or actionbar state. */
	UFUNCTION(BlueprintCallable, Category = "Quick Access|Radial")
	void SetRadialSelected(bool bInSelected);

	/**
	 * Sets the soft icon presented by this segment.
	 *
	 * MVVM writes this presentation-only value; UImage owns the asynchronous soft-texture request.
	 */
	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "Quick Access|Radial")
	void SetIconSource(TSoftObjectPtr<UTexture2D> InIconSource);

	/** Current presentation-only soft icon supplied by the slot ViewModel. */
	UFUNCTION(BlueprintPure, BlueprintGetter, Category = "Quick Access|Radial")
	TSoftObjectPtr<UTexture2D> GetIconSource() const { return IconSource; }

	/** Current read-only actionbar slot projection. */
	UFUNCTION(BlueprintPure, Category = "Quick Access|Radial")
	URpgActionBarSlotViewModel* GetActionBarSlotViewModel() const
	{
		return SlotViewModel.Get();
	}

	/** True when this segment is highlighted by the local quick-access router. */
	UFUNCTION(BlueprintPure, Category = "Quick Access|Radial")
	bool IsRadialSelected() const { return bRadialSelected; }

protected:
	virtual void NativeDestruct() override;

	/** Authored segment background used for selected, unavailable, empty, and normal colors. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> SegmentBorder = nullptr;

	/** Authored one-based segment number. Runtime presentation only. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SlotNumberText = nullptr;

	/** Authored item icon; fed by the MVVM-facing IconSource setter. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon = nullptr;

	/** Authored unavailable-state label. Runtime presentation only. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> BlockedText = nullptr;

	/** Normal occupied segment color. Cosmetic designer tuning only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style")
	FLinearColor NormalSegmentColor = FLinearColor(0.035f, 0.03f, 0.028f, 0.94f);

	/** Selected segment color. Cosmetic designer tuning only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style")
	FLinearColor SelectedSegmentColor = FLinearColor(0.58f, 0.43f, 0.16f, 0.98f);

	/** Unavailable occupied segment color. Cosmetic designer tuning only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style")
	FLinearColor BlockedSegmentColor = FLinearColor(0.18f, 0.055f, 0.045f, 0.94f);

	/** Empty segment color. Cosmetic designer tuning only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Access|Radial|Style")
	FLinearColor EmptySegmentColor = FLinearColor(0.02f, 0.018f, 0.017f, 0.72f);

private:
	UFUNCTION()
	void HandleSlotViewModelChanged(
		URpgActionBarSlotViewModel* ChangedSlotViewModel);

	void RefreshPresentation();

	UPROPERTY(Transient)
	TObjectPtr<URpgActionBarSlotViewModel> SlotViewModel = nullptr;

	/** Runtime presentation value written by MVVM; never authoritative or saved. */
	UPROPERTY(
		Transient,
		BlueprintReadWrite,
		BlueprintGetter = GetIconSource,
		BlueprintSetter = SetIconSource,
		Category = "Quick Access|Radial",
		meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UTexture2D> IconSource;

	bool bRadialSelected = false;
};

/**
 * Persistent CommonUI/UIExtension presenter for the shared eight-slot quick-access radial.
 *
 * The authored widget owns one read-only URpgActionBarViewModel and eight fixed segment widgets.
 * The controller input router owns open/selection state and is the only path that can trigger a slot.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgQuickAccessRadialWidget
	: public UCommonUserWidget
	, public IUIExtensionWidgetLifecycle
{
	GENERATED_BODY()

public:
	explicit URpgQuickAccessRadialWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** True while the local player is holding the radial input. Presentation state only. */
	UFUNCTION(BlueprintPure, Category = "Quick Access|Radial")
	bool IsRadialOpen() const { return bRadialOpen; }

	/** Highlighted zero-based segment or INDEX_NONE inside the stick dead zone. */
	UFUNCTION(BlueprintPure, Category = "Quick Access|Radial")
	int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }

	/** Screen-owned read-only projection shared by all eight radial entries. */
	UFUNCTION(BlueprintPure, Category = "Quick Access|Radial")
	URpgActionBarViewModel* GetActionBarViewModel() const
	{
		return ActionBarViewModel.Get();
	}

	/** Number of authored radial segment widgets found on this instance. */
	UFUNCTION(BlueprintPure, Category = "Quick Access|Radial")
	int32 GetAuthoredSlotEntryCount() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnExtensionAdded() override;
	virtual void NativeOnExtensionRemoved() override;

	/** Authored segment zero, pointing up. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgQuickAccessRadialSlotWidget> RadialSlot_0 = nullptr;

	/** Authored segment one, pointing up-right. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgQuickAccessRadialSlotWidget> RadialSlot_1 = nullptr;

	/** Authored segment two, pointing right. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgQuickAccessRadialSlotWidget> RadialSlot_2 = nullptr;

	/** Authored segment three, pointing down-right. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgQuickAccessRadialSlotWidget> RadialSlot_3 = nullptr;

	/** Authored segment four, pointing down. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgQuickAccessRadialSlotWidget> RadialSlot_4 = nullptr;

	/** Authored segment five, pointing down-left. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgQuickAccessRadialSlotWidget> RadialSlot_5 = nullptr;

	/** Authored segment six, pointing left. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgQuickAccessRadialSlotWidget> RadialSlot_6 = nullptr;

	/** Authored segment seven, pointing up-left. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URpgQuickAccessRadialSlotWidget> RadialSlot_7 = nullptr;

	/**
	 * CommonUI Back row consumed only while the radial is open.
	 *
	 * Static designer data; the canonical asset points at UI.Back in CDT_RpgUIActions_All.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quick Access|Radial|Input")
	FDataTableRowHandle CommonUiCancelAction;

private:
	UFUNCTION()
	void HandleRadialChanged(bool bIsOpen, int32 InSelectedSlotIndex);

	UFUNCTION()
	void HandleActionBarSlotsChanged();

	TArray<URpgQuickAccessRadialSlotWidget*> GetAuthoredSlotEntries() const;
	void HandleCommonUiCancel();
	void RegisterCommonUiCancelBinding();
	void UnregisterCommonUiCancelBinding();
	void ActivateExtensionPresenter();
	void DeactivateExtensionPresenter();
	void RefreshSlotViewModels();
	void RefreshSelectionPresentation();

	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerGameplayInputRouterComponent> ObservedInputRouter = nullptr;

	/** Screen-owned projection of owner-only actionbar state; UI reads it but never mutates gameplay. */
	UPROPERTY(Transient)
	TObjectPtr<URpgActionBarViewModel> ActionBarViewModel = nullptr;

	FUIActionBindingHandle CommonUiCancelBinding;
	bool bExtensionPresenterActive = false;
	bool bRadialOpen = false;
	int32 SelectedSlotIndex = INDEX_NONE;
};
