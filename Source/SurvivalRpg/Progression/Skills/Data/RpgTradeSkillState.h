#pragma once


#include "CoreMinimal.h"
#include "RpgTradeSkillState.generated.h"

USTRUCT(BlueprintType)
struct FTradeSkillState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Level = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float XP = 0.f;
};