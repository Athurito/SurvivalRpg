#pragma once

#include "CoreMinimal.h"
#include "RpgItemGrantTypes.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemGameplayEffectGrant
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ToolTip = "GameplayEffect granted by the item. Use passive effects in equipment fragments and active combat effects in weapon fragments."))
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ToolTip = "Level used when applying the granted GameplayEffect. Keep this at 1.0 unless the effect scales by item tier or roll."))
	float EffectLevel = 1.0f;
};
