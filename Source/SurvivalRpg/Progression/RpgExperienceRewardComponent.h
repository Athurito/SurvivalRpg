#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgExperienceRewardComponent.generated.h"

UCLASS(ClassGroup = (Custom), Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgExperienceRewardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgExperienceRewardComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progression")
	float GetXPReward() const { return XPReward; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float XPReward = 10.0f;
};
