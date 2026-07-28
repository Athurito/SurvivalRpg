#pragma once

#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"

#include "RpgGameplayAbility_HarvestCorpse.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class URpgHarvestableCorpseComponent;

/**
 * Server-only Interaction ability that reserves one corpse and commits solely at its montage gameplay event.
 * Cancellation before that event releases the reservation and never grants reward or XP.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Harvest Corpse Ability"))
class GF_HARVESTING_MAGIC_API URpgGameplayAbility_HarvestCorpse final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_HarvestCorpse(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Stable feature-owned ability id used by progression and diagnostics. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Corpse Harvesting")
	static FGameplayTag GetHarvestCorpseAbilityId();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleCommitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageCancelled();

	void HandleReservationEnded(
		URpgHarvestableCorpseComponent* Component,
		AActor* Harvester,
		int32 ReservationRevision);

	void BeginMovementBlock();
	void EndMovementBlock();
	void BeginToolCue(const UObject* SourceObject);
	void EndToolCue();
	void FinishHarvest(bool bWasCancelled);

	UPROPERTY(Transient)
	TObjectPtr<URpgHarvestableCorpseComponent> ActiveCorpse;

	FInteractionOption ActiveInteractionOption;
	TWeakObjectPtr<AActor> ActiveHarvester;
	FRpgInventoryItemId ActiveToolItemId;
	FGameplayTag ActiveToolCueTag;
	int32 ActiveReservationRevision = INDEX_NONE;
	bool bRewardCommitted = false;
	bool bFinishing = false;
	bool bReservationEndedExternally = false;
	bool bMovementBlocked = false;
	bool bToolCueActive = false;
};
