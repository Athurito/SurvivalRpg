#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_FromEquipment.generated.h"

class URpgEquipmentInstance;
class URpgInventoryItemInstance;

/** Abstract equipment-source mechanism; concrete ability identity and tuning belong to Blueprint assets. */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgGameplayAbility_FromEquipment : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the equipment instance stored on this activation's granted spec; runtime and read-only. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment|Ability")
	URpgEquipmentInstance* GetAssociatedEquipment() const;

	/** Returns the source inventory item of the granted equipment instance; equipment remains authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Equipment|Ability")
	URpgInventoryItemInstance* GetAssociatedItem() const;

protected:
	FGameplayTag GetInputTagFromSpec(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsEquipmentActiveForInput(const URpgEquipmentInstance* EquipmentInstance, FGameplayTag InputTag) const;
};
