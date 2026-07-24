#pragma once

#include "Delegates/Delegate.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "RpgUIScreenSubsystem.generated.h"

class UCommonActivatableWidget;
class UPrimaryGameLayout;
class URpgUIScreenRegistry;
struct FStreamableHandle;
struct FRpgUIScreenRegistryEntry;
enum class EAsyncWidgetLayerState : uint8;

/**
 * Local-player UI screen router that opens CommonGame widgets by UI.Screen gameplay tag.
 *
 * Gameplay remains authoritative elsewhere; this subsystem only resolves screen tags, pushes widgets,
 * and forwards local payload objects to the resulting screen widget.
 */
UCLASS()
class SURVIVALRPG_API URpgUIScreenSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Opens the screen mapped to ScreenTag. Returns an already-active single instance, otherwise nullptr while async loading completes. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	UCommonActivatableWidget* OpenScreen(FGameplayTag ScreenTag, UObject* Payload = nullptr);

	/** Closes an active screen, or opens it when no active instance exists. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	UCommonActivatableWidget* ToggleScreen(FGameplayTag ScreenTag, UObject* Payload = nullptr);

	/** Deactivates the currently active widget for ScreenTag. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	void CloseScreen(FGameplayTag ScreenTag);

	/** Returns the active widget tracked for ScreenTag, if it is still valid and activated. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	UCommonActivatableWidget* GetActiveScreen(FGameplayTag ScreenTag) const;

	/** Returns true while a screen is active or still being asynchronously pushed. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	bool IsScreenActiveOrPending(FGameplayTag ScreenTag) const;

protected:
	UPrimaryGameLayout* GetPrimaryGameLayout() const;
	const URpgUIScreenRegistry* GetScreenRegistry() const;
	bool ResolveScreenEntry(FGameplayTag ScreenTag, FRpgUIScreenRegistryEntry& OutEntry) const;
	void ApplyPayloadToWidget(UCommonActivatableWidget* Widget, UObject* Payload) const;
	void HandleScreenPushState(
		FGameplayTag ScreenTag,
		EAsyncWidgetLayerState State,
		UCommonActivatableWidget* Widget);
	void HandleScreenDeactivated(
		FGameplayTag ScreenTag,
		UCommonActivatableWidget* Widget,
		uint64 CheckoutId);
	uint64 RegisterScreenDeactivationBinding(
		FGameplayTag ScreenTag,
		UCommonActivatableWidget* Widget);
	void ReleaseScreenDeactivationBinding(uint64 CheckoutId);
	void ReleaseScreenDeactivationBindings(
		FGameplayTag ScreenTag,
		UCommonActivatableWidget* Widget);
	void ClearPendingScreenState(FGameplayTag ScreenTag);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgUIScreenRegistryExactResolutionTest;
	friend class FRpgUIScreenAsyncCloseLifecycleTest;
#endif

	struct FScreenDeactivationBinding
	{
		FGameplayTag ScreenTag;
		TWeakObjectPtr<UCommonActivatableWidget> Widget;
		FDelegateHandle DelegateHandle;
	};

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidget>> ActiveScreens;

	/** Checkout identity prevents an old pooled callback from clearing a newer same-tag screen. */
	TMap<FGameplayTag, uint64> ActiveScreenCheckoutIds;

	/** Exact delegate ownership; overlapping callbacks are valid during synchronous pool reuse. */
	TMap<uint64, FScreenDeactivationBinding> ScreenDeactivationBindings;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UObject>> PendingPayloads;

	UPROPERTY(Transient)
	TSet<FGameplayTag> PendingScreenTags;

	/** Keeps each CommonGame async push cancelable until its AfterPush callback completes. */
	TMap<FGameplayTag, TSharedPtr<FStreamableHandle>> PendingScreenLoads;

	/** Pending pushes canceled because gameplay access disappeared before initialization completed. */
	UPROPERTY(Transient)
	TSet<FGameplayTag> CanceledPendingScreenTags;

	uint64 NextScreenCheckoutId = 0;
	bool bIsDeinitializing = false;
};
