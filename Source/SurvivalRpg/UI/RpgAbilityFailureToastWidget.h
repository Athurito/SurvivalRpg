#pragma once

#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgAbilitySimpleFailureMessage.h"

#include "RpgAbilityFailureToastWidget.generated.h"

class UCommonLazyImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/** How the HUD presents one kind of ability failure. Designer data on the toast Widget Blueprint. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgAbilityFailurePresentation
{
	GENERATED_BODY()

	/** Failure tag this row applies to, for example Ability.ActivateFail.Cost or Ability.ActivateFail.Cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Abilities|Feedback")
	FGameplayTag FailureTag;

	/** Cost attribute this row applies to, for example Stamina; empty matches any cost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Abilities|Feedback")
	FGameplayAttribute CostAttribute;

	/** Icon shown beside the reason. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Abilities|Feedback")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Maximum of CostAttribute, for example MaxStamina. When set, the toast shows the resource's current fill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Abilities|Feedback")
	FGameplayAttribute MaxAttribute;
};

/**
 * Owner-local HUD toast for abilities that failed with a user-facing reason.
 *
 * Abilities map failure tags to text in FailureTagToUserFacingMessages and broadcast
 * Ability.UserFacingSimpleActivateFail.Message. This widget shows the reason of its owning player for DisplayDuration
 * seconds; the Widget Blueprint owns layout and styling. A matching Presentations row adds an icon and, for a cost
 * failure, the current fill of the missing resource. Cosmetic and UI-read-only.
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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Required authored label for the localized failure reason. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Rpg|Abilities|Feedback")
	TObjectPtr<UTextBlock> MessageText = nullptr;

	/** Seconds a failure reason remains visible. Cosmetic client-only tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Abilities|Feedback", meta = (ClampMin = "0.25", UIMin = "0.25", Units = "s"))
	float DisplayDuration = 2.0f;

	/** Icon and resource bar per failure kind; the first matching row wins. Cosmetic client-only tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Abilities|Feedback")
	TArray<FRpgAbilityFailurePresentation> Presentations;

	/** Optional icon beside the reason. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Rpg|Abilities|Feedback")
	TObjectPtr<UCommonLazyImage> FailureIcon = nullptr;

	/** Optional bar showing the missing resource's current fill. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Rpg|Abilities|Feedback")
	TObjectPtr<UProgressBar> ResourceBar = nullptr;

	/** Optional presentation hook for sound or animation when a reason is shown. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Abilities|Feedback", meta = (DisplayName = "On Ability Failure Shown"))
	void BP_OnAbilityFailureShown(const FRpgAbilitySimpleFailureMessage& Message);

private:
	void HandleFailureMessage(FGameplayTag Channel, const FRpgAbilitySimpleFailureMessage& Message);
	void ApplyPresentation(const FRpgAbilitySimpleFailureMessage& Message);
	void RefreshResourceBar();

	/** Resource shown in ResourceBar while the toast is visible. */
	FGameplayAttribute ShownResource;
	FGameplayAttribute ShownResourceMax;

	FGameplayMessageListenerHandle FailureListenerHandle;
	FTimerHandle HideTimerHandle;
};
