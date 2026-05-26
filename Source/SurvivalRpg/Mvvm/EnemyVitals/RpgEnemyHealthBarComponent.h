#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"

#include "RpgEnemyHealthBarComponent.generated.h"

class UEnemyVitalsViewmodel;

/**
 * World widget component that injects an owner-bound vitals model into an MVVM health bar widget.
 */
UCLASS(ClassGroup = (RpgUI), meta = (BlueprintSpawnableComponent, DisplayName = "Enemy Health Bar"))
class SURVIVALRPG_API URpgEnemyHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	URpgEnemyHealthBarComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitWidget() override;

protected:
	virtual void OnUnregister() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UEnemyVitalsViewmodel> EnemyVitalsViewmodel;
};
