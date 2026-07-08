#pragma once

#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "RpgUIScreenSubsystem.generated.h"

class UCommonActivatableWidget;
class UPrimaryGameLayout;
class URpgUIScreenRegistry;
struct FRpgUIScreenRegistryEntry;

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

protected:
	UPrimaryGameLayout* GetOrCreatePrimaryGameLayout() const;
	const URpgUIScreenRegistry* GetScreenRegistry() const;
	bool ResolveScreenEntry(FGameplayTag ScreenTag, FRpgUIScreenRegistryEntry& OutEntry) const;
	void ApplyPayloadToWidget(UCommonActivatableWidget* Widget, UObject* Payload) const;
	void HandleScreenDeactivated(FGameplayTag ScreenTag, UCommonActivatableWidget* Widget);

private:
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidget>> ActiveScreens;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UObject>> PendingPayloads;

	UPROPERTY(Transient)
	TSet<FGameplayTag> PendingScreenTags;
};
