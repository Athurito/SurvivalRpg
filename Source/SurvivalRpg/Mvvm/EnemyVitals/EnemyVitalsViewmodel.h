#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Mvvm/PlayerVitals/PlayerVitalsViewmodel.h"

#include "EnemyVitalsViewmodel.generated.h"

class AActor;
class URpgPawnExtensionComponent;

/**
 * A per-widget vitals viewmodel that observes the ability system of a world actor.
 * It derives from the player vitals model so existing health percent bindings can be reused.
 */
UCLASS(BlueprintType)
class SURVIVALRPG_API UEnemyVitalsViewmodel : public UPlayerVitalsViewmodel
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Rpg|UI|Vitals")
	void BindToActor(AActor* InObservedActor);

	UFUNCTION(BlueprintCallable, Category = "Rpg|UI|Vitals")
	void UnbindFromActor();

protected:
	virtual void BeginDestroy() override;

private:
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();

	TWeakObjectPtr<AActor> ObservedActor;

	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundPawnExtension;
};
