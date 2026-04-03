#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_ActivateWeaponSet.generated.h"

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_ActivateWeaponSet : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_ActivateWeaponSet();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 WeaponSetIndex = 0;
};
