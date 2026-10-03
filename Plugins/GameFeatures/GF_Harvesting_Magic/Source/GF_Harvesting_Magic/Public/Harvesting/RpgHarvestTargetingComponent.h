#pragma once

#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Harvesting/RpgHarvestTargeting.h"
#include "TimerManager.h"

#include "RpgHarvestTargetingComponent.generated.h"

class AController;
class UAbilitySystemComponent;
struct FGameplayAbilitySpec;

/** Cosmetic notification fired on the owning client when the previewed harvest targets change. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgHarvestPreviewChangedEvent, const FRpgHarvestPreview&, Preview);

/**
 * Local-only read model of what the player's harvest abilities would hit, for target indicators.
 *
 * Added to player controllers by the harvesting GameFeature. On the locally controlled player it re-evaluates at
 * UpdateRateHz with the same query the server uses at commit time:
 * - While a hold-to-aim harvest ability is held, that ability's area and targets are previewed.
 * - Otherwise the harvest ability bound to the active main-hand tool's primary input is previewed.
 * OnPreviewChanged fires only when the presentation-relevant content changes. The preview never grants or
 * mutates anything; presentation Blueprints and widgets read it.
 */
UCLASS(BlueprintType, ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Harvest Targeting"))
class GF_HARVESTING_MAGIC_API URpgHarvestTargetingComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgHarvestTargetingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the targeting component of Controller, or null when the harvesting feature did not add one. */
	static URpgHarvestTargetingComponent* FindForController(const AController* Controller);

	/** Returns the latest local preview; empty when no harvest ability is available. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Targeting")
	FRpgHarvestPreview GetCurrentPreview() const { return CurrentPreview; }

	/** Re-evaluates immediately and notifies listeners when the preview changed. Does nothing on non-local controllers. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Harvesting|Targeting")
	void RefreshPreview();

	/** Called by hold-to-aim harvest abilities on the owning client while their input is held. */
	void SetAimingAbility(FGameplayAbilitySpecHandle Handle, bool bAiming);

	/** Cosmetic event for target highlights and indicators. Never grant loot from it. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Harvesting|Targeting")
	FRpgHarvestPreviewChangedEvent OnPreviewChanged;

protected:
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/** Preview evaluations per second on the local client; each runs one view query. Cosmetic tuning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Targeting", meta = (ClampMin = "1.0", ClampMax = "60.0", UIMin = "5.0", UIMax = "30.0"))
	float UpdateRateHz = 15.0f;

private:
	bool IsLocallyControlled() const;
	UAbilitySystemComponent* FindAbilitySystem() const;
	const FGameplayAbilitySpec* FindPreviewSpec(UAbilitySystemComponent& AbilitySystem, bool& bOutAiming) const;

	/** Latest local preview, compared on refresh to suppress redundant notifications. */
	FRpgHarvestPreview CurrentPreview;

	/** Hold-to-aim ability currently held on this client, if any. */
	FGameplayAbilitySpecHandle AimingSpecHandle;

	FTimerHandle RefreshTimerHandle;
};
