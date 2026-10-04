#pragma once

#include "Delegates/Delegate.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "RpgUIScreenSubsystem.generated.h"

class UCommonActivatableWidget;
class UCommonActivatableWidgetContainerBase;
class UPrimaryGameLayout;
class UUserWidget;
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

	/** Opens a local screen, or returns its retained single instance without raising a covered page. Returns nullptr while loading or closing. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	UCommonActivatableWidget* OpenScreen(FGameplayTag ScreenTag, UObject* Payload = nullptr);

	/** Closes an open screen, including a covered page, or opens it when no instance exists. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	UCommonActivatableWidget* ToggleScreen(FGameplayTag ScreenTag, UObject* Payload = nullptr);

	/** Removes the local screen from its container, including a covered page, or cancels its pending push. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	void CloseScreen(FGameplayTag ScreenTag);

	/** Cancels pending loads and removes all locally routed screens without revealing retained pages. New requests are accepted after the drain completes. */
	UFUNCTION(BlueprintCallable, Category = "UI|Screens")
	void CloseAllScreens();

	/** Returns the active widget tracked for ScreenTag, if it is still valid and activated. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	UCommonActivatableWidget* GetActiveScreen(FGameplayTag ScreenTag) const;

	/** Returns true while a screen is active or still being asynchronously pushed. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	bool IsScreenActiveOrPending(FGameplayTag ScreenTag) const;

	/** Returns true while a local screen is loading, retained in its stack (possibly covered), or finishing its close transition. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|Screens", meta = (Categories = "UI.Screen"))
	bool IsScreenOpenOrPending(FGameplayTag ScreenTag) const;

protected:
	UPrimaryGameLayout* GetPrimaryGameLayout() const;
	const URpgUIScreenRegistry* GetScreenRegistry() const;
	bool ResolveScreenEntry(FGameplayTag ScreenTag, FRpgUIScreenRegistryEntry& OutEntry) const;
	void ApplyPayloadToWidget(UCommonActivatableWidget* Widget, UObject* Payload) const;
	void HandleScreenPushState(
		uint64 RequestGeneration,
		FGameplayTag ScreenTag,
		EAsyncWidgetLayerState State,
		UCommonActivatableWidget* Widget,
		UCommonActivatableWidgetContainerBase* Layer = nullptr);
	void HandleScreenDeactivated(
		FGameplayTag ScreenTag,
		UCommonActivatableWidget* Widget,
		uint64 CheckoutId);
	uint64 RegisterScreenDeactivationBinding(
		FGameplayTag ScreenTag,
		UCommonActivatableWidget* Widget,
		UCommonActivatableWidgetContainerBase* Layer);
	void HandleScreenLayerChanged(UCommonActivatableWidget* DisplayedWidget, uint64 CheckoutId);
	void HandleScreenDestructed(UUserWidget* Widget, uint64 CheckoutId);
	void CompleteScreenCheckout(uint64 CheckoutId);
	UCommonActivatableWidget* GetOpenScreen(FGameplayTag ScreenTag) const;
	void ReleaseScreenDeactivationBinding(uint64 CheckoutId);
	void ReleaseScreenDeactivationBindings(
		FGameplayTag ScreenTag,
		UCommonActivatableWidget* Widget);
	void ClearPendingScreenState(FGameplayTag ScreenTag);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgUIScreenRegistryExactResolutionTest;
	friend class FRpgUIScreenAsyncCloseLifecycleTest;
	friend class FRpgUIScreenRetainedStackLifecycleTest;
	friend class FRpgUIScreenCloseAllLifecycleTest;
#endif

	struct FScreenDeactivationBinding
	{
		FGameplayTag ScreenTag;
		TWeakObjectPtr<UCommonActivatableWidget> Widget;
		TWeakObjectPtr<UCommonActivatableWidgetContainerBase> Layer;
		FDelegateHandle DelegateHandle;
		FDelegateHandle LayerDelegateHandle;
		FDelegateHandle DestructDelegateHandle;
		bool bClosing = false;
	};

	/** Locally retained screen checkouts; covered entries remain tracked until their container releases them. */
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
	/** Async callbacks from an earlier drain must never mutate a subsequent screen request. */
	uint64 ScreenGeneration = 0;
	bool bIsClosingAllScreens = false;
	bool bIsDeinitializing = false;
};
