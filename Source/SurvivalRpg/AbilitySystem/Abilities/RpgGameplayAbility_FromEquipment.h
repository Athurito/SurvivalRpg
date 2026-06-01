#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_FromEquipment.generated.h"

class URpgEquipmentInstance;
class URpgInventoryItemInstance;

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_FromEquipment : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment|Ability")
	URpgEquipmentInstance* GetAssociatedEquipment() const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment|Ability")
	URpgInventoryItemInstance* GetAssociatedItem() const;

protected:
	FGameplayTag GetInputTagFromSpec(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsEquipmentActiveForInput(const URpgEquipmentInstance* EquipmentInstance, FGameplayTag InputTag) const;
};
