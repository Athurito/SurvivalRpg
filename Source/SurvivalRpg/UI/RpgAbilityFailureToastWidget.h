#pragma once

#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgAbilitySimpleFailureMessage.h"

#include "RpgAbilityFailureToastWidget.generated.h"

class UTextBlock;

/**
 * Owner-local HUD toast for abilities that failed with a user-facing reason.
 *
 * Abilities map failure tags to text in FailureTagToUserFacingMessages and broadcast
 * Ability.UserFacingSimpleActivateFail.Message. This widget shows the reason of its owning player for DisplayDuration
 * seconds; the Widget Blueprint owns layout and styling. Cosmetic and UI-read-only.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SURVIVALRPG_API URpgAbilityFailureToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit URpgAbilityFailureToastWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Shows one failure reason and restarts the auto-hide timer. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Abilities|Feedback")
	void ShowAbilityFailure(const FText& Reason);

	/** Immediately hides the current failure reason. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Abilities|Feedback")
	void HideAbilityFailure();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Required authored label for the localized failure reason. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Rpg|Abilities|Feedback")
	TObjectPtr<UTextBlock> MessageText = nullptr;

	/** Seconds a failure reason remains visible. Cosmetic client-only tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Abilities|Feedback", meta = (ClampMin = "0.25", UIMin = "0.25", Units = "s"))
	float DisplayDuration = 2.0f;

	/** Optional presentation hook for sound or animation when a reason is shown. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Abilities|Feedback", meta = (DisplayName = "On Ability Failure Shown"))
	void BP_OnAbilityFailureShown(const FRpgAbilitySimpleFailureMessage& Message);

private:
	void HandleFailureMessage(FGameplayTag Channel, const FRpgAbilitySimpleFailureMessage& Message);

	FGameplayMessageListenerHandle FailureListenerHandle;
	FTimerHandle HideTimerHandle;
};
