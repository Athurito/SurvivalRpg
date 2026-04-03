#pragma once

#include "CoreMinimal.h"
#include "RpgItemGrantTypes.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemGameplayEffectGrant
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float EffectLevel = 1.0f;
};
