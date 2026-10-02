#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_Stagger.generated.h"

class UAnimMontage;

/** Abstract server stagger lifecycle with replicated state cleanup and activation-group restoration. */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_Stagger : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_Stagger(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

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

	UFUNCTION()
	void OnStaggerFinished();

private:
	void ApplyStaggerTags() const;
	void ClearStaggerTags() const;
	void ApplyStaggerImmunity() const;
	float GetStaggerDuration() const;
	float GetStaggerImmunityDuration() const;

private:
	ERpgAbilityActivationGroup ActivationGroupBeforeStagger = ERpgAbilityActivationGroup::Independent;

	/** Designer-selected replicated stagger montage; optional when a duration supplies completion. */
	UPROPERTY(EditDefaultsOnly, Category = "Stagger")
	TObjectPtr<UAnimMontage> StaggerMontage;

	/** Designer-selected guard-break montage, preferred over the stagger montage when configured. */
	UPROPERTY(EditDefaultsOnly, Category = "Stagger")
	TObjectPtr<UAnimMontage> GuardBreakMontage;

	/** Positive cosmetic montage playback multiplier; authoritative duration comes from the defense attributes. */
	UPROPERTY(EditDefaultsOnly, Category = "Stagger", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	/** Designer fallback duration in seconds when the server has no positive stagger-duration attribute. */
	UPROPERTY(EditDefaultsOnly, Category = "Stagger", meta = (ClampMin = "0.0", Units = "s"))
	float FallbackDuration = 0.0f;
};
