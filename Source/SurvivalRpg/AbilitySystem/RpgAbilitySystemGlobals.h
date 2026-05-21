#pragma once

#include "AbilitySystemGlobals.h"

#include "RpgAbilitySystemGlobals.generated.h"

struct FGameplayEffectContext;

UCLASS(Config = Game)
class SURVIVALRPG_API URpgAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()

public:
	URpgAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
