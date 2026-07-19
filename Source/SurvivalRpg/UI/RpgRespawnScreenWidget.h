#pragma once

#include "RpgActivatableWidget.h"

#include "RpgRespawnScreenWidget.generated.h"

class ARpgPlayerState;
class UButton;
class UWidget;

/**
 * CommonUI presenter for the blocking local respawn screen.
 *
 * Replicated respawn truth remains on ARpgPlayerState. This widget only observes
 * that state, manages local presentation/focus, and sends the existing
 * server-authoritative respawn request through ARpgPlayerController.
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgRespawnScreenWidget : public URpgActivatableWidget
{
	GENERATED_BODY()

public:
	explicit URpgRespawnScreenWidget(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	/** Authored respawn action; enabled only when replicated server time permits the request. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Rpg|Respawn")
	TObjectPtr<UButton> RespawnButton;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRpgRespawnScreenNativeContractTest;
#endif

	UFUNCTION()
	void HandleRespawnButtonClicked();

	UFUNCTION()
	void HandleRespawnStateChanged(
		bool bIsWaitingForRespawn,
		float RespawnAvailableServerTime);

	void BindToOwningPlayerState();
	void UnbindFromPlayerState();
	void RefreshRespawnAvailability();
	void ScheduleAvailabilityRefresh(float DelaySeconds);

	/** Current persistent replicated source; the pawn is intentionally not used because death destroys it. */
	UPROPERTY(Transient)
	TObjectPtr<ARpgPlayerState> BoundPlayerState;

	FTimerHandle AvailabilityRefreshTimer;
};
