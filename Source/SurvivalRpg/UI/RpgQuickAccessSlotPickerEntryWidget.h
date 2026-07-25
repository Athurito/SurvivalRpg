#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"

#include "RpgQuickAccessSlotPickerEntryWidget.generated.h"

class UTextBlock;
class URpgInventoryContextMenuWidget;

/**
 * Designer-owned CommonUI row for one of the eight shared Quick Access slots.
 *
 * SlotIndex is always the internal zero-based index (0..7). DisplaySlotNumber is the player-facing
 * number (1..8), preventing keyboard/radial labels from becoming a second binding truth.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgQuickAccessSlotPickerEntryWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	explicit URpgQuickAccessSlotPickerEntryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Configures a selectable 1..8 destination. Occupied slots are intentionally overwriteable. */
	void InitializeQuickAccessSlot(
		URpgInventoryContextMenuWidget* InOwningMenu,
		int32 InSlotIndex,
		const FText& InBindingLabel,
		bool bInOccupied,
		bool bInCurrentBinding);

	/** Internal actionbar array index in the inclusive range 0..7. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	int32 GetSlotIndex() const { return SlotIndex; }

	/** Player-facing slot number in the inclusive range 1..8. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	int32 GetDisplaySlotNumber() const { return SlotIndex + 1; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	bool IsOccupied() const { return bOccupied; }

	UFUNCTION(BlueprintPure, Category = "Inventory|Quick Access")
	bool IsCurrentBinding() const { return bCurrentBinding; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnClicked() override;

	/** Required combined slot/occupant label owned by the canonical authored picker-row Blueprint. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|Quick Access")
	TObjectPtr<UTextBlock> Text_SlotLabel = nullptr;

	/** Presentation hook for a custom number, occupant icon/name, and current-binding indicator. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Quick Access", meta = (DisplayName = "On Quick Access Slot Configured"))
	void BP_OnQuickAccessSlotConfigured(
		int32 DisplaySlotNumber,
		const FText& InBindingLabel,
		bool bIsOccupied,
		bool bIsCurrentBinding);

private:
	void RefreshSlotPresentation();

	UPROPERTY(Transient)
	TObjectPtr<URpgInventoryContextMenuWidget> OwningMenu = nullptr;

	UPROPERTY(Transient)
	FText BindingLabel;

	int32 SlotIndex = INDEX_NONE;
	bool bOccupied = false;
	bool bCurrentBinding = false;
};
