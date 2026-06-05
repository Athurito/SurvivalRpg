#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_HitReaction.generated.h"

class UAnimMontage;

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_HitReaction : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_HitReaction(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnHitReactionFinished();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> HitReactionMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction", meta = (ClampMin = "0.0"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Hit Reaction", meta = (ClampMin = "0.0"))
	float FallbackDuration = 0.35f;
};
