#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "RpgPlayerGameplayInputRouterComponent.generated.h"

class UCommonUIActionRouterBase;
enum class ECommonInputMode : uint8;

/** Notifies the gameplay HUD when the shared eight-way quick-access radial opens or changes selection. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRpgQuickAccessRadialChanged, bool, bIsOpen, int32, SelectedSlotIndex);

/**
 * Controller-owned router for systemic gameplay hotkeys that are not direct ASC ability inputs.
 *
 * Pawn input binding forwards pressed/released gameplay tags here. The router keeps actionbar,
 * weapon-ability, and future player-controller-owned input features out of the pawn component.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPlayerGameplayInputRouterComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgPlayerGameplayInputRouterComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	//~ UControllerComponent interface
	virtual void ReceivedPlayer() override;
	//~ End UControllerComponent interface

	/** Routes an owning-client system command such as inventory, actionbar, or weapon-ability input. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input")
	void HandleGameplayInputPressed(FGameplayTag InputTag);

	/** Routes a released systemic gameplay input tag such as InputTag.ActionBar.Slot.1 or InputTag.Weapon.Ability.1. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input")
	void HandleGameplayInputReleased(FGameplayTag InputTag);

	/** Opens the gameplay radial. The radial reads the same eight bindings used by keyboard 1..8. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input|Quick Access")
	void BeginQuickAccessRadial();

	/** Updates the highlighted radial segment from a right-stick vector in the -1..1 range. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input|Quick Access")
	void UpdateQuickAccessRadial(FVector2D StickInput);

	/** Closes the radial and activates the highlighted shared quick-access binding. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input|Quick Access")
	void CommitQuickAccessRadial();

	/** Closes the radial without activating a binding. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Input|Quick Access")
	void CancelQuickAccessRadial();

	/** True while the local player is holding the gameplay radial input. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Input|Quick Access")
	bool IsQuickAccessRadialOpen() const { return bQuickAccessRadialOpen; }

	/** Zero-based highlighted segment, or INDEX_NONE while the stick is inside the dead zone. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Input|Quick Access")
	int32 GetQuickAccessRadialSelection() const { return QuickAccessRadialSelection; }

	/**
	 * Resolves a right-stick vector to one of eight clockwise segments.
	 *
	 * Segment zero points up. Returns INDEX_NONE when the vector is invalid or inside the dead zone.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Input|Quick Access")
	static int32 ResolveQuickAccessRadialSelection(FVector2D StickInput, float DeadZone);

	/** UI-read-only local state change used to drive the gameplay radial without owning its bindings. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Input|Quick Access")
	FRpgQuickAccessRadialChanged OnQuickAccessRadialChanged;

private:
	static constexpr int32 QuickAccessRadialSlotCount = 8;

	static int32 GetActionBarSlotIndexFromInputTag(FGameplayTag InputTag);
	static int32 GetWeaponAbilitySlotIndexFromInputTag(FGameplayTag InputTag);
	bool CanBeginQuickAccessRadial();
	void BindCommonUiInputRouter();
	void UnbindCommonUiInputRouter();
	void HandleActiveInputModeChanged(ECommonInputMode ActiveInputMode);
	void SetQuickAccessRadialState(bool bIsOpen, int32 SelectedSlotIndex);
	void SetQuickAccessLookSuppressed(bool bShouldSuppress);

	/** Minimum right-stick magnitude required to select a segment. Local cosmetic/input state only. */
	UPROPERTY(EditDefaultsOnly, Category = "Rpg|Input|Quick Access", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float QuickAccessRadialDeadZone = 0.45f;

	TWeakObjectPtr<UCommonUIActionRouterBase> ObservedCommonUiInputRouter;
	FDelegateHandle ActiveInputModeChangedHandle;
	bool bQuickAccessRadialOpen = false;
	bool bOwnsQuickAccessLookSuppression = false;
	int32 QuickAccessRadialSelection = INDEX_NONE;
};
