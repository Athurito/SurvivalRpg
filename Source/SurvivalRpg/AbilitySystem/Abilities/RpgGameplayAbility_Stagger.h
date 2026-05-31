#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_Stagger.generated.h"

class UAnimMontage;

UCLASS()
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

private:
	ERpgAbilityActivationGroup ActivationGroupBeforeStagger = ERpgAbilityActivationGroup::Independent;

	UPROPERTY(EditDefaultsOnly, Category = "Stagger")
	TObjectPtr<UAnimMontage> StaggerMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Stagger")
	TObjectPtr<UAnimMontage> GuardBreakMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Stagger", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Stagger", meta = (ClampMin = "0.0"))
	float FallbackDuration = 0.7f;
};
